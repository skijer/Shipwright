void BossGoma_SetupFloorStunned(BossGoma*);
void BossGoma_SetupFallStruckDown(BossGoma*);
void BossGoma_FloorStunned(BossGoma*,PlayState*);
void BossGoma_FallStruckDown(BossGoma*,PlayState*);
void BossGoma_FloorLandStruckDown(BossGoma*,PlayState*);
void BossGoma_CeilingMoveToCenter(BossGoma*,PlayState*);
void BossGoma_CeilingIdle(BossGoma*,PlayState*);
void BossGoma_CeilingPrepareSpawnGohmas(BossGoma*,PlayState*);
void BossGoma_CeilingSpawnGohmas(BossGoma*,PlayState*);
void BossGoma_Defeated(BossGoma*,PlayState*);
void BossDodongo_LayDown(BossDodongo*,PlayState*);
void BossDodongo_Vulnerable(BossDodongo*,PlayState*);
void BossDodongo_DeathCutscene(BossDodongo*,PlayState*);

static bool IsUpgradeHit(ColliderInfo* info){
 if(!info||!gPlayState)return false;
 Player* p=GET_PLAYER(gPlayState);
 if(Weapon(p)==CH_MASTER&&gSaveContext.health>=gSaveContext.healthCapacity&&
    (info==&p->meleeWeaponQuads[0].info||info==&p->meleeWeaponQuads[1].info))return true;
 for(Actor* a=gPlayState->actorCtx.actorLists[ACTORCAT_ITEMACTION].head;a;a=a->next)
  if(a->id==sBeamActorId&&a->update&&info==&((SwordBeam*)a)->collider.info)return true;
 return false;
}
// NEI's FD super-hit rule: first contact opens Gohma / King Dodongo, subsequent contacts hurt them.
// Use the bosses' own states so their hurt, weak-point and death sequences remain native.
static void ClearUpgradeBossHit(ColliderJntSph* collider) {
    for(s32 i=0;i<collider->count;i++) {
        ColliderInfo* info=&collider->elements[i].info;
        if(IsUpgradeHit(info->acHitInfo)) info->bumperFlags&=~BUMP_HIT;
    }
}
static void StunUpgradeBoss(Actor* actor,PlayState* play) {
    if(actor->id==ACTOR_BOSS_GOMA) {
        BossGoma* boss=(BossGoma*)actor;
        if(actor->colChkInfo.health==0 || boss->actionFunc==BossGoma_Defeated ||
           boss->actionFunc==BossGoma_FloorStunned || boss->actionFunc==BossGoma_FallStruckDown ||
           boss->actionFunc==BossGoma_FloorLandStruckDown) return;
        ClearUpgradeBossHit(&boss->collider);
        if(boss->actionFunc==BossGoma_CeilingMoveToCenter || boss->actionFunc==BossGoma_CeilingIdle ||
           boss->actionFunc==BossGoma_CeilingPrepareSpawnGohmas || boss->actionFunc==BossGoma_CeilingSpawnGohmas) {
            BossGoma_SetupFallStruckDown(boss);
        } else {
            BossGoma_SetupFloorStunned(boss);
            boss->sfxFaintTimer=100;boss->framesUntilNextAction=90;boss->timer=4;
        }
        boss->invincibilityFrames=10;
        Audio_PlayActorSound2(actor,NA_SE_EN_GOMA_DAM2);
    } else if(actor->id==ACTOR_BOSS_DODONGO) {
        BossDodongo* boss=(BossDodongo*)actor;
        if(boss->health<=0 || boss->actionFunc==BossDodongo_DeathCutscene) return;
        if(boss->actionFunc==BossDodongo_Vulnerable || boss->actionFunc==BossDodongo_LayDown) {
            boss->unk_1DA=100;return;
        }
        ClearUpgradeBossHit(&boss->collider);
        AnimationHeader* anim=(AnimationHeader*)ResourceMgr_LoadAnimByName(object_kingdodongo_Anim_004E0C);
        if(!anim) return;
        Animation_Change(&boss->skelAnime,anim,1,0,Animation_GetLastFrame(anim),ANIMMODE_ONCE,-5);
        boss->actionFunc=BossDodongo_LayDown;boss->unk_1C0=5;
        Audio_PlayActorSound2(actor,NA_SE_EN_DODO_K_DAMAGE);
    }
}

static void BeforeUpgradeActor(void* ref,bool* should){
 if(!*should||!gPlayState)return;Actor* a=ref;
 ColliderJntSph* collider=a->id==ACTOR_BOSS_GOMA?&((BossGoma*)a)->collider:a->id==ACTOR_BOSS_DODONGO?&((BossDodongo*)a)->collider:NULL;
 if(!collider||!(collider->base.acFlags&AC_HIT))return;
 for(int i=0;i<collider->count;i++)if((collider->elements[i].info.bumperFlags&BUMP_HIT)&&IsUpgradeHit(collider->elements[i].info.acHitInfo)){StunUpgradeBoss(a,gPlayState);break;}
}
