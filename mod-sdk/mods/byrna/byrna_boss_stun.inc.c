// NEI's FD super-hit rule: first contact opens Gohma / King Dodongo, subsequent contacts hurt them.
// Use the bosses' own states so their hurt, weak-point and death sequences remain native.
static void ClearByrnaBossHit(ColliderJntSph* collider) {
    for(s32 i=0;i<collider->count;i++) {
        ColliderInfo* info=&collider->elements[i].info;
        if(IsByrnaHitInfo(info->acHitInfo)) info->bumperFlags&=~BUMP_HIT;
    }
}
static void StunByrnaBoss(Actor* actor,PlayState* play) {
    if(actor->id==ACTOR_BOSS_GOMA) {
        BossGoma* boss=(BossGoma*)actor;
        if(actor->colChkInfo.health==0 || boss->actionFunc==BossGoma_Defeated ||
           boss->actionFunc==BossGoma_FloorStunned || boss->actionFunc==BossGoma_FallStruckDown ||
           boss->actionFunc==BossGoma_FloorLandStruckDown) return;
        ClearByrnaBossHit(&boss->collider);
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
        ClearByrnaBossHit(&boss->collider);
        AnimationHeader* anim=(AnimationHeader*)ResourceMgr_LoadAnimByName(object_kingdodongo_Anim_004E0C);
        if(!anim) return;
        Animation_Change(&boss->skelAnime,anim,1,0,Animation_GetLastFrame(anim),ANIMMODE_ONCE,-5);
        boss->actionFunc=BossDodongo_LayDown;boss->unk_1C0=5;
        Audio_PlayActorSound2(actor,NA_SE_EN_DODO_K_DAMAGE);
    }
}
