/**
 * Shield of Ikana: Majora's Mask's Mirror Shield. A guard landed right after raising it drains the attacker's soul
 * into Link, and once per area it refuses to let him die.
 */

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ResourceManagerHelpers.h"
#include "objects/object_link_child/object_link_child.h"
#include "soh/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"

#define IKANA_SHIELD_KEY "nei.equip.ikana_shield"
#define GUARD_WINDOW 12
#define SOUL_DRAIN 4
#define SOUL_HEAL 8
#define REVIVE_HEALTH (3 * 16)
#define REVIVE_FLASH_FRAMES 20
#define GET_ITEM_SCALE 0.035f

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__icon_item_static_yar/gItemIconMirrorShieldTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gShieldOfIkanaNameTex";
// Majora's Mask's own child mirror shield, from the mm_assets package. Without it the vanilla Mirror stays.
static const ALIGN_ASSET(2) char sMmShieldInHandDL[] =
    "__OTR__objects/object_link_child/gLinkHumanRightHandHoldingMirrorShieldDL";
static const ALIGN_ASSET(2) char sMmShieldOnBackDL[] = "__OTR__objects/object_link_child/gLinkHumanMirrorShieldDL";

static const char* const sBareSheathDL[] = { "__OTR__objects/object_link_boy/gLinkAdultSheathNearDL",
                                             gLinkChildSheathNearDL };
static const char* const sSheathedSwordDL[] = { "__OTR__objects/object_link_boy/gLinkAdultMasterSwordAndSheathNearDL",
                                                gLinkChildSwordAndSheathNearDL };

static const SOHModApi* sApi;

static struct {
    s16 guardTimer;
    bool isGuarding;
    bool isDeathSaveUsed;
    s16 flashTimer;
} sIkana;

static bool IsWorn(void) {
    return CustomEquipRegistry_IsWorn(IKANA_SHIELD_KEY);
}

// Asked on every limb of every frame; the archives mounted do not change within a session.
static bool HasMmModel(void) {
    static s8 sHasModel = -1;

    if (sHasModel < 0) {
        sHasModel = SOH_MOD_API_HAS(sApi, HasResource) && sApi->HasResource(sMmShieldInHandDL) &&
                    sApi->HasResource(sMmShieldOnBackDL);
    }
    return sHasModel;
}

static bool IsShieldOnBack(Player* player) {
    return player->sheathType == PLAYER_MODELTYPE_SHEATH_18 || player->sheathType == PLAYER_MODELTYPE_SHEATH_19;
}

static bool IsShieldInHand(Player* player) {
    return player->rightHandType == PLAYER_MODELTYPE_RH_SHIELD;
}

static void PlaySfx(Player* player, u16 sfxId) {
    Audio_PlaySoundGeneral(sfxId, &player->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static void TrackGuard(Player* player) {
    bool isGuarding = (player->stateFlags1 & PLAYER_STATE1_SHIELDING) != 0;

    if (isGuarding && !sIkana.isGuarding) {
        sIkana.guardTimer = 0;
    } else if (isGuarding) {
        sIkana.guardTimer++;
    } else {
        sIkana.guardTimer = 0;
    }
    sIkana.isGuarding = isGuarding;
}

static void FadeReviveFlash(PlayState* play) {
    if (sIkana.flashTimer <= 0 || --sIkana.flashTimer > 0) {
        return;
    }
    play->envCtx.fillScreen = false;
    play->envCtx.screenFillColor[3] = 0;
}

static void TickShield(void) {
    PlayState* play = gPlayState;
    Player* player;

    if (play == NULL) {
        return;
    }
    FadeReviveFlash(play);
    if (!IsWorn()) {
        return;
    }
    player = GET_PLAYER(play);
    if (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                               PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM)) {
        return;
    }
    TrackGuard(player);
}

// Zeroing health is not enough: most enemies only die from their own AC_HIT handling, which the drain bypasses,
// so the kill is driven by hand. Bosses keep their scripted death and need a real last hit.
static void DrainSoul(PlayState* play, Player* player, Actor* attacker) {
    if (attacker->colChkInfo.health > SOUL_DRAIN) {
        attacker->colChkInfo.health -= SOUL_DRAIN;
    } else {
        attacker->colChkInfo.health = 0;
        if (attacker->category != ACTORCAT_BOSS) {
            Enemy_StartFinishingBlow(play, attacker);
            Item_DropCollectibleRandom(play, attacker, &attacker->world.pos, 0xA0);
            Actor_Kill(attacker);
        }
    }
    PlaySfx(player, NA_SE_EN_GANON_AT_RETURN);
    Health_ChangeBy(play, SOUL_HEAL);
}

static void DrainOnGuard(PlayState* play, Player* player, Actor* attacker) {
    if (!IsWorn() || attacker == NULL || attacker == &player->actor) {
        return;
    }
    if (sIkana.isGuarding && sIkana.guardTimer <= GUARD_WINDOW) {
        DrainSoul(play, player, attacker);
    }
}

// Health_ChangeBy reports the loss before anything reads it, so a revive here means the game never sees a death.
static void SaveFromDeath(int16_t amount) {
    PlayState* play = gPlayState;

    if (amount >= 0 || play == NULL || !IsWorn() || sIkana.isDeathSaveUsed || gSaveContext.health > 0) {
        return;
    }
    sIkana.isDeathSaveUsed = true;
    gSaveContext.health = REVIVE_HEALTH;
    play->envCtx.fillScreen = true;
    play->envCtx.screenFillColor[0] = 80;
    play->envCtx.screenFillColor[1] = 0;
    play->envCtx.screenFillColor[2] = 120;
    play->envCtx.screenFillColor[3] = 200;
    sIkana.flashTimer = REVIVE_FLASH_FRAMES;
    Audio_PlaySoundGeneral(NA_SE_EN_FANTOM_LAUGH, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static void HideVanillaShield(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (!IsWorn() || !HasMmModel()) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_R_HAND && IsShieldInHand(player)) {
        *dList = ResourceMgr_LoadGfxByName(sMmShieldInHandDL);
    } else if (limbIndex == PLAYER_LIMB_SHEATH && IsShieldOnBack(player)) {
        // Sheath 18 still carries the sword; only 19 is the empty sheath.
        *dList = ResourceMgr_LoadGfxByName(player->sheathType == PLAYER_MODELTYPE_SHEATH_18
                                               ? sSheathedSwordDL[gSaveContext.linkAge]
                                               : sBareSheathDL[gSaveContext.linkAge]);
    }
}

// On XLU on purpose: a foreign display list leaves its combiner on the pipe, and on OPA that blacks out every limb
// drawn after it.
static void DrawShieldOnBack(PlayState* play, Player* player, int32_t limbIndex) {
    if (!IsWorn() || !HasMmModel() || player != GET_PLAYER(play) || limbIndex != PLAYER_LIMB_SHEATH ||
        !IsShieldOnBack(player) || IsShieldInHand(player) || player->rightHandType == PLAYER_MODELTYPE_RH_FF) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sMmShieldOnBackDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The MM shield is modelled for the arm: spin about world up first, then stand it upright facing the camera.
static void DrawGetItem(PlayState* play, GetItemEntry* entry) {
    if (!HasMmModel()) {
        GetItem_Draw(play, GID_SHIELD_MIRROR);
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    Matrix_RotateX(M_PI / 2.0f, MTXMODE_APPLY);
    Matrix_Scale(GET_ITEM_SCALE, GET_ITEM_SCALE, GET_ITEM_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sMmShieldOnBackDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void RenewDeathSave(int16_t sceneNum) {
    sIkana.isDeathSaveUsed = false;
    sIkana.flashTimer = 0;
}

static void ForgetGuard(const char* key) {
    sIkana.guardTimer = 0;
    sIkana.isGuarding = false;
}

static const char* const sRequiredHooks[] = { "OnPlayerUpdate",          "OnPlayerShieldBlocked",
                                              "OnPlayerHealthChange",    "OnPlayerResolveLimbDraw",
                                              "OnPlayerPostLimbDraw",    "OnSceneInit" };

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
    definition.key = IKANA_SHIELD_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rShield of Ikana&%wBlock with %y\xA3%w right as you raise it to drain the attacker's "
                           "soul. Once per area it holds death off.";
    definition.getItemText = "You got the %rShield of Ikana%w!&The mirror of a kingdom of ghosts. It drinks the soul of "
                             "whatever strikes it in time, and once in every place it will not let you fall.";
    definition.slot = SOH_EQUIP_SLOT_SHIELD;
    definition.page = 0;
    definition.row = SOH_EQUIP_SLOT_SHIELD;
    definition.column = 3;
    definition.ageRequirement = SOH_CUSTOM_ITEM_AGE_CHILD;
    definition.vanillaBase = EQUIP_VALUE_SHIELD_MIRROR;
    definition.toggles = 1;
    definition.onUnequip = ForgetGuard;
    definition.getItemEntry.drawFunc = DrawGetItem;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }

    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, TickShield);
    SOH_REGISTER_HOOK(sApi, OnPlayerShieldBlocked, DrainOnGuard);
    SOH_REGISTER_HOOK(sApi, OnPlayerHealthChange, SaveFromDeath);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, HideVanillaShield);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawShieldOnBack);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, RenewDeathSave);
}
