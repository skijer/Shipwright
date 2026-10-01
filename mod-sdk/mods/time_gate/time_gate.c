#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define TIME_GATE_KEY "nei.time_gate"
#define TIME_GATE_MAGIC_COST 48
#define TIME_GATE_ANIM_SPEED 0.83f
#define TIME_GATE_PROMPT_DELAY 10
#define TIME_GATE_ITEM_FRAME 10.0f
#define TIME_GATE_HELD_SCALE 0.008f
#define TIME_GATE_GIVE_SCALE 0.5f

typedef enum {
    TIME_GATE_IDLE,
    TIME_GATE_CASTING,
    TIME_GATE_HOVERING,
    TIME_GATE_CANCELLING,
} TimeGatePhase;

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconTimeGateTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gTimeGateNameTex";
static const ALIGN_ASSET(2) char sGateDL[] = "__OTR__objects/object_nei_time_gate/g_timegate_dl";
static const ALIGN_ASSET(2) char sWarpPortalDL[] = "__OTR__objects/object_warp1/gWarpPortalDL";
static const ALIGN_ASSET(2) char sCastAnims[][64] = {
    "__OTR__objects/gameplay_keep/gPlayerAnim_link_magic_tamashii1",
    "__OTR__objects/gameplay_keep/gPlayerAnim_link_magic_tamashii2",
    "__OTR__objects/gameplay_keep/gPlayerAnim_link_magic_tamashii3",
};
static const ALIGN_ASSET(2) char sHoverAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_demo_warp";

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnActorDrawEnd" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static u8 sPhase;
static u8 sCastStep;
static s16 sTimer;
static bool sIsPromptShown;
static bool sIsCastPending;
static bool sIsGateInHand;
static f32 sPortalAlpha;
static f32 sPortalScale;
static f32 sPortalScroll;

void SwitchAge(void);
void Player_PlayVoiceSfx(Player* player, u16 sfxId);
s32 Player_InBlockingCsMode(PlayState* play, Player* player);
void Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void Player_ZeroSpeedXZ(Player* player);
void func_80839FFC(Player* player, PlayState* play);

static void PlaySfxAt(u16 sfxId, Vec3f* pos) {
    Audio_PlaySoundGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

static void TimeGateAction(Player* player, PlayState* play);

static bool IsCasting(Player* player) {
    return player->actionFunc == TimeGateAction;
}

static void PlayOnce(PlayState* play, Player* player, const char* animation, f32 morph) {
    LinkAnimationHeader* anim = (LinkAnimationHeader*)animation;

    LinkAnimation_Change(play, &player->skelAnime, anim, TIME_GATE_ANIM_SPEED, 0.0f, Animation_GetLastFrame(anim),
                         ANIMMODE_ONCE, morph);
}

static void SpawnTimeSparkle(Player* player, PlayState* play, f32 height) {
    Vec3f pos = { player->actor.world.pos.x + Rand_CenteredFloat(25.0f),
                  player->actor.world.pos.y + height + Rand_ZeroFloat(40.0f),
                  player->actor.world.pos.z + Rand_CenteredFloat(25.0f) };
    Vec3f velocity = { 0.0f, 1.0f, 0.0f };
    Vec3f accel = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 prim = { 150, 150, 255, 255 };
    Color_RGBA8 env = { 80, 50, 200, 255 };

    EffectSsKiraKira_SpawnFocused(play, &pos, &velocity, &accel, &prim, &env, 500, 18);
}

static void ResetGate(void) {
    sIsCastPending = false;
    sPhase = TIME_GATE_IDLE;
    sCastStep = 0;
    sTimer = 0;
    sIsPromptShown = false;
    sIsGateInHand = false;
    sPortalAlpha = 0.0f;
    sPortalScale = 0.0f;
}

static void CloseOpenPrompt(PlayState* play) {
    if (!sIsPromptShown) {
        return;
    }
    Message_CloseTextbox(play);
    play->msgCtx.msgMode = MSGMODE_TEXT_DONE;
    sIsPromptShown = false;
}

static void ReleasePlayer(Player* player, PlayState* play) {
    CloseOpenPrompt(play);
    func_8005B1A4(Play_GetCamera(play, 0));
    if (IsCasting(player)) {
        func_80839FFC(player, play);
    }
    ResetGate();
}

static bool HasMagicForGate(void) {
    return gSaveContext.isMagicAcquired && gSaveContext.magic >= TIME_GATE_MAGIC_COST;
}

// The meter drains over several frames and the travel reloads the scene in this one, so the cost is settled
// by hand: asking the interface for it would give the magic back with the scene.
static void SpendMagic(PlayState* play) {
    if (Magic_RequestChange(play, TIME_GATE_MAGIC_COST, MAGIC_CONSUME_NOW)) {
        gSaveContext.magic = gSaveContext.magicTarget;
    }
    Magic_Reset(play);
}

static bool CanOpenGate(Player* player, PlayState* play) {
    return sPhase == TIME_GATE_IDLE && (player->actor.bgCheckFlags & 1) &&
           !(player->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_FIRST_PERSON));
}

static void BeginCasting(PlayState* play, Player* player) {
    Player_SetupAction(play, player, TimeGateAction, 0);
    Player_ZeroSpeedXZ(player);
    player->stateFlags1 |= PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE;
    Camera_ChangeSetting(Play_GetCamera(play, 0), CAM_SET_TURN_AROUND);
    Camera_SetCameraData(Play_GetCamera(play, 0), 4, NULL, NULL, 10, 0, 0);
    ResetGate();
    sPhase = TIME_GATE_CASTING;
    PlayOnce(play, player, sCastAnims[0], -8.0f);
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_MAGIC_NALE);
}

// Pressing the button while running lands in the middle of vanilla's own state change, which would replace the
// action func the same frame: the cast waits for the next one.
static void OpenGate(PlayState* play, Player* player) {
    if (!HasMagicForGate()) {
        PlaySfxAt(NA_SE_SY_ERROR, &player->actor.world.pos);
        return;
    }
    sIsCastPending = true;
}

static void GrowPortal(void) {
    sPortalAlpha = MIN(sPortalAlpha + 8.0f, 255.0f);
    sPortalScale = MIN(sPortalScale + 0.05f, 1.0f);
}

static void ShrinkPortal(void) {
    sPortalAlpha = MAX(sPortalAlpha - 12.0f, 0.0f);
    sPortalScale = MAX(sPortalScale - 0.04f, 0.0f);
}

static void StartHovering(Player* player, PlayState* play) {
    sPhase = TIME_GATE_HOVERING;
    sTimer = 0;
    PlayOnce(play, player, sHoverAnim, -8.0f);
    PlaySfxAt(NA_SE_PL_MAGIC_WIND_WARP, &player->actor.world.pos);
}

static void UpdateCasting(Player* player, PlayState* play, bool isAnimDone) {
    if (sCastStep == 0 && !sIsGateInHand && player->skelAnime.curFrame >= TIME_GATE_ITEM_FRAME) {
        sIsGateInHand = true;
        PlaySfxAt(NA_SE_EV_WARP_HOLE, &player->actor.world.pos);
    }
    if (sIsGateInHand) {
        GrowPortal();
    }
    if (++sTimer > 10 && (play->gameplayFrames % 4) == 0) {
        SpawnTimeSparkle(player, play, 30.0f);
    }
    if (!isAnimDone) {
        return;
    }
    if (++sCastStep < ARRAY_COUNT(sCastAnims)) {
        PlayOnce(play, player, sCastAnims[sCastStep], 0.0f);
        return;
    }
    StartHovering(player, play);
}

static void HoldLastHoverFrames(Player* player) {
    f32 lastFrame = Animation_GetLastFrame((LinkAnimationHeader*)sHoverAnim);

    if (player->skelAnime.curFrame >= lastFrame - 0.5f) {
        player->skelAnime.curFrame = MAX(lastFrame - 2.0f, 0.0f);
    }
}

static void TravelThroughTime(Player* player, PlayState* play) {
    SpendMagic(play);
    func_800AA000(400.0f, 200, 30, 100);
    PlaySfxAt(NA_SE_SY_WHITE_OUT_T, &player->actor.world.pos);
    ReleasePlayer(player, play);
    SwitchAge();
}

static void StartCancelling(Player* player, PlayState* play) {
    sPhase = TIME_GATE_CANCELLING;
    sIsGateInHand = false;
    PlayOnce(play, player, sCastAnims[2], -8.0f);
}

static void UpdateHovering(Player* player, PlayState* play) {
    HoldLastHoverFrames(player);
    if ((play->gameplayFrames % 3) == 0) {
        SpawnTimeSparkle(player, play, 20.0f);
    }
    if (++sTimer == TIME_GATE_PROMPT_DELAY) {
        sIsPromptShown = sApi->ShowTextbox(play, "Travel through time?\x1B%g&&Yes&No%w", false);
    }
    if (!sIsPromptShown || Message_GetState(&play->msgCtx) != TEXT_STATE_CHOICE || !Message_ShouldAdvance(play)) {
        return;
    }
    CloseOpenPrompt(play);
    if (play->msgCtx.choiceIndex == 0) {
        TravelThroughTime(player, play);
    } else {
        StartCancelling(player, play);
    }
}

static void UpdateCancelling(Player* player, PlayState* play, bool isAnimDone) {
    ShrinkPortal();
    if (isAnimDone) {
        ReleasePlayer(player, play);
    }
}

static void TimeGateAction(Player* player, PlayState* play) {
    bool isAnimDone = LinkAnimation_Update(play, &player->skelAnime);

    Player_ZeroSpeedXZ(player);
    switch (sPhase) {
        case TIME_GATE_CASTING:
            UpdateCasting(player, play, isAnimDone);
            break;
        case TIME_GATE_HOVERING:
            UpdateHovering(player, play);
            break;
        case TIME_GATE_CANCELLING:
            UpdateCancelling(player, play, isAnimDone);
            break;
        default:
            ReleasePlayer(player, play);
            break;
    }
}

static void StartPendingCast(Player* player, PlayState* play) {
    if (!sIsCastPending) {
        return;
    }
    if (!CanOpenGate(player, play) || Player_InBlockingCsMode(play, player)) {
        sIsCastPending = false;
        return;
    }
    sIsCastPending = false;
    BeginCasting(play, player);
}

static void UpdateGateOutsideCast(void) {
    PlayState* play = gPlayState;
    Player* player = gPlayState == NULL ? NULL : GET_PLAYER(gPlayState);

    if (play == NULL || player == NULL) {
        return;
    }
    StartPendingCast(player, play);
    if (sPhase == TIME_GATE_IDLE || IsCasting(player)) {
        return;
    }
    ReleasePlayer(player, play);
}

static void DrawGateInHand(Player* player, PlayState* play) {
    Vec3f hand = player->bodyPartsPos[PLAYER_BODYPART_L_HAND];

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate(hand.x + Math_SinS(player->actor.shape.rot.y) * 5.0f, hand.y - 10.0f,
                     hand.z + Math_CosS(player->actor.shape.rot.y) * 5.0f, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(player->actor.shape.rot.y), MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD(-0x1000), MTXMODE_APPLY);
    Matrix_Scale(TIME_GATE_HELD_SCALE, TIME_GATE_HELD_SCALE, TIME_GATE_HELD_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGateDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawPortalLayer(PlayState* play, u32 scroll, f32 height, f32 width) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScroll(play->state.gfxCtx, 0, scroll & 0xFF, -((s16)(scroll * 2) & 511), 0x100, 0x100, 1,
                                scroll & 0xFF, -((s16)(scroll * 2) & 511), 0x100, 0x100));
    Matrix_Translate(0.0f, height, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(width, 1.0f, width, MTXMODE_APPLY);
    gSPSegment(POLY_XLU_DISP++, 0x09, MATRIX_NEWMTX(play->state.gfxCtx));
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sWarpPortalDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Modelled on DoorWarp1's blue warp: segment 0x0A holds the base matrix, 0x09 each layer's, 0x08 the scroll.
static void DrawPortal(Player* player, PlayState* play) {
    f32 height = 0.3f * sPortalScale;

    sPortalScroll = sPortalScroll + 15.0f > 512.0f ? sPortalScroll + 15.0f - 512.0f : sPortalScroll + 15.0f;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0x00, 0x80, 180, 200, 255, (u8)sPortalAlpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 50, 100, 255, 255);
    Matrix_Translate(player->actor.world.pos.x, player->actor.world.pos.y + 1.0f, player->actor.world.pos.z,
                     MTXMODE_NEW);
    gSPSegment(POLY_XLU_DISP++, 0x0A, MATRIX_NEWMTX(play->state.gfxCtx));
    CLOSE_DISPS(play->state.gfxCtx);

    Matrix_Push();
    DrawPortalLayer(play, (u32)sPortalScroll, height * 230.0f, 0.8f * sPortalScale);
    Matrix_Pop();
    if (sPortalAlpha <= 128.0f) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0x00, 0x80, 200, 220, 255, (u8)MIN((sPortalAlpha - 128.0f) * 2.0f, 255.0f));
    gDPSetEnvColor(POLY_XLU_DISP++, 100, 150, 255, 255);
    CLOSE_DISPS(play->state.gfxCtx);
    DrawPortalLayer(play, (u32)sPortalScroll * 2, height * 60.0f, 0.6f * sPortalScale);
}

static void DrawGate(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    if (sPhase == TIME_GATE_IDLE || (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW)) {
        return;
    }
    if (sIsGateInHand) {
        DrawGateInHand(player, play);
    }
    if (sPortalAlpha > 0.0f) {
        DrawPortal(player, play);
    }
}

static void DrawGetItem(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(TIME_GATE_GIVE_SCALE, TIME_GATE_GIVE_SCALE, TIME_GATE_GIVE_SCALE, MTXMODE_APPLY);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGateDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, ShowTextbox)) {
        return;
    }

    SOHCustomItemDefinition gate = Z64Items_Define(TIME_GATE_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&gate, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&gate, 0, 7, 0);
    Z64Items_SetTextbox(&gate, "You got the %rTime Gate%w!&A relic of the Sages that folds&the years between "
                               "past and future.^Press %y\xA1%w on solid ground to open&it and travel through "
                               "%ytime%w.^Opening the gate costs %gmuch magic%w.");
    Z64Items_SetPauseText(&gate, "%rTime Gate&%wPress %y\xA1%w on the ground, then choose&Yes to change age.");
    Z64Items_SetCanUse(&gate, CanOpenGate);
    Z64Items_SetAction(&gate, OpenGate, NULL);
    gate.flags |= SOH_CUSTOM_ITEM_INSTANT;
    gate.getItemEntry.drawFunc = DrawGetItem;

    if (!Z64Items_Register(sApi, &gate)) {
        return;
    }
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateGateOutsideCast);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawGate);
}
