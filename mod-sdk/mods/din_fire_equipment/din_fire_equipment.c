// Din's Fire on the shield and sword. Port of marsh6487's din_fire_shield / din_fire_sword POCs (d54391b5,
// db3f11fc). The originals draw HD flame meshes from an external pack; this one asks the engine's own fire
// particles for the same look, so there is nothing to install. Everything runs from OnPlayerUpdate, with the
// limb positions the engine already keeps for Link (bodyPartsPos, meleeWeaponInfo).

#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include <libultraship/bridge/consolevariablebridge.h>

#define DIN_SHIELD_CVAR "gMods.DinFire.Shield"
#define DIN_SWORD_CVAR "gMods.DinFire.Sword"
#define DIN_SFX_CVAR "gMods.DinFire.ChargeSfx"
#define SHIELD_PERIOD 2
#define SWORD_PERIOD 2
#define BLADE_FLAMES 4

static const char* const sRequiredHooks[] = { "OnPlayerUpdate" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;

static bool IsGuarding(Player* player) {
    return (player->stateFlags1 & PLAYER_STATE1_SHIELDING) && player->rightHandType == PLAYER_MODELTYPE_RH_SHIELD;
}

static bool IsBladeOut(Player* player) {
    return player->leftHandType == PLAYER_MODELTYPE_LH_SWORD || player->meleeWeaponState != 0;
}

static bool IsPlayerVisible(Player* player) {
    return player->actor.scale.y > 0.0f && !(player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW) &&
           !(player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_WATER));
}

static void WreatheShield(PlayState* play, Player* player) {
    // The shield is held in the right hand; flames sit on the hand so they follow every guard pose.
    if (play->gameplayFrames % SHIELD_PERIOD == 0) {
        EffectSsFireTail_SpawnFlameOnPlayer(play, 0.012f, PLAYER_BODYPART_R_HAND, 0.0f);
    }
    if (CVarGetInteger(DIN_SFX_CVAR, 0)) {
        Audio_PlayActorSound2(&player->actor, NA_SE_PL_ARROW_CHARGE_FIRE - SFX_FLAG);
    }
}

static void WreatheBlade(PlayState* play, Player* player) {
    Vec3f accel = { 0.0f, 0.3f, 0.0f };
    Color_RGBA8 prim = { 255, 225, 122, 255 };
    Color_RGBA8 env = { 255, 43, 3, 0 };
    WeaponInfo* info = &player->meleeWeaponInfo[0];

    if (play->gameplayFrames % SWORD_PERIOD != 0) {
        return;
    }
    for (s32 i = 0; i < BLADE_FLAMES; i++) {
        f32 t = (f32)i / (BLADE_FLAMES - 1);
        Vec3f pos = { info->base.x + (info->tip.x - info->base.x) * t, info->base.y + (info->tip.y - info->base.y) * t,
                      info->base.z + (info->tip.z - info->base.z) * t };
        Vec3f vel = { Rand_CenteredFloat(0.8f), 0.6f + Rand_ZeroFloat(0.6f), Rand_CenteredFloat(0.8f) };

        if (player->meleeWeaponState == 0) {
            // Blade at rest: the weapon quad is not live, so anchor on the left hand and let it rise.
            pos = player->bodyPartsPos[PLAYER_BODYPART_L_HAND];
            pos.y += 10.0f + 6.0f * i;
        }
        EffectSsKiraKira_SpawnSmall(play, &pos, &vel, &accel, &prim, &env);
    }
}

static void Burn(void) {
    PlayState* play = gPlayState;
    Player* player;

    if (play == NULL || play->pauseCtx.state != 0 || play->transitionTrigger != TRANS_TRIGGER_OFF) {
        return;
    }
    player = GET_PLAYER(play);
    if (!IsPlayerVisible(player)) {
        return;
    }
    if (CVarGetInteger(DIN_SHIELD_CVAR, 1) && IsGuarding(player)) {
        WreatheShield(play, player);
    }
    if (CVarGetInteger(DIN_SWORD_CVAR, 1) && IsBladeOut(player) && player->currentSwordItemId != ITEM_NONE) {
        WreatheBlade(play, player);
    }
}

static void RegisterToggle(const char* label, const char* cvar, const char* tooltip, int32_t defaultValue) {
    SOHModMenuWidget widget = { sizeof(SOHModMenuWidget) };

    widget.section = "Enhancements";
    widget.sidebar = "Items";
    widget.type = SOH_MOD_MENU_CHECKBOX;
    widget.label = label;
    widget.cvar = cvar;
    widget.tooltip = tooltip;
    widget.defaultInt = defaultValue;
    sApi->RegisterMenuWidget(&widget);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, Burn);
    if (SOH_MOD_API_HAS(sApi, RegisterMenuWidget)) {
        RegisterToggle("Din's Fire Shield", DIN_SHIELD_CVAR, "Flames wreathe the shield while you guard.", 1);
        RegisterToggle("Din's Fire Sword", DIN_SWORD_CVAR, "Flames run along the drawn blade.", 1);
        RegisterToggle("Din's Fire Charge Sound", DIN_SFX_CVAR, "Play the Fire Arrow charge sound while guarding.", 0);
    }
}
