typedef enum {
    BYRNA_NONE, BYRNA_DRAW, BYRNA_SHEATHE, BYRNA_SLASH, BYRNA_DASH, BYRNA_LAUNCH,
    BYRNA_AIR_IDLE, BYRNA_AIR_B, BYRNA_AIR_A, BYRNA_REBOUND, BYRNA_POUND, BYRNA_LAND, BYRNA_GUARD
} ByrnaMove;
typedef struct { LinkAnimationHeader header; s16* values; } ByrnaClip;
#include "byrna_vault_floor.inc.c"
static ByrnaClip sClips[CLIP_MAX];
static struct {
    ByrnaMove move;
    ClipId clip;
    f32 frame, gravity, minVelocity, launchFloor;
    s16 ticks, shieldTicks;
    bool active, landedAttack, impulse, trailBreak, immuneOwned, bodyAcOwned;
    s32 item;
} sByrna;
static ColliderCylinder sForward, sGuard;
static ColliderTris sAirCylinder;
static ColliderTrisElement sAirElements[BYRNA_CYL_TRIS];
static ColliderTrisElementInit sAirInitElements[BYRNA_CYL_TRIS];
static bool sCollidersReady;
static s32 sTrail=-1;
static bool sGuardRejected;
static void ByrnaAction(Player* player,PlayState* play);
static void StartAirIdle(PlayState* play,Player* player);

static bool IsWorn(void) { return CustomEquipRegistry_IsWorn(BYRNA_KEY); }
static bool IsSwordAction(s32 action) { return action>=PLAYER_IA_SWORD_MASTER && action<=PLAYER_IA_SWORD_BIGGORON; }
static bool IsWielding(Player* player) { return IsWorn() && IsSwordAction(player->heldItemAction); }
static bool IsGrounded(Player* player) { return (player->actor.bgCheckFlags&BGCHECKFLAG_GROUND)!=0; }
static bool IsPlayerBusy(Player* player) {
    return (player->stateFlags1&(PLAYER_STATE1_DEAD|PLAYER_STATE1_LOADING|PLAYER_STATE1_IN_CUTSCENE|
        PLAYER_STATE1_IN_ITEM_CS|PLAYER_STATE1_GETTING_ITEM|PLAYER_STATE1_DAMAGED|PLAYER_STATE1_IN_WATER|
        PLAYER_STATE1_ON_HORSE|PLAYER_STATE1_TALKING|PLAYER_STATE1_CLIMBING_LADDER|PLAYER_STATE1_CLIMBING_LEDGE))!=0;
}
static bool IsEnemy(Actor* actor) { return actor && actor->update && (actor->category==ACTORCAT_ENEMY || actor->category==ACTORCAT_BOSS); }
static bool ActorIsAlive(PlayState* play,Actor* actor) {
    if (!actor) return false;
    for (s32 c=0;c<ACTORCAT_MAX;c++) for (Actor* a=play->actorCtx.actorLists[c].head;a;a=a->next)
        if (a==actor) return a->update!=NULL;
    return false;
}

// Positions interpolate linearly. Packed angles use the shortest signed arc across +/-32768.
static s16 InterpolateAngle(s16 a,s16 b,f32 t) { return (s16)(a+(s16)(b-a)*t); }
static LinkAnimationHeader* LoadClip(ClipId id) {
    ByrnaClip* clip=&sClips[id];
    if (clip->values) return &clip->header;
    LinkAnimationHeader* raw=ResourceMgr_LoadPlayerAnimAsHeader(sClipSources[id].path);
    if (!raw || raw->common.frameCount<2) return NULL;
    // Native locomotion samples fixed phases directly, bypassing playSpeed (29 walk / 20 run).
    s32 phaseCount=id==CLIP_WALK?29:id==CLIP_RUN?20:0;
    s32 count=phaseCount?phaseCount:(raw->common.frameCount-1)*BYRNA_SAMPLES+1;
    clip->values=malloc((size_t)count*67*sizeof(s16));
    if (!clip->values) return NULL;
    s16* src=(s16*)raw->segment;
    for (s32 i=0;i<count;i++) {
        f32 sourceFrame=phaseCount?i*(f32)raw->common.frameCount/phaseCount:i/(f32)BYRNA_SAMPLES;
        s32 a=(s32)sourceFrame,b=phaseCount?(a+1)%raw->common.frameCount:MIN(a+1,raw->common.frameCount-1);
        f32 t=sourceFrame-a;
        for (s32 j=0;j<67;j++) clip->values[i*67+j]=j<3 ?
            (s16)(src[a*67+j]+(src[b*67+j]-src[a*67+j])*t):InterpolateAngle(src[a*67+j],src[b*67+j],t);
        clip->values[i*67]=0; clip->values[i*67+2]=0;
        if (!sClipSources[id].ground) clip->values[i*67+1]=src[1];
    }
    clip->header.common.frameCount=(s16)count;clip->header.segment=clip->values;
    return &clip->header;
}
static f32 OriginalFrame(Player* player) { return player->skelAnime.curFrame/BYRNA_SAMPLES; }
static f32 LastOriginalFrame(ClipId id) { LinkAnimationHeader* clip=LoadClip(id);return clip?Animation_GetLastFrame(clip)/BYRNA_SAMPLES:0; }
static bool CrossedFrame(f32 before,f32 now,f32 event) { return before<event && now>=event; }
static bool HitWindow(f32 before,f32 now,f32 start,f32 end) { return now>=start && before<=end; }
static void PlayClip(PlayState* play,Player* player,ClipId clip,bool loop) {
    LinkAnimationHeader* anim=LoadClip(clip);
    if (!anim) return;
    sByrna.clip=clip;sByrna.frame=0;
    LinkAnimation_Change(play,&player->skelAnime,anim,BYRNA_PLAY_SPEED*BYRNA_SAMPLES,0,Animation_GetLastFrame(anim),
        loop?ANIMMODE_LOOP:ANIMMODE_ONCE,-3.0f);
}
static void SilenceVanillaBlade(Player* player) {
    player->meleeWeaponState=0;
    player->meleeWeaponQuads[0].base.atFlags&=~AT_ON;
    player->meleeWeaponQuads[1].base.atFlags&=~AT_ON;
}
static void SetImmunity(Player* player,bool enabled) {
    if (enabled) {
        if (player->invincibilityTimer>=-2 && player->invincibilityTimer<=0) {
            player->invincibilityTimer=-2;sByrna.immuneOwned=true;
        }
    } else if (sByrna.immuneOwned) {
        if (player->invincibilityTimer>=-2 && player->invincibilityTimer<=0) player->invincibilityTimer=0;
        sByrna.immuneOwned=false;
    }
}
static void ReleaseGuard(Player* player) {
    if (sByrna.bodyAcOwned) { player->cylinder.base.acFlags|=AC_ON;sByrna.bodyAcOwned=false; }
    player->stateFlags1&=~PLAYER_STATE1_SHIELDING;
    if (sCollidersReady) sGuard.base.acFlags&=~AC_ON;
}
static void EndMove(PlayState* play,Player* player) {
    bool owned=player->actionFunc==ByrnaAction;
    ReleaseGuard(player);SetImmunity(player,false);SilenceVanillaBlade(player);
    player->actor.gravity=sByrna.gravity;player->actor.minVelocityY=sByrna.minVelocity;
    player->stateFlags3&=~PLAYER_STATE3_MIDAIR;
    player->stateFlags2&=~PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    sByrna.active=false;sByrna.move=BYRNA_NONE;
    if (owned && !IsPlayerBusy(player)) func_80839FFC(player,play);
}
static void BeginMove(PlayState* play,Player* player,ByrnaMove move,ClipId clip,bool loop) {
    if (!sByrna.active) { sByrna.gravity=player->actor.gravity;sByrna.minVelocity=player->actor.minVelocityY; }
    if (sByrna.move==BYRNA_GUARD) ReleaseGuard(player);
    SetImmunity(player,false);
    sByrna.active=true;sByrna.move=move;sByrna.ticks=0;sByrna.impulse=false;sByrna.landedAttack=false;sByrna.trailBreak=true;
    Player_SetupAction(play,player,ByrnaAction,0);
    SilenceVanillaBlade(player);
    if (sCollidersReady) { sForward.base.atFlags&=~(AT_HIT|AT_ON);sAirCylinder.base.atFlags&=~(AT_HIT|AT_ON); }
    PlayClip(play,player,clip,loop);
}
static void HandOver(Player* player,s32 item) {
    player->heldItemId=item;
    player->heldItemAction=item==ITEM_NONE?PLAYER_IA_NONE:Player_ItemToItemAction(item);
    player->itemAction=player->heldItemAction;
    func_8008EC70(player);
}
static void FaceTarget(PlayState* play,Player* player) {
    if (!ActorIsAlive(play,player->focusActor)) return;
    s16 yaw=Actor_WorldYawTowardActor(&player->actor,player->focusActor);
    Math_SmoothStepToS(&player->actor.shape.rot.y,yaw,1,0x1800,0);
    player->yaw=player->actor.world.rot.y=player->actor.shape.rot.y;
}
static void Airborne(Player* player,f32 gravity) {
    player->actor.gravity=gravity;player->actor.minVelocityY=-26;
    player->stateFlags3|=PLAYER_STATE3_MIDAIR;
}

static void EnsureColliders(PlayState* play,Player* player) {
    if (sCollidersReady) return;
    ColliderCylinderInit attack={
        {COLTYPE_NONE,AT_ON|AT_TYPE_PLAYER,AC_NONE,OC1_NONE,OC2_NONE,COLSHAPE_CYLINDER},
        {ELEMTYPE_UNK2,{BYRNA_SWORD_FLAGS,0,BYRNA_NORMAL_DAMAGE},{0,0,0},TOUCH_ON|TOUCH_SFX_NORMAL,BUMP_NONE,OCELEM_NONE},
        {42,75,0,{0,0,0}}};
    ColliderCylinderInit guard={
        {COLTYPE_METAL,AT_NONE,AC_ON|AC_HARD|AC_TYPE_ENEMY,OC1_NONE,OC2_TYPE_PLAYER,COLSHAPE_CYLINDER},
        {ELEMTYPE_UNK2,{0,0,0},{0xFFFFFFFF,0,0},TOUCH_NONE,BUMP_ON,OCELEM_NONE},{60,100,0,{0,0,0}}};
    Collider_InitCylinder(play,&sForward);Collider_SetCylinder(play,&sForward,&player->actor,&attack);
    Collider_InitCylinder(play,&sGuard);Collider_SetCylinder(play,&sGuard,&player->actor,&guard);
    for (s32 i=0;i<BYRNA_CYL_TRIS;i++) {
        ColliderTrisElementInit* e=&sAirInitElements[i];memset(e,0,sizeof(*e));
        e->info.elemType=ELEMTYPE_UNK2;e->info.toucher.dmgFlags=BYRNA_SWORD_FLAGS;
        e->info.toucher.damage=BYRNA_NORMAL_DAMAGE;e->info.toucherFlags=TOUCH_ON|TOUCH_SFX_NORMAL;
    }
    ColliderTrisInit tris={{COLTYPE_NONE,AT_ON|AT_TYPE_PLAYER,AC_NONE,OC1_NONE,OC2_NONE,COLSHAPE_TRIS},BYRNA_CYL_TRIS,sAirInitElements};
    Collider_InitTris(play,&sAirCylinder);Collider_SetTris(play,&sAirCylinder,&player->actor,&tris,sAirElements);
    sCollidersReady=true;
}
static void ForwardHit(PlayState* play,Player* player,bool pound,bool impact) {
    if (sByrna.landedAttack) return;
    EnsureColliders(play,player);
    f32 reach=pound?0:55;
    sForward.dim.pos.x=player->actor.world.pos.x+Math_SinS(player->actor.shape.rot.y)*reach;
    sForward.dim.pos.z=player->actor.world.pos.z+Math_CosS(player->actor.shape.rot.y)*reach;
    sForward.dim.pos.y=player->actor.world.pos.y+(pound?-50:5);
    sForward.dim.radius=impact?85:(pound?38:42);sForward.dim.height=impact?60:75;
    sForward.info.toucher.dmgFlags=BYRNA_SWORD_FLAGS|(pound?DMG_HAMMER_SWING:0);
    sForward.info.toucher.damage=BYRNA_NORMAL_DAMAGE;
    sForward.base.atFlags|=AT_ON;CollisionCheck_SetAT(play,&play->colChkCtx,&sForward.base);
}
static Vec3f CylinderPoint(Player* player,f32 right,f32 angle) {
    f32 up=35+cosf(angle)*38,forward=48+sinf(angle)*38;
    f32 sy=Math_SinS(player->actor.shape.rot.y),cy=Math_CosS(player->actor.shape.rot.y);
    return (Vec3f){player->actor.world.pos.x+right*cy+forward*sy,player->actor.world.pos.y+up,
        player->actor.world.pos.z+forward*cy-right*sy};
}
static void AirCylinderHit(PlayState* play,Player* player) {
    if (sByrna.landedAttack) return;
    EnsureColliders(play,player);
    Vec3f centerL=CylinderPoint(player,-45,0),centerR=CylinderPoint(player,45,0);
    centerL.y-=38;centerR.y-=38;
    for (s32 i=0;i<BYRNA_CYL_SEGMENTS;i++) {
        f32 a=i*(2.0f*M_PI/BYRNA_CYL_SEGMENTS),b=(i+1)*(2.0f*M_PI/BYRNA_CYL_SEGMENTS);
        Vec3f l0=CylinderPoint(player,-45,a),l1=CylinderPoint(player,-45,b);
        Vec3f r0=CylinderPoint(player,45,a),r1=CylinderPoint(player,45,b);
        Collider_SetTrisVertices(&sAirCylinder,i*4,&l0,&r0,&r1);
        Collider_SetTrisVertices(&sAirCylinder,i*4+1,&l0,&r1,&l1);
        Collider_SetTrisVertices(&sAirCylinder,i*4+2,&centerL,&l1,&l0);
        Collider_SetTrisVertices(&sAirCylinder,i*4+3,&centerR,&r0,&r1);
    }
    sAirCylinder.base.atFlags|=AT_ON;CollisionCheck_SetAT(play,&play->colChkCtx,&sAirCylinder.base);
}
static bool ProcessHit(PlayState* play,Player* player) {
    if (!sCollidersReady || sByrna.landedAttack) return false;
    Collider* hit=(sAirCylinder.base.atFlags&AT_HIT)?&sAirCylinder.base:
        (sForward.base.atFlags&AT_HIT)?&sForward.base:NULL;
    if (!hit) return false;
    Actor* enemy=hit->at;sByrna.landedAttack=true;
    sForward.base.atFlags&=~(AT_ON|AT_HIT);sAirCylinder.base.atFlags&=~(AT_ON|AT_HIT);
    if ((sByrna.move==BYRNA_AIR_A || sByrna.move==BYRNA_AIR_B) && IsEnemy(enemy)) {
        BeginMove(play,player,BYRNA_REBOUND,CLIP_REBOUND,false);
        player->linearVelocity*=0.5f;player->actor.velocity.y=0;Airborne(player,0);
        return true;
    }
    return false;
}
static void StartAirIdle(PlayState* play,Player* player) {
    BeginMove(play,player,BYRNA_AIR_IDLE,CLIP_AIR_IDLE,true);Airborne(player,BYRNA_AIR_GRAVITY);
}
static void StartLaunch(PlayState* play,Player* player) {
    BeginMove(play,player,BYRNA_LAUNCH,CLIP_LAUNCH,false);
    sByrna.launchFloor=player->actor.world.pos.y;
    player->linearVelocity=0;player->actor.velocity.y=0;player->actor.gravity=0;
}
static void StartDash(PlayState* play,Player* player,bool jump) {
    BeginMove(play,player,BYRNA_DASH,CLIP_DASH,false);
    FaceTarget(play,player);player->linearVelocity=0;
    if (jump) { player->actor.velocity.y=4.5f;player->actor.bgCheckFlags&=~BGCHECKFLAG_GROUND;Airborne(player,BYRNA_AIR_GRAVITY); }
}
static bool PayShield(void) {
    if (gSaveContext.magic<2) return false;
    gSaveContext.magic-=2;return true;
}
static void StartGuard(PlayState* play,Player* player) {
    if (!PayShield()) {
        if (!sGuardRejected) Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
        sGuardRejected=true;return;
    }
    BeginMove(play,player,BYRNA_GUARD,CLIP_IDLE,true);sByrna.shieldTicks=0;
    player->linearVelocity=0;player->stateFlags1|=PLAYER_STATE1_SHIELDING;
}
static void GuardTick(PlayState* play,Player* player) {
    if (!(play->state.input[0].cur.button&BTN_R) || (++sByrna.shieldTicks>=20 && !PayShield())) {
        EndMove(play,player);return;
    }
    if (sByrna.shieldTicks>=20) sByrna.shieldTicks=0;
    EnsureColliders(play,player);
    player->stateFlags1|=PLAYER_STATE1_SHIELDING;player->linearVelocity=0;
    if (player->cylinder.base.acFlags&AC_ON) {sByrna.bodyAcOwned=true;player->cylinder.base.acFlags&=~AC_ON;}
    SetImmunity(player,true);
    sGuard.base.acFlags|=AC_ON;Collider_UpdateCylinder(&player->actor,&sGuard);
    CollisionCheck_SetAC(play,&play->colChkCtx,&sGuard.base);
}
static void LandPound(PlayState* play,Player* player) {
    sByrna.move=BYRNA_LAND;sByrna.ticks=0;
    player->linearVelocity=0;player->actor.velocity.y=0;player->actor.gravity=sByrna.gravity;
    player->stateFlags3&=~PLAYER_STATE3_MIDAIR;
    Vec3f zero={0};EffectSsBlast_SpawnWhiteShockwave(play,&player->actor.world.pos,&zero,&zero);
    Sfx_PlaySfxCentered(NA_SE_IT_HAMMER_HIT);
    Rumble_Request(0,200,12,80);
}
static void ReadAirButtons(PlayState* play,Player* player) {
    u16 press=play->state.input[0].press.button;
    if (press&BTN_R) {
        BeginMove(play,player,BYRNA_POUND,CLIP_POUND,false);player->linearVelocity=0;
        Airborne(player,BYRNA_NORMAL_GRAVITY);player->actor.velocity.y=MIN(player->actor.velocity.y,-3);
    } else if ((press&BTN_B) && sByrna.move!=BYRNA_AIR_B) {
        BeginMove(play,player,BYRNA_AIR_B,CLIP_AIR_B,false);Airborne(player,BYRNA_NORMAL_GRAVITY);
    } else if ((press&BTN_A) && sByrna.move==BYRNA_AIR_IDLE) {
        BeginMove(play,player,BYRNA_AIR_A,CLIP_AIR_A,false);Airborne(player,BYRNA_AIR_GRAVITY);
        FaceTarget(play,player);player->yaw=player->actor.shape.rot.y;player->linearVelocity=BYRNA_AIR_SPEED;
    }
}
static void ByrnaAction(Player* player,PlayState* play) {
    if (!IsWielding(player) || IsPlayerBusy(player)) { EndMove(play,player);return; }
    SilenceVanillaBlade(player);
    if (ProcessHit(play,player)) {ReadAirButtons(play,player);return;}
    LinkAnimationHeader* clip=LoadClip(sByrna.clip);
    if (clip && player->skelAnime.animation!=(void*)clip) {
        LinkAnimation_Change(play,&player->skelAnime,clip,BYRNA_PLAY_SPEED*BYRNA_SAMPLES,sByrna.frame*BYRNA_SAMPLES,
            Animation_GetLastFrame(clip),sByrna.move==BYRNA_AIR_IDLE || sByrna.move==BYRNA_GUARD?ANIMMODE_LOOP:ANIMMODE_ONCE,0);
    }
    f32 before=sByrna.frame;
    bool done=LinkAnimation_Update(play,&player->skelAnime);
    f32 frame=OriginalFrame(player);sByrna.frame=frame;sByrna.ticks++;
    if (sByrna.move==BYRNA_GUARD) {GuardTick(play,player);return;}
    if (sByrna.move>=BYRNA_AIR_IDLE && sByrna.move<=BYRNA_POUND) {
        player->stateFlags3|=PLAYER_STATE3_MIDAIR;
        if (IsGrounded(player) && player->actor.velocity.y<=0 && sByrna.move!=BYRNA_REBOUND) {
            if (sByrna.move==BYRNA_POUND) LandPound(play,player);else EndMove(play,player);
            return;
        }
    }
    switch (sByrna.move) {
        case BYRNA_DRAW: case BYRNA_SLASH:
            Math_StepToF(&player->linearVelocity,0,1.0f);
            if (HitWindow(before,frame,sByrna.move==BYRNA_DRAW?18:0,sByrna.move==BYRNA_DRAW?52:40)) ForwardHit(play,player,false,false);
            if (done) EndMove(play,player);
            break;
        case BYRNA_SHEATHE:
            player->linearVelocity=0;
            if (done) {HandOver(player,ITEM_NONE);EndMove(play,player);}
            break;
        case BYRNA_DASH:
            SetImmunity(player,frame>=13 && frame<=50);
            if (frame>=13 && frame<50) {
                FaceTarget(play,player);player->yaw=player->actor.shape.rot.y;
                f32 distance=ActorIsAlive(play,player->focusActor)?Actor_WorldDistXZToActor(&player->actor,player->focusActor):9999;
                player->linearVelocity=distance<45?0:MIN(18.0f,(distance-40)/MAX(R_UPDATE_RATE*0.5f,0.1f))/3.0f;
            } else player->linearVelocity=0;
            if (HitWindow(before,frame,50,LastOriginalFrame(CLIP_DASH))) ForwardHit(play,player,false,false);
            if (done) {if(IsGrounded(player))EndMove(play,player);else StartAirIdle(play,player);}
            break;
        case BYRNA_LAUNCH:
            if (sByrna.impulse && !IsGrounded(player)) {
                ReadAirButtons(play,player);
                if(sByrna.move!=BYRNA_LAUNCH) break;
            }
            // Raise the real actor with the plant of the cane, rather than letting its tip sink through the floor.
            // These heights use both age skeletons and the same baked hand transform as the displayed cane.
            {
                s32 sample=MIN((s32)(frame*BYRNA_SAMPLES),104);
                f32 minimumY=sByrna.launchFloor+sVaultFloorLift[gSaveContext.linkAge][sample];
                if (player->actor.world.pos.y<minimumY) {
                    player->actor.world.pos.y=minimumY;
                    player->actor.bgCheckFlags&=~BGCHECKFLAG_GROUND;
                }
            }
            if (CrossedFrame(before,frame,16)) {
                player->actor.velocity.y=3;player->actor.bgCheckFlags&=~BGCHECKFLAG_GROUND;Airborne(player,0);sByrna.impulse=true;
            } else if (sByrna.impulse) player->actor.velocity.y=0;
            if (done) {StartAirIdle(play,player);player->actor.velocity.y=11;player->actor.bgCheckFlags&=~BGCHECKFLAG_GROUND;}
            break;
        case BYRNA_AIR_IDLE: {
            Airborne(player,BYRNA_AIR_GRAVITY);
            Input* input=&play->state.input[0];f32 magnitude=sqrtf(input->rel.stick_x*input->rel.stick_x+input->rel.stick_y*input->rel.stick_y);
            if (magnitude>10) {
                player->yaw=Camera_GetCamDirYaw(GET_ACTIVE_CAM(play))+Math_Atan2S(input->rel.stick_y,-input->rel.stick_x);
                player->linearVelocity=MIN(magnitude/60,1.0f)*8;
            } else Math_StepToF(&player->linearVelocity,0,0.5f);
            ReadAirButtons(play,player);break;
        }
        case BYRNA_AIR_B:
            ReadAirButtons(play,player);
            if(sByrna.move!=BYRNA_AIR_B) break;
            Airborne(player,BYRNA_NORMAL_GRAVITY);AirCylinderHit(play,player);
            if (done) StartAirIdle(play,player);
            break;
        case BYRNA_AIR_A:
            ReadAirButtons(play,player);
            if(sByrna.move!=BYRNA_AIR_A) break;
            Airborne(player,BYRNA_AIR_GRAVITY);FaceTarget(play,player);player->yaw=player->actor.shape.rot.y;
            player->linearVelocity=ActorIsAlive(play,player->focusActor) && Actor_WorldDistXZToActor(&player->actor,player->focusActor)<40?0:BYRNA_AIR_SPEED;
            ForwardHit(play,player,false,false);
            if (done) StartAirIdle(play,player);
            break;
        case BYRNA_REBOUND:
            ReadAirButtons(play,player);
            if(sByrna.move!=BYRNA_REBOUND) break;
            if (!sByrna.impulse && frame>=LastOriginalFrame(CLIP_REBOUND)*0.5f) {
                sByrna.impulse=true;player->actor.velocity.y=BYRNA_REBOUND_IMPULSE;player->actor.bgCheckFlags&=~BGCHECKFLAG_GROUND;
            }
            Airborne(player,sByrna.impulse?BYRNA_AIR_GRAVITY:0);
            if (!sByrna.impulse) player->actor.velocity.y=0;
            if (done) StartAirIdle(play,player);
            break;
        case BYRNA_POUND:
            Airborne(player,BYRNA_NORMAL_GRAVITY);ForwardHit(play,player,true,false);
            if (done) {
                player->skelAnime.curFrame=player->skelAnime.endFrame;
                player->skelAnime.playSpeed=0;
            }
            break;
        case BYRNA_LAND:
            ForwardHit(play,player,true,true);
            if (sByrna.ticks>=3) EndMove(play,player);
            break;
        default: EndMove(play,player);break;
    }
}

static void InterceptDraw(bool* should,va_list args) {
    s32 item=va_arg(args,s32);PlayState* play=gPlayState;
    if (!play || !*should || !IsWorn() || item<ITEM_SWORD_KOKIRI || item>ITEM_SWORD_BGS) return;
    Player* player=GET_PLAYER(play);
    if (IsWielding(player) || sByrna.active || IsPlayerBusy(player) || !IsGrounded(player)) return;
    *should=false;HandOver(player,item);BeginMove(play,player,BYRNA_DRAW,CLIP_DRAW,false);
}
static void TakeMeleeButton(PlayState* play,Player* player,int32_t action,bool* consumed,bool* started) {
    if (*consumed || !IsWielding(player) || !IsGrounded(player) || IsPlayerBusy(player) || sByrna.active ||
        !(play->state.input[0].press.button&BTN_B)) return;
    bool dash=fabsf(player->linearVelocity)>3.0f && player->controlStickDirections[player->controlStickDataIndex]==PLAYER_STICK_DIR_FORWARD;
    if (dash) StartDash(play,player,false);else BeginMove(play,player,BYRNA_SLASH,CLIP_SLASH,false);
    *consumed=true;*started=true;
}
static void TakeAButton(PlayState* play,Player* player,int32_t action,bool* consumed,bool* started) {
    if (*consumed || !IsWielding(player) || !IsGrounded(player) || IsPlayerBusy(player) || sByrna.active ||
        !(play->state.input[0].press.button&BTN_A)) return;
    bool targeted=Player_IsZTargeting(player);
    s32 dir=player->controlStickDirections[player->controlStickDataIndex];
    if (action==SOH_PLAYER_ACTION_ROLL && targeted) return;
    if (action==SOH_PLAYER_ACTION_ZTARGET_A && !targeted) return;
    if (targeted && dir>PLAYER_STICK_DIR_FORWARD) return; // Native left/right/back hops own their movement.
    if (targeted && dir==PLAYER_STICK_DIR_FORWARD && IsEnemy(player->focusActor)) StartDash(play,player,true);
    else if (!targeted && abs(play->state.input[0].rel.stick_x)+abs(play->state.input[0].rel.stick_y)<20) {
        if (player->putAwayCooldownTimer!=0) return;
        BeginMove(play,player,BYRNA_SHEATHE,CLIP_SHEATHE,false);
    }
    else StartLaunch(play,player);
    *consumed=true;*started=true;
}
static void TakeShieldButton(PlayState* play,Player* player,int32_t action,bool* consumed,bool* started) {
    if (*consumed || !IsWielding(player) || !IsGrounded(player) || IsPlayerBusy(player) || sByrna.active ||
        !(play->state.input[0].cur.button&BTN_R)) return;
    StartGuard(play,player);*consumed=true;*started=sByrna.active;
}
static void KeepButtonsFromVanilla(Player* player,Input* input) {
    if (!(input->cur.button&BTN_R)) sGuardRejected=false;
    if (!IsWielding(player) || IsPlayerBusy(player)) return;
    if (sByrna.active) {
        // The custom action reads raw input; a native fall transition must not also use these presses.
        input->cur.button&=~(BTN_A|BTN_B|BTN_R);input->press.button&=~(BTN_A|BTN_B|BTN_R);
        return;
    }
    if (!IsGrounded(player) && !(player->stateFlags2&PLAYER_STATE2_HOPPING) &&
        (input->press.button&(BTN_A|BTN_B|BTN_R))) {
        StartAirIdle(gPlayState,player);ReadAirButtons(gPlayState,player);
        input->press.button&=~(BTN_A|BTN_B|BTN_R);
    }
}
static void ResolveStaffAnimation(int32_t group,int32_t type,LinkAnimationHeader** anim) {
    if (!gPlayState || !IsWielding(GET_PLAYER(gPlayState))) return;
    if ((group==PLAYER_ANIMGROUP_wait || group==PLAYER_ANIMGROUP_waitL || group==PLAYER_ANIMGROUP_waitR ||
        group==PLAYER_ANIMGROUP_normal2fighter) && Player_IsZTargeting(GET_PLAYER(gPlayState))) *anim=LoadClip(CLIP_IDLE);
    else if (group==PLAYER_ANIMGROUP_walk || group==PLAYER_ANIMGROUP_run || group==PLAYER_ANIMGROUP_side_walk ||
        group==PLAYER_ANIMGROUP_side_walkL || group==PLAYER_ANIMGROUP_side_walkR)
        *anim=LoadClip(group==PLAYER_ANIMGROUP_run?CLIP_RUN:CLIP_WALK);
}
static void ResolveStaffHop(int32_t site,int32_t index,LinkAnimationHeader** anim) {
    if (!gPlayState || site!=SOH_PLAYER_ANIM_SITE_HOP || index%3 || !IsWielding(GET_PLAYER(gPlayState))) return;
    s32 dir=index/3;
    if (dir==1 || dir==2 || dir==3) *anim=LoadClip(dir==1?CLIP_HOP_RIGHT:dir==3?CLIP_HOP_LEFT:CLIP_BACKFLIP);
}
static void TickByrna(void) {
    PlayState* play=gPlayState;if (!play) return;
    Player* player=GET_PLAYER(play);
    if (sByrna.active && (!IsWielding(player) || IsPlayerBusy(player))) {EndMove(play,player);return;}
    if (sByrna.active && player->actionFunc!=ByrnaAction) {
        // Native air/floor transitions must not replace the requested aerial action on takeoff.
        if (sByrna.move>=BYRNA_LAUNCH && sByrna.move<=BYRNA_POUND && !IsPlayerBusy(player)) {
            Player_SetupAction(play,player,ByrnaAction,0);player->stateFlags3|=PLAYER_STATE3_MIDAIR;
        } else EndMove(play,player);
    }
    if (!sByrna.active && IsWielding(player) && !IsPlayerBusy(player)) {
        // Native idle/hop sites choose the clips; retain the common 1.4x original-frame clock.
        for (s32 i=0;i<CLIP_WALK;i++) if (sClips[i].values && player->skelAnime.animation==(void*)&sClips[i].header)
            player->skelAnime.playSpeed=BYRNA_PLAY_SPEED*BYRNA_SAMPLES;
    }
}
