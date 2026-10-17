// Damage is selected for the actor actually struck, independently of Link's target.
// Keep recipient-specific hit views: a boss and an enemy can share the same attacking triangle.
static ColliderInfo sDamageViews[4];
static Collider* sCurrentAC[64];
static Collider* sPreviousAC[64];
static s32 sCurrentACCount,sPreviousACCount;
static u32 sACFrame;
static Actor* sDamageVictim;

static bool IsByrnaHitInfo(ColliderInfo* info) {
    if (!info || !sCollidersReady) return false;
    if (info==&sForward.info) return true;
    for (s32 i=0;i<BYRNA_CYL_TRIS;i++) if (info==&sAirElements[i].info) return true;
    for (s32 i=0;i<4;i++) if (info==&sDamageViews[i]) return true;
    return false;
}
static void CacheDamageAC(Collider* collider) {
    if (!gPlayState || !collider) return;
    if (sACFrame!=gPlayState->gameplayFrames) {
        memcpy(sPreviousAC,sCurrentAC,sizeof(sCurrentAC));sPreviousACCount=sCurrentACCount;
        sCurrentACCount=0;sACFrame=gPlayState->gameplayFrames;
    }
    for (s32 i=0;i<sCurrentACCount;i++) if(sCurrentAC[i]==collider) return;
    if(sCurrentACCount<64) sCurrentAC[sCurrentACCount++]=collider;
}
static void SetRecipientView(Actor* victim,ColliderInfo* receiver) {
    ColliderInfo* source=receiver->acHitInfo;
    if (!(receiver->bumperFlags&BUMP_HIT) || !IsByrnaHitInfo(source)) return;
    bool boss=victim->category==ACTORCAT_BOSS;
    bool hammer=(source->toucher.dmgFlags&DMG_HAMMER_SWING)!=0;
    ColliderInfo* view=&sDamageViews[(boss?2:0)+(hammer?1:0)];
    *view=*source;
    view->toucher.dmgFlags=(boss?DMG_SLASH_MASTER:DMG_SLASH_KOKIRI)|(hammer?DMG_HAMMER_SWING:0);
    view->toucher.damage=boss?BYRNA_BOSS_DAMAGE:BYRNA_NORMAL_DAMAGE;
    receiver->acHitInfo=view;sDamageVictim=victim;
}
static void PrepareDamageRecipient(void* actorPtr,bool* should) {
    sDamageVictim=NULL;
    if (!*should || !gPlayState) return;
    Actor* actor=actorPtr;
    // Collision resolution uses last frame's registrations; the current frame may already be registering new ones.
    Collider** list=sACFrame==gPlayState->gameplayFrames?sPreviousAC:sCurrentAC;
    s32 count=sACFrame==gPlayState->gameplayFrames?sPreviousACCount:sCurrentACCount;
    for (s32 i=0;i<count;i++) {
        Collider* col=list[i];
        if (!col || col->actor!=actor || !(col->acFlags&AC_HIT)) continue;
        switch(col->shape) {
            case COLSHAPE_CYLINDER: SetRecipientView(actor,&((ColliderCylinder*)col)->info);break;
            case COLSHAPE_QUAD: SetRecipientView(actor,&((ColliderQuad*)col)->info);break;
            case COLSHAPE_JNTSPH: {
                ColliderJntSph* sph=(ColliderJntSph*)col;
                for(s32 j=0;j<sph->count;j++) SetRecipientView(actor,&sph->elements[j].info);
                break;
            }
            case COLSHAPE_TRIS: {
                ColliderTris* tris=(ColliderTris*)col;
                for(s32 j=0;j<tris->count;j++) SetRecipientView(actor,&tris->elements[j].info);
                break;
            }
        }
    }
    if(sDamageVictim==actor && actor->category==ACTORCAT_BOSS) StunByrnaBoss(actor,gPlayState);
}
static void FinishDamageRecipient(void* actorPtr) {
    if(sDamageVictim==(Actor*)actorPtr) sDamageVictim=NULL;
}
static void ForgetDamageRecipient(void* actorPtr) {
    Actor* actor=actorPtr;
    if(sDamageVictim==actor) sDamageVictim=NULL;
    for(s32 i=0;i<sCurrentACCount;i++) if(sCurrentAC[i] && sCurrentAC[i]->actor==actor) sCurrentAC[i]=NULL;
    for(s32 i=0;i<sPreviousACCount;i++) if(sPreviousAC[i] && sPreviousAC[i]->actor==actor) sPreviousAC[i]=NULL;
}
static void ResolveByrnaDamage(Actor* victim,ColliderInfo* attack,float* damage) {
    if(!victim || !damage || !IsByrnaHitInfo(attack)) return;
    bool boss=victim->category==ACTORCAT_BOSS;
    *damage=boss?BYRNA_BOSS_DAMAGE:BYRNA_NORMAL_DAMAGE;
    // Collision initially accepts both sword classes. Restore the actual class's native reaction effect.
    if(victim->colChkInfo.damageTable) {
        s32 bit=(attack->toucher.dmgFlags&DMG_HAMMER_SWING)?6:(boss?9:8);
        victim->colChkInfo.damageEffect=victim->colChkInfo.damageTable->table[bit]>>4;
    }
}
static void ResolveByrnaSwordDamage(PlayState* play,int32_t flags,uint8_t* damage) {
    if(!sDamageVictim || !damage) return;
    bool boss=sDamageVictim->category==ACTORCAT_BOSS;
    u32 expected=boss?DMG_SLASH_MASTER:DMG_SLASH_KOKIRI;
    if((u32)flags==expected || (u32)flags==(expected|DMG_HAMMER_SWING))
        *damage=boss?BYRNA_BOSS_DAMAGE:BYRNA_NORMAL_DAMAGE;
}
static void ResetDamageRecipients(void) {
    sCurrentACCount=sPreviousACCount=0;sACFrame=0;sDamageVictim=NULL;
    memset(sDamageViews,0,sizeof(sDamageViews));
}
