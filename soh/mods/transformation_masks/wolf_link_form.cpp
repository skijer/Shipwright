/**
 * Wolf Link full transformation.
 *
 * Assets are intentionally a loose `nei/wolf_link.bin`: mesh, weights,
 * skeleton, RGBA16 texture and all TP actions are loaded at runtime.  The
 * renderer reuses the SSBB CPU skinning path used by Pikachu.
 *
 * Behaviour is a port of Twilight Princess' wolf procs as recovered by the
 * decompilation that Dusklight is built on (src/d/actor/d_a_alink_wolf.inc,
 * d_a_alink.cpp, d_a_alink_HIO_data.inc).  Each proc below names the TP proc
 * it mirrors and the HIO table its numbers come from.  Conventions used to
 * bring TP values into OoT:
 *
 *   - TP runs its logic at 30 Hz, OoT at 20 Hz.  Animation frame counts are
 *     kept as authored (the .bin holds the 30 fps BCK data), so anim playback
 *     rates are scaled x1.5 and frame-count timers x2/3.
 *   - Horizontal speeds are expressed relative to OoT Link's run speed using
 *     the TP wolf/human ratios (human 23, wolf 25, A-dash 45, burst 65).
 *   - Vertical launches preserve TP air time: TP wolf gravity is -3.6 per
 *     30 Hz frame, OoT Link's is -1.0 per 20 Hz frame.
 *   - Collider radii/heights are in TP units and scaled by the wolf's own
 *     render scale, since the .bin geometry is in TP units.
 *
 * Everything gameplay-facing is multiplied by `gMods.WolfLink.SpeedScale`
 * so it can be tuned in-game without a rebuild.
 */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "mods/transformation_masks/wolf_link_form.h"
#include "soh/frame_interpolation.h"

// Defined in OTRGlobals.cpp. Declared here rather than including that header, which drags the whole
// OTRGlobals class into this TU for one accessor.
extern "C" const char* Nei_AssetDir(void);

#include <libultraship/bridge.h>
#include <libultraship/libultraship.h>

extern "C" {
#include "expansions/ssbb/ssbb_anim.h"
#include "expansions/ssbb/ssbb_character.h"
#include "expansions/ssbb/ssbb_skin.h"
extern PlayState* gPlayState;
}

namespace {

constexpr char kMagic[8] = { 'N', 'E', 'I', 'W', 'O', 'L', 'F', '1' };
constexpr u32 kVersion = 1;
constexpr size_t kHeaderSize = 8 + 20 * sizeof(u32);

// The exporter writes vertex records packed (3f + 3s8 + 2s16 + u8 = 20 bytes).
// SSBBSkinVertex is 4-byte aligned, so the compiler pads it to 24 — the blob can
// never be cast to SSBBSkinVertex* directly, it has to be unpacked field by
// field.  Every other record in the file happens to match its struct exactly
// (weights 8, MtxF 64, SSBBSkinBonePos 12, SSBBBoneFrame 36).
constexpr size_t kFileVertexStride = 20;

// ─────────────────────────────────────────────────────────────────────────────
// Animations.  Names are the TP BCK names ("wl_<name>") as exported from the
// rig; the WANM_ comments give the daAlink_WANM index they map to in TP.
// ─────────────────────────────────────────────────────────────────────────────
enum WolfAnim {
    WANM_WAIT,                 // wl_waita
    WANM_WALK_A,               // wl_walka
    WANM_WALK_B,               // wl_walkb (brisk walk / jog)
    WANM_DASH_A,               // wl_dasha (run)
    WANM_DASH_B,               // wl_dashb (quick run, A-dash)
    WANM_DASH_START,           // wl_dashst  (WANM 0x73)
    WANM_JUMP_ATTACK_START,    // wl_jumpast (0x04) — also the auto-jump takeoff
    WANM_JUMP_ATTACK,          // wl_jumpa   (0x05) — airborne loop
    WANM_JUMP_ATTACK_END,      // wl_jumpaed (0x06) — landing
    WANM_FALL_LAND,            // wl_landdama (0x60) — long fall pose
    WANM_ATTACK_B_LEFT,        // wl_attackbl (0x40) bite left
    WANM_ATTACK_B_RIGHT,       // wl_attackbr (0x41) bite right
    WANM_ATTACK_B_FRONT,       // wl_attackbs (0x42) front scratch
    WANM_ATTACK_B_TAIL,        // wl_attackbt (0x43) tail sweep (combo finisher)
    WANM_ATTACK_A_START,       // wl_attackast (0x50) lunge takeoff
    WANM_ATTACK_A,             // wl_attacka   (0x51) lunge airborne
    WANM_ATTACK_A_END,         // wl_attackaed (0x52) lunge normal landing
    WANM_ATTACK_A_END_FRONT,   // wl_attackaedf(0x53) lunge front slide
    WANM_ATTACK_A_END_BACK,    // wl_attackaedb(0x54) lunge back slide
    WANM_CUT_TURN_LEFT,        // wl_cutstl (0x5A) spin left
    WANM_CUT_TURN_RIGHT,       // wl_cutstr (0x5B) spin right
    WANM_ATTACK_RECOIL_START,  // wl_attackrest (0x74) bounced off a shield
    WANM_ATTACK_RECOIL_END,    // wl_attackreed (0x75)
    WANM_ATTACK_RECOIL_GROUND, // wl_attackregd (0x7A) dash rebound
    WANM_DMG_FRONT,            // wl_damf (0x3C)
    WANM_DMG_BACK,             // wl_damb (0x3D)
    WANM_COUNT,
};

const char* const kAnimNames[WANM_COUNT] = {
    "wl_armature_wl_waita",     "wl_armature_wl_walka",      "wl_armature_wl_walkb",      "wl_armature_wl_dasha",
    "wl_armature_wl_dashb",     "wl_armature_wl_dashst",     "wl_armature_wl_jumpast",    "wl_armature_wl_jumpa",
    "wl_armature_wl_jumpaed",   "wl_armature_wl_landdama",   "wl_armature_wl_attackbl",   "wl_armature_wl_attackbr",
    "wl_armature_wl_attackbs",  "wl_armature_wl_attackbt",   "wl_armature_wl_attackast",  "wl_armature_wl_attacka",
    "wl_armature_wl_attackaed", "wl_armature_wl_attackaedf", "wl_armature_wl_attackaedb", "wl_armature_wl_cutstl",
    "wl_armature_wl_cutstr",    "wl_armature_wl_attackrest", "wl_armature_wl_attackreed", "wl_armature_wl_attackregd",
    "wl_armature_wl_damf",      "wl_armature_wl_damb",
};

// ─────────────────────────────────────────────────────────────────────────────
// TP tuning tables (daAlinkHIO_*_c0::m defaults).  Values are the raw TP
// numbers; the Tp* helpers below convert them at the point of use so the
// tables stay diff-able against the decomp.
// ─────────────────────────────────────────────────────────────────────────────

// daAlinkHIO_anm_c: {endFrame, speed, startFrame, interpolation, cancelFrame}
struct TpAnm {
    f32 end, speed, start, interp, cancel;
};

// daAlinkHIO_wlMoveNoP_c0::m — normal locomotion (no A-dash active)
constexpr f32 kNopMaxSpeed = 25.0f;
constexpr f32 kNopIdleAnmSpeed = 1.0f, kNopWalkAnmSpeed = 0.8f, kNopJogAnmSpeed = 2.2f, kNopRunAnmSpeed = 1.1f;
constexpr f32 kNopIdleToWalk = 0.1f, kNopWalkToJog = 0.6f, kNopJogToRun = 0.6f;
constexpr f32 kNopDeceleration = 1.8f;

// daAlinkHIO_wlMove_c0::m — A-dash locomotion
constexpr f32 kDashIdleAnmSpeed = 1.6f, kDashWalkAnmSpeed = 1.1f, kDashBriskAnmSpeed = 2.2f;
constexpr f32 kDashRunAnmSpeed = 1.2f, kDashQuickRunAnmSpeed = 1.3f;
constexpr f32 kDashIdleToWalk = 0.1f, kDashWalkToBrisk = 0.4f, kDashStandbyRunToRun = 0.4f, kDashRunToQuick = 0.5f;
constexpr s16 kDashTurnMax = 9000, kDashTurnMin = 100, kDashTurnRate = 5;
constexpr s16 kADashDuration = 90, kADashCooldown = 50;
constexpr f32 kADashMaxSpeed = 45.0f, kADashAcceleration = 6.0f, kADashInitSpeed = 65.0f;
constexpr f32 kDashReboundH = 20.0f, kDashReboundV = 15.0f;
constexpr TpAnm kADashAnm = { 8, 1.0f, 0, 1, 20 };
constexpr TpAnm kDashReboundAnm = { 41, 1.0f, 0, 3, 20 };
constexpr f32 kTpHumanRun = 23.0f; // daAlinkHIO_move_c0::m.mMaxSpeed

// daAlinkHIO_wlAtWa{Lr,Sc,Tl}_c0::m — bite / scratch / tail sweep
struct TpWaitAttack {
    TpAnm anm;
    s16 stopTime, comboMidStopTime;
    f32 speed, speedAddForward, judgeStart, judgeEnd, comboMidCancel, comboMidStart, radiusOffset, radius, height;
};
constexpr TpWaitAttack kAtWaLr = { { 41, 0.9f, 4, 3, 16 }, 5, 3, 0.0f, 10.0f, 4.0f, 11.0f, 18.0f, 5.0f, 70, 70, 150 };
constexpr TpWaitAttack kAtWaSc = { { 15, 0.9f, 0, 3, 15 }, 5, 5, 10.0f, 3.0f, 5.0f, 11.0f, 18.0f, 0.0f, 100, 85, 150 };
constexpr TpWaitAttack kAtWaTl = {
    { 42, 1.05f, 3, 3, 28 }, 0, 3, 10.0f, 5.0f, 10.0f, 14.0f, 25.0f, 0.0f, 40, 150, 100
};

// daAlinkHIO_wlAtNjump_c0::m — lunge (B forward / combo)
constexpr TpAnm kNjumpAerialAnm = { 6, 1.0f, 4, 3, 7 };
constexpr f32 kNjumpInitSpeed = 30.0f, kNjumpMaxH = 40.0f, kNjumpMaxV = 23.0f, kNjumpMinV = 17.0f;
constexpr f32 kNjumpAerialAnmSpeed = 0.8f, kNjumpRadiusOffset = 80.0f, kNjumpRadius = 60.0f, kNjumpHeight = 120.0f;
constexpr f32 kNjumpMinH = 10.0f;

// daAlinkHIO_wlAtLand_c0::m
constexpr TpAnm kLandNormalAnm = { 19, 0.9f, 0, 2, 2 };
constexpr TpAnm kLandFrontSlideAnm = { 14, 1.0f, 0, 3, 1 };
constexpr TpAnm kLandBackSlideAnm = { 19, 1.1f, 0, 2, 1 };
constexpr f32 kLandSlideDecel = 2.0f;

// daAlinkHIO_wlAttack_c0::m
constexpr TpAnm kJumpBackLandAnm = { 59, 1.2f, 0, 2, 5 };
constexpr s16 kComboDuration = 5;
constexpr f32 kJumpBackSpeedH = 10.0f, kJumpBackSpeedV = 12.0f;

// daAlinkHIO_wlAtRoll_c0::m — spin
constexpr TpAnm kRollAnm = { 40, 1.0f, 4, 3, 23 };
constexpr f32 kRollRadius = 250.0f, kRollSpeed = 20.0f;

// daAlinkHIO_wlAutoJump_c0::m
constexpr TpAnm kAutoJumpAnm = { 3, 1.2f, 1, 2, 4 };
constexpr TpAnm kAutoLandAnm = { 24, 1.0f, 1, 2, 2 };
constexpr TpAnm kAutoClimbAnm = { 5, 0.5f, 2, 5, 7 };
constexpr f32 kTpWolfGravity = -3.6f;

// wolf damage / fall: air pose after this many TP units of fall
constexpr f32 kAirAnmTransitionHeight = 300.0f;

// ─────────────────────────────────────────────────────────────────────────────
// Unit conversion helpers
// ─────────────────────────────────────────────────────────────────────────────

// OoT Link's full-stick run target (Player_CalcSpeedAndYawFromControlStick,
// curved mode: ((1-cos(40*450))^2*30+7)*0.14).  Only a reference for ratios.
constexpr f32 kOotRunSpeed = 6.58f;
constexpr f32 kOotGravity = -1.0f; // REG(68)/100 for the player
constexpr f32 kTpToOotFrames = 20.0f / 30.0f;

static f32 SpeedScale() {
    return CVarGetFloat("gMods.WolfLink.SpeedScale", 1.0f);
}
// TP per-30Hz-frame horizontal speed → OoT per-20Hz-frame, relative to Link's run.
static f32 TpSpeed(f32 tp) {
    return tp * (kOotRunSpeed / kTpHumanRun) * (30.0f / 20.0f) * SpeedScale();
}
// TP vertical launch speed → OoT, preserving flight time (t = 2v/g).
static f32 TpVSpeed(f32 tp) {
    return tp * (kOotGravity / kTpWolfGravity) * (20.0f / 30.0f);
}
// TP animation rate (frames per 30Hz tick) → frames per 20Hz tick.
static f32 TpAnimRate(f32 tp) {
    return tp * 1.5f;
}
static s16 TpFrames(f32 tp) {
    return (s16)std::lround(tp * kTpToOotFrames);
}
// TP collider extents are in TP units, the same units the .bin mesh is in.
static f32 TpLength(f32 tp, f32 renderScale) {
    return tp * renderScale;
}

// Paw bones in the exported rig (armature order: FlegL4, FlegR4, BlegL4, BlegR4).
constexpr s32 kPawBones[4] = { 19, 24, 31, 36 };

// ─────────────────────────────────────────────────────────────────────────────
// Runtime state
// ─────────────────────────────────────────────────────────────────────────────
enum WolfProc {
    PROC_WOLF_MOVE,           // wait / walk / run / air, Link's actionFunc drives movement
    PROC_WOLF_LAND,           // procWolfLand — landing anim, cancellable
    PROC_WOLF_DASH,           // procWolfDash — A-dash burst
    PROC_WOLF_DASH_REVERSE,   // procWolfDashReverse — dash hit a wall
    PROC_WOLF_WAIT_ATTACK,    // procWolfWaitAttack — bite / scratch / tail
    PROC_WOLF_JUMP_ATTACK,    // procWolfJumpAttack — lunge
    PROC_WOLF_JUMP_AT_LAND,   // procWolfJumpAttack{Slide,Normal}Land
    PROC_WOLF_ROLL_ATTACK,    // procWolfRollAttack — spin
    PROC_WOLF_ATTACK_REVERSE, // procWolfAttackReverse — bounced off a shield
    PROC_WOLF_DAMAGE,         // hurt pose while OoT runs its own damage action
};

enum WolfDir { DIR_FORWARD, DIR_BACKWARD, DIR_LEFT, DIR_RIGHT, DIR_NONE };

struct WolfRuntime {
    SSBBCharacterInstance character{};
    s32 animIndex[WANM_COUNT];
    WolfAnim anim = WANM_WAIT;
    f32 animRate = 1.0f;
    f32 animEnd = 0.0f;
    u8 animLoop = 0;
    u8 initialized = 0;

    WolfProc proc = PROC_WOLF_MOVE;
    u8 procOwnsPlayer = 0;

    // A-dash mode (FLG1_DASH_MODE): timer field_0x30d0, cooldown field_0x30d2
    s16 dashModeTimer = 0;
    s16 dashCooldown = 0;
    u8 dashAttackQueued = 0; // mProcVar3 in procWolfDash: B pressed during the burst

    // combo (mComboCutCount / field_0x307e combo window)
    s32 comboCount = 0;
    s16 comboWindow = 0;
    u8 comboReserved = 0; // setComboReserb: B pressed while an attack was busy

    // per-proc scratch (mProcVar*)
    s16 stopTimer = 0;
    f32 cancelFrame = 0.0f;
    f32 judgeStart = 0.0f, judgeEnd = 0.0f, speedAddFrame = 0.0f, attackSpeed = 0.0f;
    f32 radiusOffset = 0.0f;
    s16 judgeFrames = 0;
    u8 flag0 = 0, flag1 = 0, flag2 = 0;
    u8 lungeType = 0;
    u8 lungeHit = 0;
    u8 airborneFlag = 0;
    f32 fallStartY = 0.0f;
    u8 wasOnGround = 1;
    u8 prevInvincible = 0;

    ColliderCylinder atCyl;
    u8 atCylInit = 0;
    u8 atActive = 0;
};

static WolfRuntime sWolf;
static u8 sSelected = 0;
static u8 sAssetsLoaded = 0;
static s32 sDefIndex = -1;
static std::vector<u8> sBlob;
static std::vector<SSBBSkinVertex> sVertices;
static std::vector<u16> sTexture;
static std::vector<StandardLimb> sLimbs;
static std::vector<void*> sLimbPointers;
static std::vector<SSBBAnim> sAnimations;
static std::vector<const SSBBAnim*> sAnimationPointers;
static std::vector<Gfx> sMeshDl;
static std::vector<Gfx> sMaterialDl;
static FlexSkeletonHeader sSkeleton{};
static SSBBSkinMesh sSkin{};
static SSBBCharacterDef sDefinition{};
static u32 sTextureWidth = 0;
static u32 sTextureHeight = 0;

static ColliderCylinderInit sAtCylInit = {
    { COLTYPE_HIT8, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0,
      { 0xFFCFFFFF, 0x00, 0x04 }, // toucher: all flags, damage set per attack (dCcD_SE_WOLF_BITE 2/3)
      { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { 20, 30, 0, { 0, 0, 0 } },
};

// ─────────────────────────────────────────────────────────────────────────────
// Blob reading
// ─────────────────────────────────────────────────────────────────────────────
static u16 ReadU16(const u8* p) {
    u16 value;
    std::memcpy(&value, p, sizeof(value));
    return value;
}

static s16 ReadS16(const u8* p) {
    s16 value;
    std::memcpy(&value, p, sizeof(value));
    return value;
}

static u32 ReadU32(const u8* p) {
    u32 value;
    std::memcpy(&value, p, sizeof(value));
    return value;
}

static f32 ReadF32(const u8* p) {
    f32 value;
    std::memcpy(&value, p, sizeof(value));
    return value;
}

static bool RangeOk(u32 offset, u32 size) {
    return offset <= sBlob.size() && size <= sBlob.size() - offset;
}

static std::string FindAssetPath() {
    const std::string rel = std::string(Nei_AssetDir()) + "/wolf_link.bin";
    std::string path = Ship::Context::LocateFileAcrossAppDirs(rel);
    if (path.empty()) {
        path = rel;
    }
    return path;
}

static void BuildMeshDisplayList(u32 vertexCount) {
    size_t commandCount = 1;
    for (u32 start = 0; start < vertexCount; start += 30) {
        u32 count = std::min<u32>(30, vertexCount - start);
        commandCount += 1 + ((count / 3) + 1) / 2;
    }
    sMeshDl.assign(commandCount, {});
    Gfx* gfx = sMeshDl.data();
    for (u32 start = 0; start < vertexCount; start += 30) {
        u32 count = std::min<u32>(30, vertexCount - start);
        // Do not call Ship's gSPVertex wrapper here.  A segmented address is a
        // display-list token, not readable host memory; the wrapper probes its
        // argument for an OTR signature and would dereference 0x08000000.
        // Emit the F3DEX2 command directly, as a static gsSPVertex would.
        //
        // The low bit is the segment marker: Interpreter::SegAddr only resolves
        // w1 through gSPSegment when (w1 & 1); without it the raw 0x080000xx is
        // dereferenced verbatim.  Static data does the same — see Pikachu's
        // `gsSPVertex(0x08000001, 32, 0)`.
        __gSPVertex(gfx++, (uintptr_t)(0x08000001u + start * sizeof(Vtx)), count, 0);
        u32 tri = 0;
        for (; tri + 1 < count / 3; tri += 2) {
            u32 a = tri * 3;
            u32 b = a + 3;
            gSP2Triangles(gfx++, a, a + 1, a + 2, 0, b, b + 1, b + 2, 0);
        }
        if (tri < count / 3) {
            u32 a = tri * 3;
            gSP1Triangle(gfx++, a, a + 1, a + 2, 0);
        }
    }
    gSPEndDisplayList(gfx++);
    sMeshDl.resize((size_t)(gfx - sMeshDl.data()));
}

static u32 Log2(u32 value) {
    u32 bits = 0;
    while ((1u << bits) < value) {
        ++bits;
    }
    return bits;
}

// The exporter emits every triangle twice (authored winding + reversed winding
// with the normal flipped), so back-face culling is what gives two-sided
// rendering here: only the copy facing the camera survives, and its normal
// faces the viewer, which is what Blender's two-sided lighting shows.  The
// CVar exists for A/B testing; with culling off both copies rasterise on top
// of each other and the far-facing normal wins on some pixels (dark specks).
static void BuildMaterialDisplayList(const u16* texture, u32 width, u32 height) {
    sMaterialDl.assign(24, {});
    Gfx* gfx = sMaterialDl.data();
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
    // LoadBlock is not usable here: its texel count is a 12-bit field clamped to
    // G_TX_LDBLK_MAX_TXL (4095), and a 256x256 sheet is 65536 texels.  LoadTile
    // takes 10.2 fixed-point bounds instead (up to 1024 texels per axis) and
    // derives the source stride from SetTextureImage's width, so that has to be
    // the real width rather than the usual LoadBlock sentinel of 1.
    gDPSetTextureImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, texture);
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
    sMaterialDl.resize((size_t)(gfx - sMaterialDl.data()));
}

static s32 FindAnim(const char* name) {
    for (size_t i = 0; i < sAnimations.size(); ++i) {
        if (sAnimations[i].name && std::strcmp(sAnimations[i].name, name) == 0) {
            return (s32)i;
        }
    }
    return -1;
}

static bool LoadAssets() {
    if (sAssetsLoaded) {
        return true;
    }
    std::ifstream file(FindAssetPath(), std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }
    std::streamsize fileSize = file.tellg();
    if (fileSize < (std::streamsize)kHeaderSize) {
        return false;
    }
    file.seekg(0, std::ios::beg);
    sBlob.resize((size_t)fileSize);
    if (!file.read((char*)sBlob.data(), fileSize)) {
        sBlob.clear();
        return false;
    }
    if (std::memcmp(sBlob.data(), kMagic, 8) != 0 || ReadU32(sBlob.data() + 8) != kVersion) {
        sBlob.clear();
        return false;
    }

    const u8* h = sBlob.data() + 12;
    u32 vertexCount = ReadU32(h + 0 * 4);
    u32 triangleCount = ReadU32(h + 1 * 4);
    u32 boneCount = ReadU32(h + 2 * 4);
    u32 animCount = ReadU32(h + 3 * 4);
    u32 textureWidth = ReadU32(h + 4 * 4);
    u32 textureHeight = ReadU32(h + 5 * 4);
    u32 offVertices = ReadU32(h + 6 * 4);
    u32 offWeights = ReadU32(h + 7 * 4);
    u32 offParents = ReadU32(h + 8 * 4);
    u32 offInvBind = ReadU32(h + 9 * 4);
    u32 offBonePos = ReadU32(h + 10 * 4);
    u32 offEntries = ReadU32(h + 11 * 4);
    u32 offNames = ReadU32(h + 12 * 4);
    u32 namesSize = ReadU32(h + 13 * 4);
    u32 offFrames = ReadU32(h + 14 * 4);
    u32 framesSize = ReadU32(h + 15 * 4);
    u32 offTexture = ReadU32(h + 16 * 4);
    u32 textureSize = ReadU32(h + 17 * 4);
    u32 totalSize = ReadU32(h + 18 * 4);

    // Texture must be a power of two per axis; the upper bound is LoadTile's
    // 10.2 fixed-point bounds field, which tops out at 1024 texels per axis.
    bool textureOk = textureWidth >= 8 && textureWidth <= 1024 && textureHeight >= 8 && textureHeight <= 1024 &&
                     (textureWidth & (textureWidth - 1)) == 0 && (textureHeight & (textureHeight - 1)) == 0;

    if (totalSize != sBlob.size() || vertexCount != triangleCount * 3 || vertexCount > 65535 || boneCount == 0 ||
        boneCount > SSBB_MAX_SKIN_BONES || animCount == 0 || !textureOk ||
        !RangeOk(offVertices, vertexCount * kFileVertexStride) ||
        !RangeOk(offWeights, vertexCount * sizeof(SSBBSkinWeight)) || !RangeOk(offParents, boneCount * 2) ||
        !RangeOk(offInvBind, boneCount * sizeof(MtxF)) || !RangeOk(offBonePos, boneCount * sizeof(SSBBSkinBonePos)) ||
        !RangeOk(offEntries, animCount * 16) || !RangeOk(offNames, namesSize) || !RangeOk(offFrames, framesSize) ||
        !RangeOk(offTexture, textureSize) || textureSize != textureWidth * textureHeight * 2) {
        sBlob.clear();
        return false;
    }

    // Unpack the 20-byte file records into the padded runtime struct.
    sVertices.assign(vertexCount, {});
    for (u32 i = 0; i < vertexCount; ++i) {
        const u8* v = sBlob.data() + offVertices + i * kFileVertexStride;
        SSBBSkinVertex& out = sVertices[i];
        out.posX = ReadF32(v + 0);
        out.posY = ReadF32(v + 4);
        out.posZ = ReadF32(v + 8);
        out.normX = (s8)v[12];
        out.normY = (s8)v[13];
        out.normZ = (s8)v[14];
        out.texS = ReadS16(v + 15);
        out.texT = ReadS16(v + 17);
        out.alpha = v[19];
    }

    // RGBA16 is loaded big-endian by Fast3D (`(data[0] << 8) | data[1]`), while
    // the exporter writes little-endian u16s.  Byte-swap once at load.
    sTexture.assign(textureWidth * textureHeight, 0);
    for (size_t i = 0; i < sTexture.size(); ++i) {
        u16 texel = ReadU16(sBlob.data() + offTexture + i * 2);
        sTexture[i] = (u16)((texel >> 8) | (texel << 8));
    }
    sTextureWidth = textureWidth;
    sTextureHeight = textureHeight;

    sLimbs.assign(boneCount, {});
    sLimbPointers.resize(boneCount);
    std::vector<s32> lastChild(boneCount, -1);
    for (u32 i = 0; i < boneCount; ++i) {
        sLimbs[i].child = LIMB_DONE;
        sLimbs[i].sibling = LIMB_DONE;
        sLimbs[i].dList = nullptr;
        sLimbPointers[i] = &sLimbs[i];
        s16 parent = ReadS16(sBlob.data() + offParents + i * 2);
        if (parent >= 0) {
            if ((u32)parent >= boneCount) {
                sBlob.clear();
                return false;
            }
            if (lastChild[parent] < 0) {
                sLimbs[parent].child = (u8)i;
            } else {
                sLimbs[lastChild[parent]].sibling = (u8)i;
            }
            lastChild[parent] = (s32)i;
        }
    }

    sAnimations.assign(animCount, {});
    sAnimationPointers.resize(animCount);
    for (u32 i = 0; i < animCount; ++i) {
        const u8* e = sBlob.data() + offEntries + i * 16;
        u32 nameOffset = ReadU32(e);
        u16 frameCount = ReadU16(e + 4);
        u16 animBones = ReadU16(e + 6);
        f32 frameRate = ReadF32(e + 8);
        u32 frameOffset = ReadU32(e + 12);
        u64 bytes = (u64)frameCount * animBones * sizeof(SSBBBoneFrame);
        if (nameOffset < offNames || nameOffset >= offNames + namesSize || animBones != boneCount ||
            !RangeOk(frameOffset, (u32)bytes)) {
            sBlob.clear();
            return false;
        }
        sAnimations[i] = { (const char*)sBlob.data() + nameOffset, frameCount, animBones, frameRate,
                           (const SSBBBoneFrame*)(sBlob.data() + frameOffset) };
        sAnimationPointers[i] = &sAnimations[i];
    }

    BuildMeshDisplayList(vertexCount);
    BuildMaterialDisplayList(sTexture.data(), textureWidth, textureHeight);

    sSkeleton.sh.segment = sLimbPointers.data();
    sSkeleton.sh.limbCount = (u8)(boneCount - 1);
    sSkeleton.sh.skeletonType = SKELANIME_TYPE_FLEX;
    sSkeleton.dListCount = 0;

    std::memset(&sSkin, 0, sizeof(sSkin));
    sSkin.vertexCount = (u16)vertexCount;
    sSkin.boneCount = (u16)boneCount;
    sSkin.vertices = sVertices.data();
    sSkin.weights = (SSBBSkinWeight*)(sBlob.data() + offWeights);
    sSkin.invBindMatrices = (MtxF*)(sBlob.data() + offInvBind);
    sSkin.bonePositions = (SSBBSkinBonePos*)(sBlob.data() + offBonePos);
    sSkin.daeToF64.mf[0][0] = 1.0f;
    sSkin.daeToF64.mf[1][1] = 1.0f;
    sSkin.daeToF64.mf[2][2] = 1.0f;
    sSkin.daeToF64.mf[3][3] = 1.0f;
    sSkin.f64ToDae = sSkin.daeToF64;
    sSkin.displayList = sMeshDl.data();
    sSkin.materialDL = sMaterialDl.data();
    sSkin.neutralizeRootMotion = 0;
    // 30 fps clips advanced 1.5 frames per tick: blend between the two
    // surrounding frames instead of snapping (the exporter keeps the Euler
    // tracks continuous, so component-wise blending is safe).
    sSkin.interpolateFrames = 1;

    std::memset(&sDefinition, 0, sizeof(sDefinition));
    sDefinition.name = "Wolf Link";
    sDefinition.skeleton = &sSkeleton;
    sDefinition.ssbbAnims = sAnimationPointers.data();
    sDefinition.numSSBBAnims = (u16)animCount;
    // The .bin is in TP units (wolf ~120 tall, ~230 long).  Tune live with
    // gExpansions.SSBB.SkinScale.
    sDefinition.scale = 0.5f;
    sDefinition.numLimbs = (u8)boneCount;
    sDefinition.rotOrder = SSBB_ROT_ORDER_ZYX;
    sDefinition.skinMesh = &sSkin;

    sDefIndex = SSBBChar_Register(&sDefinition);
    if (sDefIndex < 0) {
        sBlob.clear();
        return false;
    }
    sAssetsLoaded = 1;
    return true;
}

// Wolf's own size knob (the shared SSBB skin path draws at def->scale and no
// longer reads a CVar, so Pikachu is unaffected).  0.3 is the tuned value.
static f32 RenderScale() {
    f32 scale = CVarGetFloat("gMods.WolfLink.Scale", 0.3f);
    if (scale < 0.05f) {
        scale = 0.3f;
    }
    sDefinition.scale = scale;
    return scale;
}

// ─────────────────────────────────────────────────────────────────────────────
// Animation control.  SSBBSkin_Draw samples `character.curFrame` of
// `character.ssbbAnim`; we drive the frame counter ourselves so one-shots can
// hold their last frame and TP's start/end/cancel frames apply unchanged.
// ─────────────────────────────────────────────────────────────────────────────
static const SSBBAnim* AnimData(WolfAnim anim) {
    s32 index = sWolf.animIndex[anim];
    return index >= 0 ? &sAnimations[index] : nullptr;
}

// setSingleAnimeWolf(anim, speed, startFrame, endFrame, interp): endFrame < 0 = full clip.
static void SetAnim(WolfAnim anim, f32 tpRate, f32 startFrame, f32 endFrame, u8 loop) {
    const SSBBAnim* data = AnimData(anim);
    if (!data) {
        return;
    }
    sWolf.anim = anim;
    sWolf.character.ssbbAnim = data;
    sWolf.character.animLength = (f32)data->numFrames;
    sWolf.character.curFrame = std::min(startFrame, (f32)data->numFrames - 1);
    sWolf.animRate = TpAnimRate(tpRate);
    sWolf.animEnd = (endFrame < 0.0f || endFrame >= data->numFrames) ? (f32)data->numFrames - 1 : endFrame;
    sWolf.animLoop = loop;
}

static void SetAnimTp(WolfAnim anim, const TpAnm& p) {
    SetAnim(anim, p.speed, p.start, p.end, 0);
}

static void SetLoopAnim(WolfAnim anim, f32 tpRate) {
    if (sWolf.anim == anim && sWolf.animLoop && sWolf.character.ssbbAnim) {
        sWolf.animRate = TpAnimRate(tpRate);
        return;
    }
    SetAnim(anim, tpRate, 0.0f, -1.0f, 1);
}

static f32 Frame() {
    return sWolf.character.curFrame;
}

static bool AnimEnded() {
    return !sWolf.animLoop && sWolf.character.curFrame >= sWolf.animEnd;
}

// frameCtrl->checkPass(f): true on the tick the counter crosses f.
static f32 sPrevFrame = 0.0f;
static bool FramePassed(f32 f) {
    return sPrevFrame < f && sWolf.character.curFrame >= f;
}

static void AdvanceAnim() {
    sPrevFrame = sWolf.character.curFrame;
    if (!sWolf.character.ssbbAnim) {
        return;
    }
    f32 step = sWolf.animRate * (R_UPDATE_RATE * (1.0f / 3.0f));
    f32 next = sWolf.character.curFrame + step;
    if (sWolf.animLoop) {
        f32 len = sWolf.character.animLength;
        while (next >= len) {
            next -= len;
        }
    } else if (next > sWolf.animEnd) {
        next = sWolf.animEnd;
    }
    sWolf.character.curFrame = next;
}

// ─────────────────────────────────────────────────────────────────────────────
// Input / player helpers
// ─────────────────────────────────────────────────────────────────────────────
struct WolfInput {
    f32 stickMag = 0.0f;   // 0..60
    s16 stickWorldYaw = 0; // camera-relative stick direction, world yaw
    u8 aPress = 0, bPress = 0;
    u8 blocked = 0;
};

static WolfInput ReadInput(Player* player, PlayState* play) {
    WolfInput in;
    Input* input = &play->state.input[0];
    func_80077D10(&in.stickMag, &in.stickWorldYaw, input);
    in.stickWorldYaw = (s16)(Camera_GetInputDirYaw(GET_ACTIVE_CAM(play)) + in.stickWorldYaw);
    in.aPress = CHECK_BTN_ALL(input->press.button, BTN_A) != 0;
    in.bPress = CHECK_BTN_ALL(input->press.button, BTN_B) != 0;
    u32 blockMask = PLAYER_STATE1_LOADING | PLAYER_STATE1_TALKING | PLAYER_STATE1_DEAD | PLAYER_STATE1_GETTING_ITEM |
                    PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_HANGING_OFF_LEDGE |
                    PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_IN_ITEM_CS |
                    PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_IN_WATER | PLAYER_STATE1_ON_HORSE;
    if (player->stateFlags1 & blockMask) {
        in.aPress = in.bPress = 0;
        in.blocked = 1;
    }
    return in;
}

static bool OnGround(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
}

// getCutDirection: stick relative to facing, or DIR_NONE when neutral.
static WolfDir CutDirection(const WolfInput& in, Player* player) {
    if (in.stickMag < 20.0f) {
        return DIR_NONE;
    }
    s16 rel = (s16)(in.stickWorldYaw - player->actor.shape.rot.y);
    if (rel > -0x2000 && rel < 0x2000) {
        return DIR_FORWARD;
    }
    if (rel >= 0x6000 || rel <= -0x6000) {
        return DIR_BACKWARD;
    }
    return rel > 0 ? DIR_LEFT : DIR_RIGHT;
}

static void FacePlayer(Player* player, s16 yaw) {
    player->actor.shape.rot.y = yaw;
    player->actor.world.rot.y = yaw;
    player->yaw = yaw;
}

// Player yaw toward the lock-on target (cLib_targetAngleY(&pos, &target->eyePos)).
static bool FaceLockOn(Player* player) {
    Actor* target = player->focusActor;
    if (!target) {
        return false;
    }
    FacePlayer(player, Math_Vec3f_Yaw(&player->actor.world.pos, &target->world.pos));
    return true;
}

static void TakeOver(Player* player) {
    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
    sWolf.procOwnsPlayer = 1;
    player->yaw = player->actor.shape.rot.y;
    player->actor.world.rot.y = player->actor.shape.rot.y;
}

static void Release(Player* player) {
    if (sWolf.procOwnsPlayer) {
        player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
        sWolf.procOwnsPlayer = 0;
    }
    sWolf.atActive = 0;
}

// setCylAtParam(AT_TYPE_WOLF_ATTACK, ..., damage, radius, height): damage
// dCcD_SE_WOLF_BITE tier 2 (combo hits) or 3 (finishers), scaled to OoT quarter
// hearts as Kokiri sword (2) / Master sword (3)-ish equivalents.
static void SetAttackCollider(Player* player, PlayState* play, f32 tpRadius, f32 tpHeight, f32 tpOffset, u8 strong) {
    if (!sWolf.atCylInit) {
        Collider_InitCylinder(play, &sWolf.atCyl);
        Collider_SetCylinder(play, &sWolf.atCyl, &player->actor, &sAtCylInit);
        sWolf.atCylInit = 1;
    }
    f32 s = RenderScale();
    sWolf.atCyl.dim.radius = (s16)std::max(8.0f, TpLength(tpRadius, s));
    sWolf.atCyl.dim.height = (s16)std::max(10.0f, TpLength(tpHeight, s));
    sWolf.atCyl.dim.yShift = 0;
    sWolf.atCyl.info.toucher.damage = strong ? 4 : 2;
    sWolf.atCyl.info.toucher.dmgFlags = strong ? 0x00000200 /* DMG_SLASH_MASTER */ : 0x00000100 /* DMG_SLASH_KOKIRI */;
    sWolf.radiusOffset = tpOffset;
    sWolf.atActive = 1;
    sWolf.atCyl.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
}

static void UpdateAttackCollider(Player* player, PlayState* play) {
    if (!sWolf.atActive || !sWolf.atCylInit) {
        return;
    }
    Vec3f pos = player->actor.world.pos;
    f32 off = TpLength(sWolf.radiusOffset, RenderScale());
    pos.x += Math_SinS(player->actor.shape.rot.y) * off;
    pos.z += Math_CosS(player->actor.shape.rot.y) * off;
    sWolf.atCyl.dim.pos.x = (s16)pos.x;
    sWolf.atCyl.dim.pos.y = (s16)pos.y;
    sWolf.atCyl.dim.pos.z = (s16)pos.z;
    sWolf.atCyl.base.atFlags = AT_ON | AT_TYPE_PLAYER;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sWolf.atCyl.base);
}

static bool AttackHit() {
    return sWolf.atCylInit && (sWolf.atCyl.base.atFlags & AT_HIT);
}

static bool AttackBounced() {
    return sWolf.atCylInit && (sWolf.atCyl.base.atFlags & AT_BOUNCED);
}

// ─────────────────────────────────────────────────────────────────────────────
// Procs
// ─────────────────────────────────────────────────────────────────────────────
static void ProcMoveInit(Player* player);
static void ProcLandInit(Player* player);
static void ProcDashInit(Player* player, PlayState* play);
static void ProcDashReverseInit(Player* player);
static void ProcWaitAttackInit(Player* player, PlayState* play, s32 attackType);
static void ProcJumpAttackInit(Player* player, PlayState* play, s32 param);
static void ProcJumpAttackLandInit(Player* player, u8 slide, u8 back);
static void ProcRollAttackInit(Player* player, PlayState* play, s32 dir);
static void ProcAttackReverseInit(Player* player);
static void ProcDamageInit(Player* player);
static bool CheckWolfAttackAction(Player* player, PlayState* play, const WolfInput& in);
static bool CheckButtonAction(Player* player, PlayState* play, const WolfInput& in);
static bool CheckNextActionWolf(Player* player, PlayState* play, const WolfInput& in, u8 requireInput);

static void ResetCombo() {
    sWolf.comboCount = 0;
    sWolf.comboWindow = 0;
    sWolf.comboReserved = 0;
}

// setComboReserb: a B press during an attack is remembered for the cancel window.
static void SetComboReserve(const WolfInput& in) {
    if (in.bPress) {
        sWolf.comboReserved = 1;
    }
}

static bool DashModeActive() {
    return sWolf.dashModeTimer > 0;
}

// ── PROC_WOLF_MOVE: setBlendWolfMoveAnime + procWolfWait/procWolfMove ────────
static void ProcMoveInit(Player* player) {
    Release(player);
    sWolf.proc = PROC_WOLF_MOVE;
}

static void MoveUpdate(Player* player, PlayState* play, const WolfInput& in) {
    bool onGround = OnGround(player);
    f32 speed = std::fabs(player->linearVelocity);

    // ── airborne: procWolfAutoJump / procWolfFall visuals ──
    if (!onGround) {
        if (sWolf.wasOnGround) {
            sWolf.fallStartY = player->actor.world.pos.y;
            sWolf.airborneFlag = 0;
            SetAnimTp(WANM_JUMP_ATTACK_START, kAutoJumpAnm); // takeoff pose
        }
        f32 fallen = sWolf.fallStartY - player->actor.world.pos.y;
        if (fallen * (1.0f / RenderScale()) > kAirAnmTransitionHeight) {
            if (sWolf.anim != WANM_FALL_LAND) {
                SetAnim(WANM_FALL_LAND, 1.0f, 0.0f, -1.0f, 0);
            }
        } else if (sWolf.anim == WANM_JUMP_ATTACK_START && AnimEnded()) {
            SetAnimTp(WANM_JUMP_ATTACK, kAutoClimbAnm);
        } else if (sWolf.anim == WANM_JUMP_ATTACK && !sWolf.airborneFlag && player->actor.velocity.y < 0.0f) {
            // apex reached: TP switches the climb loop to falling rate
            sWolf.airborneFlag = 1;
            sWolf.animRate = TpAnimRate(kAutoClimbAnm.speed);
        }
        if (sWolf.anim == WANM_JUMP_ATTACK && AnimEnded()) {
            sWolf.character.curFrame = kAutoClimbAnm.start; // hold the airborne pose
        }
        // Lunge from mid-air is TP behaviour too (jump attack while falling)
        if (in.bPress && !in.blocked) {
            ProcJumpAttackInit(player, play, 0);
        }
        return;
    }

    if (!sWolf.wasOnGround) {
        // touched down: procWolfLandInit
        ProcLandInit(player);
        return;
    }

    // checkNextActionFromButton / checkMoveDoAction: B = attack, A = dash
    if (CheckButtonAction(player, play, in)) {
        return;
    }

    // ── locomotion blend (setBlendWolfMoveAnime) ──
    f32 mult = DashModeActive() ? (kADashMaxSpeed / kTpHumanRun) : (kNopMaxSpeed / kTpHumanRun);
    f32 maxSpeed = kOotRunSpeed * mult * SpeedScale();
    f32 rate = speed / std::max(0.01f, maxSpeed);
    if (rate > 1.0f) {
        rate = 1.0f;
    }

    if (DashModeActive()) {
        if (rate < kDashWalkToBrisk) {
            // dropping below brisk walk ends dash mode (setBlendWolfMoveAnime)
            sWolf.dashModeTimer = 0;
        }
        if (rate < kDashIdleToWalk) {
            SetLoopAnim(WANM_WAIT, kDashIdleAnmSpeed);
        } else if (rate < kDashWalkToBrisk) {
            f32 t = (rate - kDashIdleToWalk) / (kDashWalkToBrisk - kDashIdleToWalk);
            SetLoopAnim(t < 0.5f ? WANM_WALK_A : WANM_WALK_B,
                        kDashWalkAnmSpeed + (kDashBriskAnmSpeed - kDashWalkAnmSpeed) * t);
        } else if (rate < kDashRunToQuick) {
            SetLoopAnim(WANM_DASH_A, kDashRunAnmSpeed);
        } else {
            SetLoopAnim(WANM_DASH_B, kDashQuickRunAnmSpeed);
        }
    } else {
        if (rate < kNopIdleToWalk) {
            SetLoopAnim(WANM_WAIT, kNopIdleAnmSpeed);
        } else if (rate < kNopWalkToJog) {
            f32 t = (rate - kNopIdleToWalk) / (kNopWalkToJog - kNopIdleToWalk);
            SetLoopAnim(t < 0.5f ? WANM_WALK_A : WANM_WALK_B,
                        kNopWalkAnmSpeed + (kNopJogAnmSpeed - kNopWalkAnmSpeed) * t);
        } else {
            SetLoopAnim(WANM_DASH_A, kNopRunAnmSpeed);
        }
    }
}

// ── PROC_WOLF_LAND: procWolfLand ─────────────────────────────────────────────
static void ProcLandInit(Player* player) {
    Release(player);
    sWolf.proc = PROC_WOLF_LAND;
    SetAnimTp(WANM_JUMP_ATTACK_END, kAutoLandAnm);
    sWolf.cancelFrame = kAutoLandAnm.cancel;
}

static void LandUpdate(Player* player, PlayState* play, const WolfInput& in) {
    if (!OnGround(player)) {
        ProcMoveInit(player);
        return;
    }
    if (AnimEnded()) {
        ProcMoveInit(player);
        CheckNextActionWolf(player, play, in, 0);
    } else if (Frame() > sWolf.cancelFrame) {
        // cancellable by movement or buttons; otherwise finish the landing
        if (std::fabs(player->linearVelocity) > 0.5f || in.aPress || in.bPress) {
            ProcMoveInit(player);
            CheckNextActionWolf(player, play, in, 1);
        }
    }
}

// ── PROC_WOLF_DASH: procWolfDash (A while moving) ────────────────────────────
static void ProcDashInit(Player* player, PlayState* play) {
    TakeOver(player);
    sWolf.proc = PROC_WOLF_DASH;
    SetAnimTp(WANM_DASH_START, kADashAnm);
    sWolf.dashModeTimer = TpFrames(kADashDuration);
    sWolf.dashAttackQueued = 0;
    sWolf.flag0 = 0;
    f32 init = TpSpeed(kADashInitSpeed);
    if (player->linearVelocity < init) {
        player->linearVelocity = init;
    }
    FacePlayer(player, player->actor.shape.rot.y);
    Player_PlaySfx(&player->actor, NA_SE_PL_ROLL_DUST);
}

static void DashUpdate(Player* player, PlayState* play, const WolfInput& in) {
    f32 maxSpeed = TpSpeed(kADashMaxSpeed);
    // cLib_chaseF(&mNormalSpeed, mMaxSpeed, mADashAcceleration)
    Math_StepToF(&player->linearVelocity, maxSpeed, TpSpeed(kADashAcceleration));

    // steering: cLib_addCalcAngleS(&angle, mMoveAngle, rate 5, max 9000, min 100)
    if (in.stickMag > 8.0f) {
        s16 yaw = player->actor.shape.rot.y;
        Math_SmoothStepToS(&yaw, in.stickWorldYaw, kDashTurnRate, (s16)(kDashTurnMax * 1.5f),
                           (s16)(kDashTurnMin * 1.5f));
        FacePlayer(player, yaw);
    }

    // dash into a wall: procWolfDashReverse
    if (Frame() > 3.0f && (player->actor.bgCheckFlags & BGCHECKFLAG_WALL)) {
        ProcDashReverseInit(player);
        return;
    }
    if (in.bPress) {
        sWolf.dashAttackQueued = 1;
    }
    if (AnimEnded() || Frame() > kADashAnm.cancel) {
        sWolf.dashCooldown = TpFrames(kADashCooldown);
        if (sWolf.dashAttackQueued) {
            CheckWolfAttackAction(player, play, in);
            return;
        }
        ProcMoveInit(player);
        // hand the speed back to Link's actionFunc: it keeps chasing the stick
        // target, so dash mode's max stays in effect through the multiplier.
    }
}

// ── PROC_WOLF_DASH_REVERSE: procWolfDashReverse ──────────────────────────────
static void ProcDashReverseInit(Player* player) {
    TakeOver(player);
    sWolf.proc = PROC_WOLF_DASH_REVERSE;
    SetAnim(WANM_ATTACK_RECOIL_GROUND, kDashReboundAnm.speed, kDashReboundAnm.start, 5.0f, 0);
    player->linearVelocity = -TpSpeed(kDashReboundH);
    player->actor.velocity.y = TpVSpeed(kDashReboundV);
    sWolf.dashModeTimer = 0;
    sWolf.flag0 = 1; // airborne phase
    Player_PlaySfx(&player->actor, NA_SE_PL_BODY_HIT);
    Player_PlaySfx(&player->actor, NA_SE_VO_LI_DAMAGE_S);
}

static void DashReverseUpdate(Player* player, PlayState* play, const WolfInput& in) {
    if (sWolf.flag0) {
        if (OnGround(player) && player->actor.velocity.y <= 0.0f) {
            sWolf.flag0 = 0;
            player->linearVelocity = 0.0f;
            // continue the anim from frame 5 to the end at the rebound rate
            sWolf.animEnd = kDashReboundAnm.end;
        }
        return;
    }
    Math_StepToF(&player->linearVelocity, 0.0f, TpSpeed(kNopDeceleration));
    if (AnimEnded()) {
        ProcMoveInit(player);
    } else if (Frame() > kDashReboundAnm.cancel && (in.stickMag > 20.0f || in.aPress || in.bPress)) {
        ProcMoveInit(player);
        CheckNextActionWolf(player, play, in, 1);
    }
}

// ── PROC_WOLF_WAIT_ATTACK: procWolfWaitAttack (bite / scratch / tail) ───────
// attackType: 0 = B_LEFT (attackbl), 1 = B_FRONT (attackbs), 2 = TAIL (attackbt), 3 = B_RIGHT (attackbr)
static void ProcWaitAttackInit(Player* player, PlayState* play, s32 attackType) {
    static const WolfAnim anims[4] = { WANM_ATTACK_B_LEFT, WANM_ATTACK_B_FRONT, WANM_ATTACK_B_TAIL,
                                       WANM_ATTACK_B_RIGHT };
    const TpWaitAttack* hio = (attackType == 2) ? &kAtWaTl : (attackType == 1) ? &kAtWaSc : &kAtWaLr;

    TakeOver(player);
    sWolf.proc = PROC_WOLF_WAIT_ATTACK;
    sWolf.flag0 = 0; // voice played
    sWolf.flag2 = (u8)attackType;

    f32 startFrame;
    if (sWolf.comboCount == 4) {
        SetAttackCollider(player, play, hio->radius, hio->height, hio->radiusOffset, 1);
        sWolf.cancelFrame = hio->anm.cancel;
        sWolf.stopTimer = TpFrames(hio->stopTime);
        startFrame = hio->anm.start;
    } else {
        SetAttackCollider(player, play, hio->radius, hio->height, hio->radiusOffset, 0);
        sWolf.cancelFrame = hio->comboMidCancel;
        sWolf.stopTimer = TpFrames(hio->comboMidStopTime);
        startFrame = hio->comboMidStart;
    }
    SetAnim(anims[attackType], hio->anm.speed, startFrame, hio->anm.end, 0);

    FaceLockOn(player);
    FacePlayer(player, player->actor.shape.rot.y);
    sWolf.judgeFrames = 2; // mProcVar1: hitbox active ticks
    sWolf.judgeStart = hio->judgeStart;
    sWolf.judgeEnd = hio->judgeEnd;
    sWolf.speedAddFrame = hio->speedAddForward;
    sWolf.attackSpeed = hio->speed;
    sWolf.comboWindow = TpFrames(kComboDuration);
    sWolf.atActive = 0; // armed inside the judgement window
    Player_PlaySfx(&player->actor, sWolf.comboCount == 4 ? NA_SE_VO_LI_SWORD_L : NA_SE_VO_LI_SWORD_N);
    Player_PlaySfx(&player->actor, NA_SE_IT_SWORD_SWING);
}

static void WaitAttackUpdate(Player* player, PlayState* play, const WolfInput& in) {
    Math_StepToF(&player->linearVelocity, 0.0f, TpSpeed(kNopDeceleration));
    SetComboReserve(in);

    // checkWolfAttackReverse: bounced off a shield (not for the tail sweep)
    if (sWolf.flag2 != 2 && AttackBounced()) {
        ProcAttackReverseInit(player);
        return;
    }

    if (AnimEnded()) {
        ResetCombo();
        if (sWolf.stopTimer > 0) {
            if (!(Frame() > sWolf.cancelFrame && CheckNextActionWolf(player, play, in, 1))) {
                sWolf.stopTimer--;
            }
        } else {
            player->linearVelocity = 0.0f;
            ProcMoveInit(player);
            CheckNextActionWolf(player, play, in, 0);
        }
    } else if (Frame() > sWolf.cancelFrame) {
        if (!CheckNextActionWolf(player, play, in, 1)) {
            ResetCombo();
        }
    } else {
        FaceLockOn(player);
        FacePlayer(player, player->actor.shape.rot.y);
        if (FramePassed(sWolf.speedAddFrame)) {
            player->linearVelocity = TpSpeed(sWolf.attackSpeed);
        }
        if (Frame() >= sWolf.judgeStart && Frame() < sWolf.judgeEnd) {
            sWolf.atActive = 1; // onResetFlg0(RFLG0_UNK_2): attack judgement on this tick
        } else {
            sWolf.atActive = 0;
        }
    }
}

// ── PROC_WOLF_JUMP_ATTACK: procWolfJumpAttack (lunge) ───────────────────────
// param: 0 = normal, 2 = strong variant (from checkWolfAttackAction), 3 = follow-up
static void ProcJumpAttackInit(Player* player, PlayState* play, s32 param) {
    TakeOver(player);
    sWolf.proc = PROC_WOLF_JUMP_ATTACK;
    sWolf.lungeType = (u8)param;
    sWolf.lungeHit = 0;
    sWolf.flag0 = 0; // aerial loop started
    sWolf.flag1 = 0; // airborne at least one tick (checkWolfAttackReverse arg)

    u8 finisher = sWolf.comboCount == 4;
    SetAttackCollider(player, play, kNjumpRadius, kNjumpHeight, kNjumpRadiusOffset, finisher);
    SetAnimTp(WANM_ATTACK_A_START, kNjumpAerialAnm);

    f32 h = TpSpeed(kNjumpInitSpeed);
    f32 v = TpVSpeed(kNjumpMinV);
    Actor* target = player->focusActor;
    if (target) {
        FaceLockOn(player);
        // aim the arc at the target: h = d/t with the flight time TP derives from
        // the height difference; clamped to [minH, maxH] and [minV, maxV].
        f32 dy = (target->world.pos.y - player->actor.world.pos.y) - 10.0f * RenderScale();
        f32 t = dy > 0.0f ? sqrtf((2.0f * dy) / -kOotGravity) : 0.0f;
        f32 dist = Math_Vec3f_DistXZ(&player->actor.world.pos, &target->world.pos);
        if (t >= 1.0f) {
            h = dist / t;
            v = CLAMP(dy / t - 0.5f * kOotGravity * t, TpVSpeed(kNjumpMinV), TpVSpeed(kNjumpMaxV));
        } else if (sWolf.comboCount == 1 && param != 1) {
            v = TpVSpeed(kNjumpMinV);
            h = (-kOotGravity * dist) / (2.0f * v);
        }
    }
    h = CLAMP(h, TpSpeed(kNjumpMinH), TpSpeed(kNjumpMaxH));
    player->linearVelocity = h;
    player->actor.velocity.y = v;
    player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    FacePlayer(player, player->actor.shape.rot.y);
    sWolf.comboWindow = TpFrames(kComboDuration);
    sWolf.atActive = 1;
    Player_PlaySfx(&player->actor, finisher ? NA_SE_VO_LI_SWORD_L : NA_SE_VO_LI_SWORD_N);
    Player_PlaySfx(&player->actor, NA_SE_PL_SKIP);
}

static void JumpAttackUpdate(Player* player, PlayState* play, const WolfInput& in) {
    if (AttackHit()) {
        sWolf.lungeHit = 1;
    }
    if (AttackBounced() && sWolf.flag1) {
        ProcAttackReverseInit(player);
        return;
    }
    if (OnGround(player) && sWolf.flag1) {
        // landing: slide (finisher / strong / hit something) or normal
        u8 slide = (sWolf.comboCount == 4) || sWolf.lungeType == 2 || sWolf.lungeType == 3;
        u8 back = sWolf.lungeType == 2 && sWolf.lungeHit;
        ProcJumpAttackLandInit(player, slide, back);
        return;
    }
    sWolf.flag1 = 1;
    sWolf.comboWindow = TpFrames(kComboDuration);
    if (AnimEnded() && !sWolf.flag0) {
        sWolf.flag0 = 1;
        SetAnim(WANM_ATTACK_A, kNjumpAerialAnmSpeed, 0.0f, -1.0f, 0);
    }
    sWolf.atActive = 1;
}

// ── PROC_WOLF_JUMP_AT_LAND: procWolfJumpAttack{Slide,Normal}Land ────────────
static void ProcJumpAttackLandInit(Player* player, u8 slide, u8 back) {
    TakeOver(player);
    sWolf.proc = PROC_WOLF_JUMP_AT_LAND;
    sWolf.atActive = 0;
    sWolf.flag0 = slide;
    if (slide) {
        const TpAnm& anm = back ? kLandBackSlideAnm : kLandFrontSlideAnm;
        SetAnimTp(back ? WANM_ATTACK_A_END_BACK : WANM_ATTACK_A_END_FRONT, anm);
        sWolf.cancelFrame = anm.cancel;
        player->linearVelocity *= 0.5f;
    } else {
        SetAnimTp(WANM_ATTACK_A_END, kLandNormalAnm);
        sWolf.cancelFrame = kLandNormalAnm.cancel;
        player->linearVelocity = 0.0f;
    }
    sWolf.comboWindow = TpFrames(kComboDuration);
    Player_PlaySfx(&player->actor, NA_SE_PL_LAND);
}

static void JumpAttackLandUpdate(Player* player, PlayState* play, const WolfInput& in) {
    SetComboReserve(in);
    if (sWolf.flag0) {
        Math_StepToF(&player->linearVelocity, 0.0f, TpSpeed(kLandSlideDecel));
        if (AnimEnded()) {
            if (std::fabs(player->linearVelocity) < 0.1f) {
                ProcMoveInit(player);
                CheckNextActionWolf(player, play, in, 0);
            }
        } else if (Frame() > sWolf.cancelFrame && player->linearVelocity <= TpSpeed(5.0f)) {
            CheckNextActionWolf(player, play, in, 1);
        }
    } else {
        Math_StepToF(&player->linearVelocity, 0.0f, TpSpeed(kNopDeceleration));
        if (AnimEnded()) {
            ProcMoveInit(player);
            CheckNextActionWolf(player, play, in, 0);
        } else if (Frame() > sWolf.cancelFrame) {
            CheckNextActionWolf(player, play, in, 1);
        }
    }
}

// ── PROC_WOLF_ROLL_ATTACK: procWolfRollAttack (spin) ────────────────────────
static void ProcRollAttackInit(Player* player, PlayState* play, s32 dir) {
    TakeOver(player);
    sWolf.proc = PROC_WOLF_ROLL_ATTACK;
    SetAnimTp(dir == 1 ? WANM_CUT_TURN_RIGHT : WANM_CUT_TURN_LEFT, kRollAnm);
    // setCylAtParam(..., radius * 0.5, 155): the radius grows to the full value
    // during the active frames (cLib_chaseF(mAtCyl.GetRP(), radius, 20))
    SetAttackCollider(player, play, kRollRadius * 0.5f, 155.0f, 0.0f, 1);
    sWolf.atActive = 0;
    player->linearVelocity = 0.0f;
    FacePlayer(player, player->actor.shape.rot.y);
    Player_PlaySfx(&player->actor, NA_SE_VO_LI_SWORD_L);
    Player_PlaySfx(&player->actor, NA_SE_IT_SWORD_SWING_HARD);
}

static void RollAttackUpdate(Player* player, PlayState* play, const WolfInput& in) {
    Math_StepToF(&player->linearVelocity, 0.0f, TpSpeed(kNopDeceleration));
    if (AnimEnded()) {
        ProcMoveInit(player);
        CheckNextActionWolf(player, play, in, 0);
    } else if (Frame() > kRollAnm.cancel) {
        CheckNextActionWolf(player, play, in, 1);
    } else if (Frame() >= 4.0f && Frame() < 13.0f) {
        if (!AttackHit()) {
            player->linearVelocity = TpSpeed(kRollSpeed);
        }
        sWolf.atActive = 1;
        f32 full = TpLength(kRollRadius, RenderScale());
        f32 r = sWolf.atCyl.dim.radius;
        Math_StepToF(&r, full, TpLength(20.0f, RenderScale()) * 1.5f);
        sWolf.atCyl.dim.radius = (s16)r;
    } else {
        sWolf.atActive = 0;
    }
}

// ── PROC_WOLF_ATTACK_REVERSE: procWolfAttackReverse (shield bounce) ─────────
static void ProcAttackReverseInit(Player* player) {
    TakeOver(player);
    sWolf.proc = PROC_WOLF_ATTACK_REVERSE;
    sWolf.atActive = 0;
    SetAnim(WANM_ATTACK_RECOIL_START, 1.0f, 0.0f, -1.0f, 0);
    player->linearVelocity = -TpSpeed(kJumpBackSpeedH);
    player->actor.velocity.y = TpVSpeed(kJumpBackSpeedV);
    player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    sWolf.flag0 = 1; // airborne
    ResetCombo();
    Player_PlaySfx(&player->actor, NA_SE_IT_SHIELD_BOUND);
    Player_PlaySfx(&player->actor, NA_SE_VO_LI_DAMAGE_S);
}

static void AttackReverseUpdate(Player* player, PlayState* play, const WolfInput& in) {
    if (sWolf.flag0) {
        if (OnGround(player) && player->actor.velocity.y <= 0.0f) {
            sWolf.flag0 = 0;
            player->linearVelocity = 0.0f;
            SetAnimTp(WANM_ATTACK_RECOIL_END, kJumpBackLandAnm);
        }
        return;
    }
    if (AnimEnded()) {
        ProcMoveInit(player);
        CheckNextActionWolf(player, play, in, 0);
    } else if (Frame() > kJumpBackLandAnm.cancel && (in.stickMag > 20.0f || in.aPress || in.bPress)) {
        ProcMoveInit(player);
        CheckNextActionWolf(player, play, in, 1);
    }
}

// ── PROC_WOLF_DAMAGE: hurt pose while OoT runs its own damage action ────────
static void ProcDamageInit(Player* player) {
    Release(player);
    sWolf.proc = PROC_WOLF_DAMAGE;
    ResetCombo();
    sWolf.dashModeTimer = 0;
    // pick the anim from where the hit came from (front/back of the wolf)
    s16 diff = (s16)(player->actor.world.rot.y - player->actor.shape.rot.y);
    SetAnim(ABS(diff) < 0x4000 ? WANM_DMG_BACK : WANM_DMG_FRONT, 1.0f, 0.0f, -1.0f, 0);
}

static void DamageUpdate(Player* player, PlayState* play, const WolfInput& in) {
    if (AnimEnded() || player->invincibilityTimer <= 0) {
        ProcMoveInit(player);
    }
}

// ── checkWolfAttackAction: combo dispatcher (B) ─────────────────────────────
static bool CheckWolfAttackAction(Player* player, PlayState* play, const WolfInput& in) {
    static const s32 normalType0[] = { 3, 3, 3, 0, 0 };
    static const s32 normalType1[] = { 0, 0, 0, 3, 3 };

    if (sWolf.comboCount == 4) {
        ResetCombo();
    }
    sWolf.comboCount++;
    sWolf.comboReserved = 0;
    WolfDir dir = CutDirection(in, player);
    bool hasTarget = player->focusActor != nullptr;

    if (DashModeActive()) {
        sWolf.comboCount = 4;
        ProcJumpAttackInit(player, play, 0);
    } else if (sWolf.comboCount == 4) {
        if (!hasTarget) {
            if (dir == DIR_LEFT || dir == DIR_NONE) {
                ProcWaitAttackInit(player, play, 2);
            } else {
                ProcJumpAttackInit(player, play, 0);
            }
        } else if (dir == DIR_LEFT) {
            ProcRollAttackInit(player, play, 0);
        } else if (dir == DIR_RIGHT) {
            ProcRollAttackInit(player, play, 1);
        } else if (dir == DIR_FORWARD) {
            ProcJumpAttackInit(player, play, 0);
        } else {
            ProcJumpAttackInit(player, play, 2);
        }
    } else if (sWolf.comboCount == 2) {
        ProcWaitAttackInit(player, play, normalType0[dir]);
    } else if (sWolf.comboCount == 1 && dir == DIR_FORWARD) {
        ProcJumpAttackInit(player, play, 0);
    } else {
        ProcWaitAttackInit(player, play, normalType1[dir]);
    }
    return true;
}

// ── checkNextActionFromButton / checkMoveDoAction: B = attack, A = dash ──────
static bool CheckButtonAction(Player* player, PlayState* play, const WolfInput& in) {
    if (in.blocked) {
        return false;
    }
    // checkItemAction: swordSwingTrigger() || checkComboReserb()
    if (in.bPress || sWolf.comboReserved) {
        return CheckWolfAttackAction(player, play, in);
    }
    // BUTTON_STATUS_DASH: A while moving, once the cooldown (field_0x30d2) has run out
    if (in.aPress && OnGround(player) && sWolf.dashCooldown <= 0 && in.stickMag > 20.0f &&
        std::fabs(player->linearVelocity) > 0.5f) {
        ProcDashInit(player, play);
        return true;
    }
    return false;
}

// ── checkNextActionWolf(requireInput): what interrupts / follows the current proc ──
// requireInput == 0: the proc is over, always fall back to Move.
// requireInput == 1: still inside a cancel window, only leave on input.
static bool CheckNextActionWolf(Player* player, PlayState* play, const WolfInput& in, u8 requireInput) {
    if (in.blocked) {
        if (!requireInput) {
            ProcMoveInit(player);
        }
        return false;
    }
    if (CheckButtonAction(player, play, in)) {
        return true;
    }
    if (!requireInput || (in.stickMag > 20.0f && OnGround(player))) {
        ProcMoveInit(player);
        return true;
    }
    return false;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────
static bool PawWorldPos(Player* player, s32 paw, Vec3f* out);

extern "C" u8 WolfLinkForm_IsEnabled(void) {
    if (!CVarGetInteger("gMods.WolfLink.Enabled", 1)) {
        return 0;
    }
    std::ifstream file(FindAssetPath(), std::ios::binary);
    return file.good() ? 1 : 0;
}

extern "C" u8 WolfLinkForm_IsSelected(void) {
    return sSelected;
}

extern "C" void WolfLinkForm_Select(u8 selected) {
    sSelected = selected ? 1 : 0;
}

// Speed multiplier for Link's own locomotion while the wolf is active
// (Player_GetMovementSpeedAndYaw): TP wolf 25 / human 23 normally, A-dash 45 / 23.
extern "C" f32 WolfLinkForm_SpeedMultiplier(void) {
    if (!sWolf.initialized) {
        return 1.0f;
    }
    f32 base = DashModeActive() ? (kADashMaxSpeed / kTpHumanRun) : (kNopMaxSpeed / kTpHumanRun);
    return base * SpeedScale();
}

extern "C" u8 WolfLinkForm_LoadSkeleton(PlayState* play) {
    std::memset(&sWolf, 0, sizeof(sWolf));
    for (s32& index : sWolf.animIndex) {
        index = -1;
    }
    if (!LoadAssets()) {
        return 0;
    }
    // Rebuild the material each transform so the culling CVar applies live.
    BuildMaterialDisplayList(sTexture.data(), sTextureWidth, sTextureHeight);
    sSkin.materialDL = sMaterialDl.data();

    SSBBChar_Init(&sWolf.character, sDefIndex, play);
    for (s32 i = 0; i < WANM_COUNT; ++i) {
        sWolf.animIndex[i] = FindAnim(kAnimNames[i]);
        if (sWolf.animIndex[i] < 0) {
            WolfLinkForm_Cleanup();
            return 0;
        }
    }
    sWolf.proc = PROC_WOLF_MOVE;
    sWolf.wasOnGround = 1;
    SetLoopAnim(WANM_WAIT, kNopIdleAnmSpeed);
    sWolf.initialized = 1;
    return 1;
}

extern "C" void WolfLinkForm_Cleanup(void) {
    if (sWolf.character.def && sWolf.character.def->skinMesh) {
        SSBBSkin_Destroy(sWolf.character.def->skinMesh);
    }
    if (sWolf.character.jointTable) {
        ZELDA_ARENA_FREE_DEBUG(sWolf.character.jointTable);
        sWolf.character.jointTable = nullptr;
    }
    sWolf.initialized = 0;
    sWolf.procOwnsPlayer = 0;
    sWolf.atActive = 0;
    // Hand the shadow back to OoT (MmForm_UpdateActive re-asserts DrawFeet for
    // other forms; vanilla Link needs it restored here).
    if (gPlayState != NULL && GET_PLAYER(gPlayState) != NULL) {
        GET_PLAYER(gPlayState)->actor.shape.shadowDraw = ActorShadow_DrawFeet;
    }
}

extern "C" void WolfLinkForm_Update(Player* player, PlayState* play) {
    if (!sWolf.initialized || !sWolf.character.ssbbAnim) {
        return;
    }
    // ── body / collider shape (wolf is long and low) ──
    f32 s = RenderScale();
    for (s32 i = 0; i < PLAYER_BODYPART_MAX; ++i) {
        player->bodyPartsPos[i] = player->actor.world.pos;
        player->bodyPartsPos[i].y += 85.0f * s * 0.7f;
    }
    player->bodyPartsPos[PLAYER_BODYPART_L_FOOT].y = player->actor.world.pos.y;
    player->bodyPartsPos[PLAYER_BODYPART_R_FOOT].y = player->actor.world.pos.y;
    player->bodyPartsPos[PLAYER_BODYPART_HEAD].y = player->actor.world.pos.y + 110.0f * s;
    // feet = front paws (last draw); the hind pair is handled by DrawShadow
    if (!PawWorldPos(player, 0, &player->actor.shape.feetPos[0]) ||
        !PawWorldPos(player, 1, &player->actor.shape.feetPos[1])) {
        player->actor.shape.feetPos[0] = player->actor.world.pos;
        player->actor.shape.feetPos[1] = player->actor.world.pos;
    }
    player->actor.shape.shadowDraw = WolfLinkForm_DrawShadow;
    player->cylinder.dim.radius = (s16)std::max(12.0f, 40.0f * s);
    player->cylinder.dim.height = (s16)std::max(20.0f, 100.0f * s);
    player->cylinder.dim.yShift = 0;

    WolfInput in = ReadInput(player, play);

    // timers
    if (sWolf.dashModeTimer > 0) {
        sWolf.dashModeTimer--;
    }
    if (sWolf.dashCooldown > 0) {
        sWolf.dashCooldown--;
    }
    // field_0x307e: the combo survives this many ticks once an attack hands
    // control back; attacks refresh it while they run.
    if (sWolf.proc == PROC_WOLF_MOVE || sWolf.proc == PROC_WOLF_LAND) {
        if (sWolf.comboWindow > 0) {
            sWolf.comboWindow--;
        } else if (sWolf.comboCount != 0 || sWolf.comboReserved) {
            ResetCombo();
        }
    }

    // OoT's damage action fired (knockback / invincibility started): show the hurt pose
    u8 invincible = player->invincibilityTimer > 0;
    if (invincible && !sWolf.prevInvincible && sWolf.proc != PROC_WOLF_DAMAGE) {
        ProcDamageInit(player);
    }
    sWolf.prevInvincible = invincible;

    if (in.blocked && sWolf.procOwnsPlayer) {
        // a cutscene / dialogue / water took the player: drop the owned proc
        ProcMoveInit(player);
    }

    switch (sWolf.proc) {
        case PROC_WOLF_MOVE:
            MoveUpdate(player, play, in);
            break;
        case PROC_WOLF_LAND:
            LandUpdate(player, play, in);
            break;
        case PROC_WOLF_DASH:
            DashUpdate(player, play, in);
            break;
        case PROC_WOLF_DASH_REVERSE:
            DashReverseUpdate(player, play, in);
            break;
        case PROC_WOLF_WAIT_ATTACK:
            WaitAttackUpdate(player, play, in);
            break;
        case PROC_WOLF_JUMP_ATTACK:
            JumpAttackUpdate(player, play, in);
            break;
        case PROC_WOLF_JUMP_AT_LAND:
            JumpAttackLandUpdate(player, play, in);
            break;
        case PROC_WOLF_ROLL_ATTACK:
            RollAttackUpdate(player, play, in);
            break;
        case PROC_WOLF_ATTACK_REVERSE:
            AttackReverseUpdate(player, play, in);
            break;
        case PROC_WOLF_DAMAGE:
            DamageUpdate(player, play, in);
            break;
    }

    if (sWolf.procOwnsPlayer) {
        // keep the pause alive every tick (Link's own code clears it in places)
        player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
        // owned procs face where they move
        player->actor.world.rot.y = player->yaw = player->actor.shape.rot.y;
    }
    UpdateAttackCollider(player, play);
    sWolf.wasOnGround = OnGround(player);
    AdvanceAnim();
}

// World position of a paw from the last skinning pass (model space → actor
// pos/rot/scale, same transform SSBBSkin_Draw applies to the mesh).
static bool PawWorldPos(Player* player, s32 paw, Vec3f* out) {
    Vec3f local;
    if (!SSBBSkin_GetBoneWorldPos(kPawBones[paw], &local)) {
        return false;
    }
    f32 s = sDefinition.scale;
    f32 sn = Math_SinS(player->actor.shape.rot.y);
    f32 cs = Math_CosS(player->actor.shape.rot.y);
    local.x *= s;
    local.y *= s;
    local.z *= s;
    out->x = player->actor.world.pos.x + local.x * cs + local.z * sn;
    out->y = player->actor.world.pos.y + local.y;
    out->z = player->actor.world.pos.z - local.x * sn + local.z * cs;
    return true;
}

// actor->shape.shadowDraw for the wolf: OoT's DrawFeet only knows two feet, so
// run it twice — front paws, then hind paws — by swapping feetPos underneath
// it, the same trick that gives the MM forms per-foot shadows.  The "high in
// the air" circle it also draws is skipped on the second pass.
extern "C" void WolfLinkForm_DrawShadow(Actor* actor, Lights* lights, PlayState* play) {
    Player* player = (Player*)actor;
    Vec3f paws[4];
    bool ok = sWolf.initialized;
    for (s32 i = 0; ok && i < 4; ++i) {
        ok = PawWorldPos(player, i, &paws[i]);
    }
    if (!ok) {
        ActorShadow_DrawFeet(actor, lights, play);
        return;
    }
    f32 shadowScale = actor->shape.shadowScale;
    actor->shape.shadowScale = shadowScale * 0.75f; // paws are smaller than Link's boots
    actor->shape.feetPos[0] = paws[0];
    actor->shape.feetPos[1] = paws[1];
    ActorShadow_DrawFeet(actor, lights, play);
    if (actor->world.pos.y - actor->floorHeight <= 20.0f) {
        actor->shape.feetPos[0] = paws[2];
        actor->shape.feetPos[1] = paws[3];
        ActorShadow_DrawFeet(actor, lights, play);
    }
    actor->shape.feetPos[0] = paws[0];
    actor->shape.feetPos[1] = paws[1];
    actor->shape.shadowScale = shadowScale;
}

extern "C" void WolfLinkForm_Draw(PlayState* play, Player* player) {
    if (!sWolf.initialized) {
        return;
    }
    Vec3f pos = player->actor.world.pos;
    Vec3s rot = player->actor.shape.rot;
    SSBBSkin_Draw(&sWolf.character, play, &pos, &rot);
}
