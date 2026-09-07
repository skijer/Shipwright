/**
 * boss_remains.cpp - The four boss remains as custom wearable "masks" (Skijer's NEI).
 * SoH/OoT port of the 2ship module (mm/mods/boss_remains/boss_remains.cpp), 1:1 behavior.
 *
 * See boss_remains.h. Summary of the three phases wired here:
 *
 *  Phase 1 (equip): BossRemains_TryEquipAtCursor is called from the NEI MM kaleido
 *  quest page (z_kaleido_collect.c, KaleidoScope_DrawMmQuestStatus) when the cursor
 *  is on a remains point (0-3). The remains are plain u8 items on repurposed free
 *  ids (0x80/0x81/0x9C/0x89 — NON-contiguous, see BossRemains_ItemIndex), so they
 *  go straight into buttonItems — the same direct idiom Sw97_TryEquipMedallion
 *  uses. HUD icons resolve through ExtInv_GetItemIcon (mm.o2r remains icons).
 *
 *  Phase 2 (wear): BossRemains_TickInput (from Player_UpdateCommon) watches for a
 *  press on whichever C/D-pad button holds a remains and toggles it on/off Link's
 *  face. BossRemains_DrawWornMask (from Player_PostLimbDrawGameplay at the head
 *  limb) draws the Moon Child's face-fitted mask DL (gMoonChild*MaskDL, object_ob)
 *  — the exact model the moon children wear — using the moon child's own per-mask
 *  scale/rotate/translate. OoT port: mm.o2r isn't indexed in SoH, so the DLs are
 *  resolved to REAL pointers via MmAssets_LoadResource (transformation_masks
 *  bridge) instead of 2ship's pass-the-path-string trick.
 *
 *  Phase 3 (actions): per-remains A/B/R actions + friendly ally summons —
 *  Odolwa run/trail/moths/flight, Goht charge/pound/thunder/bombchu, Gyorg
 *  fish/whirlpool. Twinmold's Dark Link companion is NOT ported yet (stubbed).
 */

#include "boss_remains.h"

#include <libultraship/bridge.h> // CVarGetInteger

// OPEN_DISPS / CLOSE_DISPS redeclare these two symbols inline at each call site; in a C++ TU that
// takes C++ linkage unless a C declaration exists at file scope. Force the C symbols (same trick as
// spiritual_stones.cpp / PropHunt.cpp) so the macro's redeclaration matches and links.
extern "C" {
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
}

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
extern SaveContext gSaveContext;
// Custom player animations retargeted to Link's skeleton, packed in npc_link_anims.o2r (auto-mounted
// at boot). A SOH_PlayerAnimation resource is a raw s16 payload; ResourceMgr_LoadPlayerAnimAsHeader
// wraps it in a real LinkAnimationHeader and caches it (same pointer every call).
uint8_t ResourceMgr_FileExists(const char* resName);
LinkAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* animPath);
// OoT-native player animations (path symbols).
#include "objects/gameplay_keep/gameplay_keep.h"
// EnBom (Goht wall-crash keg blast).
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
// MM asset + SFX bridges (mm.o2r): MmAssets_LoadResource (real pointer or NULL — NULL means the
// archive isn't mounted yet; cache-retry next frame, never latch failure) and MmSfx_PlayAtPos/Stop.
#include "mods/transformation_masks/assets/mm_asset_loader.h"
#include "mods/anim_translator/mm_anim_loader.h" // MmAnim_LoadByPath — MM player anims out of mm.o2r
#include "mods/sound_translator/mm_sfx_ids.h"
// Ownership bits (Nei_Save()->mmQuestItems & FC_MMQ_REMAINS_*).
#include "mods/nei_save.h"
// z_player.c internal (no header) — the intended movement yaw/speed from the stick+camera. We reuse
// it to steer the bull charge stiffly. SPEED_MODE_CURVED (0.018f) is local to z_player.c, so its
// value is inlined at the call site.
s32 Player_GetMovementSpeedAndYaw(Player* player, f32* outSpeedTarget, s16* outYawTarget, f32 speedMode,
                                  PlayState* play);
// SW97 dynamic actor id for the Fire-Medallion ground-fire wave (Odolwa shield-deflect burst).
// -1 until the sw97 expansion registers it.
extern s16 gSw97ActorId_MagicFire;
// Power keg port: the "bonk to break stuff" obstacle blast (heavy blocks / boulders in radius).
void PowerKeg_SetBlast(Vec3f* center, f32 radius, s32 frame);

// ── Phase 3 summon allies ───────────────────────────────────────────────────
// Unity-#include the friendly-ally source files INSIDE this extern "C" block (like
// spiritual_stones.cpp #includes ../actors/spiritual_stone_statue.c) so their init/spawn symbols get
// C linkage. In SoH the actor ids are DYNAMIC (ActorDB) — gRemainsAllyBugId/ChuId/FishId, registered
// by BossRemains_EnsureActorsRegistered() (boss_remains_actor_reg.cpp). remains_ally_common.c MUST
// precede the three actors: they call RemainsAlly_FindNearestEnemy / _HomeTowardPos / _FollowPlayer,
// defined there. None of these are in CMake — they compile as part of this boss_remains.cpp TU.
// NOTE: remains_ally_link.c (Twinmold's Dark Link) is NOT ported yet.
#include "actors/remains_ally_common.h"
#include "actors/remains_ally_common.c"
#include "actors/remains_ally_bug.c"
#include "actors/remains_ally_chu.c"
#include "actors/remains_ally_fish.c"
// Dynamic actor ids + lazy profile registration (defined in boss_remains_actor_reg.cpp).
extern s16 gRemainsAllyBugId;
extern s16 gRemainsAllyChuId;
extern s16 gRemainsAllyFishId;
void BossRemains_EnsureActorsRegistered(void);
}

// ── MM SFX ids not present in mm_sfx_ids.h ──────────────────────────────────
// Values read from 2ship mm/include/sfx.h (verified). Played through the MmSfx bridge
// (MmSfx_PlayAtPos / MmSfx_Stop) — the MM "flagged continuous" (- SFX_FLAG) model doesn't exist in
// the bridge, so sustained cues are played once + explicitly stopped, and cadenced loops re-fire
// plain one-shots on their frame cadence.
#ifndef NA_SE_EN_MIBOSS_GND1_OLD
#define NA_SE_EN_MIBOSS_GND1_OLD 0x380C
#endif
#ifndef NA_SE_EN_MIBOSS_RHYTHM_OLD
#define NA_SE_EN_MIBOSS_RHYTHM_OLD 0x3810
#endif
#ifndef NA_SE_EN_MIBOSS_JUMP1
#define NA_SE_EN_MIBOSS_JUMP1 0x3813
#endif
#ifndef NA_SE_EN_MIBOSS_VOICE1_OLD
#define NA_SE_EN_MIBOSS_VOICE1_OLD 0x3815
#endif
#ifndef NA_SE_EN_MIBOSS_VOICE2_OLD
#define NA_SE_EN_MIBOSS_VOICE2_OLD 0x3816
#endif
#ifndef NA_SE_EN_COMMON_THUNDER_THR
#define NA_SE_EN_COMMON_THUNDER_THR 0x384D
#endif
#ifndef NA_SE_EN_BOMCHU_AIM
#define NA_SE_EN_BOMCHU_AIM 0x3855
#endif
#ifndef NA_SE_EN_ICEB_FOOTSTEP_OLD
#define NA_SE_EN_ICEB_FOOTSTEP_OLD 0x394A
#endif
#ifndef NA_SE_EN_COMMON_THUNDER
#define NA_SE_EN_COMMON_THUNDER 0x394B
#endif
#ifndef NA_SE_EN_MB_MOTH_FLY
#define NA_SE_EN_MB_MOTH_FLY 0x399B
#endif
#ifndef NA_SE_EN_PIRANHA_ATTACK
#define NA_SE_EN_PIRANHA_ATTACK 0x39F4
#endif

// ============================================================================
// State + config
// ============================================================================

namespace {

// Master toggle. Default ON so the feature works out of the box; a menu entry can
// gate it later. When OFF the remains behave as inert quest items.
inline bool RemainsEnabled() {
    return CVarGetInteger("gMods.BossRemains.Enabled", 1) != 0;
}

// The remains currently worn on Link's face (ITEM_MM_REMAINS_*), or ITEM_NONE.
// Transient per-session state (like the native currentMask "on" state).
s16 sWornRemains = ITEM_NONE;

// Odolwa A-action: while the Odolwa remains is worn, Link's roll is suppressed and he just RUNS —
// human locomotion is boosted (BossRemains_RunSpeedMul, read in the z_player.c run action), the
// red trail draws while moving, and Odolwa footstep SFX play on a cadence. No toggle needed.
s16 sOdolwaRunSfxTimer = 0;     // footstep-SFX cadence while running
s16 sOdolwaShieldFireTimer = 0; // cooldown between shield-deflect fire bursts
bool sOdolwaRunBoost = false;   // true while HOLDING A (worn) → 2x run + purple trail

// ── Odolwa custom animations (retargeted to Link, from npc_link_anims.o2r) ───
// Loaded once and cached: ResourceMgr_LoadPlayerAnimAsHeader already caches, but we also cache the
// pointer so the idle accessor can return the SAME header every call.
enum OdolwaAnimId {
    ODOLWA_ANIM_READY,
    ODOLWA_ANIM_SWING_DANCE,
    ODOLWA_ANIM_MOTH_DANCE,
    ODOLWA_ANIM_CROUCH,
    ODOLWA_ANIM_MAX
};
const char* const kOdolwaAnimPath[ODOLWA_ANIM_MAX] = {
    "__OTR__misc/link_animetion/gPlayerAnim_mhr_npc_odolwa_ready",
    "__OTR__misc/link_animetion/gPlayerAnim_mhr_npc_odolwa_arm_swing_dance",
    "__OTR__misc/link_animetion/gPlayerAnim_mhr_npc_odolwa_moth_summon_dance",
    "__OTR__misc/link_animetion/gPlayerAnim_mhr_npc_odolwa_crouch",
};
LinkAnimationHeader* sOdolwaAnimCache[ODOLWA_ANIM_MAX] = { nullptr, nullptr, nullptr, nullptr };
bool sOdolwaAnimTried[ODOLWA_ANIM_MAX] = { false, false, false, false };

// Returns the cached wrapped header, or NULL if the o2r isn't present (every caller must handle NULL).
LinkAnimationHeader* OdolwaAnim(OdolwaAnimId id) {
    if ((id < 0) || (id >= ODOLWA_ANIM_MAX)) {
        return nullptr;
    }
    if (!sOdolwaAnimTried[id]) {
        sOdolwaAnimTried[id] = true;
        if (ResourceMgr_FileExists(kOdolwaAnimPath[id])) {
            sOdolwaAnimCache[id] = ResourceMgr_LoadPlayerAnimAsHeader(kOdolwaAnimPath[id]);
        }
    }
    return sOdolwaAnimCache[id];
}

// ── Goht's MM player animations, pulled straight out of mm.o2r ──────────────
// These are REAL MM player anims (not OoT stand-ins): the bull charge is Link's MM flee-run
// (cl_nigeru) and the quake pound is the Zora jump-kick pair (pz_jumpAT/pz_jumpATend) — exactly what
// the 2ship version plays. They live in misc/link_animetion, which EXISTS IN BOTH archives, so a
// path-as-pointer would resolve to OoT's copy; MmAnim_LoadByPath goes through
// MmAssets_LoadResourceWithSize (archive-scoped to mm.o2r) instead, and rebuilds a LinkAnimationHeader
// with the MM baseTransl fix. limbCount 22 = the player skeleton these get played on (67 s16/frame),
// which is the same reader MM itself uses at playback — so the on-screen result matches 1:1.
enum GohtAnimId { GOHT_ANIM_CHARGE, GOHT_ANIM_JUMP, GOHT_ANIM_JUMP_END, GOHT_ANIM_MAX };
struct GohtAnimDef {
    const char* path;
    s16 frames;
};
const GohtAnimDef kGohtAnimDefs[GOHT_ANIM_MAX] = {
    { "misc/link_animetion/gPlayerAnim_cl_nigeru_Data", 8 },     // bull-charge run
    { "misc/link_animetion/gPlayerAnim_pz_jumpAT_Data", 13 },    // pound hop
    { "misc/link_animetion/gPlayerAnim_pz_jumpATend_Data", 13 }, // pound landing
};
LinkAnimationHeader* sGohtAnimCache[GOHT_ANIM_MAX] = { nullptr, nullptr, nullptr };

// Retry until mm.o2r is mounted, then latch (never latch a failure — same rule as MmRes).
LinkAnimationHeader* GohtAnim(GohtAnimId id) {
    if ((id < 0) || (id >= GOHT_ANIM_MAX)) {
        return nullptr;
    }
    if (sGohtAnimCache[id] == nullptr) {
        sGohtAnimCache[id] = MmAnim_LoadByPath(kGohtAnimDefs[id].path, kGohtAnimDefs[id].frames, 22);
    }
    return sGohtAnimCache[id];
}

// mm.o2r resource cache helper: retry every call until the archive is mounted, then latch the REAL
// pointer (the model draw.cpp:2507 uses — never latch failure).
inline void* MmRes(const char* path, void** cache) {
    if (*cache == nullptr) {
        *cache = MmAssets_LoadResource(path);
    }
    return *cache;
}

// ── Odolwa "Nimbus" flight (moth cloud) ─────────────────────────────────────
//   0 = grounded/off, 1 = the moth-summon dance is playing (takeoff windup), 2 = airborne on the cloud.
// Forced "spell-style" summon dance (bug/moth summon): a locked, uninterruptible dance we OWN frame by
// frame (the locomotion func can't steal it), like a spell cast holds the player.
s16 sOdolwaSummonLock = 0; // frames left in the locked dance
LinkAnimationHeader* sOdolwaSummonAnim = nullptr;
f32 sOdolwaSummonFrame = 0.0f; // hand-driven cycle frame
u16 sOdolwaSummonChant = 0;    // the chant sfx sustained through the locked dance (played ONCE
                               // at dance start via the MmSfx bridge, MmSfx_Stop'd at every
                               // dance-end/exit path — the bridge has no flagged-continuous)

s16 sOdolwaFlightState = 0;
s16 sOdolwaFlightWindup = 0;                   // frames left in the summon-dance windup before liftoff
s16 sOdolwaFlightTimer = 0;                    // frames left before the flight auto-lands (10s timeout)
s16 sOdolwaFlightPitch = 0;                    // aim pitch (binang), steered by stick Y
s16 sOdolwaFlightYaw = 0;                      // aim yaw (binang), steered by stick X
constexpr f32 kOdolwaFlightSpeed = 8.0f;       // forward advance speed (A held)
constexpr f32 kOdolwaFlightSoilRange = 120.0f; // how near soft soil (Obj_Bean) you must be to take off
constexpr s16 kOdolwaFlightMaxFrames = 200;    // ~10s at the game's 20fps logic tick → auto-land
constexpr s16 kOdolwaFlightTurnRate = 0x0A;    // yaw/pitch steer per stick unit (~7°/frame at full stick)
constexpr s16 kOdolwaFlightPitchMax = 0x3800;  // clamp so you can't flip straight up/down
constexpr s16 kOdolwaSummonDanceFrames = 45;   // hold the summon dance this long (chant length + a bit)
// NOTE (OoT port): MM gated takeoff on being near a deku flower (kOdolwaFlightDekuRange). OoT has no
// deku flowers, so the gate is REMOVED — take off anywhere on the ground.

// Goht A-action state:
//   A held        → BULL CHARGE: run anim + Majora-red cone + 3x forward speed. Drains 1 magic per
//                   15 frames (Pegasus-dash cadence); with NO magic it still charges, it just loses the
//                   contact damage + the cone. Goron-roll terrain: it ignores ledges/slopes and ramps
//                   launch Link. Crashing into a wall detonates a keg-class blast ("bonk to break stuff").
//   B held        → CHARGED GOHT THUNDER: hold to charge (light-orb VFX grows), release to fire at the
//                   enemy most in front. Charge scales reach + damage. Costs magic. Pierces walls.
//   R + A         → GORON QUAKE POUND: jump-attack anim + hop; on landing a small earthquake (camera
//                   quake + radial damage burst) then the landing anim.
//   R + B         → bombchu toggle: throw ONE friendly bombchu (one at a time); press R+B AGAIN while
//                   it's out to detonate it manually. It runs its own Real-Bombchu AI. Never targets Link.
bool sGohtCharging = false;      // A held → bull charge
s16 sGohtChargeCooldown = 0;     // lockout after a crash
s16 sGohtSfxTimer = 0;           // hoof-stomp cadence while charging
s16 sGohtQuakeState = 0;         // 0=idle, 1=airborne (jump anim), 2=landed (landing anim + damage burst)
s16 sGohtQuakeDmgFrames = 0;     // frames the radial quake AT stays live
s16 sGohtQuakeAnimDelay = 0;     // frames to wait after leaving the ground before playing the jump anim
s16 sGohtJumpPlayTimer = 0;      // frames the jump anim is allowed to play before we FREEZE its last frame
s16 sGohtRecoverFrames = 0;      // after the pound: force a plain idle so the attack pose doesn't linger
f32 sGohtChargeAnimFrame = 0.0f; // hand-driven run-cycle frame (we OWN the pose while charging)
ColliderCylinder sGohtQuakeCollider;
bool sGohtQuakeColReady = false;      // lazy Collider_InitCylinder done
ColliderCylinder sGohtChargeCollider; // light contact hit box while charging
bool sGohtChargeColReady = false;
bool sGohtSwordStashed = false;    // true while the B-button sword is unequipped (stashed) for Goht
bool sGohtNoSnapOwned = false;     // true while WE own the 0x800 bgCheckFlag (bull-charge ledge/slope ignore)
s16 sGohtChargeMagicTick = 0;      // bull-charge magic drain counter (Pegasus-style dedicated tick)
s16 sGohtThunderCharge = 0;        // B-hold charge level (frames) → scales the bolt's distance + damage
bool sGohtThunderCharging = false; // true while B (no R) is held (drives the charging light-orb VFX)

constexpr f32 kGohtChargeSpeed = 18.0f; // ~3x a normal run
// Bull charge drains magic OVER TIME, 1:1 with the Pegasus Anklet dash (equip_pegasus.c): 1 magic every
// 15 frames off a DEDICATED tick counter. Like the Pegasus, running dry does NOT stop the charge — it
// just loses the magic-powered parts (the contact-damage collider and the cone).
constexpr s16 kGohtChargeMagicInterval = 15;
constexpr f32 kGohtChargeAnimSpeed = 1.4f;  // charge-run anim advance per frame
constexpr s16 kGohtTurnStep = 0x2AA;        // ~3.7°/frame — stiff bull steering, harder than a Goron roll
constexpr f32 kGohtJumpVel = 15.5f;         // high, snappy pound hop
constexpr f32 kGohtJumpGravityBoost = 5.0f; // extra downward accel past the apex → fast, snappy descent
constexpr s16 kGohtJumpAnimDelay = 3;       // play the jump anim a few frames AFTER leaving the ground
constexpr f32 kGohtBounceSpeed = -16.0f;    // wall-crash recoil (backwards)
constexpr f32 kGohtBounceHop = 6.0f;        // wall-crash recoil little hop
constexpr s16 kGohtThunderMagicCost = 4;    // magic per thunder shot (flat, deducted directly like Odolwa)
constexpr s16 kGohtThunderChargeMax = 45;   // frames of B hold for a full charge
constexpr s16 kGohtThunderChargeMin = 5;    // minimum charge before a shot will fire (anti-spam)
constexpr s16 kGohtThunderTtlMin = 12;      // bolt lifetime → distance at min charge
constexpr s16 kGohtThunderTtlMax = 64;      // ... at full charge
constexpr s16 kGohtThunderDmgMin = 2;       // bolt damage at min charge
constexpr s16 kGohtThunderDmgMax = 10;      // ... at full charge

// The bull-charge anim frame count (hand-driven loop). OoT gPlayerAnim_link_normal_run is a ~20-frame
// cycle; the accumulator wraps on the real last frame at runtime (Animation_GetLastFrame).
// The Goron-roll "don't snap to small drops" player bgCheckFlag. SoH z_actor.c:1652 honors the raw
// bit but exposes NO macro for it (MM calls it BGCHECKFLAG_PLAYER_800) — keep the literal.
constexpr s32 kBgCheckFlagPlayer800 = 0x800;

inline bool DpadEquipsEnabled() {
    // CVAR_ENHANCEMENT("DpadEquips") — prefix "gEnhancements" comes from soh-cvars.cmake; raw string
    // here so this TU doesn't need the cvar_prefixes compile definitions.
    return CVarGetInteger("gEnhancements.DpadEquips", 0) != 0;
}

// A remains is owned iff its Fleet combo-sync bit is set (FC_MMQ_REMAINS_ODOLWA..TWINMOLD = bits 0-3).
inline bool RemainsOwned(s32 idx) {
    return (idx >= 0) && (idx < 4) && ((Nei_Save()->mmQuestItems & (1u << idx)) != 0);
}

// ── Worn-mask draw data ────────────────────────────────────────────────────
// The face-fitted geometry is the Moon Child's masks (object_ob, mm.o2r). mm.o2r isn't indexed in
// SoH, so the DLs are resolved to real pointers with MmAssets_LoadResource (paths WITHOUT the
// __OTR__ prefix). Index: 0=Odolwa 1=Goht 2=Gyorg 3=Twinmold.
const char* const kMaskDLPath[4] = {
    "objects/object_ob/gMoonChildOdolwasMaskDL",
    "objects/object_ob/gMoonChildGohtsMaskDL",
    "objects/object_ob/gMoonChildGyorgsMaskDL",
    "objects/object_ob/gMoonChildTwinmoldsMaskDL",
};
void* sMaskDLCache[4] = { nullptr, nullptr, nullptr, nullptr };

// Per-mask transform, copied verbatim from MM z_en_js.c (D_8096ABE0/ABF4/AC08/AC1C, entries 1..4 —
// index 0 there is Majora's, which we skip). These are authored for the Moon Child's head node;
// Link's head node has a different base scale, so kScaleMul / the offsets below are the knobs to
// tune visually if the mask sits off the face.
const f32 kMaskScale[4] = { 0.5f, 0.5f, 0.48f, 0.45f };
const f32 kMaskTransX[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
const f32 kMaskTransY[4] = { 1400.0f, 1470.0f, 1670.0f, 1470.0f };
const f32 kMaskTransZ[4] = { 700.0f, 900.0f, 900.0f, 900.0f };

// Worn-mask placement on Link's head — final values tuned in-game (2ship) then baked. Rotation is in
// degrees; the offset is ADDED to each mask's per-mask base translate; the scale MULTIPLIES each
// mask's per-mask base scale. Shared by all four remains.
// TODO(port-verify): tuned on MM human Link's head node — OoT child/adult head nodes may need a
// retune of these constants (visual only).
constexpr s32 kRotXDeg = 180;
constexpr s32 kRotYDeg = -90;
constexpr s32 kRotZDeg = 15;
constexpr f32 kOffX = 0.0f;
constexpr f32 kOffY = -510.0f;
constexpr f32 kOffZ = 383.0f;
constexpr f32 kScaleMul = 1.04f;

inline s16 DegToBinang(s32 deg) {
    return (s16)((deg * 0x10000) / 360);
}

} // namespace

// ============================================================================
// Item-id helpers (the OoT ids are NON-contiguous: 0x80, 0x81, 0x9C, 0x89)
// ============================================================================

extern "C" s32 BossRemains_ItemIndex(s16 item) {
    switch (item) {
        case ITEM_MM_REMAINS_ODOLWA:
            return 0;
        case ITEM_MM_REMAINS_GOHT:
            return 1;
        case ITEM_MM_REMAINS_GYORG:
            return 2;
        case ITEM_MM_REMAINS_TWINMOLD:
            return 3;
        default:
            return -1;
    }
}

extern "C" s16 BossRemains_IndexItem(s32 idx) {
    switch (idx) {
        case 0:
            return ITEM_MM_REMAINS_ODOLWA;
        case 1:
            return ITEM_MM_REMAINS_GOHT;
        case 2:
            return ITEM_MM_REMAINS_GYORG;
        case 3:
            return ITEM_MM_REMAINS_TWINMOLD;
        default:
            return ITEM_NONE;
    }
}

// ============================================================================
// Phase 1 — equip from the NEI MM kaleido quest page
// ============================================================================

// Modeled 1:1 on Sw97_TryEquipMedallion (z_kaleido_collect.c): C/D press detect → sentinel equip.
// `item` is the hovered remains (the MM quest page zeroes cursorItem for non-song points, so the
// caller passes it explicitly — see the header note).
extern "C" s32 BossRemains_TryEquipAtCursor(PlayState* play, Input* input, s16 item) {
    if (!RemainsEnabled() || play == nullptr || input == nullptr) {
        return false;
    }

    s32 idx = BossRemains_ItemIndex(item);
    if (idx < 0) {
        return false;
    }
    // Must actually own it (the mmQuestItems bit), so an empty diamond can't be equipped.
    if (!RemainsOwned(idx)) {
        return false;
    }

    // Detect C-button or D-pad press (Sw97 mapping: 0/1/2 = C-left/down/right, 3..6 = D-pad).
    s32 targetCBtn = -1;
    if (CHECK_BTN_ALL(input->press.button, BTN_CLEFT)) {
        targetCBtn = 0;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_CDOWN)) {
        targetCBtn = 1;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_CRIGHT)) {
        targetCBtn = 2;
    } else if (DpadEquipsEnabled()) {
        if (CHECK_BTN_ALL(input->press.button, BTN_DUP)) {
            targetCBtn = 3;
        } else if (CHECK_BTN_ALL(input->press.button, BTN_DDOWN)) {
            targetCBtn = 4;
        } else if (CHECK_BTN_ALL(input->press.button, BTN_DLEFT)) {
            targetCBtn = 5;
        } else if (CHECK_BTN_ALL(input->press.button, BTN_DRIGHT)) {
            targetCBtn = 6;
        }
    }
    if (targetCBtn < 0) {
        return false;
    }

    // Sentinel equip: buttonItems[targetCBtn + 1] (slot 0 is B), 0xFF cButtonSlots marker for the
    // real C buttons ("not from an inventory slot"), then refresh the HUD icon.
    gSaveContext.equips.buttonItems[targetCBtn + 1] = (u8)item;
    if (targetCBtn < 3) {
        gSaveContext.equips.cButtonSlots[targetCBtn] = 0xFF;
    }
    Interface_LoadItemIcon1(play, (u16)(targetCBtn + 1));
    Audio_PlaySoundGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    return true;
}

// ============================================================================
// Phase 2 — wear toggle + face draw
// ============================================================================

extern "C" s16 BossRemains_GetWorn(void) {
    return sWornRemains;
}

extern "C" void BossRemains_ClearWorn(void) {
    sWornRemains = ITEM_NONE;
}

// ============================================================================
// Phase 3 — per-remains friendly ally summon driver
// ============================================================================
//   Odolwa (idx 0): SHIELD (R) + B  → spawn a small swarm of friendly beetles near Link (magic).
//   Goht   (idx 1): SHIELD (R) + B  → friendly bombchu;  B held → charged thunder bolt (magic).
//   Gyorg  (idx 2): R while swimming / SHIELD (R) + B on land → Desbreko-look fish school.
// The RemainsAlly*_Spawn functions are unity-included above, so they are directly callable.

// Defined in the Goht block further down; TickSummons (Goht thunder aiming) needs it earlier.
static Actor* BossRemains_FindFrontEnemy(PlayState* play, Player* player, f32 maxDist, s16 cone);

// Start a forced "spell-style" Odolwa summon dance (locked until it finishes). Defined near the flight
// driver; TickSummons calls it when summoning bugs.
static void BossRemains_OdolwaSummonDance(PlayState* play, Player* player, OdolwaAnimId animId);

// Spawn Gyorg's little fish school near Link. Shared by the Shield+B summon and the in-water
// "dive button" summon hooked player-side — pressing the dive input while swimming with Gyorg makes
// fish instead of diving. Fish self-manage (swim/dart in water, flop/bite/die on land).
extern "C" void BossRemains_GyorgSummonFish(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr || !BossRemains_IsGyorgWorn()) {
        return;
    }
    BossRemains_EnsureActorsRegistered();
    if (gRemainsAllyFishId < 0) {
        return;
    }
    s16 yaw = player->actor.shape.rot.y;
    for (s32 i = 0; i < 3; i++) {
        Vec3f p = player->actor.world.pos;
        p.x += Rand_CenteredFloat(90.0f);
        p.z += Rand_CenteredFloat(90.0f);
        p.y += 12.0f;
        Actor_Spawn(&play->actorCtx, play, gRemainsAllyFishId, p.x, p.y, p.z, 0, yaw, 0, (s16)i);
    }
    MmSfx_PlayAtPos(NA_SE_EN_PIRANHA_ATTACK, &player->actor.projectedPos);
}

// ============================================================================
// GYORG — WHIRLPOOL (B while swimming): a stationary vortex that PULLS IN and DAMAGES nearby enemies
// (Gyorg's water-current trap). Damage via a friendly AT cylinder centered on the cast point (same
// friend/foe idea as the Goht quake / the fish); the pull nudges each enemy's world pos toward the
// center each frame. The MM visual (Gyorg's REAL EnWaterEffect water funnel) has NO OoT counterpart
// — see the stub below.
// ============================================================================
static bool sGyorgWhirlpoolActive = false; // true while B is HELD (swimming) with magic to spend
static s16 sGyorgWhirlpoolDrain = 0;       // Deku-leaf-style magic-drain frame counter
static Vec3f sGyorgWhirlpoolPos;           // aim point — the mask mouth (reach 0, see below)
static ColliderCylinder sGyorgWhirlpoolCollider;
static bool sGyorgWhirlpoolColReady = false;

static constexpr f32 kGyorgWhirlpoolRadius = 220.0f; // pull + damage XZ radius
static constexpr f32 kGyorgWhirlpoolPull = 14.0f;    // pull speed toward the aim point per frame
static constexpr s16 kGyorgWhirlpoolDrainRate = 10;  // 1 magic every 10 frames — the Mogma Mitts pace

// Reach 0 = the vortex forms AT the mask's mouth. This also puts the pull centre on Link himself, so
// the vortex drags enemies onto him — straight into the damage sphere.
static constexpr f32 kGyorgFunnelReach = 0.0f;
static constexpr f32 kGyorgFunnelOffsetY = -5.25f;
static constexpr s16 kGyorgAuraRadius = 95;

// OoT ColliderCylinderInit layout (COLTYPE/ELEMTYPE/TOUCH/BUMP naming). Friendly AT: hits enemies'
// AC_TYPE_PLAYER bumpers, never Link. DMG_DEKU_STICK class so enemy bumpers accept it; small dmg.
static ColliderCylinderInit sGyorgWhirlpoolColliderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { DMG_DEKU_STICK, 0x00, 0x02 }, // toucher {dmgFlags, effect, damage}
        { 0xFFCFFFFF, 0x00, 0x00 },     // bumper mask inert (AC off)
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { 200, 120, -40, { 0, 0, 0 } },
};

// The protective "damage sphere" around LINK himself while the whirlpool is up: anything the vortex
// drags close keeps taking hits and never gets to touch him. Same friendly AT setup, centered on Link.
static ColliderCylinder sGyorgAuraCollider;
static bool sGyorgAuraColReady = false;

static ColliderCylinderInit sGyorgAuraColliderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { DMG_DEKU_STICK, 0x00, 0x04 }, // hits harder than the vortex body — this is the "keep off" ring
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { kGyorgAuraRadius, 110, -45, { 0, 0, 0 } },
};

// Per-frame whirlpool driver: aim + magic drain + pull + damage. Runs while HELD (sGyorgWhirlpoolActive,
// set in TickSummons from B-held + magic). It drains magic Deku-leaf style and drags every nearby
// enemy into it.
static void BossRemains_GyorgWhirlpoolTick(PlayState* play, Player* player) {
    if (!BossRemains_IsGyorgWorn()) {
        sGyorgWhirlpoolActive = false; // doffed / not Gyorg → force off (TickSummons won't run its case)
    }
    if (!sGyorgWhirlpoolActive) {
        return;
    }

    // Aim point: Link's head (the worn mask's mouth). MM aimed it out along facing yaw + swim pitch
    // (player->unk_AAA); with kGyorgFunnelReach = 0 the yaw/pitch terms are all zero, so the OoT port
    // needs no swim-pitch field at all (OoT has no unk_AAA equivalent — the Gyorg swim is hooked
    // player-side by the z_player.c agent).
    sGyorgWhirlpoolPos.x = player->actor.focus.pos.x;
    sGyorgWhirlpoolPos.y = player->actor.focus.pos.y + kGyorgFunnelOffsetY;
    sGyorgWhirlpoolPos.z = player->actor.focus.pos.z;

#if 0 // TODO(port-verify): MM visual — Gyorg's REAL water funnel (ACTOR_EN_WATER_EFFECT,
      // ENWATEREFFECT_TYPE_GYORG_PRIMARY_SPRAY, OBJECT_WATER_EFFECT) does not exist in OoT. The MM
      // module respawned an aimed EnWaterEffect spray every 14 frames and re-aimed/reshaped the live
      // cones each frame. Port a stand-in visual (e.g. a custom translucent cone) later; the pull +
      // damage + drain below are fully functional without it.
#endif

    // Deku-leaf-style drain: 1 magic every kGyorgWhirlpoolDrainRate frames (running-out is handled in
    // TickSummons, which clears sGyorgWhirlpoolActive when magic hits 0).
    if (++sGyorgWhirlpoolDrain >= kGyorgWhirlpoolDrainRate) {
        sGyorgWhirlpoolDrain = 0;
        if (gSaveContext.magic > 0) {
            gSaveContext.magic--;
        }
    }

    if (!sGyorgWhirlpoolColReady) {
        Collider_InitCylinder(play, &sGyorgWhirlpoolCollider);
        Collider_SetCylinder(play, &sGyorgWhirlpoolCollider, &player->actor, &sGyorgWhirlpoolColliderInit);
        sGyorgWhirlpoolColReady = true;
    }
    // Keep the AT cylinder live at the aim point. RE-ARM it every frame (set AT_ON, clear the sticky
    // AT_HIT): once an AT lands a hit the engine latches AT_HIT and it stops connecting, so without
    // this the vortex only ever damaged once instead of ticking — the Gust Jar does the same.
    sGyorgWhirlpoolCollider.dim.pos.x = (s16)sGyorgWhirlpoolPos.x;
    sGyorgWhirlpoolCollider.dim.pos.y = (s16)sGyorgWhirlpoolPos.y;
    sGyorgWhirlpoolCollider.dim.pos.z = (s16)sGyorgWhirlpoolPos.z;
    sGyorgWhirlpoolCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER;
    sGyorgWhirlpoolCollider.base.atFlags &= ~AT_HIT;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sGyorgWhirlpoolCollider.base);

    // The damage sphere on LINK: whatever the vortex drags in keeps getting hit and can't reach him.
    // Live every frame, so contact damage repeats as fast as each enemy's own i-frames allow.
    if (!sGyorgAuraColReady) {
        Collider_InitCylinder(play, &sGyorgAuraCollider);
        Collider_SetCylinder(play, &sGyorgAuraCollider, &player->actor, &sGyorgAuraColliderInit);
        sGyorgAuraColReady = true;
    }
    Collider_UpdateCylinder(&player->actor, &sGyorgAuraCollider);
    sGyorgAuraCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER;
    sGyorgAuraCollider.base.atFlags &= ~AT_HIT; // re-arm so the ring keeps ticking, not one-and-done
    CollisionCheck_SetAT(play, &play->colChkCtx, &sGyorgAuraCollider.base);

    if ((play->gameplayFrames & 7) == 0) {
        Player_PlaySfx(&player->actor, NA_SE_EV_DIVE_INTO_WATER); // OoT-native
    }

    // Gust-jar-style trap on every enemy in range: PULL toward the vortex + PARALYZE + (via the AT
    // cylinder above) remote damage.
    //  - Paralyze = PULSED freezeTimer (frozen 14 of every 15 frames): a frozen actor skips its
    //    update, which is exactly what lets the pull win against enemies that pin/reset their own
    //    position every frame — but a frozen actor also never re-registers its AC collider, so a
    //    permanent freeze would make it UNDAMAGEABLE. The free frame per 15 lets it register
    //    colliders (vortex AT hits land) without giving it real control back.
    //  - The pull is a position nudge (the Gust Jar idiom): "try if possible" — anything the engine
    //    hard-anchors simply doesn't move, and the remote AT damage still applies.
    for (s32 cat = 0; cat < 2; cat++) {
        Actor* a = play->actorCtx.actorLists[(cat == 0) ? ACTORCAT_ENEMY : ACTORCAT_BOSS].head;
        for (; a != nullptr; a = a->next) {
            if (a->update == nullptr) {
                continue;
            }
            f32 d = Math_Vec3f_DistXZ(&a->world.pos, &sGyorgWhirlpoolPos);
            if ((d < kGyorgWhirlpoolRadius) && (d > 1.0f)) {
                s16 pullYaw = Math_Vec3f_Yaw(&a->world.pos, &sGyorgWhirlpoolPos); // toward the vortex
                a->world.pos.x += Math_SinS(pullYaw) * kGyorgWhirlpoolPull;
                a->world.pos.z += Math_CosS(pullYaw) * kGyorgWhirlpoolPull;
                // Y too, so fliers get dragged down/up INTO the vortex, not just sideways.
                Math_ApproachF(&a->world.pos.y, sGyorgWhirlpoolPos.y, 0.3f, kGyorgWhirlpoolPull * 0.6f);
                // Near-continuous paralyze (frozen 14 of every 15 frames) + blue tint. freezeTimer=2 →
                // DECR leaves 1 → frozen this frame. OoT colorFlag 0 = blue (see cane_pacci.c).
                if ((play->gameplayFrames % 15) != 0) {
                    a->freezeTimer = 2;
                }
                Actor_SetColorFilter(a, 0, 180, 0, 10);
            }
        }
    }
}

// The MM look was Gyorg's REAL water funnel (EnWaterEffect actors respawned each ~14 frames in the
// tick above) — absent in OoT (see the #if 0 note). Kept as a no-op so the z_player.c draw hook /
// header ABI stays stable.
extern "C" void BossRemains_DrawGyorgWhirlpool(Player* player, PlayState* play) {
    (void)player;
    (void)play;
}

static void BossRemains_TickSummons(PlayState* play, Player* player) {
    s32 idx = BossRemains_ItemIndex(sWornRemains);
    if (idx < 0) {
        return;
    }

    Input* in = &play->state.input[0];
    s16 yaw = player->actor.shape.rot.y;

    switch (idx) {
        case 0: // Odolwa — friendly beetles on SHIELD (R) + B. Costs magic (like the sword-moth beam).
            if (CHECK_BTN_ALL(in->cur.button, BTN_R) && CHECK_BTN_ALL(in->press.button, BTN_B) &&
                gSaveContext.isMagicAcquired && (gSaveContext.magic >= 6)) {
                BossRemains_EnsureActorsRegistered();
                // Deduct magic DIRECTLY, not via the Magic_RequestChange state machine: it collides
                // with the spell system (blocks casting) and only consumes when the magic state is
                // idle. We already gated on magic >= 6 above, so this can't go negative.
                gSaveContext.magic -= 6;
                for (s32 i = 0; i < 6; i++) {
                    Vec3f p = player->actor.world.pos;
                    p.x += Rand_CenteredFloat(70.0f);
                    p.y += 15.0f + Rand_ZeroFloat(30.0f); // chest height so they visibly pop out
                    p.z += Rand_CenteredFloat(70.0f);
                    RemainsAllyBug_Spawn(play, &p, (s16)(s32)Rand_CenteredFloat(60000.0f));
                }
                // Odolwa's chant (VOICE1 for the beetle summon) + his arm-swing dance, forced
                // spell-style. The chant is played once at dance start and MmSfx_Stop'd at every
                // dance-end/exit path (the bridge has no MM flagged-continuous model).
                sOdolwaSummonChant = NA_SE_EN_MIBOSS_VOICE1_OLD;
                BossRemains_OdolwaSummonDance(play, player, ODOLWA_ANIM_SWING_DANCE);
            }
            break;
        case 1: // Goht — SHIELD(R)+B = friendly bombchu; B (no R) held = charged thunder bolt (magic)
            BossRemains_EnsureActorsRegistered();
            // R+B is the bombchu toggle: throw ONE (only if none of ours is out), then press R+B AGAIN
            // while it's alive to detonate it manually right where it is.
            if (CHECK_BTN_ALL(in->cur.button, BTN_R) && CHECK_BTN_ALL(in->press.button, BTN_B)) {
                if (RemainsAllyChu_IsAlive()) {
                    RemainsAllyChu_DetonateActive(play);
                } else {
                    Vec3f p = player->actor.world.pos;
                    p.x += Math_SinS(yaw) * 30.0f;
                    p.z += Math_CosS(yaw) * 30.0f;
                    RemainsAllyChu_Spawn(play, &p, yaw);
                    MmSfx_PlayAtPos(NA_SE_EN_BOMCHU_AIM, &player->actor.projectedPos);
                }
            }
            // B (no R) = CHARGED thunder (like the real Goht). HOLD B to charge — a Goht light-orb
            // grows in front of Link; the longer you charge the FARTHER and HARDER the bolt
            // (anti-spam). Release to fire.
            {
                bool canCast = gSaveContext.isMagicAcquired && (gSaveContext.magic >= kGohtThunderMagicCost);
                bool holding = CHECK_BTN_ALL(in->cur.button, BTN_B) && !CHECK_BTN_ALL(in->cur.button, BTN_R);

                if (holding && canCast) {
                    if (sGohtThunderCharge < kGohtThunderChargeMax) {
                        sGohtThunderCharge++;
                    }
                    sGohtThunderCharging = true;
                    // Goht's charging thunder crackle. MM sustained it flagged every frame; the MmSfx
                    // bridge has no flagged model, so re-fire the one-shot on a cadence instead.
                    if ((play->gameplayFrames & 7) == 0) {
                        MmSfx_PlayAtPos(NA_SE_EN_COMMON_THUNDER, &player->actor.projectedPos);
                    }
                } else {
                    // Released (or ran out of magic) → fire if we charged enough and can still pay.
                    if (sGohtThunderCharging && (sGohtThunderCharge >= kGohtThunderChargeMin) && canCast) {
                        // Direct deduction (see the R+B beetle note). Gated on magic >= cost.
                        gSaveContext.magic -= kGohtThunderMagicCost;

                        f32 t = (f32)sGohtThunderCharge / (f32)kGohtThunderChargeMax; // 0..1
                        s16 ttl = kGohtThunderTtlMin + (s16)(t * (kGohtThunderTtlMax - kGohtThunderTtlMin));
                        s16 dmg = kGohtThunderDmgMin + (s16)(t * (kGohtThunderDmgMax - kGohtThunderDmgMin));

                        // Aim at the enemy most in front of Link (if any); else straight ahead.
                        s16 boltYaw = yaw;
                        Actor* front = BossRemains_FindFrontEnemy(play, player, 900.0f, 0x5000);
                        if (front != NULL) {
                            boltYaw = Actor_WorldYawTowardActor(&player->actor, front);
                        }
                        // Fire from the raised shield (just ahead of Link, chest height).
                        Vec3f p = player->actor.world.pos;
                        p.x += Math_SinS(yaw) * 22.0f;
                        p.z += Math_CosS(yaw) * 22.0f;
                        p.y += 40.0f;
                        RemainsAllyBug_SpawnThunderCharged(play, &p, boltYaw, ttl, dmg);
                        // Goht's real thunder-shoot sfx.
                        MmSfx_PlayAtPos(NA_SE_EN_COMMON_THUNDER_THR, &player->actor.projectedPos);
                    }
                    sGohtThunderCharge = 0;
                    sGohtThunderCharging = false;
                }
            }
            break;

        case 2: // Gyorg — while SWIMMING, R alone summons Gyorg's fish and B held holds the whirlpool;
                // ON LAND, R + B summons fish. (The Zora-style swim itself is hooked player-side.)
            if (player->actor.yDistToWater > 0.0f) {
                // Swimming: R = fish; B HELD = the whirlpool (drains magic while up).
                if (CHECK_BTN_ALL(in->press.button, BTN_R)) {
                    BossRemains_GyorgSummonFish(play, player);
                }
                sGyorgWhirlpoolActive = CHECK_BTN_ALL(in->cur.button, BTN_B) && (gSaveContext.magic > 0);
            } else {
                sGyorgWhirlpoolActive = false;
                // On land: R + B = fish.
                if (CHECK_BTN_ALL(in->cur.button, BTN_R) && CHECK_BTN_ALL(in->press.button, BTN_B)) {
                    BossRemains_GyorgSummonFish(play, player);
                }
            }
            break;

        case 3: // Twinmold — a DARK LINK companion walks with you for as long as the remains is worn.
#if 0           // TODO: Dark Link companion ported separately — remains_ally_link is NOT ported yet.
            if (!RemainsAllyLink_IsAlive()) {
                Vec3f p = player->actor.world.pos;
                p.x += Math_SinS(yaw) * -60.0f; // step in just behind Link
                p.z += Math_CosS(yaw) * -60.0f;
                RemainsAllyLink_Spawn(play, &p, yaw);
            }
#endif
            break;

        default:
            break;
    }
}

// Magic drained per sword-swing moth projectile.
static constexpr s16 kOdolwaMothMagicCost = 2;

// Sword swing with magic → a moth projectile flies at the LOCK-ON target. Only fires while Z-targeting
// (an enemy is locked on) and always aims at that target. Called from the z_player.c melee setup.
extern "C" void BossRemains_OdolwaSwordMoth(PlayState* play, Player* player) {
    if (!BossRemains_IsOdolwaWorn() || play == nullptr || player == nullptr) {
        return;
    }
    // ONLY while Z-targeting — no lock-on target, no moth.
    if (player->focusActor == nullptr) {
        return;
    }
    // SHIELD (R) held = the R+B bug summon, not a normal swing — no beam / no magic drain then.
    if (CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_R)) {
        return;
    }
    if (!gSaveContext.isMagicAcquired || (gSaveContext.magic < kOdolwaMothMagicCost)) {
        return;
    }
    // Direct deduction (see the R+B beetle note). Gated on magic >= cost above, so this stays >= 0.
    gSaveContext.magic -= kOdolwaMothMagicCost;

    BossRemains_EnsureActorsRegistered();
    Vec3f pos = player->bodyPartsPos[PLAYER_BODYPART_WAIST];
    s16 yaw = Actor_WorldYawTowardActor(&player->actor, player->focusActor); // straight at the target
    RemainsAllyBug_SpawnProjectile(play, &pos, yaw);
    MmSfx_PlayAtPos(NA_SE_EN_MB_MOTH_FLY, &player->actor.projectedPos);
}

// Odolwa's shield deflects an attack → a defensive fire burst at Link (the SW97 Fire-Medallion
// ground-fire wave), with a cooldown so a sustained block doesn't spam it.
static void BossRemains_OdolwaShieldFire(PlayState* play, Player* player) {
    if (sOdolwaShieldFireTimer > 0) {
        return;
    }
    // SoH registers the SW97 magic-fire actor dynamically; -1 until the sw97 expansion inits.
    if (gSw97ActorId_MagicFire < 0) {
        return;
    }
    sOdolwaShieldFireTimer = 30;
    Vec3f p = player->actor.world.pos;
    Actor_Spawn(&play->actorCtx, play, gSw97ActorId_MagicFire, p.x, p.y, p.z, 0, player->actor.shape.rot.y, 0, 0);
}

// ============================================================================
// GOHT — bull charge (A held), quake pound (R+A), thunder (B), one friendly bombchu (R+B)
// ============================================================================

extern "C" s32 BossRemains_IsGohtWorn(void) {
    return (RemainsEnabled() && (sWornRemains == ITEM_MM_REMAINS_GOHT)) ? 1 : 0;
}

// True while the bull charge is running. Read by z_player.c's walk-off handler, which bails out for a
// charging Goht exactly like MM's does for the Goron roll — that early-out is what stops the fall
// action / auto-hop / ledge-grab from eating the launch, so leaving a ledge or ramp at charge speed
// sends Link flying instead of just dropping him back into a normal run.
extern "C" s32 BossRemains_IsGohtCharging(void) {
    return sGohtCharging ? 1 : 0;
}

// True whenever Gyorg's remains is worn — drives human Link's Zora-style free 3D dive/swim, current
// immunity, "can't walk in water", and damage resilience (all gated in z_player.c on THIS + being in
// water). The in-water test lives at each z_player.c call site (yDistToWater).
extern "C" s32 BossRemains_IsGyorgWorn(void) {
    return (RemainsEnabled() && (sWornRemains == ITEM_MM_REMAINS_GYORG)) ? 1 : 0;
}

// Roll suppression: both Odolwa (runs instead) and Goht (A = bull charge) take over A.
extern "C" s32 BossRemains_SuppressRoll(void) {
    return BossRemains_IsOdolwaWorn() || BossRemains_IsGohtWorn();
}

// Radial quake-pound damage: a fat AT cylinder centered on Link for a few frames after landing.
// OoT has no DMG_GORON_POUND — DMG_HAMMER_SWING is the closest ground-shock hit class.
static ColliderCylinderInit sGohtQuakeColliderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { DMG_HAMMER_SWING, 0x00, 0x04 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { 110, 60, -20, { 0, 0, 0 } },
};

// Light bull-charge contact damage: a small AT cylinder on Link while charging. Goron-punch hit class
// in MM (DMG_HAMMER_SWING here), but only 1 damage.
static ColliderCylinderInit sGohtChargeColliderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { DMG_HAMMER_SWING, 0x00, 0x01 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { 45, 60, 0, { 0, 0, 0 } },
};

// The enemy most IN FRONT of Link: smallest |yaw difference| to Link's facing within range/cone.
// Returns NULL if none qualifies.
static Actor* BossRemains_FindFrontEnemy(PlayState* play, Player* player, f32 maxDist, s16 cone) {
    Actor* best = NULL;
    s16 bestAbs = cone;
    for (s32 cat = 0; cat < 2; cat++) {
        Actor* a = play->actorCtx.actorLists[(cat == 0) ? ACTORCAT_ENEMY : ACTORCAT_BOSS].head;
        for (; a != NULL; a = a->next) {
            if ((a->update == NULL) || (Actor_WorldDistXZToActor(&player->actor, a) > maxDist)) {
                continue;
            }
            s16 diff = Actor_WorldYawTowardActor(&player->actor, a) - player->actor.shape.rot.y;
            s16 absDiff = (diff < 0) ? -diff : diff;
            if (absDiff < bestAbs) {
                bestAbs = absDiff;
                best = a;
            }
        }
    }
    return best;
}

// The bull QUAKE POUND: jump-attack anim + hop; landing handled in GohtPostAction.
// Returns true if it took over. While Goht is worn the sword is unequipped, so this is triggered
// directly from R+A in BossRemains_GohtPostAction (not the melee path).
extern "C" s32 BossRemains_GohtQuakeStart(PlayState* play, Player* player) {
    if (!BossRemains_IsGohtWorn() || (play == nullptr) || (player == nullptr)) {
        return false;
    }
    if (sGohtQuakeState != 0) {
        return true; // already pounding — don't restack
    }
    // High, snappy hop. We DON'T play the anim now — native jump physics kick in for a few frames,
    // then the jump anim plays a bit after leaving the ground (sGohtQuakeAnimDelay, handled below).
    player->actor.velocity.y = kGohtJumpVel;
    player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    sGohtQuakeState = 1;
    sGohtQuakeAnimDelay = kGohtJumpAnimDelay;
    MmSfx_PlayAtPos(NA_SE_EN_MIBOSS_JUMP1, &player->actor.projectedPos);
    return true;
}

// Per-frame Goht driver, called from z_player.c AFTER the player's action func so our speed/anim
// overrides win over locomotion. Owns: bull charge, quake landing.
extern "C" void BossRemains_GohtPostAction(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr) {
        return;
    }

    if (!BossRemains_IsGohtWorn()) {
        sGohtCharging = false;
        sGohtQuakeState = 0;
        // Never leave our no-snap flag behind (it would make Link float over every ledge). Only clear
        // the copy WE set — a real Goron-roll-style owner of this bit must keep its own.
        if (sGohtNoSnapOwned) {
            player->actor.bgCheckFlags &= ~kBgCheckFlagPlayer800;
            sGohtNoSnapOwned = false;
        }
        return;
    }

    if (sGohtChargeCooldown > 0) {
        sGohtChargeCooldown--;
    }

    Input* in = &play->state.input[0];

    // ── GORON QUAKE POUND on SHIELD(R)+A ──────────────────────────────────────
    // The sword is UNEQUIPPED while Goht is worn, so none of this goes through the melee path.
    if (CHECK_BTN_ALL(in->press.button, BTN_A) && CHECK_BTN_ALL(in->cur.button, BTN_R) && (sGohtQuakeState == 0) &&
        (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && !(player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) &&
        (play->msgCtx.msgMode == MSGMODE_NONE)) {
        BossRemains_GohtQuakeStart(play, player);
    }

    // ── BULL CHARGE: while HOLDING A (no R), not mid-pound ────────────────────
    // Like the Goron roll, a charge can only START on the ground but KEEPS GOING through the air, so a
    // ledge/ramp launch stays a charge (bull anim + cone + full speed) and Link sails a long way.
    {
        bool wasCharging = sGohtCharging;
        // NOTE: no magic gate here — like the Pegasus dash, the charge itself always runs; magic only
        // powers the contact damage + the cone (see the drain below).
        bool wantCharge = CHECK_BTN_ALL(in->cur.button, BTN_A) && !CHECK_BTN_ALL(in->cur.button, BTN_R) &&
                          (sGohtQuakeState == 0) && (sGohtChargeCooldown == 0) &&
                          !(player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) &&
                          (play->msgCtx.msgMode == MSGMODE_NONE);
        bool grounded = (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
        sGohtCharging = wantCharge && (grounded || wasCharging);
        if (!sGohtCharging) {
            sGohtChargeMagicTick = 0; // Pegasus_Stop resets its tick the same way
        }
    }

    // GORON-ROLL TERRAIN HANDLING: while charging, stop Link from snapping down onto the floor. This
    // is the raw 0x800 player bgCheckFlag (MM's BGCHECKFLAG_PLAYER_800 — no SoH macro): z_actor.c's
    // floor check only hugs small drops when it's clear, so with it set ledges and slopes are IGNORED
    // and a ramp launches Link into a long flight instead of gluing him to the terrain. Cleared the
    // moment the charge ends so normal ground-hugging comes right back. We only ever clear the flag
    // when WE set it (sGohtNoSnapOwned).
    if (sGohtCharging) {
        player->actor.bgCheckFlags |= kBgCheckFlagPlayer800;
        sGohtNoSnapOwned = true;
    } else if (sGohtNoSnapOwned) {
        player->actor.bgCheckFlags &= ~kBgCheckFlagPlayer800;
        sGohtNoSnapOwned = false;
    }

    if (sGohtCharging) {
        // 3x forward speed — override whatever the action set. On the GROUND, split that speed along
        // the floor pitch exactly like the Goron roll: linearVelocity = speed·cos(pitch),
        // velocity.y = speed·sin(pitch). Running UP a ramp therefore BANKS real upward velocity.y, and
        // the instant Link leaves the ledge that Y momentum carries him into a long arc. Airborne we
        // keep the horizontal speed and leave velocity.y alone so gravity + the banked launch play out.
        if (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
            player->linearVelocity = kGohtChargeSpeed * Math_CosS(player->floorPitch);
            player->actor.velocity.y = kGohtChargeSpeed * Math_SinS(player->floorPitch);
        } else {
            player->linearVelocity = kGohtChargeSpeed;
        }

        // STIFF steering, like a Goron roll but harder to turn: the locomotion func snapped the yaw to
        // the stick this frame; we overwrite it and instead crawl toward the stick's intended
        // direction at a small fixed rate. Facing barely moves per frame, so hard turns become arcs.
        f32 speedTarget;
        s16 yawTarget;
        // SPEED_MODE_CURVED (0.018f) — the macro is local to z_player.c, so inline its value here.
        if (Player_GetMovementSpeedAndYaw(player, &speedTarget, &yawTarget, 0.018f, play)) {
            Math_ScaledStepToS(&player->actor.world.rot.y, yawTarget, kGohtTurnStep);
        }
        player->actor.shape.rot.y = player->actor.world.rot.y;
        player->yaw = player->actor.world.rot.y;

        // Bull run anim: we OWN the pose so the locomotion func can't steal it on turns. Set the exact
        // cycle frame ourselves each frame (morph 0) and advance our own accumulator. This is MM's REAL
        // cl_nigeru pulled from mm.o2r; if the archive isn't mounted yet we fall back to OoT's run so
        // the charge still animates instead of freezing.
        LinkAnimationHeader* chargeAnim = GohtAnim(GOHT_ANIM_CHARGE);
        if (chargeAnim == nullptr) {
            chargeAnim = (LinkAnimationHeader*)gPlayerAnim_link_normal_run;
        }
        f32 runLast = Animation_GetLastFrame((void*)chargeAnim);
        if (runLast <= 0.0f) {
            runLast = 8.0f;
        }
        sGohtChargeAnimFrame += kGohtChargeAnimSpeed;
        if (sGohtChargeAnimFrame >= runLast) {
            sGohtChargeAnimFrame -= runLast;
        }
        LinkAnimation_Change(play, &player->skelAnime, chargeAnim, 1.0f, sGohtChargeAnimFrame, runLast, ANIMMODE_ONCE,
                             0.0f);

        // Hoof stomps — Goht's own heavy footstep.
        if (--sGohtSfxTimer <= 0) {
            MmSfx_PlayAtPos(NA_SE_EN_ICEB_FOOTSTEP_OLD, &player->actor.projectedPos);
            sGohtSfxTimer = 6;
        }

        // MAGIC-POWERED PART (1:1 with Pegasus_StateRunning): only WITH magic does the charge get its
        // light contact damage, and only then does it drain. Out of magic the charge keeps running at
        // full speed — it just stops hurting things (and the cone stops drawing).
        if (gSaveContext.magic > 0) {
            if (!sGohtChargeColReady) {
                Collider_InitCylinder(play, &sGohtChargeCollider);
                Collider_SetCylinder(play, &sGohtChargeCollider, &player->actor, &sGohtChargeColliderInit);
                sGohtChargeColReady = true;
            }
            Collider_UpdateCylinder(&player->actor, &sGohtChargeCollider);
            CollisionCheck_SetAT(play, &play->colChkCtx, &sGohtChargeCollider.base);

            sGohtChargeMagicTick++;
            if (sGohtChargeMagicTick >= kGohtChargeMagicInterval) {
                sGohtChargeMagicTick = 0;
                gSaveContext.magic--;
                if (gSaveContext.magic < 0) {
                    gSaveContext.magic = 0;
                }
            }
        } else {
            sGohtChargeMagicTick = 0;
        }

        // CRASH: hitting a wall detonates a KEG-CLASS blast at the impact point — an En_Bom set to
        // blow NOW plus the power-keg obstacle blast (PowerKeg_SetBlast), so it breaks every
        // bombable/keg-breakable thing (that's the "bonk to break stuff"). Link takes NO damage from
        // his own blast (brief intangibility) and BOUNCES back off the wall like a real bonk recoil.
        if (player->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
            Vec3f p = player->actor.world.pos;
            p.x += Math_SinS(player->actor.shape.rot.y) * 30.0f;
            p.z += Math_CosS(player->actor.shape.rot.y) * 30.0f;
            p.y += 20.0f;
            EnBom* keg = (EnBom*)Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOM, p.x, p.y, p.z, 0,
                                             player->actor.shape.rot.y, 0, 0);
            if (keg != NULL) {
                keg->timer = 0; // blow NOW
            }
            // The keg-class obstacle blast (boulders / heavy blocks in radius) — the SoH power keg port.
            PowerKeg_SetBlast(&p, 350.0f, (s32)play->gameplayFrames);
            // Intangible through the blast (invincibilityTimer != 0 skips damage; negative = no flash).
            player->invincibilityTimer = -40;
            // Bonk recoil: shoot backwards off the wall with a little hop.
            player->linearVelocity = kGohtBounceSpeed;
            player->actor.velocity.y = kGohtBounceHop;
            player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
            Player_PlaySfx(&player->actor, NA_SE_PL_BODY_BOUND); // bonk thud (OoT-native)
            sGohtCharging = false;
            sGohtChargeCooldown = 25;
        }
    }

    // ── QUAKE POUND: airborne → landing after the high hop ────────────────────
    // MM's REAL pound anims (pz_jumpAT / pz_jumpATend) loaded from mm.o2r; OoT's jump-slash pair is
    // only the fallback for when the archive isn't mounted.
    if (sGohtQuakeState == 1) {
        LinkAnimationHeader* jumpAnim = GohtAnim(GOHT_ANIM_JUMP);
        if (jumpAnim == nullptr) {
            jumpAnim = (LinkAnimationHeader*)gPlayerAnim_link_fighter_jump_kiru;
        }
        // End on the LAST REAL frame: one past it renders the bind/rest pose (the bug MM hit).
        f32 jumpLast = Animation_GetLastFrame((void*)jumpAnim) - 1.0f;
        if (jumpLast < 0.0f) {
            jumpLast = 0.0f;
        }
        // Play the jump anim a few frames AFTER leaving the ground (native jump physics show first).
        if (sGohtQuakeAnimDelay > 0) {
            sGohtQuakeAnimDelay--;
            if (sGohtQuakeAnimDelay == 0) {
                LinkAnimation_Change(play, &player->skelAnime, jumpAnim, 3.0f, 0.0f, jumpLast, ANIMMODE_ONCE, -3.0f);
                sGohtJumpPlayTimer = 5; // let it play, then hold on the last frame
            }
        } else if (sGohtJumpPlayTimer > 0) {
            sGohtJumpPlayTimer--; // let the jump anim play through
        } else {
            // Jump anim finished: HOLD its last frame for the rest of the airtime, so the player's
            // action func can't drop us into another pose. Re-asserted every frame (morph 0) to hold.
            LinkAnimation_Change(play, &player->skelAnime, jumpAnim, 1.0f, jumpLast, jumpLast, ANIMMODE_LOOP, 0.0f);
        }
        // Fast, snappy descent: pile on extra downward accel once past the apex.
        if (player->actor.velocity.y <= 0.0f) {
            player->actor.velocity.y -= kGohtJumpGravityBoost;
        }
        if ((player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && (player->actor.velocity.y <= 0.0f)) {
            Actor_RequestQuake(play, 10, 16);
            MmSfx_PlayAtPos(NA_SE_EN_ICEB_FOOTSTEP_OLD, &player->actor.projectedPos); // heavy stomp
            // Landing anim: MM's pz_jumpATend from mm.o2r (OoT jump-slash finish as fallback), also
            // ending one frame short of the tail so it can't land on the bind pose.
            LinkAnimationHeader* endAnim = GohtAnim(GOHT_ANIM_JUMP_END);
            if (endAnim == nullptr) {
                endAnim = (LinkAnimationHeader*)gPlayerAnim_link_fighter_jump_kiru_finsh;
            }
            f32 endLast = Animation_GetLastFrame((void*)endAnim) - 1.0f;
            if (endLast < 0.0f) {
                endLast = 0.0f;
            }
            LinkAnimation_Change(play, &player->skelAnime, endAnim, 3.0f, 0.0f, endLast, ANIMMODE_ONCE, -2.0f);
            sGohtQuakeState = 2;
            sGohtQuakeDmgFrames = 8;
        }
    } else if (sGohtQuakeState == 2) {
        if (sGohtQuakeDmgFrames > 0) {
            sGohtQuakeDmgFrames--;
            // Lazy one-time collider init (needs a live PlayState).
            if (!sGohtQuakeColReady) {
                Collider_InitCylinder(play, &sGohtQuakeCollider);
                Collider_SetCylinder(play, &sGohtQuakeCollider, &player->actor, &sGohtQuakeColliderInit);
                sGohtQuakeColReady = true;
            }
            Collider_UpdateCylinder(&player->actor, &sGohtQuakeCollider);
            CollisionCheck_SetAT(play, &play->colChkCtx, &sGohtQuakeCollider.base);
        } else {
            sGohtQuakeState = 0;
            sGohtRecoverFrames = 8; // kill the lingering attack pose next
        }
    }

    // RECOVERY: the jump-slash pair ends in an attack pose; force the normal idle for a few frames so
    // the pose resets cleanly.
    if ((sGohtRecoverFrames > 0) && !sGohtCharging && (sGohtQuakeState == 0)) {
        sGohtRecoverFrames--;
        f32 waitLast = Animation_GetLastFrame((void*)gPlayerAnim_link_normal_waitR_free);
        LinkAnimation_Change(play, &player->skelAnime, (LinkAnimationHeader*)gPlayerAnim_link_normal_waitR_free, 1.0f,
                             0.0f, waitLast, ANIMMODE_LOOP, -4.0f);
    }
}

// ── Majora-red bull-charge cone (pegasus cone geometry, retinted + Majora chant texture) ──────
static Vtx sGohtConeVtx[] = {
    { { { 0, 0, 0 }, 0, { 512, 2048 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },  // tip (front)
    { { { 4000, 8000, 0 }, 0, { 0, 0 }, { 0xFF, 0xFF, 0xFF, 0x00 } } }, // base ring
    { { { 2828, 8000, 2828 }, 0, { 256, 0 }, { 0xFF, 0xFF, 0xFF, 0x00 } } },
    { { { 0, 8000, 4000 }, 0, { 512, 0 }, { 0xFF, 0xFF, 0xFF, 0x00 } } },
    { { { -2828, 8000, 2828 }, 0, { 768, 0 }, { 0xFF, 0xFF, 0xFF, 0x00 } } },
    { { { -4000, 8000, 0 }, 0, { 1024, 0 }, { 0xFF, 0xFF, 0xFF, 0x00 } } },
    { { { -2828, 8000, -2828 }, 0, { 1280, 0 }, { 0xFF, 0xFF, 0xFF, 0x00 } } },
    { { { 0, 8000, -4000 }, 0, { 1536, 0 }, { 0xFF, 0xFF, 0xFF, 0x00 } } },
    { { { 2828, 8000, -2828 }, 0, { 1792, 0 }, { 0xFF, 0xFF, 0xFF, 0x00 } } },
};

// Same combiner/geometry as the pegasus cone (equip_pegasus.c — the ORIGINAL source of this idiom)
// but with MAJORA's reddish chant colors baked in.
static Gfx sGohtConeDL[] = {
    gsDPSetCombineLERP(TEXEL1, PRIMITIVE, PRIM_LOD_FRAC, TEXEL0, TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0, PRIMITIVE,
                       ENVIRONMENT, COMBINED, ENVIRONMENT, COMBINED, 0, SHADE, 0),
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_XLU_SURF2),
    gsSPClearGeometryMode(G_CULL_BACK | G_FOG | G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR),
    gsDPSetPrimColor(0, 0x80, 255, 90, 60, 255), // Majora chant red-orange
    gsDPSetEnvColor(130, 10, 10, 0),             // deep red
    gsSPDisplayList(0x08000001),                 // segment 0x08: animated tex scroll (set per frame)
    gsSPVertex(sGohtConeVtx, 9, 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsSP2Triangles(0, 3, 4, 0, 0, 4, 5, 0),
    gsSP2Triangles(0, 5, 6, 0, 0, 6, 7, 0),
    gsSP2Triangles(0, 7, 8, 0, 0, 8, 1, 0),
    gsSPEndDisplayList(),
};

// Draw the charge cone around Link (called from the player draw, next to the Odolwa trail).
extern "C" void BossRemains_DrawGohtCone(Player* player, PlayState* play) {
    if (!sGohtCharging || (play == nullptr) || (player == nullptr)) {
        return;
    }
    // Magic powers the cone (Pegasus_Draw bails the same way at magic <= 0): out of magic the bull
    // still charges, it just loses the aura.
    if (gSaveContext.magic <= 0) {
        return;
    }
    // Majora's chant streak texture (i8 32x64, object_stk2) from mm.o2r — real pointer (mm.o2r isn't
    // indexed in SoH); NULL means the archive isn't mounted yet → skip this frame, retry next.
    static void* sStk2Tex = nullptr;
    if (MmRes("objects/object_stk2/object_stk2_Tex_008B50", &sStk2Tex) == nullptr) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    // Placement baked from the editor tuning: 59 forward, 20 up (head height), tip pitched forward.
    Matrix_Push();
    f32 sinY = Math_SinS(player->actor.shape.rot.y);
    f32 cosY = Math_CosS(player->actor.shape.rot.y);
    Matrix_Translate(player->actor.world.pos.x + sinY * 59.0f, player->actor.world.pos.y + 20.0f,
                     player->actor.world.pos.z + cosY * 59.0f, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(player->actor.shape.rot.y), MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD((s16)-0x4000), MTXMODE_APPLY); // tip forward
    Matrix_Scale(0.015f, 0.015f, 0.015f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    // Runtime texture load (can't live in the static DL — SoH interprets raw DL pointers as OTR
    // paths; equip_pegasus.c does the same).
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetTextureLUT(POLY_XLU_DISP++, G_TT_NONE);
    gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPLoadTextureBlock(POLY_XLU_DISP++, sStk2Tex, G_IM_FMT_I, G_IM_SIZ_8b, 32, 64, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, 5, 6, G_TX_NOLOD, G_TX_NOLOD);
    gDPLoadMultiBlock(POLY_XLU_DISP++, sStk2Tex, 0x0100, 1, G_IM_FMT_I, G_IM_SIZ_8b, 32, 64, 0,
                      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 5, 6, 14, 14);

    u32 frames = play->gameplayFrames;
    // (uintptr_t) cast: Gfx_TwoTexScroll returns Gfx*, and this C++ TU has no implicit
    // pointer→integer conversion for gSPSegment's uintptr_t arg (same cast the seg-0xC noop uses).
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, -(s32)(frames * 1), (s32)(frames * 20), 0x20, 0x40, 1,
                                           -(s32)(frames * 2), (s32)(frames * 10), 0x20, 0x40));

    gSPDisplayList(POLY_XLU_DISP++, sGohtConeDL);

    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

// Charging-thunder VFX at Link's shield (Goht's real light-orb + crossed lightning bolts, from
// BossHakugin_DrawChargingLightning; DLs pulled from mm.o2r). Grows as the B charge builds. Call from
// the player draw.
extern "C" void BossRemains_DrawGohtChargingThunder(Player* player, PlayState* play) {
    if (!sGohtThunderCharging || (sGohtThunderCharge <= 0) || (play == nullptr) || (player == nullptr)) {
        return;
    }
    static void* sLightningMatDL = nullptr;
    static void* sLightningModelDL = nullptr;
    static void* sOrbMatDL = nullptr;
    static void* sOrbModelDL = nullptr;
    if (MmRes("objects/object_boss_hakugin/gGohtLightningMaterialDL", &sLightningMatDL) == nullptr ||
        MmRes("objects/object_boss_hakugin/gGohtLightningModelDL", &sLightningModelDL) == nullptr ||
        MmRes("objects/object_boss_hakugin/gGohtLightOrbMaterialDL", &sOrbMatDL) == nullptr ||
        MmRes("objects/object_boss_hakugin/gGohtLightOrbModelDL", &sOrbModelDL) == nullptr) {
        return; // mm.o2r not mounted yet — retry next frame
    }

    f32 t = (f32)sGohtThunderCharge / (f32)kGohtThunderChargeMax; // 0..1
    s16 yaw = player->actor.shape.rot.y;
    Vec3f pos;
    pos.x = player->actor.world.pos.x + Math_SinS(yaw) * 22.0f;
    pos.y = player->actor.world.pos.y + 40.0f;
    pos.z = player->actor.world.pos.z + Math_CosS(yaw) * 22.0f;
    s16 spin = (s16)(play->gameplayFrames * 0x1000);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    // Two crossed lightning models rotating around the center (mirrors the real charging effect).
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 255, 255, 0); // sLightningColor
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sLightningMatDL);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
    for (s32 i = -1; i <= 1; i += 2) {
        Vec3s rot = { 0, yaw, 0 };
        Matrix_SetTranslateRotateYXZ(pos.x, pos.y, pos.z, &rot);
        Matrix_RotateY(BINANG_TO_RAD((s16)(0x1400 * i)), MTXMODE_APPLY);
        Matrix_RotateX(BINANG_TO_RAD((s16)(0xC00 * i)), MTXMODE_APPLY);
        Matrix_RotateZ(BINANG_TO_RAD(spin), MTXMODE_APPLY);
        Matrix_Scale(0.15f, 0.15f, 0.45f * t, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sLightningModelDL);
        Matrix_RotateZ(BINANG_TO_RAD((s16)0x4000), MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sLightningModelDL);
    }

    // Growing light orb at the center (billboarded).
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetEnvColor(POLY_XLU_DISP++, 180, 255, 255, 0);
    f32 orb = 0.015f + (0.05f * t);
    Matrix_Translate(pos.x, pos.y, pos.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(orb, orb, orb, MTXMODE_APPLY);
    Matrix_RotateZ(BINANG_TO_RAD(spin), MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sOrbMatDL);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sOrbModelDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

// Per-frame upkeep while the Odolwa remains is worn (footsteps + shield-deflect fire). The boosted
// run, the roll suppression, and the flight are wired via the z_player.c hooks; this only owns the
// mask-side effects.
static void BossRemains_TickOdolwa(PlayState* play, Player* player) {
    // The 2x run + purple trail are active only while HOLDING A.
    sOdolwaRunBoost = CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_A);

    // Heavy Odolwa footsteps only during the boosted run.
    if (sOdolwaRunBoost && (player->actor.speedXZ > 1.0f)) {
        if (--sOdolwaRunSfxTimer <= 0) {
            MmSfx_PlayAtPos(NA_SE_EN_MIBOSS_GND1_OLD, &player->actor.projectedPos);
            sOdolwaRunSfxTimer = 8;
        }
    }
    if (sOdolwaShieldFireTimer > 0) {
        sOdolwaShieldFireTimer--;
    }
    if (player->shieldQuad.base.acFlags & AC_BOUNCED) {
        BossRemains_OdolwaShieldFire(play, player);
    }
}

// Kill every lingering Odolwa SFX the instant the mask is doffed (toggled off or covered by a native
// mask). The run footsteps play on a fast cadence and the moth/beetle cues ring out, so without this
// they keep sounding after the mask is gone. MmSfx_Stop is the bridge's global stop (the OoT-side
// replacement for MM's AudioSfx_StopById).
static void BossRemains_StopOdolwaSfx(void) {
    MmSfx_Stop(NA_SE_EN_MIBOSS_GND1_OLD);   // running footsteps cadence
    MmSfx_Stop(NA_SE_EN_MB_MOTH_FLY);       // sword-swing moth beam / flight wing-flap loop
    MmSfx_Stop(NA_SE_EN_MIBOSS_VOICE1_OLD); // beetle-summon chant
    MmSfx_Stop(NA_SE_EN_MIBOSS_VOICE2_OLD); // moth-summon chant
    MmSfx_Stop(NA_SE_EN_MIBOSS_RHYTHM_OLD); // (legacy summon chant)
}

// Goht UNEQUIPS the sword (like MM Goron): while worn we stash the B-button item and blank it, so B
// is free for the thunder and no sword can ever be drawn/swung. Restored on doff / mask-swap. The
// stashed id is also mirrored to a CVar so a save+reload that lands mid-stash can recover it
// (buttonItems persist in the save file). Runs every frame; only acts on the stash/restore edges.
static void BossRemains_SyncGohtSword(PlayState* play, Player* player) {
    (void)player;
    bool gohtNow = (sWornRemains == ITEM_MM_REMAINS_GOHT);

    if (gohtNow && !sGohtSwordStashed) {
        // OoT differs from MM in a load-bearing way: the B button is DERIVED from the sword equipment
        // bits (z_parameter.c re-writes buttonItems[0] from them), so blanking buttonItems[0] alone is
        // undone within a frame — that's why the sword kept coming back. Go fully SWORDLESS instead
        // (the state OoT already supports) by ALSO clearing EQUIP_TYPE_SWORD, and stash both halves.
        s16 cur = gSaveContext.equips.buttonItems[0];
        s16 curEquip = CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
        CVarSetInteger("gBossRemains.GohtStashedSword", cur);
        CVarSetInteger("gBossRemains.GohtStashedSwordEquip", curEquip);
        Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_NONE);
        gSaveContext.equips.buttonItems[0] = ITEM_NONE;
        gSaveContext.buttonStatus[0] = ITEM_NONE; // the slot z_parameter restores B from
        Interface_LoadItemIcon1(play, 0);
        sGohtSwordStashed = true;
    } else if (!gohtNow) {
        s16 stashed = (s16)CVarGetInteger("gBossRemains.GohtStashedSword", ITEM_NONE);
        s16 stashedEquip = (s16)CVarGetInteger("gBossRemains.GohtStashedSwordEquip", EQUIP_VALUE_SWORD_NONE);
        if (sGohtSwordStashed) {
            if (stashedEquip != EQUIP_VALUE_SWORD_NONE) {
                Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, (u16)stashedEquip);
            }
            gSaveContext.equips.buttonItems[0] = (u8)stashed;
            gSaveContext.buttonStatus[0] = (u8)stashed;
            CVarSetInteger("gBossRemains.GohtStashedSword", ITEM_NONE);
            CVarSetInteger("gBossRemains.GohtStashedSwordEquip", EQUIP_VALUE_SWORD_NONE);
            Interface_LoadItemIcon1(play, 0);
            sGohtSwordStashed = false;
        } else if ((stashed != ITEM_NONE) && (gSaveContext.equips.buttonItems[0] == ITEM_NONE)) {
            // Recover a sword lost to a save/reload that happened while Goht was worn.
            if (stashedEquip != EQUIP_VALUE_SWORD_NONE) {
                Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, (u16)stashedEquip);
            }
            gSaveContext.equips.buttonItems[0] = (u8)stashed;
            gSaveContext.buttonStatus[0] = (u8)stashed;
            CVarSetInteger("gBossRemains.GohtStashedSword", ITEM_NONE);
            CVarSetInteger("gBossRemains.GohtStashedSwordEquip", EQUIP_VALUE_SWORD_NONE);
            Interface_LoadItemIcon1(play, 0);
        }
    }
}

extern "C" void BossRemains_TickInput(PlayState* play, Player* player) {
    if (!RemainsEnabled() || play == nullptr || player == nullptr) {
        return;
    }
    // Keep the sword unequip in sync EVERY frame, before any early return below can skip it (so
    // doffing via textbox / mask-swap still restores the blade).
    BossRemains_SyncGohtSword(play, player);

    // (Twinmold's Dark Link companion despawn lived here in MM — companion not ported yet.)

    // No wearing mid-textbox.
    if (play->msgCtx.msgMode != MSGMODE_NONE) {
        return;
    }
    // (MM gated on PLAYER_FORM_HUMAN and dropped the remains on transformation — OoT Link is always
    // "human", so that gate is gone.)
    // Mutual exclusion with native masks — if one got donned, our remains comes off.
    if ((player->currentMask != PLAYER_MASK_NONE) && (sWornRemains != ITEM_NONE)) {
        if (sWornRemains == ITEM_MM_REMAINS_ODOLWA) {
            BossRemains_StopOdolwaSfx();
        }
        sWornRemains = ITEM_NONE;
    }

    // Phase 3 — drive this frame's per-remains ally summons. Runs unconditionally so Gyorg's fish
    // school can self-maintain; Odolwa (R+B) / Goht (B / R+B) gate internally.
    BossRemains_TickSummons(play, player);

    // Gyorg's whirlpool keeps pulling + damaging while held (self-gates when inactive).
    BossRemains_GyorgWhirlpoolTick(play, player);

    // Odolwa mask-side effects (footsteps + shield-deflect fire).
    if (sWornRemains == ITEM_MM_REMAINS_ODOLWA) {
        BossRemains_TickOdolwa(play, player);
    }

    u16 press = play->state.input[0].press.button;
    if (press == 0) {
        return;
    }

    // (button bit, equipped item) for each remains-capable slot: 3 C buttons + 4 D-pad. SoH slot
    // order (equip_helper.h sButtonMasks): buttonItems[1..3] = C-left/down/right, [4..7] = D-pad
    // up/down/left/right.
    struct SlotBind {
        u16 btn;
        s16 item;
    };
    SlotBind slots[7] = {
        { BTN_CLEFT, (s16)gSaveContext.equips.buttonItems[1] },  { BTN_CDOWN, (s16)gSaveContext.equips.buttonItems[2] },
        { BTN_CRIGHT, (s16)gSaveContext.equips.buttonItems[3] }, { BTN_DUP, (s16)gSaveContext.equips.buttonItems[4] },
        { BTN_DDOWN, (s16)gSaveContext.equips.buttonItems[5] },  { BTN_DLEFT, (s16)gSaveContext.equips.buttonItems[6] },
        { BTN_DRIGHT, (s16)gSaveContext.equips.buttonItems[7] },
    };
    bool dpadOn = DpadEquipsEnabled();

    for (s32 i = 0; i < 7; i++) {
        bool isDpad = (i >= 3);
        if (isDpad && !dpadOn) {
            continue;
        }
        if (!(press & slots[i].btn)) {
            continue;
        }
        if (BossRemains_ItemIndex(slots[i].item) < 0) {
            continue;
        }
        // Toggle: same remains → doff, different/none → don.
        if (sWornRemains == slots[i].item) {
            if (sWornRemains == ITEM_MM_REMAINS_ODOLWA) {
                BossRemains_StopOdolwaSfx();
            }
            sWornRemains = ITEM_NONE;
            Player_PlaySfx(&player->actor, NA_SE_PL_TAKE_OUT_SHIELD);
        } else {
            sWornRemains = slots[i].item;
            player->currentMask = PLAYER_MASK_NONE; // hide any native mask under it
            Player_PlaySfx(&player->actor, NA_SE_PL_CHANGE_ARMS);
        }
        break;
    }
}

extern "C" void BossRemains_DrawWornMask(PlayState* play, Player* player) {
    if (!RemainsEnabled() || play == nullptr || player == nullptr) {
        return;
    }
    s32 idx = BossRemains_ItemIndex(sWornRemains);
    if (idx < 0) {
        return;
    }

    // Resolve the Moon Child mask DL from mm.o2r (real pointer; NULL = not mounted yet → skip frame).
    if (MmRes(kMaskDLPath[idx], &sMaskDLCache[idx]) == nullptr) {
        return;
    }

    f32 scale = kMaskScale[idx] * kScaleMul;

    OPEN_DISPS(play->state.gfxCtx);

    // Current matrix here is Link's head node (we are called from the PLAYER_LIMB_HEAD limb draw).
    // Mirror the Moon Child's order: Scale -> RotateZYX -> Translate, then load + draw. Push/Pop so
    // we don't perturb the hat/next limbs.
    Matrix_Push();
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    Matrix_RotateZYX(DegToBinang(kRotXDeg), DegToBinang(kRotYDeg), DegToBinang(kRotZDeg), MTXMODE_APPLY);
    Matrix_Translate(kMaskTransX[idx] + kOffX, kMaskTransY[idx] + kOffY, kMaskTransZ[idx] + kOffZ, MTXMODE_APPLY);

    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sMaskDLCache[idx]);
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

// ============================================================================
// (deprecated hook — Odolwa's roll is suppressed directly at the z_player.c choke point, and the
// other actions are handled per-frame. Kept as a no-op so the header/ABI stays stable.)
// ============================================================================

extern "C" s32 BossRemains_TryActionA(PlayState* play, Player* player) {
    (void)play;
    (void)player;
    return false;
}

// ============================================================================
// A-action accessors read by the player (z_player.c) — kept trivial + gated.
// ============================================================================

// True whenever the Odolwa remains is worn (drives the boosted run, sword/shield swap, faster
// attacks, trail).
extern "C" s32 BossRemains_IsOdolwaWorn(void) {
    return (RemainsEnabled() && (sWornRemains == ITEM_MM_REMAINS_ODOLWA)) ? 1 : 0;
}

// Idle stance override read by the z_player.c idle selection: while Odolwa is worn, Link's idle pose
// is Odolwa's "ready" stance. Returns NULL if the anim isn't loaded → vanilla idle.
extern "C" LinkAnimationHeader* BossRemains_GetOdolwaIdleAnim(void) {
    if (!BossRemains_IsOdolwaWorn()) {
        return nullptr;
    }
    return OdolwaAnim(ODOLWA_ANIM_READY);
}

// True while Link is riding the Odolwa moth-cloud (state 2). Read by z_player.c hooks so the flight
// driver owns movement (no gravity, no fall action) — mirrors the Goht-charge exemptions.
extern "C" s32 BossRemains_IsOdolwaFlying(void) {
    return (sOdolwaFlightState == 2) ? 1 : 0;
}

// Is there SOFT SOIL (ACTOR_OBJ_BEAN — the bean-planting spot, OoT's stand-in for MM's deku flower)
// within range of Link? Gates the moth-flight takeoff: only there does A summon the moths; anywhere
// else A stays Odolwa's fast run.
static bool BossRemains_NearSoftSoil(PlayState* play, Player* player) {
    // Obj_Bean is a BG actor (it carries a dynapoly platform for the ridable plant).
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_BG].head; a != nullptr; a = a->next) {
        if ((a->id == ACTOR_OBJ_BEAN) && (Actor_WorldDistXZToActor(&player->actor, a) < kOdolwaFlightSoilRange)) {
            return true;
        }
    }
    return false;
}

// Start a forced spell-style summon dance (locked, uninterruptible, plays fully). Held frame-by-frame
// in BossRemains_OdolwaFlightTick so the locomotion func can't steal it — like a spell cast takes
// over the player.
static void BossRemains_OdolwaSummonDance(PlayState* play, Player* player, OdolwaAnimId animId) {
    LinkAnimationHeader* anim = OdolwaAnim(animId);
    if ((anim == nullptr) || (play == nullptr) || (player == nullptr)) {
        return;
    }
    sOdolwaSummonAnim = anim;
    sOdolwaSummonFrame = 0.0f;
    // Hold for a fixed span (chant length + a bit), LOOPING the dance so it repeats enough to be heard.
    sOdolwaSummonLock = kOdolwaSummonDanceFrames;
    LinkAnimation_Change(play, &player->skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame((void*)anim), ANIMMODE_LOOP,
                         -4.0f);
    player->linearVelocity = 0.0f;
    // Sustained chant: MM played it FLAGGED every locked frame; the MmSfx bridge has no flagged model,
    // so play it ONCE here and MmSfx_Stop it at every dance-end/exit path.
    if (sOdolwaSummonChant != 0) {
        MmSfx_PlayAtPos(sOdolwaSummonChant, &player->actor.projectedPos);
    }
}

// Per-frame Odolwa flight driver — the "Nimbus" + the forced summon-dance lock. Called from
// z_player.c AFTER the action func so our overrides win. Flight = 1:1 the Gyorg/Zora free-swim feel:
// the STICK aims pitch + yaw and A advances forward along that 3D heading; the crouch pose stays
// completely static.
extern "C" void BossRemains_OdolwaFlightTick(PlayState* play, Player* player) {
    if ((play == nullptr) || (player == nullptr)) {
        return;
    }
    if (!BossRemains_IsOdolwaWorn()) {
        if (sOdolwaFlightState == 2) {
            player->actor.gravity = -2.0f; // don't strand a floating Link if the mask comes off mid-flight
        }
        sOdolwaFlightState = 0;
        sOdolwaSummonLock = 0;
        if (sOdolwaSummonChant != 0) {
            MmSfx_Stop(sOdolwaSummonChant); // cut a chant if the mask comes off mid-dance
            sOdolwaSummonChant = 0;
        }
        return;
    }

    Input* in = &play->state.input[0];

    // ── FORCED SUMMON DANCE: lock the player, own the anim frame by frame, LOOPING until the lock ends ──
    if (sOdolwaSummonLock > 0) {
        sOdolwaSummonLock--;
        player->linearVelocity = 0.0f;
        if (sOdolwaSummonAnim != nullptr) {
            f32 last = Animation_GetLastFrame((void*)sOdolwaSummonAnim);
            sOdolwaSummonFrame += 1.0f;
            if ((last > 0.0f) && (sOdolwaSummonFrame > last)) {
                sOdolwaSummonFrame -= last; // wrap → keep dancing (repeat), don't freeze on the last frame
            }
            LinkAnimation_Change(play, &player->skelAnime, sOdolwaSummonAnim, 1.0f, sOdolwaSummonFrame, last,
                                 ANIMMODE_ONCE, 0.0f);
        }
        if (sOdolwaSummonLock == 0) {
            // Dance done → stop the chant (the bridge one-shot may ring out; the stop is the contract).
            if (sOdolwaSummonChant != 0) {
                MmSfx_Stop(sOdolwaSummonChant);
            }
            sOdolwaSummonChant = 0;
        }
        // The moth-dance windup drives takeoff even while locked (state 1 spawns the cloud below).
        if (sOdolwaFlightState != 1) {
            return;
        }
    }

    // ── TAKEOFF: A (no R) while standing on the ground NEAR SOFT SOIL ─────────
    // The soil (ACTOR_OBJ_BEAN) is OoT's stand-in for MM's deku flower: ONLY there does A summon the
    // moths. Everywhere else A must stay Odolwa's fast run (that's his normal move), so the gate is
    // what keeps the two from fighting over the button.
    if ((sOdolwaFlightState == 0) && CHECK_BTN_ALL(in->press.button, BTN_A) && !CHECK_BTN_ALL(in->cur.button, BTN_R) &&
        (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && (play->msgCtx.msgMode == MSGMODE_NONE) &&
        BossRemains_NearSoftSoil(play, player)) {
        // Odolwa's chant (VOICE2 for the moth summon — a different voice) + the moth-summon dance as
        // windup. Played once at dance start, stopped at the exits (see the chant notes above).
        sOdolwaSummonChant = NA_SE_EN_MIBOSS_VOICE2_OLD;
        BossRemains_OdolwaSummonDance(play, player, ODOLWA_ANIM_MOTH_DANCE);
        sOdolwaFlightState = 1;
        sOdolwaFlightWindup = kOdolwaSummonDanceFrames; // wait out the full dance before liftoff
        return;
    }

    // ── WINDUP: the moth-summon dance plays, then Link lifts off onto the cloud ──
    if (sOdolwaFlightState == 1) {
        player->linearVelocity = 0.0f;
        if (--sOdolwaFlightWindup <= 0) {
            sOdolwaFlightState = 2;
            sOdolwaSummonLock = 0; // done dancing → let the airborne crouch pose take over
            if (sOdolwaSummonChant != 0) {
                MmSfx_Stop(sOdolwaSummonChant); // stop the takeoff chant (liftoff bypasses the lock handler)
                sOdolwaSummonChant = 0;
            }
            sOdolwaFlightTimer = kOdolwaFlightMaxFrames; // 10s before the moths tire out
            sOdolwaFlightYaw = player->actor.shape.rot.y;
            sOdolwaFlightPitch = 0;
            player->actor.velocity.y = 4.0f; // pop up onto the cloud
            player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
            // Spawn the carrying moth cloud — 8 moths that orbit under Link.
            BossRemains_EnsureActorsRegistered();
            for (s32 i = 0; i < 8; i++) {
                Vec3f p = player->actor.world.pos;
                p.y -= 8.0f;
                RemainsAllyBug_SpawnCloud(play, &p, player->actor.shape.rot.y);
            }
        }
        return;
    }

    // ── AIRBORNE: ride the cloud — 1:1 Zora free-swim (stick aims pitch+yaw, A advances) ──
    if (sOdolwaFlightState == 2) {
        // 10s timeout, or manual exit (R / touching ground). Restore gravity (we zeroed it) + stop the
        // wing-flap loop so it doesn't keep sounding after landing.
        bool timedOut = (--sOdolwaFlightTimer <= 0);
        bool manualExit = CHECK_BTN_ALL(in->cur.button, BTN_R) ||
                          ((player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && !CHECK_BTN_ALL(in->cur.button, BTN_A));
        if (timedOut || manualExit) {
            sOdolwaFlightState = 0;
            player->actor.gravity = -2.0f;
            MmSfx_Stop(NA_SE_EN_MB_MOTH_FLY);
            return;
        }

        // Static crouch pose — completely frozen (playSpeed 0, held on frame 0), re-asserted every frame.
        LinkAnimationHeader* crouch = OdolwaAnim(ODOLWA_ANIM_CROUCH);
        if (crouch != nullptr) {
            LinkAnimation_Change(play, &player->skelAnime, crouch, 0.0f, 0.0f, 0.0f, ANIMMODE_ONCE, 0.0f);
        }

        // Steer pitch + yaw from the stick, INVERTED to match Zora free-swim (stick up = nose down).
        sOdolwaFlightYaw -= (s16)(in->rel.stick_x * kOdolwaFlightTurnRate);
        sOdolwaFlightPitch -= (s16)(in->rel.stick_y * kOdolwaFlightTurnRate);
        sOdolwaFlightPitch = CLAMP(sOdolwaFlightPitch, (s16)-kOdolwaFlightPitchMax, kOdolwaFlightPitchMax);

        player->actor.world.rot.y = sOdolwaFlightYaw;
        player->actor.shape.rot.y = sOdolwaFlightYaw;
        player->yaw = sOdolwaFlightYaw;
        player->actor.gravity = 0.0f;

        // A advances forward along the aim; no A = hover in place.
        if (CHECK_BTN_ALL(in->cur.button, BTN_A)) {
            player->linearVelocity = kOdolwaFlightSpeed * Math_CosS(sOdolwaFlightPitch);
            player->actor.velocity.y = kOdolwaFlightSpeed * Math_SinS(sOdolwaFlightPitch);
            if ((play->gameplayFrames & 7) == 0) {
                // Wing-flap loop: MM sustained it flagged; the bridge re-fires the one-shot on cadence.
                MmSfx_PlayAtPos(NA_SE_EN_MB_MOTH_FLY, &player->actor.projectedPos);
            }
        } else {
            player->linearVelocity = 0.0f;
            player->actor.velocity.y = 0.0f;
        }
    }
}

// Run-speed multiplier applied at the z_player.c run action: 2x while wearing Odolwa AND HOLDING A;
// 1.0x otherwise. (sOdolwaRunBoost is refreshed each frame in BossRemains_TickOdolwa.)
extern "C" f32 BossRemains_RunSpeedMul(void) {
    return (BossRemains_IsOdolwaWorn() && sOdolwaRunBoost) ? 2.0f : 1.0f;
}

// True while the 2x run boost (A held) is active — drives the purple trail (trail func also gates on
// actual movement), so the trail only shows during the boosted run. NEVER while riding the moth cloud
// (A there means "advance", not "run") — the flight has no run trail.
extern "C" s32 BossRemains_IsOdolwaRunning(void) {
    return (BossRemains_IsOdolwaWorn() && sOdolwaRunBoost && !BossRemains_IsOdolwaFlying()) ? 1 : 0;
}

// ============================================================================
// Odolwa red running afterimage (trail)
// ============================================================================
// Frozen-pose ghosts of Link's OWN skeleton, tinted at the FOG stage: keep a ring buffer of past
// {pos, yaw, pose} captured each frame while running, then redraw Link's skeleton at those past poses
// with a constant-fraction dark-purple fog (combiner-independent — several player limb combiners
// ignore env color entirely, which is why an env tint showed nothing). POLY_OPA, no per-copy alpha —
// the fade is purely temporal (older samples get overwritten). Drawn from the player draw hook.

namespace {
constexpr s32 kOdolwaTrailMax = 6; // ghost copies trailing at once
struct OdolwaTrailSample {
    Vec3f pos;
    s16 yaw;
    u8 valid;
    Vec3s joints[PLAYER_LIMB_MAX];
};
OdolwaTrailSample sOdolwaTrail[kOdolwaTrailMax];
s32 sOdolwaTrailHead = 0;

inline void OdolwaTrailClear() {
    for (s32 i = 0; i < kOdolwaTrailMax; i++) {
        sOdolwaTrail[i].valid = 0;
    }
}
} // namespace

extern "C" void BossRemains_DrawOdolwaTrail(Player* player, PlayState* play) {
    if (play == nullptr || player == nullptr) {
        return;
    }
    // Only trail while actively running; otherwise clear so stale ghosts don't linger.
    if (!BossRemains_IsOdolwaRunning() || (player->actor.speedXZ < 1.0f)) {
        OdolwaTrailClear();
        return;
    }

    s32 lc = player->skelAnime.limbCount;
    if (lc >= PLAYER_LIMB_MAX) {
        lc = PLAYER_LIMB_MAX - 1;
    }

    // Capture this frame's finalized pose into the ring buffer.
    OdolwaTrailSample* cur = &sOdolwaTrail[sOdolwaTrailHead];
    cur->pos = player->actor.world.pos;
    cur->yaw = player->actor.shape.rot.y;
    cur->valid = 1;
    for (s32 j = 0; j <= lc; j++) {
        cur->joints[j] = player->skelAnime.jointTable[j];
    }
    sOdolwaTrailHead = (sOdolwaTrailHead + 1) % kOdolwaTrailMax;

    OPEN_DISPS(play->state.gfxCtx);

    // Keep each ghost's REAL model — textures, tunic, sword, everything — and lay a DARK PURPLE tint
    // OVER it at the FOG stage, which runs AFTER every limb's combiner and so is combiner-independent
    // (SETUPDL_25 keeps G_FOG on, and scene fog already tints Link this way). gDPSetFogColor picks the
    // dark purple, and gSPFogFactor with multiplier 0 makes the blend a CONSTANT fraction at EVERY
    // depth (depth-independent, so it reaches Link right up against the lens). Offset 120/255 ≈ a 47%
    // overlay, so the textured model clearly shows through — a tint, not a flat silhouette.
    // Play_SetFog restores the scene's own fog afterward.
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPSetFogColor(POLY_OPA_DISP++, 55, 12, 90, 255);
    gSPFogFactor(POLY_OPA_DISP++, 0, 120);

    Vec3s blended[PLAYER_LIMB_MAX];
    for (s32 k = 0; k < kOdolwaTrailMax; k++) {
        OdolwaTrailSample* smp = &sOdolwaTrail[k];
        if (!smp->valid || (smp == cur)) { // skip the just-captured (live) body
            continue;
        }
        for (s32 j = 0; j <= lc; j++) {
            blended[j] = smp->joints[j];
        }

        Matrix_Translate(smp->pos.x, smp->pos.y, smp->pos.z, MTXMODE_NEW);
        Matrix_RotateY(BINANG_TO_RAD(smp->yaw), MTXMODE_APPLY);
        Matrix_Scale(player->actor.scale.x, player->actor.scale.y, player->actor.scale.z, MTXMODE_APPLY);

        // Player_OverrideLimbDrawGameplayDefault selects Link's per-limb equipment DLs (sword/shield/
        // sheath/tunic) so the ghosts wear exactly what Link wears. Its SoH signature already takes
        // void* data (the Player); pass the player.
        SkelAnime_DrawFlexOpa(play, player->skelAnime.skeleton, blended, player->skelAnime.dListCount,
                              Player_OverrideLimbDrawGameplayDefault, NULL, player);
    }
    // Restore the scene's fog so the real Link + later actors aren't left purple.
    POLY_OPA_DISP = Play_SetFog(play, POLY_OPA_DISP);

    CLOSE_DISPS(play->state.gfxCtx);
}

// ============================================================================
// Odolwa sword + shield swap
// ============================================================================
// While the Odolwa remains is worn, Link's native sword/shield are hidden in the limb override
// (z_player_lib.c) and Odolwa's own DLs (object_boss01, mm.o2r) are drawn here in their place,
// following the hand-limb matrix (called from Player_PostLimbDrawGameplay at LEFT_HAND / RIGHT_HAND).
// Odolwa's models are authored at boss scale, so the fit was tuned live (2ship) and baked.
static const char* const kOdolwaSwordDLPath = "objects/object_boss01/gOdolwaSwordDL";
static const char* const kOdolwaShieldDLPath = "objects/object_boss01/gOdolwaShieldDL";
static void* sOdolwaSwordDLCache = nullptr;
static void* sOdolwaShieldDLCache = nullptr;

// In-hand placement, tuned in-game (2ship) and baked. Sword on Link's LEFT hand, shield on the RIGHT.
constexpr f32 kOdolwaSwordScale = 0.4f;
constexpr s32 kOdolwaSwordRotX = 0, kOdolwaSwordRotY = 7, kOdolwaSwordRotZ = 75;
constexpr f32 kOdolwaSwordOffX = -21.0f, kOdolwaSwordOffY = 393.0f, kOdolwaSwordOffZ = -157.0f;
constexpr f32 kOdolwaShieldScale = 0.7f;
constexpr s32 kOdolwaShieldRotX = -90, kOdolwaShieldRotY = 90, kOdolwaShieldRotZ = 0;
constexpr f32 kOdolwaShieldOffX = 0.0f, kOdolwaShieldOffY = 0.0f, kOdolwaShieldOffZ = 0.0f;

// The boss DLs call into segment 0x0C (eye/limb sub-DL jumps); park it on a no-op list so they draw
// standalone on Link.
static Gfx sOdolwaEquipSeg0xC_Noop[] = {
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
};

// Draw one boss DL at the current (hand-limb) matrix with a baked scale/rot/offset.
static void DrawOdolwaEquipDL(PlayState* play, void* dl, f32 scale, s16 rx, s16 ry, s16 rz, f32 ox, f32 oy, f32 oz) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sOdolwaEquipSeg0xC_Noop);

    Matrix_Push();
    Matrix_Translate(ox, oy, oz, MTXMODE_APPLY);
    Matrix_RotateZYX(rx, ry, rz, MTXMODE_APPLY);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)dl);
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void BossRemains_DrawOdolwaSword(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr || !BossRemains_IsOdolwaWorn()) {
        return;
    }
    // Only while Link actually has a one-handed sword in hand (OoT model type: LH_SWORD).
    if (player->leftHandType != PLAYER_MODELTYPE_LH_SWORD) {
        return;
    }
    if (MmRes(kOdolwaSwordDLPath, &sOdolwaSwordDLCache) == nullptr) {
        return; // mm.o2r not mounted yet — retry next frame
    }
    DrawOdolwaEquipDL(play, sOdolwaSwordDLCache, kOdolwaSwordScale, DegToBinang(kOdolwaSwordRotX),
                      DegToBinang(kOdolwaSwordRotY), DegToBinang(kOdolwaSwordRotZ), kOdolwaSwordOffX, kOdolwaSwordOffY,
                      kOdolwaSwordOffZ);
}

extern "C" void BossRemains_DrawOdolwaShield(PlayState* play, Player* player) {
    if (play == nullptr || player == nullptr || !BossRemains_IsOdolwaWorn()) {
        return;
    }
    if (MmRes(kOdolwaShieldDLPath, &sOdolwaShieldDLCache) == nullptr) {
        return; // mm.o2r not mounted yet — retry next frame
    }
    DrawOdolwaEquipDL(play, sOdolwaShieldDLCache, kOdolwaShieldScale, DegToBinang(kOdolwaShieldRotX),
                      DegToBinang(kOdolwaShieldRotY), DegToBinang(kOdolwaShieldRotZ), kOdolwaShieldOffX,
                      kOdolwaShieldOffY, kOdolwaShieldOffZ);
}
