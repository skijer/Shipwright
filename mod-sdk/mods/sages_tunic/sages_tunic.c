/**
 * Sage's Tunic: one resistance for every medallion Link carries, and the cloth takes that medallion's colour
 * whenever the resistance absorbs something. Water: ice. Fire: heat and flame. Light: electricity.
 * Shadow: grabs and paralysis. Spirit: falls. Forest: wind.
 */

#include <math.h>

#include "z64items.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/object_gi_clothes/object_gi_clothes.h"
#include "objects/object_gi_medal/object_gi_medal.h"
#include "soh/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"

#define SAGES_TUNIC_KEY "nei.equip.sages_tunic"
#define FLASH_HOLD_FRAMES 20
#define FLASH_FADE_FRAMES 30
#define HIT_EFFECT_FIRE 1
#define HIT_EFFECT_ICE 2
#define HIT_EFFECT_ELECTRIC 3
#define BGCHECK_ON_GROUND 0x0001
#define FIREPROOF_DEKU_SHIELD_CVAR "gCheats.FireproofDekuShield"

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__textures/icon_item_custom/gItemIconSagesTunicTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gSagesTunicNameTex";

s32 func_80838144(s32 floorType);

typedef enum {
    SAGES_RESIST_ICE,
    SAGES_RESIST_FIRE,
    SAGES_RESIST_THUNDER,
    SAGES_RESIST_STUN,
    SAGES_RESIST_FALL,
    SAGES_RESIST_WIND,
    SAGES_RESIST_MAX,
} SagesResistance;

static const s32 sResistanceMedallions[SAGES_RESIST_MAX] = {
    QUEST_MEDALLION_WATER,  QUEST_MEDALLION_FIRE,   QUEST_MEDALLION_LIGHT,
    QUEST_MEDALLION_SHADOW, QUEST_MEDALLION_SPIRIT, QUEST_MEDALLION_FOREST,
};

static const Color_RGB8 sResistanceColors[SAGES_RESIST_MAX] = {
    { 60, 130, 235 }, { 235, 60, 30 }, { 245, 225, 80 }, { 155, 70, 220 }, { 240, 140, 40 }, { 70, 195, 90 },
};

static const Color_RGB8 sClothColor = { 235, 240, 245 };

static const SOHModApi* sApi;

static struct {
    s16 flashTimer;
    SagesResistance flashResistance;
    s32 (*vanillaGrabPlayer)(PlayState* play, Player* player);
    bool isFireproofCheatBorrowed;
    s32 fireproofCheatBefore;
} sSages;

static bool IsWorn(void) {
    return CustomEquipRegistry_IsWorn(SAGES_TUNIC_KEY);
}

static bool HasResistance(SagesResistance resistance) {
    return IsWorn() && CHECK_QUEST_ITEM(sResistanceMedallions[resistance]);
}

static void Flash(SagesResistance resistance) {
    sSages.flashResistance = resistance;
    sSages.flashTimer = FLASH_HOLD_FRAMES + FLASH_FADE_FRAMES;
}

static bool Resists(SagesResistance resistance) {
    if (!HasResistance(resistance)) {
        return false;
    }
    Flash(resistance);
    return true;
}

static bool IsLocalPlayer(Actor* actor) {
    return gPlayState != NULL && actor == &GET_PLAYER(gPlayState)->actor;
}

// Heat rooms count down only while the hazard reads as heat; every frame it is resisted the flash is refreshed.
static void ResistHeat(PlayState* play, int16_t* hazard) {
    if (*hazard == PLAYER_ENV_HAZARD_HOTROOM && Resists(SAGES_RESIST_FIRE)) {
        *hazard = PLAYER_ENV_HAZARD_NONE;
    }
}

// Skipping the assignment keeps the previous floor type, so a burning floor is walked like any other.
static void ResistBurningFloors(bool* should, va_list args) {
    Player* player = va_arg(args, Player*);
    PlayState* play = gPlayState;
    s32 floorType;

    if (play == NULL || !HasResistance(SAGES_RESIST_FIRE) || !(player->actor.bgCheckFlags & BGCHECK_ON_GROUND) ||
        player->actor.floorPoly == NULL) {
        return;
    }
    floorType = func_80041D4C(&play->colCtx, player->actor.floorPoly, player->actor.floorBgId);
    if (func_80838144(floorType) >= 0) {
        Flash(SAGES_RESIST_FIRE);
        *should = false;
    }
}

// The hit effect is decided in the AT pass and read by the player next update; clearing it here keeps the damage
// and drops the freeze, the shock or the flames.
static void ResistElementalHit(Actor* victim, ColliderInfo* attack, float* damage) {
    u8 effect;

    if (!IsLocalPlayer(victim)) {
        return;
    }
    effect = victim->colChkInfo.acHitEffect;
    if ((effect == HIT_EFFECT_ICE && Resists(SAGES_RESIST_ICE)) ||
        (effect == HIT_EFFECT_ELECTRIC && Resists(SAGES_RESIST_THUNDER)) ||
        (effect == HIT_EFFECT_FIRE && Resists(SAGES_RESIST_FIRE))) {
        victim->colChkInfo.acHitEffect = 0;
    }
}

// An enemy shoving Link with a shock (a Beamos, a Biri) leaves the same knockback without the jolt.
static void ResistShockingKnockback(Player* player, Input* input) {
    if (player->knockbackType == PLAYER_KNOCKBACK_LARGE_SHOCK && Resists(SAGES_RESIST_THUNDER)) {
        player->knockbackType = PLAYER_KNOCKBACK_LARGE;
    }
}

// A Deku Shield only burns on the frame of the block, after this hook: the game's own fireproof switch is lent
// for that one update and handed back at its end.
static void KeepShieldFromBurning(PlayState* play, Player* player, Actor* attacker) {
    ColliderInfo* toucher = player->shieldQuad.info.acHitInfo;

    if (player->currentShield != PLAYER_SHIELD_DEKU || toucher == NULL || toucher->toucher.effect != HIT_EFFECT_FIRE ||
        sSages.isFireproofCheatBorrowed || !Resists(SAGES_RESIST_FIRE)) {
        return;
    }
    sSages.fireproofCheatBefore = CVarGetInteger(FIREPROOF_DEKU_SHIELD_CVAR, 0);
    CVarSetInteger(FIREPROOF_DEKU_SHIELD_CVAR, 1);
    sSages.isFireproofCheatBorrowed = true;
}

static void ReturnFireproofCheat(void) {
    if (!sSages.isFireproofCheatBorrowed) {
        return;
    }
    CVarSetInteger(FIREPROOF_DEKU_SHIELD_CVAR, sSages.fireproofCheatBefore);
    sSages.isFireproofCheatBorrowed = false;
}

static void ResistFallDamage(bool* should, va_list args) {
    if (Resists(SAGES_RESIST_FALL)) {
        *should = false;
    }
}

static void ResistParalysis(bool* should, va_list args) {
    if (Resists(SAGES_RESIST_STUN)) {
        *should = false;
    }
}

// Wallmasters, Floormasters and Dead Hand's hands all seize Link through this one pointer.
static s32 GrabUnlessResisted(PlayState* play, Player* player) {
    if (Resists(SAGES_RESIST_STUN)) {
        return false;
    }
    return sSages.vanillaGrabPlayer(play, player);
}

static void WrapGrabPlayer(PlayState* play) {
    if (play->grabPlayer == GrabUnlessResisted) {
        return;
    }
    sSages.vanillaGrabPlayer = play->grabPlayer;
    play->grabPlayer = GrabUnlessResisted;
}

// The blades push Link through pushedSpeed, aimed along their own yaw, from their own update; this runs straight
// after it.
static void ResistFanWind(void* actorPtr) {
    PlayState* play = gPlayState;
    Actor* fan = (Actor*)actorPtr;
    Player* player;

    if (play == NULL) {
        return;
    }
    player = GET_PLAYER(play);
    if (player->pushedSpeed == 0.0f || player->pushedYaw != fan->shape.rot.y || !Resists(SAGES_RESIST_WIND)) {
        return;
    }
    player->pushedSpeed = 0.0f;
}

static void TickTunic(void) {
    PlayState* play = gPlayState;

    ReturnFireproofCheat();
    if (play == NULL) {
        return;
    }
    WrapGrabPlayer(play);
    if (sSages.flashTimer > 0) {
        sSages.flashTimer--;
    }
}

static void DyeTunic(bool* should, va_list args) {
    Color_RGB8* color;
    Color_RGB8 medallion;
    s32 weight;

    va_arg(args, void*);
    color = va_arg(args, Color_RGB8*);
    if (!IsWorn()) {
        return;
    }
    *color = sClothColor;
    if (sSages.flashTimer <= 0) {
        return;
    }
    medallion = sResistanceColors[sSages.flashResistance];
    weight = MIN(sSages.flashTimer, FLASH_FADE_FRAMES);
    color->r = (u8)(sClothColor.r + ((s32)medallion.r - sClothColor.r) * weight / FLASH_FADE_FRAMES);
    color->g = (u8)(sClothColor.g + ((s32)medallion.g - sClothColor.g) * weight / FLASH_FADE_FRAMES);
    color->b = (u8)(sClothColor.b + ((s32)medallion.b - sClothColor.b) * weight / FLASH_FADE_FRAMES);
}

// The six medallions launch out of the tunic in staggered ballistic arcs, popping in and shrinking out so the
// loop never snaps.
static void DrawMedallionFountain(PlayState* play) {
    static const char* const sMedallionFaces[SAGES_RESIST_MAX] = {
        gGiForestMedallionFaceDL, gGiFireMedallionFaceDL,   gGiWaterMedallionFaceDL,
        gGiSpiritMedallionFaceDL, gGiShadowMedallionFaceDL, gGiLightMedallionFaceDL,
    };
    const f32 launchSpeed = 1.5f;
    const f32 lift = 1.5f;
    const f32 gravity = 0.07f;
    const s32 cycle = 40;
    const s32 stagger = 7;
    const f32 medallionScale = 0.35f;
    const f32 depth = 14.0f;

    OPEN_DISPS(play->state.gfxCtx);
    for (s32 i = 0; i < SAGES_RESIST_MAX; i++) {
        f32 t = (f32)((play->gameplayFrames + i * stagger) % cycle);
        f32 theta = (M_PI / 2.0f) + i * (f32)(M_PI / 3.0f);
        f32 scale = medallionScale;

        if (t < 4.0f) {
            scale *= t / 4.0f;
        } else if (t >= cycle - 9.0f) {
            scale *= (cycle - 1.0f - t) / 8.0f;
        }
        if (scale <= 0.001f) {
            continue;
        }
        // The medallion display lists draw nothing under setup 25.
        Gfx_SetupDL_26Opa(play->state.gfxCtx);
        Matrix_Push();
        Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
        Matrix_Translate(cosf(theta) * launchSpeed * t, (sinf(theta) * launchSpeed + lift) * t - 0.5f * gravity * t * t,
                         depth, MTXMODE_APPLY);
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
        Matrix_RotateY(play->gameplayFrames * 0.09f + i, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sMedallionFaces[i]);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiMedallionDL);
        Matrix_Pop();
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawGetItem(PlayState* play, GetItemEntry* entry) {
    DrawMedallionFountain(play);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPGrayscale(POLY_OPA_DISP++, true);
    gDPSetGrayscaleColor(POLY_OPA_DISP++, 240, 244, 250, 255);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiTunicCollarDL);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiTunicDL);
    gSPGrayscale(POLY_OPA_DISP++, false);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void ClearFlash(const char* key) {
    sSages.flashTimer = 0;
}

static const char* const sRequiredHooks[] = {
    "OnPlayerUpdate",          "OnPlayerFilterInput", "OnPlayerShieldBlocked", "OnCollisionResolveDamage",
    "OnResolveEnvHazard",      "OnActorUpdate",
};

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    SOHCustomEquipDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = SAGES_TUNIC_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rSage's Tunic&%wEach medallion you carry shields you from one danger: ice, fire, "
                           "lightning, grabs, falls and wind.";
    definition.getItemText = "You got the %rSage's Tunic%w!&Woven for the Sages of the Temples. Every medallion you "
                             "hold becomes a resistance you wear, and the cloth shines with its colour when it saves "
                             "you.";
    definition.slot = SOH_EQUIP_SLOT_TUNIC;
    definition.page = 0;
    definition.row = SOH_EQUIP_SLOT_TUNIC;
    definition.column = 3;
    definition.ageRequirement = SOH_CUSTOM_ITEM_AGE_ANY;
    definition.vanillaBase = EQUIP_VALUE_TUNIC_KOKIRI;
    definition.toggles = 1;
    definition.onUnequip = ClearFlash;
    definition.getItemEntry.drawFunc = DrawGetItem;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }

    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, TickTunic);
    SOH_REGISTER_HOOK(sApi, OnPlayerFilterInput, ResistShockingKnockback);
    SOH_REGISTER_HOOK(sApi, OnPlayerShieldBlocked, KeepShieldFromBurning);
    SOH_REGISTER_HOOK(sApi, OnCollisionResolveDamage, ResistElementalHit);
    SOH_REGISTER_HOOK(sApi, OnResolveEnvHazard, ResistHeat);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_BG_HAKA_TRAP, ResistFanWind);
    sApi->RegisterVB(VB_APPLY_TUNIC_COLOR, DyeTunic);
    sApi->RegisterVB(VB_SET_STATIC_FLOOR_TYPE, ResistBurningFloors);
    sApi->RegisterVB(VB_RECIEVE_FALL_DAMAGE, ResistFallDamage);
    sApi->RegisterVB(VB_LIKE_LIKE_GRAB_PLAYER, ResistParalysis);
    sApi->RegisterVB(VB_REDEAD_GIBDO_FREEZE_LINK, ResistParalysis);
}
