#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/ModApi/PlayerInput/PlayerInput.h"

#define GORON_MASK_KEY "nei.goron_form_mask"
#define GORON_FORM_KEY "nei.goron"
// No resources live here: a model path is what turns on the host's root scaling, as for Deku.
#define GORON_MODEL_PATH "objects/forms/goron"
#define GORON_HAZARD_OWNER "nei.goron"
#define GORON_MASK_PAGE 1
#define GORON_MASK_SLOT 11

#define BG_ON_GROUND 1
#define BG_TOUCHING_WALL 8
#define BG_LEAVE_LEDGES 0x800
#define BG_LEFT_GROUND 4
#define BALL_HUG_TOLERANCE 0.5f
#define MM_ANIM_VALUES_PER_FRAME 67
#define MM_ANIM_BASE_TRANSL_X (-57)
// Rest height of the Goron root (2700) over the height Link's animations carry the root to.
#define GORON_ROOT_SCALE_ADULT 0.800f
#define GORON_ROOT_SCALE_CHILD 1.136f
// MM plays every Goron clip at this root scale, so its root motion is in these units.
#define GORON_ANIM_SCALE 0.74f
#define GORON_HEIGHT 80.0f
#define GORON_CURLED_HEIGHT 34.0f

#define PUNCH_STEPS 3
#define PUNCH_HIT_START 5.0f
#define PUNCH_DMG_FLAGS DMG_HAMMER_SWING
#define PUNCH_DAMAGE 2
#define PUNCH_RECOIL_SPEED -18.0f
#define BUTT_IMPACT_FRAME 8.0f

#define CURL_PLAY_SPEED 0.67f
#define BALL_MAX_SPEED 18.0f
#define BALL_BOUNCE_SPEED 12.0f
#define BALL_STICK_SPEED (8.0f * 2.6f)
#define BALL_CHARGE_SPIN 0x36B0
#define BALL_CHARGE_READY 0x36
#define BALL_CHARGE_CAP 0x100
#define BALL_CHARGE_IDLE 4
#define BALL_SPIKE_FULL 7
#define BALL_SPIKE_COST 2
#define BALL_DRAIN_INTERVAL 10
#define BALL_STEEP_SLOPE 0x3A98
#define BALL_DRAW_SCALE 1.15f
#define BALL_DRAW_LIFT 1200.0f
#define BALL_SHADOW 30.0f
#define ROLL_DMG_FLAGS DMG_HAMMER_SWING
#define ROLL_DAMAGE 1
#define ROLL_RADIUS 25
#define POUND_LIFT 14.0f
#define POUND_MIN_SPIN 0x1F40
#define POUND_DMG_FLAGS DMG_HAMMER_JUMP
#define POUND_DAMAGE 4
#define POUND_RADIUS 60
#define POUND_PAUSE 10
#define GRAVITY_NORMAL -1.2f
#define GRAVITY_APEX -0.2f
#define GRAVITY_SLAM -10.0f

#define GUARD_RADIUS 30
#define GUARD_HEIGHT 35
#define SHIELDING_JOINTS 8

#define SINK_DEPTH 30.0f
#define SINK_BALL_FRAMES 17

// LinkAnimation advances R_UPDATE_RATE * 0.5 clip frames per update, and the rate is always 3.
#define CLIP_FRAMES_PER_UPDATE 1.5f

#define OCARINA_NOTE_NONE 0xFF
#define LINK_VOICE_ACTIONS 0x20
#define MM_GORON_VOICE_OFFSET 0xC0
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"
#define MM_SFX_STOP_SERVICE "mm.sfx.stop"
#define MM_SFX_PLAY_SCALED_SERVICE "mm.sfx.play_scaled"
#define MM_GORON_STEP_OFFSET 0x150
#define MM_GORON_BALLJUMP 0x08E1
#define MM_GORON_TO_BALL 0x08E6
#define MM_BALL_TO_GORON 0x08E7
#define MM_GORON_PUNCH 0x08E8
// MM asks for its loops as `id - SFX_FLAG` every frame: without the flag its scheduler plays the sound for as
// long as it keeps being asked for, with it every request starts the sample over.
#define MM_SFX_FLAG 0x800
#define MM_GORON_BALL_CHARGE_LOOP (0x08EB - MM_SFX_FLAG)
#define MM_GORON_SLIP_LOOP (0x09AD - MM_SFX_FLAG)
#define MM_GORON_SQUAT 0x08EF
#define MM_GORON_CHG_ROLL 0x0980
#define MM_GORON_ROLL 0x0990
#define MM_FLOOR_SFX_COUNT 0x10
#define MM_GORON_BALL_CHARGE_FAILED 0x09A2
#define MM_GORON_BALL_CHARGE_DASH 0x09A3
#define MM_LI_FUTTOBI 0x09C8
#define MM_GORON_PUNCH_SWING 0x1857
#define MM_GORON_ROLLING_REFLECTION 0x185E
#define MM_VO_GORON_SWORD_N 0x68C0
#define MM_VO_GORON_SWORD_L 0x68C1
typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);
typedef void (*MmSfxStopFunc)(uint16_t sfxId);
typedef bool (*MmSfxPlayScaledFunc)(uint16_t sfxId, Vec3f* pos, f32* freqScale, f32* volume);
#define MM_OCARINA_NOTE_SERVICE "mm.ocarina.note"
#define MM_OCARINA_INSTRUMENT_GORON_DRUMS 7
typedef bool (*MmOcarinaNoteFunc)(uint8_t instrumentId, uint8_t pitch, f32* bendFreq);

#define OBJ_GORON "__OTR__objects/object_link_goron/"
static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_static/gItemIconMaskGoronTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_static/gGoronMaskItemNameENGTex";
static const ALIGN_ASSET(2) char sGetItemDL[] = "__OTR__objects/object_gi_golonmask/gGiGoronMaskDL";
static const ALIGN_ASSET(2) char sMaskDL[] = "__OTR__objects/object_link_child/gLinkChildGoronMaskDL";
static const ALIGN_ASSET(2) char sCurledDL[] = OBJ_GORON "gLinkGoronCurledDL";
static const ALIGN_ASSET(2) char sSpikesDL[] = OBJ_GORON "object_link_goron_DL_00C540";
static const ALIGN_ASSET(2) char sChargeGlowDL[] = OBJ_GORON "object_link_goron_DL_0127B0";
static const ALIGN_ASSET(2) char sChargeFlareDL[] = OBJ_GORON "object_link_goron_DL_0134D0";
static const ALIGN_ASSET(2) char sPunchEffectDL[] = OBJ_GORON "gLinkGoronGoronPunchEffectDL";
static const ALIGN_ASSET(2) char sDrumDL[] = OBJ_GORON "object_link_goron_DL_00FC18";
static const ALIGN_ASSET(2) char sShieldingSkelPath[] = OBJ_GORON "gLinkGoronShieldingSkel";
static const ALIGN_ASSET(2) char sShieldingAnimPath[] = OBJ_GORON "gLinkGoronShieldingAnim";
static const char* const sDrumPieceDLs[] = {
    OBJ_GORON "object_link_goron_DL_00FCF0", OBJ_GORON "object_link_goron_DL_00FF18",
    OBJ_GORON "object_link_goron_DL_010140", OBJ_GORON "object_link_goron_DL_010368",
    OBJ_GORON "object_link_goron_DL_010590",
};

#define MM_ANIM_PATH(name) "__OTR__misc/link_animetion/gPlayerAnim_" name "_Data"
typedef enum {
    ANIM_PUNCH_A,
    ANIM_PUNCH_B,
    ANIM_PUNCH_C,
    ANIM_PUNCH_A_END,
    ANIM_PUNCH_B_END,
    ANIM_PUNCH_C_END,
    ANIM_PUNCH_A_END_LOCKED,
    ANIM_PUNCH_B_END_LOCKED,
    ANIM_PUNCH_C_END_LOCKED,
    ANIM_CURL,
    ANIM_DRUM_RAISE,
    ANIM_DRUM_PLAY,
    ANIM_MASK_ON,
    ANIM_WAIT,
    ANIM_MASK_OFF,
    ANIM_COUNT,
} GoronAnim;

static const char* const sAnimPaths[ANIM_COUNT] = {
    MM_ANIM_PATH("pg_punchA"),      MM_ANIM_PATH("pg_punchB"),     MM_ANIM_PATH("pg_punchC"),
    MM_ANIM_PATH("pg_punchAend"),   MM_ANIM_PATH("pg_punchBend"),  MM_ANIM_PATH("pg_punchCend"),
    MM_ANIM_PATH("pg_punchAendR"),  MM_ANIM_PATH("pg_punchBendR"), MM_ANIM_PATH("pg_punchCendR"),
    MM_ANIM_PATH("pg_maru_change"), MM_ANIM_PATH("pg_gakkistart"), MM_ANIM_PATH("pg_gakkiplay"),
    MM_ANIM_PATH("cl_setmask"),     MM_ANIM_PATH("pg_wait"),       MM_ANIM_PATH("pg_maskoffstart"),
};

static const char* const sGoronLimbDL[PLAYER_LIMB_MAX] = {
    NULL,
    NULL,
    OBJ_GORON "gLinkGoronWaistDL",
    NULL,
    OBJ_GORON "gLinkGoronRightThighDL",
    OBJ_GORON "gLinkGoronRightShinDL",
    OBJ_GORON "gLinkGoronRightFootDL",
    OBJ_GORON "gLinkGoronLeftThighDL",
    OBJ_GORON "gLinkGoronLeftShinDL",
    OBJ_GORON "gLinkGoronLeftFootDL",
    NULL,
    OBJ_GORON "gLinkGoronHeadDL",
    OBJ_GORON "gLinkGoronHatDL",
    OBJ_GORON "gLinkGoronCollarDL",
    OBJ_GORON "gLinkGoronLeftShoulderDL",
    OBJ_GORON "gLinkGoronLeftForearmDL",
    OBJ_GORON "gLinkGoronLeftHandOpenDL",
    OBJ_GORON "gLinkGoronRightShoulderDL",
    OBJ_GORON "gLinkGoronRightForearmDL",
    OBJ_GORON "gLinkGoronRightHandOpenDL",
    NULL,
    OBJ_GORON "gLinkGoronTorsoDL",
};
static const ALIGN_ASSET(2) char sLeftFistDL[] = OBJ_GORON "gLinkGoronLeftHandClosedDL";
static const ALIGN_ASSET(2) char sRightFistDL[] = OBJ_GORON "gLinkGoronRightHandClosedDL";

// Joint translations of gLinkGoronSkel, read from mm.o2r; same limb order as OoT Link.
static const Vec3f sGoronJointPos[PLAYER_LIMB_MAX] = {
    { 0, 0, 0 },        { 0, 0, 0 },     { 0, 0, 0 },           { 945, 0, 0 },  { -835, 218, -799 },
    { 1156, 0, 0 },     { 1056, 5, 11 }, { -835, 218, 799 },    { 1154, 0, 0 }, { 1057, 6, 3 },
    { 0, 21, -7 },      { 2679, 56, 0 }, { -831, -998, 0 },     { 0, 0, 0 },    { 2385, -476, 1200 },
    { 2019, 0, 0 },     { 1605, 0, 0 },  { 2385, -476, -1200 }, { 2021, 0, 0 }, { 1605, 0, 0 },
    { 978, -692, 342 }, { 0, 0, 0 },
};

static const ALIGN_ASSET(2) char sEyesOpenTex[] = OBJ_GORON "gLinkGoronEyesOpenTex";
static const ALIGN_ASSET(2) char sEyesHalfTex[] = OBJ_GORON "gLinkGoronEyesHalfTex";
static const ALIGN_ASSET(2) char sEyesClosedTex[] = OBJ_GORON "gLinkGoronEyesClosedTex";
static const ALIGN_ASSET(2) char sEyesSurprisedTex[] = OBJ_GORON "gLinkGoronEyesSurprisedTex";

// MM's Goron punch collider anchors (z_player_lib.c D_801C09B8/D_801C0970 for the fists and
// D_801C0A90/D_801C0A6C for the butt slam), in the space of the limb that throws the blow.
static Vec3f sFistTips[3] = { { 0, 750, 750 }, { 1500, 1500, 1500 }, { -2500, -2000, -3000 } };
static Vec3f sFistBases[3] = { { 0, 400, 0 }, { 0, 1400, -1000 }, { 0, -400, 1000 } };
static Vec3f sButtTips[3] = { { -400, 1800, 0 }, { 5000, 8000, 4000 }, { 5000, -500, -4000 } };
static Vec3f sButtBases[3] = { { -400, 800, 0 }, { -5000, -500, -4000 }, { -5000, 8000, 4000 } };
static const f32 sPunchHitEnd[PUNCH_STEPS] = { 8.0f, 18.0f, 14.0f };
static const s32 sPunchLimb[PUNCH_STEPS] = { PLAYER_LIMB_L_HAND, PLAYER_LIMB_R_HAND, PLAYER_LIMB_WAIST };
static const u8 kPunchEffectAlpha[] = { 100, 200, 255, 255, 255, 200, 100 };

static const char* const sRequiredHooks[] = {
    "OnPlayerActionHandler", "OnPlayerResolveLimbDraw",
    "OnPlayerPostLimbDraw",  "OnPlayerResolveFaceTextures",
    "OnPlayerResolveHeight", "OnPlayerFilterInput",
    "OnActorPlaySfx",        "OnOcarinaNote",
    "OnOcarinaPlaybackNote", "OnSceneInit",
};
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

typedef enum {
    GORON_MOVE_NONE,
    GORON_MOVE_PUNCH,
    GORON_MOVE_PUNCH_END,
    GORON_MOVE_RECOIL,
    GORON_MOVE_CURL,
    GORON_MOVE_ROLL,
    GORON_MOVE_POUND_JUMP,
    GORON_MOVE_POUND_LAND,
    GORON_MOVE_UNCURL,
    GORON_MOVE_GUARD,
    GORON_MOVE_SINK,
} GoronMove;

typedef enum {
    DRUM_IDLE,
    DRUM_RAISING,
    DRUM_PLAYING,
    DRUM_LOWERING,
} DrumPhase;

void Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void func_80839FFC(Player* player, PlayState* play);
s32 func_80842DF4(PlayState* play, Player* player);
void func_80837918(Player* player, s32 quadIndex, u32 dmgFlags);
int Player_IsZTargeting(Player* player);
void Player_ZeroSpeedXZ(Player* player);
void Player_RequestRumble(Player* player, s32 sourceStrength, s32 duration, s32 decreaseRate, s32 distSq);
GetItemEntry ItemTable_RetrieveEntry(s16 modIndex, s16 getItemID);

static const SOHModApi* sApi;
static MmSfxPlayFunc sPlayMmSfx;
static MmSfxStopFunc sStopMmSfx;

static LinkAnimationHeader sMmAnims[ANIM_COUNT];
static LinkAnimationHeader* sAnims[ANIM_COUNT];
static s16* sAnimFrames[ANIM_COUNT];
static s16* sAnimRootZ[ANIM_COUNT];

static u8 sMove;
static s32 sPunchStep;
static bool sPunchQueued;
static s32 sPunchFrames;
static s32 sAnimSlot;
static s32 sRootMotionFrame;

static s16 sSpinRate;
static f32 sBallSpeed;
static s16 sHomeYaw;
static f32 sBounce;
static s16 sChargeLevel;
static s16 sSpikes;
static s16 sNoInputTimer;
static s16 sWallBounceTimer;
static s16 sPoundTimer;
static bool sPoundStruck;
static f32 sSquash;
static f32 sSquashVelocity;
static f32 sColorLerp;
static s16 sRollSfxCounter;
static s16 sMagicDrainTimer;
static u32 sRollFrames;
static f32 sStandingShadow;
static s16 sStandingRadius;

static s16 sGuardYaw;
static ColliderCylinder sGuardCollider;
static bool sGuardColliderReady;
static SkelAnime sShieldingSkel;
static Vec3s sShieldingJoints[SHIELDING_JOINTS];
static Vec3s sShieldingMorph[SHIELDING_JOINTS];
static bool sShieldingReady;

static s16 sSinkTimer;
static bool sSinkCurled;

static u8 sDrumPhase;
static bool sIsOcarinaOut;
static bool sWasNoteStruck;
static u8 sLastNote = OCARINA_NOTE_NONE;
// Set on the game thread while the drums are out; the note hook runs on the audio thread and only reads it.
static MmOcarinaNoteFunc sDrumVoice;
static f32 sDrumBend = 1.0f;

static u8 sScaledDamage;

static ColliderCylinderInit sGuardInit = {
    { COLTYPE_METAL, AT_NONE, AC_ON | AC_HARD | AC_TYPE_ENEMY, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_NONE },
    { GUARD_RADIUS, GUARD_HEIGHT, 0, { 0, 0, 0 } },
};

static bool IsGoron(void) {
    return sApi != NULL && sApi->IsFormActive(GORON_FORM_KEY);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BG_ON_GROUND) != 0;
}

static bool CanAct(Player* player) {
    return !(player->stateFlags1 &
             (PLAYER_STATE1_DEAD | PLAYER_STATE1_LOADING | PLAYER_STATE1_TALKING | PLAYER_STATE1_GETTING_ITEM |
              PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_IN_WATER |
              PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_IN_ITEM_CS |
              PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_DAMAGED | PLAYER_STATE1_CLIMBING_LADDER |
              PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_HANGING_OFF_LEDGE));
}

static bool IsBallShape(void) {
    return sMove == GORON_MOVE_ROLL || sMove == GORON_MOVE_POUND_JUMP || sMove == GORON_MOVE_POUND_LAND;
}

static bool IsRolling(void) {
    return sMove == GORON_MOVE_ROLL || sMove == GORON_MOVE_POUND_JUMP || sMove == GORON_MOVE_POUND_LAND;
}

static bool IsPunchLive(Player* player) {
    return sMove == GORON_MOVE_PUNCH && player->meleeWeaponState != 0;
}

// ---- sound ----

// Looked up on use rather than in ModInit: mm_assets may load after this mod.
static MmSfxPlayFunc GetMmSfxPlayer(void) {
    if (sPlayMmSfx == NULL && SOH_MOD_API_HAS(sApi, FindService)) {
        sPlayMmSfx = (MmSfxPlayFunc)sApi->FindService(MM_SFX_PLAY_SERVICE);
    }
    return sPlayMmSfx;
}

static void PlayMmSfx(u16 mmSfx, u16 fallback, Vec3f* pos) {
    MmSfxPlayFunc play = GetMmSfxPlayer();

    if (play == NULL || !play(mmSfx, pos)) {
        Audio_PlaySoundGeneral(fallback, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultReverb);
    }
}

static void RefreshMmSfx(u16 mmSfx, Vec3f* pos) {
    MmSfxPlayFunc play = GetMmSfxPlayer();

    if (play != NULL) {
        play(mmSfx, pos);
    }
}

static void StopMmSfx(u16 mmSfx) {
    if (sStopMmSfx == NULL && SOH_MOD_API_HAS(sApi, FindService)) {
        sStopMmSfx = (MmSfxStopFunc)sApi->FindService(MM_SFX_STOP_SERVICE);
    }
    if (sStopMmSfx != NULL) {
        sStopMmSfx(mmSfx);
    }
}

// The loops fade on their own once nobody asks for them; the rolling thuds (one per floor type) are cut here so
// uncurling does not leave the last one ringing.
static void StopRollSfx(void) {
    StopMmSfx(MM_GORON_BALL_CHARGE_LOOP);
    StopMmSfx(MM_GORON_SLIP_LOOP);
    StopMmSfx(MM_GORON_ROLLING_REFLECTION);
    for (u16 floor = 0; floor < MM_FLOOR_SFX_COUNT; floor++) {
        StopMmSfx(MM_GORON_ROLL + floor);
        StopMmSfx(MM_GORON_CHG_ROLL + floor);
    }
}

static bool IsOnIce(Player* player) {
    return player->floorSfxOffset == (NA_SE_PL_WALK_ICE - SFX_FLAG);
}

// ---- animations ----

// MM clips are read with Link's root pinned to the base translation; the Z channel is kept aside so
// the punches can still move the body the way their root motion does in MM.
static void LoadMmAnim(s32 slot) {
    LinkAnimationHeader* source = ResourceMgr_LoadPlayerAnimAsHeader(sAnimPaths[slot]);

    if (source == NULL || source->common.frameCount <= 0) {
        sAnims[slot] = NULL;
        return;
    }
    s32 frameCount = source->common.frameCount;
    size_t values = (size_t)frameCount * MM_ANIM_VALUES_PER_FRAME;
    s16* frames = (s16*)malloc(values * sizeof(s16));
    s16* rootZ = (s16*)malloc((size_t)frameCount * sizeof(s16));
    if (frames == NULL || rootZ == NULL) {
        free(frames);
        free(rootZ);
        sAnims[slot] = NULL;
        return;
    }
    memcpy(frames, source->segment, values * sizeof(s16));
    for (s32 frame = 0; frame < frameCount; frame++) {
        s16* root = &frames[frame * MM_ANIM_VALUES_PER_FRAME];
        rootZ[frame] = root[2];
        root[0] = MM_ANIM_BASE_TRANSL_X;
        root[2] = 0;
    }
    free(sAnimFrames[slot]);
    free(sAnimRootZ[slot]);
    sAnimFrames[slot] = frames;
    sAnimRootZ[slot] = rootZ;
    sMmAnims[slot].common.frameCount = (s16)frameCount;
    sMmAnims[slot].segment = frames;
    sAnims[slot] = &sMmAnims[slot];
}

static void LoadAnims(void) {
    for (s32 slot = 0; slot < ANIM_COUNT; slot++) {
        LoadMmAnim(slot);
    }
}

static void PlayAnim(PlayState* play, Player* player, s32 slot, f32 speed, u8 mode, f32 morph) {
    LinkAnimationHeader* anim = sAnims[slot];

    if (anim == NULL) {
        return;
    }
    f32 last = Animation_GetLastFrame(anim);
    f32 start = speed >= 0.0f ? 0.0f : last;
    f32 end = speed >= 0.0f ? last : 0.0f;
    LinkAnimation_Change(play, &player->skelAnime, anim, speed, start, end, mode, morph);
    sAnimSlot = slot;
    sRootMotionFrame = (s32)start;
}

// Moves the body by the clip's root Z delta, which LoadMmAnim flattened out of the pose.
static void ApplyRootMotion(Player* player) {
    const s16* rootZ = sAnimRootZ[sAnimSlot];

    if (rootZ == NULL || sAnims[sAnimSlot] == NULL) {
        player->linearVelocity = 0.0f;
        return;
    }
    s32 last = sAnims[sAnimSlot]->common.frameCount - 1;
    s32 frame = CLAMP((s32)player->skelAnime.curFrame, 0, last);
    f32 delta = (f32)(rootZ[frame] - rootZ[CLAMP(sRootMotionFrame, 0, last)]);
    sRootMotionFrame = frame;
    player->linearVelocity = delta * player->actor.scale.z * GORON_ANIM_SCALE;
    player->yaw = player->actor.shape.rot.y;
}

// ---- body ----

static const char* HandDL(Player* player, int32_t limbIndex) {
    bool isFist = sMove == GORON_MOVE_PUNCH || sMove == GORON_MOVE_PUNCH_END;

    if (limbIndex == PLAYER_LIMB_L_HAND) {
        return isFist || player->leftHandType == PLAYER_MODELTYPE_LH_CLOSED ? sLeftFistDL
                                                                            : sGoronLimbDL[PLAYER_LIMB_L_HAND];
    }
    return isFist || player->rightHandType == PLAYER_MODELTYPE_RH_CLOSED ? sRightFistDL
                                                                         : sGoronLimbDL[PLAYER_LIMB_R_HAND];
}

static void ResolveGoronBody(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (!IsGoron() || limbIndex <= PLAYER_LIMB_NONE || limbIndex >= PLAYER_LIMB_MAX) {
        return;
    }
    if (limbIndex != PLAYER_LIMB_ROOT && pos != NULL) {
        *pos = sGoronJointPos[limbIndex];
    }
    // The ball and the guard are drawn in place of the body, never over it.
    if (IsBallShape() || sMove == GORON_MOVE_GUARD) {
        *dList = NULL;
        return;
    }
    const char* path = sGoronLimbDL[limbIndex];
    if (limbIndex == PLAYER_LIMB_L_HAND || limbIndex == PLAYER_LIMB_R_HAND) {
        path = HandDL(player, limbIndex);
    }
    if (path != NULL) {
        *dList = ResourceMgr_LoadGfxByName(path);
    } else if (limbIndex == PLAYER_LIMB_SHEATH) {
        *dList = NULL;
    }
}

static void ResolveGoronFace(const char** eyes, const char** mouth) {
    (void)mouth;
    if (!IsGoron() || *eyes == NULL) {
        return;
    }
    if (strstr(*eyes, "EyesHalf") != NULL) {
        *eyes = sEyesHalfTex;
    } else if (strstr(*eyes, "EyesClosed") != NULL) {
        *eyes = sEyesClosedTex;
    } else if (strstr(*eyes, "EyesShock") != NULL) {
        *eyes = sEyesSurprisedTex;
    } else {
        *eyes = sEyesOpenTex;
    }
}

static void ResolveGoronHeight(Player* player, float* height) {
    if (IsGoron() && IsBallShape()) {
        *height = GORON_CURLED_HEIGHT;
    }
}

static void ResolveGoronStrength(bool* should, va_list args) {
    int32_t* strength = va_arg(args, int32_t*);

    if (!IsGoron()) {
        return;
    }
    *strength = PLAYER_STR_SILVER_G;
    *should = false;
}

// Runs before Player_UpdateCommon consumes the hit, after CollisionCheck_Damage wrote it.
static void ScaleIncomingDamage(Player* player, Input* input) {
    (void)input;
    u8 damage = player->actor.colChkInfo.damage;

    if (!IsGoron() || damage == 0) {
        sScaledDamage = 0;
        return;
    }
    if (damage == sScaledDamage || !(player->cylinder.base.acFlags & AC_HIT) ||
        player->cylinder.info.acHitInfo == NULL) {
        return;
    }
    bool isFire = player->cylinder.info.acHitInfo->toucher.effect == 1;
    s32 scaled = isFire ? (damage * 3 + 1) / 2 : (damage * 3 + 2) / 4;
    sScaledDamage = (u8)CLAMP(scaled, 1, 0xFF);
    player->actor.colChkInfo.damage = sScaledDamage;
}

// ---- punches ----

static void EndMove(PlayState* play, Player* player) {
    player->meleeWeaponState = 0;
    sMove = GORON_MOVE_NONE;
    sPunchFrames = 0;
    player->linearVelocity = 0.0f;
    func_80839FFC(player, play);
}

static void StartPunchStep(PlayState* play, Player* player, s32 step) {
    sPunchStep = step;
    sPunchQueued = false;
    player->meleeWeaponState = 0;
    PlayAnim(play, player, ANIM_PUNCH_A + step, 1.0f, ANIMMODE_ONCE, -3.0f);
    PlayMmSfx(MM_GORON_PUNCH_SWING, NA_SE_IT_SWORD_SWING_HARD, &player->actor.projectedPos);
    RefreshMmSfx(step == PUNCH_STEPS - 1 ? MM_VO_GORON_SWORD_L : MM_VO_GORON_SWORD_N, &player->actor.projectedPos);
}

static void StartPunchRecovery(PlayState* play, Player* player) {
    s32 slot = (Player_IsZTargeting(player) ? ANIM_PUNCH_A_END_LOCKED : ANIM_PUNCH_A_END) + sPunchStep;

    player->meleeWeaponState = 0;
    sMove = GORON_MOVE_PUNCH_END;
    if (sAnims[slot] == NULL) {
        EndMove(play, player);
        return;
    }
    PlayAnim(play, player, slot, 1.0f, ANIMMODE_ONCE, -3.0f);
}

static void ShakeGround(PlayState* play, s16 countdown) {
    s16 quake = Quake_Add(GET_ACTIVE_CAM(play), 3);

    Quake_SetSpeed(quake, 27767);
    Quake_SetQuakeValues(quake, 7, 0, 0, 0);
    Quake_SetCountdown(quake, countdown);
}

static void StrikeGroundWithButt(PlayState* play, Player* player) {
    if (!IsGrounded(player)) {
        return;
    }
    PlayMmSfx(MM_GORON_PUNCH, NA_SE_IT_HAMMER_HIT, &player->actor.projectedPos);
    ShakeGround(play, 10);
    Actor_SpawnFloorDustRing(play, &player->actor, &player->actor.world.pos, 30.0f, 6, 6.0f, 300, 10, 1);
}

// A blocked or walled punch knocks the Goron back like a hammer that hits stone.
static void StartRecoil(PlayState* play, Player* player) {
    player->meleeWeaponState = 0;
    player->meleeWeaponQuads[0].base.atFlags &= ~(AT_ON | AT_BOUNCED);
    player->meleeWeaponQuads[1].base.atFlags &= ~(AT_ON | AT_BOUNCED);
    sMove = GORON_MOVE_RECOIL;
    player->linearVelocity = PUNCH_RECOIL_SPEED;
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_HAMMER_HIT);
    ShakeGround(play, 20);
    PlayAnim(play, player, ANIM_PUNCH_A_END + sPunchStep, 1.0f, ANIMMODE_ONCE, -3.0f);
}

static void GoronPunchAction(Player* player, PlayState* play) {
    Input* input = &play->state.input[0];

    if (sMove == GORON_MOVE_RECOIL) {
        Math_StepToF(&player->linearVelocity, 0.0f, 2.0f);
        if (LinkAnimation_Update(play, &player->skelAnime)) {
            EndMove(play, player);
        }
        return;
    }
    if (sMove == GORON_MOVE_PUNCH &&
        ((player->meleeWeaponQuads[0].base.atFlags | player->meleeWeaponQuads[1].base.atFlags) & AT_BOUNCED)) {
        StartRecoil(play, player);
        return;
    }
    if (func_80842DF4(play, player)) {
        sMove = GORON_MOVE_NONE;
        player->meleeWeaponState = 0;
        return;
    }
    bool finished = LinkAnimation_Update(play, &player->skelAnime);
    ApplyRootMotion(player);

    if (sMove == GORON_MOVE_PUNCH_END) {
        if (finished) {
            EndMove(play, player);
        }
        return;
    }
    f32 frame = player->skelAnime.curFrame;
    bool isHitting = frame >= PUNCH_HIT_START && frame <= sPunchHitEnd[sPunchStep];
    player->meleeWeaponState = isHitting ? 1 : 0;
    sPunchFrames = isHitting ? sPunchFrames + 1 : 0;
    if (sPunchStep == PUNCH_STEPS - 1 && LinkAnimation_OnFrame(&player->skelAnime, BUTT_IMPACT_FRAME)) {
        StrikeGroundWithButt(play, player);
    }
    if (frame >= PUNCH_HIT_START && CHECK_BTN_ALL(input->press.button, BTN_B)) {
        sPunchQueued = true;
    }
    if (!finished) {
        return;
    }
    if (sPunchQueued && sPunchStep + 1 < PUNCH_STEPS && sAnims[ANIM_PUNCH_A + sPunchStep + 1] != NULL) {
        StartPunchStep(play, player, sPunchStep + 1);
        return;
    }
    StartPunchRecovery(play, player);
}

static bool IsGoronBusy(Player* player);

static int32_t StartPunch(PlayState* play, Player* player) {
    // A worn mask that dances or marches on B blocks it for the player, and the punch gives way.
    if (!CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B) || (PlayerInput_GetBlockedButtons() & BTN_B)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    if (sAnims[ANIM_PUNCH_A] == NULL || IsGoronBusy(player) || !CanAct(player) || !IsGrounded(player)) {
        return SOH_FORM_ACTION_BLOCKED;
    }
    sMove = GORON_MOVE_PUNCH;
    sPunchFrames = 0;
    func_80837918(player, 0, PUNCH_DMG_FLAGS);
    func_80837918(player, 1, PUNCH_DMG_FLAGS);
    Player_SetupAction(play, player, GoronPunchAction, 0);
    StartPunchStep(play, player, 0);
    return SOH_FORM_ACTION_STARTED;
}

// func_8012669C from MM: point 0 feeds meleeWeaponInfo[0], which func_80842DF4 walks for walls, and points 1-2
// are the two attack quads.
static void PlaceStrike(PlayState* play, Player* player, Vec3f* tips, Vec3f* bases) {
    Vec3f tip;
    Vec3f base;

    Matrix_MultVec3f(&tips[0], &tip);
    Matrix_MultVec3f(&bases[0], &base);
    func_80090480(play, NULL, &player->meleeWeaponInfo[0], &tip, &base);
    for (s32 quad = 0; quad < 2; quad++) {
        ColliderQuad* collider = &player->meleeWeaponQuads[quad];

        collider->info.toucher.dmgFlags = PUNCH_DMG_FLAGS;
        collider->info.toucher.damage = PUNCH_DAMAGE;
        collider->info.toucherFlags = TOUCH_ON | TOUCH_NEAREST;
        collider->base.atFlags |= AT_ON;
        Matrix_MultVec3f(&tips[quad + 1], &tip);
        Matrix_MultVec3f(&bases[quad + 1], &base);
        func_80090480(play, collider, &player->meleeWeaponInfo[quad + 1], &tip, &base);
    }
}

static void DrawPunchEffect(PlayState* play) {
    if (sPunchFrames < 1 || sPunchFrames > (s32)ARRAY_COUNT(kPunchEffectAlpha)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 0, 0, kPunchEffectAlpha[sPunchFrames - 1]);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sPunchEffectDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- roll ----

static void SetRollAttack(Player* player, u32 dmgFlags, u8 damage, s16 radius) {
    player->cylinder.base.atFlags = AT_ON | AT_TYPE_PLAYER;
    player->cylinder.base.ocFlags1 = radius > ROLL_RADIUS + 5 ? OC1_NONE : OC1_ON | OC1_TYPE_ALL;
    player->cylinder.info.elemType = ELEMTYPE_UNK2;
    player->cylinder.info.toucherFlags = TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL;
    player->cylinder.info.toucher.dmgFlags = dmgFlags;
    player->cylinder.info.toucher.damage = damage;
    player->cylinder.dim.radius = radius;
    // MM leaves the slam untouchable: its cylinder stops receiving hits for as long as it is out.
    if (radius > ROLL_RADIUS + 5) {
        player->cylinder.base.acFlags = AC_NONE;
    }
}

static void ClearRollAttack(Player* player) {
    player->cylinder.base.atFlags = AT_NONE;
    player->cylinder.base.ocFlags1 = OC1_ON | OC1_TYPE_ALL;
    player->cylinder.base.acFlags = AC_ON | AC_TYPE_ENEMY;
    player->cylinder.info.toucherFlags = TOUCH_NONE;
    player->cylinder.info.toucher.dmgFlags = 0;
    if (sStandingRadius > 0) {
        player->cylinder.dim.radius = sStandingRadius;
    }
}

static void ResetBall(void) {
    sSpinRate = 0;
    sBallSpeed = 0.0f;
    sBounce = 0.0f;
    sChargeLevel = BALL_CHARGE_IDLE;
    sSpikes = 0;
    sNoInputTimer = 0;
    sWallBounceTimer = 0;
    sPoundTimer = 0;
    sPoundStruck = false;
    sSquash = 0.0f;
    sSquashVelocity = 0.0f;
    sColorLerp = 0.0f;
    sRollSfxCounter = 0;
    sRollFrames = 0;
}

static void LeaveBall(Player* player) {
    ClearRollAttack(player);
    StopRollSfx();
    player->actor.gravity = GRAVITY_NORMAL;
    player->actor.shape.rot.x = 0;
    player->actor.shape.rot.z = 0;
    if (sStandingShadow > 0.0f) {
        player->actor.shape.shadowScale = sStandingShadow;
    }
    player->stateFlags2 &= ~(PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET | PLAYER_STATE2_DISABLE_ROTATION_ALWAYS);
    player->stateFlags1 &= ~PLAYER_STATE1_JUMPING;
    player->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
    player->actor.bgCheckFlags &= ~BG_LEAVE_LEDGES;
}

// MM's BGCHECKFLAG_PLAYER_800: OoT's func_8002E234 keeps an actor "on the ground" over drops of up to 11 units,
// which glues the ball to ramps and lips. This undoes that hug the same frame, as NEI's patch there did.
static void LaunchOffLedges(Player* player) {
    bool isHugging = (player->actor.bgCheckFlags & BG_ON_GROUND) &&
                     player->actor.world.pos.y > player->actor.floorHeight + BALL_HUG_TOLERANCE;

    if (!IsRolling() || !isHugging) {
        return;
    }
    player->actor.bgCheckFlags &= ~BG_ON_GROUND;
    player->actor.bgCheckFlags |= BG_LEFT_GROUND;
    if (player->actor.velocity.y < 0.0f) {
        player->actor.velocity.y = 0.0f;
    }
}

static void GoronRollAction(Player* player, PlayState* play);
static void GoronCurlAction(Player* player, PlayState* play);

static void EnterBall(PlayState* play, Player* player) {
    ResetBall();
    sMove = GORON_MOVE_ROLL;
    sHomeYaw = player->yaw;
    sBallSpeed = player->linearVelocity;
    sSpinRate = (s16)(player->linearVelocity * 500.0f);
    sStandingShadow = player->actor.shape.shadowScale;
    sStandingRadius = player->cylinder.dim.radius;
    player->actor.shape.shadowScale = BALL_SHADOW;
    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET | PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    Player_SetupAction(play, player, GoronRollAction, 0);
}

static void StartUncurl(PlayState* play, Player* player) {
    LeaveBall(player);
    player->actor.world.pos = player->actor.prevPos;
    PlayMmSfx(MM_BALL_TO_GORON, NA_SE_PL_BODY_HIT, &player->actor.projectedPos);
    sMove = GORON_MOVE_UNCURL;
    // The roll action reads any non-ROLL move as the pound, which ends by rolling again.
    Player_SetupAction(play, player, GoronCurlAction, 0);
    PlayAnim(play, player, ANIM_CURL, -CURL_PLAY_SPEED, ANIMMODE_ONCE, 0.0f);
}

static void GoronCurlAction(Player* player, PlayState* play) {
    bool finished = LinkAnimation_Update(play, &player->skelAnime);

    Math_StepToF(&player->linearVelocity, 0.0f, 1.0f);
    if (!finished) {
        return;
    }
    if (sMove == GORON_MOVE_UNCURL) {
        EndMove(play, player);
        return;
    }
    EnterBall(play, player);
}

static int32_t StartCurl(PlayState* play, Player* player) {
    if (!CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    if (sAnims[ANIM_CURL] == NULL || IsGoronBusy(player) || !CanAct(player) || !IsGrounded(player)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    sMove = GORON_MOVE_CURL;
    Player_SetupAction(play, player, GoronCurlAction, 0);
    PlayAnim(play, player, ANIM_CURL, CURL_PLAY_SPEED, ANIMMODE_ONCE, -3.0f);
    PlayMmSfx(MM_GORON_TO_BALL, NA_SE_PL_CHANGE_ARMS, &player->actor.projectedPos);
    return SOH_FORM_ACTION_STARTED;
}

// Z-target's A also owns the side hop and back flip, which a Goron keeps; only the forward roll curls.
static int32_t HandleZTargetA(PlayState* play, Player* player) {
    const s8 direction = player->controlStickDirections[player->controlStickDataIndex];

    if (direction > PLAYER_STICK_DIR_FORWARD) {
        return SOH_FORM_ACTION_VANILLA;
    }
    return StartCurl(play, player);
}

static void StepSpinRate(s32 target) {
    s16 diff = (s16)(target - sSpinRate);
    s16 step = diff >= 0 ? (target >= 0 ? 0x7D0 : 0x4B0) : (target >= 0 ? 0x4B0 : 0x3E8);

    if (ABS(diff) <= step) {
        sSpinRate = (s16)target;
    } else {
        sSpinRate += diff > 0 ? step : -step;
    }
}

// Squash and stretch from MM's func_808577E0: the ball bulges while it spins up and settles after.
static void UpdateSquash(void) {
    f32 target = (f32)ABS(sSpinRate) * 0.00004f;

    sSquashVelocity += sSquash < target ? 0.08f : -0.07f;
    sSquashVelocity = CLAMP(sSquashVelocity, -0.2f, 0.14f);
    if (fabsf(sSquashVelocity) < 0.12f) {
        if (Math_StepUntilF(&sSquash, target, sSquashVelocity)) {
            sSquashVelocity = 0.0f;
        }
    } else {
        sSquash = CLAMP(sSquash + sSquashVelocity, -0.7f, 0.3f);
    }
}

static void ReadRollStick(PlayState* play, f32* speedTarget, s16* yawTarget) {
    Input* input = &play->state.input[0];
    f32 magnitude = sqrtf(SQ(input->rel.stick_x) + SQ(input->rel.stick_y));

    if (sNoInputTimer > 0 || magnitude <= 10.0f) {
        return;
    }
    *yawTarget = Math_Atan2S(input->rel.stick_y, -input->rel.stick_x) + Camera_GetInputDirYaw(GET_ACTIVE_CAM(play));
    *speedTarget = (magnitude / 60.0f) * BALL_STICK_SPEED;
}

static void BounceOffWall(PlayState* play, Player* player) {
    if (!(player->actor.bgCheckFlags & BG_TOUCHING_WALL) || sBallSpeed < BALL_BOUNCE_SPEED) {
        return;
    }
    if (player->doorType != PLAYER_DOORTYPE_NONE) {
        player->linearVelocity *= 0.1f;
        sBallSpeed *= 0.1f;
        if (sSpikes > 0) {
            sSpikes = 0;
            sChargeLevel = 3;
        }
        PlayMmSfx(MM_GORON_ROLLING_REFLECTION, NA_SE_PL_BODY_HIT, &player->actor.projectedPos);
        return;
    }
    // A breakable wall the ball is already hitting keeps it, so its own AC handler gets to shatter it.
    if (player->actor.wallBgId != BGCHECK_SCENE) {
        DynaPolyActor* dyna = DynaPoly_GetActor(&play->colCtx, player->actor.wallBgId);
        if (dyna != NULL && (player->cylinder.base.atFlags & AT_HIT) && player->cylinder.base.at == &dyna->actor) {
            return;
        }
    }
    s16 wallAngle = player->actor.wallYaw + 0x8000;
    s16 relWallAngle = player->yaw - wallAngle;
    s16 bounceAngle = (relWallAngle >= 0 ? 1 : -1) * ((ABS(relWallAngle) + 0x100) & ~0x1FF);

    player->yaw += (s16)(0x8000 - (bounceAngle * 2));
    sHomeYaw = player->yaw;
    player->actor.shape.rot.y = player->yaw;
    player->actor.world.rot.y = player->yaw;
    sBounce += sBallSpeed * 0.05f;
    sWallBounceTimer = 4;
    PlayMmSfx(MM_GORON_ROLLING_REFLECTION, NA_SE_PL_BODY_HIT, &player->actor.projectedPos);
}

// Magic is drained raw, as in NEI: the spikes never open a magicState, so there is nothing to Magic_Reset.
static void UpdateSpikes(PlayState* play, Player* player, f32* speedTarget) {
    Input* input = &play->state.input[0];

    if (sSpikes <= 0) {
        return;
    }
    *speedTarget = BALL_MAX_SPEED;
    Math_StepToS(&sChargeLevel, BALL_CHARGE_IDLE, 1);
    if (--sMagicDrainTimer <= 0) {
        if (gSaveContext.magic > 0) {
            gSaveContext.magic--;
        }
        sMagicDrainTimer = BALL_DRAIN_INTERVAL;
    }
    bool drop = !CHECK_BTN_ALL(input->cur.button, BTN_A) || gSaveContext.magic <= 0 ||
                (sChargeLevel == BALL_CHARGE_IDLE && sBallSpeed < BALL_BOUNCE_SPEED);
    if (drop) {
        if (Math_StepToS(&sSpikes, 0, 1)) {
            PlayMmSfx(MM_GORON_BALL_CHARGE_FAILED, NA_SE_PL_BODY_HIT, &player->actor.projectedPos);
        }
        sChargeLevel = BALL_CHARGE_IDLE;
    } else if (sSpikes < BALL_SPIKE_FULL) {
        sSpikes++;
    }
}

static void ChargeSpikes(Player* player) {
    if (sSpikes != 0) {
        return;
    }
    sBounce = 0.0f;
    if (sChargeLevel >= BALL_CHARGE_READY) {
        if (gSaveContext.magic >= BALL_SPIKE_COST) {
            gSaveContext.magic -= BALL_SPIKE_COST;
        }
        sMagicDrainTimer = BALL_DRAIN_INTERVAL;
        sBallSpeed = BALL_MAX_SPEED;
        sSpikes = 1;
        StopMmSfx(MM_GORON_BALL_CHARGE_LOOP);
        PlayMmSfx(MM_GORON_BALL_CHARGE_DASH, NA_SE_PL_BODY_HIT, &player->actor.projectedPos);
        return;
    }
    if (gSaveContext.magicState == MAGIC_STATE_IDLE && gSaveContext.magic >= BALL_SPIKE_COST &&
        sSpinRate >= BALL_CHARGE_SPIN) {
        if (sChargeLevel < BALL_CHARGE_CAP) {
            sChargeLevel++;
        }
        RefreshMmSfx(MM_GORON_BALL_CHARGE_LOOP, &player->actor.projectedPos);
    } else {
        sChargeLevel = BALL_CHARGE_IDLE;
    }
}

static void StartPound(PlayState* play, Player* player) {
    player->actor.velocity.y = POUND_LIFT;
    player->linearVelocity = 0.0f;
    sSpinRate = MAX(sSpinRate, POUND_MIN_SPIN);
    sChargeLevel = 1;
    sSquashVelocity = 1.0f;
    sMove = GORON_MOVE_POUND_JUMP;
    player->actor.gravity = GRAVITY_NORMAL;
    player->stateFlags1 |= PLAYER_STATE1_JUMPING;
    PlayMmSfx(MM_GORON_BALLJUMP, NA_SE_PL_JUMP, &player->actor.projectedPos);
    RefreshMmSfx(MM_VO_GORON_SWORD_N, &player->actor.projectedPos);
}

static void LandPound(PlayState* play, Player* player) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };

    sMove = GORON_MOVE_POUND_LAND;
    sPoundStruck = false;
    player->actor.gravity = GRAVITY_NORMAL;
    player->stateFlags1 &= ~PLAYER_STATE1_JUMPING;
    // The same impact Link's hammer leaves: tektites flip and hammer-rusted switches respond.
    play->actorCtx.unk_02 = 4;
    ShakeGround(play, 20);
    PlayMmSfx(MM_GORON_PUNCH, NA_SE_IT_HAMMER_HIT, &player->actor.projectedPos);
    Player_RequestRumble(player, 255, 20, 150, 0);
    EffectSsBlast_SpawnWhiteShockwave(play, &player->actor.world.pos, &zero, &zero);
    Actor_SpawnFloorDustRing(play, &player->actor, &player->actor.world.pos, player->actor.shape.shadowScale * 1.5f, 4,
                             8.0f, 500, 10, 1);
    sBallSpeed = 0.0f;
    sSpinRate = 0;
}

static void UpdatePound(PlayState* play, Player* player) {
    if (sMove == GORON_MOVE_POUND_JUMP) {
        if (player->actor.velocity.y > 0.0f) {
            if (player->actor.velocity.y + player->actor.gravity < 0.0f) {
                player->actor.velocity.y = -player->actor.gravity;
            }
        } else {
            sPoundTimer = POUND_PAUSE;
            player->actor.gravity = player->actor.velocity.y > -1.0f ? GRAVITY_APEX : GRAVITY_SLAM;
        }
        if (IsGrounded(player) && player->actor.velocity.y <= 0.0f) {
            LandPound(play, player);
        }
        player->actor.shape.rot.x += sSpinRate;
        Math_ScaledStepToS(&player->actor.shape.rot.y, sHomeYaw, 0x7D0);
        return;
    }
    if (sPoundTimer > 0) {
        sPoundTimer--;
        player->linearVelocity = 0.0f;
        if (!sPoundStruck) {
            SetRollAttack(player, POUND_DMG_FLAGS, POUND_DAMAGE, POUND_RADIUS);
            CollisionCheck_SetAT(play, &play->colChkCtx, &player->cylinder.base);
            sPoundStruck = true;
        } else {
            ClearRollAttack(player);
        }
    } else {
        ClearRollAttack(player);
        sChargeLevel = BALL_CHARGE_IDLE;
        sMove = GORON_MOVE_ROLL;
        player->actor.gravity = GRAVITY_NORMAL;
    }
    player->actor.shape.rot.x += sSpinRate;
}

// MM's Audio_PlaySfx_AtPosWithSyncedFreqAndVolume (code_8019AF00.c): full pitch and volume from speed 6 up,
// and a step down of both for every unit below it.
static void PlayRollingSound(Player* player, bool isCharged) {
    static f32 sRollFreq;
    static f32 sRollVolume;
    static MmSfxPlayScaledFunc sPlayScaled;
    f32 shortfall = sBallSpeed >= 6.0f ? 0.0f : 6.0f - sBallSpeed;
    // Player_GetFloorSfx: MM's bank holds one rolling thud per floor type, right after the base one.
    u16 sfx = (isCharged ? MM_GORON_CHG_ROLL : MM_GORON_ROLL) + player->floorSfxOffset;

    sRollFreq = MAX(1.1f - shortfall * 0.0333f, 0.5f);
    sRollVolume = MAX(1.0f - shortfall * 0.0375f, 0.1f);
    if (sPlayScaled == NULL && SOH_MOD_API_HAS(sApi, FindService)) {
        sPlayScaled = (MmSfxPlayScaledFunc)sApi->FindService(MM_SFX_PLAY_SCALED_SERVICE);
    }
    if (sPlayScaled == NULL || !sPlayScaled(sfx, &player->actor.projectedPos, &sRollFreq, &sRollVolume)) {
        PlayMmSfx(sfx, NA_SE_PL_BODY_HIT, &player->actor.projectedPos);
    }
}

static void UpdateRollingSound(Player* player) {
    if (!IsGrounded(player)) {
        sRollSfxCounter = 0;
        return;
    }
    if (sSpinRate == 0) {
        s16 previous = sRollSfxCounter;
        s16 increment = (s16)(sBallSpeed * 800.0f);

        sRollSfxCounter += increment;
        if (increment != 0 && ((s32)(previous + increment) * (s32)previous) <= 0) {
            PlayRollingSound(player, false);
        }
        return;
    }
    s16 previousRotX = player->actor.shape.rot.x;
    Math_ScaledStepToS(&sRollSfxCounter, 0, ABS(sSpinRate));
    if (((s32)(sSpinRate + previousRotX) * (s32)previousRotX) <= 0) {
        PlayRollingSound(player, sSpikes > 0);
    }
}

static void SpawnSkidDust(PlayState* play, Player* player) {
    s32 skid = (s32)(((player->actor.velocity.z * Math_CosS(player->yaw)) +
                      (player->actor.velocity.x * Math_SinS(player->yaw))) *
                     800.0f);
    Color_RGBA8 prim = { 170, 130, 90, 255 };
    Color_RGBA8 env = { 100, 80, 60, 255 };

    skid = ABS(skid - sSpinRate);
    if (skid > 0x1770) {
        RefreshMmSfx(MM_GORON_SLIP_LOOP, &player->actor.projectedPos);
    }
    if (skid <= 0x7D0 || (sRollFrames % 2) != 0) {
        return;
    }
    if (IsOnIce(player)) {
        prim = (Color_RGBA8){ 220, 220, 240, 255 };
        env = (Color_RGBA8){ 180, 180, 200, 255 };
    } else if (player->floorSfxOffset == (NA_SE_PL_WALK_SAND - SFX_FLAG)) {
        prim = (Color_RGBA8){ 200, 170, 110, 255 };
        env = (Color_RGBA8){ 130, 100, 60, 255 };
    } else if (player->floorSfxOffset == (NA_SE_PL_WALK_GRASS - SFX_FLAG)) {
        prim = (Color_RGBA8){ 120, 160, 80, 255 };
        env = (Color_RGBA8){ 80, 120, 50, 255 };
    }
    Vec3f pos = { player->actor.world.pos.x + Rand_CenteredFloat(10.0f), player->actor.world.pos.y,
                  player->actor.world.pos.z + Rand_CenteredFloat(10.0f) };
    Vec3f velocity = { -Math_SinS(player->yaw) * sBallSpeed * 0.1f, 1.5f, -Math_CosS(player->yaw) * sBallSpeed * 0.1f };
    Vec3f accel = { 0.0f, 0.3f, 0.0f };
    s16 scale = (s16)MIN((skid >> 10) + 1, 200);
    func_8002829C(play, &pos, &velocity, &accel, &prim, &env, scale, 5);
}

static void TiltWithGround(PlayState* play, Player* player, s16 lean) {
    CollisionPoly* poly;
    s32 bgId;
    f32 sideX = Math_SinS(player->yaw + 0x4000) * 30.0f;
    f32 sideZ = Math_CosS(player->yaw + 0x4000) * 30.0f;
    Vec3f left = { player->actor.world.pos.x - sideX, player->actor.world.pos.y + 60.0f,
                   player->actor.world.pos.z - sideZ };
    Vec3f right = { player->actor.world.pos.x + sideX, player->actor.world.pos.y + 60.0f,
                    player->actor.world.pos.z + sideZ };
    f32 leftY = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &left);
    f32 rightY = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &right);
    s16 target = lean;

    if (leftY > BGCHECK_Y_MIN && rightY > BGCHECK_Y_MIN) {
        target += Math_Atan2S(60.0f, rightY - leftY);
    }
    Math_ScaledStepToS(&player->actor.shape.rot.z, target, 0x190);
}

// MM's Player_Action_96 grounded core: the ball keeps a trajectory (home yaw + speed) separate from
// a sideways drift, and steering only bends the trajectory.
static void RollOnGround(PlayState* play, Player* player, f32 speedTarget, s16 yawTarget, s32* spinFloor,
                         bool* reverseBrake) {
    s16 visualYaw = player->yaw;
    s16 offTrajectory = player->yaw - sHomeYaw;
    f32 alignment = Math_CosS(offTrajectory);
    f32 trajectorySpeed = (1.0f - sBounce) * sBallSpeed * alignment;
    s16 lean = 0;

    if (trajectorySpeed < 0.0f || (speedTarget == 0.0f && ABS(offTrajectory) > 0xFA0)) {
        trajectorySpeed = 0.0f;
    }
    Math_StepToF(&sBounce, 0.0f, fabsf(alignment) * 20.0f);
    *spinFloor = MAX((s32)(trajectorySpeed * 500.0f), 0);
    s32 demand = MAX((s32)(speedTarget * 400.0f) - *spinFloor, 0);

    f32 driftX = sBallSpeed * Math_SinS(player->yaw) - trajectorySpeed * Math_SinS(sHomeYaw);
    f32 driftZ = sBallSpeed * Math_CosS(player->yaw) - trajectorySpeed * Math_CosS(sHomeYaw);
    player->linearVelocity = trajectorySpeed;
    player->yaw = sHomeYaw;
    player->actor.world.rot.y = player->yaw;

    if (sSpikes == 0 && player->actor.floorPoly != NULL) {
        f32 slopedX = 0.6f * COLPOLY_GET_NORMAL(player->actor.floorPoly->normal.x) + driftX;
        f32 slopedZ = 0.6f * COLPOLY_GET_NORMAL(player->actor.floorPoly->normal.z) + driftZ;
        f32 slopedLength = sqrtf(SQ(slopedX) + SQ(slopedZ));

        if (slopedLength < sqrtf(SQ(driftX) + SQ(driftZ)) || slopedLength < 6.0f) {
            driftX = slopedX;
            driftZ = slopedZ;
        }
    }
    f32 driftLength = sqrtf(SQ(driftX) + SQ(driftZ));
    if (driftLength != 0.0f) {
        f32 shrink = MAX(driftLength - 0.3f, 0.0f) / driftLength;
        driftX *= shrink;
        driftZ *= shrink;
    }

    bool reversing = false;
    f32 appliedTarget = speedTarget;
    if (ABS((s16)(player->yaw - yawTarget)) > 0x6000) {
        if (Math_StepToF(&player->linearVelocity, 0.0f, sChargeLevel >= 5 ? 0.0f : 1.0f)) {
            appliedTarget = 0.0f;
        } else {
            reversing = true;
        }
    }
    if (reversing) {
        if (sSpikes == 0) {
            sChargeLevel = BALL_CHARGE_IDLE;
        }
        *reverseBrake = sChargeLevel == BALL_CHARGE_IDLE;
    } else {
        bool isSlippery = IsOnIce(player) || player->floorSfxOffset == (NA_SE_PL_WALK_SAND - SFX_FLAG) ||
                          player->floorSfxOffset == (NA_SE_PL_WALK_DIRT - SFX_FLAG);
        f32 accel = isSlippery && demand >= 0x7D0 ? 0.08f : 0.0003f * ABS(sSpinRate);
        f32 decel = MAX((Math_SinS(player->floorPitch) * 8.0f) + 0.6f, 0.0f);

        if (speedTarget != appliedTarget) {
            player->yaw = yawTarget;
        }
        Math_AsymStepToF(&player->linearVelocity, speedTarget, MAX(accel, 0.0f), decel);
        s16 turnRate = MAX((s16)(fabsf(player->linearVelocity) * 20.0f) + 300, 100);
        lean = (s16)((s16)(yawTarget - player->yaw) * -0.5f);
        sBounce += (f32)SQ(lean) * 8e-9f;
        Math_ScaledStepToS(&player->yaw, yawTarget, turnRate);
    }

    sHomeYaw = player->yaw;
    player->yaw = visualYaw;
    f32 moveX = Math_SinS(sHomeYaw) * player->linearVelocity + driftX;
    f32 moveZ = Math_CosS(sHomeYaw) * player->linearVelocity + driftZ;
    sBallSpeed = MIN(sqrtf(SQ(moveX) + SQ(moveZ)), BALL_MAX_SPEED);
    player->yaw = Math_Atan2S(moveZ, moveX);

    player->linearVelocity = sBallSpeed * Math_CosS(player->floorPitch);
    player->actor.velocity.y = sBallSpeed * Math_SinS(player->floorPitch);
    player->actor.world.rot.y = player->yaw;
    Math_AsymStepToF(&sColorLerp, sPoundTimer != 0 ? 1.0f : 0.0f, 0.8f, 0.05f);
    TiltWithGround(play, player, lean);
    UpdateRollingSound(player);
    SpawnSkidDust(play, player);
}

static void RollInAir(Player* player, s16 yawTarget) {
    Math_ScaledStepToS(&player->actor.shape.rot.z, 0, 0x190);
    sRollSfxCounter = 0;
    if (sSpikes > 0) {
        player->actor.gravity = -1.0f;
        Math_ScaledStepToS(&sHomeYaw, yawTarget, 0x190);
        sBallSpeed = sqrtf(SQ(player->linearVelocity) + SQ(player->actor.velocity.y)) *
                     (player->linearVelocity >= 0.0f ? 1.0f : -1.0f);
        sBallSpeed = MIN(sBallSpeed, BALL_MAX_SPEED);
    } else {
        sSquashVelocity += player->actor.velocity.y * 0.005f;
        sBallSpeed = player->linearVelocity;
    }
}

static void GoronRollAction(Player* player, PlayState* play) {
    Input* input = &play->state.input[0];

    sRollFrames++;
    player->actor.bgCheckFlags |= BG_LEAVE_LEDGES;
    // Without MIDAIR, func_8083AA10 reads the ball leaving the ground as Link stepping off an edge: it swaps in
    // the fall, the running jump or the ledge grab (MM's restricted handler list never lets that happen).
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
    if (player->actor.floorPoly != NULL &&
        SurfaceType_GetSlope(&play->colCtx, player->actor.floorPoly, player->actor.floorBgId) == 1) {
        player->pushedSpeed = 0.0f;
    }
    if (sMove != GORON_MOVE_ROLL) {
        UpdatePound(play, player);
        return;
    }
    if (sSpikes == 0 && !CHECK_BTN_ALL(input->cur.button, BTN_A)) {
        StartUncurl(play, player);
        return;
    }
    if (sNoInputTimer > 0) {
        sNoInputTimer--;
    }
    f32 speedTarget = 0.0f;
    s16 yawTarget = player->yaw;
    ReadRollStick(play, &speedTarget, &yawTarget);
    if (sWallBounceTimer > 0) {
        sWallBounceTimer--;
        yawTarget = player->yaw;
    }
    BounceOffWall(play, player);
    UpdateSpikes(play, player, &speedTarget);

    s32 spinFloor = 0;
    bool reverseBrake = false;
    if (IsGrounded(player)) {
        player->actor.gravity = GRAVITY_NORMAL;
        if (sSpikes == 0 && CHECK_BTN_ALL(input->press.button, BTN_B)) {
            StartPound(play, player);
            player->actor.shape.rot.x += sSpinRate;
            return;
        }
        ChargeSpikes(player);
        if (sSpikes > 0 && (ABS((s16)(player->yaw - sHomeYaw)) + ABS(player->floorPitch)) > BALL_STEEP_SLOPE) {
            sSpikes = 0;
            sChargeLevel = BALL_CHARGE_IDLE;
            sSpinRate = 0;
            sNoInputTimer = 0x14;
        }
        RollOnGround(play, player, speedTarget, yawTarget, &spinFloor, &reverseBrake);
    } else {
        RollInAir(player, yawTarget);
    }

    Math_ScaledStepToS(&player->actor.shape.rot.y, sHomeYaw, 0x7D0);
    StepSpinRate(reverseBrake ? -0xFA0 : MAX((s32)(speedTarget * 900.0f), spinFloor));
    player->actor.shape.rot.x += sSpinRate;
    UpdateSquash();

    if (sSpikes > 0 || sBallSpeed > 2.0f) {
        SetRollAttack(player, ROLL_DMG_FLAGS, ROLL_DAMAGE, ROLL_RADIUS);
        CollisionCheck_SetAT(play, &play->colChkCtx, &player->cylinder.base);
    } else {
        ClearRollAttack(player);
    }
}

static void DrawBall(PlayState* play, Player* player) {
    f32 bulge = sSquash + 1.0f;
    f32 pinch = 1.0f - (sSquash * 0.5f);
    u32 frames = play->gameplayFrames;

    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(player->actor.world.pos.x, player->actor.world.pos.y + BALL_DRAW_LIFT * player->actor.scale.y,
                     player->actor.world.pos.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(player->actor.shape.rot.y), MTXMODE_APPLY);
    Matrix_RotateZ(BINANG_TO_RAD(player->actor.shape.rot.z), MTXMODE_APPLY);
    Matrix_Scale(player->actor.scale.x * pinch * BALL_DRAW_SCALE, player->actor.scale.y * bulge * BALL_DRAW_SCALE,
                 player->actor.scale.z * MAX(bulge, pinch) * BALL_DRAW_SCALE, MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD(player->actor.shape.rot.x), MTXMODE_APPLY);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Gfx* scroll = Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 0x40, 0x40, 1, frames * 2, frames * 2, 0x40, 0x40);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)scroll);
    gDPSetEnvColor(POLY_OPA_DISP++, (u8)(255.0f - 175.0f * sColorLerp), (u8)(255.0f - 175.0f * sColorLerp),
                   (u8)(255.0f - 55.0f * sColorLerp), 255);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sCurledDL);

    if (sSpikes > 0) {
        if (sSpikes < 3) {
            f32 grow = sSpikes / 3.0f;
            Matrix_Scale(grow, grow, grow, MTXMODE_APPLY);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        }
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sSpikesDL);
    }
    if (sSpikes < 3 && sChargeLevel >= 5) {
        f32 glow = sSpikes != 0 ? 0.65f : (sChargeLevel - 4) * 0.02f;
        u8 alpha = sSpikes != 0 ? (u8)(0xFF - sSpikes * 0x55) : (u8)MIN(200.0f * glow, 200.0f);

        Matrix_Scale(1.0f, glow, glow, MTXMODE_APPLY);
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)scroll);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gDPSetEnvColor(POLY_XLU_DISP++, 155, 0, 0, alpha);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sChargeGlowDL);
        gDPSetEnvColor(POLY_XLU_DISP++, (frames % 2) == 0 ? 100 : 200, 0, 0, 255);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sChargeFlareDL);
    }
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- guard ----

static bool PrepareShieldingSkeleton(PlayState* play) {
    if (sShieldingReady) {
        return true;
    }
    SkeletonHeader* header = ResourceMgr_LoadSkeletonByName(sShieldingSkelPath, NULL);
    if (header == NULL || header->limbCount <= 0 || header->limbCount + 1 > SHIELDING_JOINTS) {
        return false;
    }
    SkelAnime_InitFlex(play, &sShieldingSkel, (FlexSkeletonHeader*)sShieldingSkelPath,
                       (AnimationHeader*)sShieldingAnimPath, sShieldingJoints, sShieldingMorph, header->limbCount + 1);
    sShieldingReady = true;
    return true;
}

static void SubmitGuard(PlayState* play, Player* player) {
    if (!sGuardColliderReady) {
        Collider_InitCylinder(play, &sGuardCollider);
        Collider_SetCylinder(play, &sGuardCollider, &player->actor, &sGuardInit);
        sGuardColliderReady = true;
    }
    if (sGuardCollider.base.acFlags & AC_HIT) {
        sGuardCollider.base.acFlags &= ~AC_HIT;
        Audio_PlayActorSound2(&player->actor, NA_SE_IT_SHIELD_BOUND);
    }
    // Link's own cylinder sits inside the shell: while it keeps its AC, every blocked hit still lands.
    player->cylinder.base.acFlags &= ~(AC_ON | AC_HIT);
    sGuardCollider.base.acFlags |= AC_ON;
    sGuardCollider.dim.pos.x = (s16)player->actor.world.pos.x;
    sGuardCollider.dim.pos.y = (s16)player->actor.world.pos.y;
    sGuardCollider.dim.pos.z = (s16)player->actor.world.pos.z;
    CollisionCheck_SetAC(play, &play->colChkCtx, &sGuardCollider.base);
}

static void GoronGuardAction(Player* player, PlayState* play) {
    Math_StepToF(&player->linearVelocity, 0.0f, 2.0f);
    player->yaw = sGuardYaw;
    player->actor.shape.rot.y = sGuardYaw;
    if (sShieldingReady) {
        SkelAnime_Update(&sShieldingSkel);
    }
    if (!CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_R) || !CanAct(player)) {
        player->cylinder.base.acFlags |= AC_ON;
        StopMmSfx(MM_GORON_SQUAT);
        PlayMmSfx(MM_BALL_TO_GORON, NA_SE_PL_BODY_HIT, &player->actor.projectedPos);
        EndMove(play, player);
    }
}

static int32_t StartGuard(PlayState* play, Player* player) {
    if (!CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_R)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    if (IsGoronBusy(player) || !CanAct(player) || !IsGrounded(player) || !PrepareShieldingSkeleton(play)) {
        return SOH_FORM_ACTION_BLOCKED;
    }
    sMove = GORON_MOVE_GUARD;
    sGuardYaw = player->actor.shape.rot.y;
    Animation_PlayLoop(&sShieldingSkel, (AnimationHeader*)sShieldingAnimPath);
    Player_SetupAction(play, player, GoronGuardAction, 0);
    PlayMmSfx(MM_GORON_SQUAT, NA_SE_IT_SHIELD_POSTURE, &player->actor.projectedPos);
    return SOH_FORM_ACTION_STARTED;
}

static void DrawGuard(PlayState* play, Player* player) {
    if (!sShieldingReady) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sEyesOpenTex);
    Matrix_Push();
    Matrix_SetTranslateRotateYXZ(player->actor.world.pos.x, player->actor.world.pos.y, player->actor.world.pos.z,
                                 &player->actor.shape.rot);
    Matrix_Scale(player->actor.scale.x, player->actor.scale.y, player->actor.scale.z, MTXMODE_APPLY);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    SkelAnime_DrawFlexOpa(play, sShieldingSkel.skeleton, sShieldingSkel.jointTable, sShieldingSkel.dListCount, NULL,
                          NULL, &player->actor);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- deep water ----

// Link only takes the curling pose and holds it while he sinks: the ball itself never comes out.
static void RunSink(Player* player, PlayState* play) {
    Player_ZeroSpeedXZ(player);
    if (!sSinkCurled) {
        if (LinkAnimation_Update(play, &player->skelAnime)) {
            sSinkCurled = true;
            player->actor.gravity = -2.0f;
        }
        return;
    }
    if (sSinkTimer > 0 && --sSinkTimer == 0) {
        Play_TriggerVoidOut(play);
    }
}

// A Goron cannot swim: MM curls him up and lets him sink until the scene voids him out. Vanilla's func_8083D53C
// re-seats its swim on anyone deep in water whose action is not a swim, so the sink keeps that swim installed
// and runs in its place through VB_EXECUTE_PLAYER_ACTION_FUNC, which is how NEI's hook kept OoT out.
static void TrySink(PlayState* play, Player* player) {
    if (sMove == GORON_MOVE_SINK || !(player->stateFlags1 & PLAYER_STATE1_IN_WATER) ||
        player->actor.yDistToWater <= SINK_DEPTH ||
        (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_GETTING_ITEM))) {
        return;
    }
    if (IsRolling()) {
        LeaveBall(player);
    }
    sMove = GORON_MOVE_SINK;
    sSinkCurled = sAnims[ANIM_CURL] == NULL;
    sSinkTimer = SINK_BALL_FRAMES;
    player->actor.velocity.y = -2.0f;
    player->actor.gravity = -0.5f;
    PlayAnim(play, player, ANIM_CURL, 1.0f, ANIMMODE_ONCE, -3.0f);
    Audio_PlayActorSound2(&player->actor, NA_SE_EV_DIVE_INTO_WATER);
    PlayMmSfx(MM_GORON_TO_BALL, NA_SE_PL_CHANGE_ARMS, &player->actor.projectedPos);
}

static void DriveSink(bool* should, va_list args) {
    Player* player = va_arg(args, Player*);

    if (!IsGoron() || sMove != GORON_MOVE_SINK || gPlayState == NULL ||
        (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE))) {
        return;
    }
    *should = false;
    RunSink(player, gPlayState);
}

// ---- drum ----

static bool IsHoldingOcarina(Player* player) {
    return player->rightHandType == PLAYER_MODELTYPE_RH_OCARINA || player->rightHandType == PLAYER_MODELTYPE_RH_OOT;
}

static bool IsPlayingOcarinaClip(Player* player, const char* name) {
    const char* anim = (const char*)player->skelAnime.animation;

    return anim != NULL && ResourceMgr_OTRSigCheck((char*)anim) && strstr(anim, name) != NULL;
}

static MmOcarinaNoteFunc FindDrumVoice(void) {
    if (!SOH_MOD_API_HAS(sApi, FindService)) {
        return NULL;
    }
    return (MmOcarinaNoteFunc)sApi->FindService(MM_OCARINA_NOTE_SERVICE);
}

// MM's drums sing through Sequence_0's ocarina channel. The hook runs right after Ocarina triggered its own
// voice, so stopping that here always lands.
static void VoiceDrum(uint8_t note, float bendFreq) {
    MmOcarinaNoteFunc voice = sDrumVoice;

    if (voice == NULL) {
        return;
    }
    sDrumBend = bendFreq;
    if (note != OCARINA_NOTE_NONE) {
        Audio_StopSfxById(NA_SE_OC_OCARINA);
    }
    if (note != sLastNote) {
        voice(MM_OCARINA_INSTRUMENT_GORON_DRUMS, note, &sDrumBend);
    }
}

// Runs inside the ocarina's own update; the pose consumes the edge on the game frame.
static void NoteStruck(uint8_t note, float modulator, int8_t bend) {
    (void)bend;
    if (note != OCARINA_NOTE_NONE && note != sLastNote) {
        sWasNoteStruck = true;
    }
    VoiceDrum(note, modulator);
    sLastNote = note;
}

static void PlaybackNoteStruck(uint8_t note, float modulator) {
    NoteStruck(note, modulator, 0);
}

static void SilenceDrum(void) {
    MmOcarinaNoteFunc voice = sDrumVoice;

    sDrumVoice = NULL;
    if (voice != NULL) {
        voice(MM_OCARINA_INSTRUMENT_GORON_DRUMS, OCARINA_NOTE_NONE, &sDrumBend);
    }
}

static void ChangeDrumClip(PlayState* play, Player* player, s32 slot, f32 speed, u8 mode, f32 morph) {
    LinkAnimationHeader* anim = sAnims[slot];
    f32 last = Animation_GetLastFrame(anim);

    LinkAnimation_Change(play, &player->skelAnime, anim, speed, speed >= 0.0f ? 0.0f : last,
                         speed >= 0.0f ? last : 0.0f, mode, morph);
}

// Vanilla's ocarina action waits for the end of its intro and outro clips; the drums' own clips take their
// place, so that wait is what raises and lowers them, as MM's Player_Action_63 does.
static bool SwapOcarinaClip(PlayState* play, Player* player) {
    if (IsPlayingOcarinaClip(player, "okarina_start")) {
        ChangeDrumClip(play, player, ANIM_DRUM_RAISE, 1.0f, ANIMMODE_ONCE, -6.0f);
        sDrumPhase = DRUM_RAISING;
        return true;
    }
    if (IsPlayingOcarinaClip(player, "okarina_end")) {
        ChangeDrumClip(play, player, ANIM_DRUM_RAISE, -1.0f, ANIMMODE_ONCE, -6.0f);
        sDrumPhase = DRUM_LOWERING;
        return true;
    }
    return false;
}

// The beat loops: that same action reopens the ocarina whenever a clip reports its end, which would wipe
// the notes of the song being played.
static void PoseDrum(PlayState* play, Player* player) {
    LinkAnimationHeader* beat = sAnims[ANIM_DRUM_PLAY];
    f32 beatEnd = Animation_GetLastFrame(beat) - CLIP_FRAMES_PER_UPDATE;

    if (player->skelAnime.animation != beat) {
        ChangeDrumClip(play, player, ANIM_DRUM_PLAY, 1.0f, ANIMMODE_LOOP, -4.0f);
        player->skelAnime.playSpeed = 0.0f;
        sDrumPhase = DRUM_PLAYING;
    }
    bool isBeatDone = player->skelAnime.curFrame >= beatEnd;
    if (sWasNoteStruck) {
        if (isBeatDone) {
            player->skelAnime.curFrame = 0.0f;
        }
        player->skelAnime.playSpeed = 1.0f;
    } else if (isBeatDone) {
        player->skelAnime.playSpeed = 0.0f;
    }
    sWasNoteStruck = false;
}

// The ocarina stays out across the message session, so the latch only lets go once no message is up.
static void UpdateDrum(PlayState* play, Player* player) {
    bool isSessionOpen = play->msgCtx.msgMode != MSGMODE_NONE;

    if (IsHoldingOcarina(player)) {
        sIsOcarinaOut = true;
    } else if (!isSessionOpen) {
        sIsOcarinaOut = false;
    }
    if (!sIsOcarinaOut) {
        sDrumPhase = DRUM_IDLE;
        if (sDrumVoice != NULL) {
            SilenceDrum();
        }
        return;
    }
    if (sDrumVoice == NULL) {
        sDrumVoice = FindDrumVoice();
    }
    if (sAnims[ANIM_DRUM_RAISE] == NULL || sAnims[ANIM_DRUM_PLAY] == NULL || SwapOcarinaClip(play, player)) {
        return;
    }
    bool isRaising = sDrumPhase == DRUM_RAISING && player->skelAnime.animation == sAnims[ANIM_DRUM_RAISE];
    if (isRaising || sDrumPhase == DRUM_LOWERING) {
        return;
    }
    // A raise that ended hands over to the beat even before an actor opens the session it was played for.
    if (isSessionOpen || sDrumPhase == DRUM_RAISING) {
        PoseDrum(play, player);
    }
}

static void DrawDrum(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sDrumDL);
    for (u32 i = 0; i < ARRAY_COUNT(sDrumPieceDLs); i++) {
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sDrumPieceDLs[i]);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- drawing ----

static void DrawGoronLimbExtras(PlayState* play, Player* player, int32_t limbIndex) {
    if (!IsGoron()) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_TORSO && sIsOcarinaOut && sDrumPhase != DRUM_IDLE) {
        DrawDrum(play);
    }
    if (sMove != GORON_MOVE_PUNCH || limbIndex != sPunchLimb[sPunchStep]) {
        return;
    }
    if (IsPunchLive(player)) {
        bool isButt = sPunchStep == PUNCH_STEPS - 1;
        PlaceStrike(play, player, isButt ? sButtTips : sFistTips, isButt ? sButtBases : sFistBases);
    } else {
        player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
        player->meleeWeaponQuads[1].base.atFlags &= ~AT_ON;
        player->meleeWeaponInfo[1].active = 0;
        player->meleeWeaponInfo[2].active = 0;
    }
    DrawPunchEffect(play);
}

static void DrawGoron(PlayState* play, Player* player) {
    if (IsBallShape()) {
        DrawBall(play, player);
    } else if (sMove == GORON_MOVE_GUARD) {
        DrawGuard(play, player);
    }
}

// A Goron never speaks with Link's voice, even for a grunt MM's bank has no sample for.
static void SpeakWithGoronVoice(Actor* actor, uint16_t sfxId) {
    const uint16_t action = (uint16_t)(sfxId - NA_SE_VO_LI_SWORD_N);
    MmSfxPlayFunc play = GetMmSfxPlayer();

    if (action < LINK_VOICE_ACTIONS && play != NULL) {
        play((uint16_t)(NA_SE_VO_LI_SWORD_N + MM_GORON_VOICE_OFFSET + action), &actor->projectedPos);
    }
}

// MM's Player_GetFloorSfxByAge: the floor offset plus the form's own block of the player bank.
static bool StepWithGoronFeet(Actor* actor) {
    Player* player = (Player*)actor;
    MmSfxPlayFunc play = GetMmSfxPlayer();

    if (IsBallShape()) {
        return true;
    }
    return play != NULL &&
           play((uint16_t)(NA_SE_PL_WALK_GROUND + player->floorSfxOffset + MM_GORON_STEP_OFFSET), &actor->projectedPos);
}

// Thrown to the floor, a Goron lands with MM's heavy impact instead of Link's bounce.
static bool LandWithGoronWeight(Actor* actor, uint16_t sfxId) {
    Player* player = (Player*)actor;
    MmSfxPlayFunc play = GetMmSfxPlayer();

    return sfxId == (uint16_t)(NA_SE_PL_BOUND + player->floorSfxOffset) && play != NULL &&
           play(MM_LI_FUTTOBI, &actor->projectedPos);
}

static void ResolveGoronSfx(Actor* actor, int32_t kind, uint16_t* sfxId, bool* handled) {
    if (!IsGoron()) {
        return;
    }
    if (kind == SOH_ACTOR_SFX_VOICE) {
        SpeakWithGoronVoice(actor, *sfxId);
        *handled = true;
    } else if (kind == SOH_ACTOR_SFX_STEP) {
        *handled = StepWithGoronFeet(actor);
    } else if (kind == SOH_ACTOR_SFX_GENERIC) {
        *handled = LandWithGoronWeight(actor, *sfxId);
    }
}

// ---- lifetime ----

static bool IsGoronBusy(Player* player) {
    return player->actionFunc == GoronPunchAction || player->actionFunc == GoronCurlAction ||
           player->actionFunc == GoronRollAction || player->actionFunc == GoronGuardAction || sMove == GORON_MOVE_SINK;
}

static void ResetState(Player* player) {
    if (player != NULL && IsRolling()) {
        LeaveBall(player);
    }
    if (player != NULL) {
        player->cylinder.base.acFlags |= AC_ON;
    }
    sMove = GORON_MOVE_NONE;
    sPunchFrames = 0;
    sPunchQueued = false;
    sDrumPhase = DRUM_IDLE;
    sIsOcarinaOut = false;
    sSinkCurled = false;
    sScaledDamage = 0;
    ResetBall();
    StopRollSfx();
}

static void EnterGoron(PlayState* play, Player* player) {
    ResetState(player);
    sApi->RequestHazard(GORON_HAZARD_OWNER, 0, PLAYER_ENV_HAZARD_NONE, EQUIP_VALUE_TUNIC_KOKIRI);
}

static void ExitGoron(PlayState* play, Player* player) {
    bool wasBusy = IsGoronBusy(player) && sMove != GORON_MOVE_SINK;

    sApi->ReleaseHazard(GORON_HAZARD_OWNER);
    SilenceDrum();
    ResetState(player);
    if (wasBusy) {
        func_80839FFC(player, play);
    }
}

// A slope slide or a stray vanilla action can take the ball's action func without anything having hit it.
static void KeepBallRolling(PlayState* play, Player* player) {
    if (!IsRolling() || player->actionFunc == GoronRollAction) {
        return;
    }
    if (CanAct(player)) {
        Player_SetupAction(play, player, GoronRollAction, 0);
        return;
    }
    LeaveBall(player);
    sMove = GORON_MOVE_NONE;
}

static void UpdateGoron(PlayState* play, Player* player) {
    KeepBallRolling(play, player);
    LaunchOffLedges(player);
    if (sMove != GORON_MOVE_NONE && !IsRolling() && !IsGoronBusy(player)) {
        player->cylinder.base.acFlags |= AC_ON;
        sMove = GORON_MOVE_NONE;
        sPunchFrames = 0;
    }
    if (sMove == GORON_MOVE_GUARD) {
        SubmitGuard(play, player);
    }
    // In water the hazard claim would also waive drowning, and a Goron sinks there anyway.
    if (player->stateFlags1 & PLAYER_STATE1_IN_WATER) {
        sApi->ReleaseHazard(GORON_HAZARD_OWNER);
    } else {
        sApi->RequestHazard(GORON_HAZARD_OWNER, 0, PLAYER_ENV_HAZARD_NONE, EQUIP_VALUE_TUNIC_KOKIRI);
    }
    TrySink(play, player);
    UpdateDrum(play, player);
}

// Colliders remember their actor, and a new scene brings a new player.
static void ForgetScene(int16_t sceneNum) {
    (void)sceneNum;
    sGuardColliderReady = false;
    sShieldingReady = false;
    sStandingRadius = 0;
    sStandingShadow = 0.0f;
    ResetState(NULL);
}

// ---- mask ----

static bool CanWearMask(Player* player, PlayState* play) {
    return IsGrounded(player) &&
           !(player->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_IN_CUTSCENE));
}

static void WearMask(PlayState* play, Player* player) {
    sApi->ToggleForm(GORON_FORM_KEY);
}

// A vanilla GetItemEntry has no draw callback of its own, and the registry reads NULL as "no model".
static void DrawMaskGetItem(PlayState* play, GetItemEntry* entry) {
    (void)entry;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_26Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGetItemDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void RegisterMask(void) {
    SOHCustomItemDefinition mask = Z64Items_Define(GORON_MASK_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&mask, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    mask.flags |= SOH_CUSTOM_ITEM_INSTANT | SOH_CUSTOM_ITEM_WEARABLE;
    Z64Items_SetPlacement(&mask, GORON_MASK_PAGE, GORON_MASK_SLOT, 0);
    mask.getItemEntry = ItemTable_RetrieveEntry(MOD_NONE, GI_MASK_GORON);
    mask.getItemEntry.textId = 0;
    mask.getItemEntry.drawFunc = DrawMaskGetItem;
    Z64Items_SetReplaces(&mask, ITEM_MASK_GORON);
    Z64Items_SetVanillaMode(&mask, SOH_VANILLA_ITEM_REPLACE, RG_GORON_MASK);
    Z64Items_SetTextbox(&mask, "You got the %rGoron Mask%w!&It holds the spirit of a proud Goron hero.^"
                               "Wear it with %y\xA1%w to become a Goron: heavy fists, a rock-hard shell and a body "
                               "that rolls.");
    Z64Items_SetPauseText(&mask, "%rGoron Mask&%wPress %y\xA1%w to become a Goron.&%y\xA0%w: punch  Hold %y\x9F%w: "
                                 "roll  %y\xA0%w rolling: pound  Hold %y\xA3%w: guard");
    Z64Items_SetCanUse(&mask, CanWearMask);
    Z64Items_SetAction(&mask, WearMask, NULL);
    Z64Items_Register(sApi, &mask);
}

static const SOHFormActionOverride sActions[] = {
    { SOH_PLAYER_ACTION_MELEE, StartPunch },
    { SOH_PLAYER_ACTION_ROLL, StartCurl },
    { SOH_PLAYER_ACTION_ZTARGET_A, HandleZTargetA },
    { SOH_PLAYER_ACTION_SHIELD, StartGuard },
};

// Standing still the Goron takes MM's own stance; walking and running stay Link's, as in NEI.
static SOHFormAnimOverride sAnimOverrides[] = {
    { PLAYER_ANIMGROUP_wait, -1, NULL },
};

static uint16_t ResolveGoronEquipment(int32_t equipType, uint16_t value) {
    if (equipType == EQUIP_TYPE_SWORD) {
        return EQUIP_VALUE_SWORD_NONE;
    }
    if (equipType == EQUIP_TYPE_SHIELD) {
        return EQUIP_VALUE_SHIELD_NONE;
    }
    if (equipType == EQUIP_TYPE_BOOTS) {
        return EQUIP_VALUE_BOOTS_KOKIRI;
    }
    return value;
}

static bool IsFormMask(const char* customKey) {
    static const char* const sFormMasks[] = {
        "nei.deku_form_mask", "nei.zora_form_mask",   "nei.keaton_form_mask",  "nei.mask_kafei",
        "nei.garo_form_mask", "nei.gerudo_form_mask", "nei.fierce_deity_mask", "nei.rito_form_mask",
    };

    for (u32 i = 0; i < ARRAY_COUNT(sFormMasks); i++) {
        if (strcmp(customKey, sFormMasks[i]) == 0) {
            return true;
        }
    }
    return false;
}

static bool AllowsGoronItem(uint16_t item, const char* customKey) {
    if (customKey != NULL) {
        return IsFormMask(customKey);
    }
    return item == ITEM_OCARINA_FAIRY || item == ITEM_OCARINA_TIME || item == ITEM_BOMB || item == ITEM_BOMBCHU ||
           item == ITEM_LENS || item == ITEM_NAYRUS_LOVE || (item >= ITEM_BOTTLE && item <= ITEM_POE);
}

static void RegisterForm(void) {
    SOHFormDefinition goron = { 0 };

    goron.structSize = sizeof(goron);
    goron.key = GORON_FORM_KEY;
    goron.label = "Goron";
    goron.kind = SOH_FORM_KIND_LINK;
    goron.item = GORON_MASK_KEY;
    goron.modelPath = GORON_MODEL_PATH;
    goron.rootScaleAdult = GORON_ROOT_SCALE_ADULT;
    goron.rootScaleChild = GORON_ROOT_SCALE_CHILD;
    goron.height = GORON_HEIGHT;
    goron.motionScale = 1.0f;
    goron.resolveEquipment = ResolveGoronEquipment;
    goron.allowsButtonItem = AllowsGoronItem;
    goron.transformAnim = sAnims[ANIM_MASK_ON];
    goron.transformOffAnim = sAnims[ANIM_MASK_OFF];
    goron.transformMask = sMaskDL;
    goron.transformVoiceSfx = NA_SE_VO_LI_FALL_L;
    // MM D_801C0E40[PLAYER_FORM_GORON].
    goron.transformGlowOffset = (Vec3f){ -578.3f, -1100.9f, 0.0f };
    sAnimOverrides[0].anim = sAnims[ANIM_WAIT];
    goron.anims = sAnimOverrides;
    goron.animCount = sAnims[ANIM_WAIT] != NULL ? ARRAY_COUNT(sAnimOverrides) : 0;
    goron.actions = sActions;
    goron.actionCount = ARRAY_COUNT(sActions);
    goron.onEnter = EnterGoron;
    goron.onExit = ExitGoron;
    goron.update = UpdateGoron;
    goron.draw = DrawGoron;
    sApi->RegisterForm(&goron);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    LoadAnims();
    RegisterMask();
    RegisterForm();
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, ResolveGoronBody);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawGoronLimbExtras);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveFaceTextures, ResolveGoronFace);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveHeight, ResolveGoronHeight);
    SOH_REGISTER_HOOK(sApi, OnPlayerFilterInput, ScaleIncomingDamage);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorPlaySfx, ACTOR_PLAYER, ResolveGoronSfx);
    SOH_REGISTER_HOOK(sApi, OnOcarinaNote, NoteStruck);
    SOH_REGISTER_HOOK(sApi, OnOcarinaPlaybackNote, PlaybackNoteStruck);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
    sApi->RegisterVB(VB_USE_STRENGTH_UPGRADE, ResolveGoronStrength);
    sApi->RegisterVB(VB_EXECUTE_PLAYER_ACTION_FUNC, DriveSink);
}
