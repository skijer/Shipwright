/**
 * garo_form.cpp - Garo form: skin, moveset and rod.
 *
 * Every button is read RAW: TransformMasks_FilterB strips B from what OOT's
 * action func sees, so the state machine below owns Garo's combat outright.
 * Poses run on a form-exclusive SkelAnime so that action func cannot interrupt.
 */

#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "mods/transformation_masks/transformation_masks.h"
#include "mods/transformation_masks/garo_skin.h"
#include "mods/transformation_masks/garo_hybrid_render.h"
#include "mods/o2r_loader/o2r_loader.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/resource/type/PlayerAnimation.h"
#include "soh/resource/type/SohResourceType.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include <ship/resource/ResourceManager.h>
#include <libultraship/bridge.h>
#include <spdlog/spdlog.h>
#include <cmath>
#include <cstring>
#include <map>
#include <string>

#define GARO_SKEL_PATH "__OTR__objects/forms/garo/gGaroSkel"

// BGCHECKFLAG_GROUND isn't exposed in a global header; other mods that need
// it inline it locally (see equip_champion.c, gerudo_form.cpp, etc.).
#ifndef BGCHECKFLAG_GROUND
#define BGCHECKFLAG_GROUND 0x0001
#endif

// OPEN_DISPS declares these inline, which takes C++ linkage in this TU unless an
// extern "C" is already visible — without it the macro's calls fail to link.
extern "C" void FrameInterpolation_RecordOpenChild(const void* a, int b);
extern "C" void FrameInterpolation_RecordCloseChild(void);

extern "C" PlayState* gPlayState;

// The rod aim borrows OOT's slingshot pipeline, the mechanism the Deku bubble uses: it un-pauses
// the action func and runs the real aim camera, which a paused form can never reproduce by hand.
extern "C" void Player_StartDekuBubble(Player* this_, PlayState* play);
extern "C" void Player_DekuBubbleCleanup(Player* this_);

#define GARO_ROD_MAGIC_COST 4

// Attack tuning
// 3-slash combo + Garo signature finisher.
//
// Garo's melee is a single move: a free dual-sword spin (garo_spinAttack) that
// fires on the B press. The old 3-slash combo — and the shurikens / paralyzing
// knives it threw — were removed by design; the only projectile left is the
// rod orb. Tap-vs-hold is decided INSIDE the spin (hold B long enough and it
// converts to the rod charge), so the attack never waits on the button.

#define GARO_SWING_DAMAGE 2 // default damage of the shared swing quad
#define GARO_SPIN_DAMAGE 2  // B-tap dual-sword spin
// Fixed length, not "until the anim ends": garo_spinAttack is short enough that Garo
// would leave the state before coming round. The rate spreads the turns over that window.
#define GARO_SPIN_FRAMES 14
#define GARO_SPIN_TURNS 2
#define GARO_SPIN_YAW_RATE ((0x10000 * GARO_SPIN_TURNS) / GARO_SPIN_FRAMES)
#define GARO_SPIN_PLAYSPEED 1.5f
#define GARO_SPIN_MOVE_SPEED (9.0f * 1.5f) // 1.5x Link's run (R_RUN_SPEED_LIMIT 900)

#define GARO_ORB_POOL_MAX 12

#define GARO_PARRY_DAMAGE 4
#define GARO_PARRY_STRIKE_HIT_F 6
#define GARO_PARRY_FREEZE_FRAMES 60
// freezeTimer halts an actor's update, and an actor that does not update never re-registers its AC
// collider: a frozen enemy is untouchable, so both strikes thaw their target just before landing.
#define GARO_PARRY_THAW_LEAD 2
#define GARO_BANISH_THAW_LEAD 3

#define GARO_REFLECT_FRAMES 60
#define GARO_REFLECT_DAMAGE 4
#define GARO_REFLECT_HALF 10.0f
#define GARO_REFLECT_MIN_SPEED 8.0f
#define GARO_REFLECT_INVULN 12
#define GARO_REFLECT_PUSH_OUT 35.0f // clear of Garo, or the shot pops on his own collider
// Shots are caught in the air: most projectiles delete themselves on impact, so waiting for
// the hit leaves nothing to send back.
#define GARO_GUARD_CATCH_RADIUS 130.0f
#define GARO_GUARD_CATCH_HEIGHT 40.0f
#define GARO_GUARD_CATCH_MIN_SPEED 1.0f
#define GARO_GUARD_MELEE_RANGE 160.0f // past this the counter would chase a shooter across the room

// gGaroGuardAnim raises in its first half and returns in its second, so the stance holds at the
// middle and plays the rest backwards on release.
#define GARO_GUARD_HOLD_FRACTION 0.5f
#define GARO_GUARD_RETURN_SPEED 1.5f

#define GARO_DASH_SPEED 14.0f
#define GARO_DASH_DAMAGE 4
#define GARO_BANISH_COOLDOWN 300 // 5s @ 60fps
#define GARO_BANISH_OFFSET 50.0f
#define GARO_BANISH_VANISH_END 8
#define GARO_BANISH_STUN_FRAMES 60
#define GARO_BANISH_SHADOW_LEN 9
#define GARO_BANISH_STUN_RADIUS 80.0f
#define GARO_RIPOSTE_OFFSET 45.0f

#define GARO_GUARD_PATH "objects/forms/garo/gPlayerAnim_garo_guard"
#define GARO_DASHATTACK_PATH "objects/forms/garo/gPlayerAnim_garo_dashAttack"
#define GARO_SPINATTACK_PATH "objects/forms/garo/gPlayerAnim_garo_spinAttack"
#define GARO_COLLAPSE_PATH "objects/forms/garo/gPlayerAnim_garo_collapse"
#define GARO_APPEAR_PATH "objects/forms/garo/gPlayerAnim_garo_appear"

#define GARO_SLASHSTART_PATH "objects/forms/garo/gPlayerAnim_garo_slashStart"
#define GARO_TAKEOUTBOMB_PATH "objects/forms/garo/gPlayerAnim_garo_takeOutBomb"
#define GARO_LAUGH_PATH "objects/forms/garo/gPlayerAnim_garo_laugh"
// v10 A-button overhaul anims — all camelCase, verified in soh.o2r.
#define GARO_APPEARDRAWSWORDS_PATH "objects/forms/garo/gPlayerAnim_garo_appearDrawSwords"
#define GARO_BOUNCE_PATH "objects/forms/garo/gPlayerAnim_garo_bounce"
#define GARO_JUMPBACK_PATH "objects/forms/garo/gPlayerAnim_garo_jumpBack"
#define GARO_SLASHLOOP_PATH "objects/forms/garo/gPlayerAnim_garo_slashLoop"
#define GARO_DRAWSWORDS_PATH "objects/forms/garo/gPlayerAnim_garo_drawSwords"

#define GARO_SHADOW_BALL_DAMAGE 8
#define GARO_SHADOW_BALL_HIT_F 12
#define GARO_SHADOW_BALL_PLAYSPEED 2.5f
#define GARO_LAND_STRIKE_DAMAGE 8
#define GARO_LAND_STRIKE_HIT_F 12
#define GARO_LAND_STRIKE_TAIL_F 3

// Three charge levels: below TIER2 nothing is fired at all, TIER2 sends one seeking orb,
// TIER3 sends one that breaks into GARO_ORB_SEEKERS fragments where it lands.
#define GARO_ROD_CHARGE_MAX 120
#define GARO_ROD_CHARGE_TIER2 35
#define GARO_ROD_CHARGE_TIER3 85
#define GARO_ROD_LEVEL_MAX 3
#define GARO_ROD_L2_DAMAGE 4
#define GARO_ROD_L3_DAMAGE 3 // lower: its fragments carry the rest

#define GARO_ORB_SEEKERS 4
#define GARO_ORB_SEEKER_DAMAGE 2
#define GARO_ORB_SEEKER_LIFETIME 70
#define GARO_ORB_SEEKER_SPEED 16.0f
#define GARO_ORB_SEEKER_TURN 0x1200
#define GARO_ORB_SEEKER_FAN 0x2000
#define GARO_ORB_BURST_BALLS 8
// The target must be roughly AHEAD of the orb, or a shot would turn round and chase
// something it has already flown past.
#define GARO_ORB_HOME_RANGE 700.0f
#define GARO_ORB_HOME_CONE 0.2f
#define GARO_ORB_TURN_RATE 0x0700

// The six SW97 arrow types, in SW97's own order. Cycled with L/R while aiming.
#define GARO_ROD_ELEMENT_COUNT 6
#define GARO_ELEM_FIRE 0
#define GARO_ELEM_ICE 1
#define GARO_ELEM_LIGHT 2
#define GARO_ELEM_DARK 3
#define GARO_ELEM_SOUL 4
#define GARO_ELEM_WIND 5

#define GARO_ROD_RELEASE_CD 10 // or a release into an instant re-press reads as rapid fire
// The spin starts on the press and converts to the rod charge if B is still down this
// many frames later, so the attack never waits on the button.
#define GARO_B_HOLD_THRESHOLD 9
#define GARO_LAUGH_CHANCE 0.20f

// Half the Trident's head-sized ball: Garo's is a spark, and it hangs in front of the aim camera.
#define GARO_ROD_BALL_CIRCLE_MAX 0.08f
#define GARO_ROD_BALL_SCALE_MAX 7.0f

#define GARO_RETICLE_DIST 320.0f
#define GARO_RETICLE_SPREAD 26.0f
#define GARO_RETICLE_DOT_SCALE 0.55f

// 67 s16 per frame for the 21-limb Link/Garo PlayerAnimation format.
static constexpr s32 GARO_ANIM_S16_PER_FRAME = 67;

enum GaroAttackState {
    GARO_IDLE = 0,
    GARO_SPIN,          // B press: free dual-sword spin
    GARO_ROD_AIM,       // B held: charge ball, L/R cycle element
    GARO_PARRY_GUARD,   // R held: guard stance, holding mid-anim
    GARO_GUARD_RETURN,  // R released: the stance coming back down
    GARO_PARRY_RIPOSTE, // hit while guarding: appear behind the attacker and strike
    GARO_DASH_ATTACK,   // A held, no Z
    GARO_BANISH_VANISH, // Z+A neutral: stun the target and dissolve
    GARO_BANISH_SHADOW, // the shadow ball crossing to the target
    GARO_SHADOW_BALL,   // arrival: appear behind it and strike
    GARO_LAUGH_TAUNT,   // post-kill taunt, non-pausing
    GARO_SIDEHOP_L,     // Z+A stick left
    GARO_SIDEHOP_R,     // Z+A stick right
    GARO_BACKFLIP,      // Z+A stick back
    GARO_JUMP_ATTACK,   // Z+A stick forward while moving
    GARO_AIR_SLASH,     // B in mid-air
    GARO_LAND_STRIKE,   // AIR_SLASH touching down
};

// The rod orb, Garo's only projectile. Its dmgFlag rides a real AC quad so boss
// vulnerability masks accept the hit.
#define GARO_ORB_WAKE_SCALE 85
// More path samples are kept than are drawn, so a fragment's streak holds its length
// while the head moves.
#define GARO_ORB_TRAIL_LEN 15
#define GARO_ORB_TRAIL_DRAWN 12
#define GARO_ORB_STREAK_SCALE 0.008f

typedef struct {
    u8 active;
    Vec3f pos;
    s16 yaw;
    s16 pitch;
    s16 timer;
    u8 element; // GARO_ELEM_*
    u8 damage;
    u32 dmgFlag; // OR'd with DMG_SLASH_MASTER at AC time, for restrictive enemies
    u8 bursts;   // the level-3 ball: breaks into seekers instead of vanishing
    u8 isSeeker; // one of those fragments
    Actor* target;
    // Carried from the charge so the shot reads as THAT ball flying off.
    f32 ballCircle;
    f32 ballScale;
    Vec3f trailPos[GARO_ORB_TRAIL_LEN];
    Vec3f trailRot[GARO_ORB_TRAIL_LEN]; // radians: .x pitch, .y yaw
    s16 trailIdx;
} GaroOrb;

static struct {
    GaroAttackState state;
    s16 stateTimer;

    s16 rodChargeTimer; // frames B has been held in ROD_AIM
    u8 rodElement;
    u8 rodSfxPlayed; // full-charge chime latch
    f32 rodBallCircle;
    f32 rodBallScale;
    s16 rodBallRays;
    u8 rodAimActive;      // the borrowed slingshot aim is ours right now
    s16 bHoldDetectTimer; // B held since the spin started; tap vs hold
    s16 rodReleaseCD;
    u8 laughPending;  // an enemy died; the next idle frame rolls for the taunt
    s16 spinEntryYaw; // restored at the end, so spinning on the spot does not re-aim him

    // Vertices are fed in garo_hybrid_render.cpp, at the blade bones.
    s32 trailEffectIndex;  // left sword
    s32 trailEffectIndex2; // right sword
    u8 trailActive;

    s16 banishCooldown;
    Actor* banishTarget;
    Actor* parryAttacker;
    Actor* reflectShot;
    s16 reflectTimer;
    Vec3f shadowBallStart;
    Vec3f shadowBallEnd;
    Vec3f shadowBallPos; // written by the travel state, read by the draw
    s16 shadowBallTimer;
    u8 shadowBallSlashFired; // edge latch, so the quad enables exactly once

    u8 hopDir;
    s16 hopAirTimer;
    u8 airSlashActive;
    u8 landStrikeFired;
    s16 landStrikeTailFrames;
    u8 deathFlamesSpawned; // cleared on revival and on scene reload
    u8 prevJumping;        // rising edge for the jump multiplier
    GaroOrb orbs[GARO_ORB_POOL_MAX];
} sGaroAttack = {};

// Sword trail LENGTH lives in garo_post_limb.cpp (GARO_POST_LIMB_TRAIL_LENGTH),
// which is where the vertices are fed during the L_HAND limb draw — this TU only
// owns the effect's lifetime.
// Vertex segment lifetime in frames (EffectBlureInit1.elemDuration). Zora uses 8.
#define GARO_TRAIL_ELEM_DURATION 8

// Form-exclusive SkelAnime — runs the combo animation on its own buffers so
// OOT's action func can NEVER interrupt it. Each frame in combo we
// LinkAnimation_Update this and memcpy its jointTable over player->skelAnime's,
// so the visible pose is whatever the form skelAnime computed.
//
// Equivalent of gFormState.formSkelAnime in mm_player_form.cpp:2987-2993, but
// shares Link's skeleton (Garo doesn't change body topology, only textures).
static SkelAnime sFormSkelAnime;
static Vec3s sFormJointTable[PLAYER_LIMB_BUF_COUNT];
static Vec3s sFormMorphTable[PLAYER_LIMB_BUF_COUNT];
static u8 sFormSkelAnimeReady = 0;
static s8 sFormSkelAnimeAge = -1; // tracks linkAge to detect adult/child swap

// Cached LinkAnimationHeader wrappers for raw PlayerAnimation resources from
// .o2r (soh.o2r anims have no header struct — just the s16 payload). Pointer
// stability matters since LinkAnimation_Change retains the address — std::map
// gives stable iterators across rehash.
static std::map<std::string, LinkAnimationHeader> sAnimWrappers;

// v9 rod mode helpers — element table + damage tier resolution
// Maps GARO_ELEM_* → AC damage flag. Only three of the six have a vanilla
// arrow flag to ride on (which is what makes element-vulnerable bosses react:
// Phantom Ganon to arrows, Ganon2's weakpoint to LIGHT, Dodongo to FIRE);
// dark / soul / wind fall back to DMG_ARROW_NORMAL, so they still damage
// everything ordinary without falsely claiming a boss weakness.
static u32 GaroAttack_GetRodDmgFlag(u8 element) {
    switch (element) {
        case GARO_ELEM_FIRE:
            return DMG_ARROW_FIRE;
        case GARO_ELEM_ICE:
            return DMG_ARROW_ICE;
        case GARO_ELEM_LIGHT:
            return DMG_ARROW_LIGHT;
        default:
            return DMG_ARROW_NORMAL;
    }
}

// Resolves the charge-tier damage from the live timer. Capped 1..4 so a
// rapid-fire shot still inflicts something while a fully-charged release
// (≥ tier 4 threshold) deals 4 — same scale as the parry/banish counter
// strikes so element vulnerability is the differentiator, not raw numbers.
// Charge level 1..3 — see the tier thresholds. Level 1 fires nothing.
static u8 GaroAttack_GetRodLevel(s16 chargeTimer) {
    if (chargeTimer < GARO_ROD_CHARGE_TIER2)
        return 1;
    if (chargeTimer < GARO_ROD_CHARGE_TIER3)
        return 2;
    return 3;
}

// Centralized reset for rod-mode latches. Called on release, on interrupt
// (parry / banish / damage break), and on form change. rodElement is NOT
// cleared — the last-selected element persists across aims (UI continuity).
static void GaroForm_LeaveRodState(void) {
    sGaroAttack.rodChargeTimer = 0;
    sGaroAttack.rodSfxPlayed = 0;
    sGaroAttack.rodBallCircle = 0.0f;
    sGaroAttack.rodBallScale = 0.0f;
    sGaroAttack.rodBallRays = 0;
}

// Animation loader (pattern from animationViewer.cpp:119-144)
static LinkAnimationHeader* GaroForm_LoadAnim(const char* path) {
    auto res = ResourceMgr_GetResourceByNameHandlingMQ(path);
    if (res == nullptr) {
        return nullptr;
    }

    uint32_t type = res->GetInitData()->Type;
    if (type == static_cast<uint32_t>(SOH::ResourceType::SOH_PlayerAnimation)) {
        auto playerAnim = std::static_pointer_cast<SOH::PlayerAnimation>(res);
        LinkAnimationHeader& wrapper = sAnimWrappers[path];
        size_t totalS16 = playerAnim->GetPointerSize() / sizeof(int16_t);
        wrapper.common.frameCount = (s16)(totalS16 / GARO_ANIM_S16_PER_FRAME);
        wrapper.segment = (void*)playerAnim->GetPointer();
        return &wrapper;
    }

    // AnimationHeader (indexed) shares a common prefix with LinkAnimationHeader.
    return (LinkAnimationHeader*)ResourceMgr_LoadAnimByName(path);
}

// Two blades, two EffectBlure1s. They are fed in garo_hybrid_render.cpp at the sword
// bones: fed from Link's hidden hand instead, the streak came out detached from them.
static void GaroAttack_SpawnTrail(PlayState* play) {
    MmForm_KillTrail(play, &sGaroAttack.trailEffectIndex, &sGaroAttack.trailActive);
    MmForm_KillTrail(play, &sGaroAttack.trailEffectIndex2, &sGaroAttack.trailActive);

    // Garo palette: dark violet → near-black fade. Distinct from Zora cyan.
    EffectBlureInit1 blure = {};
    blure.p1StartColor[0] = 120;
    blure.p1StartColor[1] = 60;
    blure.p1StartColor[2] = 200;
    blure.p1StartColor[3] = 200;
    blure.p2StartColor[0] = 60;
    blure.p2StartColor[1] = 30;
    blure.p2StartColor[2] = 140;
    blure.p2StartColor[3] = 100;
    blure.p1EndColor[0] = 60;
    blure.p1EndColor[1] = 30;
    blure.p1EndColor[2] = 140;
    blure.p1EndColor[3] = 0;
    blure.p2EndColor[0] = 60;
    blure.p2EndColor[1] = 30;
    blure.p2EndColor[2] = 140;
    blure.p2EndColor[3] = 0;
    blure.elemDuration = GARO_TRAIL_ELEM_DURATION;
    blure.unkFlag = 0;
    blure.calcMode = 0;
    Effect_Add(play, &sGaroAttack.trailEffectIndex, EFFECT_BLURE1, 0, 0, &blure);
    Effect_Add(play, &sGaroAttack.trailEffectIndex2, EFFECT_BLURE1, 0, 0, &blure);
    sGaroAttack.trailActive = 1;
}

static void GaroAttack_KillTrail(PlayState* play) {
    MmForm_KillTrail(play, &sGaroAttack.trailEffectIndex, &sGaroAttack.trailActive);
    MmForm_KillTrail(play, &sGaroAttack.trailEffectIndex2, &sGaroAttack.trailActive);
}

// Public accessors so garo_post_limb.cpp can read trail state + feed vertices.
extern "C" u8 GaroAttack_IsTrailActive(void) {
    return sGaroAttack.trailActive;
}
extern "C" s32 GaroAttack_GetTrailEffectIndex2(void) {
    return sGaroAttack.trailEffectIndex2;
}

extern "C" s32 GaroAttack_GetTrailEffectIndex(void) {
    return sGaroAttack.trailEffectIndex;
}

// MmAnim_LoadByPath cannot serve these: it is gated on mm.o2r and on a non-zero frame
// count, and Garo's anims live in soh.o2r with counts derived from the resource size.
extern "C" LinkAnimationHeader* GaroForm_LoadAnimPublic(const char* path) {
    return GaroForm_LoadAnim(path);
}

// A SkelAnime independent of player->skelAnime, so OOT's action func cannot touch the
// pose; each frame it is memcpy'd over the player's jointTable, which the skin draw reads.
static void GaroAttack_EnsureFormSkelAnime(PlayState* play) {
    if (sFormSkelAnimeReady && sFormSkelAnimeAge == gSaveContext.linkAge) {
        return;
    }
    SkelAnime_InitLink(play, &sFormSkelAnime, gPlayerSkelHeaders[gSaveContext.linkAge],
                       (LinkAnimationHeader*)gPlayerAnim_link_normal_wait, 9, sFormJointTable, sFormMorphTable,
                       PLAYER_LIMB_MAX);
    sFormSkelAnime.baseTransl.x = -57;
    sFormSkelAnime.baseTransl.y = 3377;
    sFormSkelAnime.baseTransl.z = 0;
    sFormSkelAnimeReady = 1;
    sFormSkelAnimeAge = gSaveContext.linkAge;
}

// endFrame < 0 plays to the anim's last frame; a negative playSpeed runs it backwards.
static void GaroAttack_StartFormAnim(PlayState* play, LinkAnimationHeader* anim, f32 startFrame, f32 endFrame,
                                     f32 playSpeed) {
    if (anim == nullptr)
        return;
    GaroAttack_EnsureFormSkelAnime(play);
    if (endFrame < 0.0f) {
        endFrame = Animation_GetLastFrame(anim);
    }
    LinkAnimation_Change(play, &sFormSkelAnime, anim, playSpeed, startFrame, endFrame, ANIMMODE_ONCE, -2.0f);
}

// Returns 1 when the current form animation reached its final frame this tick.
static s32 GaroAttack_AdvanceFormAnim(PlayState* play, Player* player) {
    if (!sFormSkelAnimeReady)
        return 0;
    s32 done = LinkAnimation_Update(play, &sFormSkelAnime);
    memcpy(player->skelAnime.jointTable, sFormJointTable, PLAYER_LIMB_MAX * sizeof(Vec3s));
    return done;
}

// Existing Garo form entry points
extern "C" FlexSkeletonHeader* GaroForm_LoadSkeleton(PlayState* play) {
    SkeletonHeader* hdr = ResourceMgr_LoadSkeletonByName(GARO_SKEL_PATH, NULL);
    if (hdr == NULL) {
        SPDLOG_WARN("[GaroForm] LoadSkeleton: NULL for {}", GARO_SKEL_PATH);
        return NULL;
    }
    return (FlexSkeletonHeader*)hdr;
}

// Forward decl of the rod-orb and reflect-escort init flags — the actual
// storage lives near the UpdateSwords helpers further down, but
// GaroForm_Cleanup needs to clear them before that block is reachable in TU
// order.
static u8 sRodOrbQuadsInited;
static u8 sReflectQuadInited;

extern "C" void GaroForm_Cleanup(void) {
    // Skin teardown handled by GaroSkin_Teardown — called by the engine on
    // scene transition via Play's heap reset.
    sGaroAttack = {};
    // Rod-orb AC quads are bound to the prior scene's Play* via
    // Collider_SetQuad. After a scene transition the parent Actor pointer
    // (player) and Play* are stale, so we re-init lazily on the next rod
    // fire. Without this reset, EnsureRodOrbQuads would early-return and
    // StampRodOrbQuad would write to a quad whose base->ac context is gone.
    sRodOrbQuadsInited = 0;
    // Same for the reflect escort — and sGaroAttack above already dropped the
    // Actor* it was following, which the old scene owned.
    sReflectQuadInited = 0;
}

// v9 — Death / Reset hooks
//
// GaroForm_OnDeath fires SYNCHRONOUSLY from TransformMasks_OnDeath BEFORE
// MmForm_OnDeath rolls back the form. Spawns 9 EffectSsDFire flame particles
// in a ring around the body (MM Garo Master death canon). The OOT death
// cutscene that follows still renders Link normally; the flames live in the
// effect system independent of Player_Draw so they remain visible.
//
// GaroForm_OnReset clears once-per-life flags. Invoked at:
//   - Ikana shield revival (z_player.c)
//   - Fairy revival (z_player.c)
//   - Scene reload / form change (MmForm_Reset)
#define GARO_DEATH_FLAME_COUNT 9
#define GARO_DEATH_FLAME_RADIUS 20.0f

extern "C" void GaroForm_OnDeath(Player* player, PlayState* play) {
    if (player == NULL || play == NULL)
        return;
    if (sGaroAttack.deathFlamesSpawned)
        return;

    Vec3f center = player->actor.world.pos;
    center.y += 5.0f;
    for (s32 i = 0; i < GARO_DEATH_FLAME_COUNT; i++) {
        f32 ang = (f32)i * ((f32)M_PI * 2.0f / (f32)GARO_DEATH_FLAME_COUNT);
        Vec3f pos = {
            center.x + cosf(ang) * GARO_DEATH_FLAME_RADIUS,
            center.y,
            center.z + sinf(ang) * GARO_DEATH_FLAME_RADIUS,
        };
        Vec3f vel = { cosf(ang) * 0.5f, 1.5f, sinf(ang) * 0.5f };
        Vec3f accel = { 0.0f, 0.1f, 0.0f };
        EffectSsDFire_Spawn(play, &pos, &vel, &accel, 100, 35, 255, 8, 12);
    }
    sGaroAttack.deathFlamesSpawned = 1;
    // Stal-family death sample doubles as the Garo collapse cry — the
    // dedicated MM Garo death voice lives in mm.o2r and will be wired
    // through TransformMasks_PlayMmVoice once samples ship.
    Audio_PlayActorSound2(&player->actor, NA_SE_EN_STAL_DEAD);
}

extern "C" void GaroForm_OnReset(void) {
    sGaroAttack.deathFlamesSpawned = 0;
    sGaroAttack.prevJumping = 0;
}

// Accessors for the rising-edge jump multiplier, used by MmForm_UpdateActive
// glass-cannon physics. The state lives inside sGaroAttack so it shares the
// same reset / cleanup lifecycle as the rest of the Garo combat machine.
extern "C" u8 GaroForm_GetPrevJumping(void) {
    return sGaroAttack.prevJumping;
}

extern "C" void GaroForm_SetPrevJumping(u8 v) {
    sGaroAttack.prevJumping = v;
}

// v10.4: true when Link currently has a NON-GRAB contextual A action pending
// (speak / check / read sign / open door / enter / climb / mount). Garo lets
// A through to vanilla in these cases so the player can still interact with
// the world; otherwise A is owned by the Garo moveset (dash / hops / shadow
// ball / jump-attack). GRAB is deliberately excluded — Garo can't lift
// objects, so a grabbable in range does NOT count as "vanilla wants A", and
// the A-strip in TransformMasks_FilterB suppresses the grab handler (which
// reads the same filtered input, sControlInput == the stripped sp44 copy).
// Called from both FilterB (pre-UpdateCommon, 1-frame-stale fields — fine for
// a "is an NPC/door in front of me" heuristic) and GaroForm_Update.
extern "C" u8 GaroForm_VanillaWantsAButton(Player* player) {
    if (player == NULL)
        return 0;
    if (player->doorType != PLAYER_DOORTYPE_NONE)
        return 1; // open / enter door
    if ((player->stateFlags2 & PLAYER_STATE2_CAN_ACCEPT_TALK_OFFER) && (player->talkActor != NULL))
        return 1; // speak / check / read
    if (player->stateFlags2 & PLAYER_STATE2_DO_ACTION_CLIMB)
        return 1; // climb wall / vine
    if (player->stateFlags2 & PLAYER_STATE2_DO_ACTION_ENTER)
        return 1; // enter crawlspace / transition
    return 0;
}

// Forward decl: query MmForm's current active form. Allows the Garo skin draw
// to fire when Garo is active as a proper MmForm transformation (not only via
// the legacy O2rLoader skin-swap path).
extern "C" MmPlayerTransformation MmForm_GetCurrentForm(void);
// Forward decl: Z-target predicate from z_player.c. Not in functions.h —
// every other mod file (item_rod_*.c, item_switchhook.c) externs it locally.
extern "C" int Player_IsZTargeting(Player* this_);

extern "C" s32 GaroForm_TryDrawSmoothSkin(PlayState* play, Player* player) {
    // Activation paths: (1) legacy O2rLoader skin swap, (2) full MmForm transformation.
    u8 garoActive = 0;
    const char* name = O2rLoader_GetForcedName();
    if (name && std::strcmp(name, "garo") == 0) {
        garoActive = 1;
    } else if (MmForm_GetCurrentForm() == MM_PLAYER_FORM_GARO) {
        garoActive = 1;
    }
    if (!garoActive)
        return 0;

    // v10: respect PLAYER_STATE2_DISABLE_DRAW for the Garo skin too. The
    // vanilla flag only suppresses Link's own draw path — our hybrid /
    // smooth-skin draw runs independently, so without this guard the
    // shadow-ball / banish "invisible" effect was a no-op visually.
    // Projectiles still draw (orbs, knives) because they're tied to
    // world state, not Link's visibility.
    if (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW) {
        extern void GaroForm_DrawProjectiles(PlayState * play);
        GaroForm_DrawProjectiles(play);
        return 1;
    }

    // Hybrid render path: 19-bone skeleton combining MM Garo upper body + OOT
    // Link adult lower body. Draws at player's world pos with own SkelAnime
    // running Garo native anims (gGaroIdleAnim by default). When the CVar
    // gGaroHybrid.AnimSource = 1, the hybrid jointTable is populated from
    // player->skelAnime instead so Link's vanilla anims drive the body.
    //
    // GaroSkin_Draw is the previous switchhook.glb-based path; kept as a
    // fallback if the hybrid skeleton fails to load (set CVar gGaroHybrid.
    // Disable = 1 to force the old path).
    if (CVarGetInteger("gGaroHybrid.Disable", 0) == 0) {
        GaroHybrid_Update(play, player);
        GaroHybrid_Draw(play, player);
    } else {
        GaroSkin_Draw(play, player);
    }

    // Draw any live sword projectiles in the same Garo draw pass — keeps the
    // z_player.c hook list small (one call instead of two).
    extern void GaroForm_DrawProjectiles(PlayState * play);
    GaroForm_DrawProjectiles(play);
    return 1;
}

// Garo activity check — true if Garo is active via either the legacy O2rLoader
// skin-swap path OR the full MmForm transformation pipeline.
static bool GaroForm_IsActive() {
    if (O2rLoader_HasActiveModel()) {
        const char* name = O2rLoader_GetForcedName();
        if (name != nullptr && std::strcmp(name, "garo") == 0)
            return true;
    }
    if (MmForm_GetCurrentForm() == MM_PLAYER_FORM_GARO)
        return true;
    return false;
}

// A Garo runs across water. Read by the water-walk gate the Roc Boots own
// (equip_roc_boots.c), so the pinning itself stays in the one place z_player.c
// already calls — this only says whether the form grants it.
extern "C" u8 GaroForm_WalksOnWater(void) {
    return GaroForm_IsActive() ? 1 : 0;
}

// A Garo sees through the world: hidden things show themselves and false ones
// stop pretending, for as long as the form lasts and without touching magic.
// Same deal as the water walk — the lens itself is driven by the passive-lens
// gate the Poe lantern owns (Lantern_UpdateLens), so there is exactly one place
// that decides whether actorCtx.lensActive is on for a reason other than the
// Lens of Truth item.
extern "C" u8 GaroForm_HasPassiveLens(void) {
    return GaroForm_IsActive() ? 1 : 0;
}

// Attack collider (spinning slash)
//
// Mirrors mm_form_combat.c:57-141 — that helper is static and not exported,
// so we replicate the geometry math inline. Calls public OOT collision API
// (Collider_SetQuadVertices, Collider_ResetQuadAT, CollisionCheck_SetAT).
static void GaroAttack_EnableSpinQuad(Player* player, PlayState* play) {
    ColliderQuad* quad = &player->meleeWeaponQuads[0];

    // v10: aligned with vanilla Link spin reach (researcher #1) — slightly
    // wider than the swing quad (sweeps around the body) but same forward
    // reach as Link's spin attack. Forward-extending rectangular slab in
    // front of the player; as shape.rot.y rotates each frame, the quad
    // sweeps the full 360° around Garo.
    const f32 nearDist = 10.0f;
    const f32 farDist = 60.0f;
    const f32 halfW = 35.0f;
    const f32 yBottom = 0.0f;
    const f32 yTop = 55.0f;

    f32 sinYaw = Math_SinS(player->actor.shape.rot.y);
    f32 cosYaw = Math_CosS(player->actor.shape.rot.y);
    f32 rightX = cosYaw;
    f32 rightZ = -sinYaw;

    Vec3f pos = player->actor.world.pos;

    f32 farCX = pos.x + sinYaw * farDist;
    f32 farCZ = pos.z + cosYaw * farDist;
    f32 nearCX = pos.x + sinYaw * nearDist;
    f32 nearCZ = pos.z + cosYaw * nearDist;

    Vec3f a, b, c, d;
    a.x = farCX - rightX * halfW;
    a.y = pos.y + yTop;
    a.z = farCZ - rightZ * halfW;
    b.x = farCX + rightX * halfW;
    b.y = pos.y + yTop;
    b.z = farCZ + rightZ * halfW;
    c.x = nearCX + rightX * halfW;
    c.y = pos.y + yBottom;
    c.z = nearCZ + rightZ * halfW;
    d.x = nearCX - rightX * halfW;
    d.y = pos.y + yBottom;
    d.z = nearCZ - rightZ * halfW;

    Collider_ResetQuadAT(play, &quad->base);
    Collider_SetQuadVertices(quad, &a, &b, &c, &d);

    quad->base.atFlags = AT_ON | AT_TYPE_PLAYER;
    // v10: DMG_SLASH_MASTER keeps the AT/AC vulnerability match working
    // (enemies are vulnerable to master sword). DMG_FIXED_DAMAGE makes the
    // collision system use toucher.damage verbatim — constant damage
    // regardless of enemy table or equipped sword class (v10.3 fix).
    quad->info.toucher.dmgFlags = DMG_SLASH_MASTER | DMG_FIXED_DAMAGE;
    quad->info.toucher.damage = GARO_SPIN_DAMAGE;
    quad->info.toucherFlags = TOUCH_ON | TOUCH_NEAREST;

    CollisionCheck_SetAT(play, &play->colChkCtx, &quad->base);
}

static void GaroAttack_DisableSpinQuad(Player* player) {
    player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
}

// Front sweep quad — for strikes that must reliably connect with whatever is
// standing IN FRONT of Garo (the shadow-ball finisher).
//
// The plain swing quad is a plane slanted from near-bottom to far-top: at any
// given distance it is one thin horizontal line, so an enemy whose bumper sits
// slightly above or below that line at that distance is simply missed — which
// is why the shadow-ball strike kept whiffing on the enemy it had just
// teleported behind. This one is a vertical rectangle PERPENDICULAR to the
// facing (full ±halfW wide, yBottom..yTop tall) that the caller marches
// outward one step per live frame, so over the active window it sweeps the
// whole volume in front of him instead of one line through it.
#define GARO_FRONT_SWEEP_HALF_W 45.0f
#define GARO_FRONT_SWEEP_Y_BOT -10.0f
#define GARO_FRONT_SWEEP_Y_TOP 75.0f
#define GARO_FRONT_SWEEP_NEAR 15.0f // distance of the first live frame
#define GARO_FRONT_SWEEP_STEP 12.0f // how far it marches out per frame

static void GaroAttack_EnableFrontSweepQuad(Player* player, PlayState* play, s16 liveFrame, u8 damage) {
    ColliderQuad* quad = &player->meleeWeaponQuads[0];

    if (liveFrame < 0)
        liveFrame = 0;
    f32 dist = GARO_FRONT_SWEEP_NEAR + (f32)liveFrame * GARO_FRONT_SWEEP_STEP;

    f32 sinYaw = Math_SinS(player->actor.shape.rot.y);
    f32 cosYaw = Math_CosS(player->actor.shape.rot.y);
    f32 rightX = cosYaw * GARO_FRONT_SWEEP_HALF_W;
    f32 rightZ = -sinYaw * GARO_FRONT_SWEEP_HALF_W;

    Vec3f pos = player->actor.world.pos;
    f32 cx = pos.x + sinYaw * dist;
    f32 cz = pos.z + cosYaw * dist;

    Vec3f a = { cx - rightX, pos.y + GARO_FRONT_SWEEP_Y_TOP, cz - rightZ };
    Vec3f b = { cx + rightX, pos.y + GARO_FRONT_SWEEP_Y_TOP, cz + rightZ };
    Vec3f c = { cx + rightX, pos.y + GARO_FRONT_SWEEP_Y_BOT, cz + rightZ };
    Vec3f d = { cx - rightX, pos.y + GARO_FRONT_SWEEP_Y_BOT, cz - rightZ };

    Collider_ResetQuadAT(play, &quad->base);
    Collider_SetQuadVertices(quad, &a, &b, &c, &d);

    quad->base.atFlags = AT_ON | AT_TYPE_PLAYER;
    quad->info.toucher.dmgFlags = DMG_SLASH_MASTER | DMG_FIXED_DAMAGE;
    quad->info.toucher.damage = damage;
    quad->info.toucherFlags = TOUCH_ON | TOUCH_NEAREST;

    CollisionCheck_SetAT(play, &play->colChkCtx, &quad->base);
}

// NOTE (v10.3): the old GaroAttack_ApplyAOEDirectDamage helper was removed.
// It set colChkInfo.health directly via Actor_ApplyDamage, which silently
// dropped HP WITHOUT triggering an enemy's AC-gated death/reaction routine —
// so enemies like Wolfos sat at 0 HP and only "died" when a later real
// sword hit set their AC_HIT flag (the bug: damage appeared sword-dependent).
// All Garo strikes now route through real AT quads (AC pipeline) carrying
// DMG_FIXED_DAMAGE for constant, equipment-independent damage.

// AOE quad for the landing strike.
//
// A ColliderQuad is a FLAT polygon: an enemy only registers when its bumper
// cylinder crosses the plane. The old version pinned that plane to the world X
// axis through Garo's position, which left the whole area in FRONT of him
// uncovered — after a forward leap the enemy you jumped at was exactly the one
// that survived. Now the slab is laid along Garo's FACING (front-to-back on
// entry, so the landing hit connects with whatever he leapt onto) and rotated
// ~30° per frame while the quad is live, so over the strike's ~6 active frames
// it sweeps a full 180° and covers every direction — the "big cylinder" the
// move was always supposed to be.
static void GaroAttack_EnableLandStrikeQuad(Player* player, PlayState* play) {
    ColliderQuad* quad = &player->meleeWeaponQuads[0];

    const f32 halfSize = 130.0f; // 260u total span
    const f32 yBottom = -20.0f;
    const f32 yTop = 130.0f;

    Vec3f pos = player->actor.world.pos;

    // Sweep yaw: FIRST LIVE frame = Garo's facing (covers dead ahead), +0x1555
    // (~30°) per frame after that. A line covers BOTH directions, so 180° of
    // sweep is full coverage. Counted from the frame the quad goes live, not
    // from state entry — otherwise the opening frame starts 12 steps into the
    // sweep and points behind him.
    s16 live = sGaroAttack.stateTimer - GARO_LAND_STRIKE_HIT_F;
    if (live < 0)
        live = 0;
    s16 sweepYaw = player->actor.shape.rot.y + (s16)(live * 0x1555);
    f32 dirX = Math_SinS(sweepYaw) * halfSize;
    f32 dirZ = Math_CosS(sweepYaw) * halfSize;

    Vec3f a = { pos.x + dirX, pos.y + yTop, pos.z + dirZ };
    Vec3f b = { pos.x - dirX, pos.y + yTop, pos.z - dirZ };
    Vec3f c = { pos.x - dirX, pos.y + yBottom, pos.z - dirZ };
    Vec3f d = { pos.x + dirX, pos.y + yBottom, pos.z + dirZ };

    Collider_ResetQuadAT(play, &quad->base);
    Collider_SetQuadVertices(quad, &a, &b, &c, &d);

    quad->base.atFlags = AT_ON | AT_TYPE_PLAYER;
    quad->info.toucher.dmgFlags = DMG_SLASH_MASTER | DMG_FIXED_DAMAGE;
    quad->info.toucher.damage = GARO_LAND_STRIKE_DAMAGE;
    quad->info.toucherFlags = TOUCH_ON | TOUCH_NEAREST;

    CollisionCheck_SetAT(play, &play->colChkCtx, &quad->base);
}

// Forward-facing slash quad, shared by every state that needs a plain "hit
// what's in front of Garo" box: the dash, the parry riposte and the banish
// strike. Same geometry as the spin quad, only narrower.
static void GaroAttack_EnableSwingQuad(Player* player, PlayState* play) {
    ColliderQuad* quad = &player->meleeWeaponQuads[0];

    // v10: aligned with vanilla Link sword-swing reach (researcher #1).
    // Vanilla uses bone-positioned quads (D_80854650) which we can't
    // mirror exactly in form-local space, but matching the rough
    // dimensions makes Garo's hits feel like Link's rather than the
    // earlier over-reaching box. 60u forward × 50u wide × 60u tall.
    const f32 nearDist = 10.0f;
    const f32 farDist = 60.0f;
    const f32 halfW = 25.0f;
    const f32 yBottom = 0.0f;
    const f32 yTop = 60.0f;

    f32 sinYaw = Math_SinS(player->actor.shape.rot.y);
    f32 cosYaw = Math_CosS(player->actor.shape.rot.y);
    f32 rightX = cosYaw;
    f32 rightZ = -sinYaw;

    Vec3f pos = player->actor.world.pos;
    f32 farCX = pos.x + sinYaw * farDist;
    f32 farCZ = pos.z + cosYaw * farDist;
    f32 nearCX = pos.x + sinYaw * nearDist;
    f32 nearCZ = pos.z + cosYaw * nearDist;

    Vec3f a, b, c, d;
    a.x = farCX - rightX * halfW;
    a.y = pos.y + yTop;
    a.z = farCZ - rightZ * halfW;
    b.x = farCX + rightX * halfW;
    b.y = pos.y + yTop;
    b.z = farCZ + rightZ * halfW;
    c.x = nearCX + rightX * halfW;
    c.y = pos.y + yBottom;
    c.z = nearCZ + rightZ * halfW;
    d.x = nearCX - rightX * halfW;
    d.y = pos.y + yBottom;
    d.z = nearCZ - rightZ * halfW;

    Collider_ResetQuadAT(play, &quad->base);
    Collider_SetQuadVertices(quad, &a, &b, &c, &d);

    quad->base.atFlags = AT_ON | AT_TYPE_PLAYER;
    // v10.3: DMG_SLASH_MASTER keeps the AT/AC vulnerability match (enemies
    // are vulnerable to master sword). DMG_FIXED_DAMAGE makes the collision
    // system use toucher.damage verbatim — CONSTANT damage independent of
    // the enemy damage table AND of Link's equipped sword class. Damage value
    // is overwritten per-state by the caller (parry=4, dash=4, shadow_ball=8).
    quad->info.toucher.dmgFlags = DMG_SLASH_MASTER | DMG_FIXED_DAMAGE;
    quad->info.toucher.damage = GARO_SWING_DAMAGE;
    quad->info.toucherFlags = TOUCH_ON | TOUCH_NEAREST;

    CollisionCheck_SetAT(play, &play->colChkCtx, &quad->base);
}

// Rod orb projectiles
static Vec3f GaroAttack_HandOrigin(Player* player) {
    Vec3f origin = player->leftHandPos;
    if (origin.y == 0.0f) {
        // Fallback if hand pos not populated yet (very first frame).
        origin = player->actor.world.pos;
        origin.y += 30.0f;
    }
    return origin;
}

static void GaroAttack_SpawnOne(GaroOrb src, Player* player) {
    for (s32 slot = 0; slot < GARO_ORB_POOL_MAX; slot++) {
        GaroOrb* sw = &sGaroAttack.orbs[slot];
        if (!sw->active) {
            *sw = src;
            sw->active = 1;
            return;
        }
    }
}

// ── v9 rod orb projectile ───────────────────────────────────────────────
// Fired when B is released from GARO_ROD_AIM. Damage tier and element flag
// are passed in from the release path (see ROD_AIM state handler). The orb
// travels forward from the player's left hand at GARO_ROD_ORB_SPEED and dies
// on its first AC bumper hit OR when its lifetime expires. The
// DMG_SLASH_MASTER flag is OR'd in at AC time as a fallback for
// restrictive-AC enemies that don't accept arrow flags (Like-Like,
// Iron Knuckle), so the orb still hits them.
#define GARO_ROD_ORB_SPEED 12.0f
#define GARO_ROD_ORB_LIFETIME 60
// The level-3 ball covers half that ground before it breaks. Its damage lives
// in the fragments, so it is meant to open up near the fight rather than sail
// across the room first — and the burst reads better close enough to see.
#define GARO_ROD_BURST_LIFETIME (GARO_ROD_ORB_LIFETIME / 2)

static void GaroAttack_SpawnRodOrb(Player* player, u8 element, u8 damage, u32 dmgFlag, s16 yaw, s16 pitch, u8 bursts) {
    GaroOrb tmp = {};
    tmp.pos = GaroAttack_HandOrigin(player);
    tmp.yaw = yaw;     // v10.6 first-person aim yaw (focus.rot.y)
    tmp.pitch = pitch; // v10.6 first-person aim pitch (focus.rot.x)
    tmp.element = element;
    tmp.damage = damage;
    tmp.dmgFlag = dmgFlag;
    tmp.timer = bursts ? GARO_ROD_BURST_LIFETIME : GARO_ROD_ORB_LIFETIME;
    tmp.bursts = bursts;
    // Leave at the size it was charged to. Floors guard against a release on
    // the very first frames, before the eased scales have grown into anything.
    tmp.ballCircle = (sGaroAttack.rodBallCircle > GARO_ROD_BALL_CIRCLE_MAX * 0.35f) ? sGaroAttack.rodBallCircle
                                                                                    : GARO_ROD_BALL_CIRCLE_MAX * 0.35f;
    tmp.ballScale = (sGaroAttack.rodBallScale > GARO_ROD_BALL_SCALE_MAX * 0.35f) ? sGaroAttack.rodBallScale
                                                                                 : GARO_ROD_BALL_SCALE_MAX * 0.35f;
    GaroAttack_SpawnOne(tmp, player);
}

// v10.11: true while the rod charge-aim owns the borrowed slingshot pipeline.
// mm_player_form.cpp reads this to skip nulling heldItemAction (which would
// break the aim, since the aim sets heldItemAction = SLINGSHOT).
extern "C" u8 GaroForm_IsRodAiming(void) {
    return (sGaroAttack.state == GARO_ROD_AIM) ? 1 : 0;
}

// v10.11: fire one rod orb. Called from the GARO_ROD_AIM handler the frame B is
// released — NOT from the slingshot's own fire path, which never progressed its
// bow-draw counter for Garo. The aim direction is already in focus.rot because
// the borrowed slingshot aim (Player_StartDekuBubble) owns it. Damage tier +
// ball scale come from rodChargeTimer; element from the L/R cycle.
// (L/R cycle). Consumes our own magic. Charge resets after so the player can
// hold-charge-release again (rapid-fire), like the Deku bubble.
extern "C" void GaroForm_FireRodOrb(Player* player, PlayState* play) {
    u8 element = sGaroAttack.rodElement;
    u8 level = GaroAttack_GetRodLevel(sGaroAttack.rodChargeTimer);
    u32 dmgFlag = GaroAttack_GetRodDmgFlag(element);
    s16 aimYaw = player->actor.focus.rot.y; // set by the slingshot aim
    s16 aimPitch = player->actor.focus.rot.x;

    // Level 1: released too early. The ball had not formed, so nothing leaves
    // the hand and no magic is spent — just the dry click of a wasted draw.
    if (level < 2) {
        Audio_PlayActorSound2(&player->actor, NA_SE_IT_BOW_FLICK);
        sGaroAttack.rodChargeTimer = 0;
        sGaroAttack.rodSfxPlayed = 0;
        return;
    }

    // Both levels fire ONE ball. What separates them is what happens when it
    // lands: level 3's breaks apart into seekers.
    u8 burst = (level >= 3) ? 1 : 0;
    u8 dmg = burst ? GARO_ROD_L3_DAMAGE : GARO_ROD_L2_DAMAGE;

    // Own magic; the breaking shot costs double, since the fragments are free
    // damage afterwards. Out of magic → it still flies, for a token 1 damage.
    s16 cost = (s16)(GARO_ROD_MAGIC_COST * (burst ? 2 : 1));
    if (gSaveContext.magic >= cost) {
        gSaveContext.magic -= cost;
    } else {
        dmg = 1;
    }

    GaroAttack_SpawnRodOrb(player, element, dmg, dmgFlag, aimYaw, aimPitch, burst);

    // Release: the elemental-arrow shot pair — the bow twang plus the magic
    // arrow's own launch sting, so a rod shot sounds like the magic arrow it
    // behaves like (it even carries the DMG_ARROW_* flags).
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_ARROW_SHOT);
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_MAGIC_ARROW_SHOT);

    // Reset charge for the next shot (stay in aim, rapid-fire like Deku).
    sGaroAttack.rodChargeTimer = 0;
    sGaroAttack.rodSfxPlayed = 0;
}

// ── v9 rod orb AC quad pool ─────────────────────────────────────────────
// One quad per sword slot so multiple in-flight orbs can independently
// register damage on the same frame. Init is lazy: the first rod orb
// triggers GaroAttack_EnsureRodOrbQuads which configures all 12 quads at
// once. The quads stay valid for the lifetime of the scene; reset on scene
// reload via GaroForm_Cleanup (sGaroAttack zeroing) — although the
// ColliderQuad internals are pointer-free so survival across reloads is
// harmless.
//
// The dmgFlags mask 0xFFCFFFFF is the canonical "accepts everything except
// reflection" pattern used by Link's sword quad — combined with per-orb
// the orb dmgFlag at SetAT time, this lets enemies with restrictive AC masks
// (Iron Knuckle, Like-Like) still take the hit while element-vulnerable
// bosses (Phantom Ganon, Ganon2) get routed to their light/fire/ice paths.
static ColliderQuad sRodOrbQuads[GARO_ORB_POOL_MAX];
// sRodOrbQuadsInited is forward-declared near GaroForm_Cleanup so the
// cleanup hook can reset it without re-ordering this block.

static ColliderQuadInit sRodOrbQuadInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_TYPE_PLAYER,
        COLSHAPE_QUAD,
    },
    {
        ELEMTYPE_UNK0,
        { 0xFFCFFFFF, 0x00, 0x10 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    // ColliderQuadDimInit wraps a Vec3f quad[4], so the literal needs THREE
    // brace layers: struct → array → per-Vec3f. (Matches z_en_boom.c:52.)
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static void GaroAttack_EnsureRodOrbQuads(PlayState* play, Player* player) {
    if (sRodOrbQuadsInited)
        return;
    for (s32 i = 0; i < GARO_ORB_POOL_MAX; i++) {
        Collider_InitQuad(play, &sRodOrbQuads[i]);
        Collider_SetQuad(play, &sRodOrbQuads[i], &player->actor, &sRodOrbQuadInit);
    }
    sRodOrbQuadsInited = 1;
}

static void GaroAttack_StampRodOrbQuad(PlayState* play, Player* player, GaroOrb* sw, s32 quadIdx) {
    ColliderQuad* quad = &sRodOrbQuads[quadIdx];

    // ~12-unit cube around the orb's current pos. Symmetric so the orb hits
    // enemies from any approach angle equally — element semantics are about
    // weakness routing, not directional contact.
    const f32 half = 8.0f;
    Vec3f a = { sw->pos.x - half, sw->pos.y + half, sw->pos.z };
    Vec3f b = { sw->pos.x + half, sw->pos.y + half, sw->pos.z };
    Vec3f c = { sw->pos.x + half, sw->pos.y - half, sw->pos.z };
    Vec3f d = { sw->pos.x - half, sw->pos.y - half, sw->pos.z };

    Collider_ResetQuadAT(play, &quad->base);
    Collider_SetQuadVertices(quad, &a, &b, &c, &d);

    quad->base.atFlags = AT_ON | AT_TYPE_PLAYER;
    // Combine element flag with DMG_SLASH_MASTER so restrictive-AC enemies
    // (those that only accept weapon flags, not arrow flags) still take the
    // hit. Element-vulnerable enemies route via the matching arrow bit.
    quad->info.toucher.dmgFlags = sw->dmgFlag | DMG_SLASH_MASTER;
    quad->info.toucher.damage = sw->damage;
    quad->info.toucherFlags = TOUCH_ON | TOUCH_NEAREST;

    CollisionCheck_SetAT(play, &play->colChkCtx, &quad->base);
}

// Nearest enemy to `origin` that is not already in `taken`. The claim list is
// what makes the fragments split up instead of dogpiling: same rule as the
// Trident's Tcb_NearestUntaken.
static Actor* GaroAttack_NearestUntaken(PlayState* play, Vec3f* origin, Actor** taken, s32 nTaken) {
    Actor* best = NULL;
    f32 bestDistSq = GARO_ORB_HOME_RANGE * GARO_ORB_HOME_RANGE;

    for (Actor* enemy = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; enemy != NULL; enemy = enemy->next) {
        if (enemy->update == NULL) {
            continue;
        }
        s32 claimed = 0;
        for (s32 i = 0; i < nTaken; i++) {
            if (taken[i] == enemy) {
                claimed = 1;
                break;
            }
        }
        if (claimed) {
            continue;
        }
        f32 dx = enemy->world.pos.x - origin->x;
        f32 dy = enemy->world.pos.y - origin->y;
        f32 dz = enemy->world.pos.z - origin->z;
        f32 distSq = dx * dx + dy * dy + dz * dz;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            best = enemy;
        }
    }
    return best;
}

// The level-3 ball breaking up: Ganondorf's impact signature (a shock plus a
// spray of light balls) and then GARO_ORB_SEEKERS fragments fanned outward,
// each claiming a different nearby enemy. With fewer enemies than fragments the
// claim list is wiped and the sweep starts over, so the spares double up on the
// closest ones rather than flying off at nothing — the Trident does exactly
// this in Tcb_SpawnHunters.
static void GaroAttack_BurstRodOrb(PlayState* play, GaroOrb* src) {
    Vec3f pos = src->pos;
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    u8 e = src->element % GARO_ROD_ELEMENT_COUNT;
    static const u8 sBurstBallColor[GARO_ROD_ELEMENT_COUNT] = { 2, 1, 7, 5, 3, 0 };

    EffectSsFhgFlash_SpawnShock(play, NULL, &pos, 200, 0 /* FHGFLASH_SHOCK_NO_ACTOR */);
    for (s32 i = 0; i < GARO_ORB_BURST_BALLS; i++) {
        Vec3f vel = { Rand_CenteredFloat(12.0f), Rand_ZeroFloat(8.0f) + 2.0f, Rand_CenteredFloat(12.0f) };
        EffectSsFhgFlash_SpawnLightBall(play, &pos, &vel, &zero, (s16)(Rand_ZeroOne() * 60.0f) + 110,
                                        sBurstBallColor[e]);
    }
    Audio_PlaySoundGeneral(NA_SE_IT_MAGIC_ARROW_SHOT, &pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);

    Actor* taken[GARO_ORB_SEEKERS];
    s32 nTaken = 0;
    for (s32 i = 0; i < GARO_ORB_SEEKERS; i++) {
        Actor* target = GaroAttack_NearestUntaken(play, &pos, taken, nTaken);
        if ((target == NULL) && (nTaken > 0)) {
            nTaken = 0; // ran out of fresh enemies: go round again
            target = GaroAttack_NearestUntaken(play, &pos, taken, nTaken);
        }

        GaroOrb frag = {};
        frag.pos = pos;
        // Fan them around the burst so they visibly disperse before turning in.
        frag.yaw = (s16)(src->yaw + (s16)(i * GARO_ORB_SEEKER_FAN) - GARO_ORB_SEEKER_FAN);
        frag.pitch = (s16)(src->pitch - 0x0800); // a touch upward, so they arc
        frag.element = src->element;
        frag.damage = GARO_ORB_SEEKER_DAMAGE;
        frag.dmgFlag = src->dmgFlag;
        frag.timer = GARO_ORB_SEEKER_LIFETIME;
        frag.isSeeker = 1;
        frag.target = target;
        // Prime the streak at the burst point: left zeroed, the first dozen
        // frames would draw a ribbon reaching back to the world origin.
        f32 fragCosP = Math_CosS(frag.pitch);
        f32 headY = atan2f(Math_SinS(frag.yaw) * fragCosP, Math_CosS(frag.yaw) * fragCosP);
        f32 headX = atan2f(-Math_SinS(frag.pitch), fragCosP);
        for (s32 t = 0; t < GARO_ORB_TRAIL_LEN; t++) {
            frag.trailPos[t] = pos;
            frag.trailRot[t].x = headX;
            frag.trailRot[t].y = headY;
            frag.trailRot[t].z = 0.0f;
        }
        GaroAttack_SpawnOne(frag, GET_PLAYER(play));

        if (target != NULL) {
            taken[nTaken++] = target;
        }
    }
}

// The seeking half of an orb: find the nearest enemy that is AHEAD of it and
// inside range, and bend the orb's yaw/pitch toward it. Returns without
// touching the angles when nothing qualifies, which is what makes an orb with
// no target fly dead straight.
static void GaroAttack_HomeRodOrb(PlayState* play, GaroOrb* sw) {
    f32 fwdX = Math_SinS(sw->yaw) * Math_CosS(sw->pitch);
    f32 fwdY = -Math_SinS(sw->pitch);
    f32 fwdZ = Math_CosS(sw->yaw) * Math_CosS(sw->pitch);

    Actor* best = NULL;
    f32 bestDistSq = GARO_ORB_HOME_RANGE * GARO_ORB_HOME_RANGE;

    // A fragment keeps the enemy it claimed at burst time, and ignores the
    // ahead-of-me cone so it can wheel right around onto it. It drops the claim
    // if that enemy dies, and hunts freely from then on.
    if (sw->isSeeker) {
        if ((sw->target != NULL) && (sw->target->update != NULL)) {
            Vec3f claimed = { sw->target->world.pos.x, sw->target->world.pos.y + sw->target->shape.yOffset,
                              sw->target->world.pos.z };
            Math_ScaledStepToS(&sw->yaw, Math_Vec3f_Yaw(&sw->pos, &claimed), GARO_ORB_SEEKER_TURN);
            Math_ScaledStepToS(&sw->pitch, Math_Vec3f_Pitch(&sw->pos, &claimed), GARO_ORB_SEEKER_TURN);
            return;
        }
        sw->target = NULL;
    }

    for (Actor* enemy = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; enemy != NULL; enemy = enemy->next) {
        if (enemy->update == NULL) {
            continue;
        }
        f32 dx = enemy->world.pos.x - sw->pos.x;
        f32 dy = (enemy->world.pos.y + enemy->shape.yOffset) - sw->pos.y;
        f32 dz = enemy->world.pos.z - sw->pos.z;
        f32 distSq = dx * dx + dy * dy + dz * dz;
        if (distSq >= bestDistSq || distSq < 1.0f) {
            continue;
        }
        // Ahead-of-me test, so an orb never turns around and chases something
        // it has already flown past.
        f32 dist = sqrtf(distSq);
        if (((dx * fwdX + dy * fwdY + dz * fwdZ) / dist) < GARO_ORB_HOME_CONE) {
            continue;
        }
        bestDistSq = distSq;
        best = enemy;
    }

    if (best == NULL) {
        return;
    }

    // Engine helpers rather than hand-rolled atan2 calls: Math_Atan2S takes its
    // arguments in an order that is easy to get backwards (yaw is (dz, dx),
    // pitch is (distXZ, -dy)), and these two encode it correctly.
    Vec3f aimAt = { best->world.pos.x, best->world.pos.y + best->shape.yOffset, best->world.pos.z };
    s16 turn = sw->isSeeker ? GARO_ORB_SEEKER_TURN : GARO_ORB_TURN_RATE;
    Math_ScaledStepToS(&sw->yaw, Math_Vec3f_Yaw(&sw->pos, &aimAt), turn);
    Math_ScaledStepToS(&sw->pitch, Math_Vec3f_Pitch(&sw->pos, &aimAt), turn);
}

// Red ice checks WHO hit it, not what the hit carried: it melts for an actor
// whose id is EN_ICE_HONO (blue fire) or an EN_ARROW with an ARROW_ICE child
// (z_bg_ice_shelter.c). Garo's orbs are not actors at all — their AT quads
// belong to the Player — so no damage flag can ever satisfy that test. This is
// the same wall SW97's ice arrow hits, and the same way out: call the actor's
// own public melt directly, exactly as ArrowIce_MeltIceShelters, MagicIce and
// the Ice Rod do. Nothing in ovl_Bg_Ice_Shelter changes.
//
// The other two interactions need no code at all, because those actors DO test
// the damage flags: torches accept 0x20820, which includes DMG_ARROW_FIRE
// (z_obj_syokudai.c:172), and sun switches accept 0x00202000, which includes
// DMG_ARROW_LIGHT (z_obj_lightswitch.c bumper) — both already ride on the orb
// quad from GaroAttack_GetRodDmgFlag.
extern "C" void BgIceShelter_MeltInstantly(Actor* thisx, PlayState* play);

#define GARO_ORB_MELT_RADIUS 60.0f

static void GaroAttack_ApplyOrbElementEffects(PlayState* play, GaroOrb* sw) {
    if ((sw->element % GARO_ROD_ELEMENT_COUNT) != GARO_ELEM_ICE) {
        return;
    }
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_BG].head; actor != NULL; actor = actor->next) {
        if ((actor->id != ACTOR_BG_ICE_SHELTER) || (actor->update == NULL)) {
            continue;
        }
        f32 dx = actor->world.pos.x - sw->pos.x;
        f32 dz = actor->world.pos.z - sw->pos.z;
        if (sqrtf(dx * dx + dz * dz) < GARO_ORB_MELT_RADIUS) {
            BgIceShelter_MeltInstantly(actor, play);
        }
    }
}

// ── Reflected shot ──────────────────────────────────────────────────────
// A projectile the guard sent back. The actor keeps flying and keeps its own
// look; what makes it hurt on the way back is this escort — an invisible
// sword-damage quad stamped on it every frame. Its OWN collider is left alone
// (it belongs to that actor and there is no generic way to reach it), which is
// why Garo takes i-frames on the reflect: the shot starts inside him and would
// otherwise clip him once on its way out.
static ColliderQuad sReflectQuad;
// sReflectQuadInited is forward-declared next to GaroForm_Cleanup, same as the
// rod-orb flag, so the cleanup hook can clear it.

static ColliderQuadInit sReflectQuadInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_TYPE_PLAYER,
        COLSHAPE_QUAD,
    },
    {
        ELEMTYPE_UNK0,
        { 0xFFCFFFFF, 0x00, 0x10 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

// Ranged or melee? There is no engine flag that says "projectile", so this
// leans on the one thing every thrown/shot actor has in common and almost no
// melee attacker does: you cannot lock onto it. The health test that used to
// ride along with it is gone — plenty of projectiles carry a health value they
// never use, and a single silent rejection here is enough to make the whole
// reflect look broken. Whether it is really an attack is settled by the sweep
// that calls this: close, fast, and heading at him.
static bool GaroAttack_IsRangedAttacker(Actor* attacker) {
    if (attacker == NULL) {
        return false;
    }
    if (attacker->category == ACTORCAT_EXPLOSIVE) {
        return true;
    }
    return !(attacker->flags & ACTOR_FLAG_ATTENTION_ENABLED);
}

// Send the shot back where it came from and start the escort.
static void GaroAttack_ReflectShot(PlayState* play, Player* player, Actor* shot) {
    // Flip BOTH the heading and the raw velocity: some projectiles are moved by
    // Actor_MoveForward along world.rot.y, others integrate velocity directly,
    // and there is no telling which one this is.
    shot->world.rot.y += 0x8000;
    shot->shape.rot.y = shot->world.rot.y;
    shot->velocity.x = -shot->velocity.x;
    shot->velocity.z = -shot->velocity.z;
    if (shot->speedXZ < GARO_REFLECT_MIN_SPEED) {
        shot->speedXZ = GARO_REFLECT_MIN_SPEED;
    }
    // Shove it clear of Garo along its new heading. Projectiles typically kill
    // themselves the moment their collider touches anything — including him —
    // so a shot bounced while still overlapping him would simply pop instead of
    // flying back.
    shot->world.pos.x += Math_SinS(shot->world.rot.y) * GARO_REFLECT_PUSH_OUT;
    shot->world.pos.z += Math_CosS(shot->world.rot.y) * GARO_REFLECT_PUSH_OUT;

    SPDLOG_INFO("[Garo] reflect shot id=0x{:X} cat={} speed={}", (u32)shot->id, (s32)shot->category, shot->speedXZ);

    sGaroAttack.reflectShot = shot;
    sGaroAttack.reflectTimer = GARO_REFLECT_FRAMES;
    player->invincibilityTimer = GARO_REFLECT_INVULN;

    Audio_PlaySoundGeneral(NA_SE_IT_SHIELD_REFLECT_SW, &player->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

// Bounce a shot BEFORE it lands. This is the only reliable moment: most
// projectiles destroy themselves the instant they connect (the Octorok rock
// does exactly that in EnOkuta_ProjectileFly unless a shield sets AT_BOUNCED),
// so by the time the damage shows up in the player's AC there is nothing left
// to send back — the counter then resolved to the shooter instead, and Garo
// teleported across the room to sword-swing an Octorok. Catching the shot in
// flight fixes both halves of that.
static u8 GaroAttack_TryReflectIncoming(PlayState* play, Player* player) {
    Vec3f chest = player->actor.world.pos;
    chest.y += GARO_GUARD_CATCH_HEIGHT;

    // Every category, not a hand-picked few: a projectile can be re-categorised
    // by its own init (the Octorok's rock moves itself to PROP), and one guess
    // wrong here looks exactly like the feature not existing. It is one sweep
    // per frame and only while guarding.
    for (s32 c = 0; c < ACTORCAT_MAX; c++) {
        if ((c == ACTORCAT_PLAYER) || (c == ACTORCAT_BG) || (c == ACTORCAT_DOOR) || (c == ACTORCAT_CHEST)) {
            continue;
        }
        for (Actor* actor = play->actorCtx.actorLists[c].head; actor != NULL; actor = actor->next) {
            if ((actor->update == NULL) || (actor == sGaroAttack.reflectShot)) {
                continue;
            }
            if (!GaroAttack_IsRangedAttacker(actor)) {
                continue;
            }
            f32 dx = chest.x - actor->world.pos.x;
            f32 dy = chest.y - actor->world.pos.y;
            f32 dz = chest.z - actor->world.pos.z;
            if ((dx * dx + dy * dy + dz * dz) > (GARO_GUARD_CATCH_RADIUS * GARO_GUARD_CATCH_RADIUS)) {
                continue;
            }
            // Only things actually coming AT him: a shot already leaving, or a
            // prop just sitting there, is not an attack to return. Both ways of
            // moving are summed because some actors drive velocity directly and
            // others ride speedXZ along world.rot.y.
            f32 velX = actor->velocity.x + Math_SinS(actor->world.rot.y) * actor->speedXZ;
            f32 velZ = actor->velocity.z + Math_CosS(actor->world.rot.y) * actor->speedXZ;
            f32 speedSq = (velX * velX) + (velZ * velZ);
            f32 approach = (velX * dx) + (velZ * dz);

            // Diagnostic while the reflect is being dialled in: every fourth
            // frame, report what is inside the catch zone and why it was or was
            // not taken. Rate-gated on purpose — an unthrottled per-frame log
            // in a room full of props drowns the file and hides the answer.
            if ((play->gameplayFrames & 3) == 0) {
                SPDLOG_INFO("[Garo] guard sees id=0x{:X} cat={} dist={} speed={} approach={}", (u32)actor->id, c,
                            sqrtf(dx * dx + dy * dy + dz * dz), sqrtf(speedSq), approach);
            }

            if (speedSq < (GARO_GUARD_CATCH_MIN_SPEED * GARO_GUARD_CATCH_MIN_SPEED)) {
                continue;
            }
            if (approach <= 0.0f) {
                continue;
            }
            GaroAttack_ReflectShot(play, player, actor);
            return 1;
        }
    }
    return 0;
}

static void GaroAttack_UpdateReflect(PlayState* play, Player* player) {
    if (sGaroAttack.reflectTimer <= 0) {
        return;
    }
    sGaroAttack.reflectTimer--;

    Actor* shot = sGaroAttack.reflectShot;
    if ((shot == NULL) || (shot->update == NULL)) {
        sGaroAttack.reflectShot = NULL;
        sGaroAttack.reflectTimer = 0;
        return;
    }

    if (!sReflectQuadInited) {
        Collider_InitQuad(play, &sReflectQuad);
        Collider_SetQuad(play, &sReflectQuad, &player->actor, &sReflectQuadInit);
        sReflectQuadInited = 1;
    }

    const f32 half = GARO_REFLECT_HALF;
    Vec3f a = { shot->world.pos.x - half, shot->world.pos.y + half, shot->world.pos.z };
    Vec3f b = { shot->world.pos.x + half, shot->world.pos.y + half, shot->world.pos.z };
    Vec3f c = { shot->world.pos.x + half, shot->world.pos.y - half, shot->world.pos.z };
    Vec3f d = { shot->world.pos.x - half, shot->world.pos.y - half, shot->world.pos.z };

    Collider_ResetQuadAT(play, &sReflectQuad.base);
    Collider_SetQuadVertices(&sReflectQuad, &a, &b, &c, &d);
    sReflectQuad.base.atFlags = AT_ON | AT_TYPE_PLAYER;
    sReflectQuad.info.toucher.dmgFlags = DMG_SLASH_MASTER | DMG_FIXED_DAMAGE;
    sReflectQuad.info.toucher.damage = GARO_REFLECT_DAMAGE;
    sReflectQuad.info.toucherFlags = TOUCH_ON | TOUCH_NEAREST;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sReflectQuad.base);
}

static void GaroAttack_UpdateSwords(PlayState* play) {
    Player* player = GET_PLAYER(play);

    for (s32 i = 0; i < GARO_ORB_POOL_MAX; i++) {
        GaroOrb* sw = &sGaroAttack.orbs[i];
        if (!sw->active)
            continue;

        // Did last frame's quad land? The level-3 ball breaks on contact; a
        // plain shot keeps going (piercing), which is the old behaviour.
        if (sRodOrbQuadsInited && (sRodOrbQuads[i].base.atFlags & AT_HIT)) {
            sRodOrbQuads[i].base.atFlags &= ~AT_HIT;
            if (sw->bursts) {
                GaroAttack_BurstRodOrb(play, sw);
                sw->active = 0;
                continue;
            }
        }

        GaroAttack_HomeRodOrb(play, sw);

        // Rod orbs travel in 3D along their current heading (yaw + pitch),
        // mirroring Actor_SetProjectileSpeed:
        //   speedXZ      = speed * cos(pitch)
        //   velocity.y   = speed * -sin(pitch)
        f32 speed = sw->isSeeker ? GARO_ORB_SEEKER_SPEED : GARO_ROD_ORB_SPEED;
        f32 cosP = Math_CosS(sw->pitch);
        f32 velX = Math_SinS(sw->yaw) * cosP * speed;
        f32 velZ = Math_CosS(sw->yaw) * cosP * speed;
        f32 velY = -Math_SinS(sw->pitch) * speed;
        sw->pos.x += velX;
        sw->pos.z += velZ;
        sw->pos.y += velY;

        // Fragments record their path for the streak, sampled AFTER the move
        // and paired with the heading they moved on — trident_charge_ball.c:477
        // does exactly this, and it is what orients each ribbon segment.
        if (sw->isSeeker) {
            sw->trailIdx++;
            if (sw->trailIdx >= GARO_ORB_TRAIL_LEN) {
                sw->trailIdx = 0;
            }
            sw->trailPos[sw->trailIdx] = sw->pos;
            sw->trailRot[sw->trailIdx].y = atan2f(velX, velZ);
            sw->trailRot[sw->trailIdx].x = atan2f(velY, sqrtf(velX * velX + velZ * velZ));
            sw->trailRot[sw->trailIdx].z = 0.0f;
        }

        // Elemental world interactions — red ice, torches, sun switches.
        GaroAttack_ApplyOrbElementEffects(play, sw);

        // The Trident's small trail, verbatim: one FhgFlash light ball dropped
        // at the orb every 4th frame (trident_charge_ball.c:465), which the
        // effect system then fades and shrinks on its own. Colour picked per
        // element from the same FHGFLASH_LIGHTBALL_* palette the Trident picks
        // its purple and blue from.
        if ((sw->timer & 3) == 0) {
            // Numeric like the Trident's own TCB_FX_LIGHTBALL_* defines: the
            // FHGFLASH_LIGHTBALL_* enum lives in the effect overlay's private
            // header, which no mod TU includes.
            static const u8 sOrbWakeColor[GARO_ROD_ELEMENT_COUNT] = {
                2, // fire  — FHGFLASH_LIGHTBALL_RED
                1, // ice   — FHGFLASH_LIGHTBALL_LIGHTBLUE
                7, // light — FHGFLASH_LIGHTBALL_WHITE1
                5, // dark  — FHGFLASH_LIGHTBALL_PURPLE
                3, // soul  — FHGFLASH_LIGHTBALL_YELLOW
                0, // wind  — FHGFLASH_LIGHTBALL_GREEN
            };
            Vec3f wakePos = sw->pos;
            Vec3f zero = { 0.0f, 0.0f, 0.0f };
            EffectSsFhgFlash_SpawnLightBall(play, &wakePos, &zero, &zero, GARO_ORB_WAKE_SCALE,
                                            sOrbWakeColor[sw->element % GARO_ROD_ELEMENT_COUNT]);
        }

        sw->timer--;
        if (sw->timer <= 0) {
            // A level-3 ball that reaches the end of its flight without hitting
            // anything still breaks — the fragments are the point of the shot,
            // not a reward for connecting.
            if (sw->bursts) {
                GaroAttack_BurstRodOrb(play, sw);
            }
            sw->active = 0;
            continue;
        }

        // Route through the AC bumper system with proper element dmgFlags so
        // boss vulnerability masks accept the hit. Orbs persist for their full
        // lifetime (piercing semantics) — a fire orb sweeping through a row of
        // enemies is intended.
        GaroAttack_EnsureRodOrbQuads(play, player);
        GaroAttack_StampRodOrbQuad(play, player, sw, i);
    }
}

// v10.5 magic charge-ball visual. Uses the OOT-native "light orb" DLs
// (ovl_Boss_Ganon2) — the same glowing-sphere material+model the boss
// super-damage FHG flash already renders, so they're guaranteed present
// (no mm.o2r dependency) and proven in this TU. Passed to gSPDisplayList
// as OTR path strings cast to Gfx*; SoH resolves them by DL signature.
//   - Material DL: binds the I8 glow texture + (PRIM-ENV)*TEXEL+ENV combiner
//     + soft additive render mode. PRIM = bright core, ENV = surrounding glow.
//   - Model DL: a ~14-unit centered billboard quad.
#define GARO_ORB_MATERIAL_DL "__OTR__overlays/ovl_Boss_Ganon2/gGanonLightOrbMaterialDL"
#define GARO_ORB_MODEL_DL "__OTR__overlays/ovl_Boss_Ganon2/gGanonLightOrbModelDL"

// Per-element {R,G,B}, indexed by GARO_ELEM_*. prim/env are lifted VERBATIM
// from the six SW97 arrows' draws (z_arrow_fire/ice/light/dark/soul/wind
// .inc.c), so a Garo orb and a SW97 arrow of the same element are the same
// colour.
static const u8 sRodOrbPrim[GARO_ROD_ELEMENT_COUNT][3] = {
    { 255, 200, 0 },   // fire  — z_arrow_fire.inc.c:437
    { 170, 255, 255 }, // ice   — z_arrow_ice.inc.c:456
    { 255, 255, 255 }, // light — z_arrow_light.inc.c:431
    { 0, 0, 0 },       // dark  — z_arrow_dark.inc.c:431
    { 255, 255, 170 }, // soul  — z_arrow_soul.inc.c:473
    { 170, 255, 255 }, // wind  — z_arrow_wind.inc.c:543
};
static const u8 sRodOrbEnv[GARO_ROD_ELEMENT_COUNT][3] = {
    { 255, 0, 0 },     // fire
    { 0, 0, 255 },     // ice
    { 170, 170, 170 }, // light
    { 0, 0, 0 },       // dark
    { 255, 255, 0 },   // soul
    { 0, 255, 0 },     // wind
};
// Dense inner core, drawn with alpha blending instead of additive glow (see
// GaroForm_DrawLayeredOrb) — this is what gives the ball a solid middle, the
// same trick the banish shadow ball uses. Not from SW97: the arrows have no
// core layer, so these are the saturated, dark reading of each element's env.
static const u8 sRodOrbCore[GARO_ROD_ELEMENT_COUNT][3] = {
    { 150, 25, 0 },    // fire  — deep ember
    { 0, 70, 150 },    // ice   — deep glacier
    { 200, 200, 200 }, // light — near-white, the only element with a bright core
    { 10, 0, 20 },     // dark  — void
    { 165, 150, 0 },   // soul  — deep amber
    { 0, 120, 40 },    // wind  — deep green
};

// Draw one billboarded, element-tinted light orb at `pos` with `scale`.
// Self-contained OPEN_DISPS — the GBI display-list macros (POLY_XLU_DISP →
// __gfxCtx->polyXlu.p) need the __gfxCtx local that OPEN_DISPS declares, and
// this is a standalone function (not inside the caller's OPEN_DISPS scope).
// MUST NOT be called from inside another OPEN_DISPS block (no nesting).
static void GaroForm_DrawOneOrb(PlayState* play, Vec3f pos, f32 scale, u8 element) {
    u8 e = element % GARO_ROD_ELEMENT_COUNT;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, sRodOrbPrim[e][0], sRodOrbPrim[e][1], sRodOrbPrim[e][2], 255);
    gDPSetEnvColor(POLY_XLU_DISP++, sRodOrbEnv[e][0], sRodOrbEnv[e][1], sRodOrbEnv[e][2], 0);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)GARO_ORB_MATERIAL_DL);
    Matrix_Translate(pos.x, pos.y, pos.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)GARO_ORB_MODEL_DL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// ── Banish shadow ball ──────────────────────────────────────────────────
// Two nested billboards built from the SAME light-orb asset, so no new asset
// is needed and the texture is guaranteed present:
//
//   1. HALO — the material DL untouched (its soft additive glow), tinted
//      violet and drawn ~1.9x, so it reads as a translucent sphere.
//   2. CORE — same texture, but the combiner is overridden to "flat PRIM
//      color, alpha straight from the I8 glow texture" and the render mode to
//      plain XLU alpha blending. Additive light can only ever BRIGHTEN, which
//      is exactly why the old sparkle cluster never read as a shadow ball;
//      with alpha blending a near-black purple actually darkens the middle.
//
// Both layers are XLU with z-compare but no z-write (ZMODE_XLU), so the core
// paints over the halo without z-fighting. Cycle type is forced to 1-cycle
// because the core's combiner reads TEXEL0, which is not valid in cycle 2.
//
// This is the shared renderer for every Garo ball: the banish shadow ball, the
// rod charge ball and the fired rod orbs all go through it, only the colours
// change. A flat additive glow (what the orbs used to be) reads as a smear of
// light; the dense core is what makes it read as a solid sphere.
static void GaroForm_DrawLayeredOrb(PlayState* play, Vec3f pos, f32 scale, const u8 haloPrim[3], const u8 haloEnv[3],
                                    const u8 corePrim[3]) {
    OPEN_DISPS(play->state.gfxCtx);

    // Layer 1 — halo, additive glow (asset's own combiner/rendermode).
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, haloPrim[0], haloPrim[1], haloPrim[2], 255);
    gDPSetEnvColor(POLY_XLU_DISP++, haloEnv[0], haloEnv[1], haloEnv[2], 0);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)GARO_ORB_MATERIAL_DL);
    Matrix_Translate(pos.x, pos.y, pos.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(scale * 1.9f, scale * 1.9f, scale * 1.9f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)GARO_ORB_MODEL_DL);

    // Layer 2 — dense core. Material DL again (re-binds the texture), then our
    // overrides on top of it.
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)GARO_ORB_MATERIAL_DL);
    gDPSetCycleType(POLY_XLU_DISP++, G_CYC_1CYCLE);
    gDPSetCombineLERP(POLY_XLU_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, TEXEL0, 0,
                      PRIMITIVE, 0);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, corePrim[0], corePrim[1], corePrim[2], 255);
    Matrix_Translate(pos.x, pos.y, pos.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(scale * 0.8f, scale * 0.8f, scale * 0.8f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)GARO_ORB_MODEL_DL);

    CLOSE_DISPS(play->state.gfxCtx);
}

// Banish shadow ball: violet halo, near-black purple core.
static void GaroForm_DrawShadowBall(PlayState* play, Vec3f pos, f32 scale) {
    static const u8 halo[3] = { 185, 115, 255 };
    static const u8 env[3] = { 85, 20, 165 };
    static const u8 core[3] = { 46, 0, 72 };
    GaroForm_DrawLayeredOrb(play, pos, scale, halo, env, core);
}

// Rod ball (charging and fired): same construction, element colours. The core
// is the saturated version of the element so the ball reads as a solid sphere
// of fire / ice / light instead of a pale flare.
// ── Rod charge ball ─────────────────────────────────────────────────────
// The same five-layer construction the Trident's charge ball uses
// (TridentBigMagic_Draw in mods/actors/trident_charge_ball.c), which is itself
// Ganondorf's big-magic draw: scrolling flecks and a backdrop circle, a dot, the
// light ball, and a fan of rays that opens as the charge fills. Garo's is a
// head-sized version of a head-sized version — roughly half the Trident's — and
// every layer is tinted from the element tables instead of Ganondorf's yellow.
//
// The segment loads (0x08/0x09/0x0A) and the layer order are verbatim: those
// DLs index those segments for their scroll matrices, and drawing them out of
// order or without the segments leaves the tiles pointing at whatever was there
// before.
#define GARO_BM_MAT_DL "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightBallMaterialDL"
#define GARO_BM_BALL_DL "__OTR__overlays/ovl_Boss_Ganon/gGanondorfSquareDL"
#define GARO_BM_FLECKS_DL "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightFlecksDL"
#define GARO_BM_CIRCLE_DL "__OTR__overlays/ovl_Boss_Ganon/gGanondorfBigMagicBGCircleDL"
#define GARO_BM_DOT_DL "__OTR__overlays/ovl_Boss_Ganon/gGanondorfDotDL"
#define GARO_BM_RAY_DL "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightRayTriDL"
#define GARO_BM_RAYS_MAX 6

static void GaroForm_DrawRodBall(PlayState* play, Vec3f pos, f32 circleScale, f32 ballScale, s32 rays, f32 spinRad,
                                 u8 element) {
    if (circleScale <= 0.001f) {
        return;
    }
    u8 e = element % GARO_ROD_ELEMENT_COUNT;
    const u8* prim = sRodOrbPrim[e];
    const u8* env = sRodOrbEnv[e];
    const u8* core = sRodOrbCore[e];
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    u32 frame = play->gameplayFrames;

    OPEN_DISPS(gfxCtx);
    Gfx_SetupDL_25Xlu(gfxCtx);

    // Light flecks — the sparkle cloud around the ball.
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, prim[0], prim[1], prim[2], 255);
    gDPSetEnvColor(POLY_XLU_DISP++, env[0], env[1], env[2], 128);
    // The (uintptr_t) casts are the C++ tax: gSPSegment takes an integer
    // address and C++ will not convert the Gfx* implicitly the way the C
    // sources this is lifted from do.
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, frame * -2, 0, 0x40, 0x40, 1, 0, frame * 0xA, 0x40, 0x40, -2, 0,
                                             0, 0xA));
    Matrix_Translate(pos.x, pos.y, pos.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(circleScale, circleScale, circleScale, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)GARO_BM_FLECKS_DL);

    // Backdrop circle — the deep element colour, so the ball sits on its own halo.
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, core[0], core[1], core[2], 255);
    gSPSegment(POLY_XLU_DISP++, 0x09,
               (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, 0, 0, 0x20, 0x20, 1, 0, frame * -4, 0x20, 0x20, 0, 0, 0, -4));
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)GARO_BM_CIRCLE_DL);

    // Swirling dot.
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, env[0], env[1], env[2], 255);
    gSPSegment(POLY_XLU_DISP++, 0x0A,
               (uintptr_t)Gfx_TwoTexScrollEx(gfxCtx, 0, 0, 0, 0x20, 0x20, 1, frame * 2, frame * -0x14, 0x40, 0x40, 0, 0,
                                             2, -0x14));
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)GARO_BM_DOT_DL);

    // The light ball itself, spinning on its own axis.
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, env[0], env[1], env[2], 0);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)GARO_BM_MAT_DL);
    Matrix_Translate(pos.x, pos.y, pos.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(ballScale, ballScale, ballScale, MTXMODE_APPLY);
    Matrix_RotateZ(spinRad, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)GARO_BM_BALL_DL);

    // Ray fan — this is the charge-level tell: one more spoke per tier.
    if (rays > 0) {
        if (rays > GARO_BM_RAYS_MAX) {
            rays = GARO_BM_RAYS_MAX;
        }
        Matrix_Translate(pos.x, pos.y, pos.z, MTXMODE_NEW);
        Matrix_RotateY((frame * 10.0f) / 1000.0f, MTXMODE_APPLY);
        gDPSetEnvColor(POLY_XLU_DISP++, env[0], env[1], env[2], 0);
        for (s32 i = 0; i < rays; i++) {
            f32 ang = (f32)i * ((f32)M_PI * 2.0f / (f32)GARO_BM_RAYS_MAX);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, prim[0], prim[1], prim[2], 200);
            Matrix_Push();
            Matrix_RotateY(ang, MTXMODE_APPLY);
            Matrix_RotateX(0.6f * ((i & 1) ? 1.0f : -1.0f), MTXMODE_APPLY);
            Matrix_RotateZ(ang * 0.5f, MTXMODE_APPLY);
            Matrix_Translate(0.0f, 0.0f, ballScale * 1.6f, MTXMODE_APPLY);
            Matrix_Scale(ballScale * 0.115f, ballScale * 0.115f, ballScale * 0.032f, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)GARO_BM_RAY_DL);
            Matrix_Pop();
        }
    }

    CLOSE_DISPS(gfxCtx);
}

// ── Fragment streak ─────────────────────────────────────────────────────
// The Trident seeker's lit streak (Tcb_DrawStreak, itself func_808E324C from
// z_boss_ganon.c) 1:1: twelve tapering quads laid along the last twelve
// samples of the fragment's path, each turned to the heading it was flying on
// there, then the light ball billboarded on the head. Segment 0x0D carries the
// twelve matrices — that is what the streak display lists index. Only the tint
// is ours.
static const char* sGaroStreakDL[GARO_ORB_TRAIL_DRAWN] = {
    "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak12DL",
    "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak11DL",
    "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak10DL",
    "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak9DL",
    "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak8DL",
    "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak7DL",
    "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak6DL",
    "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak5DL",
    "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak4DL",
    "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak3DL",
    "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak2DL",
    "__OTR__overlays/ovl_Boss_Ganon/gGanondorfLightStreak1DL",
};

static void GaroForm_DrawOrbStreak(PlayState* play, GaroOrb* sw) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    Mtx* mtx = (Mtx*)Graph_Alloc(gfxCtx, GARO_ORB_TRAIL_DRAWN * sizeof(Mtx));
    if (mtx == NULL) {
        return;
    }
    u8 e = sw->element % GARO_ROD_ELEMENT_COUNT;
    const u8* env = sRodOrbEnv[e];
    // Prim stays white like his — the element rides in ENV, which is what keeps
    // the ribbon reading as light instead of flat paint.
    u8 alpha = (sw->timer >= 8) ? 255 : (u8)((sw->timer * 255) / 8);

    OPEN_DISPS(gfxCtx);
    Gfx_SetupDL_25Xlu(gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 255, 255, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, env[0], env[1], env[2], 128);
    gSPSegment(POLY_XLU_DISP++, 0x0D, (uintptr_t)mtx);

    for (s32 i = 0; i < GARO_ORB_TRAIL_DRAWN; i++) {
        s32 t = ((sw->trailIdx - i) + GARO_ORB_TRAIL_LEN) % GARO_ORB_TRAIL_LEN;
        Matrix_Translate(sw->trailPos[t].x, sw->trailPos[t].y, sw->trailPos[t].z, MTXMODE_NEW);
        Matrix_RotateY(sw->trailRot[t].y, MTXMODE_APPLY);
        Matrix_RotateX(-sw->trailRot[t].x, MTXMODE_APPLY);
        Matrix_Scale(GARO_ORB_STREAK_SCALE, GARO_ORB_STREAK_SCALE, GARO_ORB_STREAK_SCALE, MTXMODE_APPLY);
        Matrix_RotateY((f32)M_PI / 2.0f, MTXMODE_APPLY);
        // Not MATRIX_TOMTX: that macro hands __FILE__ to a non-const char*.
        Matrix_ToMtx(mtx, (char*)__FILE__, __LINE__);
        gSPMatrix(POLY_XLU_DISP++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sGaroStreakDL[i]);
        mtx++;
    }

    // The head, exactly as his: the big-magic material + ball spinning on Z.
    Matrix_Translate(sw->pos.x, sw->pos.y, sw->pos.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(6.0f, 6.0f, 6.0f, MTXMODE_APPLY);
    Matrix_RotateZ((f32)play->gameplayFrames * 0.2f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)GARO_BM_MAT_DL);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)GARO_BM_BALL_DL);

    CLOSE_DISPS(gfxCtx);
}

extern "C" void GaroForm_DrawProjectiles(PlayState* play) {
    if (!GaroForm_IsActive())
        return;
    Player* player = GET_PLAYER(play);

    // Two call sites reach this: the Garo body pass, and the rod-aim pass that
    // MmForm_Draw runs BEFORE its first-person early-return (without which the
    // charge ball is invisible the whole time you are aiming). In the
    // Z-targeted aim both can fire on the same frame, and drawing the additive
    // orbs twice doubles their brightness — so make the call idempotent.
    static s32 sLastDrawFrame = -1;
    if (sLastDrawFrame == (s32)play->gameplayFrames)
        return;
    sLastDrawFrame = (s32)play->gameplayFrames;

    // ── v10.10 charge ball — grows IN FRONT OF GARO'S FACE while charging ──
    // Drawn ~55u ahead of the head along the aim direction (focus.rot), like
    // the Deku bubble forming at the mouth — so it's visible whether the
    // camera is first-person or 3rd-person Z-target (the hand position was
    // off-screen in first-person). Scale ramps 1.5→4.5 with charge + a gentle
    // pulse. Self-contained OPEN_DISPS (called outside the knife block below).
    if (sGaroAttack.state == GARO_ROD_AIM) {
        Vec3f head = player->actor.focus.pos; // head/eye point (PostLimbDraw HEAD)
        if (head.y == 0.0f) {                 // fallback before PostLimb populates it
            head = player->actor.world.pos;
            head.y += 60.0f;
        }
        s16 ay = player->actor.focus.rot.y;
        s16 ax = player->actor.focus.rot.x;
        f32 cosP = Math_CosS(ax);
        f32 fwd = 55.0f;
        Vec3f ballPos = {
            head.x + Math_SinS(ay) * cosP * fwd,
            head.y + (-Math_SinS(ax)) * fwd,
            head.z + Math_CosS(ay) * cosP * fwd,
        };
        f32 t = (f32)sGaroAttack.rodChargeTimer / (f32)GARO_ROD_CHARGE_MAX;
        if (t > 1.0f)
            t = 1.0f;
        // The layered charge ball. Its size steps per tier (eased in the
        // GARO_ROD_AIM handler) and gets a gentle breath on top; the spin is
        // driven off the frame counter like the Trident's.
        f32 pulse = 1.0f + 0.06f * Math_SinS(play->gameplayFrames * 0x1000);
        GaroForm_DrawRodBall(play, ballPos, sGaroAttack.rodBallCircle * pulse, sGaroAttack.rodBallScale * pulse,
                             sGaroAttack.rodBallRays, (f32)play->gameplayFrames * 0.14f, sGaroAttack.rodElement);

        // Aiming reticle. OOT draws no crosshair for the slingshot pipeline we
        // borrow — it expects you to aim off the on-screen arm and weapon, and
        // Garo's first-person limbs are all nulled — so there was nothing at
        // all to aim with. Four small dots in a diamond around the aim ray,
        // far enough out (GARO_RETICLE_DIST) to sit on what you are pointing
        // at, plus a tiny centre dot.
        Vec3f fwdV = { Math_SinS(ay) * cosP, -Math_SinS(ax), Math_CosS(ay) * cosP };
        Vec3f centre = {
            head.x + fwdV.x * GARO_RETICLE_DIST,
            head.y + fwdV.y * GARO_RETICLE_DIST,
            head.z + fwdV.z * GARO_RETICLE_DIST,
        };
        // Screen-right and screen-up for the aim direction: right is the
        // horizontal perpendicular (yaw + 90°), up is right × forward.
        s16 rightYaw = (s16)(ay + 0x4000);
        Vec3f rightV = { Math_SinS(rightYaw), 0.0f, Math_CosS(rightYaw) };
        Vec3f upV = {
            rightV.y * fwdV.z - rightV.z * fwdV.y,
            rightV.z * fwdV.x - rightV.x * fwdV.z,
            rightV.x * fwdV.y - rightV.y * fwdV.x,
        };
        // The spread opens up as the shot charges, so the reticle doubles as a
        // charge gauge.
        f32 spread = GARO_RETICLE_SPREAD * (1.0f + t * 0.6f);
        GaroForm_DrawOneOrb(play, centre, GARO_RETICLE_DOT_SCALE * 0.7f, sGaroAttack.rodElement);
        for (s32 i = 0; i < 4; i++) {
            f32 ox = (i == 0) ? spread : (i == 1) ? -spread : 0.0f;
            f32 oy = (i == 2) ? spread : (i == 3) ? -spread : 0.0f;
            Vec3f dot = {
                centre.x + rightV.x * ox + upV.x * oy,
                centre.y + rightV.y * ox + upV.y * oy,
                centre.z + rightV.z * ox + upV.z * oy,
            };
            GaroForm_DrawOneOrb(play, dot, GARO_RETICLE_DOT_SCALE, sGaroAttack.rodElement);
        }
    }

    // ── Banish shadow ball in flight ─────────────────────────────────────
    // The travelling ball IS this orb now (it used to be a loose cluster of
    // KiraKira sparkles, which read as "magic dust", not as a shadow ball).
    // It swells slightly as it crosses so the arrival has some weight.
    if (sGaroAttack.state == GARO_BANISH_SHADOW) {
        f32 t = (f32)sGaroAttack.shadowBallTimer / (f32)GARO_BANISH_SHADOW_LEN;
        if (t > 1.0f)
            t = 1.0f;
        f32 scale = 3.4f + t * 1.6f;
        GaroForm_DrawShadowBall(play, sGaroAttack.shadowBallPos, scale);
    }

    // Fired shots, drawn the way the Trident draws its two kinds and nothing
    // else — only recoloured:
    //   the BALL keeps the exact five-layer big-magic draw it had while
    //     charging, at the size it was released at, so the shot reads as THAT
    //     ball flying off (trident_charge_ball.c, TCB_KIND_BALL);
    //   the FRAGMENTS carry his lit streak (TCB_KIND_HUNTER).
    // Their wake is not drawn here: it is FhgFlash light balls dropped in the
    // update, and the effect system draws those itself.
    for (s32 i = 0; i < GARO_ORB_POOL_MAX; i++) {
        GaroOrb* sw = &sGaroAttack.orbs[i];
        if (!sw->active) {
            continue;
        }
        if (sw->isSeeker) {
            GaroForm_DrawOrbStreak(play, sw);
        } else {
            GaroForm_DrawRodBall(play, sw->pos, sw->ballCircle, sw->ballScale, GARO_BM_RAYS_MAX,
                                 (f32)play->gameplayFrames * 0.2f, sw->element);
        }
    }
}

// Helpers
static f32 GaroForm_StickMag(PlayState* play) {
    s8 x = play->state.input[0].cur.stick_x;
    s8 y = play->state.input[0].cur.stick_y;
    f32 mag = sqrtf((f32)(x * x + y * y));
    return (mag > 80.0f) ? 1.0f : mag / 80.0f;
}

// Camera-relative stick angle. Matches OOT's input-direction yaw used by
// Player_GetMovementSpeedAndYaw: cameraInputYaw + stickAngleFromY.
static s16 GaroForm_StickAngle(PlayState* play) {
    s8 x = play->state.input[0].cur.stick_x;
    s8 y = play->state.input[0].cur.stick_y;
    s16 stickYaw = Math_Atan2S((f32)y, -(f32)x); // atan2(y, -x) → forward = up-stick
    s16 camYaw = Camera_GetInputDirYaw(GET_ACTIVE_CAM(play));
    return camYaw + stickYaw;
}

// Main update (called from z_player.c after Player_UpdateCommon)
// Garo full moveset — Goron-style action dispatch
//
// Architecture (mirrors mm_player_form.cpp Goron):
//   - IDLE: NO PAUSE_ACTION_FUNC. Link's vanilla actionFunc runs → items,
//     swim, jump, walk/run all work 1:1. IDLE just observes raw input and
//     transitions to combat states when triggered.
//   - Combat states (SPIN, PARRY, DASH, BANISH, ROD_AIM, etc.):
//     SET PAUSE_ACTION_FUNC → Link's actionFunc is suppressed, the form
//     drives the pose via formSkelAnime + memcpy + handles its own quad.
//   - B is stripped before Player_UpdateCommon (TransformMasks_FilterB), so
//     OOT's slash action never starts — combat is fully Garo-owned.

// Stop the spin mid-turn and leave Garo facing somewhere sensible. If he was
// steering, keep the direction he was travelling — ending a free spin snapped
// back to where he started would fight the player's input. A spin done on the
// spot hands back the entry yaw. `yaw` is written too: it is the field
// Player_UpdateCommon copies into world.rot.y, so leaving it stale would turn
// him again on the first frame after the move.
static void GaroForm_SettleSpinFacing(Player* player) {
    player->actor.shape.rot.y = (player->linearVelocity > 0.5f) ? player->actor.world.rot.y : sGaroAttack.spinEntryYaw;
    player->yaw = player->actor.shape.rot.y;
}

static void GaroForm_ResetToIdle(Player* player) {
    // Covers the natural end of the spin AND every interruption (damage, mask
    // swap, scene change all land here).
    if (sGaroAttack.state == GARO_SPIN) {
        GaroForm_SettleSpinFacing(player);
    }
    sGaroAttack.state = GARO_IDLE;
    sGaroAttack.stateTimer = 0;
    sGaroAttack.parryAttacker = NULL;
    sGaroAttack.banishTarget = NULL;
    // v10 defensive cleanup — any state that takes ownership of these
    // suppression / latch fields MUST roll them back when bailing out
    // (mask swap, death, scene reload all flow through here eventually).
    player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_DRAW;
    sGaroAttack.shadowBallSlashFired = 0;
    sGaroAttack.airSlashActive = 0;
    sGaroAttack.landStrikeFired = 0;
    sGaroAttack.landStrikeTailFrames = 0;
    sGaroAttack.hopAirTimer = 0;
    // v10.9: tear down the rod aim camera if it was active (covers the normal
    // release path AND any interrupt that resets to idle — damage, hard-block,
    // scene change). Clear FIRST_PERSON + unk_6AD so Player_UpdateCamAndSeqModes
    // resumes normal camera control, restore the camera to NORMAL, and zero the
    // upper-body tilt. Guarded so we don't reset a player who wasn't aiming.
    // gPlayState == play during GaroForm_Update.
    if (sGaroAttack.rodAimActive) {
        // Tear down the borrowed slingshot aim. Force heldItemAction off
        // SLINGSHOT so Player_DekuBubbleCleanup's guard passes, then run it
        // to clear sDekuBubbleActive + the aim flags and restore the camera —
        // exactly the Deku bubble's cleanup path.
        player->heldItemAction = PLAYER_IA_NONE;
        player->itemAction = PLAYER_IA_NONE;
        Player_DekuBubbleCleanup(player);
        player->upperLimbRot.x = 0;
        player->upperLimbRot.y = 0;
        player->headLimbRot.x = 0;
        player->headLimbRot.y = 0;
        sGaroAttack.rodAimActive = 0;
    }
    // v10.1: sidehop / backflip temporarily rotates world.rot.y to drive
    // the engine's lateral / backward motion via linearVelocity. We MUST
    // restore world.rot.y to match shape.rot.y so the next "forward
    // movement" intent (running, swinging) doesn't inherit the rotated yaw.
    player->actor.world.rot.y = player->actor.shape.rot.y;
    GaroAttack_DisableSpinQuad(player);
    if (sGaroAttack.trailActive) {
        // Trail killed lazily via centralized check (top of GaroForm_Update).
    }
}

// Enter the free dual-sword spin — Garo's only melee attack. Fires on the B
// PRESS, so the hold-to-charge branch is decided inside the spin instead of
// making the attack wait for the button to come up.
static void GaroForm_StartSpin(PlayState* play, Player* player) {
    LinkAnimationHeader* spin = GaroForm_LoadAnim(GARO_SPINATTACK_PATH);
    if (spin != NULL) {
        GaroAttack_StartFormAnim(play, spin, 0.0f, -1.0f, GARO_SPIN_PLAYSPEED);
    }
    sGaroAttack.state = GARO_SPIN;
    sGaroAttack.stateTimer = 0;
    sGaroAttack.bHoldDetectTimer = 0;
    sGaroAttack.spinEntryYaw = player->actor.shape.rot.y;
    if (!sGaroAttack.trailActive) {
        GaroAttack_SpawnTrail(play);
    }
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_SWING_HARD);
}

extern "C" void GaroForm_Update(PlayState* play, Player* player) {
    // ── Trail VFX lifetime ───────────────────────────────────────────────
    // Sword trail belongs to every blade-swinging state (see trailWanted below).
    // Centralized kill so individual exit paths don't need to remember.
    GaroAttack_UpdateSwords(play);
    // The escort on a reflected shot lives outside the state machine: the
    // stance that started it is over by the next frame, and the shot has to
    // keep hurting all the way out.
    GaroAttack_UpdateReflect(play, player);

    if (!GaroForm_IsActive()) {
        GaroAttack_KillTrail(play);
        GaroForm_ResetToIdle(player);
        return;
    }

    // Every state that calls GaroAttack_SpawnTrail MUST be listed here, or the
    // centralized kill at the top of the next frame wipes the trail before it
    // ever draws.
    bool trailWanted = (sGaroAttack.state == GARO_SPIN || sGaroAttack.state == GARO_AIR_SLASH ||
                        sGaroAttack.state == GARO_LAND_STRIKE || sGaroAttack.state == GARO_SHADOW_BALL ||
                        sGaroAttack.state == GARO_PARRY_RIPOSTE);
    if (!trailWanted && sGaroAttack.trailActive) {
        GaroAttack_KillTrail(play);
    }

    // ── Cooldown ticks ───────────────────────────────────────────────────
    if (sGaroAttack.banishCooldown > 0)
        sGaroAttack.banishCooldown--;
    if (sGaroAttack.rodReleaseCD > 0)
        sGaroAttack.rodReleaseCD--;

    // ── Blocking state guard ──────────────────────────────────────────────
    // Talking, cutscene, dead, climbing ledge, hooked, etc. — bail to IDLE
    // and DON'T touch any Garo state this frame. Link is doing something OOT
    // that must take precedence.
    const u32 hardBlockMask = PLAYER_STATE1_LOADING | PLAYER_STATE1_TALKING | PLAYER_STATE1_DEAD |
                              PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_CLIMBING_LEDGE |
                              PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LADDER |
                              PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE;
    if (player->stateFlags1 & hardBlockMask) {
        if (sGaroAttack.state != GARO_IDLE) {
            GaroAttack_KillTrail(play);
            GaroForm_ResetToIdle(player);
        }
        return;
    }
    // v10.9 FIRST_PERSON is a "soft" block: it bails to idle for every state
    // EXCEPT GARO_ROD_AIM, which legitimately SETS FIRST_PERSON itself for the
    // aim camera. (FIRST_PERSON was in hardBlockMask — so the moment ROD_AIM
    // raised the flag, the next frame's guard reset to idle → "enter aim then
    // exit instantly". This is exactly what the Deku bubble avoids by not
    // running under such a guard.) For other states the flag still means
    // "Link entered C-up look / bow aim" → yield.
    if ((player->stateFlags1 & PLAYER_STATE1_FIRST_PERSON) && sGaroAttack.state != GARO_ROD_AIM) {
        if (sGaroAttack.state != GARO_IDLE) {
            GaroAttack_KillTrail(play);
            GaroForm_ResetToIdle(player);
        }
        return;
    }
    // DAMAGED is a "soft" block — states bail to idle on damage so Link's
    // knockback anim plays freely. The guard chain is exempt: PARRY_GUARD reads
    // the flag on purpose (being hit is what arms its counter) and
    // PARRY_RIPOSTE is the counter itself, which starts on the very frame the
    // hit lands and would otherwise be cancelled by the flag that summoned it.
    if ((player->stateFlags1 & PLAYER_STATE1_DAMAGED) && (sGaroAttack.state != GARO_PARRY_GUARD) &&
        (sGaroAttack.state != GARO_PARRY_RIPOSTE)) {
        if (sGaroAttack.state != GARO_IDLE) {
            GaroAttack_KillTrail(play);
            GaroForm_ResetToIdle(player);
        }
        return;
    }

    // ── v9: post-kill detection via enemy-list snapshot diff ─────────────
    // Each frame we record the current enemy pointer set, then compare to
    // last frame's snapshot — any pointer that vanished is treated as a
    // kill / despawn for laughPending purposes. Cheap heuristic; false
    // positives (enemy despawned for non-Garo reasons) cost only a 20%
    // RNG roll. Pointer reuse is a known minor flaw, fine for the laugh
    // rate.
    {
        static uintptr_t sPrevEnemyIds[64];
        static u8 sPrevEnemyCount = 0;
        uintptr_t curIds[64];
        u8 curCount = 0;
        for (Actor* enemy = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; enemy != NULL && curCount < 64;
             enemy = enemy->next) {
            curIds[curCount++] = (uintptr_t)enemy;
        }
        // Only diff if BOTH frames had enemies. If sPrevEnemyCount == 0 then
        // any vanish event is meaningless (no previous state to compare to);
        // if curCount == 0 then comparing the inner loop short-circuits but
        // every prev id would "vanish", triggering a false laugh whenever
        // the player enters a scene with no enemies. Cap at min count to
        // skip the diff in those edge cases.
        if (sPrevEnemyCount > 0 && curCount > 0) {
            for (u8 i = 0; i < sPrevEnemyCount; i++) {
                u8 stillAlive = 0;
                for (u8 j = 0; j < curCount; j++) {
                    if (sPrevEnemyIds[i] == curIds[j]) {
                        stillAlive = 1;
                        break;
                    }
                }
                if (!stillAlive) {
                    sGaroAttack.laughPending = 1;
                    break;
                }
            }
        }
        if (curCount > 0) {
            memcpy(sPrevEnemyIds, curIds, sizeof(uintptr_t) * curCount);
        }
        sPrevEnemyCount = curCount;
    }

    // ── Raw input read ───────────────────────────────────────────────────
    // B is stripped from sp44 by TransformMasks_FilterB, but the raw
    // play->state.input[0] still has it. That's what we read.
    // Reading raw also means we bypass that filter's message/ocarina gate, so we apply it
    // ourselves: with a textbox or the ocarina up the buttons belong to it, and the Garo
    // moveset must not fire while the player is playing notes.
    Input* input = &play->state.input[0];
    const bool inputOwned = MmForm_InputOwnedByMessage() != 0;
    bool bHold = !inputOwned && CHECK_BTN_ALL(input->cur.button, BTN_B) != 0;
    bool bPress = !inputOwned && CHECK_BTN_ALL(input->press.button, BTN_B) != 0;
    bool rPress = !inputOwned && CHECK_BTN_ALL(input->press.button, BTN_R) != 0;
    bool aPress = !inputOwned && CHECK_BTN_ALL(input->press.button, BTN_A) != 0;
    bool aHold = !inputOwned && CHECK_BTN_ALL(input->cur.button, BTN_A) != 0;
    bool zHeld = !inputOwned && CHECK_BTN_ALL(input->cur.button, BTN_Z) != 0;
    // v9: BTN_L / BTN_R own rod-mode element cycling (read inline in the
    // ROD_AIM state via input->press.button). No standalone lPress alias —
    // the legacy `lPress = BTN_Z` was unused after the v8 refactor.

    Actor* zTarget = NULL;
    if (Player_IsZTargeting(player) && player->focusActor != NULL) {
        zTarget = player->focusActor;
    }
    bool zEnemy = (zTarget != NULL) && (zTarget->category == ACTORCAT_ENEMY);
    // v10.4: "Z engaged" = the player is committed to Z-targeting, EITHER by
    // physically holding Z (hold-type lock-on) OR by being locked on via the
    // toggle/switch lock-on setting (where Z isn't held after the lock). The
    // Z+A move dispatch keys on this so hops/jump-attack/shadow-ball fire in
    // both control schemes; the no-Z dash keys on its negation.
    bool zEngaged = zHeld || Player_IsZTargeting(player);

    // ── State dispatch ───────────────────────────────────────────────────
    switch (sGaroAttack.state) {

        case GARO_IDLE: {
            // No PAUSE_ACTION_FUNC — Link's actionFunc keeps running. Items,
            // swim, jump, walk/run, OOT shield, all work 1:1.

            // v10 stick orientation — uses OOT's canonical
            // `controlStickDirections[]` (relative to player's facing yaw),
            // populated by Player_UpdateCommon via sControlStickWorldYaw.
            // PLAYER_STICK_DIR_NONE=-1, FORWARD=0, LEFT=1, BACKWARD=2,
            // RIGHT=3. This is the same source the vanilla backflip code
            // reads (z_player.c:4755), so our gating matches OOT's exactly.
            s8 stickDir = player->controlStickDirections[player->controlStickDataIndex];
            bool stickForwardActive = (stickDir == PLAYER_STICK_DIR_FORWARD);
            bool stickBack = (stickDir == PLAYER_STICK_DIR_BACKWARD);
            bool stickSideL = (stickDir == PLAYER_STICK_DIR_LEFT);
            bool stickSideR = (stickDir == PLAYER_STICK_DIR_RIGHT);
            bool stickActive = (stickDir != PLAYER_STICK_DIR_NONE);
            bool onGround = (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
            bool movingFwd = (player->linearVelocity > 0.5f);

            // v10.4: Garo can't lift/grab objects. Clear the grab DoAction
            // icon + the grabbable target each idle frame so the "Grab"
            // prompt never shows and the vanilla grab handler has no target.
            // (The A-strip in FilterB is the primary guard — the grab
            // handler reads the same filtered input — this is belt-and-
            // suspenders + hides the prompt.)
            player->stateFlags2 &= ~PLAYER_STATE2_DO_ACTION_GRAB;
            player->interactRangeActor = NULL;

            // v10.4: when Link has a non-grab contextual action pending
            // (speak / read / open / enter / climb / mount), FilterB lets A
            // through to vanilla — so the Garo A-moveset must NOT also fire
            // off the same press. aForGaro is the A-press the form owns.
            bool aForGaro = aPress && !GaroForm_VanillaWantsAButton(player);

            // R-press → parry guard. Gated on !bHold so a B-hold rod entry
            // doesn't immediately interrupt itself with a parry if R was
            // tapped to cycle elements (rod cycle owns R while bHold is
            // active, parry owns R when B is idle).
            if (rPress && !bHold) {
                LinkAnimationHeader* guard = GaroForm_LoadAnim(GARO_GUARD_PATH);
                if (guard != NULL) {
                    // Raise only: stop at the hold frame, not the anim's end.
                    GaroAttack_StartFormAnim(play, guard, 0.0f,
                                             Animation_GetLastFrame(guard) * GARO_GUARD_HOLD_FRACTION, 1.0f);
                }
                sGaroAttack.state = GARO_PARRY_GUARD;
                sGaroAttack.stateTimer = 0;
                sGaroAttack.parryAttacker = NULL;
                // Anything that touches him from here on arms the counter, so
                // clear the AC slot first: a bumper left set from before the
                // stance would fire it on the very first guarding frame.
                player->cylinder.base.ac = NULL;
                break;
            }
            // ─── v10 Z+A air slash entry ─────────────────────────────────
            // B-press in mid-air → AIR_SLASH. Lives in IDLE because Garo
            // stays in IDLE while Link's actionFunc owns vanilla jump/fall.
            // We gate on rodReleaseCD so a rod-shot release → jump → B
            // doesn't accidentally swing during the no-fire window.
            if (bPress && !onGround && !sGaroAttack.airSlashActive && sGaroAttack.rodReleaseCD == 0) {
                LinkAnimationHeader* loop = GaroForm_LoadAnim(GARO_SLASHLOOP_PATH);
                if (loop != NULL) {
                    GaroAttack_StartFormAnim(play, loop, 0.0f, -1.0f, 1.0f);
                }
                sGaroAttack.state = GARO_AIR_SLASH;
                sGaroAttack.stateTimer = 0;
                sGaroAttack.airSlashActive = 1;
                sGaroAttack.landStrikeFired = 0;
                if (!sGaroAttack.trailActive)
                    GaroAttack_SpawnTrail(play);
                Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_SWING);
                break;
            }

            // ─── v10 Z-engaged A dispatch (idle/back/side/forward) ───────
            // Z engaged + A press → fan out to new states based on stick.
            // v10.4: gated on zEngaged (Z held OR locked-on via toggle), NOT
            // raw Z-hold, so the moves work in both lock-on control schemes.
            // Hops are evasive moves the player commits to by engaging Z —
            // they fire whether or not an enemy is actually in range.
            // SHADOW_BALL's BANISH chain handles a null target gracefully
            // (travels 60u forward of facing, no teleport).
            if (aForGaro && zEngaged && onGround && sGaroAttack.rodReleaseCD == 0) {
                // Backflip — 2x distance. The engine reads linearVelocity
                // along world.rot.y in Actor_MoveForward each frame, so
                // setting velocity.x/z directly is overridden next tick.
                // Trick: rotate world.rot.y by 180° at entry while leaving
                // shape.rot.y alone so the visual stays facing forward.
                // GaroForm_ResetToIdle restores world.rot.y from shape.rot.y
                // when the hop ends, preventing yaw drift.
                if (stickBack) {
                    LinkAnimationHeader* anim = GaroForm_LoadAnim(GARO_JUMPBACK_PATH);
                    if (anim != NULL) {
                        GaroAttack_StartFormAnim(play, anim, 0.0f, -1.0f, 1.0f);
                    }
                    player->actor.world.rot.y = player->actor.shape.rot.y + 0x8000;
                    player->actor.velocity.y = 5.8f;
                    player->linearVelocity = 12.0f;
                    sGaroAttack.hopDir = 0;
                    sGaroAttack.hopAirTimer = 0;
                    sGaroAttack.state = GARO_BACKFLIP;
                    sGaroAttack.stateTimer = 0;
                    Audio_PlayActorSound2(&player->actor, NA_SE_VO_LI_AUTO_JUMP);
                    break;
                }
                // Sidehop left / right — 1.5x distance (linearVelocity 12.75
                // vs vanilla 8.5). Same world.rot.y trick as backflip so the
                // engine propels Garo laterally while the visual body stays
                // facing the Z-target lock.
                if (stickSideL || stickSideR) {
                    LinkAnimationHeader* anim = GaroForm_LoadAnim(GARO_BOUNCE_PATH);
                    if (anim != NULL) {
                        GaroAttack_StartFormAnim(play, anim, 0.0f, -1.0f, 1.0f);
                    }
                    player->actor.world.rot.y = player->actor.shape.rot.y + (stickSideL ? -0x4000 : 0x4000);
                    player->actor.velocity.y = 4.5f; // bumped from 3.5 so hop is visible
                    player->linearVelocity = 12.75f; // 1.5x vanilla 8.5
                    sGaroAttack.hopDir = stickSideL ? 1 : 2;
                    sGaroAttack.hopAirTimer = 0;
                    sGaroAttack.state = stickSideL ? GARO_SIDEHOP_L : GARO_SIDEHOP_R;
                    sGaroAttack.stateTimer = 0;
                    Audio_PlayActorSound2(&player->actor, NA_SE_VO_LI_AUTO_JUMP);
                    break;
                }
                // Forward jump-attack — 2x distance, retains run speed.
                if (stickForwardActive && movingFwd) {
                    LinkAnimationHeader* anim = GaroForm_LoadAnim(GARO_APPEAR_PATH);
                    if (anim != NULL) {
                        GaroAttack_StartFormAnim(play, anim, 0.0f, -1.0f, 1.5f);
                    }
                    f32 keepFwd = player->linearVelocity;
                    if (keepFwd < 10.0f)
                        keepFwd = 10.0f; // 2x vanilla floor
                    player->actor.velocity.y = 7.5f;
                    player->linearVelocity = keepFwd;
                    player->actor.velocity.x = Math_SinS(player->actor.shape.rot.y) * keepFwd;
                    player->actor.velocity.z = Math_CosS(player->actor.shape.rot.y) * keepFwd;
                    sGaroAttack.hopDir = 3;
                    sGaroAttack.hopAirTimer = 0;
                    sGaroAttack.state = GARO_JUMP_ATTACK;
                    sGaroAttack.stateTimer = 0;
                    Audio_PlayActorSound2(&player->actor, NA_SE_VO_LI_AUTO_JUMP);
                    break;
                }
                // The ONE entry to the banish chain: the old "A + zEnemy" branch was
                // unreachable, which is how the cooldown and the stun stopped applying.
                if (!stickActive && sGaroAttack.banishCooldown == 0) {
                    LinkAnimationHeader* collapse = GaroForm_LoadAnim(GARO_COLLAPSE_PATH);
                    if (collapse != NULL) {
                        GaroAttack_StartFormAnim(play, collapse, 0.0f, -1.0f, 1.0f);
                    }
                    sGaroAttack.state = GARO_BANISH_VANISH;
                    sGaroAttack.stateTimer = 0;
                    sGaroAttack.banishTarget = zTarget; // may be NULL — handled
                    sGaroAttack.shadowBallSlashFired = 0;
                    sGaroAttack.banishCooldown = GARO_BANISH_COOLDOWN;
                    Audio_PlayActorSound2(&player->actor, NA_SE_EV_FANTOM_WARP_S);

                    // v9.1 stun — only when a real enemy is locked. freezeTimer
                    // halts the target's update loop for the whole sequence
                    // (vanish + shadow travel + reappear + strike ≈ 50-60f) so
                    // it can't walk out of the teleport, and the dark ring
                    // telegraphs the mark. Same mechanic as
                    // equip_divine_shield.c's parry AOE, single-target + dark
                    // palette.
                    if (zEnemy && zTarget != NULL) {
                        zTarget->freezeTimer = GARO_BANISH_STUN_FRAMES;
                        Actor_SetColorFilter(zTarget, 0x0000, 0xF8, 0x0000, GARO_BANISH_STUN_FRAMES);

                        Vec3f sparkPos;
                        Vec3f sparkVel = { 0.0f, 1.5f, 0.0f };
                        Vec3f sparkAccel = { 0.0f, -0.05f, 0.0f };
                        Color_RGBA8 primColor = { 140, 80, 220, 255 }; // violet
                        Color_RGBA8 envColor = { 40, 10, 100, 0 };     // near-black violet
                        for (s32 i = 0; i < 8; i++) {
                            f32 ang = (f32)i * ((f32)M_PI * 2.0f / 8.0f);
                            sparkPos.x = zTarget->world.pos.x + cosf(ang) * GARO_BANISH_STUN_RADIUS;
                            sparkPos.y = zTarget->world.pos.y + 30.0f;
                            sparkPos.z = zTarget->world.pos.z + sinf(ang) * GARO_BANISH_STUN_RADIUS;
                            sparkVel.x = cosf(ang) * 0.5f;
                            sparkVel.z = sinf(ang) * 0.5f;
                            EffectSsKiraKira_SpawnSmall(play, &sparkPos, &sparkVel, &sparkAccel, &primColor, &envColor);
                        }
                        Audio_PlayActorSound2(zTarget, NA_SE_IT_SHIELD_REFLECT_SW);
                    }
                    break;
                }
            }

            // No anim started here: ANIMMODE_ONCE would freeze, so DASH_ATTACK re-inits it
            // as a loop. aForGaro keeps it off the presses vanilla owns (speak/open).
            if (aForGaro && !zEngaged && onGround && sGaroAttack.rodReleaseCD == 0) {
                GaroAttack_EnsureFormSkelAnime(play);
                sGaroAttack.state = GARO_DASH_ATTACK;
                sGaroAttack.stateTimer = 0;
                break;
            }
            // B-press on ground → the spin fires ON THE PRESS, this frame. It
            // used to go through a detect state that stood still waiting for
            // the release to tell a tap from a hold, which put up to
            // GARO_B_HOLD_THRESHOLD frames of dead air between the button and
            // the attack. The tap-vs-hold decision now happens INSIDE the
            // spin: keep B down and it converts to the rod charge (see
            // GARO_SPIN), so holding still gets you the ball and tapping gets
            // you an attack with no latency at all.
            // Air-B is captured by the v10 AIR_SLASH dispatcher above; this
            // only runs grounded. Gated on rodReleaseCD so a rod release →
            // instant B re-press doesn't loop.
            if (bPress && onGround && sGaroAttack.rodReleaseCD == 0) {
                GaroForm_StartSpin(play, player);
                break;
            }

            // v9: post-kill laugh taunt — 20% chance per kill-event return to
            // idle. Fires only when no other input action took the frame
            // (this is the last check in IDLE). The laugh anim plays without
            // PAUSE_ACTION_FUNC so movement can interrupt it cleanly.
            if (sGaroAttack.laughPending) {
                sGaroAttack.laughPending = 0;
                if (Rand_ZeroOne() < GARO_LAUGH_CHANCE) {
                    LinkAnimationHeader* laugh = GaroForm_LoadAnim(GARO_LAUGH_PATH);
                    if (laugh != NULL) {
                        GaroAttack_StartFormAnim(play, laugh, 0.0f, -1.0f, 1.0f);
                        sGaroAttack.state = GARO_LAUGH_TAUNT;
                        sGaroAttack.stateTimer = 0;
                        // No dedicated MM voice ID for laugh — fallback to
                        // the Skull Kid laugh SFX (most thematically aligned
                        // with the Garo Master ninja vibe). When a Garo
                        // laugh sample is added to mm.o2r, swap this out
                        // for TransformMasks_PlayMmVoice(0x?? + 0x60).
                        Audio_PlayActorSound2(&player->actor, NA_SE_VO_SK_LAUGH);
                    }
                }
            }
            break;
        }

        // GARO_SPIN — B tap: free dual-sword spin, Garo's only melee. The
        // radial spin quad (EnableSpinQuad, DMG_FIXED_DAMAGE) sweeps around
        // Garo at sword height for the whole move, and the player keeps full
        // stick control at 1.5x run speed while it lasts, so the spin can be
        // carried into a group of enemies instead of being a standing move.
        case GARO_SPIN: {
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;

            // Hold B through the spin → it converts into the rod charge. This
            // is where tap-vs-hold is decided now, so the attack itself never
            // has to wait for the button to come up.
            if (bHold) {
                sGaroAttack.bHoldDetectTimer++;
                if (sGaroAttack.bHoldDetectTimer >= GARO_B_HOLD_THRESHOLD) {
                    player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
                    GaroForm_SettleSpinFacing(player);
                    GaroForm_LeaveRodState();
                    LinkAnimationHeader* aim = GaroForm_LoadAnim(GARO_TAKEOUTBOMB_PATH);
                    if (aim != NULL) {
                        GaroAttack_StartFormAnim(play, aim, 0.0f, -1.0f, 1.0f);
                    }
                    // Enter the EXACT Deku-bubble aim: the OOT slingshot
                    // pipeline. It un-pauses the action func and runs the real
                    // first-person / Z-target aim (camera engages, focus.rot
                    // tracks the stick).
                    Player_StartDekuBubble(player, play);
                    sGaroAttack.rodAimActive = 1;
                    sGaroAttack.state = GARO_ROD_AIM;
                    sGaroAttack.stateTimer = 0;
                    break;
                }
            }

            // FREE SPIN: the two yaws are driven apart on purpose.
            //   shape.rot.y — the spin itself. The visible body (the hybrid
            //     draw builds its matrix from it) and the radial quad both
            //     read this, so the whirl and its hitbox stay together. It is
            //     written ABSOLUTELY, from the entry yaw plus elapsed frames,
            //     never as `+= rate`: Player_UpdateShapeYaw runs earlier in
            //     the same frame and drags shape.rot.y toward the Z-target
            //     while locked on, which would silently eat part of every
            //     turn. Deriving it from the timer makes the spin rate exact.
            //   yaw / world.rot.y — where he TRAVELS. Player_UpdateCommon
            //     assigns world.rot.y = this->yaw and speedXZ = linearVelocity
            //     every frame, so the STEERING field is `yaw`; writing
            //     world.rot.y alone (what the hops do) is overwritten before
            //     it can move him. Both are set so the direction also holds
            //     for anything reading world.rot.y this frame.
            sGaroAttack.stateTimer++;
            player->actor.shape.rot.y = (s16)(sGaroAttack.spinEntryYaw + sGaroAttack.stateTimer * GARO_SPIN_YAW_RATE);

            f32 stickMag = GaroForm_StickMag(play);
            if (stickMag > 0.1f) {
                s16 moveYaw = GaroForm_StickAngle(play);
                player->yaw = moveYaw;
                player->actor.world.rot.y = moveYaw;
                player->linearVelocity = stickMag * GARO_SPIN_MOVE_SPEED;
            } else {
                player->linearVelocity = 0.0f;
            }

            // Radial sword sweep active the whole spin — it is built from
            // shape.rot.y, so the rotation above is what makes it cover the
            // full circle. Fixed damage via the DMG_FIXED_DAMAGE flag baked
            // into EnableSpinQuad (GARO_SPIN_DAMAGE).
            GaroAttack_EnableSpinQuad(player, play);

            // Loop the anim under the fixed-length spin: the state ends on the
            // frame count, never on the animation. (stateTimer was already
            // advanced above — the spin yaw is derived from it.)
            if (GaroAttack_AdvanceFormAnim(play, player)) {
                LinkAnimationHeader* spin = GaroForm_LoadAnim(GARO_SPINATTACK_PATH);
                if (spin != NULL) {
                    GaroAttack_StartFormAnim(play, spin, 0.0f, -1.0f, GARO_SPIN_PLAYSPEED);
                }
            }
            if (sGaroAttack.stateTimer >= GARO_SPIN_FRAMES) {
                player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
                // (ResetToIdle restores the entry facing — it has to handle
                // the interrupted case too, so the restore lives there.)
                GaroForm_ResetToIdle(player);
            }
            break;
        }

        // v10.11 GARO_ROD_AIM — borrows the OOT slingshot aim (entered via
        // Player_StartDekuBubble, the EXACT Deku-bubble mechanism). That runs
        // UN-paused and owns the camera + focus.rot (real first-person aim, or
        // Z-target aim when locked-on). We do NOT pause, do NOT touch the
        // camera, do NOT drive focus.rot — that's the whole point (the manual
        // shortcuts never worked). We only: track charge, cycle element, hold
        // the pose. The FIRE happens below, the frame B is released, by calling
        // GaroForm_FireRodOrb directly (charge resets there).
        case GARO_ROD_AIM: {
            // Exit the aim on A-press (matches the Deku bubble, where A cancels
            // aim). FilterB strips A from the slingshot's input, so OOT won't
            // self-exit on A — we drive the exit here (GaroForm_Update reads
            // raw input). ResetToIdle tears down the borrowed slingshot aim.
            if (aPress) {
                GaroForm_ResetToIdle(player);
                sGaroAttack.rodReleaseCD = GARO_ROD_RELEASE_CD;
                break;
            }
            // Also exit if the slingshot aim ended on its own (held item
            // restored away from SLINGSHOT by some other path / cleanup).
            if (player->heldItemAction != PLAYER_IA_SLINGSHOT) {
                GaroForm_ResetToIdle(player);
                break;
            }

            // B RELEASED → fire here directly (Garo reads raw input, so this is
            // reliable). We entered ROD_AIM only after holding B ≥9 frames, so
            // on entry B is held; the first !bHold is the deliberate release.
            // Firing here (not via the slingshot's fire path, which needs the
            // bow-draw counter to progress and didn't fire for Garo) guarantees
            // the orb launches. ResetToIdle tears down the borrowed aim.
            if (!bHold) {
                GaroForm_FireRodOrb(player, play);
                sGaroAttack.rodReleaseCD = GARO_ROD_RELEASE_CD;
                GaroForm_ResetToIdle(player);
                break;
            }

            sGaroAttack.stateTimer++;
            if (sGaroAttack.rodChargeTimer < GARO_ROD_CHARGE_MAX) {
                sGaroAttack.rodChargeTimer++;
            }

            // Charge-ball geometry, stepped BY TIER rather than ramped
            // continuously: the ball visibly jumps a size at each damage
            // threshold, so what you see is what the shot will do. Eased with
            // Math_ApproachF so each step is a swell, not a pop, and the ray
            // fan opens one spoke per tier — filling out completely once the
            // charge tops out, the same "full" tell the Trident uses.
            {
                // Level 1 is deliberately small and ray-less: it is the "not
                // ready yet" state, and releasing there fires nothing.
                static const f32 sRodBallLevelScale[GARO_ROD_LEVEL_MAX] = { 0.35f, 0.7f, 1.0f };
                static const s16 sRodBallLevelRays[GARO_ROD_LEVEL_MAX] = { 0, GARO_BM_RAYS_MAX / 2, GARO_BM_RAYS_MAX };
                u8 level = GaroAttack_GetRodLevel(sGaroAttack.rodChargeTimer);
                f32 f = sRodBallLevelScale[(level - 1) % GARO_ROD_LEVEL_MAX];
                Math_ApproachF(&sGaroAttack.rodBallCircle, GARO_ROD_BALL_CIRCLE_MAX * f, 0.3f, 0.01f);
                Math_ApproachF(&sGaroAttack.rodBallScale, GARO_ROD_BALL_SCALE_MAX * f, 0.3f, 1.0f);

                s16 wantRays = sRodBallLevelRays[(level - 1) % GARO_ROD_LEVEL_MAX];
                if ((sGaroAttack.stateTimer & 3) == 0) {
                    if (sGaroAttack.rodBallRays < wantRays) {
                        sGaroAttack.rodBallRays++;
                    } else if (sGaroAttack.rodBallRays > wantRays) {
                        sGaroAttack.rodBallRays--;
                    }
                }
            }

            // Charge loop, refreshed every frame the ball is still growing —
            // the same "keep re-playing a flagged SFX while charging" pattern
            // the elemental arrows use in ovl_Arrow_Fire/Ice/Light, and the
            // Deku bubble uses for its breath. Per element, so you HEAR which
            // shot you have picked; the plain shot borrows the sword-charge
            // whine. Goes quiet once the ball tops out, which is the cue that
            // you are at max tier.
            if (sGaroAttack.rodChargeTimer < GARO_ROD_CHARGE_MAX) {
                u16 chargeSfx;
                switch (sGaroAttack.rodElement) {
                    case 1:
                        chargeSfx = NA_SE_PL_ARROW_CHARGE_FIRE;
                        break;
                    case 2:
                        chargeSfx = NA_SE_PL_ARROW_CHARGE_ICE;
                        break;
                    case 3:
                        chargeSfx = NA_SE_PL_ARROW_CHARGE_LIGHT;
                        break;
                    default:
                        chargeSfx = NA_SE_IT_SWORD_CHARGE;
                        break;
                }
                Actor_PlaySfx_Flagged(&player->actor, chargeSfx - SFX_FLAG);
            }

            // Chime when the triple-shot level unlocks (one shot per aim).
            if (!sGaroAttack.rodSfxPlayed && sGaroAttack.rodChargeTimer >= GARO_ROD_CHARGE_TIER3) {
                Audio_PlayActorSound2(&player->actor, NA_SE_SY_SYNTH_MAGIC_ARROW);
                sGaroAttack.rodSfxPlayed = 1;
            }

            // Element cycling — BTN_L (prev) / BTN_R (next).
            if (CHECK_BTN_ALL(input->press.button, BTN_L)) {
                sGaroAttack.rodElement = (sGaroAttack.rodElement + GARO_ROD_ELEMENT_COUNT - 1) % GARO_ROD_ELEMENT_COUNT;
                Audio_PlayActorSound2(&player->actor, NA_SE_SY_DECIDE);
            }
            if (CHECK_BTN_ALL(input->press.button, BTN_R)) {
                sGaroAttack.rodElement = (sGaroAttack.rodElement + 1) % GARO_ROD_ELEMENT_COUNT;
                Audio_PlayActorSound2(&player->actor, NA_SE_SY_DECIDE);
            }

            // Hold the takeOutBomb pose (body). The slingshot aim owns the
            // camera; the form skel anime drives Garo's visible body pose.
            (void)GaroAttack_AdvanceFormAnim(play, player);
            break;
        }

        // GUARD (R held): Garo plants himself in gGaroGuardAnim and waits. The
        // anim runs ONCE and then holds on its last frame — the raised guard IS
        // the pose, so looping it would replay the wind-up over and over; the
        // return half only plays when R comes up (GARO_GUARD_RETURN).
        //
        // The counter is no longer a timed window: ANY hit that lands while he
        // is guarding triggers it. The old version made him invulnerable for
        // the whole stance and then tried to notice the hit that invulnerability
        // had already rejected — which is why the parry never fired. He takes
        // the hit now, and the attacker pays for it immediately: it freezes,
        // Garo appears behind it from wherever he was standing, and he opens up
        // with the drawSwords strike, the same swing the jump attack lands.
        case GARO_PARRY_GUARD: {
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
            player->actor.velocity.x = 0;
            player->actor.velocity.z = 0;
            player->linearVelocity = 0;

            // Advance, but never re-issue: LinkAnimation_Update clamps at the
            // end frame and AdvanceFormAnim keeps copying that pose every
            // frame, which is exactly "hold the last frame".
            (void)GaroAttack_AdvanceFormAnim(play, player);

            // Shots are answered in the air, before they can land and delete
            // themselves. This runs ahead of the damage test on purpose: a
            // bounced shot never becomes a hit at all.
            if (GaroAttack_TryReflectIncoming(play, player)) {
                LinkAnimationHeader* guard = GaroForm_LoadAnim(GARO_GUARD_PATH);
                if (guard != NULL) {
                    GaroAttack_StartFormAnim(play, guard, Animation_GetLastFrame(guard) * GARO_GUARD_HOLD_FRACTION,
                                             0.0f, -GARO_GUARD_RETURN_SPEED);
                    sGaroAttack.state = GARO_GUARD_RETURN;
                    sGaroAttack.stateTimer = 0;
                } else {
                    GaroForm_ResetToIdle(player);
                }
                break;
            }

            // Who hit him. cylinder.base.ac is the actor that touched Garo's AC
            // cylinder this frame; DAMAGED confirms a hit actually landed.
            // Either alone arms the counter — damage dealt without leaving an
            // `ac` still deserves it, and falls back to his lock-on.
            Actor* attacker = player->cylinder.base.ac;
            bool gotHit = ((attacker != NULL) && (attacker->update != NULL)) ||
                          ((player->stateFlags1 & PLAYER_STATE1_DAMAGED) != 0);

            if (gotHit) {
                player->cylinder.base.ac = NULL;
                if ((attacker == NULL) || (attacker->update == NULL)) {
                    attacker = player->focusActor;
                }
                // NO i-frames: the guard is a straight trade. He keeps the
                // health he just lost and answers. Only the DAMAGED flag goes,
                // because Link's knockback would drag him out of what comes
                // next — the HP is already gone by the time that flag is set,
                // so clearing it costs the player nothing back.
                player->stateFlags1 &= ~PLAYER_STATE1_DAMAGED;

                // Something ranged got through the catch zone — a shot that
                // died on impact leaves only its shooter behind, and that
                // shooter is across the room. Distance is the honest test at
                // this point: too far to be the thing that just touched him
                // means the stance simply ends, with no teleport chase.
                f32 dx = (attacker != NULL) ? (attacker->world.pos.x - player->actor.world.pos.x) : 0.0f;
                f32 dz = (attacker != NULL) ? (attacker->world.pos.z - player->actor.world.pos.z) : 0.0f;
                bool tooFarToCounter =
                    (attacker != NULL) && ((dx * dx + dz * dz) > (GARO_GUARD_MELEE_RANGE * GARO_GUARD_MELEE_RANGE));

                if (tooFarToCounter || GaroAttack_IsRangedAttacker(attacker)) {
                    if (GaroAttack_IsRangedAttacker(attacker)) {
                        GaroAttack_ReflectShot(play, player, attacker);
                    }
                    LinkAnimationHeader* guard = GaroForm_LoadAnim(GARO_GUARD_PATH);
                    if (guard != NULL) {
                        GaroAttack_StartFormAnim(play, guard, Animation_GetLastFrame(guard) * GARO_GUARD_HOLD_FRACTION,
                                                 0.0f, -GARO_GUARD_RETURN_SPEED);
                        sGaroAttack.state = GARO_GUARD_RETURN;
                        sGaroAttack.stateTimer = 0;
                    } else {
                        GaroForm_ResetToIdle(player);
                    }
                    break;
                }

                if (attacker != NULL) {
                    // Freeze the attacker for the whole counter and mark it
                    // with the violet ring the banish uses.
                    attacker->freezeTimer = GARO_PARRY_FREEZE_FRAMES;
                    Actor_SetColorFilter(attacker, 0x0000, 0xF8, 0x0000, GARO_PARRY_FREEZE_FRAMES);

                    Vec3f spPos;
                    Vec3f spVel = { 0.0f, 1.0f, 0.0f };
                    Vec3f spAccel = { 0.0f, 0.0f, 0.0f };
                    Color_RGBA8 primColor = { 140, 80, 220, 255 };
                    Color_RGBA8 envColor = { 40, 10, 100, 0 };
                    for (s32 k = 0; k < 8; k++) {
                        f32 ang = (f32)k * ((f32)M_PI * 2.0f / 8.0f);
                        spPos.x = attacker->world.pos.x + cosf(ang) * GARO_BANISH_STUN_RADIUS;
                        spPos.y = attacker->world.pos.y + 30.0f;
                        spPos.z = attacker->world.pos.z + sinf(ang) * GARO_BANISH_STUN_RADIUS;
                        spVel.x = cosf(ang) * 0.5f;
                        spVel.z = sinf(ang) * 0.5f;
                        EffectSsKiraKira_SpawnSmall(play, &spPos, &spVel, &spAccel, &primColor, &envColor);
                    }

                    // Appear behind it, facing its back, from wherever he was.
                    s16 aYaw = attacker->shape.rot.y;
                    player->actor.world.pos.x = attacker->world.pos.x - Math_SinS(aYaw) * GARO_RIPOSTE_OFFSET;
                    player->actor.world.pos.z = attacker->world.pos.z - Math_CosS(aYaw) * GARO_RIPOSTE_OFFSET;
                    player->actor.world.pos.y = attacker->world.pos.y;
                    player->actor.world.rot.y = aYaw;
                    player->actor.shape.rot.y = aYaw;
                    player->yaw = aYaw;
                    Audio_PlayActorSound2(attacker, NA_SE_IT_SHIELD_REFLECT_SW);
                }
                sGaroAttack.parryAttacker = attacker;

                // The jump attack's swing: garo_drawSwords, landed with the
                // wide land-strike sweep (see GARO_PARRY_RIPOSTE).
                LinkAnimationHeader* strike = GaroForm_LoadAnim(GARO_DRAWSWORDS_PATH);
                if (strike != NULL) {
                    GaroAttack_StartFormAnim(play, strike, 0.0f, -1.0f, 1.0f);
                }
                sGaroAttack.state = GARO_PARRY_RIPOSTE;
                sGaroAttack.stateTimer = 0;
                sGaroAttack.landStrikeFired = 0;
                if (!sGaroAttack.trailActive)
                    GaroAttack_SpawnTrail(play);
                Audio_PlayActorSound2(&player->actor, NA_SE_EV_FANTOM_WARP_S);
                break;
            }

            sGaroAttack.stateTimer++;
            // Only an R release ends the stance — into the return half of the
            // anim rather than snapping straight back to idle.
            if (!CHECK_BTN_ALL(input->cur.button, BTN_R)) {
                LinkAnimationHeader* guard = GaroForm_LoadAnim(GARO_GUARD_PATH);
                if (guard != NULL) {
                    // Back down from the hold frame, not from the anim's end.
                    GaroAttack_StartFormAnim(play, guard, Animation_GetLastFrame(guard) * GARO_GUARD_HOLD_FRACTION,
                                             0.0f, -GARO_GUARD_RETURN_SPEED);
                    sGaroAttack.state = GARO_GUARD_RETURN;
                    sGaroAttack.stateTimer = 0;
                } else {
                    GaroForm_ResetToIdle(player);
                }
            }
            break;
        }

        // GUARD RETURN: the stance coming back down — the same anim played
        // backwards, which is the return the held last frame was waiting on.
        case GARO_GUARD_RETURN: {
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
            player->actor.velocity.x = 0;
            player->actor.velocity.z = 0;
            player->linearVelocity = 0;

            sGaroAttack.stateTimer++;
            if (GaroAttack_AdvanceFormAnim(play, player)) {
                GaroForm_ResetToIdle(player);
            }
            break;
        }

        // COUNTER: the strike Garo lands after appearing behind his attacker.
        // It uses the land-strike quad — the wide sweep the jump attack
        // finishes with — so it connects on the frozen target from behind.
        case GARO_PARRY_RIPOSTE: {
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
            player->actor.velocity.x = 0;
            player->actor.velocity.z = 0;
            player->linearVelocity = 0;

            // THAW THE TARGET BEFORE SWINGING. freezeTimer halts the actor's
            // update, and an actor that does not update never re-registers its
            // AC collider that frame — so a frozen enemy is untouchable, and
            // the counter was landing on nothing. Release it a couple of frames
            // early so it is back in the AC list by the time the quad goes
            // live; the colour filter carries the stunned look, and the hit
            // itself takes over from there.
            {
                Actor* target = sGaroAttack.parryAttacker;
                if ((sGaroAttack.stateTimer >= GARO_PARRY_STRIKE_HIT_F - GARO_PARRY_THAW_LEAD) && (target != NULL) &&
                    (target->update != NULL)) {
                    target->freezeTimer = 0;
                }
            }

            if (sGaroAttack.stateTimer >= GARO_PARRY_STRIKE_HIT_F) {
                GaroAttack_EnableLandStrikeQuad(player, play);
                player->meleeWeaponQuads[0].info.toucher.damage = GARO_PARRY_DAMAGE;
                if (!sGaroAttack.landStrikeFired) {
                    sGaroAttack.landStrikeFired = 1;
                    Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_SWING_HARD);
                }
            } else {
                player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
            }

            s32 done = GaroAttack_AdvanceFormAnim(play, player);
            sGaroAttack.stateTimer++;
            if (done) {
                player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
                GaroForm_ResetToIdle(player);
            }
            break;
        }

        // DASH: A-hold forward at v=14. Stick lateral → spin variant.
        case GARO_DASH_ATTACK: {
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;

            f32 stickMag = GaroForm_StickMag(play);
            s16 stickAngle = GaroForm_StickAngle(play);

            // v10.1: smoother steering. Always use dashAttack anim (removed
            // the spinAttack variant switch — flipping anim mid-dash made
            // the visual feel janky as the loop reset every sharp turn).
            // Turn rate dropped 0x800 → 0x400 (≈5.6°/frame) so the rotation
            // is Pegasus-boots-heavy rather than instant, requiring the
            // player to commit to a direction (Goron-roll feel).
            LinkAnimationHeader* anim = GaroForm_LoadAnim(GARO_DASHATTACK_PATH);
            if (anim != NULL && sFormSkelAnime.animation != (void*)anim) {
                LinkAnimation_Change(play, &sFormSkelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim),
                                     ANIMMODE_LOOP, -4.0f);
            }

            if (stickMag > 0.3f) {
                Math_ScaledStepToS(&player->actor.world.rot.y, stickAngle, 0x400);
            }
            player->actor.shape.rot.y = player->actor.world.rot.y;
            // Set linearVelocity only — the engine's Actor_MoveForward will
            // apply it along world.rot.y next physics tick. Skipping the
            // velocity.x/z direct write avoids the one-frame mismatch that
            // contributed to the jankiness.
            player->linearVelocity = GARO_DASH_SPEED;

            GaroAttack_EnableSwingQuad(player, play);
            player->meleeWeaponQuads[0].info.toucher.damage = GARO_DASH_DAMAGE;

            (void)GaroAttack_AdvanceFormAnim(play, player);

            if (player->actor.bgCheckFlags & 0x08) {
                // Wall hit → stop.
                player->linearVelocity = 0;
                GaroForm_ResetToIdle(player);
                break;
            }
            if (!aHold) {
                player->linearVelocity = 0;
                GaroForm_ResetToIdle(player);
            }
            sGaroAttack.stateTimer++;
            break;
        }

        // BANISH: collapse → teleport → appear → slash.
        case GARO_BANISH_VANISH: {
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
            player->actor.velocity.x = 0;
            player->actor.velocity.z = 0;
            player->linearVelocity = 0;

            if (sGaroAttack.stateTimer < 8) {
                player->actor.world.pos.y -= 1.0f;
            } else if (sGaroAttack.stateTimer == 8) {
                player->stateFlags2 |= PLAYER_STATE2_DISABLE_DRAW;
            }
            s32 done = GaroAttack_AdvanceFormAnim(play, player);
            sGaroAttack.stateTimer++;
            if (done || sGaroAttack.stateTimer >= GARO_BANISH_VANISH_END) {
                // Snapshot start (Garo collapse pos) + end (target pos) for
                // the shadow-ball lerp. Stay DISABLE_DRAW the whole time the
                // ball is in flight; Garo only re-appears in SHADOW_BALL
                // (the appearDrawSwords anim) after the travel completes.
                Actor* target = sGaroAttack.banishTarget;
                sGaroAttack.shadowBallStart = player->actor.world.pos;
                sGaroAttack.shadowBallStart.y += 30.0f; // chest height
                if (target != NULL && target->update != NULL) {
                    sGaroAttack.shadowBallEnd = target->world.pos;
                    sGaroAttack.shadowBallEnd.y += 30.0f;
                } else {
                    // v10.1: no target locked → the shadow ball "travels"
                    // to a point a short distance in front of Garo's
                    // facing. This lets the SHADOW chain still play
                    // visibly (the user wanted the full flow, not an
                    // abort). Garo doesn't teleport in this case — the
                    // SHADOW state arrival in-place plays the appearDrawSwords.
                    f32 sinY = Math_SinS(player->actor.shape.rot.y);
                    f32 cosY = Math_CosS(player->actor.shape.rot.y);
                    sGaroAttack.shadowBallEnd = player->actor.world.pos;
                    sGaroAttack.shadowBallEnd.x += sinY * 60.0f;
                    sGaroAttack.shadowBallEnd.z += cosY * 60.0f;
                    sGaroAttack.shadowBallEnd.y += 30.0f;
                }
                sGaroAttack.shadowBallTimer = 0;
                sGaroAttack.state = GARO_BANISH_SHADOW;
                Audio_PlayActorSound2(&player->actor, NA_SE_EV_FANTOM_WARP_S);
            }
            break;
        }
        case GARO_BANISH_SHADOW: {
            // Garo stays DISABLE_DRAW (invisible). The shadow ball lerps from
            // shadowBallStart to shadowBallEnd over GARO_BANISH_SHADOW_LEN
            // frames; the ball itself is DRAWN in GaroForm_DrawProjectiles
            // (GaroForm_DrawShadowBall) — here we only move it and leave a
            // dark wake behind it. On arrival, teleport Garo behind the
            // (still-stunned) target and hand off to GARO_SHADOW_BALL.
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
            player->actor.velocity.x = 0;
            player->actor.velocity.z = 0;
            player->linearVelocity = 0;
            player->stateFlags2 |= PLAYER_STATE2_DISABLE_DRAW;

            f32 t = (f32)sGaroAttack.shadowBallTimer / (f32)GARO_BANISH_SHADOW_LEN;
            if (t > 1.0f)
                t = 1.0f;
            Vec3f ballPos = {
                sGaroAttack.shadowBallStart.x + (sGaroAttack.shadowBallEnd.x - sGaroAttack.shadowBallStart.x) * t,
                sGaroAttack.shadowBallStart.y + (sGaroAttack.shadowBallEnd.y - sGaroAttack.shadowBallStart.y) * t,
                sGaroAttack.shadowBallStart.z + (sGaroAttack.shadowBallEnd.z - sGaroAttack.shadowBallStart.z) * t,
            };

            sGaroAttack.shadowBallPos = ballPos;

            // The warp hum, requested per frame the way this looping sfx is
            // meant to be used. It ends on its own when the state does — no
            // stop call to forget.
            Actor_PlaySfx_Flagged(&player->actor, NA_SE_EV_FANTOM_WARP_L - SFX_FLAG);

            // Dark wake: two dust puffs per frame, dropped just behind the
            // ball and shrinking fast. Keeps the motion readable without the
            // sparkle "fairy dust" look the old cluster had.
            {
                f32 backX = sGaroAttack.shadowBallStart.x - sGaroAttack.shadowBallEnd.x;
                f32 backZ = sGaroAttack.shadowBallStart.z - sGaroAttack.shadowBallEnd.z;
                f32 len = sqrtf(backX * backX + backZ * backZ);
                if (len > 0.001f) {
                    backX = backX / len * 10.0f;
                    backZ = backZ / len * 10.0f;
                }
                Vec3f zeroVel = { 0.0f, 0.0f, 0.0f };
                Vec3f zeroAccel = { 0.0f, 0.0f, 0.0f };
                Color_RGBA8 primColor = { 110, 40, 190, 220 };
                Color_RGBA8 envColor = { 25, 0, 60, 0 };
                for (s32 i = 0; i < 2; i++) {
                    Vec3f dustPos = {
                        ballPos.x + backX + Rand_CenteredFloat(8.0f),
                        ballPos.y + Rand_CenteredFloat(8.0f),
                        ballPos.z + backZ + Rand_CenteredFloat(8.0f),
                    };
                    EffectSsDust_Spawn(play, 0, &dustPos, &zeroVel, &zeroAccel, &primColor, &envColor,
                                       /* scale */ 90, /* scaleStep */ -6,
                                       /* life  */ 8, /* updateMode */ 0);
                }
            }

            sGaroAttack.shadowBallTimer++;
            if (sGaroAttack.shadowBallTimer >= GARO_BANISH_SHADOW_LEN) {
                // v10.1 arrival: teleport behind target (if locked), then
                // enter GARO_SHADOW_BALL which plays appearDrawSwords @1.5x
                // and stamps a master-sword quad with damage 6 at elapsed
                // frame 20. The OLD BANISH_APPEAR / BANISH_SLASH chain is
                // dead — appearDrawSwords combines the "appear" pose and
                // the strike pose into one anim, matching the user spec.
                Actor* target = sGaroAttack.banishTarget;
                if (target != NULL && target->update != NULL) {
                    s16 tYaw = target->shape.rot.y;
                    f32 sx = Math_SinS(tYaw), cz = Math_CosS(tYaw);
                    player->actor.world.pos.x = target->world.pos.x - sx * GARO_BANISH_OFFSET;
                    player->actor.world.pos.z = target->world.pos.z - cz * GARO_BANISH_OFFSET;
                    player->actor.world.pos.y = target->world.pos.y;
                    player->actor.world.rot.y = tYaw;
                    player->actor.shape.rot.y = tYaw;
                }
                LinkAnimationHeader* appear = GaroForm_LoadAnim(GARO_APPEARDRAWSWORDS_PATH);
                if (appear != NULL) {
                    GaroAttack_StartFormAnim(play, appear, 0.0f, -1.0f, GARO_SHADOW_BALL_PLAYSPEED);
                }
                player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_DRAW;
                // WARP_S, not WARP_L. The long one is a LOOPING sfx — vanilla
                // only ever plays it flagged (`NA_SE_EV_FANTOM_WARP_L -
                // SFX_FLAG`, re-requested every frame, see z_boss_mo.c and
                // z_en_fhg_fire.c) so it dies when the request stops. Fired
                // one-shot the way it was here, the loop starts and nothing
                // ever asks it to stop: that is the hum that never went away.
                // The travel hum now lives in GARO_BANISH_SHADOW, where it is
                // re-requested per frame and ends with the state.
                Audio_PlayActorSound2(&player->actor, NA_SE_EV_FANTOM_WARP_S);
                sGaroAttack.state = GARO_SHADOW_BALL;
                sGaroAttack.stateTimer = 0;
                sGaroAttack.shadowBallSlashFired = 0;
                if (!sGaroAttack.trailActive)
                    GaroAttack_SpawnTrail(play);
            }
            break;
        }
        // v9 GARO_LAUGH_TAUNT — non-pausing post-kill taunt anim. Link's
        // actionFunc is intentionally NOT suppressed so the player can
        // immediately interrupt by walking / attacking. The form skel anime
        // drives the laugh pose blended over Link's lower-body motion via
        // memcpy in GaroAttack_AdvanceFormAnim.
        case GARO_LAUGH_TAUNT: {
            s32 done = GaroAttack_AdvanceFormAnim(play, player);
            sGaroAttack.stateTimer++;
            if (done || bPress || aPress || rPress) {
                GaroForm_ResetToIdle(player);
            }
            break;
        }

        // v10 GARO_SHADOW_BALL — Z+A stationary self-cast invuln slash.
        // Garo is invisible (DISABLE_DRAW) + invincible (sustained
        // invincibilityTimer) for the entire anim. At elapsed frame 20
        // (source frame 30 at 1.5x playSpeed), un-hide briefly and stamp
        // the master-sword damage quad with damage 6. The quad stays live
        // for 4 frames; after that, anim continues to its natural end and
        // we restore visibility + reset to idle.
        // v10.1: SHADOW_BALL is the final phase of the chain (entered
        // from BANISH_SHADOW after particle travel + teleport). Garo is
        // VISIBLE here — appearDrawSwords IS the appear anim, so hiding
        // it defeats the point. Sustain invincibility for the duration
        // so the strike can't be interrupted, fire the master-sword
        // damage 6 quad at elapsed frame 20, exit on anim end.
        case GARO_SHADOW_BALL: {
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
            player->actor.velocity.x = player->actor.velocity.z = 0;
            player->linearVelocity = 0;
            if (player->invincibilityTimer < 5)
                player->invincibilityTimer = 5;
            // Make sure the skin is visible — the BANISH chain set
            // DISABLE_DRAW during travel and clears it on transition,
            // but defensively clear again here in case anything else
            // touched the flag.
            player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_DRAW;

            // THAW THE TARGET BEFORE SWINGING. The banish froze it on the way
            // in, and freezeTimer halts an actor's update — an actor that does
            // not update never re-registers its AC collider, so the collision
            // system has nothing to test the quad against. That, not the quad's
            // shape, is why this strike never connected. Let it go a few frames
            // early so it is back in the AC list when the blade arrives.
            {
                Actor* target = sGaroAttack.banishTarget;
                if ((sGaroAttack.stateTimer >= GARO_SHADOW_BALL_HIT_F - GARO_BANISH_THAW_LEAD) && (target != NULL) &&
                    (target->update != NULL)) {
                    target->freezeTimer = 0;
                }
            }

            // v10.3: quad-based strike (NOT direct Actor_ApplyDamage). The
            // direct path silently dropped HP without triggering the enemy's
            // AC-gated death/reaction routine, so Wolfos & co. wouldn't die
            // until a real sword hit landed (the bug the user saw). The quad
            // goes through the AC pipeline → proper kill/flinch, and
            // DMG_FIXED_DAMAGE makes it a constant 8 regardless of equipped
            // sword.
            //
            // It uses the FRONT SWEEP quad, not the plain swing quad: the
            // swing quad is one thin slanted line at any given distance, so
            // the enemy Garo had just teleported behind was routinely missed.
            // The sweep is a full-height wall perpendicular to his facing that
            // marches outward across the 5 live frames, covering everything
            // in front of him from ~15u to ~63u out.
            if (sGaroAttack.stateTimer >= GARO_SHADOW_BALL_HIT_F &&
                sGaroAttack.stateTimer <= GARO_SHADOW_BALL_HIT_F + 4) {
                GaroAttack_EnableFrontSweepQuad(player, play, sGaroAttack.stateTimer - GARO_SHADOW_BALL_HIT_F,
                                                GARO_SHADOW_BALL_DAMAGE);
                if (!sGaroAttack.shadowBallSlashFired) {
                    sGaroAttack.shadowBallSlashFired = 1;
                    Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_SWING_HARD);
                }
            } else {
                player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
            }

            s32 done = GaroAttack_AdvanceFormAnim(play, player);
            sGaroAttack.stateTimer++;
            // Never leave before the quad has had its window. Playing the
            // arrival faster shortened the animation past the strike frame, and
            // an early exit would end the move without a hitbox ever going
            // live — silently, which is the worst way for a strike to fail.
            if (done && (sGaroAttack.stateTimer > GARO_SHADOW_BALL_HIT_F + 4)) {
                player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
                GaroForm_ResetToIdle(player);
            }
            break;
        }

        // v10 GARO_SIDEHOP_L / _R — Z+A stick-side. Vanilla-distance hop
        // with garo_bounce anim. No damage. Gravity decays velocity.y; we
        // wait for ground contact + a minimum airtime before re-idling.
        case GARO_SIDEHOP_L:
        case GARO_SIDEHOP_R: {
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
            // No damage quad during sidehop.
            player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
            (void)GaroAttack_AdvanceFormAnim(play, player);
            sGaroAttack.stateTimer++;
            sGaroAttack.hopAirTimer++;
            if ((player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && sGaroAttack.hopAirTimer >= 3) {
                GaroForm_ResetToIdle(player);
            }
            break;
        }

        // v10 GARO_BACKFLIP — Z+A stick-back. 2x distance (linearVelocity
        // 12.0). Anim: garo_jumpBack. No damage.
        case GARO_BACKFLIP: {
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
            player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
            (void)GaroAttack_AdvanceFormAnim(play, player);
            sGaroAttack.stateTimer++;
            sGaroAttack.hopAirTimer++;
            if ((player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && sGaroAttack.hopAirTimer >= 3) {
                GaroForm_ResetToIdle(player);
            }
            break;
        }

        // v10 GARO_JUMP_ATTACK — Z+A forward + speed>0. 2x distance leap,
        // anim garo_appear @ 1.5x, damage 4 from elapsed frame 4 to land.
        // Garo's facing is locked at entry so the leap is straight forward.
        // v10.1: JUMP_ATTACK is now just the LAUNCH phase of a parabolic
        // forward leap — no damage during the rise. After ~6 frames of
        // airtime (roughly past apex of the 8-frame jump arc), we
        // auto-transition to AIR_SLASH which loops garo_slashLoop until
        // landing → LAND_STRIKE (where the big AOE quad finally fires).
        // So Z+A+forward chains: jump → mid-jump auto-slash → big strike.
        case GARO_JUMP_ATTACK: {
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
            player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON; // no dmg on the leap
            (void)GaroAttack_AdvanceFormAnim(play, player);
            sGaroAttack.stateTimer++;
            sGaroAttack.hopAirTimer++;
            // Mid-jump auto-transition to AIR_SLASH — same flow as if the
            // user had manually pressed B in mid-air. The big landing
            // strike then fires automatically on touchdown.
            if (sGaroAttack.hopAirTimer >= 6) {
                LinkAnimationHeader* loop = GaroForm_LoadAnim(GARO_SLASHLOOP_PATH);
                if (loop != NULL) {
                    GaroAttack_StartFormAnim(play, loop, 0.0f, -1.0f, 1.0f);
                }
                sGaroAttack.state = GARO_AIR_SLASH;
                sGaroAttack.stateTimer = 0;
                sGaroAttack.airSlashActive = 1;
                sGaroAttack.landStrikeFired = 0;
                if (!sGaroAttack.trailActive)
                    GaroAttack_SpawnTrail(play);
                Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_SWING);
                break;
            }
            // Safety: if for some reason we touch ground before mid-jump
            // (short-hop, terrain edge), skip straight to LAND_STRIKE.
            if ((player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && sGaroAttack.hopAirTimer >= 3) {
                LinkAnimationHeader* land = GaroForm_LoadAnim(GARO_DRAWSWORDS_PATH);
                if (land != NULL) {
                    GaroAttack_StartFormAnim(play, land, 0.0f, -1.0f, 1.0f);
                }
                sGaroAttack.state = GARO_LAND_STRIKE;
                sGaroAttack.stateTimer = 0;
                sGaroAttack.airSlashActive = 0;
                sGaroAttack.landStrikeFired = 0;
            }
            break;
        }

        // v10 GARO_AIR_SLASH — B in mid-air. Plays garo_slashLoop in a loop
        // (no damage, purely cosmetic — the damage lives in LAND_STRIKE).
        // On ground contact, transitions to LAND_STRIKE for the heavy hit.
        case GARO_AIR_SLASH: {
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
            // No damage during the air segment.
            player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;

            s32 done = GaroAttack_AdvanceFormAnim(play, player);
            if (done) {
                // Loop the slash anim while still airborne.
                LinkAnimationHeader* loop = GaroForm_LoadAnim(GARO_SLASHLOOP_PATH);
                if (loop != NULL) {
                    GaroAttack_StartFormAnim(play, loop, 0.0f, -1.0f, 1.0f);
                }
            }
            sGaroAttack.stateTimer++;

            // Land → LAND_STRIKE. Same airtime gate as JUMP_ATTACK to
            // avoid stale ground flag firing on entry.
            if ((player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && sGaroAttack.stateTimer >= 2) {
                LinkAnimationHeader* land = GaroForm_LoadAnim(GARO_DRAWSWORDS_PATH);
                if (land != NULL) {
                    GaroAttack_StartFormAnim(play, land, 0.0f, -1.0f, 1.0f);
                }
                sGaroAttack.state = GARO_LAND_STRIKE;
                sGaroAttack.stateTimer = 0;
                sGaroAttack.airSlashActive = 0;
                sGaroAttack.landStrikeFired = 0;
                Audio_PlayActorSound2(&player->actor, NA_SE_PL_ROLL_DUST);
            }
            break;
        }

        // v10 GARO_LAND_STRIKE — Garo drives garo_drawSwords on landing,
        // with a damage-8 master-sword quad live from elapsed frame 12 to
        // anim end + 3 tail frames. Forward motion is killed so the strike
        // is a planted hit.
        case GARO_LAND_STRIKE: {
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
            player->actor.velocity.x = player->actor.velocity.z = 0;
            player->linearVelocity = 0;

            if (sGaroAttack.stateTimer >= GARO_LAND_STRIKE_HIT_F) {
                // v10.3: big cylinder AOE quad (NOT direct Actor_ApplyDamage).
                // Goes through the AC pipeline so enemies actually die/react
                // (the direct path left them at 0 HP without triggering their
                // AC-gated death routine). DMG_FIXED_DAMAGE (baked into
                // EnableLandStrikeQuad) makes it a constant 8 regardless of
                // equipped sword. Quad live from frame 12 through anim end + tail.
                GaroAttack_EnableLandStrikeQuad(player, play);
                if (!sGaroAttack.landStrikeFired) {
                    sGaroAttack.landStrikeFired = 1;
                    Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_SWING_HARD);
                }
            }
            s32 done = GaroAttack_AdvanceFormAnim(play, player);
            sGaroAttack.stateTimer++;
            // Tail-out: once the anim finishes, keep the quad live for
            // GARO_LAND_STRIKE_TAIL_F additional frames so a slightly-late
            // enemy still gets clipped by the planted-sword pose.
            if (done) {
                sGaroAttack.landStrikeTailFrames++;
                if (sGaroAttack.landStrikeTailFrames > GARO_LAND_STRIKE_TAIL_F) {
                    player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
                    sGaroAttack.landStrikeTailFrames = 0;
                    GaroForm_ResetToIdle(player);
                }
            }
            break;
        }

        default:
            // Unknown state → safety net.
            GaroForm_ResetToIdle(player);
            break;
    }
}
