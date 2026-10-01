#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define ZONAI_KEY "nei.zonai_permafrost"
#define ZONAI_TIME_PRIORITY 100
#define ZONAI_MAGIC_TO_START 4
#define ZONAI_MAGIC_PER_DRAIN 1
#define ZONAI_DRAIN_INTERVAL 10
#define ZONAI_LOW_MAGIC 8
#define ZONAI_TINT_ALPHA 26
#define ZONAI_TINT_PULSE 8
#define ZONAI_RELEASE_PITCH_START 1.45f
#define ZONAI_RELEASE_PITCH_END 0.55f
#define ZONAI_RELEASE_PITCH_STEP 0.11f
#define ZONAI_GIVE_SCALE 1.0f
#define ZONAI_BUTTON_COUNT 8

#define ZONAI_BLOCKING_STATES                                                                                     \
    (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING | PLAYER_STATE1_IN_ITEM_CS |          \
     PLAYER_STATE1_TALKING | PLAYER_STATE1_GETTING_ITEM)

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconZonaiPermafrostTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gZonaiPermafrostNameTex";
static const ALIGN_ASSET(2) char sGiveDL[] = "__OTR__objects/object_nei_magic_spell/gZonaiPermafrostGiveDL";

static const u16 sItemButtons[ZONAI_BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                                      BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnSceneInit" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static bool sIsTimeStopped;
static s16 sStoppedFrames;
static u32 sToggleFrame;
static Vec3f sSfxPos;
static f32 sReleasePitch = ZONAI_RELEASE_PITCH_END;

// The ice samples carry SFX_FLAG, which parks a one-shot in PLAYING until the same position stops it.
static void PlayOwnSfx(Player* player, u16 sfxId) {
    sSfxPos = player->actor.world.pos;
    Audio_PlaySoundGeneral(sfxId - SFX_FLAG, &sSfxPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

// The sound bank rereads the pitch pointer every audio frame, so walking it down bends a cue already ringing.
static void PlayReleaseChime(void) {
    sReleasePitch = ZONAI_RELEASE_PITCH_START;
    Audio_PlaySoundGeneral(NA_SE_SY_SET_ICE_ARROW, &gSfxDefaultPos, 4, &sReleasePitch, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

static void BendReleaseChimeDown(void) {
    sReleasePitch = MAX(sReleasePitch - ZONAI_RELEASE_PITCH_STEP, ZONAI_RELEASE_PITCH_END);
}

// The same register a Deku Nut writes on impact: the transition fade runs the white-out back down by itself.
static void FlashScreen(void) {
    iREG(50) = -1;
}

static void TintScreen(PlayState* play, u8 alpha) {
    play->envCtx.fillScreen = alpha != 0;
    play->envCtx.screenFillColor[0] = 120;
    play->envCtx.screenFillColor[1] = 230;
    play->envCtx.screenFillColor[2] = 210;
    play->envCtx.screenFillColor[3] = alpha;
}

static void SpawnRuneRing(Player* player, PlayState* play, f32 radius) {
    Vec3f accel = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 prim = { 100, 255, 150, 255 };
    Color_RGBA8 env = { 0, 200, 80, 255 };

    for (u8 i = 0; i < 8; i++) {
        s16 angle = (s16)(i * (0x10000 / 8));
        Vec3f pos = { player->actor.world.pos.x + Math_SinS(angle) * radius,
                      player->actor.world.pos.y + 30.0f + Rand_CenteredFloat(20.0f),
                      player->actor.world.pos.z + Math_CosS(angle) * radius };
        Vec3f velocity = { Math_SinS(angle) * 3.0f, Rand_ZeroFloat(1.5f), Math_CosS(angle) * 3.0f };

        EffectSsKiraKira_SpawnFocused(play, &pos, &velocity, &accel, &prim, &env, 600, 20);
    }
}

static void SpawnRuneBurst(Player* player, PlayState* play) {
    SpawnRuneRing(player, play, 40.0f);
    SpawnRuneRing(player, play, 110.0f);
    SpawnRuneRing(player, play, 180.0f);
}

static void SpawnStillAir(Player* player, PlayState* play) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 prim = { 120, 255, 160, 200 };
    Color_RGBA8 env = { 0, 180, 60, 150 };

    for (u8 i = 0; i < 3; i++) {
        Vec3f pos = { player->actor.world.pos.x + Rand_CenteredFloat(360.0f),
                      player->actor.world.pos.y + 20.0f + Rand_ZeroFloat(130.0f),
                      player->actor.world.pos.z + Rand_CenteredFloat(360.0f) };

        EffectSsKiraKira_SpawnFocused(play, &pos, &zero, &zero, &prim, &env, 460, 15);
    }
}

static bool HasMagic(s16 amount) {
    return gSaveContext.isMagicAcquired && gSaveContext.magic >= amount;
}

static void StartTime(Player* player, PlayState* play) {
    sApi->ReleaseTimeControl(ZONAI_KEY);
    sIsTimeStopped = false;
    SpawnRuneBurst(player, play);
    FlashScreen();
    TintScreen(play, 0);
    func_800AA000(200.0f, 100, 15, 40);
    Audio_StopSfxByPos(&sSfxPos);
    PlayOwnSfx(player, NA_SE_EV_ICE_MELT);
    PlayReleaseChime();
}

static void StopTime(Player* player, PlayState* play) {
    if (!HasMagic(ZONAI_MAGIC_TO_START)) {
        Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }
    gSaveContext.magic -= ZONAI_MAGIC_TO_START;
    sApi->RequestTimeControl(ZONAI_KEY, ZONAI_TIME_PRIORITY, 0.0f, true);
    sIsTimeStopped = true;
    sStoppedFrames = 0;
    SpawnRuneBurst(player, play);
    FlashScreen();
    TintScreen(play, ZONAI_TINT_ALPHA);
    func_800AA000(300.0f, 150, 20, 60);
    Audio_StopSfxByPos(&sSfxPos);
    PlayOwnSfx(player, NA_SE_EV_ICE_FREEZE);
    Audio_PlaySoundGeneral(NA_SE_SY_SET_ICE_ARROW, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static void ToggleTime(PlayState* play, Player* player) {
    sToggleFrame = play->state.frames;
    if (sIsTimeStopped) {
        StartTime(player, play);
    } else {
        StopTime(player, play);
    }
}

static s32 FindEquippedButton(void) {
    for (u8 button = 0; button < ZONAI_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && strcmp(equipped, ZONAI_KEY) == 0) {
            return button;
        }
    }
    return -1;
}

static void KeepTimeStopped(Player* player, PlayState* play) {
    if (sStoppedFrames < 0x7000) {
        sStoppedFrames++;
    }
    if ((sStoppedFrames % ZONAI_DRAIN_INTERVAL) == 0) {
        if (!HasMagic(ZONAI_MAGIC_PER_DRAIN)) {
            StartTime(player, play);
            return;
        }
        gSaveContext.magic -= ZONAI_MAGIC_PER_DRAIN;
    }
    bool isFlickering = !HasMagic(ZONAI_LOW_MAGIC) && (play->gameplayFrames % 4) < 2;
    if (isFlickering) {
        TintScreen(play, 0);
    } else {
        SpawnStillAir(player, play);
        TintScreen(play, (u8)(ZONAI_TINT_ALPHA + (s32)(Math_SinS((s16)(sStoppedFrames * 1200)) * ZONAI_TINT_PULSE)));
    }
    if ((play->gameplayFrames % 40) == 0) {
        PlayOwnSfx(player, NA_SE_EV_ICE_MELT);
    }
}

// Vanilla stops reading item buttons in midair or while swimming; the time stop keeps answering there.
static bool WasToggledOutsideItemButtons(Player* player, PlayState* play, s32 button) {
    return button >= 0 && (play->state.input[0].press.button & sItemButtons[button]) &&
           play->state.frames != sToggleFrame && !(player->stateFlags1 & ZONAI_BLOCKING_STATES) &&
           (!(player->actor.bgCheckFlags & 1) || (player->stateFlags1 & PLAYER_STATE1_IN_WATER));
}

static void UpdateZonai(void) {
    PlayState* play = gPlayState;
    Player* player = gPlayState == NULL ? NULL : GET_PLAYER(gPlayState);

    BendReleaseChimeDown();
    if (play == NULL || player == NULL) {
        return;
    }
    s32 button = FindEquippedButton();
    if (sIsTimeStopped && button < 0) {
        StartTime(player, play);
        return;
    }
    if (WasToggledOutsideItemButtons(player, play, button)) {
        ToggleTime(play, player);
        return;
    }
    if (sIsTimeStopped) {
        KeepTimeStopped(player, play);
    }
}

static void ForgetStopOnSceneChange(int16_t sceneNum) {
    sIsTimeStopped = false;
}

static bool CanToggleTime(Player* player, PlayState* play) {
    return !(player->stateFlags1 & ZONAI_BLOCKING_STATES);
}

static void DrawGetItem(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(ZONAI_GIVE_SCALE, ZONAI_GIVE_SCALE, ZONAI_GIVE_SCALE, MTXMODE_APPLY);
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
    if (!SOH_MOD_API_HAS(sApi, RequestTimeControl)) {
        return;
    }

    SOHCustomItemDefinition zonai = Z64Items_Define(ZONAI_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&zonai, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&zonai, 0, 11, 0);
    Z64Items_SetTextbox(&zonai, "You got the %rZonai Permafrost%w!&A Zonai device that holds the&flow of time "
                                "perfectly still.^Press %y\xA1%w to %cstop time%w. Only you&keep moving, and "
                                "enemies stay open&to your blows.^It spends %gmagic%w for as long as the&world "
                                "is frozen.");
    Z64Items_SetPauseText(&zonai, "%rZonai Permafrost&%wPress %y\xA1%w to stop time, again to&let it run. "
                                  "Drains magic.");
    Z64Items_SetCanUse(&zonai, CanToggleTime);
    Z64Items_SetAction(&zonai, ToggleTime, NULL);
    zonai.flags |= SOH_CUSTOM_ITEM_INSTANT;
    zonai.getItemEntry.drawFunc = DrawGetItem;

    if (!Z64Items_Register(sApi, &zonai)) {
        return;
    }
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateZonai);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetStopOnSceneChange);
}
