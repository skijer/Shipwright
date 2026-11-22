#include <stdbool.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "z64items.h"
#include "z64strength.h"
#include "z64aiming.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/actors/ovl_En_Boom/z_en_boom.h"

#define ZORA_MASK_KEY "nei.zora_form_mask"
#define ZORA_FORM_KEY "nei.zora"
#define ZORA_MODEL_PATH "objects/forms/zora"
#define ZORA_HAZARD_OWNER "nei.zora"
#define ZORA_MASK_PAGE 1
#define ZORA_MASK_SLOT 17

#define BG_ON_GROUND 1
#define MM_ANIM_VALUES_PER_FRAME 67
#define MM_ANIM_BASE_TRANSL_X (-57)
#define ZORA_ROOT_SCALE_ADULT 1.066f
#define ZORA_ROOT_SCALE_CHILD 1.515f
#define ZORA_HEIGHT 68.0f

#define PUNCH_STEPS 3
#define PUNCH_DMG_FLAGS DMG_SLASH_MASTER
#define PUNCH_DAMAGE 1
#define KICK_DMG_FLAGS DMG_JUMP_MASTER
#define KICK_DAMAGE 2
#define KICK_SPEED 5.5f
#define KICK_LIFT 4.5f
#define KICK_GRAVITY -0.8f
#define BOOMERANG_HOLD_FRAMES 10
#define BOOMERANG_THROW_FRAME 6.0f
#define BOOMERANG_RETURN_TIMER 20
#define BOOMERANG_SPREAD_LOCKED 0x36B0
#define BOOMERANG_SPREAD_FREE 0x190
#define FIN_SPIN_STEP 0x2EE0
#define FIN_TILT 0x1F40

#define SWIM_EXIT_DEPTH 30.0f
#define SWIM_ENTER_DEPTH 45.0f
#define SWIM_BUOYANCY_DEPTH 44.8f
#define SWIM_SURFACE_DEPTH (SWIM_EXIT_DEPTH + 10.0f)
#define SWIM_DEEP_DEPTH (SWIM_ENTER_DEPTH + 12.0f)
#define SWIM_GRAVITY 0.0f
#define FAST_SWIM_SPEED 9.0f
#define SWIM_INVERT_X_CVAR "gMods.Zora.InvertSwimX"
#define SWIM_INVERT_Y_CVAR "gMods.Zora.InvertSwimY"
#define FAST_SWIM_KICK_SPEED 12.0f
#define FAST_SWIM_BURST 16.0f
#define FAST_SWIM_KICK_FRAME 13.0f
#define FAST_SWIM_RESTART_FRAME 5.0f
#define ROLL_HOLD_FRAMES 5
#define ROLL_STEER_STEP 0x640
#define BANK_SFX_ANGLE 0x3A98
#define BANK_SFX_TURN 0xFA0
#define FLOOR_SKIM_HEIGHT 20.0f
#define WALL_FACING_YAW 0x1C00
#define BONK_SPEED_SCALE 0.2f
#define BONK_RECOVER_SPEED 10.0f
#define DOLPHIN_PITCH (-0x1555)
#define DOLPHIN_MAX_SPEED 13.5f
#define DOLPHIN_GRAVITY -1.0f
#define DOLPHIN_REENTRY_FALL -2.0f
#define DASH_EXIT_DEPTH -10000.0f
#define DOLPHIN_STEEP_PITCH 0x36B0
#define DOLPHIN_STEEP_DAMAGE 0x10
#define DOLPHIN_WATERLINE ((SWIM_ENTER_DEPTH + SWIM_EXIT_DEPTH) * 0.5f)
#define GRAVITY_NORMAL -1.2f

#define BARRIER_DRAIN_INTERVAL 10
#define BARRIER_FULL_MAGIC 16
#define BARRIER_RAMP 50
#define BARRIER_DAMAGE 2
#define BARRIER_RADIUS 50
#define BARRIER_HEIGHT 80

#define GUARD_FRONT 12.0f
#define GUARD_HALF_WIDTH 25.0f
#define GUARD_BOTTOM -10.0f
#define GUARD_TOP 75.0f

#define OCARINA_NOTE_NONE 0xFF
#define FACE_NEUTRAL 0
#define FACE_OPENING 13
#define FACE_HURT 8
#define MM_ANIM_ADJUSTED_SPEED (2.0f / 3.0f)
#define DASH_CLIP_SPEED (MM_ANIM_ADJUSTED_SPEED * 2.0f)
#define CLIP_FRAMES_PER_UPDATE 1.5f
#define LINK_VOICE_ACTIONS 0x20
#define MM_ZORA_VOICE_OFFSET 0xA0
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"
#define MM_SFX_STOP_SERVICE "mm.sfx.stop"
#define MM_ZORA_STEP_OFFSET 0x120
#define MM_CATCH_BOOMERANG 0x0836
#define MM_FACE_UP 0x0863
#define MM_ZORA_SWIM_DASH 0x08EC
#define MM_ZORA_SWIM_ROLL 0x08EE
// MM asks for its loops as `id - SFX_FLAG` every frame: without the flag its scheduler plays the sound for as
// long as it keeps being asked for, with it every request starts the sample over.
#define MM_SFX_FLAG 0x800
#define MM_ZORA_SWIM_LV (0x08ED - MM_SFX_FLAG)
#define MM_ZORA_SPARK_BARRIER (0x09AF - MM_SFX_FLAG)
#define MM_BOOMERANG_THROW 0x1805
#define MM_SHIELD_SWING 0x181F
#define MM_GORON_PUNCH_SWING 0x1857
#define MM_SHIELD_REMOVE_ZORA 0x1869
#define MM_VO_ZORA_SWORD_N 0x68A0
#define MM_VO_ZORA_SWORD_L 0x68A1
typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);
#define MM_OCARINA_NOTE_SERVICE "mm.ocarina.note"
#define MM_OCARINA_INSTRUMENT_ZORA_GUITAR 8
typedef bool (*MmOcarinaNoteFunc)(uint8_t instrumentId, uint8_t pitch, f32* bendFreq);
typedef void (*MmSfxStopFunc)(uint16_t sfxId);

#define OBJ_ZORA "__OTR__objects/object_link_zora/"
static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_static/gItemIconMaskZoraTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_static/gZoraMaskItemNameENGTex";
static const ALIGN_ASSET(2) char sGetItemDL[] = "__OTR__objects/object_gi_zoramask/gGiZoraMaskDL";
static const ALIGN_ASSET(2) char sMaskDL[] = "__OTR__objects/object_link_child/gLinkChildZoraMaskDL";
static const ALIGN_ASSET(2) char sLeftFinDL[] = OBJ_ZORA "object_link_zora_DL_00CC38";
static const ALIGN_ASSET(2) char sRightFinDL[] = OBJ_ZORA "object_link_zora_DL_00CDA0";
static const ALIGN_ASSET(2) char sGuardFinDL[] = OBJ_ZORA "object_link_zora_DL_0110A8";
static const ALIGN_ASSET(2) char sLeftSwimFinDL[] = OBJ_ZORA "object_link_zora_DL_010868";
static const ALIGN_ASSET(2) char sRightSwimFinDL[] = OBJ_ZORA "object_link_zora_DL_010978";
static const ALIGN_ASSET(2) char sGuitarDL[] = OBJ_ZORA "object_link_zora_DL_00E2A0";
static const ALIGN_ASSET(2) char sBarrierDL[] = OBJ_ZORA "object_link_zora_DL_011760";
static const ALIGN_ASSET(2) char sLeftFlyingFinDL[] = "__OTR__objects/gameplay_keep/gameplay_keep_DL_06FE20";
static const ALIGN_ASSET(2) char sRightFlyingFinDL[] = "__OTR__objects/gameplay_keep/gameplay_keep_DL_06FF68";

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
    ANIM_JUMP_KICK,
    ANIM_JUMP_KICK_END,
    ANIM_FINS_READY,
    ANIM_FINS_THROW,
    ANIM_FINS_CATCH,
    ANIM_WATER_ROLL,
    ANIM_FISH_SWIM,
    ANIM_SWIM_TO_WAIT,
    ANIM_GUITAR_RAISE,
    ANIM_GUITAR_PLAY,
    ANIM_MASK_ON,
    ANIM_WAIT,
    ANIM_MASK_OFF,
    ANIM_COUNT,
} ZoraAnim;

static const char* const sAnimPaths[ANIM_COUNT] = {
    MM_ANIM_PATH("pz_attackA"),      MM_ANIM_PATH("pz_attackB"),     MM_ANIM_PATH("pz_attackC"),
    MM_ANIM_PATH("pz_attackAend"),   MM_ANIM_PATH("pz_attackBend"),  MM_ANIM_PATH("pz_attackCend"),
    MM_ANIM_PATH("pz_attackAendR"),  MM_ANIM_PATH("pz_attackBendR"), MM_ANIM_PATH("pz_attackCendR"),
    MM_ANIM_PATH("pz_jumpAT"),       MM_ANIM_PATH("pz_jumpATend"),   MM_ANIM_PATH("pz_cutterwaitanim"),
    MM_ANIM_PATH("pz_cutterattack"), MM_ANIM_PATH("pz_cuttercatch"), MM_ANIM_PATH("pz_waterroll"),
    MM_ANIM_PATH("pz_fishswim"),     MM_ANIM_PATH("pz_swimtowait"),  MM_ANIM_PATH("pz_gakkistart"),
    MM_ANIM_PATH("pz_gakkiplay"),    MM_ANIM_PATH("cl_setmask"),     MM_ANIM_PATH("pz_wait"),
    MM_ANIM_PATH("pz_maskoffstart"),
};

static const char* const sZoraLimbDL[PLAYER_LIMB_MAX] = {
    NULL,
    NULL,
    OBJ_ZORA "gLinkZoraWaistDL",
    NULL,
    OBJ_ZORA "gLinkZoraRightThighDL",
    OBJ_ZORA "gLinkZoraRightShinDL",
    OBJ_ZORA "gLinkZoraRightFootDL",
    OBJ_ZORA "gLinkZoraLeftThighDL",
    OBJ_ZORA "gLinkZoraLeftShinDL",
    OBJ_ZORA "gLinkZoraLeftFootDL",
    NULL,
    OBJ_ZORA "gLinkZoraHeadDL",
    OBJ_ZORA "gLinkZoraHatDL",
    OBJ_ZORA "gLinkZoraCollarDL",
    OBJ_ZORA "gLinkZoraLeftShoulderDL",
    OBJ_ZORA "gLinkZoraLeftForearmDL",
    OBJ_ZORA "gLinkZoraLeftHandClosedDL",
    OBJ_ZORA "gLinkZoraRightShoulderDL",
    OBJ_ZORA "gLinkZoraRightForearmDL",
    OBJ_ZORA "gLinkZoraRightHandClosedDL",
    NULL,
    OBJ_ZORA "gLinkZoraTorsoDL",
};
static const ALIGN_ASSET(2) char sLeftOpenHandDL[] = OBJ_ZORA "gLinkZoraLeftHandOpenDL";
static const ALIGN_ASSET(2) char sRightOpenHandDL[] = OBJ_ZORA "gLinkZoraRightHandOpenDL";

static const Vec3f sZoraJointPos[PLAYER_LIMB_MAX] = {
    { 0, 0, 0 },       { 0, 0, 0 },       { 0, 0, 0 },         { 945, 0, 0 },  { -442, -5, -350 }, { 1100, 0, 0 },
    { 1500, 0, 0 },    { -442, -5, 350 }, { 1100, 0, 0 },      { 1500, 0, 0 }, { 0, 21, -7 },      { 1806, 0, 0 },
    { -298, -700, 0 }, { 0, 0, 0 },       { 1350, -100, 630 }, { 1019, 0, 0 }, { 1104, 0, 0 },     { 1350, -100, -630 },
    { 1019, 0, 0 },    { 1104, 0, 0 },    { 978, -200, 0 },    { 0, 0, 0 },
};

static const ALIGN_ASSET(2) char sEyesOpenTex[] = OBJ_ZORA "gLinkZoraEyesOpenTex";
static const ALIGN_ASSET(2) char sEyesHalfTex[] = OBJ_ZORA "gLinkZoraEyesHalfTex";
static const ALIGN_ASSET(2) char sEyesClosedTex[] = OBJ_ZORA "gLinkZoraEyesClosedTex";
static const ALIGN_ASSET(2) char sEyesLeftTex[] = OBJ_ZORA "gLinkZoraEyesLeftTex";
static const ALIGN_ASSET(2) char sEyesRightTex[] = OBJ_ZORA "gLinkZoraEyesRightTex";
static const ALIGN_ASSET(2) char sEyesWincingTex[] = OBJ_ZORA "gLinkZoraEyesWincingTex";
static const ALIGN_ASSET(2) char sMouthClosedTex[] = OBJ_ZORA "gLinkZoraMouthClosedTex";
static const ALIGN_ASSET(2) char sMouthHalfTex[] = OBJ_ZORA "gLinkZoraMouthHalfTex";
static const ALIGN_ASSET(2) char sMouthOpenTex[] = OBJ_ZORA "gLinkZoraMouthOpenTex";
static const ALIGN_ASSET(2) char sMouthSmileTex[] = OBJ_ZORA "gLinkZoraMouthSmileTex";

static Vec3f sFinTips[3] = { { -2500, 1400, 1100 }, { -2900, 1000, 1500 }, { -2100, 1800, 700 } };
static Vec3f sFinBases[3] = { { 900, 300, 100 }, { 1300, 700, -300 }, { 500, -100, 500 } };
static Vec3f sKickTips[3] = { { 2000, 0, 0 }, { 2800, -800, -800 }, { 2800, 800, 800 } };
static Vec3f sKickBases[3] = { { 0, 0, 0 }, { -800, 800, 800 }, { -800, -800, -800 } };
static const f32 sPunchHitStart[PUNCH_STEPS] = { 2.0f, 3.0f, 3.0f };
static const f32 sPunchHitEnd[PUNCH_STEPS] = { 5.0f, 8.0f, 10.0f };
static const s32 sPunchLimb[PUNCH_STEPS] = { PLAYER_LIMB_L_FOREARM, PLAYER_LIMB_R_FOREARM, PLAYER_LIMB_R_SHIN };

static const char* const sRequiredHooks[] = {
    "OnPlayerActionHandler", "OnPlayerResolveLimbDraw",
    "OnPlayerPostLimbDraw",  "OnPlayerResolveFaceTextures",
    "OnPlayerFilterInput",   "OnActorPlaySfx",
    "OnActorDraw",           "OnOcarinaNote",
    "OnOcarinaPlaybackNote", "OnSceneInit",
    "OnPlayerUpdate",        "OnActorDrawEnd",
};
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

typedef enum {
    ZORA_MOVE_NONE,
    ZORA_MOVE_PUNCH,
    ZORA_MOVE_PUNCH_END,
    ZORA_MOVE_JUMP_KICK,
    ZORA_MOVE_KICK_LAND,
    ZORA_MOVE_FINS_READY,
    ZORA_MOVE_FINS_THROW,
    ZORA_MOVE_FINS_LOCKED_READY,
    ZORA_MOVE_FINS_LOCKED_THROW,
    ZORA_MOVE_GUARD,
    ZORA_MOVE_GUARD_WALK,
    ZORA_MOVE_FAST_SWIM,
    ZORA_MOVE_DOLPHIN,
} ZoraMove;

typedef enum {
    SWIM_PHASE_ROLL,
    SWIM_PHASE_CRUISE,
    SWIM_PHASE_EXIT,
    SWIM_PHASE_BONK,
} SwimPhase;

typedef enum {
    GUITAR_IDLE,
    GUITAR_RAISING,
    GUITAR_PLAYING,
    GUITAR_LOWERING,
} GuitarPhase;

void Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void func_80839FFC(Player* player, PlayState* play);
void func_80838F18(PlayState* play, Player* player);
s32 func_80842DF4(PlayState* play, Player* player);
void func_80837918(Player* player, s32 quadIndex, u32 dmgFlags);
int Player_IsZTargeting(Player* player);
void Player_ZeroSpeedXZ(Player* player);
void Player_SetBootData(PlayState* play, Player* player);
s32 Player_GetMovementSpeedAndYaw(Player* player, f32* outSpeedTarget, s16* outYawTarget, f32 speedMode,
                                  PlayState* play);
GetItemEntry ItemTable_RetrieveEntry(s16 modIndex, s16 getItemID);
s32 Player_ActionHandler_2(Player* player, PlayState* play);
void Player_RequestQuake(PlayState* play, s32 speed, s32 y, s32 countdown);
void Player_RequestRumble(Player* player, s32 sourceStrength, s32 duration, s32 decreaseRate, s32 distSq);
void Player_PlayVoiceSfx(Player* player, u16 sfxId);

#define SPEED_MODE_LINEAR 0.0f

static const ALIGN_ASSET(2) char sSwimHitAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_swimer_swim_hit";

static const SOHModApi* sApi;
static bool sBorrowedGyorgSwim;
static void HumanGyorgSwim(PlayState* play, Player* player, bool enabled);
static void StartFastSwim(PlayState* play, Player* player, f32 fromFrame);

static MmSfxPlayFunc sPlayMmSfx;
static MmSfxStopFunc sStopMmSfx;

static LinkAnimationHeader sMmAnims[ANIM_COUNT];
static LinkAnimationHeader* sAnims[ANIM_COUNT];
static s16* sAnimFrames[ANIM_COUNT];

static u8 sMove;
static s32 sPunchStep;
static bool sPunchQueued;
static s16 sHoldFrames;
static s32 sTrail = -1;
static s16 sGuardFrames;
static s16 sGuardYaw;

static Actor* sFins[2];
static bool sFinsOut;
static s32 sSwimTrail[2] = { -1, -1 };
static WeaponInfo sSwimFinInfo[2];
static f32 sSwimFinFoldFrom;
static f32 sFinFrame;
static s16 sThrowYaw;
static s16 sThrowPitch;

static bool sIsHeavy;
static u8 sSwimPhase;
static s16 sSwimPitch;
static s16 sSwimRoll;
static s16 sSwimRollSmoothed;
static bool sSwimExitQueued;
static s16 sRollHoldFrames;
static bool sIsDashing;
static u32 sWaterFrames;
static PlayerAgeProperties sZoraBody;
static f32 sWaterExitDepth = SWIM_EXIT_DEPTH;
static s16 sSwimTurn;
static s16 sSwimFloorTimer;
static f32 sSwimSpeed;
static LinkAnimationHeader* sSwimAnim;
static f32 sSwimAnimFrame;
static f32 sSwimAnimSpeed;
static u8 sSwimAnimMode;

static bool sBarrierHeld;
static s16 sBarrierIntensity;
static s16 sBarrierDrainTimer;
static LightNode* sBarrierLight;
static LightInfo sBarrierLightInfo;
static ColliderCylinder sBarrierCollider;
static bool sBarrierColliderReady;

static u8 sGuitarPhase;
static bool sIsOcarinaOut;
static bool sWasNoteStruck;
static u8 sLastNote = OCARINA_NOTE_NONE;
// Set on the game thread while the guitar is out; the note hook runs on the audio thread and only reads it.
static MmOcarinaNoteFunc sGuitarVoice;
static f32 sGuitarBend = 1.0f;

static u8 sScaledDamage;

static ColliderCylinderInit sBarrierInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2,
      { 0x00080000 | DMG_SLASH_KOKIRI | DMG_SPIN_KOKIRI | DMG_JUMP_KOKIRI | DMG_HOOKSHOT, 0x00, BARRIER_DAMAGE },
      { 0xF7CFFFFF, 0x00, 0x00 },
      TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { BARRIER_RADIUS, BARRIER_HEIGHT, 0, { 0, 0, 0 } },
};

static bool IsZora(void) {
    return sApi != NULL && sApi->IsFormActive(ZORA_FORM_KEY);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BG_ON_GROUND) != 0;
}

static bool IsInWater(Player* player) {
    return (player->stateFlags1 & PLAYER_STATE1_IN_WATER) != 0;
}

static bool IsSwimming(Player* player) {
    return IsInWater(player) && !IsGrounded(player);
}

static bool CanAct(Player* player) {
    return !(player->stateFlags1 &
             (PLAYER_STATE1_DEAD | PLAYER_STATE1_LOADING | PLAYER_STATE1_TALKING | PLAYER_STATE1_GETTING_ITEM |
              PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_CARRYING_ACTOR |
              PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE |
              PLAYER_STATE1_DAMAGED | PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_CLIMBING_LEDGE |
              PLAYER_STATE1_HANGING_OFF_LEDGE));
}

static bool IsStriking(void) {
    return sMove == ZORA_MOVE_PUNCH || sMove == ZORA_MOVE_JUMP_KICK;
}

static bool HasSwimAbility(void) {
    return !IS_RANDO || Flags_GetRandomizerInf(RAND_INF_CAN_SWIM);
}

static bool IsSwimMove(void) {
    return sMove == ZORA_MOVE_FAST_SWIM || sMove == ZORA_MOVE_DOLPHIN;
}

static MmSfxPlayFunc GetMmSfxPlayer(void) {
    if (sPlayMmSfx == NULL && SOH_MOD_API_HAS(sApi, FindService)) {
        sPlayMmSfx = (MmSfxPlayFunc)sApi->FindService(MM_SFX_PLAY_SERVICE);
    }
    return sPlayMmSfx;
}

static void PlayMmSfx(u16 mmSfx, u16 fallback, Vec3f* pos) {
    MmSfxPlayFunc play = GetMmSfxPlayer();

    if (play == NULL || !play(mmSfx, pos)) {
        Audio_PlaySfxGeneral(fallback, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
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

static void LoadMmAnim(s32 slot) {
    LinkAnimationHeader* source = ResourceMgr_LoadPlayerAnimAsHeader(sAnimPaths[slot]);

    if (source == NULL || source->common.frameCount <= 0) {
        sAnims[slot] = NULL;
        return;
    }
    size_t values = (size_t)source->common.frameCount * MM_ANIM_VALUES_PER_FRAME;
    s16* frames = (s16*)malloc(values * sizeof(s16));
    if (frames == NULL) {
        sAnims[slot] = NULL;
        return;
    }
    memcpy(frames, source->segment, values * sizeof(s16));
    for (s32 frame = 0; frame < source->common.frameCount; frame++) {
        s16* root = &frames[frame * MM_ANIM_VALUES_PER_FRAME];
        root[0] = MM_ANIM_BASE_TRANSL_X;
        root[2] = 0;
    }
    free(sAnimFrames[slot]);
    sAnimFrames[slot] = frames;
    sMmAnims[slot].common.frameCount = source->common.frameCount;
    sMmAnims[slot].segment = frames;
    sAnims[slot] = &sMmAnims[slot];
}

static void LoadAnims(void) {
    for (s32 slot = 0; slot < ANIM_COUNT; slot++) {
        LoadMmAnim(slot);
    }
}

static f32 MmAnimSpeed(s32 slot) {
    switch (slot) {
        case ANIM_PUNCH_A:
        case ANIM_PUNCH_B:
        case ANIM_PUNCH_C:
        case ANIM_JUMP_KICK:
        case ANIM_JUMP_KICK_END:
        case ANIM_GUITAR_RAISE:
        case ANIM_GUITAR_PLAY:
        case ANIM_WAIT:
            return MM_ANIM_ADJUSTED_SPEED;
        case ANIM_WATER_ROLL:
        case ANIM_FISH_SWIM:
        case ANIM_SWIM_TO_WAIT:
            return DASH_CLIP_SPEED;
        default:
            return 1.0f;
    }
}

static void PlayAnim(PlayState* play, Player* player, s32 slot, f32 startFrame, u8 mode, f32 morph) {
    LinkAnimationHeader* anim = sAnims[slot];

    if (anim == NULL) {
        return;
    }
    LinkAnimation_Change(play, &player->skelAnime, anim, MmAnimSpeed(slot), startFrame, Animation_GetLastFrame(anim),
                         mode, morph);
    if (IsSwimMove()) {
        sSwimAnim = anim;
        sSwimAnimFrame = startFrame;
        sSwimAnimSpeed = MmAnimSpeed(slot);
        sSwimAnimMode = mode;
    }
}

static const char* HandDL(Player* player, int32_t limbIndex) {
    if (limbIndex == PLAYER_LIMB_L_HAND) {
        return player->leftHandType == PLAYER_MODELTYPE_LH_OPEN ? sLeftOpenHandDL : sZoraLimbDL[PLAYER_LIMB_L_HAND];
    }
    return player->rightHandType == PLAYER_MODELTYPE_RH_OPEN ? sRightOpenHandDL : sZoraLimbDL[PLAYER_LIMB_R_HAND];
}

static void PitchSwimmingBody(Vec3f* pos) {
    Matrix_Translate(pos->x, pos->y + (Math_CosS(sSwimPitch) - 1.0f) * 200.0f, pos->z, MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD(sSwimPitch), MTXMODE_APPLY);
    Matrix_RotateZ(BINANG_TO_RAD(sSwimRoll), MTXMODE_APPLY);
    pos->x = 0.0f;
    pos->y = 0.0f;
    pos->z = 0.0f;
}

static void ResolveZoraBody(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (sBorrowedGyorgSwim && !IsZora() && limbIndex == PLAYER_LIMB_ROOT && IsSwimMove() && pos != NULL) {
        PitchSwimmingBody(pos);
    }
    if (!IsZora() || limbIndex <= PLAYER_LIMB_NONE || limbIndex >= PLAYER_LIMB_MAX) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_ROOT) {
        if (IsSwimMove() && pos != NULL) {
            PitchSwimmingBody(pos);
        }
        return;
    }
    if (pos != NULL) {
        *pos = sZoraJointPos[limbIndex];
    }
    const char* path = sZoraLimbDL[limbIndex];
    if (limbIndex == PLAYER_LIMB_L_HAND || limbIndex == PLAYER_LIMB_R_HAND) {
        path = HandDL(player, limbIndex);
    }
    if (path != NULL) {
        *dList = ResourceMgr_LoadGfxByName(path);
    } else if (limbIndex == PLAYER_LIMB_SHEATH) {
        *dList = NULL;
    }
}

static const char* MirrorEyes(const char* vanilla) {
    if (strstr(vanilla, "EyesHalf") != NULL) {
        return sEyesHalfTex;
    }
    if (strstr(vanilla, "EyesClosed") != NULL) {
        return sEyesClosedTex;
    }
    if (strstr(vanilla, "RollLeft") != NULL) {
        return sEyesLeftTex;
    }
    if (strstr(vanilla, "RollRight") != NULL) {
        return sEyesRightTex;
    }
    if (strstr(vanilla, "Unk2") != NULL) {
        return sEyesWincingTex;
    }
    return sEyesOpenTex;
}

static const char* MirrorMouth(const char* vanilla) {
    if (strstr(vanilla, "Mouth2") != NULL) {
        return sMouthHalfTex;
    }
    if (strstr(vanilla, "Mouth3") != NULL) {
        return sMouthOpenTex;
    }
    if (strstr(vanilla, "Mouth4") != NULL) {
        return sMouthSmileTex;
    }
    return sMouthClosedTex;
}

static void ResolveZoraFace(const char** eyes, const char** mouth) {
    if (!IsZora()) {
        return;
    }
    if (*eyes != NULL) {
        *eyes = MirrorEyes(*eyes);
    }
    if (*mouth != NULL) {
        *mouth = MirrorMouth(*mouth);
    }
}

static void ResolveZoraStrength(bool* should, va_list args) {
    if (!IsZora()) {
        return;
    }
    Z64Strength_ResolveUpgrade(should, args, Z64STRENGTH_OFFSET_ZORA);
}

static void ResolveZoraGrab(bool* should, va_list args) {
    if (!IsZora()) {
        return;
    }
    Z64Strength_ResolvePrevent(should, Z64STRENGTH_OFFSET_ZORA);
}

static void GrantZoraSwim(bool* should, va_list args) {
    if (IsZora()) {
        *should = true;
    }
}

// Runs before Player_UpdateCommon consumes the hit, after CollisionCheck_Damage wrote it.
static void ScaleFireDamage(Player* player) {
    u8 damage = player->actor.colChkInfo.damage;

    if (damage == 0) {
        sScaledDamage = 0;
        return;
    }
    if (damage == sScaledDamage || !(player->cylinder.base.acFlags & AC_HIT) ||
        player->cylinder.info.acHitInfo == NULL || player->cylinder.info.acHitInfo->toucher.effect != 1) {
        return;
    }
    sScaledDamage = (u8)CLAMP((damage * 3 + 1) / 2, 1, 0xFF);
    player->actor.colChkInfo.damage = sScaledDamage;
}

static void FilterZoraInput(Player* player, Input* input) {
    if (!IsZora()) {
        if (gPlayState != NULL) {
            HumanGyorgSwim(gPlayState, player, false);
        }
        if (sBorrowedGyorgSwim && player->currentBoots != PLAYER_BOOTS_IRON && IsInWater(player) &&
            sAnims[ANIM_WATER_ROLL] != NULL && player->interactRangeActor == NULL &&
            (!IsGrounded(player) || !Player_IsZTargeting(player)) &&
            player->actor.yDistToWater > player->ageProperties->unk_24) {
            if (!IsSwimMove() && CHECK_BTN_ALL(input->press.button, BTN_A)) {
                StartFastSwim(gPlayState, player, 4.0f);
            }
            input->press.button &= ~BTN_A;
            input->cur.button &= ~BTN_A;
        }
        return;
    }
    ScaleFireDamage(player);
    if (!IsInWater(player) || player->interactRangeActor != NULL) {
        return;
    }
    u16 taken = IsGrounded(player) ? (Player_IsZTargeting(player) ? 0 : BTN_A) : (BTN_A | BTN_B);
    input->press.button &= ~taken;
    input->cur.button &= ~taken;
}

static void StartTrail(PlayState* play) {
    EffectBlureInit2 blure = {
        0, 4, 0, { 255, 255, 255, 255 }, { 255, 255, 255, 64 }, { 255, 255, 255, 0 }, { 255, 255, 255, 0 }, 4,
        0, 2, 0, { 255, 255, 255, 255 }, { 255, 255, 255, 64 }, TRAIL_TYPE_SWORDS,
    };

    if (sTrail < 0) {
        Effect_Add(play, &sTrail, EFFECT_BLURE2, 0, 0, &blure);
    }
}

static void StopTrail(PlayState* play) {
    if (sTrail >= 0) {
        Effect_Delete(play, sTrail);
        sTrail = -1;
    }
}

static void StartSwimTrails(PlayState* play) {
    EffectBlureInit2 blure = {
        0, 8, 0, { 255, 255, 255, 255 }, { 255, 255, 255, 64 }, { 255, 255, 255, 0 }, { 255, 255, 255, 0 }, 4,
        0, 2, 0, { 0, 0, 0, 0 },         { 0, 0, 0, 0 },        TRAIL_TYPE_SWORDS,
    };

    for (s32 arm = 0; arm < 2; arm++) {
        sSwimFinInfo[arm].active = 0;
        if (sSwimTrail[arm] < 0) {
            Effect_Add(play, &sSwimTrail[arm], EFFECT_BLURE2, 0, 0, &blure);
        }
    }
}

static void StopSwimTrails(PlayState* play) {
    for (s32 arm = 0; arm < 2; arm++) {
        if (sSwimTrail[arm] >= 0) {
            Effect_Delete(play, sSwimTrail[arm]);
            sSwimTrail[arm] = -1;
        }
    }
}

static void EndMove(PlayState* play, Player* player) {
    player->meleeWeaponState = 0;
    player->stateFlags1 &= ~(PLAYER_STATE1_SHIELDING | PLAYER_STATE1_JUMPING);
    sMove = ZORA_MOVE_NONE;
    StopTrail(play);
    func_80839FFC(player, play);
}

static void ArmStrike(PlayState* play, Player* player, u32 dmgFlags, u8 damage) {
    for (s32 quad = 0; quad < 2; quad++) {
        func_80837918(player, quad, dmgFlags);
        player->meleeWeaponQuads[quad].info.toucher.damage = damage;
    }
    StartTrail(play);
}

static void PlaceStrike(PlayState* play, Player* player, Vec3f* tips, Vec3f* bases) {
    Vec3f tip;
    Vec3f base;

    Matrix_MultVec3f(&tips[0], &tip);
    Matrix_MultVec3f(&bases[0], &base);
    if (func_80090480(play, NULL, &player->meleeWeaponInfo[0], &tip, &base) && sTrail >= 0) {
        EffectBlure_AddVertex(Effect_GetByIndex(sTrail), &player->meleeWeaponInfo[0].tip,
                              &player->meleeWeaponInfo[0].base);
    }
    for (s32 quad = 0; quad < 2; quad++) {
        player->meleeWeaponQuads[quad].base.atFlags |= AT_ON;
        Matrix_MultVec3f(&tips[quad + 1], &tip);
        Matrix_MultVec3f(&bases[quad + 1], &base);
        func_80090480(play, &player->meleeWeaponQuads[quad], &player->meleeWeaponInfo[quad + 1], &tip, &base);
    }
}

static void DisarmStrike(Player* player) {
    player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
    player->meleeWeaponQuads[1].base.atFlags &= ~AT_ON;
    player->meleeWeaponInfo[1].active = 0;
    player->meleeWeaponInfo[2].active = 0;
}

static void StartFins(PlayState* play, Player* player);

static void StartPunchStep(PlayState* play, Player* player, s32 step) {
    sPunchStep = step;
    sPunchQueued = false;
    player->meleeWeaponState = 0;
    PlayAnim(play, player, ANIM_PUNCH_A + step, 0.0f, ANIMMODE_ONCE, -3.0f);
    if (step == PUNCH_STEPS - 1) {
        PlayMmSfx(MM_GORON_PUNCH_SWING, NA_SE_IT_SWORD_SWING_HARD, &player->actor.projectedPos);
    } else {
        Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_SWING_HARD);
    }
    RefreshMmSfx(step == PUNCH_STEPS - 1 ? MM_VO_ZORA_SWORD_L : MM_VO_ZORA_SWORD_N, &player->actor.projectedPos);
}

static void StartPunchRecovery(PlayState* play, Player* player) {
    s32 slot = (Player_IsZTargeting(player) ? ANIM_PUNCH_A_END_LOCKED : ANIM_PUNCH_A_END) + sPunchStep;

    player->meleeWeaponState = 0;
    sMove = ZORA_MOVE_PUNCH_END;
    StopTrail(play);
    if (sAnims[slot] == NULL) {
        EndMove(play, player);
        return;
    }
    PlayAnim(play, player, slot, 0.0f, ANIMMODE_ONCE, -3.0f);
}

static void ZoraStrikeAction(Player* player, PlayState* play);

static bool IsFinHoldDone(Input* input) {
    if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
        sHoldFrames = 0;
    } else if (CHECK_BTN_ALL(input->cur.button, BTN_B)) {
        sHoldFrames++;
    } else {
        sHoldFrames = 0;
    }
    return sPunchStep == 0 && sHoldFrames >= BOOMERANG_HOLD_FRAMES && !sFinsOut && sAnims[ANIM_FINS_READY] != NULL;
}

static void UpdatePunch(PlayState* play, Player* player, Input* input, bool finished) {
    f32 frame = player->skelAnime.curFrame;

    if (IsFinHoldDone(input)) {
        StartFins(play, player);
        return;
    }
    if (sMove == ZORA_MOVE_PUNCH_END) {
        if (finished) {
            EndMove(play, player);
        }
        return;
    }
    player->meleeWeaponState = (frame >= sPunchHitStart[sPunchStep] && frame <= sPunchHitEnd[sPunchStep]) ? 1 : 0;
    if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
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

static void UpdateJumpKick(PlayState* play, Player* player, bool finished) {
    if (sMove == ZORA_MOVE_KICK_LAND) {
        Math_StepToF(&player->linearVelocity, 0.0f, 1.0f);
        if (finished) {
            EndMove(play, player);
        }
        return;
    }
    player->meleeWeaponState = 1;
    player->actor.gravity = KICK_GRAVITY;
    if (IsGrounded(player) && player->actor.velocity.y <= 0.0f) {
        player->meleeWeaponState = 0;
        player->stateFlags1 &= ~PLAYER_STATE1_JUMPING;
        player->actor.gravity = GRAVITY_NORMAL;
        StopTrail(play);
        sMove = ZORA_MOVE_KICK_LAND;
        if (sAnims[ANIM_JUMP_KICK_END] == NULL) {
            EndMove(play, player);
            return;
        }
        PlayAnim(play, player, ANIM_JUMP_KICK_END, 0.0f, ANIMMODE_ONCE, -3.0f);
    }
}

static void ZoraStrikeAction(Player* player, PlayState* play) {
    Input* input = &play->state.input[0];

    if (func_80842DF4(play, player)) {
        sMove = ZORA_MOVE_NONE;
        player->meleeWeaponState = 0;
        StopTrail(play);
        return;
    }
    bool finished = LinkAnimation_Update(play, &player->skelAnime);
    if (sMove == ZORA_MOVE_JUMP_KICK || sMove == ZORA_MOVE_KICK_LAND) {
        UpdateJumpKick(play, player, finished);
        return;
    }
    player->linearVelocity = 0.0f;
    UpdatePunch(play, player, input, finished);
}

static bool IsZoraBusy(Player* player);

static int32_t StartPunch(PlayState* play, Player* player) {
    if (!CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    if (sAnims[ANIM_PUNCH_A] == NULL || sMove != ZORA_MOVE_NONE || IsZoraBusy(player) || !CanAct(player) ||
        !IsGrounded(player)) {
        return SOH_FORM_ACTION_BLOCKED;
    }
    sMove = ZORA_MOVE_PUNCH;
    sHoldFrames = 0;
    ArmStrike(play, player, PUNCH_DMG_FLAGS, PUNCH_DAMAGE);
    Player_SetupAction(play, player, ZoraStrikeAction, 0);
    StartPunchStep(play, player, 0);
    return SOH_FORM_ACTION_STARTED;
}

static void StartJumpKick(PlayState* play, Player* player, bool fromGround) {
    s16 yaw = player->actor.shape.rot.y;

    if (player->focusActor != NULL && player->focusActor->update != NULL) {
        yaw = Math_Vec3f_Yaw(&player->actor.world.pos, &player->focusActor->world.pos);
    }
    sMove = ZORA_MOVE_JUMP_KICK;
    ArmStrike(play, player, KICK_DMG_FLAGS, KICK_DAMAGE);
    Player_SetupAction(play, player, ZoraStrikeAction, 0);
    // Without MIDAIR vanilla reads a strike in the air as stepping off a ledge and pins Link to prevPos.
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
    player->stateFlags1 |= PLAYER_STATE1_JUMPING;
    player->yaw = yaw;
    player->actor.shape.rot.y = yaw;
    player->actor.world.rot.y = yaw;
    if (fromGround) {
        player->linearVelocity = KICK_SPEED;
        player->actor.velocity.y = KICK_LIFT;
        player->actor.bgCheckFlags &= ~BG_ON_GROUND;
    }
    player->actor.gravity = KICK_GRAVITY;
    PlayAnim(play, player, ANIM_JUMP_KICK, 0.0f, ANIMMODE_ONCE, -3.0f);
    Audio_PlayActorSound2(&player->actor, NA_SE_PL_JUMP);
    RefreshMmSfx(MM_VO_ZORA_SWORD_N, &player->actor.projectedPos);
}

static int32_t HandleZTargetA(PlayState* play, Player* player) {
    const s8 direction = player->controlStickDirections[player->controlStickDataIndex];

    if (direction > PLAYER_STICK_DIR_FORWARD || !CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    if (sAnims[ANIM_JUMP_KICK] == NULL || IsZoraBusy(player) || !CanAct(player) || !IsGrounded(player) ||
        IsInWater(player)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    StartJumpKick(play, player, true);
    return SOH_FORM_ACTION_STARTED;
}

static void TryAirKick(PlayState* play, Player* player) {
    if (IsZoraBusy(player) || IsGrounded(player) || IsInWater(player) || !CanAct(player) ||
        sAnims[ANIM_JUMP_KICK] == NULL || !(player->stateFlags1 & PLAYER_STATE1_JUMPING) ||
        !CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B)) {
        return;
    }
    StartJumpKick(play, player, false);
}

static void DrawStrikeLimb(PlayState* play, Player* player, int32_t limbIndex) {
    bool isKick = sMove == ZORA_MOVE_JUMP_KICK || (sMove == ZORA_MOVE_PUNCH && sPunchStep == PUNCH_STEPS - 1);
    int32_t strikingLimb = isKick ? PLAYER_LIMB_R_SHIN : sPunchLimb[sPunchStep];

    if (!IsStriking() || limbIndex != strikingLimb) {
        return;
    }
    if (player->meleeWeaponState == 0) {
        DisarmStrike(player);
        return;
    }
    PlaceStrike(play, player, isKick ? sKickTips : sFinTips, isKick ? sKickBases : sFinBases);
}

static bool IsFinAlive(PlayState* play, Actor* fin) {
    if (fin == NULL) {
        return false;
    }
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_MISC].head; actor != NULL; actor = actor->next) {
        if (actor == fin) {
            return actor->update != NULL;
        }
    }
    return false;
}

static void ThrowFins(PlayState* play, Player* player, s16 yaw, s16 pitch, Actor* target) {
    s16 spread = target != NULL ? BOOMERANG_SPREAD_LOCKED : -BOOMERANG_SPREAD_FREE;
    static const s32 sHands[2] = { PLAYER_BODYPART_L_HAND, PLAYER_BODYPART_R_HAND };

    for (s32 fin = 0; fin < 2; fin++) {
        Vec3f* hand = &player->bodyPartsPos[sHands[fin]];
        EnBoom* boom = (EnBoom*)Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOOM, hand->x, hand->y, hand->z, pitch,
                                            yaw + (fin == 0 ? spread : -spread), 0, 0);

        sFins[fin] = boom != NULL ? &boom->actor : NULL;
        if (boom != NULL) {
            boom->moveTo = target;
            boom->returnTimer = BOOMERANG_RETURN_TIMER;
        }
    }
    sFinsOut = sFins[0] != NULL || sFins[1] != NULL;
    player->boomerangActor = sFins[0];
    PlayMmSfx(MM_BOOMERANG_THROW, NA_SE_IT_BOOMERANG_THROW, &player->actor.projectedPos);
    RefreshMmSfx(MM_VO_ZORA_SWORD_N, &player->actor.projectedPos);
}

static Actor* GetLockOnTarget(Player* player) {
    Actor* target = player->focusActor;

    return Player_IsZTargeting(player) && target != NULL && target->update != NULL ? target : NULL;
}

static void PoseArms(PlayState* play, Player* player, LinkAnimationHeader* anim, s32 frame) {
    static const s32 sArmLimbs[] = { PLAYER_LIMB_L_SHOULDER, PLAYER_LIMB_L_FOREARM, PLAYER_LIMB_L_HAND,
                                     PLAYER_LIMB_R_SHOULDER, PLAYER_LIMB_R_FOREARM, PLAYER_LIMB_R_HAND };
    Vec3s pose[PLAYER_LIMB_MAX + 1];

    if (anim == NULL) {
        return;
    }
    AnimationContext_SetLoadFrame(play, anim, frame, PLAYER_LIMB_MAX, pose);
    for (u32 i = 0; i < ARRAY_COUNT(sArmLimbs); i++) {
        player->skelAnime.jointTable[sArmLimbs[i]] = pose[sArmLimbs[i]];
    }
}

static void StartLockedFins(PlayState* play, Player* player) {
    sMove = ZORA_MOVE_FINS_LOCKED_READY;
    sFinFrame = 0;
    if (IsZoraBusy(player)) {
        func_80839FFC(player, play);
    }
}

static void ZoraFinsAction(Player* player, PlayState* play) {
    bool finished = LinkAnimation_Update(play, &player->skelAnime);

    Player_ZeroSpeedXZ(player);
    if (sMove == ZORA_MOVE_FINS_READY) {
        Z64Aiming_Update(player, play);
        if (Player_IsZTargeting(player)) {
            Z64Aiming_Release(player, play);
            StartLockedFins(play, player);
            return;
        }
        if (!CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_B)) {
            Z64Aiming_GetDirection(player, &sThrowYaw, &sThrowPitch);
            Z64Aiming_Release(player, play);
            player->actor.shape.rot.y = player->yaw = sThrowYaw;
            sMove = ZORA_MOVE_FINS_THROW;
            PlayAnim(play, player, ANIM_FINS_THROW, 0.0f, ANIMMODE_ONCE, -2.0f);
        }
        return;
    }
    if (!sFinsOut && LinkAnimation_OnFrame(&player->skelAnime, BOOMERANG_THROW_FRAME)) {
        ThrowFins(play, player, sThrowYaw, sThrowPitch, NULL);
    }
    if (finished) {
        EndMove(play, player);
    }
}

static void StartAimedFins(PlayState* play, Player* player) {
    Z64Aiming_Request(player, play);
    sMove = ZORA_MOVE_FINS_READY;
    Player_SetupAction(play, player, ZoraFinsAction, 0);
    PlayAnim(play, player, ANIM_FINS_READY, 0.0f, ANIMMODE_LOOP, -3.0f);
}

static void StartFins(PlayState* play, Player* player) {
    player->meleeWeaponState = 0;
    DisarmStrike(player);
    StopTrail(play);
    if (Player_IsZTargeting(player)) {
        StartLockedFins(play, player);
        return;
    }
    StartAimedFins(play, player);
}

static void ThrowLockedFins(PlayState* play, Player* player) {
    Actor* target = GetLockOnTarget(player);
    s16 yaw = player->actor.shape.rot.y;
    s16 pitch = 0;

    if (target != NULL) {
        yaw = Math_Vec3f_Yaw(&player->actor.focus.pos, &target->focus.pos);
        pitch = Math_Vec3f_Pitch(&player->actor.focus.pos, &target->focus.pos);
    }
    ThrowFins(play, player, yaw, pitch, target);
}

static void UpdateLockedFins(PlayState* play, Player* player) {
    LinkAnimationHeader* ready = sAnims[ANIM_FINS_READY];
    LinkAnimationHeader* release = sAnims[ANIM_FINS_THROW];

    if (sMove != ZORA_MOVE_FINS_LOCKED_READY && sMove != ZORA_MOVE_FINS_LOCKED_THROW) {
        return;
    }
    if (!CanAct(player) || IsZoraBusy(player) || IsInWater(player) || ready == NULL || release == NULL) {
        sMove = ZORA_MOVE_NONE;
        return;
    }
    bool isHeld = CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_B);
    if (sMove == ZORA_MOVE_FINS_LOCKED_READY) {
        if (isHeld && !Player_IsZTargeting(player) && IsGrounded(player)) {
            StartAimedFins(play, player);
            return;
        }
        if (isHeld) {
            PoseArms(play, player, ready, (s32)sFinFrame);
            sFinFrame = fmodf(sFinFrame + CLIP_FRAMES_PER_UPDATE, ready->common.frameCount);
            return;
        }
        sMove = ZORA_MOVE_FINS_LOCKED_THROW;
        sFinFrame = 0;
    }
    PoseArms(play, player, release, (s32)sFinFrame);
    if (!sFinsOut && sFinFrame >= BOOMERANG_THROW_FRAME) {
        ThrowLockedFins(play, player);
    }
    sFinFrame += CLIP_FRAMES_PER_UPDATE;
    if (sFinFrame >= release->common.frameCount) {
        sMove = ZORA_MOVE_NONE;
    }
}

static void TrackFins(PlayState* play, Player* player) {
    if (!sFinsOut) {
        return;
    }
    bool anyAlive = false;
    for (s32 fin = 0; fin < 2; fin++) {
        if (!IsFinAlive(play, sFins[fin])) {
            sFins[fin] = NULL;
        } else {
            anyAlive = true;
        }
    }
    if (anyAlive) {
        return;
    }
    sFinsOut = false;
    player->boomerangActor = NULL;
    PlayMmSfx(MM_CATCH_BOOMERANG, NA_SE_PL_CATCH_BOOMERANG, &player->actor.projectedPos);
    RefreshMmSfx(MM_VO_ZORA_SWORD_N, &player->actor.projectedPos);
}

static void DrawFlyingFin(Actor* actor, PlayState* play, bool* drawVanilla) {
    s32 fin = actor == sFins[0] ? 0 : (actor == sFins[1] ? 1 : -1);
    EnBoom* boom = (EnBoom*)actor;
    Vec3f tip = { -960.0f, 0.0f, 0.0f };
    Vec3f base = { 960.0f, 0.0f, 0.0f };
    Vec3f tipWorld;
    Vec3f baseWorld;

    if (fin < 0) {
        return;
    }
    *drawVanilla = false;
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_RotateY(BINANG_TO_RAD(actor->world.rot.y), MTXMODE_APPLY);
    Matrix_RotateZ(BINANG_TO_RAD(fin == 0 ? -FIN_TILT : FIN_TILT), MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD(actor->world.rot.x), MTXMODE_APPLY);
    Matrix_MultVec3f(&tip, &tipWorld);
    Matrix_MultVec3f(&base, &baseWorld);
    if (func_80090480(play, &boom->collider, &boom->boomerangInfo, &tipWorld, &baseWorld)) {
        EffectBlure_AddVertex(Effect_GetByIndex(boom->effectIndex), &tipWorld, &baseWorld);
    }
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_RotateY(BINANG_TO_RAD((s16)(boom->activeTimer * FIN_SPIN_STEP)), MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)(fin == 0 ? sLeftFlyingFinDL : sRightFlyingFinDL));
    CLOSE_DISPS(play->state.gfxCtx);
}

static void PlantGuard(PlayState* play, Player* player) {
    s16 yaw = player->actor.shape.rot.y;
    f32 sinYaw = Math_SinS(yaw);
    f32 cosYaw = Math_CosS(yaw);
    f32 centerX = player->actor.world.pos.x + sinYaw * GUARD_FRONT;
    f32 centerZ = player->actor.world.pos.z + cosYaw * GUARD_FRONT;
    f32 floorY = player->actor.world.pos.y;
    Vec3f topLeft = { centerX - cosYaw * GUARD_HALF_WIDTH, floorY + GUARD_TOP, centerZ + sinYaw * GUARD_HALF_WIDTH };
    Vec3f topRight = { centerX + cosYaw * GUARD_HALF_WIDTH, floorY + GUARD_TOP, centerZ - sinYaw * GUARD_HALF_WIDTH };
    Vec3f bottomRight = { topRight.x, floorY + GUARD_BOTTOM, topRight.z };
    Vec3f bottomLeft = { topLeft.x, floorY + GUARD_BOTTOM, topLeft.z };

    SkinMatrix_SetTranslateRotateYXZScale(&player->shieldMf, 1.0f, 1.0f, 1.0f, 0, yaw + 0x8000, 0, 0.0f, 0.0f, 0.0f);
    player->shieldQuad.base.colType = COLTYPE_METAL;
    Collider_ResetQuadAC(play, &player->shieldQuad.base);
    Collider_SetQuadVertices(&player->shieldQuad, &topLeft, &topRight, &bottomRight, &bottomLeft);
    CollisionCheck_SetAC(play, &play->colChkCtx, &player->shieldQuad.base);
    CollisionCheck_SetAT(play, &play->colChkCtx, &player->shieldQuad.base);
}

static void ZoraGuardAction(Player* player, PlayState* play) {
    LinkAnimation_Update(play, &player->skelAnime);
    Math_StepToF(&player->linearVelocity, 0.0f, 2.0f);
    player->yaw = sGuardYaw;
    player->actor.shape.rot.y = sGuardYaw;
    player->stateFlags1 |= PLAYER_STATE1_SHIELDING;
    sGuardFrames++;
    if (!CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_R) || !CanAct(player)) {
        PlayMmSfx(MM_SHIELD_REMOVE_ZORA, NA_SE_IT_SHIELD_REMOVE, &player->actor.projectedPos);
        EndMove(play, player);
    }
}

static bool IsGuarding(void) {
    return sMove == ZORA_MOVE_GUARD || sMove == ZORA_MOVE_GUARD_WALK;
}

static int32_t StartGuard(PlayState* play, Player* player) {
    LinkAnimationHeader* defense = (LinkAnimationHeader*)gPlayerAnim_link_normal_defense;

    if (!CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_R) || Player_IsZTargeting(player)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    if (IsZoraBusy(player) || !CanAct(player) || !IsGrounded(player)) {
        return SOH_FORM_ACTION_BLOCKED;
    }
    sMove = ZORA_MOVE_GUARD;
    sGuardFrames = 0;
    sGuardYaw = player->actor.shape.rot.y;
    Player_SetupAction(play, player, ZoraGuardAction, 0);
    f32 last = Animation_GetLastFrame(defense);
    LinkAnimation_Change(play, &player->skelAnime, defense, 1.0f, last, last, ANIMMODE_ONCE, 0.0f);
    player->upperLimbRot.x = player->upperLimbRot.y = player->upperLimbRot.z = 0;
    PlayMmSfx(MM_SHIELD_SWING, NA_SE_IT_SHIELD_POSTURE, &player->actor.projectedPos);
    return SOH_FORM_ACTION_STARTED;
}

static void UpdateGuardWalk(PlayState* play, Player* player) {
    bool wantsGuard = (sMove == ZORA_MOVE_NONE || sMove == ZORA_MOVE_GUARD_WALK) && !IsZoraBusy(player) &&
                      Player_IsZTargeting(player) && CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_R) &&
                      CanAct(player) && IsGrounded(player) && !IsInWater(player);

    if (!wantsGuard) {
        if (sMove == ZORA_MOVE_GUARD_WALK) {
            sMove = ZORA_MOVE_NONE;
            PlayMmSfx(MM_SHIELD_REMOVE_ZORA, NA_SE_IT_SHIELD_REMOVE, &player->actor.projectedPos);
        }
        return;
    }
    if (sMove != ZORA_MOVE_GUARD_WALK) {
        sMove = ZORA_MOVE_GUARD_WALK;
        sGuardFrames = 0;
        PlayMmSfx(MM_SHIELD_SWING, NA_SE_IT_SHIELD_POSTURE, &player->actor.projectedPos);
    }
    sGuardFrames++;
    LinkAnimationHeader* pose = (LinkAnimationHeader*)(player->unk_870 < 0.5f ? gPlayerAnim_link_anchor_waitR2defense
                                                                              : gPlayerAnim_link_anchor_waitL2defense);
    PoseArms(play, player, pose, (s32)Animation_GetLastFrame(pose));
    PlantGuard(play, player);
}

static void GetFinScale(Vec3f* scale) {
    static const Vec3f sKeys[] = {
        { 0.40f, 0.60f, 0.70f }, { 0.75f, 0.90f, 0.85f }, { 1.10f, 1.20f, 1.00f }, { 1.00f, 1.00f, 1.00f }
    };
    f32 t = sGuardFrames;

    if (!IsGuarding()) {
        bool isOut = IsStriking() || sMove == ZORA_MOVE_FINS_READY || sMove == ZORA_MOVE_FINS_THROW ||
                     sMove == ZORA_MOVE_FINS_LOCKED_READY || sMove == ZORA_MOVE_FINS_LOCKED_THROW;
        *scale = isOut ? sKeys[3] : sKeys[0];
        return;
    }
    if (t <= 3.0f) {
        *scale = sKeys[0];
    } else if (t < 5.0f) {
        s32 key = t < 4.0f ? 0 : 1;
        f32 blend = t - (key == 0 ? 3.0f : 4.0f);
        scale->x = sKeys[key].x + (sKeys[key + 1].x - sKeys[key].x) * blend;
        scale->y = sKeys[key].y + (sKeys[key + 1].y - sKeys[key].y) * blend;
        scale->z = sKeys[key].z + (sKeys[key + 1].z - sKeys[key].z) * blend;
    } else if (t < 7.0f) {
        f32 blend = (t - 5.0f) / 2.0f;
        scale->x = sKeys[2].x + (sKeys[3].x - sKeys[2].x) * blend;
        scale->y = sKeys[2].y + (sKeys[3].y - sKeys[2].y) * blend;
        scale->z = 1.0f;
    } else {
        *scale = sKeys[3];
    }
}

typedef struct {
    s16 frame;
    f32 scale;
} FinKey;

static f32 InterpFinKeys(const FinKey* keys, s32 keyCount, f32 frame) {
    for (s32 i = 1; i < keyCount; i++) {
        if (frame <= keys[i].frame) {
            f32 progress = (frame - keys[i - 1].frame) / (f32)(keys[i].frame - keys[i - 1].frame);
            return keys[i - 1].scale + (keys[i].scale - keys[i - 1].scale) * progress;
        }
    }
    return keys[keyCount - 1].scale;
}

static bool IsPlaying(Player* player, s32 slot) {
    return sAnims[slot] != NULL && player->skelAnime.animation == sAnims[slot];
}

static bool GetDashBladeScale(Player* player, f32* scale) {
    static const FinKey sFold[] = { { 0, 1.0f }, { 6, 1.0f }, { 7, 0.0f }, { 17, 0.0f } };
    static const FinKey sReturn[] = { { 0, 0.0f }, { 5, 0.0f }, { 9, 1.0f } };

    if (IsPlaying(player, ANIM_WATER_ROLL)) {
        *scale = InterpFinKeys(sFold, ARRAY_COUNT(sFold), player->skelAnime.curFrame);
        return true;
    }
    if (IsPlaying(player, ANIM_SWIM_TO_WAIT)) {
        *scale = InterpFinKeys(sReturn, ARRAY_COUNT(sReturn), player->skelAnime.curFrame);
        return true;
    }
    return false;
}

static bool GetSwimFinScale(Player* player, f32* scale) {
    static const FinKey sGrow[] = { { 0, 0.0f }, { 17, 0.5f } };

    if (IsPlaying(player, ANIM_WATER_ROLL)) {
        *scale = InterpFinKeys(sGrow, ARRAY_COUNT(sGrow), player->skelAnime.curFrame);
        sSwimFinFoldFrom = 0.5f;
        return true;
    }
    if (IsPlaying(player, ANIM_FISH_SWIM)) {
        *scale = ABS(sSwimTurn) * 0.00003f + 0.5f;
        sSwimFinFoldFrom = *scale;
        return true;
    }
    if (IsPlaying(player, ANIM_SWIM_TO_WAIT)) {
        const FinKey fold[] = { { 0, sSwimFinFoldFrom }, { 5, sSwimFinFoldFrom }, { 9, 0.0f } };
        *scale = InterpFinKeys(fold, ARRAY_COUNT(fold), player->skelAnime.curFrame);
        return true;
    }
    return false;
}

static void DrawSwimFin(PlayState* play, Player* player, s32 arm) {
    static Vec3f sTrailTips[2] = { { 5400.0f, 1700.0f, 1800.0f }, { 5400.0f, 1700.0f, -1800.0f } };
    static Vec3f sTrailBases[2] = { { 5250.0f, 570.0f, 2400.0f }, { 5250.0f, 570.0f, -2400.0f } };
    Vec3f tip;
    Vec3f base;
    f32 scale;

    if (!GetSwimFinScale(player, &scale)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)(arm == 0 ? sLeftSwimFinDL : sRightSwimFinDL));
    Matrix_MultVec3f(&sTrailTips[arm], &tip);
    Matrix_MultVec3f(&sTrailBases[arm], &base);
    if (func_80090480(play, NULL, &sSwimFinInfo[arm], &tip, &base) && IsInWater(player) && sSwimTrail[arm] >= 0) {
        EffectBlure_AddVertex(Effect_GetByIndex(sSwimTrail[arm]), &sSwimFinInfo[arm].tip, &sSwimFinInfo[arm].base);
    }
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawForearmFin(PlayState* play, Player* player, int32_t limbIndex) {
    bool isGuardFin = limbIndex == PLAYER_LIMB_R_FOREARM && IsGuarding();
    Vec3f scale = { 1.0f, 1.0f, 1.0f };
    f32 dashScale;

    if (sFinsOut) {
        return;
    }
    DrawSwimFin(play, player, limbIndex == PLAYER_LIMB_L_FOREARM ? 0 : 1);
    if (IsPlaying(player, ANIM_FISH_SWIM)) {
        return;
    }
    if (GetDashBladeScale(player, &dashScale)) {
        scale.x = scale.y = scale.z = dashScale;
    } else if (!isGuardFin) {
        GetFinScale(&scale);
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Scale(scale.x, scale.y, scale.z, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    if (isGuardFin) {
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGuardFinDL);
    } else {
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)(limbIndex == PLAYER_LIMB_L_FOREARM ? sLeftFinDL : sRightFinDL));
    }
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void SetHeavy(PlayState* play, Player* player, bool heavy) {
    sIsHeavy = heavy;
    player->currentBoots = heavy ? PLAYER_BOOTS_IRON : PLAYER_BOOTS_KOKIRI;
    Player_SetBootData(play, player);
}

static void KeepWeight(PlayState* play, Player* player) {
    if (!IsInWater(player) && sIsHeavy && !IsSwimMove()) {
        SetHeavy(play, player, false);
        return;
    }
    PlayerBoots wanted = sIsHeavy ? PLAYER_BOOTS_IRON : PLAYER_BOOTS_KOKIRI;
    if (player->currentBoots != wanted) {
        player->currentBoots = wanted;
        Player_SetBootData(play, player);
    }
}

static void ApplyBuoyancy(Player* player) {
    f32 terminal = -5.0f;
    f32 restDepth = sBorrowedGyorgSwim ? player->ageProperties->unk_28 : SWIM_BUOYANCY_DEPTH;
    f32 buoyancyDepth = restDepth + (player->actor.velocity.y < 0.0f ? 1.0f : 0.0f);
    f32 depthDelta = player->actor.yDistToWater - restDepth;
    f32 push;

    if (player->actor.yDistToWater < buoyancyDepth) {
        push = CLAMP(depthDelta, -0.4f, -0.1f) -
               (player->actor.velocity.y <= 0.0f ? 0.0f : player->actor.velocity.y * 0.5f);
    } else if (sIsHeavy && player->actor.velocity.y >= -5.0f) {
        push = -0.3f;
    } else {
        terminal = 2.0f;
        push = (player->actor.velocity.y >= 0.0f ? 0.0f : player->actor.velocity.y * -0.3f) +
               CLAMP(depthDelta, 0.1f, 0.4f);
    }
    player->actor.velocity.y += push;
    if ((player->actor.velocity.y - terminal) * push > 0.0f) {
        player->actor.velocity.y = terminal;
    }
    player->actor.gravity = SWIM_GRAVITY;
}

// OoT's func_8083D53C lifts a dashing Zora out of shallow water at unk_24, ending dashes before they can breach.
static void SetWaterExitDepth(f32 depth) {
    sWaterExitDepth = depth;
    sZoraBody.unk_24 = depth;
}

// func_8083D53C re-seats vanilla's swim idle every frame, so the dash runs in its place via
// VB_EXECUTE_PLAYER_ACTION_FUNC.
static void StartFastSwim(PlayState* play, Player* player, f32 fromFrame) {
    if (sIsHeavy) {
        SetHeavy(play, player, false);
    }
    sMove = ZORA_MOVE_FAST_SWIM;
    SetWaterExitDepth(DASH_EXIT_DEPTH);
    sSwimPhase = SWIM_PHASE_ROLL;
    sRollHoldFrames = ROLL_HOLD_FRAMES;
    sIsDashing = false;
    sSwimSpeed = player->linearVelocity;
    sSwimRoll = 0;
    sSwimRollSmoothed = 0;
    sSwimTurn = 0;
    sSwimExitQueued = false;
    StartSwimTrails(play);
    player->actor.velocity.y = 0.0f;
    func_80838F18(play, player);
    player->stateFlags1 |= PLAYER_STATE1_IN_WATER;
    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    PlayAnim(play, player, ANIM_WATER_ROLL, fromFrame, ANIMMODE_ONCE, -6.0f);
    PlayMmSfx(MM_ZORA_SWIM_DASH, NA_SE_PL_DIVE_BUBBLE, &player->actor.projectedPos);
}

static void LeaveSwimMove(PlayState* play, Player* player) {
    StopMmSfx(MM_ZORA_SWIM_LV);
    StopSwimTrails(play);
    sMove = ZORA_MOVE_NONE;
    SetWaterExitDepth(SWIM_EXIT_DEPTH);
    sIsDashing = false;
    sSwimAnim = NULL;
    sSwimPitch = 0;
    sSwimRoll = 0;
    sSwimRollSmoothed = 0;
    player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    player->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
    player->actor.gravity = GRAVITY_NORMAL;
    if (sBorrowedGyorgSwim) {
        player->actor.shape.rot.x = 0;
        player->skelAnime.playSpeed = 1.0f;
    }
}

static void ReturnToSwimming(PlayState* play, Player* player) {
    LeaveSwimMove(play, player);
    func_80838F18(play, player);
}

static void KeepDolphinArc(Player* player) {
    f32 nextDepth = player->actor.yDistToWater - player->actor.velocity.y;

    if (nextDepth > DOLPHIN_WATERLINE) {
        player->stateFlags1 |= PLAYER_STATE1_IN_WATER;
        player->stateFlags1 &= ~PLAYER_STATE1_JUMPING;
    } else {
        player->stateFlags1 &= ~PLAYER_STATE1_IN_WATER;
        player->stateFlags1 |= PLAYER_STATE1_JUMPING;
    }
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
}

static bool TryDolphinJump(PlayState* play, Player* player) {
    if (!sIsDashing || sSwimPitch >= DOLPHIN_PITCH ||
        player->actor.yDistToWater - player->actor.velocity.y >= SWIM_DEEP_DEPTH) {
        return false;
    }
    f32 launch = MIN(sSwimSpeed * 1.5f, DOLPHIN_MAX_SPEED);
    sMove = ZORA_MOVE_DOLPHIN;
    SetWaterExitDepth(SWIM_EXIT_DEPTH);
    player->linearVelocity = Math_CosS(sSwimPitch) * launch;
    player->actor.velocity.y = -Math_SinS(sSwimPitch) * launch;
    player->actor.gravity = SWIM_GRAVITY;
    player->actor.bgCheckFlags &= ~BG_ON_GROUND;
    KeepDolphinArc(player);
    StopMmSfx(MM_ZORA_SWIM_LV);
    Audio_PlayActorSound2(&player->actor, NA_SE_EV_JUMP_OUT_WATER);
    return true;
}

static void DiveBackIntoDash(PlayState* play, Player* player) {
    LinkAnimationHeader* clip = player->skelAnime.animation;
    f32 clipFrame = player->skelAnime.curFrame;
    f32 clipSpeed = player->skelAnime.playSpeed;
    u8 clipMode = player->skelAnime.mode;

    sMove = ZORA_MOVE_FAST_SWIM;
    SetWaterExitDepth(DASH_EXIT_DEPTH);
    sSwimPhase = SWIM_PHASE_CRUISE;
    sSwimSpeed = sqrtf(SQ(player->linearVelocity) + SQ(player->actor.velocity.y));
    player->stateFlags1 &= ~PLAYER_STATE1_JUMPING;
    player->stateFlags1 |= PLAYER_STATE1_IN_WATER;
    player->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
    func_80838F18(play, player);
    LinkAnimation_Change(play, &player->skelAnime, clip, clipSpeed, clipFrame, Animation_GetLastFrame(clip), clipMode,
                         0.0f);
}

static void UpdateDolphin(PlayState* play, Player* player) {
    LinkAnimation_Update(play, &player->skelAnime);
    Math_SmoothStepToS(&sSwimRoll, 0, 6, 0x7D0, 0x190);
    if (player->actor.yDistToWater > SWIM_ENTER_DEPTH && player->actor.velocity.y < DOLPHIN_REENTRY_FALL) {
        DiveBackIntoDash(play, player);
        return;
    }
    if (IsGrounded(player) && player->actor.velocity.y <= 0.0f) {
        if (sSwimPitch > DOLPHIN_STEEP_PITCH) {
            Health_ChangeBy(play, -DOLPHIN_STEEP_DAMAGE);
            Audio_PlayActorSound2(&player->actor, NA_SE_PL_BODY_HIT);
        }
        player->stateFlags1 &= ~PLAYER_STATE1_JUMPING;
        LeaveSwimMove(play, player);
        func_80839FFC(player, play);
        return;
    }
    player->actor.gravity = DOLPHIN_GRAVITY;
    sSwimPitch = Math_Atan2S(player->linearVelocity, -player->actor.velocity.y);
    KeepDolphinArc(player);
}

static s32 SwimStickX(Input* input) {
    return CVarGetInteger(SWIM_INVERT_X_CVAR, 0) ? -input->rel.stick_x : input->rel.stick_x;
}

static s32 SwimStickY(Input* input) {
    return CVarGetInteger(SWIM_INVERT_Y_CVAR, 0) ? -input->rel.stick_y : input->rel.stick_y;
}

static void SteerFastSwim(PlayState* play, Player* player, f32 speedTarget) {
    Input* input = &play->state.input[0];
    s32 stickX = SwimStickX(input);
    f32 curve = 1.0f - Math_CosS((s16)(stickX * 0x10E));
    s16 turn = (s16)CLAMP((stickX >= 0 ? 1 : -1) * curve * -1100.0f, -0x1F40, 0x1F40);

    Math_AsymStepToF(&sSwimSpeed, speedTarget, 1.0f, fabsf(sSwimSpeed) * 0.01f + 0.4f);
    player->yaw += turn;
    player->actor.world.rot.y = player->yaw;
    player->actor.shape.rot.y = player->yaw;
    player->linearVelocity = Math_CosS(sSwimPitch) * sSwimSpeed;
    player->actor.velocity.y = -Math_SinS(sSwimPitch) * sSwimSpeed;
}

static bool CanStartDash(PlayState* play) {
    return CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_A) && !sIsHeavy && sAnims[ANIM_WATER_ROLL] != NULL &&
           HasSwimAbility();
}

static void StirSilt(PlayState* play, Player* player) {
    static Vec3f sAccel = { 0.0f, 0.04f, 0.0f };
    static Color_RGBA8 sWhite = { 255, 255, 255, 255 };
    Vec3f* origin = &player->actor.world.pos;
    Vec3f pos = { origin->x + Rand_CenteredFloat(40.0f), origin->y + Rand_CenteredFloat(10.0f),
                  origin->z + Rand_CenteredFloat(40.0f) };
    Vec3f velocity = { Rand_CenteredFloat(3.0f), 0.0f, Rand_CenteredFloat(3.0f) };

    EffectSsDust_Spawn(play, 0, &pos, &velocity, &sAccel, &sWhite, &sWhite, 160, 20, 42, 0);
}

static void LeaveDash(PlayState* play, Player* player) {
    sSwimPhase = SWIM_PHASE_EXIT;
    sIsDashing = false;
    PlayAnim(play, player, ANIM_SWIM_TO_WAIT, 0.0f, ANIMMODE_ONCE, 0.0f);
    StopMmSfx(MM_ZORA_SWIM_LV);
}

static void RollIntoDash(PlayState* play, Player* player, bool finished, f32* speedTarget) {
    Input* input = &play->state.input[0];

    if (!CHECK_BTN_ALL(input->cur.button, BTN_A)) {
        sSwimExitQueued = true;
    }
    if (finished && --sRollHoldFrames <= 0) {
        if (sSwimExitQueued) {
            LeaveDash(play, player);
        } else {
            sSwimPhase = SWIM_PHASE_CRUISE;
            PlayAnim(play, player, ANIM_FISH_SWIM, 0.0f, ANIMMODE_LOOP, 0.0f);
        }
    } else {
        f32 stickSpeed;
        s16 stickYaw;
        Player_GetMovementSpeedAndYaw(player, &stickSpeed, &stickYaw, SPEED_MODE_LINEAR, play);
        Math_ScaledStepToS(&player->yaw, stickYaw, ROLL_STEER_STEP);
        if (player->skelAnime.curFrame >= FAST_SWIM_KICK_FRAME) {
            *speedTarget = FAST_SWIM_KICK_SPEED;
            if (LinkAnimation_OnFrame(&player->skelAnime, FAST_SWIM_KICK_FRAME)) {
                sSwimSpeed = FAST_SWIM_BURST;
            }
            sIsDashing = true;
        }
    }
    Math_SmoothStepToS(&sSwimRoll, (s16)(SwimStickX(input) * 0xC8), 0xA, 0x3E8, 0x64);
}

static void BankFastSwim(Player* player, s16 turn) {
    s32 rollBefore = sSwimRoll;
    s32 ringAngle = turn < 0 ? -BANK_SFX_ANGLE : BANK_SFX_ANGLE;
    s32 distanceAfter = rollBefore + turn - ringAngle;
    s32 distanceBefore = rollBefore - ringAngle;
    bool isCrossing = (distanceAfter <= 0 && distanceBefore >= 0) || (distanceAfter >= 0 && distanceBefore <= 0);

    sSwimRoll += turn;
    if (ABS(turn) > BANK_SFX_TURN && isCrossing) {
        RefreshMmSfx(MM_ZORA_SWIM_ROLL, &player->actor.projectedPos);
    }
}

static void CruiseFastSwim(PlayState* play, Player* player, f32* speedTarget) {
    Input* input = &play->state.input[0];
    s16 pitchTarget = (s16)(SwimStickY(input) * 0xC8);

    if (!CHECK_BTN_ALL(input->cur.button, BTN_A)) {
        LeaveDash(play, player);
        return;
    }
    *speedTarget = FAST_SWIM_SPEED;
    RefreshMmSfx(MM_ZORA_SWIM_LV, &player->actor.projectedPos);
    if (sSwimFloorTimer != 0) {
        sSwimFloorTimer--;
        pitchTarget = MIN(pitchTarget, (s16)(player->floorPitch - 0xFA0));
    }
    if (sSwimPitch >= DOLPHIN_PITCH && player->actor.yDistToWater < SWIM_SURFACE_DEPTH) {
        pitchTarget = MAX(pitchTarget, 0x7D0);
    }
    Math_SmoothStepToS(&sSwimPitch, pitchTarget, 4, 0xFA0, 0x190);
    s16 rollTarget = (s16)(SwimStickX(input) * 0x64);
    if (Math_ScaledStepToS(&sSwimTurn, rollTarget, 0x384) && rollTarget == 0) {
        Math_SmoothStepToS(&sSwimRoll, 0, 4, 0x5DC, 0x64);
    } else {
        BankFastSwim(player, sSwimTurn);
    }
    if (player->actor.world.pos.y - player->actor.floorHeight < FLOOR_SKIM_HEIGHT) {
        StirSilt(play, player);
    }
}

static bool EaseOutOfDash(PlayState* play, Player* player, bool finished) {
    Math_SmoothStepToS(&sSwimRoll, 0, 4, 0xFA0, 0x190);
    if (player->skelAnime.curFrame > FAST_SWIM_RESTART_FRAME && CanStartDash(play)) {
        StartFastSwim(play, player, 4.0f);
        return true;
    }
    if (finished) {
        ReturnToSwimming(play, player);
        return true;
    }
    return false;
}

static bool TryWallBonk(PlayState* play, Player* player) {
    s16 yawToWall = player->yaw - (s16)(player->actor.wallYaw + 0x8000);

    if (!sIsDashing || !(player->actor.bgCheckFlags & BGCHECKFLAG_PLAYER_WALL_INTERACT) ||
        ABS(yawToWall) >= WALL_FACING_YAW) {
        return false;
    }
    sSwimPhase = SWIM_PHASE_BONK;
    sIsDashing = false;
    StopMmSfx(MM_ZORA_SWIM_LV);
    LinkAnimation_Change(play, &player->skelAnime, (LinkAnimationHeader*)sSwimHitAnim, MM_ANIM_ADJUSTED_SPEED, 0.0f,
                         Animation_GetLastFrame((void*)sSwimHitAnim), ANIMMODE_ONCE, 0.0f);
    player->linearVelocity *= -BONK_SPEED_SCALE;
    Player_RequestQuake(play, 33267, 3, 12);
    Player_RequestRumble(player, 255, 20, 150, 0);
    Audio_PlayActorSound2(&player->actor, NA_SE_PL_BODY_HIT);
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_CLIMB_END);
    return true;
}

static void RecoverFromBonk(PlayState* play, Player* player, bool finished) {
    Math_StepToF(&player->linearVelocity, 0.0f, 0.4f);
    if (finished && player->linearVelocity < BONK_RECOVER_SPEED) {
        ReturnToSwimming(play, player);
    }
}

static void GlanceOffFloor(PlayState* play, Player* player) {
    if (sSwimFloorTimer >= 8 || !IsGrounded(player)) {
        return;
    }
    sSwimPitch += (s16)((-player->floorPitch - sSwimPitch) * 2);
    sSwimFloorTimer = 15;
    StirSilt(play, player);
    Audio_PlayActorSound2(&player->actor, NA_SE_PL_BODY_BOUND);
}

static void RunSwimMove(Player* player, PlayState* play) {
    f32 speedTarget = 0.0f;

    if (sMove == ZORA_MOVE_DOLPHIN) {
        UpdateDolphin(play, player);
        return;
    }
    ApplyBuoyancy(player);
    if (TryDolphinJump(play, player)) {
        return;
    }
    bool isBeached = player->actor.yDistToWater < SWIM_EXIT_DEPTH && IsGrounded(player);
    if (isBeached || player->actor.yDistToWater <= 0.0f) {
        LeaveSwimMove(play, player);
        if (isBeached) {
            func_80839FFC(player, play);
        }
        return;
    }
    bool finished = LinkAnimation_Update(play, &player->skelAnime);
    if (sSwimPhase == SWIM_PHASE_BONK) {
        RecoverFromBonk(play, player, finished);
        return;
    }
    if (TryWallBonk(play, player)) {
        return;
    }
    switch (sSwimPhase) {
        case SWIM_PHASE_ROLL:
            RollIntoDash(play, player, finished, &speedTarget);
            break;
        case SWIM_PHASE_CRUISE:
            CruiseFastSwim(play, player, &speedTarget);
            break;
        case SWIM_PHASE_EXIT:
            if (EaseOutOfDash(play, player, finished)) {
                return;
            }
            break;
        default:
            ReturnToSwimming(play, player);
            return;
    }
    GlanceOffFloor(play, player);
    Math_SmoothStepToS(&sSwimRollSmoothed, sSwimRoll, 2, 0x5DC, 0x64);
    SteerFastSwim(play, player, speedTarget);
}

static void UpdateWaterButtons(PlayState* play, Player* player) {
    Input* input = &play->state.input[0];

    if (!IsInWater(player) || IsZoraBusy(player) || !CanAct(player) || player->interactRangeActor != NULL) {
        return;
    }
    bool isOnSeabed = IsGrounded(player) && sIsHeavy;
    if (isOnSeabed && CHECK_BTN_ALL(input->press.button, BTN_A) && !Player_IsZTargeting(player)) {
        SetHeavy(play, player, false);
        RefreshMmSfx(MM_FACE_UP, &player->actor.projectedPos);
        return;
    }
    if (CanStartDash(play) && player->actor.yDistToWater > SWIM_EXIT_DEPTH) {
        StartFastSwim(play, player, 4.0f);
        return;
    }
    if (!IsGrounded(player) && CHECK_BTN_ALL(input->press.button, BTN_B)) {
        SetHeavy(play, player, !sIsHeavy);
    }
}

static s32 CountSwimBubbles(Player* player) {
    f32 drive =
        sMove == ZORA_MOVE_FAST_SWIM ? ABS(sSwimTurn) * -0.004f + sSwimSpeed * -0.38f : player->actor.velocity.y;
    bool isStandingHeavy = sIsHeavy && IsGrounded(player);

    if (drive > -1.0f || isStandingHeavy) {
        return Rand_ZeroOne() < 0.2f ? 1 : 0;
    }
    return MIN((s32)(drive * -0.3f), 8);
}

static void SplashLimbsAtSurface(PlayState* play, Player* player, f32 surfaceY) {
    static const s32 sLimbs[] = { PLAYER_BODYPART_L_HAND, PLAYER_BODYPART_R_HAND, PLAYER_BODYPART_L_FOOT,
                                  PLAYER_BODYPART_R_FOOT };

    for (u32 i = 0; i < ARRAY_COUNT(sLimbs); i++) {
        Vec3f splash = player->bodyPartsPos[sLimbs[i]];
        if (fabsf(splash.y - surfaceY) < 30.0f) {
            splash.y = surfaceY;
            EffectSsGSplash_Spawn(play, &splash, NULL, NULL, 0, 80);
        }
    }
}

static void SpawnSwimEffects(PlayState* play, Player* player) {
    f32 surfaceY = player->actor.world.pos.y + player->actor.yDistToWater;
    Vec3f surface = { player->actor.world.pos.x, surfaceY, player->actor.world.pos.z };

    if (player->actor.yDistToWater < 20.0f) {
        return;
    }
    if ((sWaterFrames & 7) == 0 && fabsf(player->linearVelocity) > 0.5f) {
        EffectSsGRipple_Spawn(play, &surface, 100, 500, 0);
    }
    if (player->actor.yDistToWater > player->ageProperties->unk_2C) {
        for (s32 i = CountSwimBubbles(player); i > 0; i--) {
            EffectSsBubble_Spawn(play, &player->actor.world.pos, 20.0f, 10.0f, 20.0f, 0.13f);
        }
    }
    if (sWaterFrames == 1) {
        s16 scale = (s16)MIN(fabsf(player->linearVelocity) * 50.0f + player->actor.yDistToWater * 5.0f, 500.0f);
        EffectSsGSplash_Spawn(play, &surface, NULL, NULL, fabsf(player->linearVelocity) > 10.0f ? 1 : 0, scale);
    }
    if (sMove == ZORA_MOVE_FAST_SWIM && (sWaterFrames & 3) == 0) {
        SplashLimbsAtSurface(play, player, surfaceY);
    }
}

static void AcceptUnderwaterItem(PlayState* play, Player* player) {
    u32 hiddenFlags = player->stateFlags2 & (PLAYER_STATE2_UNDERWATER | PLAYER_STATE2_DIVING);

    if (!IsInWater(player) || player->interactRangeActor == NULL || player->getItemId <= GI_NONE ||
        (player->stateFlags1 & PLAYER_STATE1_GETTING_ITEM)) {
        return;
    }
    if (IsSwimMove()) {
        ReturnToSwimming(play, player);
    }
    player->stateFlags2 &= ~hiddenFlags;
    if (!Player_ActionHandler_2(player, play)) {
        player->stateFlags2 |= hiddenFlags;
    }
}

static void KeepZoraBody(Player* player) {
    if (player->ageProperties == &sZoraBody) {
        return;
    }
    sZoraBody = *player->ageProperties;
    sZoraBody.unk_2C = SWIM_ENTER_DEPTH;
    sZoraBody.unk_24 = sWaterExitDepth;
    player->ageProperties = &sZoraBody;
}

static void ReleaseBarrierLight(PlayState* play) {
    if (sBarrierLight != NULL) {
        LightContext_RemoveLight(play, &play->lightCtx, sBarrierLight);
        sBarrierLight = NULL;
    }
}

static void ClearBarrierTint(PlayState* play) {
    for (s32 i = 0; i < 3; i++) {
        play->envCtx.adjAmbientColor[i] = 0;
        play->envCtx.adjLight1Color[i] = 0;
        play->envCtx.adjFogColor[i] = 0;
    }
    play->envCtx.adjFogNear = 0;
}

static void TintSceneForBarrier(PlayState* play) {
    f32 blend = sBarrierIntensity / 255.0f;

    play->envCtx.adjAmbientColor[0] = (s16)(-blend * 40.0f);
    play->envCtx.adjAmbientColor[1] = (s16)(-blend * 40.0f);
    play->envCtx.adjAmbientColor[2] = (s16)(-blend * 20.0f);
    play->envCtx.adjLight1Color[0] = (s16)(blend * 30.0f);
    play->envCtx.adjLight1Color[1] = (s16)(blend * 30.0f);
    play->envCtx.adjLight1Color[2] = (s16)(blend * 60.0f);
    play->envCtx.adjFogColor[0] = (s16)(-blend * 20.0f);
    play->envCtx.adjFogColor[1] = (s16)(-blend * 20.0f);
    play->envCtx.adjFogColor[2] = (s16)(blend * 30.0f);
    play->envCtx.adjFogNear = (s16)(-blend * 100.0f);
}

static void OrbitBarrierLight(PlayState* play, Player* player) {
    f32 swing = Math_SinS((s16)(play->gameplayFrames * 14000)) * 40.0f;
    f32 reach = Math_CosS((s16)(play->gameplayFrames * 14000)) * 40.0f;
    s16 orbit = (s16)(play->gameplayFrames * 7000);

    Lights_PointNoGlowSetInfo(&sBarrierLightInfo, (s16)(player->actor.world.pos.x + reach),
                              (s16)(player->actor.world.pos.y + Math_SinS(orbit) * swing),
                              (s16)(player->actor.world.pos.z + Math_CosS(orbit) * swing), 100, 200, 255, 600);
    if (sBarrierLight == NULL) {
        sBarrierLight = LightContext_InsertLight(play, &play->lightCtx, &sBarrierLightInfo);
    }
}

static void StrikeWithBarrier(PlayState* play, Player* player) {
    if (!sBarrierColliderReady) {
        Collider_InitCylinder(play, &sBarrierCollider);
        Collider_SetCylinder(play, &sBarrierCollider, &player->actor, &sBarrierInit);
        sBarrierColliderReady = true;
    }
    sBarrierCollider.dim.pos.x = (s16)player->actor.world.pos.x;
    sBarrierCollider.dim.pos.y = (s16)player->actor.world.pos.y;
    sBarrierCollider.dim.pos.z = (s16)player->actor.world.pos.z;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sBarrierCollider.base);
    player->cylinder.base.acFlags &= ~(AC_ON | AC_HIT);
}

static void UpdateBarrier(PlayState* play, Player* player) {
    Input* input = &play->state.input[0];
    u16 buttons = IsInWater(player) ? BTN_R : (BTN_R | BTN_B);
    bool wasHeld = sBarrierHeld;
    s16 previous = sBarrierIntensity;

    sBarrierHeld = (IsZora() || sBorrowedGyorgSwim) && CHECK_BTN_ALL(input->cur.button, buttons) &&
                   gSaveContext.magic > 0 && (IsInWater(player) || IsGuarding()) && CanAct(player);
    if (sBarrierHeld && !wasHeld) {
        sBarrierDrainTimer = BARRIER_DRAIN_INTERVAL;
    }
    if (sBarrierHeld) {
        if (gSaveContext.magicState == MAGIC_STATE_IDLE && --sBarrierDrainTimer <= 0) {
            gSaveContext.magic = MAX(gSaveContext.magic - 1, 0);
            sBarrierDrainTimer = BARRIER_DRAIN_INTERVAL;
        }
        s32 target = gSaveContext.magic >= BARRIER_FULL_MAGIC ? 255 : (gSaveContext.magic * 255) / BARRIER_FULL_MAGIC;
        Math_StepToS(&sBarrierIntensity, (s16)target, BARRIER_RAMP);
    } else {
        Math_StepToS(&sBarrierIntensity, 0, BARRIER_RAMP);
    }
    if (sBarrierIntensity == 0) {
        ReleaseBarrierLight(play);
        StopMmSfx(MM_ZORA_SPARK_BARRIER);
        if (previous > 0) {
            ClearBarrierTint(play);
            player->cylinder.base.acFlags |= AC_ON;
        }
        return;
    }
    OrbitBarrierLight(play, player);
    StrikeWithBarrier(play, player);
    TintSceneForBarrier(play);
    RefreshMmSfx(MM_ZORA_SPARK_BARRIER, &player->actor.projectedPos);
}

static void DrawBarrier(PlayState* play, Player* player) {
    u32 frames = play->gameplayFrames;
    bool isDashing = sMove == ZORA_MOVE_FAST_SWIM;
    f32 scale = sBarrierIntensity * ((isDashing ? 0.1f : 0.05f) / 51.0f);

    if (sBarrierIntensity <= 0) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    if (isDashing) {
        Matrix_Translate(player->actor.world.pos.x,
                         player->actor.world.pos.y + 40.0f + (Math_CosS(sSwimPitch) - 1.0f) * 2.0f,
                         player->actor.world.pos.z, MTXMODE_NEW);
        Matrix_RotateY(BINANG_TO_RAD(player->actor.shape.rot.y), MTXMODE_APPLY);
        Matrix_RotateX(BINANG_TO_RAD(sSwimPitch), MTXMODE_APPLY);
        Matrix_RotateZ(BINANG_TO_RAD(sSwimRollSmoothed), MTXMODE_APPLY);
        Matrix_RotateX(M_PI, MTXMODE_APPLY);
        Matrix_Translate(0.0f, 0.0f, -40.0f, MTXMODE_APPLY);
    } else {
        Matrix_Translate(player->actor.world.pos.x, player->actor.world.pos.y + 40.0f, player->actor.world.pos.z,
                         MTXMODE_NEW);
        Matrix_RotateY(BINANG_TO_RAD(player->actor.shape.rot.y), MTXMODE_APPLY);
        Matrix_RotateX(BINANG_TO_RAD((s16)-0x4000), MTXMODE_APPLY);
        Matrix_Translate(0.0f, 0.0f, -18.0f, MTXMODE_APPLY);
    }
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    // Segments 0x0A/0x0B carry the two scrolling layers of object_link_zora_Matanimheader_012A80.
    gSPSegment(POLY_XLU_DISP++, 0x0A,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, -(s32)frames, (s32)(frames * 20), 0x20, 0x40, 1,
                                           -(s32)(frames * 2), (s32)(frames * 10), 0x20, 0x40));
    gSPSegment(POLY_XLU_DISP++, 0x0B,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, (s32)(frames * 3), (s32)(frames * 20), 0x20, 0x40, 1,
                                           -(s32)(frames * 12), (s32)(frames * 10), 0x40, 0x20));
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBarrierDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static bool IsHoldingOcarina(Player* player) {
    return player->rightHandType == PLAYER_MODELTYPE_RH_OCARINA || player->rightHandType == PLAYER_MODELTYPE_RH_OOT;
}

static bool IsPlayingOcarinaClip(Player* player, const char* name) {
    const char* anim = (const char*)player->skelAnime.animation;

    return anim != NULL && ResourceMgr_OTRSigCheck((char*)anim) && strstr(anim, name) != NULL;
}

static MmOcarinaNoteFunc FindGuitarVoice(void) {
    if (!SOH_MOD_API_HAS(sApi, FindService)) {
        return NULL;
    }
    return (MmOcarinaNoteFunc)sApi->FindService(MM_OCARINA_NOTE_SERVICE);
}

// Runs right after Ocarina triggered its own voice, so stopping it here always lands.
static void VoiceGuitar(uint8_t note, float bendFreq) {
    MmOcarinaNoteFunc voice = sGuitarVoice;

    if (voice == NULL) {
        return;
    }
    sGuitarBend = bendFreq;
    if (note != OCARINA_NOTE_NONE) {
        Audio_StopSfxById(NA_SE_OC_OCARINA);
    }
    if (note != sLastNote) {
        voice(MM_OCARINA_INSTRUMENT_ZORA_GUITAR, note, &sGuitarBend);
    }
}

static void NoteStruck(uint8_t note, float modulator, int8_t bend) {
    (void)bend;
    if (note != OCARINA_NOTE_NONE && note != sLastNote) {
        sWasNoteStruck = true;
    }
    VoiceGuitar(note, modulator);
    sLastNote = note;
}

static void PlaybackNoteStruck(uint8_t note, float modulator) {
    NoteStruck(note, modulator, 0);
}

static void SilenceGuitar(void) {
    MmOcarinaNoteFunc voice = sGuitarVoice;

    sGuitarVoice = NULL;
    if (voice != NULL) {
        voice(MM_OCARINA_INSTRUMENT_ZORA_GUITAR, OCARINA_NOTE_NONE, &sGuitarBend);
    }
}

static void ChangeGuitarClip(PlayState* play, Player* player, s32 slot, f32 speed, u8 mode, f32 morph) {
    LinkAnimationHeader* anim = sAnims[slot];
    f32 last = Animation_GetLastFrame(anim);

    LinkAnimation_Change(play, &player->skelAnime, anim, speed * MmAnimSpeed(slot), speed >= 0.0f ? 0.0f : last,
                         speed >= 0.0f ? last : 0.0f, mode, morph);
}

static bool SwapOcarinaClip(PlayState* play, Player* player) {
    if (IsPlayingOcarinaClip(player, "okarina_start")) {
        ChangeGuitarClip(play, player, ANIM_GUITAR_RAISE, 1.0f, ANIMMODE_ONCE, -6.0f);
        sGuitarPhase = GUITAR_RAISING;
        return true;
    }
    if (IsPlayingOcarinaClip(player, "okarina_end")) {
        ChangeGuitarClip(play, player, ANIM_GUITAR_RAISE, -1.0f, ANIMMODE_ONCE, -6.0f);
        sGuitarPhase = GUITAR_LOWERING;
        return true;
    }
    return false;
}

static bool IsReplayingSong(PlayState* play) {
    return play->msgCtx.msgMode >= MSGMODE_SONG_PLAYED && play->msgCtx.msgMode <= MSGMODE_DISPLAY_SONG_PLAYED;
}

static void LeanIntoGuitar(PlayState* play, Player* player) {
    Input* input = &play->state.input[0];
    f32 lean = MIN(sqrtf(SQ((f32)input->rel.stick_x) + SQ((f32)input->rel.stick_y)), 60.0f);
    s16 turn = lean > 0.0f ? Math_Atan2S(input->cur.stick_y, -input->cur.stick_x) : 0;

    if ((s16)(turn + 0x4000) < 0) {
        turn = (s16)(turn - 0x8000);
        lean = -lean;
    }
    turn = CLAMP(turn, -0x1F40, 0x2EE0);
    s16 bend = MIN((s16)(lean * -100.0f), 0xFA0);
    Math_SmoothStepToS(&player->upperLimbRot.x, bend, 4, 0x7D0, 0);
    Math_SmoothStepToS(&player->upperLimbRot.y, turn, 4, 0x7D0, 0);
    player->headLimbRot.x = -player->upperLimbRot.x;
    player->unk_6AE_rotFlags |= UNK6AE_ROT_HEAD_X | UNK6AE_ROT_UPPER_X | UNK6AE_ROT_UPPER_Y;
    s16 strain = ABS(player->upperLimbRot.x);
    player->actor.shape.face = strain < 0x7D0 ? FACE_NEUTRAL : (strain < 0xFA0 ? FACE_OPENING : FACE_HURT);
}

// The ocarina action reopens whenever a clip reports its end and would wipe the notes, so free play parks short of it.
static void PoseGuitar(PlayState* play, Player* player) {
    LinkAnimationHeader* strum = sAnims[ANIM_GUITAR_PLAY];
    f32 strumEnd = Animation_GetLastFrame(strum) - MM_ANIM_ADJUSTED_SPEED * CLIP_FRAMES_PER_UPDATE;
    bool isReplaying = IsReplayingSong(play);

    if (player->skelAnime.animation != strum) {
        ChangeGuitarClip(play, player, ANIM_GUITAR_PLAY, 1.0f, ANIMMODE_LOOP, -4.0f);
        player->skelAnime.playSpeed = 0.0f;
        sGuitarPhase = GUITAR_PLAYING;
    }
    if (isReplaying) {
        player->skelAnime.playSpeed = MM_ANIM_ADJUSTED_SPEED;
    } else if (sWasNoteStruck) {
        player->skelAnime.curFrame = 0.0f;
        player->skelAnime.playSpeed = MM_ANIM_ADJUSTED_SPEED;
    } else if (player->skelAnime.curFrame >= strumEnd) {
        player->skelAnime.playSpeed = 0.0f;
    }
    sWasNoteStruck = false;
    if (!isReplaying) {
        LeanIntoGuitar(play, player);
    }
}

static void UpdateGuitar(PlayState* play, Player* player) {
    bool isSessionOpen = play->msgCtx.msgMode != MSGMODE_NONE;

    if (IsHoldingOcarina(player)) {
        sIsOcarinaOut = true;
    } else if (!isSessionOpen) {
        sIsOcarinaOut = false;
    }
    if (!sIsOcarinaOut) {
        sGuitarPhase = GUITAR_IDLE;
        if (sGuitarVoice != NULL) {
            SilenceGuitar();
        }
        return;
    }
    if (sGuitarVoice == NULL) {
        sGuitarVoice = FindGuitarVoice();
    }
    if (sAnims[ANIM_GUITAR_RAISE] == NULL || sAnims[ANIM_GUITAR_PLAY] == NULL || SwapOcarinaClip(play, player)) {
        return;
    }
    bool isRaising = sGuitarPhase == GUITAR_RAISING && player->skelAnime.animation == sAnims[ANIM_GUITAR_RAISE];
    if (isRaising || sGuitarPhase == GUITAR_LOWERING) {
        return;
    }
    if (isSessionOpen || sGuitarPhase == GUITAR_RAISING) {
        PoseGuitar(play, player);
    }
}

static void DrawGuitar(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGuitarDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawZoraLimbExtras(PlayState* play, Player* player, int32_t limbIndex) {
    if (!IsZora()) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_L_FOREARM || limbIndex == PLAYER_LIMB_R_FOREARM) {
        DrawForearmFin(play, player, limbIndex);
    }
    if (limbIndex == PLAYER_LIMB_L_HAND && sIsOcarinaOut && sGuitarPhase != GUITAR_IDLE) {
        DrawGuitar(play);
    }
    DrawStrikeLimb(play, player, limbIndex);
}

#define FIN_RETICLE_RANGE 1200.0f

static void DrawZora(PlayState* play, Player* player) {
    DrawBarrier(play, player);
    if (sMove == ZORA_MOVE_FINS_READY && Z64Aiming_IsAiming()) {
        Z64Aiming_DrawReticle(play, player, FIN_RETICLE_RANGE,
                              (Z64AimingReticleStyle){ Z64AIMING_TOGGLE_BOW, Z64AIMING_TINT_LAUNCH });
    }
}

static void SpeakWithZoraVoice(Actor* actor, uint16_t sfxId) {
    const uint16_t action = (uint16_t)(sfxId - NA_SE_VO_LI_SWORD_N);
    MmSfxPlayFunc play = GetMmSfxPlayer();

    if (action < LINK_VOICE_ACTIONS && play != NULL) {
        play((uint16_t)(NA_SE_VO_LI_SWORD_N + MM_ZORA_VOICE_OFFSET + action), &actor->projectedPos);
    }
}

static bool StepWithZoraFeet(Actor* actor) {
    Player* player = (Player*)actor;
    MmSfxPlayFunc play = GetMmSfxPlayer();

    return play != NULL &&
           play((uint16_t)(NA_SE_PL_WALK_GROUND + player->floorSfxOffset + MM_ZORA_STEP_OFFSET), &actor->projectedPos);
}

static void ResolveZoraSfx(Actor* actor, int32_t kind, uint16_t* sfxId, bool* handled) {
    if (!IsZora()) {
        return;
    }
    if (kind == SOH_ACTOR_SFX_VOICE) {
        SpeakWithZoraVoice(actor, *sfxId);
        *handled = true;
    } else if (kind == SOH_ACTOR_SFX_STEP) {
        *handled = StepWithZoraFeet(actor);
    }
}

static bool HasHumanSwimProvider(bool requested) {
    typedef bool (*SwimCapability)(void);
    SwimCapability tunic =
        SOH_MOD_API_HAS(sApi, FindService) ? (SwimCapability)sApi->FindService("nei.zora_tunic.swim_enabled") : NULL;
    return requested || (SOH_MOD_API_HAS(sApi, IsCustomItemWorn) && sApi->IsCustomItemWorn("mm.remains.gyorg")) ||
           (tunic && tunic());
}

static void HumanGyorgSwim(PlayState* play, Player* player, bool enabled) {
    if (IsZora()) {
        sBorrowedGyorgSwim = false;
        return;
    }
    enabled = HasHumanSwimProvider(enabled);
    const char* form = sApi->GetActiveForm();
    if (!enabled || (form != NULL && form[0] != '\0') || player->currentBoots == PLAYER_BOOTS_IRON || !CanAct(player) ||
        !HasSwimAbility()) {
        if (sBorrowedGyorgSwim && IsSwimMove())
            LeaveSwimMove(play, player);
        sBorrowedGyorgSwim = false;
        return;
    }
    if (!sBorrowedGyorgSwim) {
        sSwimPitch = sSwimRoll = sSwimRollSmoothed = sSwimTurn = 0;
        sSwimFloorTimer = 0;
    }
    sBorrowedGyorgSwim = true;
}

static void UpdateHumanSwim(void) {
    if (gPlayState == NULL || IsZora())
        return;
    Player* player = GET_PLAYER(gPlayState);
    HumanGyorgSwim(gPlayState, player, false);
    sWaterFrames = sBorrowedGyorgSwim && IsInWater(player) ? sWaterFrames + 1 : 0;
    if (sWaterFrames != 0)
        SpawnSwimEffects(gPlayState, player);
    if (sBorrowedGyorgSwim || sBarrierIntensity > 0)
        UpdateBarrier(gPlayState, player);
}

static void DrawHumanSwim(Actor* actor, PlayState* play) {
    if (actor == &GET_PLAYER(play)->actor && !IsZora() && sBarrierIntensity > 0) {
        DrawBarrier(play, (Player*)actor);
    }
}

static bool IsZoraBusy(Player* player) {
    return player->actionFunc == ZoraStrikeAction || player->actionFunc == ZoraFinsAction ||
           player->actionFunc == ZoraGuardAction || IsSwimMove();
}

// Assignable Tunics and Boots runs the action itself from VB_EXECUTE_PLAYER_ACTION_FUNC, ahead of every mod, unless
// the action is paused; vanilla's swim idle then caps the arc at 2 units a frame. Player_UpdateCommon clears the flag.
static void HoldVanillaActionDuringSwimMove(void) {
    if (gPlayState != NULL && IsSwimMove()) {
        GET_PLAYER(gPlayState)->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
    }
}

static void DriveSwimMove(bool* should, va_list args) {
    Player* player = va_arg(args, Player*);

    if ((!IsZora() && !sBorrowedGyorgSwim) || !IsSwimMove() || gPlayState == NULL) {
        return;
    }
    if (!CanAct(player)) {
        LeaveSwimMove(gPlayState, player);
        *should = true;
        return;
    }
    if (sMove == ZORA_MOVE_FAST_SWIM && !IsInWater(player) && !TryDolphinJump(gPlayState, player)) {
        LeaveSwimMove(gPlayState, player);
        *should = true;
        return;
    }
    *should = false;
    // Player_UpdateCommon clears this flag immediately before dispatching this hook.
    player->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    if (sSwimAnim != NULL && player->skelAnime.animation != sSwimAnim) {
        LinkAnimation_Change(gPlayState, &player->skelAnime, sSwimAnim, sSwimAnimSpeed, sSwimAnimFrame,
                             Animation_GetLastFrame(sSwimAnim), sSwimAnimMode, -3.0f);
    }
    RunSwimMove(player, gPlayState);
    if (IsSwimMove()) {
        sSwimAnim = player->skelAnime.animation;
        sSwimAnimFrame = player->skelAnime.curFrame;
        sSwimAnimSpeed = player->skelAnime.playSpeed;
        sSwimAnimMode = player->skelAnime.mode;
    }
}

static void ResetState(PlayState* play, Player* player) {
    if (play != NULL) {
        StopTrail(play);
        ReleaseBarrierLight(play);
        if (sBarrierIntensity > 0) {
            ClearBarrierTint(play);
        }
    }
    if (player != NULL) {
        player->stateFlags1 &= ~PLAYER_STATE1_SHIELDING;
        player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
        player->cylinder.base.acFlags |= AC_ON;
    }
    StopMmSfx(MM_ZORA_SWIM_LV);
    StopMmSfx(MM_ZORA_SPARK_BARRIER);
    sMove = ZORA_MOVE_NONE;
    sSwimAnim = NULL;
    sPunchQueued = false;
    sHoldFrames = 0;
    sBarrierHeld = false;
    sBarrierIntensity = 0;
    sSwimPitch = 0;
    sSwimRoll = 0;
    sGuitarPhase = GUITAR_IDLE;
    sIsOcarinaOut = false;
    sScaledDamage = 0;
}

static u8 sHumanMeleeDamage[2] = { 1, 1 };

static void EnterZora(PlayState* play, Player* player) {
    for (s32 quad = 0; quad < 2; quad++) {
        sHumanMeleeDamage[quad] = player->meleeWeaponQuads[quad].info.toucher.damage;
    }
    ResetState(play, player);
    sIsHeavy = false;
    sFinsOut = false;
    sFins[0] = sFins[1] = NULL;
}

static const s16 sZoraBootRow[17] = { 200, 1000, 300, 700, 550, 270, 700, 300, 800,
                                      600, -100, 600, 590, 750, 125, 200, 130 };

static void WearZoraBoots(PlayState* play, Player* player) {
    if (player->currentBoots != PLAYER_BOOTS_KOKIRI) {
        return;
    }
    REG(19) = sZoraBootRow[0];
    REG(30) = sZoraBootRow[1];
    REG(32) = sZoraBootRow[2];
    REG(34) = sZoraBootRow[3];
    REG(35) = sZoraBootRow[4];
    REG(36) = sZoraBootRow[5];
    REG(37) = sZoraBootRow[6];
    REG(38) = sZoraBootRow[7];
    REG(43) = sZoraBootRow[8];
    REG(45) = sZoraBootRow[9];
    REG(68) = sZoraBootRow[10];
    REG(69) = sZoraBootRow[11];
    IREG(66) = sZoraBootRow[12];
    IREG(67) = sZoraBootRow[13];
    IREG(68) = sZoraBootRow[14];
    IREG(69) = sZoraBootRow[15];
    MREG(95) = sZoraBootRow[16];
    if (play->roomCtx.curRoom.behaviorType1 == ROOM_BEHAVIOR_TYPE1_2) {
        REG(45) = 500;
    }
}

static void ExitZora(PlayState* play, Player* player) {
    bool wasSwimming = IsSwimMove();
    bool wasBusy = IsZoraBusy(player) && !wasSwimming;

    if (wasSwimming) {
        LeaveSwimMove(play, player);
    }
    sApi->ReleaseHazard(ZORA_HAZARD_OWNER);
    SilenceGuitar();
    Z64Aiming_Release(player, play);
    ResetState(play, player);
    if (sIsHeavy) {
        SetHeavy(play, player, false);
    }
    Player_SetBootData(play, player);
    if (wasBusy) {
        func_80839FFC(player, play);
    }
    // OoT keeps AT_ON enabled and gates sword attacks with meleeWeaponState.
    // Leaving it cleared makes every subsequent human sword swing miss.
    player->meleeWeaponState = 0;
    player->meleeWeaponInfo[0].active = 0;
    for (s32 quad = 0; quad < 2; quad++) {
        ColliderQuad* collider = &player->meleeWeaponQuads[quad];
        collider->base.atFlags = (collider->base.atFlags & ~(AT_HIT | AT_BOUNCED)) | AT_ON;
        collider->info.toucher.damage = sHumanMeleeDamage[quad];
        player->meleeWeaponInfo[quad + 1].active = 0;
    }
}

static bool IsUpperBodyMove(void) {
    return sMove == ZORA_MOVE_FINS_LOCKED_READY || sMove == ZORA_MOVE_FINS_LOCKED_THROW ||
           sMove == ZORA_MOVE_GUARD_WALK;
}

static void UpdateZora(PlayState* play, Player* player) {
    WearZoraBoots(play, player);
    if (sAnims[ANIM_WAIT] != NULL && player->skelAnime.animation == sAnims[ANIM_WAIT]) {
        player->skelAnime.playSpeed = MmAnimSpeed(ANIM_WAIT);
    }
    if (sMove != ZORA_MOVE_NONE && !IsUpperBodyMove() && !IsZoraBusy(player)) {
        if (IsSwimMove()) {
            LeaveSwimMove(play, player);
        }
        Z64Aiming_Release(player, play);
        player->stateFlags1 &= ~PLAYER_STATE1_SHIELDING;
        StopTrail(play);
        sMove = ZORA_MOVE_NONE;
    }
    if (IsInWater(player)) {
        sApi->RequestHazard(ZORA_HAZARD_OWNER, 0, PLAYER_ENV_HAZARD_NONE, EQUIP_VALUE_TUNIC_KOKIRI);
    } else {
        sApi->ReleaseHazard(ZORA_HAZARD_OWNER);
    }
    if (sMove == ZORA_MOVE_GUARD) {
        PlantGuard(play, player);
    }
    KeepZoraBody(player);
    sWaterFrames = IsInWater(player) ? sWaterFrames + 1 : 0;
    UpdateGuardWalk(play, player);
    UpdateLockedFins(play, player);
    KeepWeight(play, player);
    TrackFins(play, player);
    AcceptUnderwaterItem(play, player);
    UpdateWaterButtons(play, player);
    if (IsInWater(player)) {
        SpawnSwimEffects(play, player);
    }
    TryAirKick(play, player);
    UpdateBarrier(play, player);
    UpdateGuitar(play, player);
}

static void ForgetScene(int16_t sceneNum) {
    (void)sceneNum;
    Z64Aiming_Drop();
    sBarrierColliderReady = false;
    sBarrierLight = NULL;
    sTrail = -1;
    sSwimTrail[0] = -1;
    sSwimTrail[1] = -1;
    sFinsOut = false;
    sFins[0] = sFins[1] = NULL;
    sIsHeavy = false;
    ResetState(NULL, NULL);
    sBorrowedGyorgSwim = false;
}

static bool CanWearMask(Player* player, PlayState* play) {
    return (IsGrounded(player) || IsInWater(player)) &&
           !(player->stateFlags1 & (PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_IN_CUTSCENE));
}

static void WearMask(PlayState* play, Player* player) {
    sApi->ToggleForm(ZORA_FORM_KEY);
}

static void DrawMaskGetItem(PlayState* play, GetItemEntry* entry) {
    (void)entry;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_26Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGetItemDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static const SOHCustomItemRandomizer sRandomizerItem = {
    sizeof(SOHCustomItemRandomizer), 0, SOH_CUSTOM_ITEM_TYPE_ITEM, 0, 1, ZORA_MASK_KEY, ZORA_MASK_KEY, "",
};

static void RegisterMask(void) {
    SOHCustomItemDefinition mask = Z64Items_Define(ZORA_MASK_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&mask, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    mask.flags |= SOH_CUSTOM_ITEM_INSTANT | SOH_CUSTOM_ITEM_WEARABLE;
    Z64Items_SetPlacement(&mask, ZORA_MASK_PAGE, ZORA_MASK_SLOT, 0);
    mask.getItemEntry = ItemTable_RetrieveEntry(MOD_NONE, GI_MASK_ZORA);
    mask.getItemEntry.textId = 0;
    mask.getItemEntry.drawFunc = DrawMaskGetItem;
    Z64Items_SetReplaces(&mask, ITEM_MASK_ZORA);
    Z64Items_SetVanillaMode(&mask, SOH_VANILLA_ITEM_REPLACE, RG_ZORA_MASK);
    Z64Items_SetTextbox(&mask, "You got the %rZora Mask%w! It&holds the spirit of a Zora&"
                               "guitarist who sang to the sea.^Wear it with %y\xA1%w to become a&"
                               "Zora: swim like a fish and fight&with blade-like fins.");
    Z64Items_SetPauseText(&mask, "%rZora Mask&%wPress %y\xA1%w to become a Zora.&"
                                 "%y\xA0%w: punch  Hold %y\xA0%w: fins  %y\xA3%w: guard^"
                                 "%y\xA3%w+%y\xA0%w: barrier In water: %y\x9F%w dashes,&%y\xA0%w sinks.");
    Z64Items_SetCanUse(&mask, CanWearMask);
    Z64Items_SetAction(&mask, WearMask, NULL);
    Z64Items_SetLogic(&mask, &sRandomizerItem);
    Z64Items_Register(sApi, &mask);
}

static const SOHFormActionOverride sActions[] = {
    { SOH_PLAYER_ACTION_MELEE, StartPunch },
    { SOH_PLAYER_ACTION_ZTARGET_A, HandleZTargetA },
    { SOH_PLAYER_ACTION_SHIELD, StartGuard },
};

static SOHFormAnimOverride sAnimOverrides[] = {
    { PLAYER_ANIMGROUP_wait, -1, NULL },
};

static uint16_t ResolveZoraEquipment(int32_t equipType, uint16_t value) {
    switch (equipType) {
        case EQUIP_TYPE_SWORD:
            return EQUIP_VALUE_SWORD_NONE;
        case EQUIP_TYPE_SHIELD:
            return EQUIP_VALUE_SHIELD_NONE;
        case EQUIP_TYPE_TUNIC:
            return EQUIP_VALUE_TUNIC_KOKIRI;
        case EQUIP_TYPE_BOOTS:
            return EQUIP_VALUE_BOOTS_KOKIRI;
        default:
            return value;
    }
}

static bool IsFormMask(const char* customKey) {
    static const char* const sFormMasks[] = {
        "nei.deku_form_mask", "nei.goron_form_mask",  "nei.keaton_form_mask",  "nei.mask_kafei",
        "nei.garo_form_mask", "nei.gerudo_form_mask", "nei.fierce_deity_mask", "nei.rito_form_mask",
    };

    for (u32 i = 0; i < ARRAY_COUNT(sFormMasks); i++) {
        if (strcmp(customKey, sFormMasks[i]) == 0) {
            return true;
        }
    }
    return false;
}

static bool AllowsZoraItem(uint16_t item, const char* customKey) {
    if (customKey != NULL) {
        return IsFormMask(customKey);
    }
    bool isBottle = item >= ITEM_BOTTLE && item <= ITEM_POE;
    bool isTrade =
        (item >= ITEM_WEIRD_EGG && item <= ITEM_SOLD_OUT) || (item >= ITEM_POCKET_EGG && item <= ITEM_CLAIM_CHECK);

    return item == ITEM_OCARINA_FAIRY || item == ITEM_OCARINA_TIME || item == ITEM_BOW || item == ITEM_ARROW_FIRE ||
           item == ITEM_ARROW_ICE || item == ITEM_HOOKSHOT || item == ITEM_LONGSHOT || item == ITEM_NAYRUS_LOVE ||
           isBottle || isTrade;
}

static void RegisterForm(void) {
    SOHFormDefinition zora = { 0 };

    zora.structSize = sizeof(zora);
    zora.key = ZORA_FORM_KEY;
    zora.label = "Zora";
    zora.kind = SOH_FORM_KIND_LINK;
    zora.item = ZORA_MASK_KEY;
    zora.modelPath = ZORA_MODEL_PATH;
    zora.rootScaleAdult = ZORA_ROOT_SCALE_ADULT;
    zora.rootScaleChild = ZORA_ROOT_SCALE_CHILD;
    zora.height = ZORA_HEIGHT;
    zora.motionScale = 1.0f;
    zora.resolveEquipment = ResolveZoraEquipment;
    zora.allowsButtonItem = AllowsZoraItem;
    zora.transformAnim = sAnims[ANIM_MASK_ON];
    zora.transformOffAnim = sAnims[ANIM_MASK_OFF];
    zora.transformMask = sMaskDL;
    zora.transformVoiceSfx = NA_SE_VO_LI_FALL_L;
    zora.transformGlowOffset = (Vec3f){ -189.5f, -594.87f, 0.0f };
    zora.transformGlowScale = 0.7f;
    sAnimOverrides[0].anim = sAnims[ANIM_WAIT];
    zora.anims = sAnimOverrides;
    zora.animCount = sAnims[ANIM_WAIT] != NULL ? ARRAY_COUNT(sAnimOverrides) : 0;
    zora.actions = sActions;
    zora.actionCount = ARRAY_COUNT(sActions);
    zora.onEnter = EnterZora;
    zora.onExit = ExitZora;
    zora.update = UpdateZora;
    zora.draw = DrawZora;
    sApi->RegisterForm(&zora);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

static void RegisterOption(const char* label, const char* cvar, const char* tooltip) {
    SOHModMenuWidget widget = { 0 };

    widget.structSize = sizeof(widget);
    widget.section = "Mods";
    widget.sidebar = "Zora";
    widget.type = SOH_MOD_MENU_CHECKBOX;
    widget.label = label;
    widget.cvar = cvar;
    widget.tooltip = tooltip;
    sApi->RegisterMenuWidget(&widget);
}

SOH_MOD_EXPORT void ModInit(void) {
    if (SOH_MOD_API_HAS(sApi, PublishService))
        sApi->PublishService("nei.zora.human_gyorg_swim", (void*)HumanGyorgSwim);
    LoadAnims();
    RegisterMask();
    RegisterForm();
    RegisterOption("Invert Swim Left/Right", SWIM_INVERT_X_CVAR, "Turns the fast-swimming Zora the opposite way.");
    RegisterOption("Invert Swim Up/Down", SWIM_INVERT_Y_CVAR, "Pushing up swims up instead of diving.");
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, ResolveZoraBody);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawZoraLimbExtras);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveFaceTextures, ResolveZoraFace);
    SOH_REGISTER_HOOK(sApi, OnPlayerFilterInput, FilterZoraInput);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorPlaySfx, ACTOR_PLAYER, ResolveZoraSfx);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDraw, ACTOR_EN_BOOM, DrawFlyingFin);
    SOH_REGISTER_HOOK(sApi, OnOcarinaNote, NoteStruck);
    SOH_REGISTER_HOOK(sApi, OnOcarinaPlaybackNote, PlaybackNoteStruck);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateHumanSwim);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, HoldVanillaActionDuringSwimMove);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawHumanSwim);
    sApi->RegisterVB(VB_USE_STRENGTH_UPGRADE, ResolveZoraStrength);
    sApi->RegisterVB(VB_PREVENT_STRENGTH, ResolveZoraGrab);
    sApi->RegisterVB(VB_SWIM, GrantZoraSwim);
    sApi->RegisterVB(VB_EXECUTE_PLAYER_ACTION_FUNC, DriveSwimMove);
}
