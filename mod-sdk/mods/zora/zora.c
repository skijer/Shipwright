#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "z64items.h"
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
// No resources live here: a model path is what turns on the host's root scaling, as for Deku.
#define ZORA_MODEL_PATH "objects/forms/zora"
#define ZORA_HAZARD_OWNER "nei.zora"
#define ZORA_MASK_PAGE 1
#define ZORA_MASK_SLOT 17

#define BG_ON_GROUND 1
#define BG_TOUCHING_WALL 8
#define MM_ANIM_VALUES_PER_FRAME 67
#define MM_ANIM_BASE_TRANSL_X (-57)
// Rest height of the Zora root (3600) over the height Link's animations carry the root to.
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
#define SWIM_SURFACE_DEPTH 36.0f
#define SWIM_DEEP_DEPTH 68.0f
#define SWIM_GRAVITY 0.0f
#define FAST_SWIM_SPEED 9.0f
#define FAST_SWIM_KICK_SPEED 12.0f
#define FAST_SWIM_BURST 16.0f
#define FAST_SWIM_KICK_FRAME 13.0f
#define DOLPHIN_PITCH (-0x1555)
#define DOLPHIN_MAX_SPEED 13.5f
#define DOLPHIN_MIN_SPEED 2.0f
#define DOLPHIN_GRAVITY -1.0f
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
// LinkAnimation advances R_UPDATE_RATE * 0.5 clip frames per update, and the rate is always 3.
#define CLIP_FRAMES_PER_UPDATE 1.5f
#define LINK_VOICE_ACTIONS 0x20
#define MM_ZORA_VOICE_OFFSET 0xA0
#define MM_SFX_PLAY_SERVICE "mm.sfx.play"
#define MM_SFX_STOP_SERVICE "mm.sfx.stop"
#define MM_ZORA_STEP_OFFSET 0x120
#define MM_CATCH_BOOMERANG 0x0836
#define MM_FACE_UP 0x0863
#define MM_ZORA_SWIM_DASH 0x08EC
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

// Joint translations of gLinkZoraSkel, read from mm.o2r; same limb order as OoT Link.
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

// MM's Zora strike anchors (z_player_lib.c D_801C0A00/D_801C09DC on the forearm fins and
// D_801C0A48/D_801C0A24 on the right shin), in the space of the limb that strikes.
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

#define SPEED_MODE_LINEAR 0.0f

static const SOHModApi* sApi;
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
static s16 sFinFrame;
static s16 sThrowYaw;
static s16 sThrowPitch;

static bool sIsHeavy;
static u8 sSwimPhase;
static s16 sSwimPitch;
static s16 sSwimRoll;
static s16 sSwimRollSmoothed;
static bool sSwimExitQueued;
static u32 sWaterFrames;
static PlayerAgeProperties sZoraBody;
static s16 sSwimTurn;
static s16 sSwimFloorTimer;
static f32 sSwimSpeed;

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

// A shuffled swim is granted at file creation when the setting is off, so the flag alone answers it.
static bool HasSwimAbility(void) {
    return !IS_RANDO || Flags_GetRandomizerInf(RAND_INF_CAN_SWIM);
}

static bool IsSwimMove(void) {
    return sMove == ZORA_MOVE_FAST_SWIM || sMove == ZORA_MOVE_DOLPHIN;
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

// ---- animations ----

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

static void PlayAnim(PlayState* play, Player* player, s32 slot, f32 startFrame, u8 mode, f32 morph) {
    LinkAnimationHeader* anim = sAnims[slot];

    if (anim == NULL) {
        return;
    }
    LinkAnimation_Change(play, &player->skelAnime, anim, 1.0f, startFrame, Animation_GetLastFrame(anim), mode, morph);
}

// ---- body ----

static const char* HandDL(Player* player, int32_t limbIndex) {
    if (limbIndex == PLAYER_LIMB_L_HAND) {
        return player->leftHandType == PLAYER_MODELTYPE_LH_OPEN ? sLeftOpenHandDL : sZoraLimbDL[PLAYER_LIMB_L_HAND];
    }
    return player->rightHandType == PLAYER_MODELTYPE_RH_OPEN ? sRightOpenHandDL : sZoraLimbDL[PLAYER_LIMB_R_HAND];
}

// MM's torpedo pose: the body pitches and banks around the root, lifted so the pivot sits at the chest.
static void PitchSwimmingBody(Vec3f* pos) {
    Matrix_Translate(pos->x, pos->y + (Math_CosS(sSwimPitch) - 1.0f) * 200.0f, pos->z, MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD(sSwimPitch), MTXMODE_APPLY);
    Matrix_RotateZ(BINANG_TO_RAD(sSwimRoll), MTXMODE_APPLY);
    pos->x = 0.0f;
    pos->y = 0.0f;
    pos->z = 0.0f;
}

static void ResolveZoraBody(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
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
    int32_t* strength = va_arg(args, int32_t*);

    if (!IsZora()) {
        return;
    }
    *strength = PLAYER_STR_BRACELET;
    *should = false;
}

static void GrantZoraSwim(bool* should, va_list args) {
    if (IsZora()) {
        *should = true;
    }
}

// ---- input ----

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

// In water A is the fast swim and B the heavy toggle; vanilla would dive and swing with them.
static void FilterZoraInput(Player* player, Input* input) {
    if (!IsZora()) {
        return;
    }
    ScaleFireDamage(player);
    if (!IsInWater(player) || player->interactRangeActor != NULL) {
        return;
    }
    // On the seabed B is a punch and Z-targeted A the side hop, both vanilla's.
    u16 taken = IsGrounded(player) ? (Player_IsZTargeting(player) ? 0 : BTN_A) : (BTN_A | BTN_B);
    input->press.button &= ~taken;
    input->cur.button &= ~taken;
}

// ---- strikes ----

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

// MM's D_8085D30C: the fin wakes of the fast swim.
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

// func_8012669C from MM: point 0 feeds meleeWeaponInfo[0] (the trail, and the wall probe of func_80842DF4) and
// points 1-2 are the two attack quads.
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
    // MM's func_8082FA5C swings the final kick with the Goron's heavy thud, not a sword whoosh.
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

// A fresh press is a combo; only an unbroken hold readies the fins, the way MM times it. The first punch is
// shorter than the hold, so the count carries on through its recovery.
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

// The same call vanilla's sword actions make: shield rebounds, wall sparks and taking damage come with it.
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

// Z-target's A keeps the side hop and back flip; forward, where Link would jump-slash, the Zora kicks.
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

// Vanilla never reads item buttons in the air, so the aerial kick is launched from the end of the frame.
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

// ---- fin boomerangs ----

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

// The lock-on stride stays vanilla's: only the arms take the Zora's pose, over the step already in jointTable.
static void PoseArms(PlayState* play, Player* player, LinkAnimationHeader* anim, s32 frame) {
    static const s32 sArmLimbs[] = { PLAYER_LIMB_L_SHOULDER, PLAYER_LIMB_L_FOREARM, PLAYER_LIMB_L_HAND,
                                     PLAYER_LIMB_R_SHOULDER, PLAYER_LIMB_R_FOREARM, PLAYER_LIMB_R_HAND };
    // SetLoadFrame copies the face channel after the limbs, hence the extra entry.
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

// MM aims the fins like the bow: first person when nothing is locked on.
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

// Runs after vanilla's lock-on step, which is what keeps the Zora strafing with the fins out.
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
            PoseArms(play, player, ready, sFinFrame++ % ready->common.frameCount);
            return;
        }
        sMove = ZORA_MOVE_FINS_LOCKED_THROW;
        sFinFrame = 0;
    }
    PoseArms(play, player, release, sFinFrame);
    if (!sFinsOut && sFinFrame == (s16)BOOMERANG_THROW_FRAME) {
        ThrowLockedFins(play, player);
    }
    if (++sFinFrame >= release->common.frameCount) {
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

// MM's En_Boom draw with the Zora fins: the collider and the trail are placed here, since vanilla's draw is skipped.
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

// ---- guard ----

// The Zora guards with his fins, never with the equipped shield: a fixed metal quad in front of his chest,
// with shieldMf turned so reflected projectiles fly back where they came from.
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

// A blocked hit pushes the Zora back like a shield would; the guard lets that speed bleed off.
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

// Locked on, vanilla guards through the upper body so Link keeps strafing; the standing guard is only for free aim.
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

// Vanilla's lock-on guard (func_80834758) needs a shield in hand, so the fins raise the same pose themselves.
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

// ---- fins on the forearms ----

// MM's func_80126BD0 ramp: the guard grows the blades with an overshoot (D_801C07F0), strikes snap them out.
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

// MM's func_80124618: a keyframed uniform scale, linear between keys.
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

// D_801C05A8 and D_801C05D8: the blade folds into the arm for the dash and comes back out after it.
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

// D_801C05C8/D_801C05F0: the long swimming fins open during the roll, follow the turn while cruising, and fold
// from wherever they were when the dash ends.
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

    // The fins ARE the boomerangs: while they fly, the forearms are bare.
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

// ---- water ----

static void SetHeavy(PlayState* play, Player* player, bool heavy) {
    sIsHeavy = heavy;
    player->currentBoots = heavy ? PLAYER_BOOTS_IRON : PLAYER_BOOTS_KOKIRI;
    Player_SetBootData(play, player);
}

// Player_SetEquipmentData puts the saved boots back whenever it runs; the Zora's weight is his own.
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

// MM's func_808475B4: float to the surface, or sink at a steady pace when heavy.
static void ApplyBuoyancy(Player* player) {
    f32 terminal = -5.0f;
    f32 buoyancyDepth = SWIM_BUOYANCY_DEPTH + (player->actor.velocity.y < 0.0f ? 1.0f : 0.0f);
    f32 depthDelta = player->actor.yDistToWater - SWIM_BUOYANCY_DEPTH;
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

// Vanilla's func_8083D53C re-seats its swim idle every frame on anyone deep in water whose action is not one of
// its own swims, so the dash cannot own the action func: it keeps vanilla's swim idle installed and runs in its
// place through VB_EXECUTE_PLAYER_ACTION_FUNC, the way NEI paused it.
static void StartFastSwim(PlayState* play, Player* player, f32 fromFrame) {
    if (sIsHeavy) {
        SetHeavy(play, player, false);
    }
    sMove = ZORA_MOVE_FAST_SWIM;
    sSwimPhase = SWIM_PHASE_ROLL;
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
    sSwimPitch = 0;
    sSwimRoll = 0;
    sSwimRollSmoothed = 0;
    player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    player->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
    player->actor.gravity = GRAVITY_NORMAL;
}

static void ReturnToSwimming(PlayState* play, Player* player) {
    LeaveSwimMove(play, player);
    func_80838F18(play, player);
}

// func_8083D53C re-seats the swim idle on a dry body deeper than unk_2C and drops a wet one shallower than unk_24
// into a fall; func_8083AA10 does the same to any airborne body without MIDAIR. Both would take the arc's pose.
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

// MM's func_8083B3B4: breaching near the surface with the nose up throws the Zora into the air.
static bool TryDolphinJump(PlayState* play, Player* player) {
    if (sSwimPitch >= DOLPHIN_PITCH || player->actor.yDistToWater - player->actor.velocity.y >= SWIM_DEEP_DEPTH) {
        return false;
    }
    f32 launch = MIN(sSwimSpeed * 1.5f, DOLPHIN_MAX_SPEED);
    if (launch < DOLPHIN_MIN_SPEED) {
        return false;
    }
    sMove = ZORA_MOVE_DOLPHIN;
    player->linearVelocity = Math_CosS(sSwimPitch) * launch;
    player->actor.velocity.y = -Math_SinS(sSwimPitch) * launch;
    player->actor.gravity = DOLPHIN_GRAVITY;
    player->actor.bgCheckFlags &= ~BG_ON_GROUND;
    KeepDolphinArc(player);
    PlayAnim(play, player, ANIM_FISH_SWIM, 0.0f, ANIMMODE_LOOP, -3.0f);
    StopMmSfx(MM_ZORA_SWIM_LV);
    Audio_PlayActorSound2(&player->actor, NA_SE_EV_JUMP_OUT_WATER);
    return true;
}

static void UpdateDolphin(PlayState* play, Player* player) {
    LinkAnimation_Update(play, &player->skelAnime);
    Math_SmoothStepToS(&sSwimRoll, 0, 6, 0x7D0, 0x190);
    if (player->actor.yDistToWater > SWIM_EXIT_DEPTH && player->actor.velocity.y <= 0.0f) {
        player->stateFlags1 &= ~PLAYER_STATE1_JUMPING;
        player->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
        StartFastSwim(play, player, 4.0f);
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

static void SteerFastSwim(PlayState* play, Player* player, f32 speedTarget) {
    Input* input = &play->state.input[0];
    f32 curve = 1.0f - Math_CosS((s16)(input->rel.stick_x * 0x10E));
    s16 turn = (s16)CLAMP((input->rel.stick_x >= 0 ? 1 : -1) * curve * -1100.0f, -0x1F40, 0x1F40);

    Math_AsymStepToF(&sSwimSpeed, speedTarget, 1.0f, fabsf(sSwimSpeed) * 0.01f + 0.4f);
    player->yaw += turn;
    player->actor.world.rot.y = player->yaw;
    player->actor.shape.rot.y = player->yaw;
    player->linearVelocity = Math_CosS(sSwimPitch) * sSwimSpeed;
    player->actor.velocity.y = -Math_SinS(sSwimPitch) * sSwimSpeed;
}

static void CruiseFastSwim(PlayState* play, Player* player, f32* speedTarget) {
    Input* input = &play->state.input[0];
    s16 pitchTarget = (s16)(input->rel.stick_y * 0xC8);

    if (player->actor.bgCheckFlags & BG_TOUCHING_WALL) {
        sSwimSpeed *= 0.5f;
    }
    if (!CHECK_BTN_ALL(input->cur.button, BTN_A)) {
        sSwimPhase = SWIM_PHASE_EXIT;
        PlayAnim(play, player, ANIM_SWIM_TO_WAIT, 0.0f, ANIMMODE_ONCE, -6.0f);
        StopMmSfx(MM_ZORA_SWIM_LV);
        return;
    }
    *speedTarget = FAST_SWIM_SPEED;
    RefreshMmSfx(MM_ZORA_SWIM_LV, &player->actor.projectedPos);
    if (sSwimFloorTimer != 0) {
        sSwimFloorTimer--;
        pitchTarget = MIN(pitchTarget, (s16)(player->floorPitch - 0xFA0));
    }
    if (sSwimPitch >= DOLPHIN_PITCH && player->actor.yDistToWater < SWIM_SURFACE_DEPTH + 10.0f) {
        pitchTarget = MAX(pitchTarget, 0x7D0);
    }
    Math_SmoothStepToS(&sSwimPitch, pitchTarget, 4, 0xFA0, 0x190);
    s16 rollTarget = (s16)(input->rel.stick_x * 0x64);
    if (Math_ScaledStepToS(&sSwimTurn, rollTarget, 0x384) && rollTarget == 0) {
        Math_SmoothStepToS(&sSwimRoll, 0, 4, 0x5DC, 0x64);
    } else {
        sSwimRoll += sSwimTurn;
    }
    if (sSwimFloorTimer < 8 && IsGrounded(player)) {
        sSwimPitch += (s16)((-player->floorPitch - sSwimPitch) * 2);
        sSwimFloorTimer = 15;
        EffectSsGRipple_Spawn(play, &player->actor.world.pos, 50, 300, 0);
    }
}

// MM's Player_Action_56 in three phases: the barrel roll into the dash, the cruise, and the exit.
static void RunSwimMove(Player* player, PlayState* play) {
    Input* input = &play->state.input[0];
    f32 speedTarget = 0.0f;

    if (sMove == ZORA_MOVE_DOLPHIN) {
        UpdateDolphin(play, player);
        return;
    }
    ApplyBuoyancy(player);
    if (TryDolphinJump(play, player)) {
        return;
    }
    if (player->actor.yDistToWater <= 0.0f && IsGrounded(player)) {
        LeaveSwimMove(play, player);
        func_80839FFC(player, play);
        return;
    }
    bool finished = LinkAnimation_Update(play, &player->skelAnime);
    if (sSwimPhase == SWIM_PHASE_ROLL) {
        // Letting go of A at any point of the roll commits to leaving, as in MM, even if it is pressed again.
        if (!CHECK_BTN_ALL(input->cur.button, BTN_A)) {
            sSwimExitQueued = true;
        }
        if (finished) {
            bool keepGoing = !sSwimExitQueued;
            sSwimPhase = keepGoing ? SWIM_PHASE_CRUISE : SWIM_PHASE_EXIT;
            PlayAnim(play, player, keepGoing ? ANIM_FISH_SWIM : ANIM_SWIM_TO_WAIT, 0.0f,
                     keepGoing ? ANIMMODE_LOOP : ANIMMODE_ONCE, -6.0f);
            sSwimRoll = 0;
        } else if (player->skelAnime.curFrame >= FAST_SWIM_KICK_FRAME) {
            speedTarget = FAST_SWIM_KICK_SPEED;
            if (LinkAnimation_OnFrame(&player->skelAnime, FAST_SWIM_KICK_FRAME)) {
                sSwimSpeed = FAST_SWIM_BURST;
            }
        }
        Math_SmoothStepToS(&sSwimRoll, (s16)(input->rel.stick_x * 0xC8), 0xA, 0x3E8, 0x64);
    } else if (sSwimPhase == SWIM_PHASE_CRUISE) {
        CruiseFastSwim(play, player, &speedTarget);
    } else {
        Math_SmoothStepToS(&sSwimRoll, 0, 4, 0xFA0, 0x190);
        if (player->skelAnime.curFrame <= 5.0f && CHECK_BTN_ALL(input->press.button, BTN_A)) {
            StartFastSwim(play, player, 4.0f);
            return;
        }
        if (finished) {
            ReturnToSwimming(play, player);
            return;
        }
    }
    // MM keeps a smoothed copy of the bank (unk_B8E) only for the barrier; the body uses the raw one.
    Math_SmoothStepToS(&sSwimRollSmoothed, sSwimRoll, 2, 0x5DC, 0x64);
    SteerFastSwim(play, player, speedTarget);
}

// Swimming belongs to vanilla; only A (the dash) and B (the weight) are the Zora's, read at the end of the frame.
static void UpdateWaterButtons(PlayState* play, Player* player) {
    Input* input = &play->state.input[0];

    if (!IsInWater(player) || IsZoraBusy(player) || !CanAct(player) || player->interactRangeActor != NULL) {
        return;
    }
    bool isOnSeabed = IsGrounded(player) && sIsHeavy;
    // On the seabed A lets go of the weight and floats up; Z-targeted, it stays vanilla's side hop.
    if (isOnSeabed && CHECK_BTN_ALL(input->press.button, BTN_A) && !Player_IsZTargeting(player)) {
        SetHeavy(play, player, false);
        RefreshMmSfx(MM_FACE_UP, &player->actor.projectedPos);
        return;
    }
    if (!isOnSeabed && CHECK_BTN_ALL(input->press.button, BTN_A) && sAnims[ANIM_WATER_ROLL] != NULL &&
        player->actor.yDistToWater > SWIM_EXIT_DEPTH && HasSwimAbility()) {
        StartFastSwim(play, player, 4.0f);
        return;
    }
    if (!IsGrounded(player) && CHECK_BTN_ALL(input->press.button, BTN_B)) {
        SetHeavy(play, player, !sIsHeavy);
    }
}

// MM's z_player.c:9985: how fast the body cuts through the water, a dash counting its speed and its turn.
static s32 CountSwimBubbles(Player* player) {
    f32 drive =
        sMove == ZORA_MOVE_FAST_SWIM ? ABS(sSwimTurn) * -0.004f + sSwimSpeed * -0.38f : player->actor.velocity.y;
    bool isStandingHeavy = sIsHeavy && IsGrounded(player);

    if (drive > -1.0f || isStandingHeavy) {
        return Rand_ZeroOne() < 0.2f ? 1 : 0;
    }
    return MIN((s32)(drive * -0.3f), 8);
}

// The limbs that break the surface while dashing throw their own spray, as MM's z_player.c:9120 does.
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

// NEI's MmForm_SwimEffects, on top of vanilla's own water effects.
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

// Swimming hides the offer from Player_ActionHandler_2 (it refuses UNDERWATER), and the dash never runs it:
// NEI's MmForm_HandleFormInteractions hands it the offer with those flags lifted for the call.
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

// Link's own depths (swim from 56, wade from 36) suit his height; NEI's Zora swims from 45 and wades back at 30.
// The copy follows the player: a new scene or an age change rebuilds his properties.
static void KeepZoraBody(Player* player) {
    if (player->ageProperties == &sZoraBody) {
        return;
    }
    sZoraBody = *player->ageProperties;
    sZoraBody.unk_2C = SWIM_ENTER_DEPTH;
    sZoraBody.unk_24 = SWIM_EXIT_DEPTH;
    player->ageProperties = &sZoraBody;
}

// ---- barrier ----

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
    // MM turns the Zora's own cylinder off while the barrier is up: nothing reaches him through it.
    player->cylinder.base.acFlags &= ~(AC_ON | AC_HIT);
}

// MM's func_8082F164/func_8082F1AC: a flag beside whatever the Zora is doing, R in water and R+B on guard.
static void UpdateBarrier(PlayState* play, Player* player) {
    Input* input = &play->state.input[0];
    u16 buttons = IsInWater(player) ? BTN_R : (BTN_R | BTN_B);
    bool wasHeld = sBarrierHeld;
    s16 previous = sBarrierIntensity;

    sBarrierHeld = CHECK_BTN_ALL(input->cur.button, buttons) && gSaveContext.magic > 0 &&
                   (IsInWater(player) || IsGuarding()) && CanAct(player);
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
    // Segments 0x0A/0x0B carry the two scrolling layers of MM's object_link_zora_Matanimheader_012A80.
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

// ---- guitar ----

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

// MM's guitar sings through Sequence_0's ocarina channel. The hook runs right after Ocarina triggered its own
// voice, so stopping that here always lands.
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

// Runs inside the ocarina's own update; the pose consumes the edge on the game frame.
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

    LinkAnimation_Change(play, &player->skelAnime, anim, speed, speed >= 0.0f ? 0.0f : last,
                         speed >= 0.0f ? last : 0.0f, mode, morph);
}

// Vanilla's ocarina action waits for the end of its intro and outro clips; the guitar's own clips take their
// place, so that wait is what raises and lowers it, as MM's Player_Action_63 does.
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

// The strum loops: that same action reopens the ocarina whenever a clip reports its end, which would wipe
// the notes of the song being played.
static void PoseGuitar(PlayState* play, Player* player) {
    LinkAnimationHeader* strum = sAnims[ANIM_GUITAR_PLAY];
    f32 strumEnd = Animation_GetLastFrame(strum) - CLIP_FRAMES_PER_UPDATE;

    if (player->skelAnime.animation != strum) {
        ChangeGuitarClip(play, player, ANIM_GUITAR_PLAY, 1.0f, ANIMMODE_LOOP, -4.0f);
        player->skelAnime.playSpeed = 0.0f;
        sGuitarPhase = GUITAR_PLAYING;
    }
    bool isStrumDone = player->skelAnime.curFrame >= strumEnd;
    if (sWasNoteStruck) {
        if (isStrumDone) {
            player->skelAnime.curFrame = 0.0f;
        }
        player->skelAnime.playSpeed = 1.0f;
    } else if (isStrumDone) {
        player->skelAnime.playSpeed = 0.0f;
    }
    sWasNoteStruck = false;
}

// The ocarina stays out across the message session, so the latch only lets go once no message is up.
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
    // A raise that ended hands over to the strum even before an actor opens the session it was played for.
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

// ---- drawing ----

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
        Z64Aiming_DrawReticle(play, player, FIN_RETICLE_RANGE);
    }
}

// A Zora never speaks with Link's voice, even for a grunt MM's bank has no sample for.
static void SpeakWithZoraVoice(Actor* actor, uint16_t sfxId) {
    const uint16_t action = (uint16_t)(sfxId - NA_SE_VO_LI_SWORD_N);
    MmSfxPlayFunc play = GetMmSfxPlayer();

    if (action < LINK_VOICE_ACTIONS && play != NULL) {
        play((uint16_t)(NA_SE_VO_LI_SWORD_N + MM_ZORA_VOICE_OFFSET + action), &actor->projectedPos);
    }
}

// MM's Player_GetFloorSfxByAge: the floor offset plus the form's own block of the player bank.
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

// ---- lifetime ----

static bool IsZoraBusy(Player* player) {
    return player->actionFunc == ZoraStrikeAction || player->actionFunc == ZoraFinsAction ||
           player->actionFunc == ZoraGuardAction || IsSwimMove();
}

// Anything that takes the player (a hit, a textbox, an item, a cutscene) gets vanilla's action back.
static void DriveSwimMove(bool* should, va_list args) {
    Player* player = va_arg(args, Player*);

    if (!IsZora() || !IsSwimMove() || gPlayState == NULL) {
        return;
    }
    if (!CanAct(player)) {
        LeaveSwimMove(gPlayState, player);
        return;
    }
    // func_8083D53C runs before the action and lifts Link out of water once he is shallower than unk_24; a dash
    // caught there breaches as a dolphin jump if it is nosing up, and otherwise hands the body back to vanilla.
    if (sMove == ZORA_MOVE_FAST_SWIM && !IsInWater(player) && !TryDolphinJump(gPlayState, player)) {
        LeaveSwimMove(gPlayState, player);
        return;
    }
    *should = false;
    RunSwimMove(player, gPlayState);
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

static void EnterZora(PlayState* play, Player* player) {
    ResetState(play, player);
    sIsHeavy = false;
    sFinsOut = false;
    sFins[0] = sFins[1] = NULL;
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
    if (wasBusy) {
        func_80839FFC(player, play);
    }
}

// These ride vanilla's lock-on step instead of an action func of their own.
static bool IsUpperBodyMove(void) {
    return sMove == ZORA_MOVE_FINS_LOCKED_READY || sMove == ZORA_MOVE_FINS_LOCKED_THROW ||
           sMove == ZORA_MOVE_GUARD_WALK;
}

static void UpdateZora(PlayState* play, Player* player) {
    if (sMove != ZORA_MOVE_NONE && !IsUpperBodyMove() && !IsZoraBusy(player)) {
        if (IsSwimMove()) {
            LeaveSwimMove(play, player);
        }
        Z64Aiming_Release(player, play);
        player->stateFlags1 &= ~PLAYER_STATE1_SHIELDING;
        StopTrail(play);
        sMove = ZORA_MOVE_NONE;
    }
    // Only in water: the claim waives every hazard, and a hot room still burns a Zora.
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

// Colliders remember their actor, and a new scene brings a new player; its lights are rebuilt too.
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
}

// ---- mask ----

static bool CanWearMask(Player* player, PlayState* play) {
    return (IsGrounded(player) || IsInWater(player)) &&
           !(player->stateFlags1 & (PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_IN_CUTSCENE));
}

static void WearMask(PlayState* play, Player* player) {
    sApi->ToggleForm(ZORA_FORM_KEY);
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
    SOHCustomItemDefinition mask = Z64Items_Define(ZORA_MASK_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&mask, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    mask.flags |= SOH_CUSTOM_ITEM_INSTANT | SOH_CUSTOM_ITEM_WEARABLE;
    Z64Items_SetPlacement(&mask, ZORA_MASK_PAGE, ZORA_MASK_SLOT, 0);
    mask.getItemEntry = ItemTable_RetrieveEntry(MOD_NONE, GI_MASK_ZORA);
    mask.getItemEntry.textId = 0;
    mask.getItemEntry.drawFunc = DrawMaskGetItem;
    Z64Items_SetReplaces(&mask, ITEM_MASK_ZORA);
    Z64Items_SetVanillaMode(&mask, SOH_VANILLA_ITEM_REPLACE, RG_ZORA_MASK);
    Z64Items_SetTextbox(&mask, "You got the %rZora Mask%w!&It holds the spirit of a Zora guitarist who sang to "
                               "the sea.^Wear it with %y\xA1%w to become a Zora: swim like a fish and fight with "
                               "blade-like fins.");
    Z64Items_SetPauseText(&mask, "%rZora Mask&%wPress %y\xA1%w to become a Zora.&%y\xA0%w: punch  Hold %y\xA0%w: "
                                 "fins  %y\xA3%w: guard  %y\xA3%w+%y\xA0%w: barrier&In water: %y\x9F%w: dash  "
                                 "%y\xA0%w: sink");
    Z64Items_SetCanUse(&mask, CanWearMask);
    Z64Items_SetAction(&mask, WearMask, NULL);
    Z64Items_Register(sApi, &mask);
}

static const SOHFormActionOverride sActions[] = {
    { SOH_PLAYER_ACTION_MELEE, StartPunch },
    { SOH_PLAYER_ACTION_ZTARGET_A, HandleZTargetA },
    { SOH_PLAYER_ACTION_SHIELD, StartGuard },
};

// Standing still the Zora takes MM's own stance; walking, running and surface swimming stay Link's, as in NEI.
static SOHFormAnimOverride sAnimOverrides[] = {
    { PLAYER_ANIMGROUP_wait, -1, NULL },
};

static uint16_t ResolveZoraEquipment(int32_t equipType, uint16_t value) {
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
    // MM D_801C0E40[PLAYER_FORM_ZORA], drawn at 0.7 by the head limb.
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

SOH_MOD_EXPORT void ModInit(void) {
    LoadAnims();
    RegisterMask();
    RegisterForm();
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, ResolveZoraBody);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawZoraLimbExtras);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveFaceTextures, ResolveZoraFace);
    SOH_REGISTER_HOOK(sApi, OnPlayerFilterInput, FilterZoraInput);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorPlaySfx, ACTOR_PLAYER, ResolveZoraSfx);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDraw, ACTOR_EN_BOOM, DrawFlyingFin);
    SOH_REGISTER_HOOK(sApi, OnOcarinaNote, NoteStruck);
    SOH_REGISTER_HOOK(sApi, OnOcarinaPlaybackNote, PlaybackNoteStruck);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
    sApi->RegisterVB(VB_USE_STRENGTH_UPGRADE, ResolveZoraStrength);
    sApi->RegisterVB(VB_SWIM, GrantZoraSwim);
    sApi->RegisterVB(VB_EXECUTE_PLAYER_ACTION_FUNC, DriveSwimMove);
}
