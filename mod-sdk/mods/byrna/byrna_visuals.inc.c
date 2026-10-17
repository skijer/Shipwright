static SkelAnime sFairies[2];
static Vec3s sFairyJoints[2][15],sFairyMorph[2][15];
static bool sFairiesReady,sTrailDrawing;
static u32 sFairyFrame;
typedef struct { Actor* actor;Collider* collider; } ProjectileCollider;
static ProjectileCollider sProjectiles[64];

static void EnsureTrail(PlayState* play) {
    if (sTrail>=0) return;
    EffectBlureInit2 init={0,4,0,{210,220,255,200},{130,150,255,150},{210,220,255,0},{130,150,255,0},6,
        0,2,0,{255,255,255,255},{255,255,255,255},TRAIL_TYPE_SWORDS};
    Effect_Add(play,&sTrail,EFFECT_BLURE2,0,0,&init);
}
static void TraceCaneTrail(PlayState* play) {
    bool active=sByrna.active && sByrna.move==BYRNA_AIR_B;
    if (!active) {
        if (sTrailDrawing && sTrail>=0) {EffectBlure* e=Effect_GetByIndex(sTrail);if(e)EffectBlure_AddSpace(e);}
        sTrailDrawing=false;return;
    }
    EnsureTrail(play);EffectBlure* trail=sTrail<0?NULL:Effect_GetByIndex(sTrail);
    if (!trail) return;
    if (!sTrailDrawing) EffectBlure_AddSpace(trail);
    sTrailDrawing=true;
    Vec3f tip={0,CANE_AXIS_MAX*CANE_SCALE_Y,0},base={0,CANE_AXIS_MIN*CANE_SCALE_Y,0};
    Matrix_Push();ApplyCaneMatrix();Matrix_MultVec3f(&tip,&tip);Matrix_MultVec3f(&base,&base);Matrix_Pop();
    EffectBlure_AddVertex(trail,&tip,&base);
}
static void DrawCane(PlayState* play,Player* player,int32_t limbIndex) {
    RemoteVisualState visual;
    bool local = player == GET_PLAYER(play);
    bool visible = local ? IsWielding(player) : ReadRemoteVisual(player, &visual);
    if (limbIndex != PLAYER_LIMB_L_HAND || !visible) {
        return;
    }
    if (local) {
        TraceCaneTrail(play);
    }
    uint32_t savedRemoteEnv = CaptureRemoteLimbEnv(play);
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);Matrix_Push();ApplyCaneMatrix();
    Matrix_Scale(CANE_SCALE_X,CANE_SCALE_Y,CANE_SCALE_Z,MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_MODELVIEW|G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++,(Gfx*)sCaneDL);Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);

    if (player != GET_PLAYER(play)) {
        RestoreRemoteLimbEnv(play, savedRemoteEnv);
    }
}
static s32 FairyLimb(PlayState* play,s32 limbIndex,Gfx** dList,Vec3f* pos,Vec3s* rot,void* unused) {
    if (limbIndex==4 || limbIndex==7 || limbIndex==11 || limbIndex==14) *dList=NULL;
    if (limbIndex==8) {
        Vec3f zero={0},world;Matrix_MultVec3f(&zero,&world);
        Matrix_Translate(world.x,world.y,world.z,MTXMODE_NEW);
        Matrix_Scale(0.012f,0.012f,0.012f,MTXMODE_APPLY);
    }
    return false;
}
static void DrawDashFairies(PlayState* play,Player* player) {
    if (!sFairiesReady) {
        for (s32 i=0;i<2;i++) SkelAnime_Init(play,&sFairies[i],(SkeletonHeader*)gFairySkel,(AnimationHeader*)gFairyAnim,
            sFairyJoints[i],sFairyMorph[i],15);
        sFairiesReady=true;
    }
    if (sFairyFrame!=play->gameplayFrames) {for(s32 i=0;i<2;i++)SkelAnime_Update(&sFairies[i]);sFairyFrame=play->gameplayFrames;}
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Gfx* color=Graph_Alloc(play->state.gfxCtx,4*sizeof(Gfx));Gfx* p=color;
    gDPPipeSync(p++);gDPSetPrimColor(p++,0,0x01,190,190,190,230);
    gDPSetRenderMode(p++,G_RM_PASS,G_RM_ZB_CLD_SURF2);gSPEndDisplayList(p++);
    gSPSegment(POLY_XLU_DISP++,8,(uintptr_t)color);gDPSetEnvColor(POLY_XLU_DISP++,90,90,90,180);
    for (s32 i=0;i<2;i++) {
        f32 angle=play->gameplayFrames*0.35f+i*M_PI;
        Matrix_Push();Matrix_Translate(player->actor.world.pos.x+sinf(angle)*45,player->actor.world.pos.y+45+cosf(angle*2)*12,
            player->actor.world.pos.z+cosf(angle)*45,MTXMODE_NEW);Matrix_Scale(0.008f,0.008f,0.008f,MTXMODE_APPLY);
        POLY_XLU_DISP=SkelAnime_DrawSkeleton2(play,&sFairies[i],FairyLimb,NULL,NULL,POLY_XLU_DISP);
        Matrix_Pop();
    }
    CLOSE_DISPS(play->state.gfxCtx);
}
static void DrawGuardCircle(PlayState* play,Player* player) {
    OPEN_DISPS(play->state.gfxCtx);Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++,(Gfx*)gGreatFairyParticleMaterialDL);
    gDPSetPrimColor(POLY_XLU_DISP++,0,0,170,225,255,180);gDPSetEnvColor(POLY_XLU_DISP++,70,100,230,100);
    for (s32 i=0;i<24;i++) {
        f32 angle=(i%12)*(2*M_PI/12)+play->gameplayFrames*0.05f;
        Matrix_Push();Matrix_Translate(player->actor.world.pos.x+sinf(angle)*60,player->actor.world.pos.y+(i<12?12:85),
            player->actor.world.pos.z+cosf(angle)*60,MTXMODE_NEW);
        Matrix_ReplaceRotation(&play->billboardMtxF);Matrix_Scale(0.035f,0.035f,0.035f,MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_MODELVIEW|G_MTX_LOAD);
        gSPDisplayList(POLY_XLU_DISP++,(Gfx*)gGreatFairyParticleModelDL);Matrix_Pop();
    }
    CLOSE_DISPS(play->state.gfxCtx);
}
static void DrawByrnaEffects(Actor* actor,PlayState* play) {
    if (actor!=&GET_PLAYER(play)->actor || !IsWorn() || !sByrna.active) return;
    Player* player=(Player*)actor;
    if (sByrna.move==BYRNA_DASH && sByrna.frame>=13 && sByrna.frame<=50) DrawDashFairies(play,player);
    if (sByrna.move==BYRNA_GUARD) DrawGuardCircle(play,player);
}

// Remember the exact native AT collider before the next collision pass. The shield can then reflect it
// in ShouldActorUpdate, before a projectile's own hit handling destroys it.
static void CacheProjectileColliders(void* actorPtr) {
    Actor* actor=actorPtr;PlayState* play=gPlayState;
    if (!play || !IsWorn() || !sByrna.active || sByrna.move!=BYRNA_GUARD) return;
    bool projectile=actor->category==ACTORCAT_MISC || actor->category==ACTORCAT_ITEMACTION ||
        actor->id==ACTOR_EN_NUTSBALL || (actor->id==ACTOR_EN_OKUTA && actor->params>=0x10);
    if (!projectile) return;
    for (s32 c=0;c<play->colChkCtx.colATCount;c++) {
        Collider* col=play->colChkCtx.colAT[c];if (!col || col->actor!=actor) continue;
        for (s32 i=0;i<64;i++) if (!sProjectiles[i].actor || sProjectiles[i].collider==col) {
            sProjectiles[i]=(ProjectileCollider){actor,col};break;
        }
    }
}
static void ForgetProjectile(void* actor) {
    for (s32 i=0;i<64;i++) if (sProjectiles[i].actor==actor) sProjectiles[i].actor=NULL;
}
static void ReflectProjectile(void* actorPtr,bool* shouldUpdate) {
    Actor* actor=actorPtr;PlayState* play=gPlayState;
    if (!play || !sCollidersReady || sByrna.move!=BYRNA_GUARD || !sByrna.active ||
        !(sGuard.base.acFlags&AC_HIT) || sGuard.base.ac!=actor) return;
    for (s32 i=0;i<64;i++) if (sProjectiles[i].actor==actor) {
        Collider* col=sProjectiles[i].collider;
        col->atFlags&=~(AT_HIT|AT_BOUNCED|AT_TYPE_ENEMY);col->atFlags|=AT_TYPE_PLAYER;
        col->at=NULL;
        Player* player=GET_PLAYER(play);
        s16 yaw=Actor_WorldYawTowardActor(&player->actor,actor);
        actor->world.rot.y=actor->shape.rot.y=yaw;
        f32 speed=MAX(fabsf(actor->speedXZ),sqrtf(actor->velocity.x*actor->velocity.x+actor->velocity.z*actor->velocity.z));
        actor->speedXZ=MAX(speed,4);actor->velocity.x=Math_SinS(yaw)*actor->speedXZ;actor->velocity.z=Math_CosS(yaw)*actor->speedXZ;
        sGuard.base.acFlags&=~AC_HIT;sGuard.base.ac=NULL;
        Sfx_PlaySfxCentered(NA_SE_IT_SHIELD_REFLECT_SW);break;
    }
}
