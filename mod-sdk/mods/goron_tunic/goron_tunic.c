#include <stdbool.h>
#include <stdio.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include "z64items.h"
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "soh/ModApi/ModMenu/ModUI.h"
#include "overlays/ovl_Magic_Dark/ovl_Magic_Dark.h"
#ifdef _MSC_VER
#pragma warning(error: 4013)
#endif

#define OWNER "goron_tunic"
#define ENABLED "gMods.GoronTunic.Daruk"
#define RECHARGE "gMods.GoronTunic.RechargeSeconds"
static const SOHModApi* sApi;
static const char* const sHooks[]={"OnPlayerUpdate","OnPlayerShieldBlocked","OnCollisionResolveDamage","OnActorDrawEnd","OnLoadFile"};
static const SOHModRequirements sRequirements={sizeof(SOHModRequirements),sHooks,ARRAY_COUNT(sHooks)};
static struct {uint32_t charges,ticks;} sSaved={3,0};
static int sGrace;
static bool Wearing(void) {
    const char* custom=sApi->GetWornEquipment(SOH_EQUIP_SLOT_TUNIC);
    const char* form=SOH_MOD_API_HAS(sApi,GetActiveForm)?sApi->GetActiveForm():NULL;
    return CVarGetInteger(ENABLED,1) && CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC)==EQUIP_VALUE_TUNIC_GORON &&
        (!custom || !custom[0]) && (!form || !form[0]);
}
static bool Ready(PlayState* play,Player* p) {
    return Wearing() && (sSaved.charges>0 || sGrace>0) && play->pauseCtx.state==0 &&
        play->msgCtx.msgMode==MSGMODE_NONE && (play->state.input[0].cur.button&BTN_R) &&
        !(p->stateFlags1&(PLAYER_STATE1_DEAD|PLAYER_STATE1_IN_CUTSCENE|PLAYER_STATE1_LOADING|PLAYER_STATE1_GETTING_ITEM));
}
static uint32_t RechargeTicks(void){return (uint32_t)CLAMP(CVarGetInteger(RECHARGE,180),10,600)*20;}
static void Store(void){sApi->StorageSet(OWNER,"charges",&sSaved,sizeof(sSaved));}
static void Load(int32_t file){sSaved.charges=3;sSaved.ticks=0;sGrace=0;sApi->StorageGet(OWNER,"charges",&sSaved,sizeof(sSaved));
    if(sSaved.charges>3)sSaved.charges=3;if(sSaved.ticks>RechargeTicks())sSaved.ticks=RechargeTicks();}
static void Block(PlayState* play,Player* p) {
    if(sGrace>0)return;
    if(!sSaved.charges)return;
    --sSaved.charges;sGrace=10;
    if(!sSaved.ticks)sSaved.ticks=RechargeTicks();
    Store();
    p->invincibilityTimer=MAX(p->invincibilityTimer,10);
    Vec3f pos=p->actor.world.pos,zero={0,0,0};
    EffectSsBlast_SpawnWhiteCustomScale(play,&pos,&zero,&zero,80,180,8);
    Sfx_PlaySfxCentered(NA_SE_IT_SHIELD_REFLECT_MG);
}
static void OnGuard(PlayState* play,Player* p,Actor* attacker){if(Ready(play,p))Block(play,p);}
static void OnDamage(Actor* victim,ColliderInfo* attack,float* damage) {
    if(!gPlayState || victim!=&GET_PLAYER(gPlayState)->actor ||
       (*damage<=0 && (!attack || attack->toucher.effect==HIT_SPECIAL_EFFECT_NONE)))return;
    Player* p=GET_PLAYER(gPlayState);
    if(!Ready(gPlayState,p))return;
    Block(gPlayState,p);*damage=0;
    p->cylinder.base.acFlags&=~AC_HIT;p->cylinder.info.bumperFlags&=~BUMP_HIT;
    p->actor.colChkInfo.damage=0;p->actor.colChkInfo.damageEffect=0;
    p->actor.colChkInfo.acHitEffect=HIT_SPECIAL_EFFECT_NONE;p->knockbackType=PLAYER_KNOCKBACK_NONE;
}
static void Tick(void) {
    if(!gPlayState || gPlayState->pauseCtx.state)return;
    if(sGrace>0)--sGrace;
    if(sSaved.charges<3) {
        if(!sSaved.ticks)sSaved.ticks=RechargeTicks();
        if(--sSaved.ticks==0){++sSaved.charges;if(sSaved.charges<3)sSaved.ticks=RechargeTicks();Store();}
        else if(sSaved.ticks%20==0)Store();
    }
}
static void DrawGuard(Actor* actor,PlayState* play) {
    Player* p=GET_PLAYER(play);if(actor!=&p->actor || !Ready(play,p))return;
    Matrix_Push();Matrix_Translate(p->actor.world.pos.x,p->actor.world.pos.y+35,p->actor.world.pos.z,MTXMODE_NEW);
    Matrix_Scale(.95f,.55f,.95f,MTXMODE_APPLY);
    OPEN_DISPS(play->state.gfxCtx);Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
    gDPSetPrimColor(POLY_XLU_DISP++,0,0,255,180,65,sGrace?200:85);gDPSetEnvColor(POLY_XLU_DISP++,160,45,0,100);
    gSPDisplayList(POLY_XLU_DISP++,(Gfx*)sDiamondMaterialDL);
    gSPDisplayList(POLY_XLU_DISP++,Gfx_TwoTexScrollEx(play->state.gfxCtx,0,play->gameplayFrames*2,play->gameplayFrames*-4,32,32,1,0,play->gameplayFrames*-16,64,32,2,-4,0,-16));
    gSPDisplayList(POLY_XLU_DISP++,(Gfx*)sDiamondModelDL);
    CLOSE_DISPS(play->state.gfxCtx);Matrix_Pop();
}
static void DrawStatus(void){char text[96];snprintf(text,sizeof(text),"Charges: %u / 3    Recharge: %u s",sSaved.charges,(sSaved.ticks+19)/20);ModUI_Text(text);}
SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api){sApi=api;}
SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void){return &sRequirements;}
SOH_MOD_EXPORT void ModInit(void) {
    if(!SOH_MOD_API_HAS(sApi,StorageGet) || !SOH_MOD_API_HAS(sApi,StorageSet))return;
    Load(0);
    if(SOH_MOD_API_HAS(sApi,RegisterMenuSidebar) && SOH_MOD_API_HAS(sApi,RegisterMenuWidget)) {
        sApi->RegisterMenuSidebar("Enhancements","Goron Tunic","gMods.Menu",1);
        SOHModMenuWidget w={0};w.structSize=sizeof(w);w.section="Enhancements";w.sidebar="Goron Tunic";
        w.type=SOH_MOD_MENU_CHECKBOX;w.label="Daruk's Protection";w.cvar=ENABLED;w.defaultInt=1;
        w.tooltip="Wear the vanilla Goron Tunic and hold R. Three charges block incoming hits from all sides; each blocked hit consumes one charge. No magic cost.";sApi->RegisterMenuWidget(&w);
        w.type=SOH_MOD_MENU_INT_SLIDER;w.label="Recharge time (seconds)";w.cvar=RECHARGE;w.defaultInt=180;w.minInt=10;w.maxInt=600;w.tooltip="Time in active gameplay to recover one charge. Remaining charges and recharge progress are saved per file.";sApi->RegisterMenuWidget(&w);
        w.type=SOH_MOD_MENU_CUSTOM;w.label="Protection status";w.cvar=NULL;w.onChange=DrawStatus;sApi->RegisterMenuWidget(&w);
    }
    SOH_REGISTER_HOOK(sApi,OnPlayerUpdate,Tick);SOH_REGISTER_HOOK(sApi,OnPlayerShieldBlocked,OnGuard);
    SOH_REGISTER_HOOK(sApi,OnCollisionResolveDamage,OnDamage);SOH_REGISTER_HOOK_FOR_ID(sApi,OnActorDrawEnd,ACTOR_PLAYER,DrawGuard);
    SOH_REGISTER_HOOK(sApi,OnLoadFile,Load);
}
