#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include "z64items.h"
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "soh/ModApi/ModMenu/ModUI.h"
#include "overlays/actors/ovl_En_Firefly/z_en_firefly.h"
#ifdef _MSC_VER
#pragma warning(error: 4013)
#endif

#define ENABLED "gMods.ElementalMirror.Enabled"
#define BOLT_KEY "nei.elemental_mirror.bolt"
typedef enum { COUNTER_NONE,COUNTER_BLUE_FIRE,COUNTER_ICE,COUNTER_THUNDER,COUNTER_LIGHT } Counter;
typedef struct {Actor actor;ColliderCylinder hit;Actor* target;int life;Counter kind;} MirrorBolt;
static const SOHModApi* sApi;
static int16_t sBoltId=-1;
static uint32_t sNextReflect;
static Actor* sLastSource;
static const char* const sHooks[]={"OnPlayerShieldBlocked","OnCollisionResolveDamage","OnActorDestroy","OnSceneInit"};
static const SOHModRequirements sRequirements={sizeof(SOHModRequirements),sHooks,ARRAY_COUNT(sHooks)};

static bool OutsideTwinrova(PlayState* play) {
    if(play->sceneNum==SCENE_SPIRIT_TEMPLE_BOSS)return false;
    for(Actor* a=play->actorCtx.actorLists[ACTORCAT_BOSS].head;a;a=a->next)
        if(a->id==ACTOR_BOSS_TW && a->update)return false;
    return true;
}
static bool Ready(PlayState* play,Player* p) {
    const char* custom=sApi->GetWornEquipment(SOH_EQUIP_SLOT_SHIELD);
    return CVarGetInteger(ENABLED,1) && CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD)==EQUIP_VALUE_SHIELD_MIRROR &&
        (!custom || !custom[0]) && OutsideTwinrova(play) && play->pauseCtx.state==0 &&
        (p->stateFlags1&PLAYER_STATE1_SHIELDING) &&
        !(p->stateFlags1&(PLAYER_STATE1_DEAD|PLAYER_STATE1_IN_CUTSCENE|PLAYER_STATE1_LOADING));
}
static Counter CounterFor(Actor* source,ColliderInfo* attack) {
    // Explicit exceptions take precedence over the general opposite-element rule.
    if(source && source->id==ACTOR_EN_FZ)return COUNTER_BLUE_FIRE;
    if(source && source->id==ACTOR_EN_FIREFLY && ((EnFirefly*)source)->onFire)return COUNTER_THUNDER;
    u32 flags=attack?attack->toucher.dmgFlags:0;
    u8 effect=attack?attack->toucher.effect:0;
    if(source && (source->id==ACTOR_EN_ANUBICE_FIRE || source->id==ACTOR_EN_FW))return COUNTER_ICE;
    if(source && source->id==ACTOR_EN_ICE_HONO)return COUNTER_BLUE_FIRE;
    if((flags&(DMG_MAGIC_ICE|DMG_ARROW_ICE)) || effect==HIT_SPECIAL_EFFECT_ICE)return COUNTER_BLUE_FIRE;
    if((flags&(DMG_MAGIC_FIRE|DMG_ARROW_FIRE)) || effect==HIT_SPECIAL_EFFECT_FIRE)return COUNTER_ICE;
    if(effect==HIT_SPECIAL_EFFECT_ELECTRIC)return COUNTER_LIGHT;
    if(flags&(DMG_MAGIC_LIGHT|DMG_ARROW_LIGHT|DMG_MIR_RAY))return COUNTER_LIGHT;
    return COUNTER_NONE;
}
static u32 DamageFlags(Counter kind) {
    return kind==COUNTER_BLUE_FIRE?DMG_MAGIC_FIRE:kind==COUNTER_ICE?DMG_MAGIC_ICE:DMG_MAGIC_LIGHT;
}
static bool LiveTarget(PlayState* play,Actor* target) {
    if(!target)return false;
    for(int category=0;category<ACTORCAT_MAX;++category)
        for(Actor* a=play->actorCtx.actorLists[category].head;a;a=a->next)
            if(a==target)return a->update!=NULL;
    return false;
}
static void BoltInit(Actor* actor,PlayState* play) {
    MirrorBolt* b=(MirrorBolt*)actor;b->kind=(Counter)actor->params;b->life=40;b->target=NULL;
    static ColliderCylinderInit init={
        {COLTYPE_NONE,AT_ON|AT_TYPE_PLAYER,AC_NONE,OC1_NONE,OC2_NONE,COLSHAPE_CYLINDER},
        {ELEMTYPE_UNK2,{DMG_MAGIC_LIGHT,0,2},{0,0,0},TOUCH_ON|TOUCH_SFX_NORMAL,BUMP_NONE,OCELEM_NONE},
        {12,24,-12,{0,0,0}}
    };
    Collider_InitCylinder(play,&b->hit);Collider_SetCylinder(play,&b->hit,actor,&init);
    b->hit.info.toucher.dmgFlags=DamageFlags(b->kind);
}
static void BoltDestroy(Actor* actor,PlayState* play){Collider_DestroyCylinder(play,&((MirrorBolt*)actor)->hit);}
static void BoltUpdate(Actor* actor,PlayState* play) {
    MirrorBolt* b=(MirrorBolt*)actor;
    if(--b->life<=0 || (b->hit.base.atFlags&AT_HIT) || !OutsideTwinrova(play)){Actor_Kill(actor);return;}
    Vec3f next=actor->world.pos;
    if(LiveTarget(play,b->target)) {
        Vec3f goal=b->target->focus.pos;
        if(Math_Vec3f_DistXYZ(&goal,&b->target->world.pos)>200 || Math_Vec3f_DistXYZ(&goal,&b->target->world.pos)<1){goal=b->target->world.pos;goal.y+=30;}
        f32 dx=goal.x-next.x,dy=goal.y-next.y,dz=goal.z-next.z,dist=sqrtf(dx*dx+dy*dy+dz*dz);
        f32 step=dist>18?18/dist:1;next.x+=dx*step;next.y+=dy*step;next.z+=dz*step;
    } else {b->target=NULL;next.x+=Math_SinS(actor->world.rot.y)*18;next.z+=Math_CosS(actor->world.rot.y)*18;}
    Vec3f contact;CollisionPoly* poly=NULL;s32 bg;
    if(BgCheck_EntityLineTest1(&play->colCtx,&actor->world.pos,&next,&contact,&poly,true,true,true,true,&bg)){Actor_Kill(actor);return;}
    actor->world.pos=next;Collider_UpdateCylinder(actor,&b->hit);CollisionCheck_SetAT(play,&play->colChkCtx,&b->hit.base);
}
static void BoltDraw(Actor* actor,PlayState* play) {
    MirrorBolt* b=(MirrorBolt*)actor;
    Color_RGB8 color=b->kind==COUNTER_BLUE_FIRE?(Color_RGB8){70,130,255}:b->kind==COUNTER_ICE?(Color_RGB8){120,240,255}:
        b->kind==COUNTER_THUNDER?(Color_RGB8){255,235,80}:(Color_RGB8){255,255,180};
    Matrix_Push();Matrix_Translate(actor->world.pos.x,actor->world.pos.y,actor->world.pos.z,MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);Matrix_Scale(.008f,.008f,.008f,MTXMODE_APPLY);
    OPEN_DISPS(play->state.gfxCtx);Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++,0,0,color.r,color.g,color.b,230);gDPSetEnvColor(POLY_XLU_DISP++,color.r/2,color.g/2,color.b/2,100);
    gSPMatrix(POLY_XLU_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++,(Gfx*)gEffFlash1DL);CLOSE_DISPS(play->state.gfxCtx);Matrix_Pop();
}
static bool Reflect(PlayState* play,Player* p,Actor* source,ColliderInfo* hit) {
    if(!Ready(play,p))return false;
    Counter kind=CounterFor(source,hit);if(kind==COUNTER_NONE)return false;
    // A stream or several colliders from one attack cannot flood the actor list.
    if(play->gameplayFrames<sNextReflect && source==sLastSource)return true;
    Vec3f pos=p->bodyPartsPos[PLAYER_BODYPART_R_HAND];
    Actor* spawned=Actor_Spawn(&play->actorCtx,play,sBoltId,pos.x,pos.y,pos.z,0,p->actor.shape.rot.y,0,kind);
    if(!spawned)return false;
    MirrorBolt* b=(MirrorBolt*)spawned;
    Actor* target=source;
    if(source && source->parent && (source->parent->category==ACTORCAT_ENEMY || source->parent->category==ACTORCAT_BOSS))target=source->parent;
    if(target && (target->category==ACTORCAT_ENEMY || target->category==ACTORCAT_BOSS))b->target=target;
    else if(p->focusActor)b->target=p->focusActor;
    sLastSource=source;sNextReflect=play->gameplayFrames+8;
    Sfx_PlaySfxCentered(NA_SE_IT_SHIELD_REFLECT_MG);return true;
}
static void OnGuard(PlayState* play,Player* p,Actor* source){Reflect(play,p,source,p->shieldQuad.info.acHitInfo);}
static void OnDamage(Actor* victim,ColliderInfo* attack,float* damage) {
    PlayState* play=gPlayState;if(!play)return;Player* p=GET_PLAYER(play);
    if(victim==&p->actor) {
        if(Reflect(play,p,p->cylinder.base.ac,attack)) {
            *damage=0;p->cylinder.base.acFlags&=~AC_HIT;p->cylinder.info.bumperFlags&=~BUMP_HIT;
            p->actor.colChkInfo.damage=0;p->actor.colChkInfo.damageEffect=0;
            p->actor.colChkInfo.acHitEffect=HIT_SPECIAL_EFFECT_NONE;
        }
        return;
    }
    // OoT has no thunder damage flag. Own thunder bolts use the magic-light slot
    // and deliver real collision damage even on Keese's zero-damage light table.
    for(Actor* a=play->actorCtx.actorLists[ACTORCAT_MISC].head;a;a=a->next) {
        if(a->id==sBoltId && a->update) {
            MirrorBolt* b=(MirrorBolt*)a;
            if(attack==&b->hit.info && b->kind==COUNTER_THUNDER) {
                if(*damage<2)*damage=2;
                Color_RGBA8 prim={255,255,180,255},env={100,140,255,255};
                EffectSsLightning_Spawn(play,&victim->focus.pos,&prim,&env,15,0,6,2);
            }
        }
    }
}
static void ForgetActor(void* ref) {
    Actor* dead=(Actor*)ref;if(sLastSource==dead)sLastSource=NULL;
    if(!gPlayState)return;
    for(Actor* a=gPlayState->actorCtx.actorLists[ACTORCAT_MISC].head;a;a=a->next)
        if(a->id==sBoltId && ((MirrorBolt*)a)->target==dead)((MirrorBolt*)a)->target=NULL;
}
static void Reset(int16_t scene){sNextReflect=0;sLastSource=NULL;}
static void DrawList(void) {
    ModUI_Text("Freezard / ice -> blue fire");ModUI_Text("Fire Keese -> thunder magic");
    ModUI_Text("Other fire -> ice; electricity -> light");ModUI_Text("Light magic / mirror ray -> light");
    ModUI_Text("Physical attacks: vanilla shield. Disabled during Twinrova.");
}
SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api){sApi=api;}
SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void){return &sRequirements;}
SOH_MOD_EXPORT void ModInit(void) {
    if(!SOH_MOD_API_HAS(sApi,RegisterActor))return;
    SOHActorDefinition d={0};d.structSize=sizeof(d);d.key=BOLT_KEY;d.description="Elemental Mirror counter bolt";
    d.category=ACTORCAT_MISC;d.actorFlags=ACTOR_FLAG_UPDATE_CULLING_DISABLED;d.objectId=OBJECT_GAMEPLAY_KEEP;
    d.instanceSize=sizeof(MirrorBolt);d.init=BoltInit;d.destroy=BoltDestroy;d.update=BoltUpdate;d.draw=BoltDraw;
    sBoltId=sApi->RegisterActor(&d);if(sBoltId<0)return;
    if(SOH_MOD_API_HAS(sApi,RegisterMenuSidebar) && SOH_MOD_API_HAS(sApi,RegisterMenuWidget)) {
        sApi->RegisterMenuSidebar("Enhancements","Elemental Mirror Shield","gMods.Menu",1);
        SOHModMenuWidget w={0};w.structSize=sizeof(w);w.section="Enhancements";w.sidebar="Elemental Mirror Shield";
        w.type=SOH_MOD_MENU_CHECKBOX;w.label="Reflect counter-element magic";w.cvar=ENABLED;w.defaultInt=1;
        w.tooltip="The vanilla Mirror Shield sends blocked magic back at its source. Uses real enemy collision/damage reactions. Custom shields keep their own behavior. Never runs during Twinrova.";sApi->RegisterMenuWidget(&w);
        w.type=SOH_MOD_MENU_CUSTOM;w.label="Counter-element list";w.cvar=NULL;w.onChange=DrawList;sApi->RegisterMenuWidget(&w);
    }
    SOH_REGISTER_HOOK(sApi,OnPlayerShieldBlocked,OnGuard);SOH_REGISTER_HOOK(sApi,OnCollisionResolveDamage,OnDamage);
    SOH_REGISTER_HOOK(sApi,OnActorDestroy,ForgetActor);SOH_REGISTER_HOOK(sApi,OnSceneInit,Reset);
}
