#include <stdbool.h>
#include <string.h>
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "soh/ModApi/ModApi.h"
#include "soh/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"
#include "objects/object_gi_bracelet/object_gi_bracelet.h"
#include "textures/icon_item_static/icon_item_static.h"
#include "libultraship/bridge/consolevariablebridge.h"

#define RAVIO_KEY "nei.equip.ravio_bracelet"
#define REGEN_DELAY 40
static const SOHModApi* sApi;
static s16 sLastMagic, sDelay;
static u32 sRefillProgress;
typedef struct {
    const char* changedKey;
    const char* colorKey;
    bool hadChanged, hadColor;
    s32 changed;
    Color_RGBA8 color;
} BarColor;
static BarColor sBarColors[]={
    {"gCosmetics.Consumable.Magic.Changed","gCosmetics.Consumable.Magic.Value"},
    {"gCosmetics.Consumable.MagicActive.Changed","gCosmetics.Consumable.MagicActive.Value"},
    {"gCosmetics.Consumable_MagicInfinite.Changed","gCosmetics.Consumable_MagicInfinite.Value"}
};
static bool sTinted;
static const char* sHooks[]={"OnGameFrameUpdate","OnMagicResolveCost","OnPlayDrawBegin","OnGameStateMainStart",
                           "OnInterfaceDrawEnd","OnLoadGame","OnExitGame","OnCustomEquipOwnedChange"};
static const SOHModRequirements sRequirements={sizeof(SOHModRequirements),sHooks,ARRAY_COUNT(sHooks)};

static bool Active(void) {
    return CustomEquipRegistry_IsOwned(RAVIO_KEY) && CustomEquipRegistry_IsToggleOn(RAVIO_KEY);
}
static bool HasColor(const char* key) {
    Color_RGBA8 a=CVarGetColor(key,(Color_RGBA8){0,0,0,0});
    Color_RGBA8 b=CVarGetColor(key,(Color_RGBA8){255,255,255,255});
    return !memcmp(&a,&b,sizeof(a));
}
static void RestoreTint(void) {
    if (!sTinted) return;
    for (s32 i=0;i<ARRAY_COUNT(sBarColors);++i) {
        BarColor* saved=&sBarColors[i];
        if (saved->hadChanged) CVarSetInteger(saved->changedKey,saved->changed);
        else CVarClear(saved->changedKey);
        if (saved->hadColor) CVarSetColor(saved->colorKey,saved->color);
        else CVarClear(saved->colorKey);
    }
    sTinted=false;
}
static void TintBar(void) {
    RestoreTint();
    if (!gPlayState || !Active()) return;
    // Let the real HUD draw its own position, size, fill and alpha. Only its palette changes.
    for (s32 i=0;i<ARRAY_COUNT(sBarColors);++i) {
        BarColor* saved=&sBarColors[i];
        saved->hadChanged=CVarGetInteger(saved->changedKey,0)==CVarGetInteger(saved->changedKey,1);
        saved->hadColor=HasColor(saved->colorKey);
        saved->changed=CVarGetInteger(saved->changedKey,0);
        saved->color=CVarGetColor(saved->colorKey,(Color_RGBA8){0,0,0,255});
        CVarSetInteger(saved->changedKey,1);
        CVarSetColor24(saved->colorKey,i==1?(Color_RGB8){210,150,255}:(Color_RGB8){160,80,220});
    }
    sTinted=true;
}
static void InterfaceDrawn(PlayState* play) { RestoreTint(); }
static void Load(s32 file) { RestoreTint();sLastMagic=gSaveContext.magic;sDelay=REGEN_DELAY;sRefillProgress=0; }
static void Exit(s32 file) { RestoreTint(); }
static void Acquired(const char* key, bool owned) {
    if (owned && !strcmp(key,RAVIO_KEY)) CustomEquipRegistry_SetToggle(RAVIO_KEY,true);
}
static void Cost(s16* amount) { if (Active() && *amount>0) {sDelay=REGEN_DELAY;sRefillProgress=0;} }
static void Tick(void) {
    PlayState* play=gPlayState;
    if (!play) return;
    if (!Active() || !gSaveContext.isMagicAcquired || gSaveContext.magicCapacity<=0) {
        sLastMagic=gSaveContext.magic;sRefillProgress=0;return;
    }
    Player* player=GET_PLAYER(play);
    if (!player || play->pauseCtx.state || play->msgCtx.msgMode!=MSGMODE_NONE || play->csCtx.state!=CS_STATE_IDLE ||
        play->transitionTrigger!=TRANS_TRIGGER_OFF || (player->stateFlags1&(PLAYER_STATE1_DEAD|PLAYER_STATE1_IN_CUTSCENE))) {
        sLastMagic=gSaveContext.magic;return;
    }
    if (gSaveContext.magic<sLastMagic) {sDelay=REGEN_DELAY;sRefillProgress=0;}
    if (sDelay>0) --sDelay;
    else if (!play->actorCtx.lensActive && gSaveContext.magicState==MAGIC_STATE_IDLE &&
             gSaveContext.magic<gSaveContext.magicCapacity) {
        // Previous rate was 1 MP / 2 updates. Scale it by (1 + fill) / 6:
        // 1/6 of that rate when empty, smoothly rising to 1/3 as the bar fills.
        u32 capacity=gSaveContext.magicCapacity;
        sRefillProgress+=capacity+gSaveContext.magic;
        if (sRefillProgress>=12*capacity) {
            sRefillProgress-=12*capacity;
            ++gSaveContext.magic;
            gSaveContext.magicTarget=gSaveContext.magic;
        }
    }
    if (gSaveContext.magic>=gSaveContext.magicCapacity) sRefillProgress=0;
    sLastMagic=gSaveContext.magic;
}
static void DrawGet(PlayState* play,GetItemEntry* entry) {
    OPEN_DISPS(play->state.gfxCtx);Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++,(Gfx*)gGiGoronBraceletDL);CLOSE_DISPS(play->state.gfxCtx);
}
SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api){sApi=api;}
SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void){return &sRequirements;}
SOH_MOD_EXPORT void ModInit(void) {
    SOHCustomEquipDefinition equip={0};equip.structSize=sizeof(equip);equip.key=RAVIO_KEY;
    equip.iconPath=gItemIconGoronsBraceletTex; // Temporary native placeholder until the requested icon exists.
    static const ALIGN_ASSET(2) char name[]="__OTR__textures/item_name_custom/gRavioBraceletNameTex";
    equip.namePath=name;
    equip.getItemText="You got %rRavio's Bracelet%w!&Your magic bar turns purple&and recovers after use.^"
        "Spells keep their normal cost.&Stop using magic to recover.&Press %b\x9F%w in pause to toggle it.";
    equip.pauseText="%rRavio's Bracelet&%wPurple magic recovers after use.&%b\x9F%w toggles its effect.";
    equip.slot=SOH_EQUIP_SLOT_UPGRADE;equip.page=0;equip.row=3;equip.column=0;
    equip.toggles=1;equip.ageRequirement=SOH_CUSTOM_ITEM_AGE_ANY;equip.getItemEntry.drawFunc=DrawGet;
    // The equipment registry creates the pickup; this block also puts it into new seeds.
    static const SOHCustomItemRandomizer randomizer = {
        sizeof(SOHCustomItemRandomizer), 0, SOH_CUSTOM_ITEM_TYPE_EQUIP, 0, 1,
        RAVIO_KEY, RAVIO_KEY, "",
    };
    equip.randomizer = &randomizer;
    if(!CustomEquipRegistry_Register(&equip))return;
    SOH_REGISTER_HOOK(sApi,OnGameFrameUpdate,Tick);SOH_REGISTER_HOOK(sApi,OnMagicResolveCost,Cost);
    // PlayDrawEnd precedes the HUD and is skipped in pause. Restore only after the actual interface draw.
    SOH_REGISTER_HOOK(sApi,OnPlayDrawBegin,TintBar);SOH_REGISTER_HOOK(sApi,OnGameStateMainStart,RestoreTint);
    SOH_REGISTER_HOOK(sApi,OnInterfaceDrawEnd,InterfaceDrawn);
    SOH_REGISTER_HOOK(sApi,OnLoadGame,Load);SOH_REGISTER_HOOK(sApi,OnCustomEquipOwnedChange,Acquired);
    SOH_REGISTER_HOOK(sApi,OnExitGame,Exit);
}
