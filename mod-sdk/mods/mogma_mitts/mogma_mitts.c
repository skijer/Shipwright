#include <stdarg.h>
#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/Enhancements/randomizer/randomizerTypes.h"

#define MITTS_KEY "nei.mogma_mitts"
#define MITTS_MAGIC_PER_DRAIN 1
#define MITTS_DRAIN_INTERVAL 10
#define MITTS_GIVE_SCALE 0.5f
#define MITTS_BUTTON_COUNT 8
#define CLIMB_EVERYTHING_CVAR "gCheats.ClimbEverything"

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconMogmaMittsTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gMogmaMittsNameTex";
static const ALIGN_ASSET(2) char sGiveDL[] = "__OTR__objects/object_nei_mogma_mitts/gMogmaMittsGiveDL";
static const ALIGN_ASSET(2) char sLeftPlateDL[] = "__OTR__objects/object_link_boy/gLinkAdultLeftGauntletPlate1DL";
static const ALIGN_ASSET(2) char sRightPlateDL[] = "__OTR__objects/object_link_boy/gLinkAdultRightGauntletPlate1DL";
static const ALIGN_ASSET(2) char sLeftOpenPlateDL[] = "__OTR__objects/object_link_boy/gLinkAdultLeftGauntletPlate2DL";
static const ALIGN_ASSET(2) char sLeftFistPlateDL[] = "__OTR__objects/object_link_boy/gLinkAdultLeftGauntletPlate3DL";
static const ALIGN_ASSET(2) char sRightOpenPlateDL[] = "__OTR__objects/object_link_boy/gLinkAdultRightGauntletPlate2DL";
static const ALIGN_ASSET(2) char sRightFistPlateDL[] = "__OTR__objects/object_link_boy/gLinkAdultRightGauntletPlate3DL";

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnActorDrawEnd", "OnLoadGame" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static bool sIsGripping;
static bool sIsClimbEverythingLent;
static s32 sClimbEverythingBefore;
static s16 sDrainTimer;
static s8 sPreviousInvincibility;

u8 Randomizer_GetSettingValue(RandomizerSettingKey randoSettingKey);

static void PlaySfxAt(u16 sfxId, Vec3f* pos) {
    Audio_PlaySoundGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

static bool HasClimbAbility(void) {
    return !IS_RANDO || !Randomizer_GetSettingValue(RSK_SHUFFLE_CLIMB) || Flags_GetRandomizerInf(RAND_INF_CAN_CLIMB);
}

static void LendClimbEverything(bool shouldLend) {
    if (shouldLend == sIsClimbEverythingLent) {
        return;
    }
    if (shouldLend) {
        sClimbEverythingBefore = CVarGetInteger(CLIMB_EVERYTHING_CVAR, 0);
        CVarSetInteger(CLIMB_EVERYTHING_CVAR, 1);
    } else {
        CVarSetInteger(CLIMB_EVERYTHING_CVAR, sClimbEverythingBefore);
    }
    sIsClimbEverythingLent = shouldLend;
}

static bool IsEquipped(void) {
    for (u8 button = 0; button < MITTS_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && strcmp(equipped, MITTS_KEY) == 0) {
            return true;
        }
    }
    return false;
}

static bool HasMagicToGrip(void) {
    return gSaveContext.isMagicAcquired && gSaveContext.magic >= MITTS_MAGIC_PER_DRAIN;
}

static void StartGripping(Player* player) {
    sIsGripping = true;
    sDrainTimer = 0;
    sPreviousInvincibility = player->invincibilityTimer;
    PlaySfxAt(NA_SE_SY_LOCK_ON, &player->actor.world.pos);
}

static void StopGripping(void) {
    sIsGripping = false;
    LendClimbEverything(false);
}

static void ToggleGrip(PlayState* play, Player* player) {
    if (sIsGripping) {
        StopGripping();
        PlaySfxAt(NA_SE_SY_CANCEL, &player->actor.world.pos);
        return;
    }
    if (!HasMagicToGrip()) {
        PlaySfxAt(NA_SE_SY_ERROR, &player->actor.world.pos);
        return;
    }
    StartGripping(player);
}

// Without the climb ability the randomizer zeroes the stick; the mitts hand the real one back.
static void GrantClimbing(bool* should, va_list args) {
    PlayState* play = gPlayState;
    s32* stickX = va_arg(args, s32*);
    s32* stickY = va_arg(args, s32*);

    if (!sIsGripping || HasClimbAbility() || play == NULL) {
        return;
    }
    *stickX = play->state.input[0].rel.stick_x;
    *stickY = play->state.input[0].rel.stick_y;
}

static void GrantCrawling(bool* should, va_list args) {
    if (sIsGripping) {
        *should = true;
    }
}

static bool WasJustHurt(Player* player) {
    bool isHurt = player->invincibilityTimer > 0 && sPreviousInvincibility == 0;

    sPreviousInvincibility = player->invincibilityTimer;
    return isHurt;
}

static void DrainMagic(Player* player) {
    if (++sDrainTimer < MITTS_DRAIN_INTERVAL) {
        return;
    }
    sDrainTimer = 0;
    if (!HasMagicToGrip()) {
        StopGripping();
        PlaySfxAt(NA_SE_SY_ERROR, &player->actor.world.pos);
        return;
    }
    gSaveContext.magic -= MITTS_MAGIC_PER_DRAIN;
}

static void UpdateMitts(void) {
    Player* player = gPlayState == NULL ? NULL : GET_PLAYER(gPlayState);

    if (!sIsGripping || player == NULL) {
        return;
    }
    if (!IsEquipped() || WasJustHurt(player)) {
        StopGripping();
        return;
    }
    LendClimbEverything(HasClimbAbility());
    DrainMagic(player);
}

static void StopGrippingOnLoad(int32_t fileNum) {
    StopGripping();
}

static bool IsShowingVanillaGauntlets(void) {
    return LINK_IS_ADULT && CUR_UPG_VALUE(UPG_STRENGTH) >= 2;
}

static void DrawGauntlets(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    if (!sIsGripping || IsShowingVanillaGauntlets() || (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW) ||
        (player->stateFlags1 & PLAYER_STATE1_FIRST_PERSON) || (player->stateFlags2 & PLAYER_STATE2_CRAWLING)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 0);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sLeftPlateDL);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sRightPlateDL);
    gSPDisplayList(POLY_OPA_DISP++,
                   (Gfx*)(player->leftHandType == PLAYER_MODELTYPE_LH_OPEN ? sLeftOpenPlateDL : sLeftFistPlateDL));
    gSPDisplayList(POLY_OPA_DISP++,
                   (Gfx*)(player->rightHandType == PLAYER_MODELTYPE_RH_OPEN ? sRightOpenPlateDL : sRightFistPlateDL));
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawGetItem(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(MITTS_GIVE_SCALE, MITTS_GIVE_SCALE, MITTS_GIVE_SCALE, MTXMODE_APPLY);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGiveDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterCustomItem)) {
        return;
    }

    SOHCustomItemDefinition mitts = Z64Items_Define(MITTS_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&mitts, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&mitts, 0, 13, 0);
    Z64Items_SetTextbox(&mitts, "You got the %rMogma Mitts%w!&Digging claws of the Mogma tribe,&tough enough to "
                                "bite into stone.^Press %y\xA1%w to grip. Link climbs and&crawls where he couldn't, "
                                "and a&climber grips %yany wall%w.^The grip spends %gmagic%w while&it holds.");
    Z64Items_SetPauseText(&mitts, "%rMogma Mitts&%wPress %y\xA1%w to grip walls and crawl.&Press again to stop. "
                                  "Drains magic.");
    Z64Items_SetAction(&mitts, ToggleGrip, NULL);
    mitts.flags |= SOH_CUSTOM_ITEM_INSTANT;
    mitts.getItemEntry.drawFunc = DrawGetItem;

    if (!Z64Items_Register(sApi, &mitts)) {
        return;
    }
    sApi->RegisterVB(VB_CLIMB, GrantClimbing);
    sApi->RegisterVB(VB_CRAWL, GrantCrawling);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateMitts);
    SOH_REGISTER_HOOK(sApi, OnLoadGame, StopGrippingOnLoad);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawGauntlets);
}
