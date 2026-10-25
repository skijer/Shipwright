#include <stdbool.h>
#include <math.h>
#include <string.h>
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "soh/ModApi/ModApi.h"
#include "soh/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"
#include "soh/ResourceManagerHelpers.h"
#include "objects/object_niw/object_niw.h"
#include "objects/object_gi_niwatori/object_gi_niwatori.h"
#include "textures/icon_item_static/icon_item_static.h"
#include "textures/item_name_static/item_name_static.h"
#include "partner_control.h"
#include "overlays/actors/ovl_En_Niw/z_en_niw.h"
#include "limb_models.inc.c"
void func_80AB5BF8(EnNiw*,PlayState*,s16);
s32 EnNiw_OverrideLimbDraw(PlayState*,s32,Gfx**,Vec3f*,Vec3s*,void*);
s32 Player_UpperAction_CarryActor(Player*,PlayState*);

#define COJIRO_KEY "nei.equip.cojiro"
#define COJIRO_REWARD "nei.cojiro.trade_reward"
#define COJIRO_RECOVERY 400
typedef struct {
    EnNiw niw;
    u8 hits,invulnerable,controlled,held;Input input;s16 cameraYaw;
    s16 voiceTimer,flapTimer,stepTimer,wanderTimer,fleeTimer;Vec3f wander;
} Cojiro;
static const SOHModApi* sApi;
static Cojiro* sCojiro;
static s16 sActorId=-1,sRecovery,sSpawnDelay;
static bool sRegistered;
static const char* sHooks[]={"OnGameFrameUpdate","OnSceneInit","OnLoadGame","OnCustomEquipOwnedChange","OnPlayerUpdate"};
static const SOHModRequirements sRequirements={sizeof(SOHModRequirements),sHooks,ARRAY_COUNT(sHooks)};
static ColliderCylinderInit sCollider={
    {COLTYPE_NONE,AT_NONE,AC_ON|AC_TYPE_ALL,OC1_ON|OC1_TYPE_ALL,OC2_TYPE_1,COLSHAPE_CYLINDER},
    {ELEMTYPE_UNK0,{0,0,0},{0xFFFFFFFF,0,0},TOUCH_NONE,BUMP_ON,OCELEM_ON},
    {15,28,0,{0,0,0}}
};
static bool Enabled(void){return CustomEquipRegistry_IsOwned(COJIRO_KEY)&&CustomEquipRegistry_IsToggleOn(COJIRO_KEY);}
static bool Live(PlayState* play,const Actor* wanted){
    if(!wanted)return false;
    for(int c=0;c<ACTORCAT_MAX;++c)for(Actor*a=play->actorCtx.actorLists[c].head;a;a=a->next)
        if(a==wanted)return a->update!=NULL;
    return false;
}
static bool Busy(PlayState* play){
    Player*p=GET_PLAYER(play);
    return !p||play->pauseCtx.state||play->msgCtx.msgMode!=MSGMODE_NONE||play->csCtx.state!=CS_STATE_IDLE||
        play->transitionTrigger!=TRANS_TRIGGER_OFF||(p->stateFlags1&(PLAYER_STATE1_DEAD|PLAYER_STATE1_IN_CUTSCENE|PLAYER_STATE1_GETTING_ITEM));
}
static void Detach(Cojiro*self,PlayState*play){
    Player*p=GET_PLAYER(play);
    if(p&&p->heldActor==&self->niw.actor){p->heldActor=NULL;p->stateFlags1&=~PLAYER_STATE1_CARRYING_ACTOR;}
    if(p&&p->interactRangeActor==&self->niw.actor)p->interactRangeActor=NULL;
    if(p&&p->actor.child==&self->niw.actor)p->actor.child=NULL;
    self->niw.actor.parent=NULL;
}
static Actor* PartnerActor(PlayState*play){return sCojiro&&Live(play,&sCojiro->niw.actor)&&Enabled()?&sCojiro->niw.actor:NULL;}
static bool Begin(PlayState*play){
    if(!PartnerActor(play)||Busy(play)||sCojiro->niw.actor.parent)return false;
    sCojiro->controlled=true;memset(&sCojiro->input,0,sizeof(Input));return true;
}
static void End(PlayState*play){if(PartnerActor(play)){sCojiro->controlled=false;sCojiro->niw.actor.speedXZ=0;}}
static void InputPartner(PlayState*play,const Input*input,s16 yaw){
    if(PartnerActor(play)&&sCojiro->controlled){sCojiro->input=*input;sCojiro->cameraYaw=yaw;}
}
static void RegisterPartner(void){
    if(sRegistered||!SOH_MOD_API_HAS(sApi,FindService))return;
    NeiRegisterPartnerV1 reg=(NeiRegisterPartnerV1)sApi->FindService("nei.partner.register.v1");
    static const NeiPartnerV1 provider={sizeof(provider),COJIRO_KEY,"Cojiro",PartnerActor,Begin,End,InputPartner};
    if(reg)sRegistered=reg(&provider);
}
static void InitActor(Actor*actor,PlayState*play){
    Cojiro*self=(Cojiro*)actor;Actor_SetScale(actor,.01f);actor->gravity=-2;actor->minVelocityY=-2;actor->room=-1;
    ActorShape_Init(&actor->shape,0,ActorShadow_DrawCircle,18);
    SkelAnime_InitFlex(play,&self->niw.skelAnime,(FlexSkeletonHeader*)gCuccoSkel,(AnimationHeader*)gCuccoAnim,self->niw.jointTable,self->niw.morphTable,16);
    Collider_InitCylinder(play,&self->niw.collider);Collider_SetCylinder(play,&self->niw.collider,actor,&sCollider);
    sCojiro=self;Actor_UpdateBgCheckInfo(play,actor,18,13,0,5);Actor_SetFocus(actor,18);
}
static void DestroyActor(Actor*actor,PlayState*play){
    Cojiro*self=(Cojiro*)actor;Detach(self,play);Collider_DestroyCylinder(play,&self->niw.collider);
    ResourceMgr_UnregisterSkeleton(&self->niw.skelAnime);if(sCojiro==self)sCojiro=NULL;
}
static void Recall(Cojiro*self,PlayState*play){
    Player*p=GET_PLAYER(play);if(!(p->actor.bgCheckFlags&BGCHECKFLAG_GROUND))return;
    self->niw.actor.world.pos=self->niw.actor.prevPos=p->actor.world.pos;self->niw.actor.velocity=(Vec3f){0,0,0};self->niw.actor.speedXZ=0;
    Actor_UpdateBgCheckInfo(play,&self->niw.actor,18,13,0,5);
}
static void Animate(Cojiro*self,PlayState*play){
    EnNiw*niw=&self->niw;Actor*actor=&niw->actor;
    bool airborne=actor->parent||!(actor->bgCheckFlags&BGCHECKFLAG_GROUND);
    bool running=actor->speedXZ>.2f;
    if(niw->timer1>0)--niw->timer1;
    if(niw->timer2>0)--niw->timer2;
    func_80AB5BF8(niw,play,airborne?2:running?1:0);
    niw->skelAnime.playSpeed=airborne||running?2:1;SkelAnime_Update(&niw->skelAnime);
    if(self->voiceTimer>0)--self->voiceTimer;
    if(self->voiceTimer==0){
        Audio_PlayActorSound2(actor,airborne||self->fleeTimer?NA_SE_EV_CHICKEN_CRY_A:NA_SE_EV_CHICKEN_CRY_N);
        self->voiceTimer=airborne?30:150+(s16)Rand_ZeroFloat(150);
    }
    if(self->flapTimer>0)--self->flapTimer;
    if(airborne&&self->flapTimer==0){Audio_PlayActorSound2(actor,NA_SE_EN_DEKU_WAKEUP);self->flapTimer=7;}
}
static void Follow(Cojiro*self,PlayState*play){
    Actor*actor=&self->niw.actor;Player*p=GET_PLAYER(play);
    f32 distance=Actor_WorldDistXZToActor(actor,&p->actor);
    if(distance>600||fabsf(actor->world.pos.y-p->actor.world.pos.y)>220||(actor->bgCheckFlags&BGCHECKFLAG_WATER)){
        Recall(self,play);self->wanderTimer=0;return;
    }
    if(self->stepTimer>0)--self->stepTimer;
    f32 speed=0;s16 yaw=actor->world.rot.y;
    if(self->fleeTimer>0){--self->fleeTimer;speed=6;yaw=actor->yawTowardsPlayer+0x8000;}
    else if(distance>110){speed=MIN(8,(distance-55)*.12f);yaw=actor->yawTowardsPlayer;self->wanderTimer=0;}
    else {
        if(self->wanderTimer>0)--self->wanderTimer;
        else {
            self->wanderTimer=45+(s16)Rand_ZeroFloat(55);
            s16 angle=(s16)Rand_CenteredFloat(65536);f32 radius=35+Rand_ZeroFloat(55);
            self->wander=(Vec3f){p->actor.world.pos.x+Math_SinS(angle)*radius,p->actor.world.pos.y,
                                p->actor.world.pos.z+Math_CosS(angle)*radius};
        }
        if(self->wanderTimer>25&&Math_Vec3f_DistXZ(&actor->world.pos,&self->wander)>12){
            speed=3;yaw=Math_Vec3f_Yaw(&actor->world.pos,&self->wander);
        }
    }
    Math_SmoothStepToS(&actor->world.rot.y,yaw,3,0x1400,0x40);actor->shape.rot.y=actor->world.rot.y;
    Math_ApproachF(&actor->speedXZ,speed,.4f,1.5f);
    if(speed>.2f&&self->stepTimer==0&&(actor->bgCheckFlags&BGCHECKFLAG_GROUND)){
        actor->velocity.y=(actor->bgCheckFlags&BGCHECKFLAG_WALL)?7:3.5f;
        actor->bgCheckFlags&=~BGCHECKFLAG_GROUND;self->stepTimer=6;
    }
    Actor_OfferGetItem(actor,play,GI_NONE,50,25);
}
static void GlidePlayer(void){
    PlayState*play=gPlayState;if(!play||!Enabled()||!sCojiro||!Live(play,&sCojiro->niw.actor))return;
    Player*p=GET_PLAYER(play);
    if(p->heldActor!=&sCojiro->niw.actor||sCojiro->niw.actor.parent!=&p->actor||
        !(p->stateFlags1&PLAYER_STATE1_CARRYING_ACTOR)||(p->actor.bgCheckFlags&BGCHECKFLAG_GROUND)||p->actor.velocity.y>0)return;
    // Same fall settings as Player_UpperAction_CarryActor for a native cucco.
    p->actor.minVelocityY=-2;p->actor.gravity=-.5f;p->actor.velocity.y=MAX(p->actor.velocity.y,-2);
    p->fallStartHeight=p->actor.world.pos.y;
}
static s32 CarryUpper(Player*p,PlayState*play){
    s32 result=Player_UpperAction_CarryActor(p,play);
    GlidePlayer(); // Apply at the native carry-action phase, before player movement integrates gravity.
    return result;
}
static void PlayerUpdated(void){
    PlayState*play=gPlayState;if(!play)return;Player*p=GET_PLAYER(play);
    bool carrying=Enabled()&&sCojiro&&Live(play,&sCojiro->niw.actor)&&p->heldActor==&sCojiro->niw.actor;
    if(carrying&&p->upperActionFunc==Player_UpperAction_CarryActor)p->upperActionFunc=CarryUpper;
    else if(!carrying&&p->upperActionFunc==CarryUpper)p->upperActionFunc=Player_UpperAction_CarryActor;
    GlidePlayer();
}
static bool TakeHit(Cojiro*self){
    Actor*actor=&self->niw.actor;
    if(self->niw.collider.base.acFlags&AC_HIT){
        self->niw.collider.base.acFlags&=~AC_HIT;
        if(!self->invulnerable){
            self->invulnerable=16;++self->hits;self->fleeTimer=40;
            Audio_PlayActorSound2(actor,NA_SE_EV_CHICKEN_CRY_M);
            if(self->hits>=3){sRecovery=COJIRO_RECOVERY;Actor_Kill(actor);return true;}
        }
    }
    return false;
}
static void UpdateActor(Actor*actor,PlayState*play){
    Cojiro*self=(Cojiro*)actor;Player*p=GET_PLAYER(play);
    if(!Enabled()){Actor_Kill(actor);return;}if(Busy(play))return;
    actor->room=-1;
    actor->minVelocityY=-2;
    Actor_SetFocus(actor,18);
    if(self->invulnerable)--self->invulnerable;
    if(TakeHit(self))return;
    if(actor->parent){
        if(!self->held){Audio_PlayActorSound2(actor,NA_SE_EV_CHICKEN_CRY_M);self->voiceTimer=30;}
        self->held=true;actor->speedXZ=0;Animate(self,play);
        Collider_UpdateCylinder(actor,&self->niw.collider);
        if(!self->invulnerable)CollisionCheck_SetAC(play,&play->colChkCtx,&self->niw.collider.base);
        return;
    }
    if(self->held){self->held=false;actor->bgCheckFlags&=~BGCHECKFLAG_GROUND;}
    if(self->controlled){
        f32 x=self->input.cur.stick_x,y=self->input.cur.stick_y,m=sqrtf(x*x+y*y);
        if(m>10){s16 yaw=self->cameraYaw+(s16)(atan2f(x,y)*32768/M_PI);Math_SmoothStepToS(&actor->world.rot.y,yaw,2,0x1800,0x40);}
        actor->shape.rot.y=actor->world.rot.y;Math_ApproachF(&actor->speedXZ,m>10?MIN(8,(m-10)*.13f):0,.4f,1.5f);
        if((self->input.press.button&BTN_A)&&(actor->bgCheckFlags&BGCHECKFLAG_GROUND)){
            actor->velocity.y=8;actor->bgCheckFlags&=~BGCHECKFLAG_GROUND;
        }
        if(!(actor->bgCheckFlags&BGCHECKFLAG_GROUND)&&(self->input.cur.button&BTN_A))actor->velocity.y=MAX(actor->velocity.y,-2);
        if(self->input.press.button&BTN_B)Audio_PlayActorSound2(actor,NA_SE_EV_CHICKEN_CRY_M);
        self->input.press.button=0;
    }else Follow(self,play);
    Actor_MoveXZGravity(actor);Actor_UpdateBgCheckInfo(play,actor,18,13,30,7);
    Animate(self,play);Collider_UpdateCylinder(actor,&self->niw.collider);
    CollisionCheck_SetOC(play,&play->colChkCtx,&self->niw.collider.base);
    if(!self->invulnerable)CollisionCheck_SetAC(play,&play->colChkCtx,&self->niw.collider.base);
}
static s32 OverrideLimb(PlayState*play,s32 limb,Gfx**dl,Vec3f*pos,Vec3s*rot,void*ptr){
    Cojiro*self=ptr;EnNiw_OverrideLimbDraw(play,limb,dl,pos,rot,&self->niw);
    for(s32 i=0;i<ARRAY_COUNT(sModels);++i)if(limb==sModels[i].limb){*dl=(Gfx*)sModels[i].path;break;}
    return false;
}
static void DrawActor(Actor*actor,PlayState*play){
    Cojiro*self=(Cojiro*)actor;OPEN_DISPS(play->state.gfxCtx);Gfx_SetupDL_25Opa(play->state.gfxCtx);
    SkelAnime_DrawSkeletonOpa(play,&self->niw.skelAnime,OverrideLimb,NULL,self);
    CLOSE_DISPS(play->state.gfxCtx);
}
static void DrawGet(PlayState*play,GetItemEntry*entry){
    OPEN_DISPS(play->state.gfxCtx);Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++,(Gfx*)gGiCojiroColorDL);gSPDisplayList(POLY_OPA_DISP++,(Gfx*)gGiChickenDL);
    gSPDisplayList(POLY_OPA_DISP++,(Gfx*)gGiChickenEyesDL);CLOSE_DISPS(play->state.gfxCtx);
}
static void Owned(const char*key,bool owned){
    if(!strcmp(key,COJIRO_KEY)&&owned){CustomEquipRegistry_SetToggle(COJIRO_KEY,true);
        if(SOH_MOD_API_HAS(sApi,SetRandoFlag))sApi->SetRandoFlag(COJIRO_REWARD,true);}
}
static void Scene(s16 scene){sCojiro=NULL;sSpawnDelay=5;}
static void Load(s32 file){sCojiro=NULL;sSpawnDelay=5;sRecovery=0;}
static void Tick(void){
    RegisterPartner();PlayState*play=gPlayState;if(!play||Busy(play))return;
    if(sCojiro&&!Live(play,&sCojiro->niw.actor))sCojiro=NULL;
    if(SOH_MOD_API_HAS(sApi,GetRandoFlag)&&Flags_GetRandomizerInf(RAND_INF_ADULT_TRADES_LW_TRADE_COJIRO)&&
       !sApi->GetRandoFlag(COJIRO_REWARD)&&!CustomEquipRegistry_IsOwned(COJIRO_KEY)){
        sApi->GiveCustomItem(play,COJIRO_KEY);return;
    }
    if(sRecovery>0){--sRecovery;return;}
    if(!Enabled()||sCojiro||sActorId<0)return;
    if(sSpawnDelay>0){--sSpawnDelay;return;}
    Player*p=GET_PLAYER(play);if(!(p->actor.bgCheckFlags&BGCHECKFLAG_GROUND))return;
    Vec3f pos=p->actor.world.pos;Actor_Spawn(&play->actorCtx,play,sActorId,pos.x,pos.y,pos.z,0,p->actor.shape.rot.y,0,0);sSpawnDelay=20;
}
SOH_MOD_EXPORT void ModSetApi(const SOHModApi*api){sApi=api;}
SOH_MOD_EXPORT const SOHModRequirements*ModGetRequirements(void){return &sRequirements;}
SOH_MOD_EXPORT void ModInit(void){
    if(!SOH_MOD_API_HAS(sApi,RegisterActor))return;
    if(SOH_MOD_API_HAS(sApi,RegisterRandoFlag))sApi->RegisterRandoFlag(COJIRO_REWARD);
    SOHCustomEquipDefinition equip={0};equip.structSize=sizeof(equip);equip.key=COJIRO_KEY;
    equip.iconPath=gItemIconCojiroTex;equip.namePath=gCojiroItemNameENGTex;
    equip.getItemText="%bCojiro%w wants to travel with you!&He follows you wherever you go.^"
        "Press %b\x9F%w on his pause slot to&call him or send him away.&Three hits make him rest briefly.^"
        "Play the %rCommand Melody%w&to control him directly.";
    equip.pauseText="%bCojiro&%b\x9F%w calls him or sends him away.&Command Melody lets you guide him.";
    equip.slot=SOH_EQUIP_SLOT_UPGRADE;equip.page=0;equip.row=2;equip.column=0;equip.toggles=1;
    equip.ageRequirement=SOH_CUSTOM_ITEM_AGE_ANY;equip.getItemEntry.drawFunc=DrawGet;
    // The equipment registry creates the pickup; this block also puts it into new seeds.
    static const SOHCustomItemRandomizer randomizer = {
        sizeof(SOHCustomItemRandomizer), 0, SOH_CUSTOM_ITEM_TYPE_EQUIP, 0, 1,
        COJIRO_KEY, COJIRO_KEY, "",
    };
    equip.randomizer = &randomizer;
    if(!CustomEquipRegistry_Register(&equip))return;
    SOHActorDefinition actor={0};actor.structSize=sizeof(actor);actor.key="nei.cojiro_partner";actor.description="Cojiro companion";
    actor.category=ACTORCAT_NPC;actor.actorFlags=ACTOR_FLAG_UPDATE_CULLING_DISABLED|ACTOR_FLAG_DRAW_CULLING_DISABLED|
        ACTOR_FLAG_ATTENTION_ENABLED|ACTOR_FLAG_FRIENDLY|ACTOR_FLAG_CAN_PRESS_SWITCHES;
    actor.objectId=OBJECT_GAMEPLAY_KEEP;actor.instanceSize=sizeof(Cojiro);actor.init=InitActor;actor.destroy=DestroyActor;
    actor.update=UpdateActor;actor.draw=DrawActor;sActorId=sApi->RegisterActor(&actor);
    SOH_REGISTER_HOOK(sApi,OnGameFrameUpdate,Tick);SOH_REGISTER_HOOK(sApi,OnSceneInit,Scene);
    SOH_REGISTER_HOOK(sApi,OnPlayerUpdate,PlayerUpdated);
    SOH_REGISTER_HOOK(sApi,OnLoadGame,Load);SOH_REGISTER_HOOK(sApi,OnCustomEquipOwnedChange,Owned);
}
