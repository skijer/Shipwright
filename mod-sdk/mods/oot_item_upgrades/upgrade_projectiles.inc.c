#define BEAM_SPEED 80.0f
#define BEAM_FADE_RATE .05f
#define BEAM_TARGET_SCALE 12.0f
#define BEAM_DRAW_SCALE .02f
#define BEAM_DAMAGE 4
#define BEAM_DMG_FLAGS DMG_JUMP_KOKIRI
static const ALIGN_ASSET(2) char sSwordBeamDL[]="__OTR__objects/gameplay_keep/gSwordBeamDL";
static s16 sBeamActorId=-1;
typedef struct {
    Actor actor;
    ColliderCylinder collider;
    LightNode* light;
    LightInfo lightInfo;
    f32 life;
    f32 scroll;
} SwordBeam;

static ColliderCylinderInit sBeamColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2,
      { BEAM_DMG_FLAGS, 0x00, BEAM_DAMAGE },
      { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { 0, 60, -30, { 0, 0, 0 } },
};

static bool IsActorAlive(PlayState* play, Actor* actor) {
    for (s32 category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* it = play->actorCtx.actorLists[category].head; it != NULL; it = it->next) {
            if (it == actor) {
                return true;
            }
        }
    }
    return false;
}
// Aim assistance is applied once at launch, within the forward cone.
#define BEAM_AIM_CONE 0x1555
#define BEAM_AIM_BIAS 0x05B0
static bool BeamCandidate(Player* player,Actor* target,Vec3f* origin){
    if(!target||!target->update||(target->category!=ACTORCAT_ENEMY&&target->category!=ACTORCAT_BOSS))return false;
    s16 delta=Math_Vec3f_Yaw(origin,&target->focus.pos)-player->actor.shape.rot.y;
    return ABS(delta)<=BEAM_AIM_CONE&&Math_Vec3f_DistXYZ(origin,&target->focus.pos)<=800.0f;
}
static void BeamLaunchDirection(PlayState* play,Player* player,Vec3f* origin,s16* yaw,s16* pitch){
    *yaw=player->actor.shape.rot.y;*pitch=0;
    Actor* target=player->focusActor?player->focusActor:play->actorCtx.targetCtx.arrowPointedActor;
    if(!BeamCandidate(player,target,origin))target=NULL;
    float best=800.0f;
    const int categories[]={ACTORCAT_ENEMY,ACTORCAT_BOSS};
    if(!target)for(int i=0;i<2;i++)for(Actor* a=play->actorCtx.actorLists[categories[i]].head;a;a=a->next){
        if(!BeamCandidate(player,a,origin))continue;
        float distance=Math_Vec3f_DistXYZ(origin,&a->focus.pos);
        if(distance<best){best=distance;target=a;}
    }
    if(target){
        s16 delta=Math_Vec3f_Yaw(origin,&target->focus.pos)-*yaw;
        *yaw+=CLAMP(delta,-BEAM_AIM_BIAS,BEAM_AIM_BIAS);
        *pitch=CLAMP(Math_Vec3f_Pitch(origin,&target->focus.pos),-BEAM_AIM_BIAS,BEAM_AIM_BIAS);
    }
}

static void PlaceBeamLight(SwordBeam* beam) {
    u8 glow = (u8)(255.0f * beam->life);

    Lights_PointNoGlowSetInfo(&beam->lightInfo, beam->actor.world.pos.x, beam->actor.world.pos.y,
                              beam->actor.world.pos.z, glow, glow, (u8)(100.0f * beam->life), 800);
}
static void InitBeam(Actor* actor, PlayState* play) {
    SwordBeam* beam = (SwordBeam*)actor;
    Player* player = GET_PLAYER(play);

    beam->life = 1.0f;
    beam->scroll = 0.0f;
    Actor_SetScale(actor, 0.0f);
    actor->room = -1;
    actor->shape.rot.y = actor->world.rot.y + 0x8000;
    actor->shape.rot.x = -actor->world.rot.x;
    Collider_InitCylinder(play, &beam->collider);
    Collider_SetCylinder(play, &beam->collider, actor, &sBeamColliderInit);
    PlaceBeamLight(beam);
    beam->light = LightContext_InsertLight(play, &play->lightCtx, &beam->lightInfo);
    Audio_PlaySfxGeneral(NA_SE_IT_ROLLING_CUT_LV1, &player->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}
static void DestroyBeam(Actor* actor, PlayState* play) {
    SwordBeam* beam = (SwordBeam*)actor;

    Collider_DestroyCylinder(play, &beam->collider);
    if (beam->light != NULL) {
        LightContext_RemoveLight(play, &play->lightCtx, beam->light);
        beam->light = NULL;
    }
}

static bool HasBeamStruck(PlayState* play, SwordBeam* beam) {
    if (!(beam->collider.base.atFlags & AT_HIT)) {
        return false;
    }
    Actor* struck = beam->collider.base.at;
    if (struck != NULL) {
        Vec3f zero = { 0.0f, 0.0f, 0.0f };
        EffectSsBlast_SpawnWhiteCustomScale(play, &struck->focus.pos, &zero, &zero, 100, 250, 8);
    }
    return true;
}
static void UpdateBeam(Actor* actor, PlayState* play) {
    SwordBeam* beam = (SwordBeam*)actor;

    if (Math_StepToF(&beam->life, 0.0f, BEAM_FADE_RATE) || HasBeamStruck(play, beam)) {
        Actor_Kill(actor);
        return;
    }
    f32 flat = -BEAM_SPEED * Math_CosS(actor->world.rot.x);
    actor->world.pos.x += flat * Math_SinS(actor->shape.rot.y);
    actor->world.pos.z += flat * Math_CosS(actor->shape.rot.y);
    actor->world.pos.y += -BEAM_SPEED * Math_SinS(actor->world.rot.x);
    Math_SmoothStepToF(&actor->scale.x, BEAM_TARGET_SCALE, 0.6f, 2.0f, 0.0f);
    Actor_SetScale(actor, actor->scale.x);
    beam->scroll += 1.0f;

    beam->collider.dim.radius = (s16)(actor->scale.x * 5.0f);
    beam->collider.dim.pos.x = (s16)((Math_SinS(actor->shape.rot.y) * -5.0f * actor->scale.x) + actor->world.pos.x);
    beam->collider.dim.pos.y = (s16)actor->world.pos.y;
    beam->collider.dim.pos.z = (s16)((Math_CosS(actor->shape.rot.y) * -5.0f * actor->scale.z) + actor->world.pos.z);
    CollisionCheck_SetAT(play, &play->colChkCtx, &beam->collider.base);
    PlaceBeamLight(beam);
}
static void DrawBeam(Actor* actor, PlayState* play) {
    SwordBeam* beam = (SwordBeam*)actor;
    f32 alphaFrac = beam->life > 0.9f ? 1.0f : beam->life * (10.0f / 9.0f);
    u16 scroll = 0x1FF - ((u16)(s32)(beam->scroll * 10.0f) & 0x1FF);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Scale(BEAM_DRAW_SCALE, BEAM_DRAW_SCALE, BEAM_DRAW_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    Gfx* scrollDL = Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 16, 64, 1, 0, scroll, 32, 128);
    gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)scrollDL);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 170, 255, 255, (u8)(alphaFrac * 255.0f));
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 100, 255, 128);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sSwordBeamDL);
    CLOSE_DISPS(play->state.gfxCtx);
}
static void RegisterBeam(void) {
    SOHActorDefinition beam = { 0 };

    beam.structSize = sizeof(beam);
    beam.key = "nei.upgrades.master_beam";
    beam.description = "Fierce Deity sword beam";
    beam.category = ACTORCAT_ITEMACTION;
    beam.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    beam.objectId = OBJECT_GAMEPLAY_KEEP;
    beam.instanceSize = sizeof(SwordBeam);
    beam.init = InitBeam;
    beam.destroy = DestroyBeam;
    beam.update = UpdateBeam;
    beam.draw = DrawBeam;
    sBeamActorId = sApi->RegisterActor(&beam);
}

static void RealMasterBeam(Player* p,PlayState* play){
 if(sBeamActorId<0)return;
 Vec3f origin=p->bodyPartsPos[PLAYER_BODYPART_WAIST];s16 yaw,pitch;
 BeamLaunchDirection(play,p,&origin,&yaw,&pitch);
 Actor_Spawn(&play->actorCtx,play,sBeamActorId,origin.x,origin.y,origin.z,pitch,yaw,0,0);
}

typedef struct {Actor actor;ColliderCylinder collider;int life;bool returning;u16 activeTimer;s32 trailIndex;u32 trailFrame;} ThrownAxe;
static s16 sAxeActorId=-1;static bool sAxeAiming;
static ColliderCylinderInit sAxeCollider={
 {COLTYPE_NONE,AT_ON|AT_TYPE_PLAYER,AC_NONE,OC1_NONE,OC2_TYPE_PLAYER,COLSHAPE_CYLINDER},
 {ELEMTYPE_UNK2,{DMG_HAMMER,0,8},{0,0,0},TOUCH_ON|TOUCH_SFX_NORMAL,BUMP_NONE,OCELEM_NONE},
 {30,60,-30,{0,0,0}}
};
static void InitAxe(Actor* a,PlayState* play){ThrownAxe* t=(ThrownAxe*)a;t->life=30;t->returning=false;t->activeTimer=0;
 EffectBlureInit1 trail={0};
 const u8 p1Start[4]={255,150,50,255},p2Start[4]={255,80,20,64},end[4]={200,60,10,0};
 memcpy(trail.p1StartColor,p1Start,sizeof(p1Start));memcpy(trail.p2StartColor,p2Start,sizeof(p2Start));
 memcpy(trail.p1EndColor,end,sizeof(end));memcpy(trail.p2EndColor,end,sizeof(end));
 trail.elemDuration=8;trail.calcMode=0;trail.trailType=TRAIL_TYPE_BOOMERANG;
 t->trailIndex=-1;t->trailFrame=0xFFFFFFFF;Effect_Add(play,&t->trailIndex,EFFECT_BLURE1,0,0,&trail);
 // En_Boom starts with the default actor scale and zero shape rotation.
 // Its world rotation is applied explicitly in Draw, not by Actor_Draw.
 Actor_SetScale(a,.01f);a->shape.rot.x=a->shape.rot.y=a->shape.rot.z=0;
 Collider_InitCylinder(play,&t->collider);Collider_SetCylinder(play,&t->collider,a,&sAxeCollider);a->room=-1;}
static void DestroyAxe(Actor* a,PlayState* play){ThrownAxe* t=(ThrownAxe*)a;
 if(t->trailIndex>=0){Effect_Delete(play,t->trailIndex);t->trailIndex=-1;}
 Collider_DestroyCylinder(play,&t->collider);if(sThrownAxe==a)sThrownAxe=NULL;}
static void UpdateAxe(Actor* a,PlayState* play){
 ThrownAxe* t=(ThrownAxe*)a;Player* p=GET_PLAYER(play);Vec3f from=a->world.pos;
 if(--t->life<=0||(t->collider.base.atFlags&AT_HIT))t->returning=true;
 if(t->life< -90){Actor_Kill(a);return;}
 if(t->returning){Vec3f to=p->bodyPartsPos[PLAYER_BODYPART_L_HAND];float d=Math_Vec3f_DistXYZ(&to,&a->world.pos);
  if(d<40){Actor_Kill(a);return;}float step=40/d;a->world.pos.x+=(to.x-a->world.pos.x)*step;a->world.pos.y+=(to.y-a->world.pos.y)*step;a->world.pos.z+=(to.z-a->world.pos.z)*step;
 }else{float flat=20*Math_CosS(a->world.rot.x);a->world.pos.x+=flat*Math_SinS(a->world.rot.y);a->world.pos.z+=flat*Math_CosS(a->world.rot.y);a->world.pos.y-=20*Math_SinS(a->world.rot.x);
  CollisionPoly* poly;Vec3f hit;s32 bg;if(BgCheck_EntityLineTest1(&play->colCtx,&from,&a->world.pos,&hit,&poly,true,true,true,true,&bg)){a->world.pos=hit;t->returning=true;}
 }
 if(t->returning){a->world.rot.y=Math_Vec3f_Yaw(&from,&a->world.pos);a->world.rot.x=Math_Vec3f_Pitch(&from,&a->world.pos);}
 t->activeTimer++;Collider_UpdateCylinder(a,&t->collider);if(!t->returning)CollisionCheck_SetAT(play,&play->colChkCtx,&t->collider.base);
}
static void DrawAxe(Actor* a,PlayState* play){ThrownAxe* t=(ThrownAxe*)a;
 Matrix_RotateY(BINANG_TO_RAD(a->world.rot.y),MTXMODE_APPLY);
 Matrix_RotateZ(BINANG_TO_RAD(0x1F40),MTXMODE_APPLY);
 Matrix_RotateX(BINANG_TO_RAD(a->world.rot.x),MTXMODE_APPLY);
 // En_Boom samples the ribbon before the model's local spin and scale.
 // Keep the same +/-960 endpoints and avoid duplicate interpolated draws.
 if(t->trailIndex>=0&&t->trailFrame!=play->gameplayFrames){
  Vec3f left={-960.0f,0,0},right={960.0f,0,0},p1,p2;
  Matrix_MultVec3f(&left,&p1);Matrix_MultVec3f(&right,&p2);
  EffectBlure* trail=Effect_GetByIndex(t->trailIndex);
  if(trail)EffectBlure_AddVertex(trail,&p1,&p2);
  t->trailFrame=play->gameplayFrames;
 }
 Matrix_RotateY((f32)t->activeTimer*4000.0f*(M_PI/0x8000),MTXMODE_APPLY);
 Matrix_Scale(.15f,.15f,.15f,MTXMODE_APPLY);
 OPEN_DISPS(play->state.gfxCtx);Gfx_SetupDL_25Opa(play->state.gfxCtx);
 gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);gSPDisplayList(POLY_OPA_DISP++,(Gfx*)sAxe);CLOSE_DISPS(play->state.gfxCtx);}
void Player_SetupActionPreserveItemAction(PlayState*,Player*,PlayerActionFunc,s32);
void Player_SetupAction(PlayState*,Player*,PlayerActionFunc,s32);
void func_80839FFC(Player*,PlayState*);
void Player_AnimPlayLoop(PlayState*,Player*,LinkAnimationHeader*);
LinkAnimationHeader* Player_GetIdleAnim(Player*);
static const ALIGN_ASSET(2) char sAxeThrowAnim[]="__OTR__objects/gameplay_keep/gPlayerAnim_link_boom_throwR";
static void AxeThrowAction(Player* p,PlayState* play){
 Player_ZeroSpeedXZ(p);if(LinkAnimation_Update(play,&p->skelAnime))func_80839FFC(p,play);
}
static void AxeAimAction(Player* p,PlayState* play){
 if((play->state.input[0].press.button&(BTN_CUP|BTN_A|BTN_B))||p->invincibilityTimer<0||Weapon(p)!=CH_HAMMER){
  play->state.input[0].press.button&=~BTN_CUP;Z64Aiming_Release(p,play);sAxeAiming=false;func_80839FFC(p,play);return;
 }
 // Like the rods, aiming rotates the held melee pose through Z64Aiming;
 // do not loop a ranged-item ready animation or run its upper action here.
 Z64Aiming_Update(p,play);Player_ZeroSpeedXZ(p);LinkAnimation_Update(play,&p->skelAnime);
}
static void DrawAxeAim(void){
 if(!gPlayState||!sAxeAiming)return;
 Matrix_Push();Z64Aiming_DrawReticle(gPlayState,GET_PLAYER(gPlayState),600.0f,(Z64AimingReticleStyle){Z64AIMING_TOGGLE_BOOMERANG,Z64AIMING_TINT_LAUNCH});Matrix_Pop();
}
static void ThrowAxe(Player* p,PlayState* play){
 Vec3f at=p->bodyPartsPos[PLAYER_BODYPART_L_HAND];s16 yaw=p->actor.shape.rot.y,pitch=0;if(sAxeAiming){Z64Aiming_GetDirection(p,&yaw,&pitch);Z64Aiming_Release(p,play);sAxeAiming=false;}
 if(sAxeActorId>=0)sThrownAxe=Actor_Spawn(&play->actorCtx,play,sAxeActorId,at.x,at.y,at.z,pitch,yaw,0,0);
 if(sThrownAxe){Player_SetupActionPreserveItemAction(play,p,AxeThrowAction,0);LinkAnimation_PlayOnce(play,&p->skelAnime,(LinkAnimationHeader*)sAxeThrowAnim);Audio_PlayActorSound2(&p->actor,NA_SE_IT_BOOMERANG_THROW);}else if(p->actionFunc==AxeAimAction)func_80839FFC(p,play);
}
void ArmsHook_Shoot(ArmsHook*,PlayState*);
void ArmsHook_Wait(ArmsHook*,PlayState*);
static Actor* sHookActor;static bool sHookFlying;static Vec3f sHookLast;
static void ResetProjectiles(void){sHookActor=NULL;sHookFlying=false;sThrownAxe=NULL;sAxeAiming=false;Z64Aiming_Drop();}
static void UpdateActor(void* ref){
 Actor* a=(Actor*)ref;if(!gPlayState)return;SmashProp(a);if(a->id!=ACTOR_ARMS_HOOK||Level(CH_HOOK)<3)return;
 Player* p=GET_PLAYER(gPlayState);if(p->heldItemAction!=PLAYER_IA_LONGSHOT||CustomItemRegistry_GetHeldKey())return;
 ArmsHook* h=(ArmsHook*)a;bool flying=h->actionFunc==ArmsHook_Shoot;
 if(sHookActor!=a){sHookActor=a;sHookFlying=false;sHookLast=a->world.pos;}
 if(flying&&!sHookFlying){Actor_SetProjectileSpeed(a,40.0f);h->timer=26*CVarGetFloat("gCheats.HookshotReachMultiplier",1.0f);}
 if(flying&&sHookFlying&&h->timer<=0){
  Vec3f delta={a->world.pos.x-sHookLast.x,a->world.pos.y-sHookLast.y,a->world.pos.z-sHookLast.z};
  float step=sqrtf(SQ(delta.x)+SQ(delta.y)+SQ(delta.z));float remaining=Math_Vec3f_DistXYZ(&a->world.pos,&p->actor.world.pos);
  if(step<remaining){a->world.pos.x+=delta.x;a->world.pos.y+=delta.y;a->world.pos.z+=delta.z;
   if(h->grabbed&&h->grabbed->update&&h->grabbed->id!=ACTOR_BG_SPOT06_OBJECTS){h->grabbed->world.pos.x+=delta.x;h->grabbed->world.pos.y+=delta.y;h->grabbed->world.pos.z+=delta.z;}
  }
  if(p->actor.parent==a){p->actor.velocity.x*=2;p->actor.velocity.y*=2;p->actor.velocity.z*=2;}
 }
 sHookFlying=flying;sHookLast=a->world.pos;
}
static void Update(void){
 PlayState* play=gPlayState;if(!play)return;
 if(sPendingGiveChain>=0&&play->pauseCtx.state==0){
  if(Level(sPendingGiveChain)>=sLength[sPendingGiveChain]){sPendingGiveChain=-1;sGiveStatus="Already fully upgraded.";}
  else if(sApi->GiveCustomItem(play,sPickup[sPendingGiveChain])){sPendingGiveChain=-1;sGiveStatus="Item granted.";}
 }
 Player* p=GET_PLAYER(play);int c=Weapon(p);
 if(c==CH_KOKIRI){
  bool gilded=Level(CH_KOKIRI)>=3;
  for(int q=0;q<2;q++){
   u32 flags=p->meleeWeaponQuads[q].info.toucher.dmgFlags;
   if(flags&DMG_SLASH)flags=(flags&~DMG_SLASH)|(gilded?DMG_SLASH_GIANT:DMG_SLASH_MASTER);
   if(flags&DMG_SPIN_ATTACK)flags=(flags&~DMG_SPIN_ATTACK)|(gilded?DMG_SPIN_GIANT:DMG_SPIN_MASTER);
   if(flags&DMG_JUMP_SLASH)flags=(flags&~DMG_JUMP_SLASH)|(gilded?DMG_JUMP_GIANT:DMG_JUMP_MASTER);
   p->meleeWeaponQuads[q].info.toucher.dmgFlags=flags;
  }
 }
 bool swing=p->meleeWeaponState!=0;AdvanceSwing(p,play);
 if(sThrownAxe&&!IsActorAlive(play,sThrownAxe))sThrownAxe=NULL;
 if(c==CH_HAMMER){
  if(p->meleeWeaponState&&p->skelAnime.endFrame>0){float progress=p->skelAnime.curFrame/p->skelAnime.endFrame;p->skelAnime.playSpeed=.35f+1.15f*progress*progress;}
  if(!swing&&p->linearVelocity>6)p->linearVelocity=6;
  Input* in=&play->state.input[0];if(!sThrownAxe&&!sAxeAiming&&!swing&&(in->press.button&BTN_CUP)){if(Z64Aiming_Request(p,play)){sAxeAiming=true;Player_SetupAction(play,p,AxeAimAction,1);Player_AnimPlayLoop(play,p,Player_GetIdleAnim(p));}}
  if(sAxeAiming){Z64Aiming_HoldState(p);for(int b=1;b<8;b++)if(gSaveContext.equips.buttonItems[b]==ITEM_HAMMER&&(in->press.button&((u16[]){BTN_B,BTN_CLEFT,BTN_CDOWN,BTN_CRIGHT,BTN_DUP,BTN_DDOWN,BTN_DLEFT,BTN_DRIGHT})[b]))ThrowAxe(p,play);}

 }else if(sAxeAiming){Z64Aiming_Release(p,play);sAxeAiming=false;if(p->actionFunc==AxeAimAction)func_80839FFC(p,play);}
}
static void AdvanceSwing(Player* p,PlayState* play){
 bool swinging=p->meleeWeaponState!=0;
 if(swinging&&!sPrevSwing){
  sSwingId++;
  if(Weapon(p)==CH_MASTER&&gSaveContext.health>=gSaveContext.healthCapacity)RealMasterBeam(p,play);
 }
 sPrevSwing=swinging;
}
static void RegisterProjectiles(void){
 RegisterBeam();SOHActorDefinition d={0};d.structSize=sizeof(d);d.key="nei.upgrades.thrown_axe";d.description="Gerudo Axe tomahawk";
 d.category=ACTORCAT_ITEMACTION;d.actorFlags=ACTOR_FLAG_UPDATE_CULLING_DISABLED|ACTOR_FLAG_DRAW_CULLING_DISABLED;d.objectId=OBJECT_GAMEPLAY_KEEP;
 d.instanceSize=sizeof(ThrownAxe);d.init=InitAxe;d.destroy=DestroyAxe;d.update=UpdateAxe;d.draw=DrawAxe;sAxeActorId=sApi->RegisterActor(&d);
}

