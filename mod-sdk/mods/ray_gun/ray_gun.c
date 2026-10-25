#include <stdbool.h>
#include <math.h>
#include <string.h>
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "z64items.h"
#include "z64aiming.h"
#include "soh/ResourceManagerHelpers.h"
#include "gun_socket.inc.c"
#define GUN_KEY "nei.ray_gun"
static const SOHModApi* sApi;
static const ALIGN_ASSET(2) char sGun[]="__OTR__objects/object_nei_ray_gun/gun_dl";
static const ALIGN_ASSET(2) char sBolt[]="__OTR__objects/object_nei_ray_gun/bolt_dl";
static const ALIGN_ASSET(2) char sIcon[]="__OTR__textures/icon_item_custom/gRayGunIconTex";
static const ALIGN_ASSET(2) char sName[]="__OTR__textures/item_name_custom/gRayGunNameTex";
static const char* sClips[2][3]={
    {"__OTR__misc/link_animetion/nei_ray_gun_adult_draw","__OTR__misc/link_animetion/nei_ray_gun_adult_hold","__OTR__misc/link_animetion/nei_ray_gun_adult_shoot"},
    {"__OTR__misc/link_animetion/nei_ray_gun_child_draw","__OTR__misc/link_animetion/nei_ray_gun_child_hold","__OTR__misc/link_animetion/nei_ray_gun_child_shoot"}
};
typedef struct{Actor actor;ColliderCylinder collider;s16 timer;}Ray;
typedef struct{Actor*actor;Vec3f velocity;s16 ticks;}Impact;
static Impact sImpacts[16];
static s16 sRayId=-1;
static u8 sPhase;
static bool sFired,sMuzzleReady;
static Vec3f sMuzzle;
static LinkAnimationHeader* sAnimation;
static Vec3s sLocomotion[PLAYER_LIMB_BUF_COUNT];
static u8 sArmMask[PLAYER_LIMB_MAX]={[PLAYER_LIMB_R_SHOULDER]=1,[PLAYER_LIMB_R_FOREARM]=1,[PLAYER_LIMB_R_HAND]=1};
static bool sBaseQueued;
static Vec3f sShotPosition;
static Actor*sShotTarget;
static s16 sShotYaw,sShotPitch;
static f32 sShotGravity;
static bool sShotLocked;
static const char* sHooks[]={"OnPlayerUpdate","OnPlayerFilterInput","OnPlayerPostLimbDraw","OnSceneInit","OnCollisionResolveDamage","OnActorUpdate","OnActorKill"};
static const SOHModRequirements sRequirements={sizeof(SOHModRequirements),sHooks,ARRAY_COUNT(sHooks)};
static ColliderCylinderInit sCollider={
    {COLTYPE_NONE,AT_ON|AT_TYPE_PLAYER,AC_NONE,OC1_NONE,OC2_NONE,COLSHAPE_CYLINDER},
    {ELEMTYPE_UNK0,{DMG_UNBLOCKABLE,0,255},{0,0,0},TOUCH_ON|TOUCH_SFX_NORMAL,BUMP_NONE,OCELEM_NONE},
    {12,24,-12,{0,0,0}}
};
static bool Live(PlayState*play,const Actor*w){if(!w)return false;for(int c=0;c<ACTORCAT_MAX;++c)for(Actor*a=play->actorCtx.actorLists[c].head;a;a=a->next)if(a==w)return a->update!=NULL;return false;}
static bool Held(void){const char*key=CustomItemRegistry_GetHeldKey();return key&&!strcmp(key,GUN_KEY);}
static bool Enemy(Actor*a){return a->category==ACTORCAT_ENEMY||a->category==ACTORCAT_BOSS;}
static void ImpactActor(PlayState*play,Actor*actor,s16 yaw){
    if(!Live(play,actor)||actor->id==ACTOR_PLAYER||actor->id==sRayId)return;
    if(!Enemy(actor)){Actor_Kill(actor);return;}
    for(int i=0;i<ARRAY_COUNT(sImpacts);++i)if(sImpacts[i].actor==actor)return;
    for(int i=0;i<ARRAY_COUNT(sImpacts);++i)if(!sImpacts[i].actor){
        sImpacts[i]=(Impact){actor,{Math_SinS(yaw)*12,10,Math_CosS(yaw)*12},12};
        actor->velocity.y=10;actor->speedXZ=12;actor->world.rot.y=yaw;actor->bgCheckFlags&=~BGCHECKFLAG_GROUND;
        Actor_SetColorFilter(actor,0x4000,255,0,12);return;
    }
    Actor_Kill(actor); // Do not make a crowded scene immune when the impact slots are full.
}
static void ResolveDamage(Actor*victim,ColliderInfo*attack,f32*damage){
    PlayState*play=gPlayState;if(!play||!attack)return;
    for(Actor*source=play->actorCtx.actorLists[ACTORCAT_ITEMACTION].head;source;source=source->next)
        if(source->id==sRayId&&attack==&((Ray*)source)->collider.info){
            *damage=255;ImpactActor(play,victim,source->world.rot.y);return;
        }
}
static void Killed(void*ptr){if(ptr==sShotTarget)sShotTarget=NULL;for(int i=0;i<ARRAY_COUNT(sImpacts);++i)if(sImpacts[i].actor==ptr)sImpacts[i].actor=NULL;}
static void ActorUpdated(void*ptr){
    Actor*actor=ptr;PlayState*play=gPlayState;if(!play)return;
    for(int i=0;i<ARRAY_COUNT(sImpacts);++i){Impact*hit=&sImpacts[i];if(hit->actor!=actor)continue;
        if(!actor->update){hit->actor=NULL;return;}
        actor->world.pos.x+=hit->velocity.x;actor->world.pos.z+=hit->velocity.z;
        actor->world.pos.y+=hit->velocity.y;hit->velocity.y-=1.2f;
        Actor_UpdateBgCheckInfo(play,actor,20,MAX(10,actor->colChkInfo.cylRadius),30,7);
        // Native AC damage has the first opportunity to run its death animation.
        // Actors which reject that damage are still removed by this deliberately lethal item.
        if(--hit->ticks<=0){Actor_Kill(actor);hit->actor=NULL;}
        return;
    }
}
static void RayInit(Actor*actor,PlayState*play){Ray*self=(Ray*)actor;Actor_SetScale(actor,.025f);self->timer=35;
    actor->velocity=(Vec3f){Math_SinS(actor->world.rot.y)*Math_CosS(actor->world.rot.x)*55,-Math_SinS(actor->world.rot.x)*55,Math_CosS(actor->world.rot.y)*Math_CosS(actor->world.rot.x)*55};
    Collider_InitCylinder(play,&self->collider);Collider_SetCylinder(play,&self->collider,actor,&sCollider);
}
static void RayDestroy(Actor*actor,PlayState*play){Collider_DestroyCylinder(play,&((Ray*)actor)->collider);}
static Actor* SegmentActor(PlayState*play,Actor*ray,Vec3f from,Vec3f to){
    Vec3f d={to.x-from.x,to.y-from.y,to.z-from.z};f32 length=d.x*d.x+d.y*d.y+d.z*d.z,best=1.1f;Actor*found=NULL;
    if(length<.01f)return NULL;
    for(int c=0;c<ACTORCAT_MAX;++c)for(Actor*a=play->actorCtx.actorLists[c].head;a;a=a->next){
        if(a==ray||a->id==ACTOR_PLAYER||a->id==sRayId||!a->update||!a->draw)continue;
        Vec3f p=a->focus.pos;if(Math_Vec3f_DistXYZ(&p,&a->world.pos)>200){p=a->world.pos;p.y+=MAX(15,a->colChkInfo.cylHeight*.5f);}
        f32 t=((p.x-from.x)*d.x+(p.y-from.y)*d.y+(p.z-from.z)*d.z)/length;if(t<0||t>1||t>=best)continue;
        Vec3f q={from.x+d.x*t,from.y+d.y*t,from.z+d.z*t};f32 radius=MAX(14,a->colChkInfo.cylRadius);
        if(Math_Vec3f_DistXYZ(&p,&q)<radius+8){found=a;best=t;}
    }return found;
}
static void RayUpdate(Actor*actor,PlayState*play){
    Ray*self=(Ray*)actor;if(--self->timer<=0){Actor_Kill(actor);return;}
    if(self->collider.base.atFlags&AT_HIT){ImpactActor(play,self->collider.base.at,actor->world.rot.y);Actor_Kill(actor);return;}
    Vec3f from=actor->world.pos,to={from.x+actor->velocity.x,from.y+actor->velocity.y,from.z+actor->velocity.z},hit;
    CollisionPoly*poly;s32 bg;bool wall=BgCheck_EntityLineTest1(&play->colCtx,&from,&to,&hit,&poly,true,true,true,true,&bg);
    if(wall)to=hit;Actor*target=SegmentActor(play,actor,from,to);
    if(target){ImpactActor(play,target,actor->world.rot.y);if(!Enemy(target)){Actor_Kill(actor);return;}}
    actor->world.pos=to;if(wall){
        DynaPolyActor*dyna=DynaPoly_GetActor(&play->colCtx,bg);
        if(dyna)ImpactActor(play,&dyna->actor,actor->world.rot.y);
        Actor_Kill(actor);return;
    }
    Collider_UpdateCylinder(actor,&self->collider);CollisionCheck_SetAT(play,&play->colChkCtx,&self->collider.base);
}
static void RayDraw(Actor*actor,PlayState*play){OPEN_DISPS(play->state.gfxCtx);Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++,(Gfx*)sBolt);CLOSE_DISPS(play->state.gfxCtx);}
static void PlayUpper(Player*p,PlayState*play,u8 phase){
    sAnimation=ResourceMgr_LoadPlayerAnimAsHeader(sClips[LINK_IS_ADULT?0:1][phase]);if(!sAnimation)return;
    sPhase=phase;LinkAnimation_Change(play,&p->upperSkelAnime,sAnimation,phase==1?0:1,0,
        phase==1?0:Animation_GetLastFrame(sAnimation),ANIMMODE_ONCE,-4);
}
static void ShotDirection(Player*p,PlayState*play,Vec3f from,s16*yaw,s16*pitch){
    if(Live(play,sShotTarget)){
        Vec3f to=sShotTarget->focus.pos;
        if(Math_Vec3f_DistXYZ(&to,&sShotTarget->world.pos)>200){to=sShotTarget->world.pos;to.y+=MAX(15,sShotTarget->colChkInfo.cylHeight*.5f);}
        *yaw=Math_Vec3f_Yaw(&from,&to);*pitch=Math_Vec3f_Pitch(&from,&to);
    }else{*yaw=sShotYaw;*pitch=sShotPitch;}
}
static void AimShot(Player*p,PlayState*play){
    s16 yaw,pitch;ShotDirection(p,play,p->actor.focus.pos,&yaw,&pitch);
    p->actor.focus.rot.y=yaw;p->actor.focus.rot.x=pitch;
    // Native bow rendering rotates the entire torso matrix with these two fields.
    p->upperLimbRot.y=(s16)(yaw-p->actor.shape.rot.y);
    p->upperLimbRot.x=CLAMP(pitch,-14000,14000);p->upperLimbRot.z=0;
    p->unk_6AE_rotFlags|=UNK6AE_ROT_FOCUS_X|UNK6AE_ROT_FOCUS_Y|UNK6AE_ROT_UPPER_X|UNK6AE_ROT_UPPER_Y;
}
static void StopShot(Player*p){
    if(sShotLocked&&p->actor.gravity==0)p->actor.gravity=sShotGravity;
    sShotLocked=false;sShotTarget=NULL;
}
static bool ShotInterrupted(Player*p,PlayState*play){
    return (p->stateFlags1&(PLAYER_STATE1_DEAD|PLAYER_STATE1_DAMAGED|PLAYER_STATE1_TALKING|PLAYER_STATE1_IN_CUTSCENE|PLAYER_STATE1_GETTING_ITEM))||
        play->transitionTrigger!=TRANS_TRIGGER_OFF||play->csCtx.state!=CS_STATE_IDLE;
}
static void HoldShot(Player*p,PlayState*play){
    p->actor.world.pos=p->actor.prevPos=sShotPosition;
    p->linearVelocity=p->actor.speedXZ=0;p->actor.velocity=(Vec3f){0,0,0};p->actor.gravity=0;
    AimShot(p,play);
}
static void Filter(Player*p,Input*input){
    if(!sShotLocked||!Held())return;
    u32 start=input->press.button&BTN_START,z=input->cur.button&BTN_Z;
    memset(input,0,sizeof(*input));input->press.button=start;input->cur.button=z;
}
static void Fire(Player*p,PlayState*play){
    Vec3f from=p->actor.focus.pos;if(sMuzzleReady&&Math_Vec3f_DistXYZ(&sMuzzle,&from)<120)from=sMuzzle;
    s16 yaw,pitch;ShotDirection(p,play,from,&yaw,&pitch);
    Actor_Spawn(&play->actorCtx,play,sRayId,from.x,from.y,from.z,pitch,yaw,0,0);
    Audio_PlayActorSound2(&p->actor,NA_SE_IT_MAGIC_ARROW_SHOT);
}
static void TakeOut(PlayState*play,Player*p){sFired=false;sMuzzleReady=false;sBaseQueued=false;PlayUpper(p,play,0);}
static void QueueLocomotion(Player*p,PlayState*play){
    if(sBaseQueued)return;
    // Animation work is deferred. Capture after the native locomotion load, before its upper-body copy.
    AnimationContext_SetCopyAll(play,p->skelAnime.limbCount,sLocomotion,p->skelAnime.jointTable);
    sBaseQueued=true;
}
static void RestoreLocomotion(Player*p,PlayState*play){
    if(!sBaseQueued)return;
    if(sPhase==2)AnimationContext_SetCopyAll(play,p->skelAnime.limbCount,p->skelAnime.jointTable,p->upperSkelAnime.jointTable);
    else AnimationContext_SetCopyFalse(play,p->skelAnime.limbCount,p->skelAnime.jointTable,sLocomotion,sArmMask);
    sBaseQueued=false;
}
static int32_t Upper(Player*p,PlayState*play){
    if(sPhase==2&&ShotInterrupted(p,play)){StopShot(p);sPhase=1;sAnimation=NULL;sBaseQueued=false;return 1;}
    QueueLocomotion(p,play);
    if(!sAnimation||p->upperSkelAnime.animation!=sAnimation)PlayUpper(p,play,sPhase);
    bool done=LinkAnimation_Update(play,&p->upperSkelAnime);
    if(sPhase==2){HoldShot(p,play);if(!sFired&&p->upperSkelAnime.curFrame>=4){Fire(p,play);sFired=true;}}
    if(done&&sPhase!=1)PlayUpper(p,play,1);
    return 1;
}
static void Use(Player*p,PlayState*play){
    if(sPhase!=1||sRayId<0)return;
    sShotPosition=p->actor.world.pos;sShotGravity=p->actor.gravity;sShotLocked=true;
    sShotTarget=Live(play,p->focusActor)?p->focusActor:NULL;
    sShotYaw=p->actor.shape.rot.y;sShotPitch=0;
    if(!sShotTarget&&Z64Aiming_IsAiming())Z64Aiming_GetDirection(p,&sShotYaw,&sShotPitch);
    sFired=false;PlayUpper(p,play,2);HoldShot(p,play);
}
static void PutAway(Player*p,PlayState*play){StopShot(p);Z64Aiming_Release(p,play);sAnimation=NULL;sMuzzleReady=false;}
static void PlayerUpdated(void){
    PlayState*play=gPlayState;if(!play)return;Player*p=GET_PLAYER(play);
    if(sShotLocked&&ShotInterrupted(p,play)){
        StopShot(p);sBaseQueued=false;sPhase=1;sAnimation=NULL;
    }
    RestoreLocomotion(p,play);
    for(int i=0;i<ARRAY_COUNT(sImpacts);++i)if(sImpacts[i].actor&&!Live(play,sImpacts[i].actor))sImpacts[i].actor=NULL;
    if(!Held()){StopShot(p);if(Z64Aiming_IsAiming())Z64Aiming_Release(p,play);return;}
    if(play->pauseCtx.state||play->msgCtx.msgMode!=MSGMODE_NONE)return;
    Z64Items_KeepHeld(sApi);
    if(sPhase==2){HoldShot(p,play);return;}
    if(sShotLocked)StopShot(p);
    if(play->state.input[0].press.button&BTN_CUP){
        if(Z64Aiming_IsAiming())Z64Aiming_Release(p,play);else if(sPhase==1)Z64Aiming_Request(p,play);
    }
    if(Z64Aiming_IsAiming())Z64Aiming_Update(p,play);
}
static void Hand(PlayState*play,Player*p,s32 limb){
    if(!Held()||limb!=PLAYER_LIMB_R_HAND)return;
    OPEN_DISPS(play->state.gfxCtx);Matrix_Push();int age=LINK_IS_ADULT?0:1;
    Matrix_RotateZYX(sGunSocket[age][0],sGunSocket[age][1],sGunSocket[age][2],MTXMODE_APPLY);
    Vec3f tip={0,0,1550};Matrix_MultVec3f(&tip,&sMuzzle);sMuzzleReady=true;
    gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++,(Gfx*)sGun);Matrix_Pop();CLOSE_DISPS(play->state.gfxCtx);
    if(Z64Aiming_IsAiming())Z64Aiming_DrawReticle(play,p,1500,(Z64AimingReticleStyle){Z64AIMING_TOGGLE_HOOKSHOT,Z64AIMING_TINT_LAUNCH});
}
static void GetDraw(PlayState*play,GetItemEntry*entry){OPEN_DISPS(play->state.gfxCtx);Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(.025f,.025f,.025f,MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++,(Gfx*)sGun);CLOSE_DISPS(play->state.gfxCtx);}
static void Scene(s16 scene){memset(sImpacts,0,sizeof(sImpacts));sAnimation=NULL;sPhase=0;sMuzzleReady=sBaseQueued=sShotLocked=false;sShotTarget=NULL;Z64Aiming_Drop();}
SOH_MOD_EXPORT void ModSetApi(const SOHModApi*api){sApi=api;}
SOH_MOD_EXPORT const SOHModRequirements*ModGetRequirements(void){return &sRequirements;}
SOH_MOD_EXPORT void ModInit(void){
    SOHActorDefinition actor={0};actor.structSize=sizeof(actor);actor.key="nei.ray_gun.bolt";actor.description="Ray Gun beam";
    actor.category=ACTORCAT_ITEMACTION;actor.objectId=OBJECT_GAMEPLAY_KEEP;actor.instanceSize=sizeof(Ray);
    actor.actorFlags=ACTOR_FLAG_UPDATE_CULLING_DISABLED|ACTOR_FLAG_DRAW_CULLING_DISABLED;
    actor.init=RayInit;actor.destroy=RayDestroy;actor.update=RayUpdate;actor.draw=RayDraw;
    sRayId=sApi->RegisterActor(&actor);
    SOHCustomItemDefinition item=Z64Items_Define(GUN_KEY,sIcon,sName);
    Z64Items_SetButtons(&item,SOH_CUSTOM_ITEM_C_BUTTON|SOH_CUSTOM_ITEM_DPAD);Z64Items_SetPlacement(&item,4,0,0);
    Z64Items_SetTextbox(&item,"You got the %rRay Gun%w!&Press its %y\xA1%w button to draw it,&then press again to fire.^"
        "Its ray throws enemies back&and kills whatever it hits.&Press %y\xA5%w to aim.");
    Z64Items_SetPauseText(&item,"%rRay Gun&%y\xA1%w draws it, then fires.&%y\xA5%w aims. Its ray is lethal.");
    Z64Items_SetAction(&item,TakeOut,Upper);Z64Items_SetHeldCallbacks(&item,Use,PutAway,NULL);
    item.getItemEntry.drawFunc=GetDraw;item.modelGroup=PLAYER_MODELGROUP_DEFAULT;
    if(!Z64Items_Register(sApi,&item))return;
    SOH_REGISTER_HOOK(sApi,OnPlayerUpdate,PlayerUpdated);SOH_REGISTER_HOOK(sApi,OnPlayerPostLimbDraw,Hand);
    SOH_REGISTER_HOOK(sApi,OnSceneInit,Scene);SOH_REGISTER_HOOK(sApi,OnCollisionResolveDamage,ResolveDamage);
    SOH_REGISTER_HOOK(sApi,OnActorUpdate,ActorUpdated);
    SOH_REGISTER_HOOK(sApi,OnActorKill,Killed);
    SOH_REGISTER_HOOK(sApi,OnPlayerFilterInput,Filter);
}
