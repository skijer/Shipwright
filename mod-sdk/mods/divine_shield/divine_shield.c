/**
 * Divine Shield: a wooden shield fire cannot burn. A block inside the first frames of raising it freezes every
 * enemy in the room, the way a Deku Nut would.
 */

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ResourceManagerHelpers.h"
#include "objects/object_link_boy/object_link_boy.h"
#include "objects/object_link_child/object_link_child.h"
#include "soh/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"

#define DIVINE_SHIELD_KEY "nei.equip.divine_shield"
#define PARRY_WINDOW 10
#define FREEZE_FRAMES 40
#define SPARKLES_PER_ENEMY 6
// The model is authored in the shield limb's own space, about 6000 units across.
#define SHIELD_SCALE 44.2f
#define SHIELD_ROT_X (-95.0f * (M_PI / 180.0f))
#define SHIELD_ROT_Y (-27.0f * (M_PI / 180.0f))
#define SHIELD_ROT_Z (-99.0f * (M_PI / 180.0f))
#define SHIELD_OFF_X (-508.0f)
#define SHIELD_OFF_Y (-372.0f)
#define SHIELD_OFF_Z (-5.0f)
#define GET_ITEM_SCALE 0.9f

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__textures/icon_item_custom/gItemIconGoddessShieldTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gGoddessShieldNameTex";
static const ALIGN_ASSET(2) char sShieldDL[] = "__OTR__objects/object_nei_divine_shield/g_divine_shield_dl";

static const char* const sOpenRightHandDL[] = { gLinkAdultRightHandNearDL, gLinkChildRightHandNearDL };
static const char* const sBareSheathDL[] = { gLinkAdultSheathNearDL, gLinkChildSheathNearDL };
static const char* const sSheathedSwordDL[] = { gLinkAdultMasterSwordAndSheathNearDL, gLinkChildSwordAndSheathNearDL };

static const SOHModApi* sApi;

static struct {
    s16 raiseTimer;
    bool wasShielding;
} sDivine;

static bool IsWorn(void) {
    return CustomEquipRegistry_IsWorn(DIVINE_SHIELD_KEY);
}

static bool IsShieldOnBack(Player* player) {
    return player->sheathType == PLAYER_MODELTYPE_SHEATH_18 || player->sheathType == PLAYER_MODELTYPE_SHEATH_19;
}

static bool IsShieldInHand(Player* player) {
    return player->rightHandType == PLAYER_MODELTYPE_RH_SHIELD;
}

static void TrackRaise(void) {
    PlayState* play = gPlayState;
    bool isShielding;

    if (play == NULL || !IsWorn()) {
        return;
    }
    isShielding = (GET_PLAYER(play)->stateFlags1 & PLAYER_STATE1_SHIELDING) != 0;
    if (isShielding && !sDivine.wasShielding) {
        sDivine.raiseTimer = 0;
    }
    sDivine.wasShielding = isShielding;
    if (isShielding) {
        sDivine.raiseTimer++;
    }
}

static void FreezeRoom(Player* player, PlayState* play) {
    Color_RGBA8 prim = { 200, 220, 255, 255 };
    Color_RGBA8 env = { 100, 150, 255, 0 };
    Vec3f accel = { 0.0f, 0.0f, 0.0f };

    for (Actor* enemy = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; enemy != NULL; enemy = enemy->next) {
        enemy->freezeTimer = FREEZE_FRAMES;
        Actor_SetColorFilter(enemy, 0x0000, 0xF8, 0x0000, FREEZE_FRAMES);
        for (s32 i = 0; i < SPARKLES_PER_ENEMY; i++) {
            Vec3f pos = { enemy->world.pos.x + Rand_CenteredFloat(60.0f),
                          enemy->world.pos.y + 20.0f + Rand_ZeroFloat(40.0f),
                          enemy->world.pos.z + Rand_CenteredFloat(60.0f) };
            Vec3f vel = { Rand_CenteredFloat(3.0f), Rand_ZeroFloat(2.0f) + 1.0f, Rand_CenteredFloat(3.0f) };

            EffectSsKiraKira_SpawnSmall(play, &pos, &vel, &accel, &prim, &env);
        }
    }
    Audio_PlaySoundGeneral(NA_SE_IT_SHIELD_REFLECT_SW, &player->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static void ParryOnBlock(PlayState* play, Player* player, Actor* attacker) {
    if (IsWorn() && attacker != NULL && attacker != &player->actor && sDivine.raiseTimer <= PARRY_WINDOW) {
        FreezeRoom(player, play);
    }
}

// The vanilla shield leaves the hand and the back; the custom one is drawn in its place after the limb.
static void HideVanillaShield(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    s32 age = gSaveContext.linkAge;

    if (!IsWorn()) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_R_HAND && IsShieldInHand(player)) {
        *dList = ResourceMgr_LoadGfxByName(sOpenRightHandDL[age]);
    } else if (limbIndex == PLAYER_LIMB_SHEATH && IsShieldOnBack(player)) {
        // Sheath 18 still carries the sword; only 19 is the empty sheath.
        *dList = ResourceMgr_LoadGfxByName(player->sheathType == PLAYER_MODELTYPE_SHEATH_18 ? sSheathedSwordDL[age]
                                                                                            : sBareSheathDL[age]);
    }
}

// On XLU on purpose: the model leaves its combiner on the pipe, and on OPA that blacks out every limb after it.
static void DrawShield(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(SHIELD_OFF_X, SHIELD_OFF_Y, SHIELD_OFF_Z, MTXMODE_APPLY);
    Matrix_RotateX(SHIELD_ROT_X, MTXMODE_APPLY);
    Matrix_RotateY(SHIELD_ROT_Y, MTXMODE_APPLY);
    Matrix_RotateZ(SHIELD_ROT_Z, MTXMODE_APPLY);
    Matrix_Scale(SHIELD_SCALE, SHIELD_SCALE, SHIELD_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sShieldDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

// The vanilla base is the Hylian Shield, which collides as metal; this one is wood.
static void DrawOnLimb(PlayState* play, Player* player, int32_t limbIndex) {
    if (!IsWorn() || player != GET_PLAYER(play)) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_R_HAND && IsShieldInHand(player)) {
        player->shieldQuad.base.colType = COLTYPE_WOOD;
        DrawShield(play);
    } else if (limbIndex == PLAYER_LIMB_SHEATH && IsShieldOnBack(player) && !IsShieldInHand(player) &&
               player->rightHandType != PLAYER_MODELTYPE_RH_FF) {
        DrawShield(play);
    }
}

static void DrawGetItem(PlayState* play, GetItemEntry* entry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(GET_ITEM_SCALE, GET_ITEM_SCALE, GET_ITEM_SCALE, MTXMODE_APPLY);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sShieldDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void ForgetRaise(const char* key) {
    sDivine.raiseTimer = 0;
    sDivine.wasShielding = false;
}

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnPlayerShieldBlocked", "OnPlayerResolveLimbDraw",
                                              "OnPlayerPostLimbDraw" };

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
    definition.key = DIVINE_SHIELD_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rDivine Shield&%wBlock with %y\xA3%w right as you raise it and every enemy nearby freezes. "
                           "Fire cannot burn it.";
    definition.getItemText = "You got the %rDivine Shield%w!&Wood blessed by the goddess: flames will not take it, and "
                             "a guard raised just in time freezes whatever dares to strike it.";
    definition.slot = SOH_EQUIP_SLOT_SHIELD;
    definition.page = 0;
    definition.row = SOH_EQUIP_SLOT_SHIELD;
    definition.column = 1;
    definition.ageRequirement = SOH_CUSTOM_ITEM_AGE_ANY;
    definition.vanillaBase = EQUIP_VALUE_SHIELD_HYLIAN;
    definition.toggles = 1;
    definition.onUnequip = ForgetRaise;
    definition.getItemEntry.drawFunc = DrawGetItem;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }

    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, TrackRaise);
    SOH_REGISTER_HOOK(sApi, OnPlayerShieldBlocked, ParryOnBlock);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, HideVanillaShield);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawOnLimb);
}
