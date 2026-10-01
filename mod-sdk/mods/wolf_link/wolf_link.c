// Wolf Link: the Shadow Crystal turns Link into Twilight Princess' wolf. Behaviour is the port of TP's wolf
// procs (Dusklight decomp, d_a_alink_wolf.inc); numbers keep TP's HIO tables and convert at the point of use.
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <libultraship/bridge/resourcebridge.h>
#include "soh/ResourceManagerHelpers.h"

#define WOLF_CRYSTAL_KEY "nei.shadow_crystal"
#define WOLF_FORM_KEY "nei.wolf_link"
#define WOLF_CRYSTAL_PAGE 0
#define WOLF_CRYSTAL_SLOT 20
#define WOLF_CRYSTAL_WHEEL_PRIORITY (-2)
#define WOLF_BLOB_PATH "objects/forms/wolf_link/gWolfLinkData"
#define CRYSTAL_GET_ITEM_SCALE 0.35f

#define BG_ON_GROUND 1
#define BG_TOUCHING_WALL 8

#define MAX_BONES 64
#define MAX_INFLUENCES 4
#define VERTS_PER_LOAD 30
#define FILE_VERTEX_STRIDE 20
#define ANIM_ENTRY_SIZE 16
#define AUDIO_NAME_LENGTH 32
#define AUDIO_ENTRY_SIZE (AUDIO_NAME_LENGTH + 12)
#define BLOB_MAGIC "NEIWOLF1"
#define BLOB_VERSION_MIN 1
#define BLOB_VERSION_AUDIO 2
#define BLOB_HEADER_SIZE (8 + 21 * 4)
#define MATERIAL_COMMANDS 24
#define MESH_SEGMENT 0x08
#define EXPORTED_VTX_SIZE 16

#define VOICE_SLOTS 4
#define VOICE_REQUESTS 4
#define MIX_RATE 32000.0f

#define MM_ANIM_VALUES_PER_FRAME 67
#define MM_ANIM_BASE_TRANSL_X (-57)

// Host cutscene frames to the flash peak: pose (PutOnLastFrame / TakeOffLastFrame) plus the 6-frame flash.
#define SHIFT_IN_PEAK_FRAMES (0x53 + 6)
#define SHIFT_OUT_PEAK_FRAMES (0x37 + 6)
#define SHIFT_IN_FADE_STEP 9
#define SHIFT_OUT_FADE_STEP 4
#define SHIFT_STUCK_FRAMES 120
#define EMERGE_MIN_SCALE 0.25f

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconShadowCrystalTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gShadowCrystalNameTex";
static const ALIGN_ASSET(2) char sCrystalDL[] = "__OTR__objects/object_nei_shadow_crystal/gNeiShadowCrystalDL";
static const ALIGN_ASSET(2) char sCurlPath[] = "__OTR__misc/link_animetion/gPlayerAnim_pg_maru_change_Data";

static const char* const sRequiredHooks[] = { "OnPlayerActionHandler", "OnPlayerResolveMotionScale",
                                              "OnActorDraw",           "OnActorResolveGrayscale",
                                              "OnActorPlaySfx",        "OnSceneInit",
                                              "OnPlayerUpdate" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

void Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void func_80839FFC(Player* player, PlayState* play);

typedef struct {
    f32 x;
    f32 y;
    f32 z;
    s8 normalX;
    s8 normalY;
    s8 normalZ;
    s16 texS;
    s16 texT;
    u8 alpha;
} WolfVertex;

typedef struct {
    u8 bone[MAX_INFLUENCES];
    u8 weight[MAX_INFLUENCES];
} WolfWeight;

typedef struct {
    f32 tx;
    f32 ty;
    f32 tz;
    f32 rx;
    f32 ry;
    f32 rz;
    f32 sx;
    f32 sy;
    f32 sz;
} WolfBoneFrame;

typedef struct {
    const char* name;
    u16 frameCount;
    const WolfBoneFrame* frames;
} WolfClip;

typedef struct {
    const s16* pcm;
    u32 sampleCount;
    f32 step;
    char name[AUDIO_NAME_LENGTH];
} WolfSound;

typedef struct {
    s32 sound;
    f32 position;
} VoiceSlot;

typedef enum {
    FIELD_VERTEX_COUNT,
    FIELD_TRIANGLE_COUNT,
    FIELD_BONE_COUNT,
    FIELD_ANIM_COUNT,
    FIELD_TEXTURE_WIDTH,
    FIELD_TEXTURE_HEIGHT,
    FIELD_VERTICES,
    FIELD_WEIGHTS,
    FIELD_PARENTS,
    FIELD_INV_BIND,
    FIELD_BONE_POS,
    FIELD_ANIM_ENTRIES,
    FIELD_NAMES,
    FIELD_NAMES_SIZE,
    FIELD_FRAMES,
    FIELD_FRAMES_SIZE,
    FIELD_TEXTURE,
    FIELD_TEXTURE_SIZE,
    FIELD_TOTAL_SIZE,
    FIELD_AUDIO,
} BlobField;

// TP BCK names as exported from the rig; the WANM comments are daAlink_WANM indices.
typedef enum {
    WANM_WAIT,                 // wl_waita
    WANM_WALK_A,               // wl_walka
    WANM_WALK_B,               // wl_walkb
    WANM_DASH_A,               // wl_dasha
    WANM_DASH_B,               // wl_dashb
    WANM_DASH_START,           // 0x73
    WANM_JUMP_ATTACK_START,    // 0x04, also the auto-jump takeoff
    WANM_JUMP_ATTACK,          // 0x05
    WANM_JUMP_ATTACK_END,      // 0x06
    WANM_FALL_LAND,            // 0x60
    WANM_ATTACK_B_LEFT,        // 0x40
    WANM_ATTACK_B_RIGHT,       // 0x41
    WANM_ATTACK_B_FRONT,       // 0x42
    WANM_ATTACK_B_TAIL,        // 0x43
    WANM_ATTACK_A_START,       // 0x50
    WANM_ATTACK_A,             // 0x51
    WANM_ATTACK_A_END,         // 0x52
    WANM_ATTACK_A_END_FRONT,   // 0x53
    WANM_ATTACK_A_END_BACK,    // 0x54
    WANM_CUT_TURN_LEFT,        // 0x5A
    WANM_CUT_TURN_RIGHT,       // 0x5B
    WANM_ATTACK_RECOIL_START,  // 0x74
    WANM_ATTACK_RECOIL_END,    // 0x75
    WANM_ATTACK_RECOIL_GROUND, // 0x7A
    WANM_DMG_FRONT,            // 0x3C
    WANM_DMG_BACK,             // 0x3D
    WANM_SIDE_JUMP_L_START,    // 0x48
    WANM_SIDE_JUMP_L_END,      // 0x49
    WANM_SIDE_JUMP_R_START,    // 0x4A
    WANM_SIDE_JUMP_R_END,      // 0x4B
    WANM_BACK_JUMP_START,      // 0x4C
    WANM_BACK_JUMP_END,        // 0x4D
    WANM_COUNT,
} WolfAnim;

static const char* const sAnimNames[WANM_COUNT] = {
    "wl_armature_wl_waita",     "wl_armature_wl_walka",      "wl_armature_wl_walkb",      "wl_armature_wl_dasha",
    "wl_armature_wl_dashb",     "wl_armature_wl_dashst",     "wl_armature_wl_jumpast",    "wl_armature_wl_jumpa",
    "wl_armature_wl_jumpaed",   "wl_armature_wl_landdama",   "wl_armature_wl_attackbl",   "wl_armature_wl_attackbr",
    "wl_armature_wl_attackbs",  "wl_armature_wl_attackbt",   "wl_armature_wl_attackast",  "wl_armature_wl_attacka",
    "wl_armature_wl_attackaed", "wl_armature_wl_attackaedf", "wl_armature_wl_attackaedb", "wl_armature_wl_cutstl",
    "wl_armature_wl_cutstr",    "wl_armature_wl_attackrest", "wl_armature_wl_attackreed", "wl_armature_wl_attackregd",
    "wl_armature_wl_damf",      "wl_armature_wl_damb",       "wl_armature_wl_atsjlst",    "wl_armature_wl_atsjled",
    "wl_armature_wl_atsjrst",   "wl_armature_wl_atsjred",    "wl_armature_wl_atsjbst",    "wl_armature_wl_atsjbed",
};

typedef enum {
    PROC_WOLF_MOVE,           // Link's own actions drive wait / walk / run / air
    PROC_WOLF_LAND,           // procWolfLand
    PROC_WOLF_DASH,           // procWolfDash
    PROC_WOLF_DASH_REVERSE,   // procWolfDashReverse
    PROC_WOLF_WAIT_ATTACK,    // procWolfWaitAttack
    PROC_WOLF_JUMP_ATTACK,    // procWolfJumpAttack
    PROC_WOLF_JUMP_AT_LAND,   // procWolfJumpAttack{Slide,Normal}Land
    PROC_WOLF_ROLL_ATTACK,    // procWolfRollAttack
    PROC_WOLF_ATTACK_REVERSE, // procWolfAttackReverse
    PROC_WOLF_DAMAGE,         // hurt pose over Link's own damage action
    PROC_WOLF_SIDESTEP,       // procWolfSideStep
    PROC_WOLF_SIDESTEP_LAND,  // procWolfSideStepLand
} WolfProc;

typedef enum {
    HOP_BACK,
    HOP_LEFT,
    HOP_RIGHT,
} WolfHop;

typedef enum {
    DIR_FORWARD,
    DIR_BACKWARD,
    DIR_LEFT,
    DIR_RIGHT,
    DIR_NONE,
} WolfDir;

typedef enum {
    ATTACK_BITE_LEFT,
    ATTACK_SCRATCH,
    ATTACK_TAIL,
    ATTACK_BITE_RIGHT,
} WolfWaitAttack;

typedef enum {
    LUNGE_NORMAL,
    LUNGE_FOLLOW_UP,
    LUNGE_STRONG,
    LUNGE_STRONG_FOLLOW_UP,
} WolfLunge;

typedef enum {
    SHIFT_NONE,
    SHIFT_TO_WOLF,
    SHIFT_TO_LINK,
} WolfShift;

// daAlinkHIO_anm_c: {endFrame, speed, startFrame, interpolation, cancelFrame}
typedef struct {
    f32 end;
    f32 speed;
    f32 start;
    f32 interp;
    f32 cancel;
} TpAnm;

typedef struct {
    TpAnm anm;
    s16 stopTime;
    s16 comboMidStopTime;
    f32 speed;
    f32 speedAddForward;
    f32 judgeStart;
    f32 judgeEnd;
    f32 comboMidCancel;
    f32 comboMidStart;
    f32 radiusOffset;
    f32 radius;
    f32 height;
} TpWaitAttack;

// daAlinkHIO_wlMoveNoP_c0::m
#define NOP_MAX_SPEED 25.0f
#define NOP_IDLE_ANM_SPEED 1.0f
#define NOP_WALK_ANM_SPEED 0.8f
#define NOP_JOG_ANM_SPEED 2.2f
#define NOP_RUN_ANM_SPEED 1.1f
#define NOP_IDLE_TO_WALK 0.1f
#define NOP_WALK_TO_JOG 0.6f
#define NOP_DECELERATION 1.8f

// daAlinkHIO_wlMove_c0::m
#define DASH_IDLE_ANM_SPEED 1.6f
#define DASH_WALK_ANM_SPEED 1.1f
#define DASH_BRISK_ANM_SPEED 2.2f
#define DASH_RUN_ANM_SPEED 1.2f
#define DASH_QUICK_RUN_ANM_SPEED 1.3f
#define DASH_IDLE_TO_WALK 0.1f
#define DASH_WALK_TO_BRISK 0.4f
#define DASH_RUN_TO_QUICK 0.5f
#define DASH_TURN_MAX 9000
#define DASH_TURN_MIN 100
#define DASH_TURN_RATE 5
#define A_DASH_DURATION 90
#define A_DASH_COOLDOWN 50
#define A_DASH_MAX_SPEED 45.0f
#define A_DASH_ACCELERATION 6.0f
#define A_DASH_INIT_SPEED 65.0f
#define DASH_REBOUND_H 20.0f
#define DASH_REBOUND_V 15.0f
#define TP_HUMAN_RUN 23.0f
static const TpAnm kADashAnm = { 8, 1.0f, 0, 1, 20 };
static const TpAnm kDashReboundAnm = { 41, 1.0f, 0, 3, 20 };

// daAlinkHIO_wlAtWa{Lr,Sc,Tl}_c0::m
static const TpWaitAttack kAtWaLr = {
    { 41, 0.9f, 4, 3, 16 }, 5, 3, 0.0f, 10.0f, 4.0f, 11.0f, 18.0f, 5.0f, 70, 70, 150
};
static const TpWaitAttack kAtWaSc = {
    { 15, 0.9f, 0, 3, 15 }, 5, 5, 10.0f, 3.0f, 5.0f, 11.0f, 18.0f, 0.0f, 100, 85, 150
};
static const TpWaitAttack kAtWaTl = {
    { 42, 1.05f, 3, 3, 28 }, 0, 3, 10.0f, 5.0f, 10.0f, 14.0f, 25.0f, 0.0f, 40, 150, 100
};

// daAlinkHIO_wlAtNjump_c0::m
static const TpAnm kNjumpAerialAnm = { 6, 1.0f, 4, 3, 7 };
#define NJUMP_INIT_SPEED 30.0f
#define NJUMP_MAX_H 40.0f
#define NJUMP_MIN_H 10.0f
#define NJUMP_MAX_V 23.0f
#define NJUMP_MIN_V 17.0f
#define NJUMP_AERIAL_ANM_SPEED 0.8f
#define NJUMP_RADIUS_OFFSET 80.0f
#define NJUMP_RADIUS 60.0f
#define NJUMP_HEIGHT 120.0f

// daAlinkHIO_wlAtLand_c0::m
static const TpAnm kLandNormalAnm = { 19, 0.9f, 0, 2, 2 };
static const TpAnm kLandFrontSlideAnm = { 14, 1.0f, 0, 3, 1 };
static const TpAnm kLandBackSlideAnm = { 19, 1.1f, 0, 2, 1 };
#define LAND_SLIDE_DECEL 2.0f

// daAlinkHIO_wlAttack_c0::m
static const TpAnm kJumpBackLandAnm = { 59, 1.2f, 0, 2, 5 };
#define COMBO_DURATION 5
#define COMBO_FINISHER 4
#define JUMP_BACK_SPEED_H 10.0f
#define JUMP_BACK_SPEED_V 12.0f

// daAlinkHIO_wlSideStep_c0::m
static const TpAnm kSideJumpAnm = { 5, 1.0f, 0, 3, 6 };
static const TpAnm kSideLandAnm = { 23, 1.0f, 1, 2, 2 };
static const TpAnm kBackJumpAnm = { 4, 0.9f, 0, 3, 5 };
static const TpAnm kBackLandAnm = { 23, 1.0f, 1, 2, 3 };
#define SIDE_JUMP_H 33.0f
#define SIDE_JUMP_V 23.0f
#define BACK_JUMP_H 23.0f
#define BACK_JUMP_V 30.0f

// daAlinkHIO_wlAtRoll_c0::m
static const TpAnm kRollAnm = { 40, 1.0f, 4, 3, 23 };
#define ROLL_RADIUS 250.0f
#define ROLL_HEIGHT 155.0f
#define ROLL_SPEED 20.0f
#define ROLL_RADIUS_STEP 20.0f
#define ROLL_ACTIVE_START 4.0f
#define ROLL_ACTIVE_END 13.0f

// daAlinkHIO_wlAutoJump_c0::m
static const TpAnm kAutoJumpAnm = { 3, 1.2f, 1, 2, 4 };
static const TpAnm kAutoLandAnm = { 24, 1.0f, 1, 2, 2 };
static const TpAnm kAutoClimbAnm = { 5, 0.5f, 2, 5, 7 };
#define TP_WOLF_GRAVITY (-3.6f)
#define AIR_ANM_TRANSITION_HEIGHT 300.0f

// OoT Link's full-stick run target; only a reference for TP's wolf/human ratios.
#define OOT_RUN_SPEED 6.58f
#define OOT_GRAVITY (-1.0f)
#define TP_TO_OOT_FRAMES (20.0f / 30.0f)

#define STICK_DEADZONE 20.0f
#define STICK_STEER 8.0f
#define MOVING_SPEED 0.5f
#define DASH_WALL_FRAME 3.0f
#define DASH_REBOUND_HOLD_FRAME 5.0f
#define LAND_SLIDE_CANCEL_SPEED 5.0f
#define LAND_SLIDE_STOP_SPEED 0.1f
#define LUNGE_TARGET_EYE 10.0f
#define JUDGE_FRAMES 2

#define BODY_HEIGHT 85.0f
#define BODY_CENTER 0.7f
#define HEAD_HEIGHT 110.0f
#define PAW_SHADOW_SCALE 0.75f
#define HIND_SHADOW_MAX_HEIGHT 20.0f
#define MIN_ATTACK_RADIUS 8.0f
#define MIN_ATTACK_HEIGHT 10.0f
#define DAMAGE_WEAK 2
#define DAMAGE_STRONG 4

// Paw bones in the exported rig: FlegL4, FlegR4, BlegL4, BlegR4.
static const s32 kPawBones[4] = { 19, 24, 31, 36 };

typedef struct {
    f32 stickMag;
    s16 stickWorldYaw;
    bool aPress;
    bool bPress;
    bool blocked;
} WolfInput;

typedef struct {
    s32 clipIndex[WANM_COUNT];
    WolfAnim anim;
    const WolfClip* clip;
    f32 frame;
    f32 prevFrame;
    f32 rate;
    f32 end;
    bool loop;

    WolfProc proc;
    bool ownsPlayer;
    s16 dashModeTimer;
    s16 dashCooldown;
    bool dashAttackQueued;
    s32 comboCount;
    s16 comboWindow;
    bool comboReserved;

    s16 stopTimer;
    f32 cancelFrame;
    f32 judgeStart;
    f32 judgeEnd;
    f32 speedAddFrame;
    f32 attackSpeed;
    f32 radiusOffset;
    WolfWaitAttack waitAttack;
    WolfLunge lunge;
    WolfHop hop;
    bool lungeHit;
    bool airborne;
    bool slide;
    bool aerialLoop;
    bool pastApex;
    f32 fallStartY;
    bool wasOnGround;
    bool wasInvincible;
    bool attackActive;
} WolfState;

static const SOHModApi* sApi;
static WolfState sWolf;
static bool sIsWolf;

static bool sAssetsLoaded;
static bool sAssetsFailed;
static u8* sBlob;
static u32 sBlobSize;
static u32 sBoneCount;
static u32 sVertexCount;
static WolfVertex* sVertices;
static const WolfWeight* sWeights;
static const MtxF* sInvBind;
static s16 sBoneParent[MAX_BONES];
static u8 sBoneOrder[MAX_BONES];
static u16* sTexture;
static WolfClip* sClips;
static u32 sClipCount;
static Vtx* sVertexBuffer[2];
static u8 sVertexBufferIndex;
static Gfx* sMeshDL;
static Gfx sMaterialDL[MATERIAL_COMMANDS];
static MtxF sBoneWorld[MAX_BONES];
static MtxF sSkinMatrix[MAX_BONES];
static bool sHasPose;

static WolfSound* sSounds;
static u32 sSoundCount;
static VoiceSlot sVoiceSlots[VOICE_SLOTS];
static volatile s32 sVoiceRequests[VOICE_REQUESTS] = { -1, -1, -1, -1 };

static ColliderCylinder sAttackCylinder;
static bool sAttackReady;

static WolfShift sShift;
static bool sShiftPastPeak;
static s32 sShiftFrames;
static u8 sBlackout;
static f32 sEmerge = 1.0f;

static LinkAnimationHeader sCurlAnim;
static s16* sCurlFrames;

static ColliderCylinderInit sAttackInit = {
    { COLTYPE_HIT8, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0,
      { 0xFFCFFFFF, 0x00, 0x04 },
      { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { 20, 30, 0, { 0, 0, 0 } },
};

static f32 SpeedScale(void) {
    return CVarGetFloat("gMods.WolfLink.SpeedScale", 1.0f);
}

static f32 RenderScale(void) {
    f32 scale = CVarGetFloat("gMods.WolfLink.Scale", 0.3f);
    return scale < 0.05f ? 0.3f : scale;
}

// TP per-30Hz-frame horizontal speed to OoT per-20Hz-frame, relative to Link's run.
static f32 TpSpeed(f32 tp) {
    return tp * (OOT_RUN_SPEED / TP_HUMAN_RUN) * (30.0f / 20.0f) * SpeedScale();
}

// Vertical launches keep TP's flight time (t = 2v/g) under OoT's gravity.
static f32 TpVSpeed(f32 tp) {
    return tp * (OOT_GRAVITY / TP_WOLF_GRAVITY) * (20.0f / 30.0f);
}

static f32 TpAnimRate(f32 tp) {
    return tp * 1.5f;
}

static s16 TpFrames(f32 tp) {
    return (s16)(tp * TP_TO_OOT_FRAMES + 0.5f);
}

// Collider extents are in TP units, the units the mesh is in.
static f32 TpLength(f32 tp) {
    return tp * RenderScale();
}

static u16 ReadU16(const u8* p) {
    u16 value;
    memcpy(&value, p, sizeof(value));
    return value;
}

static s16 ReadS16(const u8* p) {
    s16 value;
    memcpy(&value, p, sizeof(value));
    return value;
}

static u32 ReadU32(const u8* p) {
    u32 value;
    memcpy(&value, p, sizeof(value));
    return value;
}

static f32 ReadF32(const u8* p) {
    f32 value;
    memcpy(&value, p, sizeof(value));
    return value;
}

static u32 BlobField_Get(BlobField field) {
    return ReadU32(sBlob + 12 + field * 4);
}

static bool IsInBlob(u32 offset, u32 size) {
    return offset <= sBlobSize && size <= sBlobSize - offset;
}

static bool IsPowerOfTwoTexture(u32 size) {
    return size >= 8 && size <= 1024 && (size & (size - 1)) == 0;
}

static u32 Log2(u32 value) {
    u32 bits = 0;
    while ((1u << bits) < value) {
        bits++;
    }
    return bits;
}

// The exporter packs vertices into 20 bytes; WolfVertex is padded, so it is unpacked field by field.
static bool UnpackVertices(void) {
    u32 offset = BlobField_Get(FIELD_VERTICES);
    sVertices = calloc(sVertexCount, sizeof(WolfVertex));
    if (sVertices == NULL) {
        return false;
    }
    for (u32 i = 0; i < sVertexCount; i++) {
        const u8* v = sBlob + offset + i * FILE_VERTEX_STRIDE;
        WolfVertex* out = &sVertices[i];
        out->x = ReadF32(v + 0);
        out->y = ReadF32(v + 4);
        out->z = ReadF32(v + 8);
        out->normalX = (s8)v[12];
        out->normalY = (s8)v[13];
        out->normalZ = (s8)v[14];
        out->texS = ReadS16(v + 15);
        out->texT = ReadS16(v + 17);
        out->alpha = v[19];
    }
    return true;
}

// Fast3D reads RGBA16 big-endian; the exporter writes little-endian texels.
static bool UnpackTexture(u32 width, u32 height) {
    u32 offset = BlobField_Get(FIELD_TEXTURE);
    u32 texels = width * height;
    sTexture = malloc(texels * sizeof(u16));
    if (sTexture == NULL) {
        return false;
    }
    for (u32 i = 0; i < texels; i++) {
        u16 texel = ReadU16(sBlob + offset + i * 2);
        sTexture[i] = (u16)((texel >> 8) | (texel << 8));
    }
    return true;
}

// Parents first, whatever order the armature was exported in.
static bool BuildBoneOrder(void) {
    u32 offset = BlobField_Get(FIELD_PARENTS);
    bool placed[MAX_BONES] = { false };
    u32 count = 0;

    for (u32 bone = 0; bone < sBoneCount; bone++) {
        sBoneParent[bone] = ReadS16(sBlob + offset + bone * 2);
        if (sBoneParent[bone] >= (s16)sBoneCount) {
            return false;
        }
    }
    while (count < sBoneCount) {
        u32 before = count;
        for (u32 bone = 0; bone < sBoneCount; bone++) {
            s16 parent = sBoneParent[bone];
            if (placed[bone] || (parent >= 0 && !placed[parent])) {
                continue;
            }
            sBoneOrder[count] = (u8)bone;
            count++;
            placed[bone] = true;
        }
        if (count == before) {
            return false;
        }
    }
    return true;
}

static bool ParseClips(void) {
    u32 entries = BlobField_Get(FIELD_ANIM_ENTRIES);
    u32 names = BlobField_Get(FIELD_NAMES);
    u32 namesSize = BlobField_Get(FIELD_NAMES_SIZE);

    sClips = calloc(sClipCount, sizeof(WolfClip));
    if (sClips == NULL) {
        return false;
    }
    for (u32 i = 0; i < sClipCount; i++) {
        const u8* entry = sBlob + entries + i * ANIM_ENTRY_SIZE;
        u32 nameOffset = ReadU32(entry);
        u16 frameCount = ReadU16(entry + 4);
        u16 clipBones = ReadU16(entry + 6);
        u32 frameOffset = ReadU32(entry + 12);
        u64 bytes = (u64)frameCount * clipBones * sizeof(WolfBoneFrame);
        if (nameOffset < names || nameOffset >= names + namesSize || clipBones != sBoneCount || frameCount == 0 ||
            bytes > UINT32_MAX || !IsInBlob(frameOffset, (u32)bytes)) {
            return false;
        }
        sClips[i].name = (const char*)sBlob + nameOffset;
        sClips[i].frameCount = frameCount;
        sClips[i].frames = (const WolfBoneFrame*)(sBlob + frameOffset);
    }
    return true;
}

// Chunk = u32 count + 44-byte entries (name, rate, samples, PCM offset from the chunk) + mono s16.
static void ParseSounds(u32 offset) {
    if (offset == 0 || !IsInBlob(offset, 4)) {
        return;
    }
    u32 count = ReadU32(sBlob + offset);
    sSounds = calloc(count, sizeof(WolfSound));
    if (sSounds == NULL) {
        return;
    }
    for (u32 i = 0; i < count; i++) {
        u32 entry = offset + 4 + i * AUDIO_ENTRY_SIZE;
        if (!IsInBlob(entry, AUDIO_ENTRY_SIZE) || memchr(sBlob + entry, '\0', AUDIO_NAME_LENGTH) == NULL) {
            return;
        }
        u32 rate = ReadU32(sBlob + entry + AUDIO_NAME_LENGTH);
        u32 samples = ReadU32(sBlob + entry + AUDIO_NAME_LENGTH + 4);
        u32 pcm = offset + ReadU32(sBlob + entry + AUDIO_NAME_LENGTH + 8);
        if (rate == 0 || samples == 0 || !IsInBlob(pcm, samples * 2)) {
            continue;
        }
        WolfSound* sound = &sSounds[sSoundCount];
        memcpy(sound->name, sBlob + entry, AUDIO_NAME_LENGTH);
        sound->pcm = (const s16*)(sBlob + pcm);
        sound->sampleCount = samples;
        sound->step = rate / MIX_RATE;
        sSoundCount++;
    }
}

static bool IsBlobHeaderValid(void) {
    if (sBlobSize < BLOB_HEADER_SIZE || memcmp(sBlob, BLOB_MAGIC, 8) != 0) {
        return false;
    }
    u32 version = ReadU32(sBlob + 8);
    u32 width = BlobField_Get(FIELD_TEXTURE_WIDTH);
    u32 height = BlobField_Get(FIELD_TEXTURE_HEIGHT);
    u32 vertexCount = BlobField_Get(FIELD_VERTEX_COUNT);
    u32 boneCount = BlobField_Get(FIELD_BONE_COUNT);

    return version >= BLOB_VERSION_MIN && version <= BLOB_VERSION_AUDIO &&
           BlobField_Get(FIELD_TOTAL_SIZE) == sBlobSize && vertexCount == BlobField_Get(FIELD_TRIANGLE_COUNT) * 3 &&
           vertexCount > 0 && vertexCount <= 65535 && boneCount > 0 && boneCount <= MAX_BONES &&
           BlobField_Get(FIELD_ANIM_COUNT) > 0 && IsPowerOfTwoTexture(width) && IsPowerOfTwoTexture(height) &&
           BlobField_Get(FIELD_TEXTURE_SIZE) == width * height * 2 &&
           IsInBlob(BlobField_Get(FIELD_VERTICES), vertexCount * FILE_VERTEX_STRIDE) &&
           IsInBlob(BlobField_Get(FIELD_WEIGHTS), vertexCount * sizeof(WolfWeight)) &&
           IsInBlob(BlobField_Get(FIELD_PARENTS), boneCount * 2) &&
           IsInBlob(BlobField_Get(FIELD_INV_BIND), boneCount * sizeof(MtxF)) &&
           IsInBlob(BlobField_Get(FIELD_ANIM_ENTRIES), BlobField_Get(FIELD_ANIM_COUNT) * ANIM_ENTRY_SIZE) &&
           IsInBlob(BlobField_Get(FIELD_NAMES), BlobField_Get(FIELD_NAMES_SIZE)) &&
           IsInBlob(BlobField_Get(FIELD_TEXTURE), BlobField_Get(FIELD_TEXTURE_SIZE));
}

static void BuildMeshDisplayList(void) {
    u32 commands = 1;
    for (u32 start = 0; start < sVertexCount; start += VERTS_PER_LOAD) {
        u32 count = MIN(VERTS_PER_LOAD, sVertexCount - start);
        commands += 1 + ((count / 3) + 1) / 2;
    }
    sMeshDL = calloc(commands, sizeof(Gfx));
    Gfx* gfx = sMeshDL;
    for (u32 start = 0; start < sVertexCount; start += VERTS_PER_LOAD) {
        u32 count = MIN(VERTS_PER_LOAD, sVertexCount - start);
        u32 triangles = count / 3;
        u32 tri = 0;
        // Raw command: gSPVertex would probe the segmented token for an OTR path. Bit 0 marks it segmented, and
        // the interpreter reads the offset in exported 16-byte vertices and rescales it to the 24-byte Vtx.
        __gSPVertex(gfx++, (uintptr_t)((MESH_SEGMENT << 24) + 1 + start * EXPORTED_VTX_SIZE), count, 0);
        for (; tri + 1 < triangles; tri += 2) {
            u32 a = tri * 3;
            u32 b = a + 3;
            gSP2Triangles(gfx++, a, a + 1, a + 2, 0, b, b + 1, b + 2, 0);
        }
        if (tri < triangles) {
            u32 a = tri * 3;
            gSP1Triangle(gfx++, a, a + 1, a + 2, 0);
        }
    }
    gSPEndDisplayList(gfx++);
}

// Every triangle is exported twice with opposite winding, so back-face culling is the two-sided lighting.
static void BuildMaterialDisplayList(u32 width, u32 height) {
    Gfx* gfx = sMaterialDL;
    u32 geometryMode = G_SHADING_SMOOTH | G_LIGHTING | G_SHADE | G_FOG | G_ZBUFFER;
    if (CVarGetInteger("gMods.WolfLink.CullBack", 1)) {
        geometryMode |= G_CULL_BACK;
    }
    gSPLoadGeometryMode(gfx++, geometryMode);
    gDPPipeSync(gfx++);
    gDPSetCombineLERP(gfx++, TEXEL0, 0, SHADE, 0, 0, 0, 0, 1, COMBINED, 0, PRIMITIVE, 0, 0, 0, 0, COMBINED);
    gSPSetOtherMode(gfx++, G_SETOTHERMODE_H, 4, 20,
                    G_TF_BILERP | G_TC_FILT | G_TP_PERSP | G_TT_NONE | G_AD_NOISE | G_PM_NPRIMITIVE | G_CK_NONE |
                        G_TD_CLAMP | G_CYC_2CYCLE | G_CD_MAGICSQ | G_TL_TILE);
    gSPSetOtherMode(gfx++, G_SETOTHERMODE_L, 0, 32, G_RM_FOG_SHADE_A | G_AC_NONE | G_RM_AA_ZB_OPA_SURF2 | G_ZS_PIXEL);
    gSPTexture(gfx++, 65535, 65535, 0, 0, 1);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    // LoadBlock caps at 4095 texels; LoadTile takes the real width and handles 256x256.
    gDPSetTextureImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, sTexture);
    gDPSetTile(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, G_TX_WRAP | G_TX_NOMIRROR, 0, 0,
               G_TX_WRAP | G_TX_NOMIRROR, 0, 0);
    gDPLoadSync(gfx++);
    gDPLoadTile(gfx++, G_TX_LOADTILE, 0, 0, (width - 1) << G_TEXTURE_IMAGE_FRAC, (height - 1) << G_TEXTURE_IMAGE_FRAC);
    gDPPipeSync(gfx++);
    gDPSetTile(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, (width * 2) / 8, 0, G_TX_RENDERTILE, 0, G_TX_WRAP | G_TX_NOMIRROR,
               Log2(height), 0, G_TX_WRAP | G_TX_NOMIRROR, Log2(width), 0);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, (width - 1) << G_TEXTURE_IMAGE_FRAC,
                   (height - 1) << G_TEXTURE_IMAGE_FRAC);
    gSPEndDisplayList(gfx++);
}

static bool AllocateVertexBuffers(void) {
    for (s32 i = 0; i < 2; i++) {
        sVertexBuffer[i] = calloc(sVertexCount, sizeof(Vtx));
        if (sVertexBuffer[i] == NULL) {
            return false;
        }
        for (u32 v = 0; v < sVertexCount; v++) {
            sVertexBuffer[i][v].n.tc[0] = sVertices[v].texS;
            sVertexBuffer[i][v].n.tc[1] = sVertices[v].texT;
            sVertexBuffer[i][v].n.a = sVertices[v].alpha;
        }
    }
    return true;
}

static bool CopyBlobResource(void) {
    const u8* data = ResourceGetDataByName(WOLF_BLOB_PATH);
    size_t size = ResourceGetSizeByName(WOLF_BLOB_PATH);
    if (data == NULL || size == 0 || size > UINT32_MAX) {
        return false;
    }
    sBlob = malloc(size);
    if (sBlob == NULL) {
        return false;
    }
    memcpy(sBlob, data, size);
    sBlobSize = (u32)size;
    return true;
}

// Loaded once and kept: the audio thread reads sounds straight out of the blob.
static bool LoadAssets(void) {
    if (sAssetsLoaded || sAssetsFailed) {
        return sAssetsLoaded;
    }
    sAssetsFailed = true;
    if (!CopyBlobResource() || !IsBlobHeaderValid()) {
        sApi->Log("wolf_link: missing or invalid " WOLF_BLOB_PATH);
        return false;
    }
    u32 width = BlobField_Get(FIELD_TEXTURE_WIDTH);
    u32 height = BlobField_Get(FIELD_TEXTURE_HEIGHT);
    sVertexCount = BlobField_Get(FIELD_VERTEX_COUNT);
    sBoneCount = BlobField_Get(FIELD_BONE_COUNT);
    sClipCount = BlobField_Get(FIELD_ANIM_COUNT);
    sWeights = (const WolfWeight*)(sBlob + BlobField_Get(FIELD_WEIGHTS));
    sInvBind = (const MtxF*)(sBlob + BlobField_Get(FIELD_INV_BIND));
    if (!UnpackVertices() || !UnpackTexture(width, height) || !BuildBoneOrder() || !ParseClips() ||
        !AllocateVertexBuffers()) {
        sApi->Log("wolf_link: the wolf blob does not parse");
        return false;
    }
    for (s32 anim = 0; anim < WANM_COUNT; anim++) {
        sWolf.clipIndex[anim] = -1;
        for (u32 clip = 0; clip < sClipCount; clip++) {
            if (strcmp(sClips[clip].name, sAnimNames[anim]) == 0) {
                sWolf.clipIndex[anim] = (s32)clip;
                break;
            }
        }
        if (sWolf.clipIndex[anim] < 0) {
            sApi->Log("wolf_link: the wolf blob lacks a clip");
            return false;
        }
    }
    BuildMeshDisplayList();
    BuildMaterialDisplayList(width, height);
    if (ReadU32(sBlob + 8) >= BLOB_VERSION_AUDIO) {
        ParseSounds(BlobField_Get(FIELD_AUDIO));
    }
    sAssetsFailed = false;
    sAssetsLoaded = true;
    return true;
}

// T(pos) x R(euler ZYX) x S in the column-major mf[col][row] the rig was exported for.
static void BuildLocalMatrix(const WolfBoneFrame* frame, MtxF* out) {
    f32 degToRad = 3.14159265358979f / 180.0f;
    f32 cx = cosf(frame->rx * degToRad);
    f32 sx = sinf(frame->rx * degToRad);
    f32 cy = cosf(frame->ry * degToRad);
    f32 sy = sinf(frame->ry * degToRad);
    f32 cz = cosf(frame->rz * degToRad);
    f32 sz = sinf(frame->rz * degToRad);

    memset(out, 0, sizeof(MtxF));
    out->mf[0][0] = cy * cz * frame->sx;
    out->mf[0][1] = cy * sz * frame->sx;
    out->mf[0][2] = -sy * frame->sx;
    out->mf[1][0] = (sx * sy * cz - cx * sz) * frame->sy;
    out->mf[1][1] = (sx * sy * sz + cx * cz) * frame->sy;
    out->mf[1][2] = sx * cy * frame->sy;
    out->mf[2][0] = (cx * sy * cz + sx * sz) * frame->sz;
    out->mf[2][1] = (cx * sy * sz - sx * cz) * frame->sz;
    out->mf[2][2] = cx * cy * frame->sz;
    out->mf[3][0] = frame->tx;
    out->mf[3][1] = frame->ty;
    out->mf[3][2] = frame->tz;
    out->mf[3][3] = 1.0f;
}

static f32 LerpAngle(f32 from, f32 to, f32 t) {
    f32 delta = fmodf(to - from + 540.0f, 360.0f) - 180.0f;
    return from + delta * t;
}

// 30 fps clips advance 1.5 frames a tick: each bone blends between the frames around the fractional one.
static void BlendBoneFrame(const WolfBoneFrame* a, const WolfBoneFrame* b, f32 t, WolfBoneFrame* out) {
    out->tx = a->tx + (b->tx - a->tx) * t;
    out->ty = a->ty + (b->ty - a->ty) * t;
    out->tz = a->tz + (b->tz - a->tz) * t;
    out->rx = LerpAngle(a->rx, b->rx, t);
    out->ry = LerpAngle(a->ry, b->ry, t);
    out->rz = LerpAngle(a->rz, b->rz, t);
    out->sx = a->sx + (b->sx - a->sx) * t;
    out->sy = a->sy + (b->sy - a->sy) * t;
    out->sz = a->sz + (b->sz - a->sz) * t;
}

static void ComputeBoneMatrices(void) {
    const WolfClip* clip = sWolf.clip;
    u16 frame = (u16)MIN(sWolf.frame, clip->frameCount - 1);
    u16 next = MIN(frame + 1, clip->frameCount - 1);
    f32 blend = CLAMP(sWolf.frame - frame, 0.0f, 1.0f);

    for (u32 i = 0; i < sBoneCount; i++) {
        u8 bone = sBoneOrder[i];
        WolfBoneFrame blended;
        MtxF local;
        BlendBoneFrame(&clip->frames[frame * sBoneCount + bone], &clip->frames[next * sBoneCount + bone], blend,
                       &blended);
        BuildLocalMatrix(&blended, &local);
        if (sBoneParent[bone] < 0) {
            sBoneWorld[bone] = local;
        } else {
            SkinMatrix_MtxFMtxFMult(&sBoneWorld[sBoneParent[bone]], &local, &sBoneWorld[bone]);
        }
        SkinMatrix_MtxFMtxFMult(&sBoneWorld[bone], (MtxF*)&sInvBind[bone], &sSkinMatrix[bone]);
    }
    sHasPose = true;
}

static void TransformDirection(const MtxF* m, const Vec3f* in, Vec3f* out) {
    out->x = in->x * m->xx + in->y * m->xy + in->z * m->xz;
    out->y = in->x * m->yx + in->y * m->yy + in->z * m->yz;
    out->z = in->x * m->zx + in->y * m->zy + in->z * m->zz;
}

static void SkinVertices(Vtx* out) {
    for (u32 v = 0; v < sVertexCount; v++) {
        const WolfVertex* rest = &sVertices[v];
        const WolfWeight* weights = &sWeights[v];
        Vec3f restPos = { rest->x, rest->y, rest->z };
        Vec3f restNormal = { rest->normalX, rest->normalY, rest->normalZ };
        Vec3f pos = { 0.0f, 0.0f, 0.0f };
        Vec3f normal = { 0.0f, 0.0f, 0.0f };

        for (s32 j = 0; j < MAX_INFLUENCES && weights->weight[j] != 0; j++) {
            MtxF* skin = &sSkinMatrix[weights->bone[j]];
            f32 w = weights->weight[j] * (1.0f / 255.0f);
            Vec3f moved;
            Vec3f turned;
            SkinMatrix_Vec3fMtxFMultXYZ(skin, &restPos, &moved);
            TransformDirection(skin, &restNormal, &turned);
            pos.x += moved.x * w;
            pos.y += moved.y * w;
            pos.z += moved.z * w;
            normal.x += turned.x * w;
            normal.y += turned.y * w;
            normal.z += turned.z * w;
        }
        out[v].n.ob[0] = (s16)pos.x;
        out[v].n.ob[1] = (s16)pos.y;
        out[v].n.ob[2] = (s16)pos.z;
        f32 length = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        if (length > 0.001f) {
            out[v].n.n[0] = (s8)(normal.x / length * 127.0f);
            out[v].n.n[1] = (s8)(normal.y / length * 127.0f);
            out[v].n.n[2] = (s8)(normal.z / length * 127.0f);
        }
    }
}

static f32 EmergeScale(void) {
    return EMERGE_MIN_SCALE + (1.0f - EMERGE_MIN_SCALE) * sEmerge;
}

static void DrawWolfMesh(PlayState* play, Player* player) {
    if (sWolf.clip == NULL) {
        return;
    }
    f32 scale = RenderScale() * EmergeScale();
    Vtx* vertices = sVertexBuffer[sVertexBufferIndex];

    ComputeBoneMatrices();
    SkinVertices(vertices);
    sVertexBufferIndex ^= 1;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)gCullBackDList);
    gSPDisplayList(POLY_OPA_DISP++, sMaterialDL);
    Matrix_SetTranslateRotateYXZ(player->actor.world.pos.x, player->actor.world.pos.y, player->actor.world.pos.z,
                                 &player->actor.shape.rot);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPSegment(POLY_OPA_DISP++, MESH_SEGMENT, (uintptr_t)vertices);
    gSPDisplayList(POLY_OPA_DISP++, sMeshDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// World position of a paw from the last skinning pass: model space through the actor's pos, yaw and scale.
static bool GetPawWorldPos(Player* player, s32 paw, Vec3f* out) {
    if (!sHasPose) {
        return false;
    }
    const MtxF* bone = &sBoneWorld[kPawBones[paw]];
    f32 scale = RenderScale() * EmergeScale();
    f32 sin = Math_SinS(player->actor.shape.rot.y);
    f32 cos = Math_CosS(player->actor.shape.rot.y);
    f32 x = bone->xw * scale;
    f32 z = bone->zw * scale;

    out->x = player->actor.world.pos.x + x * cos + z * sin;
    out->y = player->actor.world.pos.y + bone->yw * scale;
    out->z = player->actor.world.pos.z - x * sin + z * cos;
    return true;
}

// DrawFeet only knows two feet: it runs twice, front paws then hind paws, swapping feetPos underneath it.
static void DrawWolfShadow(Actor* actor, Lights* lights, PlayState* play) {
    Player* player = (Player*)actor;
    Vec3f paws[4];
    for (s32 i = 0; i < 4; i++) {
        if (!GetPawWorldPos(player, i, &paws[i])) {
            ActorShadow_DrawFeet(actor, lights, play);
            return;
        }
    }
    f32 shadowScale = actor->shape.shadowScale;
    actor->shape.shadowScale = shadowScale * PAW_SHADOW_SCALE;
    actor->shape.feetPos[0] = paws[2];
    actor->shape.feetPos[1] = paws[3];
    if (actor->world.pos.y - actor->floorHeight <= HIND_SHADOW_MAX_HEIGHT) {
        ActorShadow_DrawFeet(actor, lights, play);
    }
    actor->shape.feetPos[0] = paws[0];
    actor->shape.feetPos[1] = paws[1];
    ActorShadow_DrawFeet(actor, lights, play);
    actor->shape.shadowScale = shadowScale;
}

static s32 FindSound(const char* name) {
    for (u32 i = 0; i < sSoundCount; i++) {
        if (strcmp(sSounds[i].name, name) == 0) {
            return (s32)i;
        }
    }
    return -1;
}

// A busy mixer drops the grunt: the game thread only fills an empty request, the audio thread empties it.
static void PlayVoice(const char* name) {
    s32 sound = FindSound(name);
    if (sound < 0) {
        return;
    }
    for (s32 i = 0; i < VOICE_REQUESTS; i++) {
        if (sVoiceRequests[i] < 0) {
            sVoiceRequests[i] = sound;
            return;
        }
    }
}

static void StartRequestedVoices(void) {
    for (s32 request = 0; request < VOICE_REQUESTS; request++) {
        s32 sound = sVoiceRequests[request];
        if (sound < 0) {
            continue;
        }
        sVoiceRequests[request] = -1;
        for (s32 slot = 0; slot < VOICE_SLOTS; slot++) {
            if (sVoiceSlots[slot].sound < 0) {
                sVoiceSlots[slot].sound = sound;
                sVoiceSlots[slot].position = 0.0f;
                break;
            }
        }
    }
}

// Audio thread.
static void MixWolfVoice(int16_t* samples, uint32_t frameCount) {
    StartRequestedVoices();
    for (s32 slot = 0; slot < VOICE_SLOTS; slot++) {
        VoiceSlot* voice = &sVoiceSlots[slot];
        if (voice->sound < 0) {
            continue;
        }
        const WolfSound* sound = &sSounds[voice->sound];
        for (u32 frame = 0; frame < frameCount; frame++) {
            u32 index = (u32)voice->position;
            if (index >= sound->sampleCount) {
                voice->sound = -1;
                break;
            }
            s32 value = sound->pcm[index];
            samples[frame * 2 + 0] = (s16)CLAMP(samples[frame * 2 + 0] + value, -32768, 32767);
            samples[frame * 2 + 1] = (s16)CLAMP(samples[frame * 2 + 1] + value, -32768, 32767);
            voice->position += sound->step;
        }
    }
}

// setSingleAnimeWolf(anim, speed, startFrame, endFrame): endFrame < 0 plays the whole clip.
static void SetAnim(WolfAnim anim, f32 tpRate, f32 startFrame, f32 endFrame, bool loop) {
    const WolfClip* clip = &sClips[sWolf.clipIndex[anim]];
    f32 last = clip->frameCount - 1.0f;

    sWolf.anim = anim;
    sWolf.clip = clip;
    sWolf.frame = MIN(startFrame, last);
    sWolf.prevFrame = sWolf.frame;
    sWolf.rate = TpAnimRate(tpRate);
    sWolf.end = (endFrame < 0.0f || endFrame > last) ? last : endFrame;
    sWolf.loop = loop;
}

static void SetAnimTp(WolfAnim anim, const TpAnm* params) {
    SetAnim(anim, params->speed, params->start, params->end, false);
}

static void SetLoopAnim(WolfAnim anim, f32 tpRate) {
    if (sWolf.anim == anim && sWolf.loop) {
        sWolf.rate = TpAnimRate(tpRate);
        return;
    }
    SetAnim(anim, tpRate, 0.0f, -1.0f, true);
}

static bool IsAnimDone(void) {
    return !sWolf.loop && sWolf.frame >= sWolf.end;
}

// frameCtrl->checkPass(f): true on the tick the counter crosses f.
static bool DidFramePass(f32 frame) {
    return sWolf.prevFrame < frame && sWolf.frame >= frame;
}

static void AdvanceAnim(void) {
    sWolf.prevFrame = sWolf.frame;
    f32 next = sWolf.frame + sWolf.rate * (R_UPDATE_RATE * (1.0f / 3.0f));
    if (sWolf.loop) {
        f32 length = sWolf.clip->frameCount;
        while (next >= length) {
            next -= length;
        }
    } else if (next > sWolf.end) {
        next = sWolf.end;
    }
    sWolf.frame = next;
}

static bool IsOnGround(Player* player) {
    return (player->actor.bgCheckFlags & BG_ON_GROUND) != 0;
}

static bool IsDashMode(void) {
    return sWolf.dashModeTimer > 0;
}

static WolfInput ReadInput(Player* player, PlayState* play) {
    WolfInput in = { 0 };
    Input* input = &play->state.input[0];
    u32 blockers = PLAYER_STATE1_LOADING | PLAYER_STATE1_TALKING | PLAYER_STATE1_DEAD | PLAYER_STATE1_GETTING_ITEM |
                   PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_HANGING_OFF_LEDGE |
                   PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_IN_ITEM_CS |
                   PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_IN_WATER | PLAYER_STATE1_ON_HORSE;

    func_80077D10(&in.stickMag, &in.stickWorldYaw, input);
    in.stickWorldYaw = (s16)(Camera_GetInputDirYaw(GET_ACTIVE_CAM(play)) + in.stickWorldYaw);
    in.blocked = (player->stateFlags1 & blockers) != 0;
    in.aPress = !in.blocked && CHECK_BTN_ALL(input->press.button, BTN_A);
    in.bPress = !in.blocked && CHECK_BTN_ALL(input->press.button, BTN_B);
    return in;
}

// getCutDirection: the stick against the wolf's facing.
static WolfDir GetCutDirection(const WolfInput* in, Player* player) {
    if (in->stickMag < STICK_DEADZONE) {
        return DIR_NONE;
    }
    s16 relative = (s16)(in->stickWorldYaw - player->actor.shape.rot.y);
    if (relative > -0x2000 && relative < 0x2000) {
        return DIR_FORWARD;
    }
    if (relative >= 0x6000 || relative <= -0x6000) {
        return DIR_BACKWARD;
    }
    return relative > 0 ? DIR_LEFT : DIR_RIGHT;
}

static void FaceYaw(Player* player, s16 yaw) {
    player->actor.shape.rot.y = yaw;
    player->actor.world.rot.y = yaw;
    player->yaw = yaw;
}

static void FaceLockOn(Player* player) {
    if (player->focusActor != NULL) {
        FaceYaw(player, Math_Vec3f_Yaw(&player->actor.world.pos, &player->focusActor->world.pos));
    }
}

// The proc drives speed, yaw and airtime from the form update; this action only keeps Link's own away.
static void WolfProcAction(Player* player, PlayState* play) {
}

static void TakeOver(PlayState* play, Player* player) {
    if (player->actionFunc != WolfProcAction) {
        Player_SetupAction(play, player, WolfProcAction, 0);
    }
    sWolf.ownsPlayer = true;
    FaceYaw(player, player->actor.shape.rot.y);
}

static void Release(PlayState* play, Player* player) {
    if (sWolf.ownsPlayer && player->actionFunc == WolfProcAction) {
        func_80839FFC(player, play);
    }
    sWolf.ownsPlayer = false;
    sWolf.attackActive = false;
}

static void SetAttackCollider(PlayState* play, Player* player, f32 tpRadius, f32 tpHeight, f32 tpOffset, bool strong) {
    if (!sAttackReady) {
        Collider_InitCylinder(play, &sAttackCylinder);
        Collider_SetCylinder(play, &sAttackCylinder, &player->actor, &sAttackInit);
        sAttackReady = true;
    }
    sAttackCylinder.dim.radius = (s16)MAX(MIN_ATTACK_RADIUS, TpLength(tpRadius));
    sAttackCylinder.dim.height = (s16)MAX(MIN_ATTACK_HEIGHT, TpLength(tpHeight));
    sAttackCylinder.dim.yShift = 0;
    sAttackCylinder.info.toucher.damage = strong ? DAMAGE_STRONG : DAMAGE_WEAK;
    sAttackCylinder.info.toucher.dmgFlags = strong ? DMG_SLASH_MASTER : DMG_SLASH_KOKIRI;
    sAttackCylinder.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
    sWolf.radiusOffset = tpOffset;
    sWolf.attackActive = true;
}

static void SubmitAttackCollider(PlayState* play, Player* player) {
    if (!sWolf.attackActive || !sAttackReady) {
        return;
    }
    f32 offset = TpLength(sWolf.radiusOffset);
    sAttackCylinder.dim.pos.x = (s16)(player->actor.world.pos.x + Math_SinS(player->actor.shape.rot.y) * offset);
    sAttackCylinder.dim.pos.y = (s16)player->actor.world.pos.y;
    sAttackCylinder.dim.pos.z = (s16)(player->actor.world.pos.z + Math_CosS(player->actor.shape.rot.y) * offset);
    sAttackCylinder.base.atFlags = AT_ON | AT_TYPE_PLAYER;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sAttackCylinder.base);
}

static bool DidAttackHit(void) {
    return sAttackReady && (sAttackCylinder.base.atFlags & AT_HIT);
}

static bool DidAttackBounce(void) {
    return sAttackReady && (sAttackCylinder.base.atFlags & AT_BOUNCED);
}

static void ResetCombo(void) {
    sWolf.comboCount = 0;
    sWolf.comboWindow = 0;
    sWolf.comboReserved = false;
}

// setComboReserb: a B press while an attack is busy waits for its cancel window.
static void ReserveCombo(const WolfInput* in) {
    if (in->bPress) {
        sWolf.comboReserved = true;
    }
}

static void StartMove(PlayState* play, Player* player);
static void StartLunge(PlayState* play, Player* player, WolfLunge lunge);
static void StartAttackReverse(PlayState* play, Player* player);
static bool TryAttack(PlayState* play, Player* player, const WolfInput* in);
static bool TryNextAction(PlayState* play, Player* player, const WolfInput* in, bool requireInput);

static void StartMove(PlayState* play, Player* player) {
    Release(play, player);
    sWolf.proc = PROC_WOLF_MOVE;
}

static void StartLand(PlayState* play, Player* player) {
    Release(play, player);
    sWolf.proc = PROC_WOLF_LAND;
    SetAnimTp(WANM_JUMP_ATTACK_END, &kAutoLandAnm);
    sWolf.cancelFrame = kAutoLandAnm.cancel;
}

static void StartDash(PlayState* play, Player* player) {
    TakeOver(play, player);
    sWolf.proc = PROC_WOLF_DASH;
    SetAnimTp(WANM_DASH_START, &kADashAnm);
    sWolf.dashModeTimer = TpFrames(A_DASH_DURATION);
    sWolf.dashAttackQueued = false;
    player->linearVelocity = MAX(player->linearVelocity, TpSpeed(A_DASH_INIT_SPEED));
    PlayVoice("WL_V_DASH__1");
}

static void StartDashReverse(PlayState* play, Player* player) {
    TakeOver(play, player);
    sWolf.proc = PROC_WOLF_DASH_REVERSE;
    SetAnim(WANM_ATTACK_RECOIL_GROUND, kDashReboundAnm.speed, kDashReboundAnm.start, DASH_REBOUND_HOLD_FRAME, false);
    player->linearVelocity = -TpSpeed(DASH_REBOUND_H);
    player->actor.velocity.y = TpVSpeed(DASH_REBOUND_V);
    sWolf.dashModeTimer = 0;
    sWolf.airborne = true;
    PlayVoice("WOLF_BODYATTACK");
    PlayVoice("WL_V_DAMAGE");
}

static void StartWaitAttack(PlayState* play, Player* player, WolfWaitAttack attack) {
    static const WolfAnim anims[] = { WANM_ATTACK_B_LEFT, WANM_ATTACK_B_FRONT, WANM_ATTACK_B_TAIL,
                                      WANM_ATTACK_B_RIGHT };
    const TpWaitAttack* hio = attack == ATTACK_TAIL ? &kAtWaTl : attack == ATTACK_SCRATCH ? &kAtWaSc : &kAtWaLr;
    bool finisher = sWolf.comboCount == COMBO_FINISHER;

    TakeOver(play, player);
    sWolf.proc = PROC_WOLF_WAIT_ATTACK;
    sWolf.waitAttack = attack;
    SetAttackCollider(play, player, hio->radius, hio->height, hio->radiusOffset, finisher);
    sWolf.cancelFrame = finisher ? hio->anm.cancel : hio->comboMidCancel;
    sWolf.stopTimer = TpFrames(finisher ? hio->stopTime : hio->comboMidStopTime);
    SetAnim(anims[attack], hio->anm.speed, finisher ? hio->anm.start : hio->comboMidStart, hio->anm.end, false);
    FaceLockOn(player);
    sWolf.judgeStart = hio->judgeStart;
    sWolf.judgeEnd = hio->judgeEnd;
    sWolf.speedAddFrame = hio->speedAddForward;
    sWolf.attackSpeed = hio->speed;
    sWolf.comboWindow = TpFrames(COMBO_DURATION);
    sWolf.attackActive = false;
    PlayVoice(finisher ? "WL_V_ATTACK_L" : "WL_V_ATTACK_S");
    PlayVoice(attack == ATTACK_TAIL ? "WOLFATTACK_WIND_TAIL__1" : "WOLFATTACK_WIND_S");
}

// Aims the arc at the lock-on target: h = d / t with the flight time the height difference gives.
static void AimLunge(Player* player, WolfLunge lunge, f32* h, f32* v) {
    Actor* target = player->focusActor;
    if (target == NULL) {
        return;
    }
    FaceLockOn(player);
    f32 dy = (target->world.pos.y - player->actor.world.pos.y) - LUNGE_TARGET_EYE * RenderScale();
    f32 t = dy > 0.0f ? sqrtf((2.0f * dy) / -OOT_GRAVITY) : 0.0f;
    f32 distance = Math_Vec3f_DistXZ(&player->actor.world.pos, &target->world.pos);
    if (t >= 1.0f) {
        *h = distance / t;
        *v = CLAMP(dy / t - 0.5f * OOT_GRAVITY * t, TpVSpeed(NJUMP_MIN_V), TpVSpeed(NJUMP_MAX_V));
    } else if (sWolf.comboCount == 1 && lunge != LUNGE_FOLLOW_UP) {
        *v = TpVSpeed(NJUMP_MIN_V);
        *h = (-OOT_GRAVITY * distance) / (2.0f * *v);
    }
}

static void StartLunge(PlayState* play, Player* player, WolfLunge lunge) {
    bool finisher = sWolf.comboCount == COMBO_FINISHER;
    f32 h = TpSpeed(NJUMP_INIT_SPEED);
    f32 v = TpVSpeed(NJUMP_MIN_V);

    TakeOver(play, player);
    sWolf.proc = PROC_WOLF_JUMP_ATTACK;
    sWolf.lunge = lunge;
    sWolf.lungeHit = false;
    sWolf.aerialLoop = false;
    sWolf.airborne = false;
    SetAttackCollider(play, player, NJUMP_RADIUS, NJUMP_HEIGHT, NJUMP_RADIUS_OFFSET, finisher);
    SetAnimTp(WANM_ATTACK_A_START, &kNjumpAerialAnm);
    AimLunge(player, lunge, &h, &v);
    player->linearVelocity = CLAMP(h, TpSpeed(NJUMP_MIN_H), TpSpeed(NJUMP_MAX_H));
    player->actor.velocity.y = v;
    player->actor.bgCheckFlags &= ~BG_ON_GROUND;
    sWolf.comboWindow = TpFrames(COMBO_DURATION);
    PlayVoice(finisher ? "WL_V_ATTACK_L" : "WL_V_ATTACK_S");
    PlayVoice(finisher ? "WOLFATTACK_WIND_SCREW__1" : "WOLFATTACK_WIND_S");
}

static void StartLungeLand(PlayState* play, Player* player, bool slide, bool back) {
    TakeOver(play, player);
    sWolf.proc = PROC_WOLF_JUMP_AT_LAND;
    sWolf.attackActive = false;
    sWolf.slide = slide;
    if (slide) {
        const TpAnm* anm = back ? &kLandBackSlideAnm : &kLandFrontSlideAnm;
        SetAnimTp(back ? WANM_ATTACK_A_END_BACK : WANM_ATTACK_A_END_FRONT, anm);
        sWolf.cancelFrame = anm->cancel;
        player->linearVelocity *= 0.5f;
    } else {
        SetAnimTp(WANM_ATTACK_A_END, &kLandNormalAnm);
        sWolf.cancelFrame = kLandNormalAnm.cancel;
        player->linearVelocity = 0.0f;
    }
    sWolf.comboWindow = TpFrames(COMBO_DURATION);
    Player_PlaySfx(&player->actor, NA_SE_PL_LAND);
}

// setCylAtParam(..., radius * 0.5, 155): the radius grows to the full value during the active frames.
static void StartSpin(PlayState* play, Player* player, bool right) {
    TakeOver(play, player);
    sWolf.proc = PROC_WOLF_ROLL_ATTACK;
    SetAnimTp(right ? WANM_CUT_TURN_RIGHT : WANM_CUT_TURN_LEFT, &kRollAnm);
    SetAttackCollider(play, player, ROLL_RADIUS * 0.5f, ROLL_HEIGHT, 0.0f, true);
    sWolf.attackActive = false;
    player->linearVelocity = 0.0f;
    PlayVoice("WL_V_ATTACK_SPIN");
    PlayVoice("WOLFATTACK_WIND_L__1");
}

static void StartAttackReverse(PlayState* play, Player* player) {
    TakeOver(play, player);
    sWolf.proc = PROC_WOLF_ATTACK_REVERSE;
    sWolf.attackActive = false;
    SetAnim(WANM_ATTACK_RECOIL_START, 1.0f, 0.0f, -1.0f, false);
    player->linearVelocity = -TpSpeed(JUMP_BACK_SPEED_H);
    player->actor.velocity.y = TpVSpeed(JUMP_BACK_SPEED_V);
    player->actor.bgCheckFlags &= ~BG_ON_GROUND;
    sWolf.airborne = true;
    ResetCombo();
    Player_PlaySfx(&player->actor, NA_SE_IT_SHIELD_BOUND);
    PlayVoice("WL_V_DAMAGE");
}

// TP launches along current.angle.y while shape_angle keeps facing the target: the hop never turns away.
static void StartSideStep(PlayState* play, Player* player, WolfHop hop) {
    s16 facing = player->actor.shape.rot.y;
    s16 launch;

    TakeOver(play, player);
    sWolf.proc = PROC_WOLF_SIDESTEP;
    sWolf.hop = hop;
    sWolf.airborne = false;
    if (hop == HOP_BACK) {
        SetAnimTp(WANM_BACK_JUMP_START, &kBackJumpAnm);
        player->linearVelocity = TpSpeed(BACK_JUMP_H);
        player->actor.velocity.y = TpVSpeed(BACK_JUMP_V);
        launch = (s16)(facing + 0x8000);
    } else {
        SetAnimTp(hop == HOP_LEFT ? WANM_SIDE_JUMP_L_START : WANM_SIDE_JUMP_R_START, &kSideJumpAnm);
        player->linearVelocity = TpSpeed(SIDE_JUMP_H);
        player->actor.velocity.y = TpVSpeed(SIDE_JUMP_V);
        launch = (s16)(facing + (hop == HOP_LEFT ? 0x4000 : -0x4000));
    }
    player->actor.world.rot.y = launch;
    player->yaw = launch;
    player->actor.bgCheckFlags &= ~BG_ON_GROUND;
    PlayVoice("WL_V_BREATH_JUMP");
}

static void StartSideStepLand(PlayState* play, Player* player) {
    const TpAnm* anm = sWolf.hop == HOP_BACK ? &kBackLandAnm : &kSideLandAnm;
    WolfAnim anim = sWolf.hop == HOP_BACK   ? WANM_BACK_JUMP_END
                    : sWolf.hop == HOP_LEFT ? WANM_SIDE_JUMP_L_END
                                            : WANM_SIDE_JUMP_R_END;

    TakeOver(play, player);
    sWolf.proc = PROC_WOLF_SIDESTEP_LAND;
    SetAnimTp(anim, anm);
    sWolf.cancelFrame = anm->cancel;
    player->linearVelocity = 0.0f;
    Player_PlaySfx(&player->actor, NA_SE_PL_LAND);
}

// Link's own damage action keeps running; the wolf only shows where the hit came from.
static void StartDamage(PlayState* play, Player* player) {
    Release(play, player);
    sWolf.proc = PROC_WOLF_DAMAGE;
    ResetCombo();
    sWolf.dashModeTimer = 0;
    s16 hitSide = (s16)(player->actor.world.rot.y - player->actor.shape.rot.y);
    SetAnim(ABS(hitSide) < 0x4000 ? WANM_DMG_BACK : WANM_DMG_FRONT, 1.0f, 0.0f, -1.0f, false);
    PlayVoice("WL_V_DAMAGE");
}

static void UpdateAirborneMove(PlayState* play, Player* player, const WolfInput* in) {
    if (sWolf.wasOnGround) {
        sWolf.fallStartY = player->actor.world.pos.y;
        sWolf.pastApex = false;
        SetAnimTp(WANM_JUMP_ATTACK_START, &kAutoJumpAnm);
    }
    f32 fallen = (sWolf.fallStartY - player->actor.world.pos.y) / RenderScale();
    if (fallen > AIR_ANM_TRANSITION_HEIGHT) {
        if (sWolf.anim != WANM_FALL_LAND) {
            SetAnim(WANM_FALL_LAND, 1.0f, 0.0f, -1.0f, false);
        }
    } else if (sWolf.anim == WANM_JUMP_ATTACK_START && IsAnimDone()) {
        SetAnimTp(WANM_JUMP_ATTACK, &kAutoClimbAnm);
    } else if (sWolf.anim == WANM_JUMP_ATTACK && !sWolf.pastApex && player->actor.velocity.y < 0.0f) {
        sWolf.pastApex = true;
        sWolf.rate = TpAnimRate(kAutoClimbAnm.speed);
    }
    if (sWolf.anim == WANM_JUMP_ATTACK && IsAnimDone()) {
        sWolf.frame = kAutoClimbAnm.start;
    }
    if (in->bPress) {
        StartLunge(play, player, LUNGE_NORMAL);
    }
}

// setBlendWolfMoveAnime: the gait follows the speed Link's own locomotion reached.
static void UpdateGait(Player* player) {
    f32 multiplier = IsDashMode() ? A_DASH_MAX_SPEED / TP_HUMAN_RUN : NOP_MAX_SPEED / TP_HUMAN_RUN;
    f32 maxSpeed = OOT_RUN_SPEED * multiplier * SpeedScale();
    f32 rate = MIN(fabsf(player->linearVelocity) / MAX(0.01f, maxSpeed), 1.0f);

    if (IsDashMode()) {
        if (rate < DASH_WALK_TO_BRISK) {
            sWolf.dashModeTimer = 0;
        }
        if (rate < DASH_IDLE_TO_WALK) {
            SetLoopAnim(WANM_WAIT, DASH_IDLE_ANM_SPEED);
        } else if (rate < DASH_WALK_TO_BRISK) {
            f32 t = (rate - DASH_IDLE_TO_WALK) / (DASH_WALK_TO_BRISK - DASH_IDLE_TO_WALK);
            SetLoopAnim(t < 0.5f ? WANM_WALK_A : WANM_WALK_B,
                        DASH_WALK_ANM_SPEED + (DASH_BRISK_ANM_SPEED - DASH_WALK_ANM_SPEED) * t);
        } else if (rate < DASH_RUN_TO_QUICK) {
            SetLoopAnim(WANM_DASH_A, DASH_RUN_ANM_SPEED);
        } else {
            SetLoopAnim(WANM_DASH_B, DASH_QUICK_RUN_ANM_SPEED);
        }
        return;
    }
    if (rate < NOP_IDLE_TO_WALK) {
        SetLoopAnim(WANM_WAIT, NOP_IDLE_ANM_SPEED);
    } else if (rate < NOP_WALK_TO_JOG) {
        f32 t = (rate - NOP_IDLE_TO_WALK) / (NOP_WALK_TO_JOG - NOP_IDLE_TO_WALK);
        SetLoopAnim(t < 0.5f ? WANM_WALK_A : WANM_WALK_B,
                    NOP_WALK_ANM_SPEED + (NOP_JOG_ANM_SPEED - NOP_WALK_ANM_SPEED) * t);
    } else {
        SetLoopAnim(WANM_DASH_A, NOP_RUN_ANM_SPEED);
    }
}

// checkNextActionFromButton / checkMoveDoAction: B attacks; A hops under lock-on and dashes while moving.
static bool TryButtonAction(PlayState* play, Player* player, const WolfInput* in) {
    if (in->blocked) {
        return false;
    }
    if (in->bPress || sWolf.comboReserved) {
        return TryAttack(play, player, in);
    }
    if (!in->aPress || !IsOnGround(player)) {
        return false;
    }
    if (player->focusActor != NULL) {
        WolfDir dir = GetCutDirection(in, player);
        StartSideStep(play, player, dir == DIR_LEFT ? HOP_LEFT : dir == DIR_RIGHT ? HOP_RIGHT : HOP_BACK);
        return true;
    }
    if (sWolf.dashCooldown <= 0 && in->stickMag > STICK_DEADZONE && fabsf(player->linearVelocity) > MOVING_SPEED) {
        StartDash(play, player);
        return true;
    }
    return false;
}

static void UpdateMove(PlayState* play, Player* player, const WolfInput* in) {
    if (!IsOnGround(player)) {
        UpdateAirborneMove(play, player, in);
        return;
    }
    if (!sWolf.wasOnGround) {
        StartLand(play, player);
        return;
    }
    if (!TryButtonAction(play, player, in)) {
        UpdateGait(player);
    }
}

static void UpdateLand(PlayState* play, Player* player, const WolfInput* in) {
    if (!IsOnGround(player)) {
        StartMove(play, player);
    } else if (IsAnimDone()) {
        StartMove(play, player);
        TryNextAction(play, player, in, false);
    } else if (sWolf.frame > sWolf.cancelFrame &&
               (fabsf(player->linearVelocity) > MOVING_SPEED || in->aPress || in->bPress)) {
        StartMove(play, player);
        TryNextAction(play, player, in, true);
    }
}

static void UpdateDash(PlayState* play, Player* player, const WolfInput* in) {
    Math_StepToF(&player->linearVelocity, TpSpeed(A_DASH_MAX_SPEED), TpSpeed(A_DASH_ACCELERATION));
    if (in->stickMag > STICK_STEER) {
        s16 yaw = player->actor.shape.rot.y;
        Math_SmoothStepToS(&yaw, in->stickWorldYaw, DASH_TURN_RATE, (s16)(DASH_TURN_MAX * 1.5f),
                           (s16)(DASH_TURN_MIN * 1.5f));
        FaceYaw(player, yaw);
    }
    if (sWolf.frame > DASH_WALL_FRAME && (player->actor.bgCheckFlags & BG_TOUCHING_WALL)) {
        StartDashReverse(play, player);
        return;
    }
    if (in->bPress) {
        sWolf.dashAttackQueued = true;
    }
    if (!IsAnimDone() && sWolf.frame <= kADashAnm.cancel) {
        return;
    }
    sWolf.dashCooldown = TpFrames(A_DASH_COOLDOWN);
    if (sWolf.dashAttackQueued) {
        TryAttack(play, player, in);
    } else {
        StartMove(play, player);
    }
}

static void UpdateDashReverse(PlayState* play, Player* player, const WolfInput* in) {
    if (sWolf.airborne) {
        if (IsOnGround(player) && player->actor.velocity.y <= 0.0f) {
            sWolf.airborne = false;
            player->linearVelocity = 0.0f;
            sWolf.end = kDashReboundAnm.end;
        }
        return;
    }
    Math_StepToF(&player->linearVelocity, 0.0f, TpSpeed(NOP_DECELERATION));
    if (IsAnimDone()) {
        StartMove(play, player);
    } else if (sWolf.frame > kDashReboundAnm.cancel && (in->stickMag > STICK_DEADZONE || in->aPress || in->bPress)) {
        StartMove(play, player);
        TryNextAction(play, player, in, true);
    }
}

static void UpdateWaitAttack(PlayState* play, Player* player, const WolfInput* in) {
    Math_StepToF(&player->linearVelocity, 0.0f, TpSpeed(NOP_DECELERATION));
    ReserveCombo(in);
    if (sWolf.waitAttack != ATTACK_TAIL && DidAttackBounce()) {
        StartAttackReverse(play, player);
        return;
    }
    if (IsAnimDone()) {
        ResetCombo();
        if (sWolf.stopTimer <= 0) {
            player->linearVelocity = 0.0f;
            StartMove(play, player);
            TryNextAction(play, player, in, false);
        } else if (!(sWolf.frame > sWolf.cancelFrame && TryNextAction(play, player, in, true))) {
            sWolf.stopTimer--;
        }
        return;
    }
    if (sWolf.frame > sWolf.cancelFrame) {
        if (!TryNextAction(play, player, in, true)) {
            ResetCombo();
        }
        return;
    }
    FaceLockOn(player);
    if (DidFramePass(sWolf.speedAddFrame)) {
        player->linearVelocity = TpSpeed(sWolf.attackSpeed);
    }
    sWolf.attackActive = sWolf.frame >= sWolf.judgeStart && sWolf.frame < sWolf.judgeEnd;
}

static void UpdateLunge(PlayState* play, Player* player, const WolfInput* in) {
    if (DidAttackHit()) {
        sWolf.lungeHit = true;
    }
    if (DidAttackBounce() && sWolf.airborne) {
        StartAttackReverse(play, player);
        return;
    }
    if (IsOnGround(player) && sWolf.airborne) {
        bool strong = sWolf.lunge == LUNGE_STRONG || sWolf.lunge == LUNGE_STRONG_FOLLOW_UP;
        bool slide = sWolf.comboCount == COMBO_FINISHER || strong;
        StartLungeLand(play, player, slide, sWolf.lunge == LUNGE_STRONG && sWolf.lungeHit);
        return;
    }
    sWolf.airborne = true;
    sWolf.comboWindow = TpFrames(COMBO_DURATION);
    if (IsAnimDone() && !sWolf.aerialLoop) {
        sWolf.aerialLoop = true;
        SetAnim(WANM_ATTACK_A, NJUMP_AERIAL_ANM_SPEED, 0.0f, -1.0f, false);
    }
    sWolf.attackActive = true;
}

static void UpdateLungeLand(PlayState* play, Player* player, const WolfInput* in) {
    ReserveCombo(in);
    if (!sWolf.slide) {
        Math_StepToF(&player->linearVelocity, 0.0f, TpSpeed(NOP_DECELERATION));
        if (IsAnimDone()) {
            StartMove(play, player);
            TryNextAction(play, player, in, false);
        } else if (sWolf.frame > sWolf.cancelFrame) {
            TryNextAction(play, player, in, true);
        }
        return;
    }
    Math_StepToF(&player->linearVelocity, 0.0f, TpSpeed(LAND_SLIDE_DECEL));
    if (IsAnimDone()) {
        if (fabsf(player->linearVelocity) < LAND_SLIDE_STOP_SPEED) {
            StartMove(play, player);
            TryNextAction(play, player, in, false);
        }
    } else if (sWolf.frame > sWolf.cancelFrame && player->linearVelocity <= TpSpeed(LAND_SLIDE_CANCEL_SPEED)) {
        TryNextAction(play, player, in, true);
    }
}

static void UpdateSpin(PlayState* play, Player* player, const WolfInput* in) {
    Math_StepToF(&player->linearVelocity, 0.0f, TpSpeed(NOP_DECELERATION));
    if (IsAnimDone()) {
        StartMove(play, player);
        TryNextAction(play, player, in, false);
        return;
    }
    if (sWolf.frame > kRollAnm.cancel) {
        TryNextAction(play, player, in, true);
        return;
    }
    sWolf.attackActive = sWolf.frame >= ROLL_ACTIVE_START && sWolf.frame < ROLL_ACTIVE_END;
    if (!sWolf.attackActive) {
        return;
    }
    if (!DidAttackHit()) {
        player->linearVelocity = TpSpeed(ROLL_SPEED);
    }
    f32 radius = sAttackCylinder.dim.radius;
    Math_StepToF(&radius, TpLength(ROLL_RADIUS), TpLength(ROLL_RADIUS_STEP) * 1.5f);
    sAttackCylinder.dim.radius = (s16)radius;
}

static void UpdateAttackReverse(PlayState* play, Player* player, const WolfInput* in) {
    if (sWolf.airborne) {
        if (IsOnGround(player) && player->actor.velocity.y <= 0.0f) {
            sWolf.airborne = false;
            player->linearVelocity = 0.0f;
            SetAnimTp(WANM_ATTACK_RECOIL_END, &kJumpBackLandAnm);
        }
        return;
    }
    if (IsAnimDone()) {
        StartMove(play, player);
        TryNextAction(play, player, in, false);
    } else if (sWolf.frame > kJumpBackLandAnm.cancel && (in->stickMag > STICK_DEADZONE || in->aPress || in->bPress)) {
        StartMove(play, player);
        TryNextAction(play, player, in, true);
    }
}

static void UpdateSideStep(PlayState* play, Player* player, const WolfInput* in) {
    FaceLockOn(player);
    if (IsOnGround(player) && sWolf.airborne) {
        StartSideStepLand(play, player);
        return;
    }
    sWolf.airborne = true;
}

static void UpdateSideStepLand(PlayState* play, Player* player, const WolfInput* in) {
    FaceLockOn(player);
    if (IsAnimDone()) {
        StartMove(play, player);
        TryNextAction(play, player, in, false);
    } else if (sWolf.frame > sWolf.cancelFrame) {
        TryNextAction(play, player, in, true);
    }
}

static void UpdateDamage(PlayState* play, Player* player, const WolfInput* in) {
    if (IsAnimDone() || player->invincibilityTimer <= 0) {
        StartMove(play, player);
    }
}

// checkWolfAttackAction: the combo count and the stick against the facing pick the move.
static bool TryAttack(PlayState* play, Player* player, const WolfInput* in) {
    static const WolfWaitAttack secondHit[] = { ATTACK_BITE_RIGHT, ATTACK_BITE_RIGHT, ATTACK_BITE_RIGHT,
                                                ATTACK_BITE_LEFT, ATTACK_BITE_LEFT };
    static const WolfWaitAttack otherHit[] = { ATTACK_BITE_LEFT, ATTACK_BITE_LEFT, ATTACK_BITE_LEFT, ATTACK_BITE_RIGHT,
                                               ATTACK_BITE_RIGHT };

    if (sWolf.comboCount == COMBO_FINISHER) {
        ResetCombo();
    }
    sWolf.comboCount++;
    sWolf.comboReserved = false;
    WolfDir dir = GetCutDirection(in, player);
    bool hasTarget = player->focusActor != NULL;

    if (IsDashMode()) {
        sWolf.comboCount = COMBO_FINISHER;
        StartLunge(play, player, LUNGE_NORMAL);
    } else if (sWolf.comboCount == COMBO_FINISHER && !hasTarget) {
        if (dir == DIR_LEFT || dir == DIR_NONE) {
            StartWaitAttack(play, player, ATTACK_TAIL);
        } else {
            StartLunge(play, player, LUNGE_NORMAL);
        }
    } else if (sWolf.comboCount == COMBO_FINISHER) {
        if (dir == DIR_LEFT || dir == DIR_RIGHT) {
            StartSpin(play, player, dir == DIR_RIGHT);
        } else {
            StartLunge(play, player, dir == DIR_FORWARD ? LUNGE_NORMAL : LUNGE_STRONG);
        }
    } else if (sWolf.comboCount == 2) {
        StartWaitAttack(play, player, secondHit[dir]);
    } else if (sWolf.comboCount == 1 && dir == DIR_FORWARD) {
        StartLunge(play, player, LUNGE_NORMAL);
    } else {
        StartWaitAttack(play, player, otherHit[dir]);
    }
    return true;
}

// checkNextActionWolf: without requireInput the proc is over and falls back to Move; with it, only input leaves.
static bool TryNextAction(PlayState* play, Player* player, const WolfInput* in, bool requireInput) {
    if (in->blocked) {
        if (!requireInput) {
            StartMove(play, player);
        }
        return false;
    }
    if (TryButtonAction(play, player, in)) {
        return true;
    }
    if (!requireInput || (in->stickMag > STICK_DEADZONE && IsOnGround(player))) {
        StartMove(play, player);
        return true;
    }
    return false;
}

static void UpdateProc(PlayState* play, Player* player, const WolfInput* in) {
    switch (sWolf.proc) {
        case PROC_WOLF_MOVE:
            UpdateMove(play, player, in);
            break;
        case PROC_WOLF_LAND:
            UpdateLand(play, player, in);
            break;
        case PROC_WOLF_DASH:
            UpdateDash(play, player, in);
            break;
        case PROC_WOLF_DASH_REVERSE:
            UpdateDashReverse(play, player, in);
            break;
        case PROC_WOLF_WAIT_ATTACK:
            UpdateWaitAttack(play, player, in);
            break;
        case PROC_WOLF_JUMP_ATTACK:
            UpdateLunge(play, player, in);
            break;
        case PROC_WOLF_JUMP_AT_LAND:
            UpdateLungeLand(play, player, in);
            break;
        case PROC_WOLF_ROLL_ATTACK:
            UpdateSpin(play, player, in);
            break;
        case PROC_WOLF_ATTACK_REVERSE:
            UpdateAttackReverse(play, player, in);
            break;
        case PROC_WOLF_DAMAGE:
            UpdateDamage(play, player, in);
            break;
        case PROC_WOLF_SIDESTEP:
            UpdateSideStep(play, player, in);
            break;
        case PROC_WOLF_SIDESTEP_LAND:
            UpdateSideStepLand(play, player, in);
            break;
        default:
            StartMove(play, player);
            break;
    }
}

static void UpdateTimers(void) {
    if (sWolf.dashModeTimer > 0) {
        sWolf.dashModeTimer--;
    }
    if (sWolf.dashCooldown > 0) {
        sWolf.dashCooldown--;
    }
    if (sWolf.proc != PROC_WOLF_MOVE && sWolf.proc != PROC_WOLF_LAND) {
        return;
    }
    if (sWolf.comboWindow > 0) {
        sWolf.comboWindow--;
    } else if (sWolf.comboCount != 0 || sWolf.comboReserved) {
        ResetCombo();
    }
}

// Link's draw is skipped, so nothing else fills the parts the camera, the cylinder and lock-on read.
static void PlaceBodyParts(Player* player) {
    f32 scale = RenderScale();
    Vec3f center = player->actor.world.pos;
    center.y += BODY_HEIGHT * scale * BODY_CENTER;
    for (s32 i = 0; i < PLAYER_BODYPART_MAX; i++) {
        player->bodyPartsPos[i] = center;
    }
    player->bodyPartsPos[PLAYER_BODYPART_L_FOOT].y = player->actor.world.pos.y;
    player->bodyPartsPos[PLAYER_BODYPART_R_FOOT].y = player->actor.world.pos.y;
    player->bodyPartsPos[PLAYER_BODYPART_HEAD].y = player->actor.world.pos.y + HEAD_HEIGHT * scale;
    player->actor.focus.pos = player->bodyPartsPos[PLAYER_BODYPART_HEAD];
    if (!GetPawWorldPos(player, 0, &player->actor.shape.feetPos[0]) ||
        !GetPawWorldPos(player, 1, &player->actor.shape.feetPos[1])) {
        player->actor.shape.feetPos[0] = player->actor.world.pos;
        player->actor.shape.feetPos[1] = player->actor.world.pos;
    }
    player->actor.shape.shadowDraw = DrawWolfShadow;
}

static void UpdateWolf(PlayState* play, Player* player) {
    if (!sAssetsLoaded) {
        return;
    }
    PlaceBodyParts(player);
    WolfInput in = ReadInput(player, play);
    UpdateTimers();

    // Damage or a cutscene that replaced the proc's action already owns Link: nothing is handed back then.
    if (sWolf.ownsPlayer && player->actionFunc != WolfProcAction) {
        sWolf.ownsPlayer = false;
        StartMove(play, player);
    } else if (sWolf.ownsPlayer && in.blocked) {
        StartMove(play, player);
    }
    bool isInvincible = player->invincibilityTimer > 0;
    if (isInvincible && !sWolf.wasInvincible && sWolf.proc != PROC_WOLF_DAMAGE) {
        StartDamage(play, player);
    }
    sWolf.wasInvincible = isInvincible;

    UpdateProc(play, player, &in);

    if (sWolf.ownsPlayer) {
        if (sWolf.proc != PROC_WOLF_SIDESTEP) {
            player->actor.world.rot.y = player->actor.shape.rot.y;
            player->yaw = player->actor.shape.rot.y;
        }
        // Without MIDAIR func_8083AA10 reads an airborne proc as walking off a ledge and installs a fall.
        if (!IsOnGround(player)) {
            player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
        }
    }
    SubmitAttackCollider(play, player);
    sWolf.wasOnGround = IsOnGround(player);
    AdvanceAnim();
}

static void ResetWolfState(void) {
    s32 clipIndex[WANM_COUNT];
    memcpy(clipIndex, sWolf.clipIndex, sizeof(clipIndex));
    memset(&sWolf, 0, sizeof(sWolf));
    memcpy(sWolf.clipIndex, clipIndex, sizeof(clipIndex));
    sWolf.wasOnGround = true;
    sHasPose = false;
    if (sAssetsLoaded) {
        SetLoopAnim(WANM_WAIT, NOP_IDLE_ANM_SPEED);
    }
}

static void EnterWolf(PlayState* play, Player* player) {
    LoadAssets();
    ResetWolfState();
    sIsWolf = true;
    if (sShift == SHIFT_TO_WOLF) {
        sShiftPastPeak = true;
        sShiftFrames = 0;
        sEmerge = 0.0f;
        sBlackout = 255;
    } else {
        sEmerge = 1.0f;
        sBlackout = 0;
    }
}

static void ExitWolf(PlayState* play, Player* player) {
    if (sWolf.ownsPlayer && player != NULL) {
        Release(play, player);
    }
    sIsWolf = false;
    sWolf.attackActive = false;
    sEmerge = 1.0f;
    if (player != NULL) {
        player->actor.shape.shadowDraw = ActorShadow_DrawFeet;
    }
    if (sShift == SHIFT_TO_LINK) {
        sShiftPastPeak = true;
        sShiftFrames = 0;
        sBlackout = 255;
    } else {
        sBlackout = 0;
    }
}

static void ClearShift(void) {
    sShift = SHIFT_NONE;
    sShiftPastPeak = false;
    sShiftFrames = 0;
    sBlackout = 0;
    sEmerge = 1.0f;
}

// TP's change: the body darkens into a silhouette up to the flash peak, and the new one grows out of black.
static void UpdateShift(void) {
    if (sShift == SHIFT_NONE) {
        return;
    }
    sShiftFrames++;
    if (!sShiftPastPeak) {
        s32 peak = sShift == SHIFT_TO_WOLF ? SHIFT_IN_PEAK_FRAMES : SHIFT_OUT_PEAK_FRAMES;
        if (sShiftFrames > peak + SHIFT_STUCK_FRAMES) {
            ClearShift();
            return;
        }
        sBlackout = (u8)MIN(255, sShiftFrames * 255 / peak);
        return;
    }
    s32 step = sShift == SHIFT_TO_WOLF ? SHIFT_IN_FADE_STEP : SHIFT_OUT_FADE_STEP;
    s32 blackout = 255 - sShiftFrames * step;
    if (blackout <= 0) {
        ClearShift();
        return;
    }
    sBlackout = (u8)blackout;
    if (sShift == SHIFT_TO_WOLF) {
        sEmerge = 1.0f - blackout / 255.0f;
    }
}

static void RunEveryFrame(void) {
    UpdateShift();
}

static void DarkenBody(Actor* actor, PlayState* play, Color_RGBA8* grayscale) {
    if (sBlackout == 0) {
        return;
    }
    grayscale->r = 0;
    grayscale->g = 0;
    grayscale->b = 0;
    grayscale->a = sBlackout;
}

// First-person aiming (unk_6AD 1-2) hides the body; 3 is only the cutscene marker and keeps it.
static void DrawWolfBody(Actor* actor, PlayState* play, bool* drawVanilla) {
    if (!sIsWolf || !sAssetsLoaded) {
        return;
    }
    Player* player = (Player*)actor;
    *drawVanilla = false;
    if (player->unk_6AD == 0 || player->unk_6AD == 3) {
        DrawWolfMesh(play, player);
    }
}

static void MuteLinkVoice(Actor* actor, int32_t kind, uint16_t* sfxId, bool* handled) {
    if (sIsWolf && kind == SOH_ACTOR_SFX_VOICE) {
        *handled = true;
    }
}

// TP wolf/human run ratio, or the A-dash's while dash mode lasts.
static void ScaleWolfSpeed(Player* player, int32_t kind, float* scale) {
    if (!sIsWolf) {
        return;
    }
    f32 ratio = IsDashMode() ? A_DASH_MAX_SPEED / TP_HUMAN_RUN : NOP_MAX_SPEED / TP_HUMAN_RUN;
    *scale *= ratio * SpeedScale();
}

// A, B, R and the lock-on A belong to the wolf's procs; the rest of Link's actions stay his.
static int32_t KeepForWolf(PlayState* play, Player* player) {
    return SOH_FORM_ACTION_BLOCKED;
}

static const SOHFormActionOverride sActions[] = {
    { SOH_PLAYER_ACTION_ROLL, KeepForWolf },        { SOH_PLAYER_ACTION_MELEE, KeepForWolf },
    { SOH_PLAYER_ACTION_SPIN_CHARGE, KeepForWolf }, { SOH_PLAYER_ACTION_ZTARGET_A, KeepForWolf },
    { SOH_PLAYER_ACTION_SHIELD, KeepForWolf },
};

static void ForgetScene(int16_t sceneNum) {
    sAttackReady = false;
    ClearShift();
    ResetWolfState();
}

// MM player anims keep their root elsewhere: OoT's base translation is forced on a copy, not on the shared resource.
static LinkAnimationHeader* LoadCurlAnim(void) {
    LinkAnimationHeader* source = ResourceMgr_LoadPlayerAnimAsHeader(sCurlPath);
    if (source == NULL || source->common.frameCount <= 0) {
        return NULL;
    }
    size_t values = (size_t)source->common.frameCount * MM_ANIM_VALUES_PER_FRAME;
    sCurlFrames = malloc(values * sizeof(s16));
    if (sCurlFrames == NULL) {
        return NULL;
    }
    memcpy(sCurlFrames, source->segment, values * sizeof(s16));
    for (s32 frame = 0; frame < source->common.frameCount; frame++) {
        s16* root = &sCurlFrames[frame * MM_ANIM_VALUES_PER_FRAME];
        root[0] = MM_ANIM_BASE_TRANSL_X;
        root[2] = 0;
    }
    sCurlAnim.common.frameCount = source->common.frameCount;
    sCurlAnim.segment = sCurlFrames;
    return &sCurlAnim;
}

static bool CanUseCrystal(Player* player, PlayState* play) {
    return IsOnGround(player) &&
           !(player->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_IN_CUTSCENE));
}

static void UseCrystal(PlayState* play, Player* player) {
    bool wasWolf = sIsWolf;
    if (sApi->ToggleForm(WOLF_FORM_KEY)) {
        sShift = wasWolf ? SHIFT_TO_LINK : SHIFT_TO_WOLF;
        sShiftPastPeak = false;
        sShiftFrames = 0;
    }
}

static void DrawCrystalGetItem(PlayState* play, GetItemEntry* entry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(CRYSTAL_GET_ITEM_SCALE, CRYSTAL_GET_ITEM_SCALE, CRYSTAL_GET_ITEM_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sCrystalDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static const SOHCustomItemRandomizer sCrystalLogic = {
    sizeof(SOHCustomItemRandomizer), 0, SOH_CUSTOM_ITEM_TYPE_ITEM, 0, 1, WOLF_CRYSTAL_KEY, WOLF_CRYSTAL_KEY, "",
};

static void RegisterCrystal(void) {
    SOHCustomItemDefinition crystal = Z64Items_Define(WOLF_CRYSTAL_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&crystal, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    crystal.flags |= SOH_CUSTOM_ITEM_INSTANT;
    Z64Items_SetPlacement(&crystal, WOLF_CRYSTAL_PAGE, WOLF_CRYSTAL_SLOT, WOLF_CRYSTAL_WHEEL_PRIORITY);
    crystal.getItemEntry.drawFunc = DrawCrystalGetItem;
    Z64Items_SetTextbox(&crystal, "You got the %rShadow Crystal%w!&A shard of twilight that remembers&the shape of "
                                  "a beast.^"
                                  "Use it with %y\xA1%w to take the form of a&wolf, and again to be yourself.");
    Z64Items_SetPauseText(&crystal, "%rShadow Crystal&%wPress %y\xA1%w to become a wolf.&As a wolf: %y\xA0%w bites, "
                                    "%y\xA1%w dashes,&%y\xA1%w with %y\xA4%w hops aside.");
    Z64Items_SetCanUse(&crystal, CanUseCrystal);
    Z64Items_SetAction(&crystal, UseCrystal, NULL);
    Z64Items_SetLogic(&crystal, &sCrystalLogic);
    Z64Items_Register(sApi, &crystal);
}

static uint16_t ResolveWolfEquipment(int32_t equipType, uint16_t value) {
    if (equipType == EQUIP_TYPE_SWORD) {
        return EQUIP_VALUE_SWORD_NONE;
    }
    if (equipType == EQUIP_TYPE_SHIELD) {
        return EQUIP_VALUE_SHIELD_NONE;
    }
    return value;
}

static bool AllowsWolfItem(uint16_t item, const char* customKey) {
    return false;
}

static void RegisterWolfForm(void) {
    SOHFormDefinition wolf = { 0 };

    wolf.structSize = sizeof(wolf);
    wolf.key = WOLF_FORM_KEY;
    wolf.label = "Wolf Link";
    wolf.kind = SOH_FORM_KIND_LINK;
    wolf.item = WOLF_CRYSTAL_KEY;
    wolf.actions = sActions;
    wolf.actionCount = ARRAY_COUNT(sActions);
    wolf.resolveEquipment = ResolveWolfEquipment;
    wolf.allowsButtonItem = AllowsWolfItem;
    wolf.transformAnim = LoadCurlAnim();
    wolf.onEnter = EnterWolf;
    wolf.onExit = ExitWolf;
    wolf.update = UpdateWolf;
    sApi->RegisterForm(&wolf);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    for (s32 slot = 0; slot < VOICE_SLOTS; slot++) {
        sVoiceSlots[slot].sound = -1;
    }
    RegisterCrystal();
    RegisterWolfForm();
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDraw, ACTOR_PLAYER, DrawWolfBody);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorResolveGrayscale, ACTOR_PLAYER, DarkenBody);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorPlaySfx, ACTOR_PLAYER, MuteLinkVoice);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnPlayerResolveMotionScale, SOH_PLAYER_MOTION_STICK_SPEED, ScaleWolfSpeed);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, RunEveryFrame);
    if (SOH_MOD_API_HAS(sApi, RegisterAudioMixInGroup)) {
        sApi->RegisterAudioMixInGroup(MixWolfVoice, SOH_AUDIO_GROUP_SFX, NULL);
    }
}
