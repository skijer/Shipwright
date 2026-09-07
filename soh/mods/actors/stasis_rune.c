/**
 * stasis_rune.c — Sheikah Slate rune: Stasis (Skijer's NEI)
 *
 * Freezes ONE actor in time (BotW holds a single target too). What the freeze means depends on what
 * was frozen:
 *
 *   ENEMY     — its AI stops. Hits Link lands are stored, not reacted to, and applied when the
 *               stasis ends. Enemies are never launched (user-locked).
 *   PROP      — a rock/boulder/breakable stops dead and can be launched.
 *   BLOCK     — a pushable block, crate or platform: launchable, never climbable.
 *   CLIMBABLE — any other dynapoly body big enough to climb: launchable AND its surface becomes a
 *               vine wall while frozen, so Link can grab on.
 *
 * Hitting a frozen object builds launch force from the damage each blow WOULD have dealt, and each
 * blow's direction steers where it flies. A curved arrow traces the resulting arc.
 *
 * NO HEADER ON PURPOSE: both repos glob mods/*.h with CONFIGURE_DEPENDS, so a new header there
 * forces a full CMake regeneration on the next build. The API is declared here and repeated as
 * local externs at the few call sites (all of which are in this same unity translation unit or the
 * files right next to it).
 *
 * The takeover is the Cane of Pacci idiom (mods/actors/cane_pacci.c): swap the actor's `update` for
 * a no-op — never NULL it, that is the engine's "dead" marker — snapshot every field we write, and
 * disable culling or the engine stops running OUR replacement update the moment the player looks
 * away.
 */

#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include <math.h>
#include <string.h>
#include "soh/ActorDB.h" // instanceSize, to walk a frozen body's own struct for its colliders
#include "../items/helpers/target_select_helper.h"
#include "../items/helpers/combat_helper.h"
#include "../items/helpers/rewind_helper.h" // a body we drive must be admitted to the recorder by hand

// TargetSelect's filter takes no PlayState, and the teardown paths can run outside a frame, so both
// lean on the global. Every mod in the tree reaches it the same way.
extern PlayState* gPlayState;

// Defined further down, next to the rest of the offer handling, but called from Stasis_Begin above
// it — without this the compiler assumes an int-returning function and the real definition clashes.
static void Stasis_ClearOffer(void);
static f32 Stasis_ChargeFraction(void);

// ── Public API ───────────────────────────────────────────────────────────────
s32 Stasis_Cast(PlayState* play, Player* player);
void Stasis_Update(PlayState* play, Player* player);
void Stasis_Draw(PlayState* play);
void Stasis_Forget(void);
static u8 Stasis_KeepsItsPose(Actor* actor);
// Defined next to the collider scan it shares code with, used by the classifier well above it.
static f32 Stasis_MeasureHeight(PlayState* play, Actor* actor);
// z_collision_check.c. Non-static, but the header only mentions it in a comment, so it is declared
// here the same way the other engine entry points this file reaches for are.
void CollisionCheck_ApplyDamage(PlayState* play, CollisionCheckContext* colChkCtx, Collider* collider,
                                ColliderInfo* info);
void Stasis_UpdateOffer(PlayState* play, u8 allowed);
u8 Stasis_IsClimbableBgId(s32 bgId);
// Defined in stasis_sfx.inc.c (pulled in at the tail of this file). MixInto is called from the
// audio thread; the other two from gameplay.
void StasisSfx_Play(f32 rate, f32 volume);
void StasisSfx_Stop(void);
void StasisSfx_SeekToTail(f32 seconds);
void StasisSfx_MixInto(s16* outBuf, u32 numSamples);

// ── Tuning ───────────────────────────────────────────────────────────────────
// Actor updates run at 20 ticks/second in both engines (cane_pacci.h documents 100 as "roughly
// five seconds"), so 200 is the requested 10 seconds. An enemy hold is derived from the cue rate,
// not written out again: the strained double-speed sound and the hold have to end together.
#define STASIS_SFX_RATE_ENEMY 2.0f
#define STASIS_FRAMES_OBJECT 200
#define STASIS_FRAMES_ENEMY ((s16)(STASIS_FRAMES_OBJECT / STASIS_SFX_RATE_ENEMY))
#define STASIS_CHAIN_FRAMES 20 // the chain burst: one second, at the start only
#define STASIS_CHAIN_FRAMES_ENEMY ((s16)(STASIS_CHAIN_FRAMES / STASIS_SFX_RATE_ENEMY))
#define STASIS_FLIGHT_FRAMES 90
#define STASIS_RANGE TARGETSEL_DEFAULT_RANGE

#define STASIS_LAUNCH_BASE 8.0f       // speed with no hits stored at all
#define STASIS_LAUNCH_PER_DAMAGE 1.8f // added speed per point of stored damage
#define STASIS_LAUNCH_PER_HIT 3.0f    // ...and what a damage-less blow (sword on a rock) is worth
#define STASIS_LAUNCH_MAX 42.0f
#define STASIS_LAUNCH_VEL_Y 6.0f
#define STASIS_GRAVITY -1.4f
// Terminal fall speed, set explicitly rather than inherited. Plenty of props leave minVelocityY at
// whatever their own logic wanted, and Actor_UpdateVelocityXZGravity clamps to it every frame — a
// body that never falls in vanilla can carry a value that cancels gravity outright.
#define STASIS_MIN_VELOCITY_Y -30.0f
#define STASIS_RECOIL_SPEED 7.0f // how hard the attacker bounces off a frozen body
#define STASIS_RECOIL_HEIGHT 3.5f

// A body has to be about Link's height before climbing it makes any sense. This is what "actor bg
// GRANDE" means in code — a rule rather than a list, so new scenes need no table edit.
#define STASIS_CLIMB_MIN_HALF_HEIGHT 24.0f
// The floor for being worth freezing at all. Low on purpose — almost anything with a body qualifies
// and what falls out is rupees, fairies, sparkles and projectiles. It is a real measurement or
// nothing: see Stasis_MeasureHeight for why colChkInfo is not one of the sources.
#define STASIS_MIN_HEIGHT 25.0f
// EVERY frozen body gets a shell — that is what makes it solid, hittable and able to carry a rider,
// and it is the same treatment for a pot as for a tree. This threshold only decides whether that
// shell's walls are CLIMBABLE: below it the body is something you pick up, not something you scale,
// and a climbable jar is a way to stand on thin air.
#define STASIS_CLIMBABLE_MIN_HEIGHT 70.0f
#define STASIS_SLIDE_FRICTION 0.93f
// The closing stretch of the recording — what you hear as a body comes back out of stasis. Casting
// again on something already held jumps the cue here so the release is heard on the frame it fires.
#define STASIS_SFX_RELEASE_SECONDS 2.5f

// Vine/climbable wall. It is wallType 4 (WALL_FLAG_3), the value the player's climb check gates on.
#define STASIS_WALL_TYPE_CLIMBABLE 4
#define STASIS_MAX_OWN_COLLIDERS 4

// The tint is a RAMP, not one colour: BotW fades a stasis body from yellow toward red as it takes
// on energy, so the colour alone tells you how hard it is about to go. Uncharged first, fully
// charged second.
// The grayscale pass multiplies these by each texel's own brightness, so they are pushed near
// white-hot — a dark model would otherwise come out muddy.
#define STASIS_TINT_R 255
#define STASIS_TINT_G 235
#define STASIS_TINT_B 90
#define STASIS_TINT_HOT_R 255
#define STASIS_TINT_HOT_G 70
#define STASIS_TINT_HOT_B 40

typedef enum {
    STASIS_KIND_NONE = 0,
    STASIS_KIND_ENEMY,
    STASIS_KIND_PROP,
    STASIS_KIND_BLOCK,
    STASIS_KIND_CLIMBABLE,
} StasisKind;

typedef enum {
    STASIS_PHASE_FROZEN = 0,
    STASIS_PHASE_FLYING,
} StasisPhase;

typedef struct {
    Actor* actor;
    u8 kind;
    u8 phase;
    s16 timer;      // stasis frames left, or flight frames once launched
    s16 chainTimer; // chain-burst frames left
    s16 age;        // frames since the freeze started (drives the pulse)
    u16 accumDamage;
    // The last blow's damage TYPE, so the single hit that lands on thaw is the one the player
    // finished with — 20 of sword and an ice arrow last comes out as 22, frozen solid.
    u32 lastDmgFlags;
    u8 lastDmgEffect;
    u8 lastDmgAmount;
    f32 force;    // accumulated launch speed — every blow adds, nothing ever subtracts
    Vec3f hitDir; // unit vector of the LAST blow, in full 3D. Direction is not accumulated:
                  // in BotW the newest hit re-aims the object and only the magnitude stacks.
    u8 hasHitDir;
    s32 bgId;    // dynapoly id while frozen, or -1
    u8 bgIsOurs; // 1 = WE registered that bgId and must delete it on thaw
    // Saved actor state, restored verbatim (the Pacci_Restore field set).
    ActorFunc origUpdate;
    ActorFunc origDraw;
    u32 origFlags;
    f32 origGravity;
    f32 origMinVelocityY;
    f32 origSpeed;
    Vec3s origShapeRot;
    Vec3s origWorldRot;
    s16 origRoom;
    u8 origMass;
    // Our own collider, OWNED BY THE TARGET so hits land in the target's colChkInfo.
    ColliderCylinder collider;
    u8 colliderReady;

    // The body's OWN colliders. These four bodies already know how to be hit — AC_ON | AC_HARD with
    // COLTYPE_HARD or COLTYPE_TREE — which is the clonk and the sword bounce you get from a rock in
    // the vanilla game. A frozen actor never runs its update, so it never re-registers them and all
    // of that is lost; the tick submits them on its behalf instead. What stays gone is the BREAK,
    // because the break lives in the actor's update, which is exactly the half we want silenced.
    ColliderCylinder* ownCollider[STASIS_MAX_OWN_COLLIDERS];
    u8 ownColliderCount;
    // Whether our own cylinder is submitted too. It covers the part of the body the actor's own
    // colliders never reach — a tree's is 18 wide and 60 tall around the base of a trunk that
    // carries a crown up at 480, so without this there is simply nothing up there to swing at.
    u8 extraCollider;

    // Link riding the launched body. Captured at the instant of the launch, because that is the one
    // frame where "was he holding on?" is still answerable — his climb state ends as soon as the
    // wall he was gripping moves out from under him.
    u8 riderAttached;
    Vec3f riderOffset;

    // A shove along the floor rather than a throw through the air. Blocks only.
    u8 slideLaunch;
} StasisState;

static StasisState sStasis = { 0 };

// The override light for a body in stasis: ambient almost white, one strong diffuse. Ordinary scene
// ambient sits far lower, which is exactly why an untouched model has no brightness to tint.
static Lights1 sStasisLights = gdSPDefLights1(210, 205, 175, 255, 250, 220, 0x28, 0x28, 0x28);

// What the rune WOULD freeze right now. Painted with the same gold so you can see the pick before
// committing to it — the "on offer" highlight. Its draw pointer is swapped exactly like the frozen
// body's, and restored at the top of every tick before the next scan, so a target that stops being
// offered never keeps our wrapper.
static Actor* sStasisOffer = NULL;
static ActorFunc sStasisOfferDraw = NULL;

// AC keeps a frozen body hittable (that is the whole point — you beat on it to charge the launch);
// AT goes live only while it is flying, so a thrown block smashes what it reaches.
static ColliderCylinderInit sStasisColliderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_ON | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0xFFCFFFFF, 0x00, 0x08 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_ON,
        OCELEM_NONE,
    },
    { 40, 60, 0, { 0, 0, 0 } },
};

// ============================================================================
// WHITELIST
// ============================================================================

// Pushable blocks, block-like crates, platforms and elevators. Launchable, never climbable — a
// pushblock you can climb would break every block puzzle in the game.
// BLOCKS: never climbable, and never thrown — they get SHOVED along the floor (see slideLaunch).
static const s16 sStasisBlockIds[] = {
    ACTOR_OBJ_OSHIHIKI,
    ACTOR_OBJ_KIBAKO2,
    ACTOR_OBJ_HSBLOCK,
    ACTOR_OBJ_TIMEBLOCK,
    ACTOR_OBJ_WARP2BLOCK,
    ACTOR_BG_JYA_BLOCK,
    ACTOR_BG_GND_ICEBLOCK,
    ACTOR_BG_SPOT08_ICEBLOCK,
    ACTOR_BG_PUSHBOX,
    ACTOR_BG_HEAVY_BLOCK,
    ACTOR_OBJ_LIFT,
    ACTOR_OBJ_ELEVATOR,
    ACTOR_BG_HIDAN_SYOKU,
    ACTOR_BG_DDAN_JD,
    ACTOR_BG_JYA_1FLIFT,
    ACTOR_BG_JYA_LIFT,
    ACTOR_BG_MORI_ELEVATOR,
    ACTOR_BG_ICE_SHELTER,
    ACTOR_BG_ICE_OBJECTS,
    // Rotating and sliding dungeon furniture. Same rule: it travels, it never takes off.
    ACTOR_BG_MORI_BIGST,
    ACTOR_BG_SPOT15_RRBOX,
};

// Rocks, boulders and large breakables. These carry plain cylinder/sphere colliders, no dynapoly.
static const s16 sStasisPropIds[] = {
    // Boulders and breakables.
    ACTOR_OBJ_BOMBIWA,
    ACTOR_OBJ_HAMISHI,
    ACTOR_BG_SPOT16_BOMBSTONE,
    ACTOR_BG_SPOT01_IDOSOKO,
    ACTOR_BG_HAKA,
    ACTOR_BG_MENKURI_EYE,
    ACTOR_OBJ_ICE_POLY,
    ACTOR_BG_HIDAN_ROCK,
    ACTOR_BG_ICE_TURARA,

    // Rolling boulders. Catching one of these in mid-roll and sending it back the way it came is
    // the single most Stasis-shaped thing in the game.
    ACTOR_EN_GOROIWA,
    ACTOR_BG_JYA_GOROIWA,

    // Things already in motion, or about to be. Freezing them is the *defensive* half of the rune:
    // the Spirit Temple iron block stops in mid-fall, Ganon's floor tiles stop dropping out from
    // under you, the Shadow Temple guillotines and the sliding spike traps simply stop.
    ACTOR_BG_JYA_HAHENIRON,
    ACTOR_BG_GANON_OTYUKA,
    ACTOR_BG_HAKA_TRAP,
    ACTOR_EN_TRAP,

    // Small carryables. They get the same shell as everything else, so they are solid and you can
    // land blows on them where you see them; below STASIS_CLIMBABLE_MIN_HEIGHT its walls just are
    // not climbable. Things you charge up and fire off, not things you scale.
    ACTOR_OBJ_TSUBO,
    ACTOR_OBJ_KIBAKO,
    ACTOR_EN_KUSA,
    ACTOR_OBJ_COMB,
    ACTOR_EN_KANBAN,
    ACTOR_BG_HAKA_TUBO,
    ACTOR_BG_SPOT18_BASKET,
    ACTOR_EN_NIW,

    // Bombs. The fuse is part of the actor's update, so freezing one freezes the countdown — hold
    // a lit bomb out of time, charge it up, and fire it where you want it. That is Remote Bomb and
    // Stasis doing the trick they do together in the game this comes from.
    ACTOR_EN_BOM,
    ACTOR_EN_BOMBF,
    ACTOR_EN_BOM_CHU,

    // Fire. A lit torch held out of time is a portable flame, and one launched across a room is a
    // thrown one — Stasis_FireTick gives these an open-flame collider for the whole hold and the
    // whole flight. The BURNS rows in cane_pacci.c are the shared list; these are the ones a rune
    // can plausibly get hold of (the Fire Temple's walls of flame are architecture, and stay
    // Ultrahand's business).
    ACTOR_OBJ_SYOKUDAI,
    ACTOR_BG_PO_SYOKUDAI,
    ACTOR_EN_ICE_HONO,

    // Cane of Somaria summons. The BLOCK and PLATFORM kinds are Obj_Oshihiki and Obj_Lift, so they
    // are already covered by the block list above; this is the Elegy statue, which rides on a
    // hijacked En_Lightbox. It gets no profile row on purpose — its scale is 0.01, so a model-space
    // row would be multiplied down to nothing, and the collider fallback is the right size anyway.
    ACTOR_EN_LIGHTBOX,
};

// Enemies that shrug Stasis off. Same spirit as sPacciBlacklist: scripted heavyweights whose AI
// does not survive being paused.
static const s16 sStasisEnemyBlacklist[] = {
    ACTOR_EN_IK, ACTOR_EN_TORCH2, ACTOR_EN_ZF, ACTOR_EN_WALLMAS, ACTOR_EN_FLOORMAS, ACTOR_EN_RD,
};

static u8 Stasis_IdInList(s16 id, const s16* list, s32 count) {
    for (s32 i = 0; i < count; i++) {
        if (list[i] == id) {
            return 1;
        }
    }
    return 0;
}

// The dynapoly scan: the index into bgActors IS the bgId (mirrors Pacci_FuseFindBg).
static CollisionHeader* Stasis_FindBg(PlayState* play, Actor* actor, s32* bgIdOut) {
    if ((play == NULL) || (actor == NULL)) {
        return NULL;
    }
    for (s32 i = 0; i < BG_ACTOR_MAX; i++) {
        BgActor* bg = &play->colCtx.dyna.bgActors[i];

        if ((bg->actor == actor) && (bg->colHeader != NULL)) {
            if (bgIdOut != NULL) {
                *bgIdOut = i;
            }
            return bg->colHeader;
        }
    }
    return NULL;
}

// ── Synthesised collision ────────────────────────────────────────────────────────────────────────
// A boulder like Obj_Hamishi carries only a cylinder collider: you cannot stand on it, you cannot
// climb it, and Link walks straight through its middle. Freezing one is supposed to make it a solid
// piece of the world, so while it is held we build a real dynapoly box from its own collider
// dimensions and register it. Deleted again on thaw, so nothing leaks into the scene.
//
// The pools are file-static and single-slot on purpose: exactly one body is ever frozen.
// A body is a LATHE PROFILE: a stack of rings, each one a (height, radius) pair, joined by
// frustum walls. That is not an approximation chosen for convenience — it is literally how these
// models are built. Every body Stasis supports decodes out of the o2r as rings of vertices sharing
// a y, which is why a profile can trace the silhouette instead of boxing it.
//
// The ring is an octagon with its vertices ON the radius (inscribed, not circumscribed), so the
// collision never sticks out past the model anywhere. A square ring — what this used to be —
// overshoots by 41% at its four corners, which is what made frozen bodies feel bigger than they
// look.
#define STASIS_MAX_RINGS 4
#define STASIS_RING_SIDES 8
#define STASIS_BOX_VTX (STASIS_RING_SIDES * STASIS_MAX_RINGS)
// sides between consecutive rings, plus a fan cap at each end
#define STASIS_BOX_POLY ((STASIS_RING_SIDES * 2 * (STASIS_MAX_RINGS - 1)) + ((STASIS_RING_SIDES - 2) * 2))

// The camera reads colHeader->cameraDataList[camId].cameraSType WITHOUT a NULL check
// (z_bgcheck.c func_80041A4C), and every poly's surface type carries a camera index. Leaving this
// NULL is an instant crash the moment the camera looks at the frozen body — which is exactly what
// happened. One neutral entry, and every poly points at index 0.
static CamData sStasisCamData[1];

static Vec3s sStasisVtxPool[STASIS_BOX_VTX];
static CollisionPoly sStasisPolyPool[STASIS_BOX_POLY];
static SurfaceType sStasisSurfPool[1];
static CollisionHeader sStasisHeader;

// The unit octagon, vertices on the circle.
#define STASIS_OCT 0.70710678f
static const f32 sStasisRingX[STASIS_RING_SIDES] = {
    1.0f, STASIS_OCT, 0.0f, -STASIS_OCT, -1.0f, -STASIS_OCT, 0.0f, STASIS_OCT,
};
static const f32 sStasisRingZ[STASIS_RING_SIDES] = {
    0.0f, STASIS_OCT, 1.0f, STASIS_OCT, 0.0f, -STASIS_OCT, -1.0f, -STASIS_OCT,
};

static void Stasis_MakePoly(CollisionPoly* poly, u16 ia, u16 ib, u16 ic) {
    Vec3f a;
    Vec3f b;
    Vec3f c;
    Vec3f e1;
    Vec3f e2;
    Vec3f n;
    f32 len;

    poly->type = 0;
    poly->flags_vIA = ia & 0x1FFF;
    poly->flags_vIB = ib & 0x1FFF;
    poly->vIC = ic & 0x1FFF;

    a.x = (f32)sStasisVtxPool[ia].x;
    a.y = (f32)sStasisVtxPool[ia].y;
    a.z = (f32)sStasisVtxPool[ia].z;
    b.x = (f32)sStasisVtxPool[ib].x;
    b.y = (f32)sStasisVtxPool[ib].y;
    b.z = (f32)sStasisVtxPool[ib].z;
    c.x = (f32)sStasisVtxPool[ic].x;
    c.y = (f32)sStasisVtxPool[ic].y;
    c.z = (f32)sStasisVtxPool[ic].z;

    e1.x = b.x - a.x;
    e1.y = b.y - a.y;
    e1.z = b.z - a.z;
    e2.x = c.x - a.x;
    e2.y = c.y - a.y;
    e2.z = c.z - a.z;
    n.x = (e1.y * e2.z) - (e1.z * e2.y);
    n.y = (e1.z * e2.x) - (e1.x * e2.z);
    n.z = (e1.x * e2.y) - (e1.y * e2.x);
    len = sqrtf((n.x * n.x) + (n.y * n.y) + (n.z * n.z));
    if (len > 0.001f) {
        n.x /= len;
        n.y /= len;
        n.z /= len;
        poly->normal.x = (s16)(n.x * 32767.0f);
        poly->normal.y = (s16)(n.y * 32767.0f);
        poly->normal.z = (s16)(n.z * 32767.0f);
        poly->dist = (s16) - ((n.x * a.x) + (n.y * a.y) + (n.z * a.z));
    }
}

// The body the collision is registered ON. It is NOT the frozen actor.
//
// This is the whole reason Stasis used to corrupt trees and rocks: the engine treats every actor in
// `bgActors[]` as a DynaPolyActor and writes through that cast — `DynaPolyActor_UnsetAllInteractFlags`
// stores `interactFlags` at offset 0x160, and `bgId` at 0x14C. En_Wood02 keeps its ColliderCylinder
// at 0x158, so registering a tree wrote straight into its collider and left `collider.base.actor`
// dangling. The next frame the tree's own update submitted that collider and
// CollisionCheck_SetAC dereferenced the garbage pointer.
//
// So we register a carrier we own every byte of, parked on top of the frozen body. The engine can
// scribble on it freely, the real actor is never touched, and it gets its AI back untouched when
// the stasis ends.
static DynaPolyActor sStasisCarrier;

// Never runs (the carrier is in no actor list), but a non-NULL update is the engine's "alive"
// marker and several collision paths test it.
static void Stasis_CarrierUpdate(Actor* thisx, PlayState* play) {
}

// Park the carrier on the frozen body.
static void Stasis_CarrierSync(Actor* target) {
    sStasisCarrier.actor.world.pos = target->world.pos;
    sStasisCarrier.actor.home.pos = target->world.pos;
    sStasisCarrier.actor.prevPos = target->world.pos;
    sStasisCarrier.actor.shape.rot = target->shape.rot;
    sStasisCarrier.actor.world.rot = target->shape.rot;
    // Scale deliberately NOT copied: the box is authored in world units, so the carrier stays at 1
    // and the engine's per-frame transform does not apply the target's scale a second time.
}

// How big the body LOOKS.
//
// Three wrong sources were tried before this one, and all three are worth naming:
//   - `colChkInfo.cylRadius/cylHeight` is not a size at all for most props. En_Wood02 leaves it at
//     zero, so a tree got no box built; Obj_Hamishi puts 12 there while its real collider is 50.
//   - the actor's own ColliderCylinder is the true COLLISION shape, but it is deliberately much
//     smaller than the model — a tree's is radius 18 x height 60 around the base of the trunk while
//     the visible tree is 480 units tall. Wrapping the collider gives an ankle-high box.
//   - hand-guessed world numbers ignore `actor->scale`, and these bodies are drawn at wildly
//     different scales (En_Wood02: 1.0 / 1.5 / 0.6; En_Ishi and Obj_Hamishi: 0.4; Obj_Bombiwa: 0.1).
//
// So the numbers below are the real bounding boxes of the vertices the actor actually draws, read
// out of `oot.o2r` (a Vertex resource is a 0x48 header then packed 16-byte Vtx), in MODEL units.
// They get multiplied by `actor->scale` at build time, so every scale variant lands right for free.
//
// Each row below is the model's own vertex rings, straight out of the file — every value is a real
// (y, radius) pair that vertices actually sit on, with radius as sqrt(x*x + z*z) rather than the
// larger of |x| and |z|:
//
//   object_wood02 conical   trunk (0,r12) tapering to (400,r0)    crown (108,r180) to apex (480,r0)
//   object_wood02 oval      trunk (0,r12) to (307,r0)             crown (108,r166) to apex (659,r4)
//   object_wood02 kakariko  trunk (0,r64) (13,r41) (27,r40) (200,r34) (277,r3)
//                           crown (193,r160) to apex (593,r3)
//   object_bombiwa          (-123,r31) (0,r405) (387,r514) (773,r256)
//   gameplay_field_keep     (-111,r10) (-80,r101) (16,r127) (113,r64) = gSilverRockDL, drawn by
//                           BOTH Obj_Hamishi and En_Ishi type 1 (28 verts, matching the 28 that the
//                           DL's G_TRI2 stream indexes)
//
// So the crowns are CONES, not the fat cylinders this table used to claim. A slab of radius 170 at
// treetop height was sitting where the real tree tapers from 48 down to nothing — that is the
// "it looks bigger than it is" the boxes had.
//
// The collider list cannot be consulted as a shortcut: the engine clears it at the end of the
// collision phase (z_play.c), so it is always empty by the time any mod code runs.
#define STASIS_FALLBACK_RADIUS 45
#define STASIS_FALLBACK_HEIGHT 90

typedef struct {
    s16 y;
    s16 radius;
} StasisRing;

// A body can be more than one lathe, because a tree is not one solid of revolution: it is a pole
// with a cone hanging around it. Each stack names a slice of the shared ring array and says whether
// its ends are closed.
typedef struct {
    u8 first;
    u8 count;
    u8 capBottom;
    u8 capTop;
} StasisStack;

#define STASIS_MAX_STACKS 2

typedef struct {
    s16 id;
    s16 paramsMax; // inclusive match on params & 0xFF; -1 for "any"
    u8 ringCount;
    StasisRing ring[STASIS_MAX_RINGS];
    u8 stackCount;
    StasisStack stack[STASIS_MAX_STACKS];
} StasisBody;

static const StasisBody sStasisBodies[] = {
    // TREES. Trunk pole first, crown cone second.
    //
    // The crown is deliberately left OPEN at both ends. Its underside is a genuine overhang in the
    // model — the canopy flares from r12 to r180 over a single unit of height — so capping it would
    // put a ceiling across the trunk at y=108 and end every climb one seventh of the way up. Open,
    // Link rides the trunk straight through the foliage to the top, which is also what climbing a
    // tree looks like in the game this rune comes from.
    //
    // The trunk pole is carried up to the crown's apex instead of tapering to nothing at y=400 as
    // the model does. That is the one liberty in this table, it is entirely inside the crown's own
    // silhouette, and it is what gives the climb something to hold all the way up.
    { ACTOR_EN_WOOD02,
      0x04,
      4,
      { { 0, 12 }, { 480, 6 }, { 108, 180 }, { 480, 4 } },
      2,
      { { 0, 2, 1, 1 }, { 2, 2, 0, 0 } } }, // conical
    { ACTOR_EN_WOOD02,
      0x09,
      4,
      { { 0, 12 }, { 659, 6 }, { 108, 166 }, { 659, 4 } },
      2,
      { { 0, 2, 1, 1 }, { 2, 2, 0, 0 } } }, // oval
    { ACTOR_EN_WOOD02,
      0x0A,
      4,
      { { 0, 64 }, { 593, 6 }, { 193, 160 }, { 593, 4 } },
      2,
      { { 0, 2, 1, 1 }, { 2, 2, 0, 0 } } }, // kakariko adult

    // BOULDERS. Solids of revolution, so one closed stack traces them exactly.
    { ACTOR_OBJ_BOMBIWA, -1, 4, { { -123, 31 }, { 0, 405 }, { 387, 514 }, { 773, 256 } }, 1, { { 0, 4, 1, 1 } } },
    // Same silver rock model, same 0.4 scale, so the same profile.
    { ACTOR_OBJ_HAMISHI, -1, 4, { { -111, 10 }, { -80, 101 }, { 16, 127 }, { 113, 64 } }, 1, { { 0, 4, 1, 1 } } },
    { ACTOR_EN_ISHI, -1, 4, { { -111, 10 }, { -80, 101 }, { 16, 127 }, { 113, 64 } }, 1, { { 0, 4, 1, 1 } } },
};

// Fills `rings` and `stacks` with world-space geometry and returns the ring count. Table rows are
// model-space and get scaled; the fallback is already world-space, since a stranger's scale tells
// us nothing.
static s32 Stasis_GetProfile(Actor* actor, StasisRing* rings, StasisStack* stacks, s32* stackCount) {
    f32 sx = fabsf(actor->scale.x);
    f32 sy = fabsf(actor->scale.y);
    s32 i;
    s32 j;

    if (sx < 0.0001f) {
        sx = 1.0f;
    }
    if (sy < 0.0001f) {
        sy = 1.0f;
    }

    for (i = 0; i < (s32)ARRAY_COUNT(sStasisBodies); i++) {
        const StasisBody* body = &sStasisBodies[i];

        if (body->id != actor->id) {
            continue;
        }
        if ((body->paramsMax >= 0) && ((actor->params & 0xFF) > body->paramsMax)) {
            continue;
        }
        for (j = 0; j < body->ringCount; j++) {
            rings[j].y = (s16)((f32)body->ring[j].y * sy);
            rings[j].radius = (s16)((f32)body->ring[j].radius * sx);
            // An apex is a real part of the shape, but a zero-area ring makes degenerate polys with
            // no usable normal, so the tip keeps the smallest radius that still builds.
            if (rings[j].radius < 3) {
                rings[j].radius = 3;
            }
        }
        for (j = 0; j < body->stackCount; j++) {
            s32 k;

            stacks[j] = body->stack[j];
            // Scaling can collapse two rings onto the same height on a tiny variant, and a stack of
            // zero thickness makes polys with no normal at all.
            for (k = stacks[j].first + 1; k < stacks[j].first + stacks[j].count; k++) {
                if (rings[k].y <= rings[k - 1].y) {
                    rings[k].y = rings[k - 1].y + 1;
                }
            }
        }
        *stackCount = body->stackCount;
        return body->ringCount;
    }

    // A body that owns dynapoly already carries an exact description of itself: the bounds of the
    // CollisionHeader it registered. That beats anything guessed, and it is what a pushable block
    // and an ice platform have instead of a row in the table above — which is why they used to
    // travel with a 45-unit probe and clip through walls half their own width.
    if (gPlayState != NULL) {
        s32 bg;

        for (bg = 0; bg < BG_ACTOR_MAX; bg++) {
            BgActor* bgActor = &gPlayState->colCtx.dyna.bgActors[bg];

            if (!(gPlayState->colCtx.dyna.bgActorFlags[bg] & 1) || (bgActor->actor != actor) ||
                (bgActor->colHeader == NULL)) {
                continue;
            }
            {
                CollisionHeader* h = bgActor->colHeader;
                f32 rx = (f32)((h->maxBounds.x - h->minBounds.x)) * 0.5f * sx;
                f32 rz = (f32)((h->maxBounds.z - h->minBounds.z)) * 0.5f * sx;

                rings[0].y = (s16)((f32)h->minBounds.y * sy);
                rings[1].y = (s16)((f32)h->maxBounds.y * sy);
                rings[0].radius = (s16)((rx > rz) ? rx : rz);
                rings[1].radius = rings[0].radius;
                if (rings[0].radius < 3) {
                    rings[0].radius = rings[1].radius = 3;
                }
                if (rings[1].y <= rings[0].y) {
                    rings[1].y = (s16)(rings[0].y + (rings[0].radius * 2));
                }
                stacks[0].first = 0;
                stacks[0].count = 2;
                stacks[0].capBottom = 1;
                stacks[0].capTop = 1;
                *stackCount = 1;
                return 2;
            }
        }
    }

    rings[0].y = 0;
    rings[0].radius = (actor->colChkInfo.cylRadius > 0) ? actor->colChkInfo.cylRadius : STASIS_FALLBACK_RADIUS;
    rings[1].y = (actor->colChkInfo.cylHeight > 0) ? actor->colChkInfo.cylHeight : STASIS_FALLBACK_HEIGHT;
    rings[1].radius = rings[0].radius;
    if (rings[1].y <= rings[0].y) {
        rings[1].y = (s16)(rings[0].radius * 2);
    }
    stacks[0].first = 0;
    stacks[0].count = 2;
    stacks[0].capBottom = 1;
    stacks[0].capTop = 1;
    *stackCount = 1;
    return 2;
}

// Is there room in the scene's dynapoly pool for one more body?
//
// That pool is SHARED and small — z_bgcheck.c gives a scene either 256 or 512 polys and the same
// number of vertices, for every moving platform, door and gate in it at once. DynaPoly_ExpandSRT
// asserts on overflow, and an assert that is compiled out in a release build is a write past the
// end of the list. So the profile is only registered if it genuinely fits; a scene that is already
// full simply gets no climbable box, which is a missing feature rather than a corrupted heap.
static s32 Stasis_DynaHasRoom(PlayState* play, s32 needVtx, s32 needPoly) {
    DynaCollisionContext* dyna = &play->colCtx.dyna;
    s32 usedVtx = 0;
    s32 usedPoly = 0;
    s32 i;

    for (i = 0; i < BG_ACTOR_MAX; i++) {
        if ((dyna->bgActorFlags[i] & 1) && (dyna->bgActors[i].colHeader != NULL)) {
            usedVtx += dyna->bgActors[i].colHeader->numVertices;
            usedPoly += dyna->bgActors[i].colHeader->numPolygons;
        }
    }
    return ((usedVtx + needVtx) <= dyna->vtxListMax) && ((usedPoly + needPoly) <= dyna->polyListMax);
}

// The body's real dimensions, in world units relative to its own position: bottom, top, the widest
// radius, and the height that widest ring sits at. Same source as the collision shell, so "where
// the blow landed", "what you can climb" and "what it bumps into in flight" can never disagree.
static void Stasis_VisualBounds(Actor* actor, f32* loY, f32* hiY, f32* maxR, f32* maxRY) {
    StasisRing ring[STASIS_MAX_RINGS];
    StasisStack stack[STASIS_MAX_STACKS];
    s32 stackCount;
    s32 count = Stasis_GetProfile(actor, ring, stack, &stackCount);
    s32 i;

    // Rings run bottom to top WITHIN a stack, but two stacks interleave, so scan them all.
    *loY = (f32)ring[0].y;
    *hiY = (f32)ring[0].y;
    *maxR = (f32)ring[0].radius;
    *maxRY = (f32)ring[0].y;
    for (i = 1; i < count; i++) {
        if ((f32)ring[i].y < *loY) {
            *loY = (f32)ring[i].y;
        }
        if ((f32)ring[i].y > *hiY) {
            *hiY = (f32)ring[i].y;
        }
        if ((f32)ring[i].radius > *maxR) {
            *maxR = (f32)ring[i].radius;
            *maxRY = (f32)ring[i].y;
        }
    }
}

// Builds and registers the box. Returns the new bgId, or -1 if the actor has nothing to build from.
static s32 Stasis_BuildCollision(PlayState* play, Actor* actor, u8 climbable) {
    StasisRing ring[STASIS_MAX_RINGS];
    StasisStack stack[STASIS_MAX_STACKS];
    s32 stackCount;
    s32 count;
    s32 nVtx = 0;
    s32 nPoly = 0;
    s16 minY;
    s16 maxY;
    s16 maxR;
    // The carrier is registered at scale 1, so the dimensions go in as-is — Stasis_GetProfile has
    // already folded the frozen actor's own scale into them.
    s32 needPoly = 0;
    s32 r;
    s32 i;
    s32 s;

    count = Stasis_GetProfile(actor, ring, stack, &stackCount);
    if ((count < 2) || (stackCount < 1)) {
        return -1;
    }
    for (s = 0; s < stackCount; s++) {
        needPoly += (stack[s].count - 1) * STASIS_RING_SIDES * 2;
        needPoly += (stack[s].capBottom ? (STASIS_RING_SIDES - 2) : 0);
        needPoly += (stack[s].capTop ? (STASIS_RING_SIDES - 2) : 0);
    }
    if (!Stasis_DynaHasRoom(play, STASIS_RING_SIDES * count, needPoly)) {
        return -1;
    }

    minY = maxY = ring[0].y;
    maxR = ring[0].radius;
    for (r = 0; r < count; r++) {
        for (i = 0; i < STASIS_RING_SIDES; i++) {
            sStasisVtxPool[nVtx].x = (s16)(sStasisRingX[i] * (f32)ring[r].radius);
            sStasisVtxPool[nVtx].y = ring[r].y;
            sStasisVtxPool[nVtx].z = (s16)(sStasisRingZ[i] * (f32)ring[r].radius);
            nVtx++;
        }
        if (ring[r].radius > maxR) {
            maxR = ring[r].radius;
        }
        if (ring[r].y < minY) {
            minY = ring[r].y;
        }
        if (ring[r].y > maxY) {
            maxY = ring[r].y;
        }
    }

    for (s = 0; s < stackCount; s++) {
        s32 base = stack[s].first;
        s32 last = base + stack[s].count - 1;

        // Frustum walls between consecutive rings. Winding is lower[k] -> upper[k] -> upper[k+1]
        // and lower[k] -> upper[k+1] -> lower[k+1], which gives an outward normal for both
        // triangles; the engine reads normal.y to tell floor from wall from ceiling, so an inverted
        // shell is a ceiling you fall through.
        for (r = base; r < last; r++) {
            s32 lo = r * STASIS_RING_SIDES;
            s32 hi = (r + 1) * STASIS_RING_SIDES;

            for (i = 0; i < STASIS_RING_SIDES; i++) {
                s32 j = (i + 1) % STASIS_RING_SIDES;

                Stasis_MakePoly(&sStasisPolyPool[nPoly++], lo + i, hi + i, hi + j);
                Stasis_MakePoly(&sStasisPolyPool[nPoly++], lo + i, hi + j, lo + j);
            }
        }

        // Fans closing the ends. The two windings are mirrored so the bottom faces down and the top
        // faces up.
        for (i = 1; i + 1 < STASIS_RING_SIDES; i++) {
            if (stack[s].capBottom) {
                s32 lo = base * STASIS_RING_SIDES;

                Stasis_MakePoly(&sStasisPolyPool[nPoly++], lo, lo + i, lo + i + 1);
            }
            if (stack[s].capTop) {
                s32 hi = last * STASIS_RING_SIDES;

                Stasis_MakePoly(&sStasisPolyPool[nPoly++], hi, hi + i + 1, hi + i);
            }
        }
    }

    // One surface type, and we own it — so the climbable wall type is baked straight in rather than
    // patched into a shared, cached scene resource.
    //
    // It has to be zeroed for a body that is not meant to be climbed: the baked value is read by
    // the engine's own SurfaceType_GetWallFlags, so leaving 4 here would make a frozen pot climbable
    // through the vanilla path no matter what Stasis_IsClimbableBgId says.
    // Bits 0..7 of data[0] are the camera index — it must stay 0, which is the one entry above.
    sStasisSurfPool[0].data[0] = (u32)((climbable ? STASIS_WALL_TYPE_CLIMBABLE : 0) << 21);
    sStasisSurfPool[0].data[1] = 0;

    sStasisHeader.minBounds.x = (s16)-maxR;
    sStasisHeader.minBounds.y = minY;
    sStasisHeader.minBounds.z = (s16)-maxR;
    sStasisHeader.maxBounds.x = maxR;
    sStasisHeader.maxBounds.y = maxY;
    sStasisHeader.maxBounds.z = maxR;
    sStasisHeader.numVertices = (u16)nVtx;
    sStasisHeader.vtxList = sStasisVtxPool;
    sStasisHeader.numPolygons = (u16)nPoly;
    sStasisHeader.polyList = sStasisPolyPool;
    sStasisHeader.surfaceTypeList = sStasisSurfPool;
    sStasisCamData[0].cameraSType = 0;
    sStasisCamData[0].numCameras = 0;
    sStasisCamData[0].camPosData = NULL;
    sStasisHeader.cameraDataList = sStasisCamData;
    sStasisHeader.cameraDataListLen = 1;
    sStasisHeader.numWaterBoxes = 0;
    sStasisHeader.waterBoxes = NULL;

    // Zeroed first: the engine writes bgId/interactFlags through the DynaPolyActor cast, and stale
    // values from a previous freeze would be read back as live state.
    memset(&sStasisCarrier, 0, sizeof(sStasisCarrier));
    sStasisCarrier.actor.update = Stasis_CarrierUpdate;
    sStasisCarrier.actor.scale.x = sStasisCarrier.actor.scale.y = sStasisCarrier.actor.scale.z = 1.0f;
    Stasis_CarrierSync(actor);

    return DynaPoly_SetBgActor(play, &play->colCtx.dyna, &sStasisCarrier.actor, &sStasisHeader);
}

static u8 Stasis_IsFreezableEnemy(Actor* actor) {
    if (actor->category != ACTORCAT_ENEMY) {
        return 0;
    }
    // MASS_IMMOVABLE inside ACTORCAT_ENEMY is how scripted minibosses mark themselves; it means
    // something different on props, which is why this test is category-scoped.
    if (actor->colChkInfo.mass == MASS_IMMOVABLE) {
        return 0;
    }
    return !Stasis_IdInList(actor->id, sStasisEnemyBlacklist, ARRAY_COUNT(sStasisEnemyBlacklist));
}

static u8 Stasis_IsFreezableProp(Actor* actor) {
    // En_Ishi type 0 is the small rock Link simply picks up — only the type-1 boulder is worth
    // freezing (the type is bit 0 of params in both games).
    if (actor->id == ACTOR_EN_ISHI) {
        return (actor->params & 1) == 1;
    }
    // Wood02's bush/leaf types carry no collider at all, so there is nothing to freeze or throw.
    // 0x0A is WOOD_TREE_KAKARIKO_ADULT, the last type that builds one; the enum lives in the
    // actor's own overlay header, which mods cannot include.
    if (actor->id == ACTOR_EN_WOOD02) {
        return (actor->params & 0xFF) <= 0x0A;
    }
    return Stasis_IdInList(actor->id, sStasisPropIds, ARRAY_COUNT(sStasisPropIds));
}

// Is this a body, or is it the room?
//
// Ultrahand had to answer exactly this question and its answer is reasoned row by row — rotating
// wall quadrants with Link standing on them, the water PLANE, twisted corridors, the Shadow Temple
// ferry that carries you. Reading its table beats keeping a second one that drifts.
static u8 Stasis_IsStructure(Actor* actor) {
    return (Pacci_UhTraits(actor) & PACCI_UH_TRAIT_EXCLUDE) != 0;
}

// Freezing an NPC is a gag right up until it is a softlock.
//
// Player_Action_Talk only ever exits on TEXT_STATE_CLOSING (z_player.c), it has no timeout, and the
// thing that walks a conversation from page to page is the NPC'S OWN UPDATE. Switch that off with a
// textbox open and nobody ever closes it: Link is stuck talking to a statue, forever. So rather than
// a blacklist of ids — which is never complete and says nothing about WHY — these are the states in
// which no NPC may be taken.
static u8 Stasis_NpcIsSafeToFreeze(PlayState* play, Actor* actor) {
    Player* player = GET_PLAYER(play);

    if ((player != NULL) && (player->talkActor == actor)) {
        return 0;
    }
    if (Player_InCsMode(play) || (play->csCtx.state != CS_STATE_IDLE)) {
        return 0;
    }
    return Message_GetState(&play->msgCtx) == TEXT_STATE_NONE;
}

// Returns the STASIS_KIND_* this actor would freeze as, or STASIS_KIND_NONE.
static u8 Stasis_Classify(PlayState* play, Actor* actor, s32* bgIdOut) {
    CollisionHeader* hdr;
    s32 bgId = -1;

    if (bgIdOut != NULL) {
        *bgIdOut = -1;
    }
    if ((actor == NULL) || (actor->update == NULL) || (actor->id == ACTOR_PLAYER)) {
        return STASIS_KIND_NONE;
    }
    if (actor->category == ACTORCAT_BOSS) {
        return STASIS_KIND_NONE;
    }
    // Invisible triggers, spawn points and cutscene markers: freezing one moves something the
    // player cannot see, which always reads as a bug.
    if (actor->draw == NULL) {
        return STASIS_KIND_NONE;
    }
    // Attached to something else, so its owner decides when it dies — and Actor_Kill overwrites
    // `update`, which is precisely where our frozen update lives.
    if ((actor->parent != NULL) || (actor->child != NULL)) {
        return STASIS_KIND_NONE;
    }
    if (Stasis_IsStructure(actor)) {
        return STASIS_KIND_NONE;
    }
    if ((actor->category == ACTORCAT_NPC) && !Stasis_NpcIsSafeToFreeze(play, actor)) {
        return STASIS_KIND_NONE;
    }
    // Big enough to be worth the rune at all. Everything below this line has already earned a real
    // measurement or is on a hand-written list, so the gate goes here and only here.
    if (Stasis_MeasureHeight(play, actor) < STASIS_MIN_HEIGHT) {
        return STASIS_KIND_NONE;
    }

    if (Stasis_IsFreezableEnemy(actor)) {
        return STASIS_KIND_ENEMY;
    }

    // BLOCK is tested before the generic dynapoly rule below, or every pushblock would come out
    // climbable.
    if (Stasis_IdInList(actor->id, sStasisBlockIds, ARRAY_COUNT(sStasisBlockIds))) {
        Stasis_FindBg(play, actor, bgIdOut);
        return STASIS_KIND_BLOCK;
    }

    if (Stasis_IsFreezableProp(actor)) {
        return STASIS_KIND_PROP;
    }

    hdr = Stasis_FindBg(play, actor, &bgId);
    if (hdr != NULL) {
        f32 halfHeight = ((f32)hdr->maxBounds.y - (f32)hdr->minBounds.y) * 0.5f * actor->scale.y;

        if (bgIdOut != NULL) {
            *bgIdOut = bgId;
        }
        // Big enough to be worth climbing; anything smaller behaves like a block.
        return (halfHeight >= STASIS_CLIMB_MIN_HALF_HEIGHT) ? STASIS_KIND_CLIMBABLE : STASIS_KIND_BLOCK;
    }

    // Everything else that got this far: it is visible, unattached, not architecture, not a boss,
    // and big enough to have been measured. That is the whole rule — the lists above are now about
    // HOW a body behaves, not about whether it may be taken.
    return STASIS_KIND_PROP;
}

static s32 Stasis_TargetFilter(Actor* actor) {
    return Stasis_Classify(gPlayState, actor, NULL) != STASIS_KIND_NONE;
}

// ACTORCAT_EXPLOSIVE is not decoration here: En_Bom and friends have been on the prop list for
// several rounds and were UNREACHABLE the whole time, because this array is the outer gate and it
// never named their category. Nothing in the list itself could have shown that.
//
// Deliberately absent: SWITCH (they are what the switch magnet aims AT — freezing a plate would
// stop the very press we built), DOOR and CHEST (both own transitions that softlock if paused),
// ITEMACTION (arrows, hookshot, effects: those are the attack, not the target), BOSS and PLAYER.
static const u8 sStasisCats[6] = {
    ACTORCAT_ENEMY, ACTORCAT_PROP, ACTORCAT_BG, ACTORCAT_NPC, ACTORCAT_EXPLOSIVE, ACTORCAT_MISC,
};

// ============================================================================
// FREEZE / THAW
// ============================================================================

// The replacement update. It does the one thing a frozen body still has to do: swallow incoming
// hits, bank them, and make sure the actor itself never reacts.
static void Stasis_FrozenUpdate(Actor* actor, PlayState* play) {
    if (!sStasis.colliderReady || (sStasis.actor != actor)) {
        return;
    }

    // Held completely still — but ONLY while frozen. The takeover is kept through the launch, so
    // zeroing unconditionally here would cancel the throw the instant it started.
    if (sStasis.phase == STASIS_PHASE_FROZEN) {
        actor->velocity.x = actor->velocity.y = actor->velocity.z = 0.0f;
        actor->speedXZ = 0.0f;
    }
}

// The tint. Wrapping the actor's own draw is the only way to recolour a foreign actor: the engine's
// colour filter is three hardcoded modes (white/red/blue) and structurally cannot make yellow, so
// the gold comes from the grayscale tint, whose 4th argument is a blend weight — free pulsing.
// Yellow when idle, red when fully charged.
static void Stasis_ChargeColor(f32 t, u8* r, u8* g, u8* b) {
    *r = (u8)(STASIS_TINT_R + ((f32)(STASIS_TINT_HOT_R - STASIS_TINT_R) * t));
    *g = (u8)(STASIS_TINT_G + ((f32)(STASIS_TINT_HOT_G - STASIS_TINT_G) * t));
    *b = (u8)(STASIS_TINT_B + ((f32)(STASIS_TINT_HOT_B - STASIS_TINT_B) * t));
}

static void Stasis_TintDraw(Actor* actor, PlayState* play) {
    ActorFunc inner = NULL;
    f32 pulse;
    u8 lerp;
    u8 tr;
    u8 tg;
    u8 tb;

    if ((sStasis.actor == actor) && (sStasis.origDraw != NULL)) {
        inner = sStasis.origDraw;
        // Held: full strength. The blend weight is what decides how much of the original survives,
        // and anything below ~230 reads as "slightly warm" rather than "this is in stasis".
        pulse = 0.5f + (0.5f * Math_SinS((s16)(sStasis.age * 0x900)));
        lerp = (u8)(235.0f + (20.0f * pulse));
        Stasis_ChargeColor(Stasis_ChargeFraction(), &tr, &tg, &tb);
    } else if ((sStasisOffer == actor) && (sStasisOfferDraw != NULL)) {
        inner = sStasisOfferDraw;
        // Merely offered: a faster, weaker shimmer, so "aimed at" never reads as "already frozen".
        pulse = 0.5f + (0.5f * Math_SinS((s16)(play->gameplayFrames * 0x1800)));
        lerp = (u8)(110.0f + (70.0f * pulse));
        Stasis_ChargeColor(0.0f, &tr, &tg, &tb); // nothing is charged yet — plain gold
    } else {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    // The three steps, in the order the hardware runs them.
    //
    // STEP 2 — BRIGHTNESS. An actor's own materials pick their own combiners, so unlike the chains
    //   there is no combiner of ours to lift them with. The lever that DOES reach them is the
    //   light: Actor_Draw calls Lights_Draw immediately before actor->draw, so replacing that with
    //   a near-white ambient here overrides it for this model only, and every lit material comes
    //   out far brighter. This is what stops a dark tree tinting to olive instead of gold.
    // STEPS 1 & 3 — GRAYSCALE, then YELLOW. The shader pass runs AFTER the combiner
    //   (libultraship default.shader.hlsl: intensity = (r+g+b)/3, new_texel = grayscale.rgb *
    //   intensity), so it takes the brightened colour, flattens its hue, and multiplies by gold.
    gSPSetLights1(POLY_OPA_DISP++, sStasisLights);
    gSPSetLights1(POLY_XLU_DISP++, sStasisLights);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetGrayscaleColor(POLY_OPA_DISP++, tr, tg, tb, lerp);
    gSPGrayscale(POLY_OPA_DISP++, true);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, tr, tg, tb, lerp);
    gSPGrayscale(POLY_XLU_DISP++, true);
    CLOSE_DISPS(play->state.gfxCtx);

    inner(actor, play);

    OPEN_DISPS(play->state.gfxCtx);
    gSPGrayscale(POLY_OPA_DISP++, false);
    gSPGrayscale(POLY_XLU_DISP++, false);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Unregister the collision we built, if we built any. Only ever deletes what WE registered:
// deleting an actor's own bgId would leave a pushblock without collision for the rest of the scene.
static void Stasis_DropCollision(void) {
    if (sStasis.bgIsOurs && (sStasis.bgId >= 0) && (gPlayState != NULL)) {
        DynaPoly_DeleteBgActor(gPlayState, &gPlayState->colCtx.dyna, sStasis.bgId);
    }
    sStasis.bgIsOurs = 0;
    sStasis.bgId = -1;
}

// Hand the actor back exactly as we found it. Guarded on update != NULL throughout: that is the
// "did it die while we owned it" test.
// A prop that never moves does not keep its collider in sync — it places it once in Init and never
// again. ObjBombiwa_InitCollision is the textbook case: `Collider_UpdateCylinder` is called exactly
// once, at spawn, and then ObjBombiwa_Update submits that same stale cylinder every single frame.
// Fly one of those across the room and the boulder you SEE is not the boulder you can bomb: the
// model is at the landing spot and the hitbox is still standing where it spawned.
//
// So a body that has been moved gets its own colliders dragged along on landing. They are found by
// scanning its instance for the back-pointer every Collider keeps to its owner (`Collider.actor`,
// offset 0), which is what lets this work without knowing the private struct of each actor — and
// the actor's real allocated size comes from the ActorDB entry, so the scan never runs off the end.
// The scan itself, writing nowhere. The classifier runs over EVERY candidate in range before one is
// chosen, so it cannot be the thing that fills in sStasis — it would leave the live state describing
// a body we then decided not to freeze.
static s32 Stasis_ScanOwnColliders(Actor* actor, ColliderCylinder** out, s32 max) {
    ActorDBEntry* dbEntry = ActorDB_Retrieve(actor->id);
    size_t size = (dbEntry != NULL) ? dbEntry->instanceSize : 0;
    u8* base = (u8*)actor;
    size_t off;
    s32 count = 0;

    if (size <= sizeof(Actor)) {
        return 0;
    }

    for (off = sizeof(Actor); (off + sizeof(ColliderCylinder)) <= size; off += 4) {
        ColliderCylinder* cyl = (ColliderCylinder*)(base + off);

        // Three things have to agree before we write through a guessed pointer: it points back at
        // this actor, it calls itself a cylinder, and its radius is a plausible one.
        if ((cyl->base.actor != actor) || (cyl->base.shape != COLSHAPE_CYLINDER)) {
            continue;
        }
        if ((cyl->dim.radius <= 0) || (cyl->dim.radius > 4000)) {
            continue;
        }
        if (count < max) {
            out[count++] = cyl;
        }
    }
    return count;
}

static void Stasis_FindOwnColliders(Actor* actor) {
    sStasis.ownColliderCount = (u8)Stasis_ScanOwnColliders(actor, sStasis.ownCollider, STASIS_MAX_OWN_COLLIDERS);
}

// How tall this actor is, in world units, for the "is it big enough to bother with" gate.
//
// Only sources that are REALLY measured count. `colChkInfo.cylRadius/cylHeight` is deliberately not
// among them: CollisionCheck_InitInfo leaves it at 10x10 for anyone who never calls SetInfo, and
// where it IS filled in it is copy-paste — {0, 12, 60} is the same literal in a pot, a crate, a
// bomb-rock, a rolling boulder and a pebble. It cannot tell large from small at all.
static f32 Stasis_MeasureHeight(PlayState* play, Actor* actor) {
    ColliderCylinder* cyls[STASIS_MAX_OWN_COLLIDERS];
    CollisionHeader* hdr;
    s32 count;
    s32 i;
    f32 tallest = 0.0f;

    // A body we have a profile row for is measured geometry from the o2r, and every one of them is
    // large. This tier exists for the trees above all: their collider is 18x60 and their model is
    // 480 tall, so the cylinder tier below would badly undersell them.
    for (i = 0; i < (s32)ARRAY_COUNT(sStasisBodies); i++) {
        if (sStasisBodies[i].id != actor->id) {
            continue;
        }
        if ((sStasisBodies[i].paramsMax >= 0) && ((actor->params & 0xFF) > sStasisBodies[i].paramsMax)) {
            continue;
        }
        return (f32)(sStasisBodies[i].ring[sStasisBodies[i].ringCount - 1].y - sStasisBodies[i].ring[0].y) *
               fabsf(actor->scale.y);
    }

    // Its own registered collision: exact, and the whole of ACTORCAT_BG has it.
    hdr = Stasis_FindBg(play, actor, NULL);
    if (hdr != NULL) {
        return ((f32)hdr->maxBounds.y - (f32)hdr->minBounds.y) * fabsf(actor->scale.y);
    }

    // Its own cylinder. Already in WORLD units — Collider_SetCylinderDim copies verbatim and
    // nothing scales it — so unlike the profile table this must NOT be multiplied by actor->scale.
    count = Stasis_ScanOwnColliders(actor, cyls, STASIS_MAX_OWN_COLLIDERS);
    for (i = 0; i < count; i++) {
        if ((f32)cyls[i]->dim.height > tallest) {
            tallest = (f32)cyls[i]->dim.height;
        }
    }
    return tallest;
}

// Keep the body hittable while its update is off.
//
// Submitting the actor's OWN colliders rather than a substitute is the whole point: they already
// carry the right colType and AC_HARD, so a sword on a frozen boulder gives the vanilla clonk and
// the vanilla bounce, for free and exactly in character. Our synthetic collider is only the
// fallback for bodies whose colliders we cannot find (enemies are usually jointed spheres).
static void Stasis_SubmitColliders(PlayState* play, Actor* actor) {
    s32 i;

    for (i = 0; i < sStasis.ownColliderCount; i++) {
        ColliderCylinder* cyl = sStasis.ownCollider[i];

        Collider_UpdateCylinder(actor, cyl);
        if (cyl->base.acFlags & AC_ON) {
            CollisionCheck_SetAC(play, &play->colChkCtx, &cyl->base);
        }
        // OC too, so a frozen body is still something Link walks into instead of through.
        if (cyl->base.ocFlags1 & OC1_ON) {
            CollisionCheck_SetOC(play, &play->colChkCtx, &cyl->base);
        }
    }

    // Ours covers whatever is left above them — the crown of a tree, and the whole body of anything
    // whose colliders we could not find at all.
    if (sStasis.colliderReady && sStasis.extraCollider) {
        Collider_UpdateCylinder(actor, &sStasis.collider);
        CollisionCheck_SetAC(play, &play->colChkCtx, &sStasis.collider.base);
    }
}

static void Stasis_RelocateActor(Actor* actor) {
    s32 i;

    // `home` is where a prop believes it belongs: ObjBombiwa_Init reads home.pos.y, En_Ishi returns
    // to it, En_Wood02 measures its despawn distance from it. Moving it makes the landing spot the
    // body's real new home — which is what "it comes back with everything intact, right there"
    // has to mean for a prop that outlives the throw.
    actor->home.pos = actor->world.pos;

    for (i = 0; i < sStasis.ownColliderCount; i++) {
        Collider_UpdateCylinder(actor, sStasis.ownCollider[i]);
    }
}

static void Stasis_RestoreActor(void) {
    Actor* actor = sStasis.actor;

    Stasis_DropCollision();

    if ((actor != NULL) && (actor->update != NULL)) {
        // Only a body that actually flew gets re-homed. An enemy that simply thawed must keep the
        // home it patrols around, and a rock that was never launched is already where it belongs.
        if (sStasis.phase == STASIS_PHASE_FLYING) {
            Stasis_RelocateActor(actor);
        }
        actor->update = sStasis.origUpdate;
        if (sStasis.origDraw != NULL) {
            actor->draw = sStasis.origDraw;
        }
        actor->flags = sStasis.origFlags;
        actor->gravity = sStasis.origGravity;
        actor->minVelocityY = sStasis.origMinVelocityY;
        actor->shape.rot = sStasis.origShapeRot;
        actor->world.rot = sStasis.origWorldRot;
        actor->room = (s8)sStasis.origRoom;
        actor->colChkInfo.mass = sStasis.origMass;
        actor->speedXZ = 0.0f;
        actor->velocity.x = actor->velocity.y = actor->velocity.z = 0.0f;
        // AFTER the flags are restored, or that line would strip the press flag right as it lands.
        // A body that earned it keeps it: a statue that presses switches goes on pressing switches.
        SwitchMagnet_MakePresser(actor);
    }
}

// Drop every pointer WITHOUT writing through them. The scene-change path: by the time this runs the
// actors are gone and restoring their state would be a use-after-free.
void Stasis_Forget(void) {
    sStasis.actor = NULL;
    sStasis.kind = STASIS_KIND_NONE;
    sStasis.phase = STASIS_PHASE_FROZEN;
    sStasis.timer = 0;
    sStasis.chainTimer = 0;
    sStasis.age = 0;
    sStasis.accumDamage = 0;
    sStasis.lastDmgFlags = 0;
    sStasis.lastDmgEffect = 0;
    sStasis.lastDmgAmount = 0;
    sStasis.force = 0.0f;
    sStasis.hitDir.x = sStasis.hitDir.y = sStasis.hitDir.z = 0.0f;
    sStasis.hasHitDir = 0;
    sStasis.bgId = -1;
    sStasis.riderAttached = 0;
    // These point INTO the actor's instance, so they die with it. A scene change frees that memory
    // out from under us, and a stale entry here would be submitted to the collision list.
    sStasis.ownColliderCount = 0;
    sStasis.extraCollider = 0;
    sStasis.slideLaunch = 0;
}

// The launch direction as a unit vector, in full 3D. Falls back to "away from Link, level" when
// nothing has struck it yet.
// A pushable block does not go diagonally, and it never has.
//
// The whole puzzle is WHICH WAY along its own grid a block travels; letting one drift off at 37
// degrees because that is where the blow came from turns a lattice of squares into something you
// cannot line up with anything. So a body that keeps its pose also keeps its axes: the direction is
// rounded to the nearest of its own four faces, and flattened, so it can only ever go +X, -X, +Z or
// -Z RELATIVE TO ITSELF. The arrow shows exactly that, because the arrow reads this same function.
static void Stasis_AxisLockDir(Actor* actor, Vec3f* dir) {
    s16 yaw = (s16)(Math_FAtan2F(dir->x, dir->z) * (0x8000 / M_PI));
    s16 rel = yaw - actor->shape.rot.y;
    s16 snapped;

    // Rounding to the nearest quarter turn: the half-step bias is what makes it round rather than
    // truncate toward the block's own facing.
    rel = (s16)((rel + 0x2000) & (s16)0xC000);
    snapped = (s16)(actor->shape.rot.y + rel);

    dir->x = Math_SinS(snapped);
    dir->y = 0.0f;
    dir->z = Math_CosS(snapped);
}

static void Stasis_LaunchDir(PlayState* play, Vec3f* out) {
    Actor* actor = sStasis.actor;

    if (sStasis.hasHitDir) {
        *out = sStasis.hitDir;
        if (Stasis_KeepsItsPose(actor)) {
            Stasis_AxisLockDir(actor, out);
        }
        return;
    }
    {
        Player* player = GET_PLAYER(play);
        f32 dx = actor->world.pos.x - player->actor.world.pos.x;
        f32 dz = actor->world.pos.z - player->actor.world.pos.z;
        f32 len = sqrtf((dx * dx) + (dz * dz));

        if (len > 0.001f) {
            out->x = dx / len;
            out->z = dz / len;
        } else {
            out->x = 0.0f;
            out->z = 1.0f;
        }
        out->y = 0.0f;
    }
    if (Stasis_KeepsItsPose(actor)) {
        Stasis_AxisLockDir(actor, out);
    }
}

// How charged it is, 0..1. Drives both the arrow's length and the yellow-to-red fade.
static f32 Stasis_ChargeFraction(void) {
    f32 f = sStasis.force / (STASIS_LAUNCH_MAX - STASIS_LAUNCH_BASE);

    return (f > 1.0f) ? 1.0f : ((f < 0.0f) ? 0.0f : f);
}

static f32 Stasis_LaunchSpeed(void) {
    f32 speed = STASIS_LAUNCH_BASE + sStasis.force;

    return (speed > STASIS_LAUNCH_MAX) ? STASIS_LAUNCH_MAX : speed;
}

// End the stasis: enemies take their stored beating, everything else flies.
// Was Link on the body when it went off? Three ways to be holding on, and all three count: standing
// on its lid, gripping its wall (the climb the whole climbable rule exists for), or registered as
// the dynapoly rider by the engine itself.
// Bodies that must come out of this facing the way they went in.
//
// A pushable block is something the player LINED UP; a block that has been spun 40 degrees no
// longer fits the slot it was meant for, and neither does an ice platform that has to bridge a gap
// square. Ultrahand solved this already and its table is the shared answer — see the NO_TURN and
// NO_FACE rows in cane_pacci.c. Everything on the BLOCK path counts too, whether or not it has a
// row, because being shoved along the floor is exactly the case where a spin looks wrong.
static u8 Stasis_KeepsItsPose(Actor* actor) {
    if (sStasis.kind == STASIS_KIND_BLOCK) {
        return 1;
    }
    return (Pacci_UhTraits(actor) & (PACCI_UH_TRAIT_NO_TURN | PACCI_UH_TRAIT_NO_FACE)) != 0;
}

// One frame of open flame, if the body is something that burns.
//
// Same idea as Pacci_UhFireTick and deliberately the same list — a torch is a torch whether the
// cane is holding it or the rune has it stopped in mid-air. What differs is the size: Ultrahand has
// to fall back to a 12-unit cube for a body with no collision geometry, while we already measured
// the thing exactly, so the flame is the body's own silhouette with a little reach past it.
//
// The condition column matters here as much as the row does: Pacci_UhTraits returns nothing at all
// for an UNLIT torch, so freezing one does not set the room on fire.
#define STASIS_FIRE_DAMAGE 4 // two hearts, where the enemy's table defers to the toucher
#define STASIS_FIRE_EFFECT 1 // the fire slot in the vanilla damage-effect tables
#define STASIS_FIRE_PAD 12.0f

static ColliderCylinder sStasisFireCol;
static Actor* sStasisFireOwner = NULL;

static void Stasis_FireTick(PlayState* play, Actor* actor) {
    CombatColliderConfig cfg;
    Vec3f pos;
    f32 loY;
    f32 hiY;
    f32 maxR;
    f32 maxRY;

    if ((actor == NULL) || (actor->update == NULL) || !(Pacci_UhTraits(actor) & PACCI_UH_TRAIT_BURNS)) {
        sStasisFireOwner = NULL;
        return;
    }

    Stasis_VisualBounds(actor, &loY, &hiY, &maxR, &maxRY);
    cfg.dmgFlags = DMG_FIRE;
    cfg.damage = STASIS_FIRE_DAMAGE;
    cfg.effect = STASIS_FIRE_EFFECT;
    cfg.radius = maxR + STASIS_FIRE_PAD;
    cfg.height = (hiY - loY) + STASIS_FIRE_PAD;

    // Re-armed whenever the body changes: Collider_SetCylinder bakes the owner in, and the burn has
    // to be attributed to the flame rather than to whatever was frozen last.
    if (sStasisFireOwner != actor) {
        Combat_InitCylinder(play, &sStasisFireCol, actor, &cfg);
        sStasisFireOwner = actor;
    }
    // A cylinder is positioned by its BASE, not its centre.
    pos.x = actor->world.pos.x;
    pos.y = actor->world.pos.y + loY - (STASIS_FIRE_PAD * 0.5f);
    pos.z = actor->world.pos.z;
    Combat_UpdateCylinder(&sStasisFireCol, &pos, &cfg);
    Combat_RegisterCollider(play, &sStasisFireCol);
    // AT_HIT has to be cleared by hand or the collider counts as spent and stops landing hits.
    if (Combat_CheckHit(&sStasisFireCol)) {
        sStasisFireCol.base.atFlags &= ~AT_HIT;
    }
}

// Everything the enemy took while it was held, delivered as ONE blow carrying the type of the LAST
// one — 20 of sword finished with an ice arrow arrives as 22, and freezes it.
//
// The old version simply subtracted colChkInfo.health and played a thud. That is not a hit: the
// enemy never notices, so it does not flinch, does not burn, does not freeze, does not die and
// drops nothing — it just walks around hollow until something else grazes it. Enemies read exactly
// one field to choose between fire, ice, stun and an ordinary blow (`colChkInfo.damageEffect`), and
// that field is a nibble of the enemy's OWN damage table indexed by the attacker's dmgFlags. So we
// do not compute it: we stage a real hit and let CollisionCheck_ApplyDamage read the table.
//
// The attacker we present is a static of our own. Nothing in the engine compares the address of an
// acHitInfo or can walk from one back to an actor, so it is indistinguishable from a real one — and
// unlike the arrow that actually landed the blow, it cannot have been freed in the meantime.
static ColliderInfo sStasisFakeAtInfo;
static Collider sStasisFakeAtCollider;

static void Stasis_DeliverStoredHit(PlayState* play, Actor* actor) {
    Player* player = GET_PLAYER(play);
    ColliderCylinder* target;
    s32 damage;

    if ((actor == NULL) || (actor->update == NULL) || (sStasis.accumDamage == 0)) {
        return;
    }
    // No damage type was ever captured, so there is no table entry to look up. Delivering with
    // dmgFlags of 0 would walk the engine's bit search off the end of the table.
    if (sStasis.lastDmgFlags == 0) {
        return;
    }
    if (sStasis.ownColliderCount == 0) {
        return; // nothing of its own to land on
    }
    target = sStasis.ownCollider[0];

    sStasisFakeAtInfo.toucher.dmgFlags = sStasis.lastDmgFlags;
    sStasisFakeAtInfo.toucher.effect = sStasis.lastDmgEffect;
    sStasisFakeAtInfo.toucher.damage = sStasis.lastDmgAmount;
    sStasisFakeAtInfo.toucherFlags = TOUCH_ON;
    sStasisFakeAtCollider.actor = &player->actor;

    // `ac` must be a LIVE actor: Poe, Floormaster, Wallmaster, Iron Knuckle and a dozen others
    // dereference it with no NULL check. Link is the honest answer anyway — he did the damage.
    target->base.acFlags |= AC_HIT;
    target->base.acFlags &= ~AC_BOUNCED;
    target->base.ac = &player->actor;
    target->info.acHit = &sStasisFakeAtCollider;
    target->info.acHitInfo = &sStasisFakeAtInfo;
    target->info.bumperFlags |= BUMP_HIT;
    target->info.bumper.hitPos.x = (s16)actor->world.pos.x;
    target->info.bumper.hitPos.y = (s16)actor->world.pos.y;
    target->info.bumper.hitPos.z = (s16)actor->world.pos.z;

    actor->colChkInfo.damage = 0;
    CollisionCheck_ApplyDamage(play, &play->colChkCtx, &target->base, &target->info);

    // ApplyDamage has now filled in damageEffect from the enemy's own table for the last blow's
    // type, which is the half we cannot compute. The damage it also wrote is that ONE blow's worth,
    // so it gets replaced by the running total — the effect stays, the number becomes the bill.
    damage = (s32)sStasis.accumDamage;
    actor->colChkInfo.damage = (u8)((damage > 255) ? 255 : damage);

    // Health is deliberately NOT touched. The enemy calls Actor_ApplyDamage itself and watches for
    // the transition to zero; that observation is its universal "I just died" signal, and taking it
    // away is what stopped anything from dying here.
}

static void Stasis_GrabRider(PlayState* play, Actor* actor) {
    Player* player = GET_PLAYER(play);

    sStasis.riderAttached = 0;
    if (sStasis.bgId < 0) {
        return;
    }

    if ((player->actor.floorBgId != sStasis.bgId) && (player->actor.wallBgId != sStasis.bgId) &&
        !(sStasis.bgIsOurs && DynaPolyActor_IsPlayerOnTop(&sStasisCarrier))) {
        // Climbing is the case the bgId fields miss. Making these bodies climbable is the whole
        // point of the rune, but once Link is IN the climb his action func drives him off its own
        // stored wall poly and stops refreshing `wallBgId` — so the one state where he is most
        // obviously holding on is the one that reported he was not.
        //
        // So if he is climbing at all, ask geometry instead: is he up against THIS body's shell?
        if (player->stateFlags1 & PLAYER_STATE1_CLIMBING_LADDER) {
            f32 loY;
            f32 hiY;
            f32 maxR;
            f32 maxRY;
            f32 dx = player->actor.world.pos.x - actor->world.pos.x;
            f32 dz = player->actor.world.pos.z - actor->world.pos.z;
            f32 horiz = sqrtf((dx * dx) + (dz * dz));
            f32 py = player->actor.world.pos.y;

            Stasis_VisualBounds(actor, &loY, &hiY, &maxR, &maxRY);
            if ((horiz > (maxR + 30.0f)) || (py < (actor->world.pos.y + loY - 20.0f)) ||
                (py > (actor->world.pos.y + hiY + 20.0f))) {
                return; // climbing something else
            }
        } else {
            return;
        }
    }

    // Offset, not absolute position: he keeps whatever spot on the body he had earned, so a climb
    // halfway up a trunk stays halfway up the trunk all the way through the arc.
    sStasis.riderOffset.x = player->actor.world.pos.x - actor->world.pos.x;
    sStasis.riderOffset.y = player->actor.world.pos.y - actor->world.pos.y;
    sStasis.riderOffset.z = player->actor.world.pos.z - actor->world.pos.z;
    sStasis.riderAttached = 1;
}

// Glue him to it for the rest of the flight.
//
// This runs at the very end of Player_Update (that is where Slate_TickInput is called from), so it
// is the last word on his position for the frame: his own action func has already applied gravity
// and given up on the wall, and we simply put him back. The result reads as riding, not as a
// teleport, because the body moved this same frame too.
static void Stasis_CarryRider(PlayState* play, Actor* actor) {
    Player* player;

    if (!sStasis.riderAttached) {
        return;
    }
    player = GET_PLAYER(play);

    player->actor.world.pos.x = actor->world.pos.x + sStasis.riderOffset.x;
    player->actor.world.pos.y = actor->world.pos.y + sStasis.riderOffset.y;
    player->actor.world.pos.z = actor->world.pos.z + sStasis.riderOffset.z;
    // prevPos too, or next frame's bg check sweeps the whole arc as one movement and snags him on
    // the first wall along the way.
    player->actor.prevPos = player->actor.world.pos;

    // No fall speed to inherit: the ride ends with him standing on whatever the body landed on,
    // rather than eating the accumulated drop of a ten-second flight.
    player->actor.velocity.y = 0.0f;
    player->actor.speedXZ = 0.0f;
}

static void Stasis_End(PlayState* play) {
    Actor* actor = sStasis.actor;

    // The cue's tail IS the release, so it is only cut short when the stasis is broken early.
    if (sStasis.timer > 0) {
        StasisSfx_Stop();
    }

    if ((actor == NULL) || (actor->update == NULL)) {
        Stasis_Forget();
        return;
    }

    if (sStasis.kind == STASIS_KIND_ENEMY) {
        // Enemies are never launched (user-locked) — they just receive everything at once.
        //
        // The damage is applied directly rather than handed to the actor's own AC branch: that
        // branch only runs when the enemy's OWN collider reports AC_HIT, and ours is a different
        // collider. An enemy whose death is written inside that branch will therefore fall on the
        // next real hit rather than the instant it thaws, with its health already at zero.
        // Restore FIRST, then hit it: the blow has to land on an actor that is itself again, with
        // its own update back in place to react to it this very frame.
        Stasis_RestoreActor();
        Stasis_DeliverStoredHit(play, actor);
        Stasis_Forget();
        return;
    }

    if (sStasis.force <= 0.0f) {
        // Nothing charged it — it simply resumes.
        Stasis_RestoreActor();
        Stasis_Forget();
        return;
    }

    // Was Link holding on? Ask now, not later: the moment the body moves, the wall he was gripping
    // is no longer under his hands and his climb state ends on its own.
    Stasis_GrabRider(play, actor);

    // Launch. The engine derives velocity.x/z from world.rot.y + speedXZ, so those are what we set.
    // The takeover is KEPT for the flight: our update stays in place and Stasis_Update flies it,
    // exactly as the Pacci lift does with a thrown object.
    {
        Vec3f dir;
        f32 speed = Stasis_LaunchSpeed();
        f32 horiz;
        s16 yaw;

        Stasis_LaunchDir(play, &dir);

        // A pushable block is SHOVED, not thrown. It keeps its feet on the floor and travels the
        // way the blows pushed it, which is the only motion those actors were ever built for — a
        // block sailing through the air reads as a bug even when the physics are right.
        sStasis.slideLaunch = (sStasis.kind == STASIS_KIND_BLOCK);

        horiz = sqrtf((dir.x * dir.x) + (dir.z * dir.z));

        // Full 3D: the horizontal part of the blow becomes speedXZ along its yaw, the vertical part
        // becomes velocity.y, and gravity takes it from there. A hit arriving from below therefore
        // genuinely launches the body upward instead of skidding it along the floor.
        yaw = (horiz > 0.001f) ? Math_FAtan2F(dir.x, dir.z) * (0x8000 / M_PI) : actor->shape.rot.y;
        // world.rot.y is the direction of TRAVEL — Actor_MoveXZGravity derives velocity from it, so
        // it always gets set. shape.rot.y is which way the body FACES, and that is a different
        // question: a block or an ice platform must keep the pose it had. Ultrahand already draws
        // this line (PACCI_UH_TRAIT_NO_TURN / _NO_FACE) and this reads the same table, so the two
        // canes and the rune cannot drift apart on it.
        actor->world.rot.y = yaw;
        if (!Stasis_KeepsItsPose(actor)) {
            actor->shape.rot.y = yaw;
        }
        if (sStasis.slideLaunch) {
            // The whole blow goes into the shove, so a glancing upward hit still moves it properly
            // instead of being thrown away with the vertical component.
            actor->speedXZ = speed;
            actor->velocity.y = 0.0f;
            // Gravity stays on so it follows a sloped floor down instead of skating off into space.
            actor->gravity = STASIS_GRAVITY;
            actor->minVelocityY = STASIS_MIN_VELOCITY_Y;
        } else {
            actor->speedXZ = speed * horiz;
            actor->velocity.y = (speed * dir.y) + STASIS_LAUNCH_VEL_Y;
            actor->gravity = STASIS_GRAVITY;
            actor->minVelocityY = STASIS_MIN_VELOCITY_Y;
            actor->bgCheckFlags &= ~(BGCHECKFLAG_GROUND | BGCHECKFLAG_GROUND_TOUCH);
        }
    }

    // The collision flies WITH it. The carrier is re-parked on the body every frame, so the thing
    // stays solid and grabbable the whole way — and it is only handed back to the real actor when
    // it lands (Stasis_RestoreActor drops the carrier there).
    sStasis.phase = STASIS_PHASE_FLYING;
    sStasis.timer = STASIS_FLIGHT_FRAMES;
    Audio_PlaySoundGeneral(NA_SE_EV_HEAVY_THROW, &actor->world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

// Take the actor over.
static void Stasis_Begin(PlayState* play, Actor* target, u8 kind, s32 bgId) {
    // Drop the offer wrapper first: it is about to be replaced by the real one, and restoring it
    // afterwards would put the plain draw back over ours.
    Stasis_ClearOffer();
    Stasis_Forget();

    sStasis.actor = target;
    sStasis.kind = kind;
    sStasis.phase = STASIS_PHASE_FROZEN;
    sStasis.timer = (kind == STASIS_KIND_ENEMY) ? STASIS_FRAMES_ENEMY : STASIS_FRAMES_OBJECT;
    sStasis.chainTimer = (kind == STASIS_KIND_ENEMY) ? STASIS_CHAIN_FRAMES_ENEMY : STASIS_CHAIN_FRAMES;
    sStasis.bgId = (kind == STASIS_KIND_CLIMBABLE) ? bgId : -1;
    sStasis.bgIsOurs = 0;
    sStasis.riderAttached = 0;

    // A body with no dynapoly of its own gets one built for it. That is what makes a frozen boulder
    // read as SOLID — you can stand on it, it stops what it is hit with, and its wall is climbable
    // — instead of a cylinder Link walks through. Blocks and platforms already own real collision,
    // so they are left alone.
    if ((kind != STASIS_KIND_ENEMY) && (kind != STASIS_KIND_BLOCK) && (bgId < 0)) {
        f32 loY;
        f32 hiY;
        f32 maxR;
        f32 maxRY;
        u8 climbable;
        s32 newBgId;

        // The shell is built for EVERY body, exactly the way the tree gets one — that is what makes
        // a frozen thing solid, what makes arrows and swings land on it where you can see it rather
        // than on a collider a tenth its size, and what carries it and its rider through the throw.
        // Only the CLIMBABLE part is conditional: a pot gets the same shell as a tree, its walls
        // just are not something Link can hang from.
        Stasis_VisualBounds(target, &loY, &hiY, &maxR, &maxRY);
        climbable = (u8)((hiY - loY) >= STASIS_CLIMBABLE_MIN_HEIGHT);

        newBgId = Stasis_BuildCollision(play, target, climbable);
        if (newBgId >= 0) {
            sStasis.bgId = newBgId;
            sStasis.bgIsOurs = 1;
            if (climbable) {
                sStasis.kind = STASIS_KIND_CLIMBABLE; // it has a wall now, so it can be climbed
            }
        }
    }

    sStasis.origUpdate = target->update;
    sStasis.origDraw = target->draw;
    sStasis.origFlags = target->flags;
    sStasis.origGravity = target->gravity;
    sStasis.origMinVelocityY = target->minVelocityY;
    sStasis.origSpeed = target->speedXZ;
    sStasis.origShapeRot = target->shape.rot;
    sStasis.origWorldRot = target->world.rot;
    sStasis.origRoom = target->room;
    sStasis.origMass = target->colChkInfo.mass;

    // Its own colliders first. If it has them, they are what stays live while it is frozen, so a
    // sword on a boulder still lands the way the game already knows how to land it.
    Stasis_FindOwnColliders(target);
    // Forced re-arm: a scene change can free the last owner and hand a new actor the same address,
    // and a stale match here would skip the re-init and attribute the burn to the wrong body.
    sStasisFireOwner = NULL;

    if (!sStasis.colliderReady) {
        Collider_InitCylinder(play, &sStasis.collider);
        sStasis.colliderReady = 1;
    }
    Collider_SetCylinder(play, &sStasis.collider, target, &sStasisColliderInit);

    // Size ours to the band the body's own colliders leave uncovered, and borrow their character
    // while we are at it — a swing into a frozen tree's crown should sound like a tree, not like a
    // generic prop, and it should bounce the sword the same way the trunk does.
    {
        f32 loY;
        f32 hiY;
        f32 maxR;
        f32 maxRY;
        f32 ownTop;
        s32 i;

        Stasis_VisualBounds(target, &loY, &hiY, &maxR, &maxRY);
        ownTop = loY;
        for (i = 0; i < sStasis.ownColliderCount; i++) {
            f32 top = (f32)(sStasis.ownCollider[i]->dim.yShift + sStasis.ownCollider[i]->dim.height);

            if (top > ownTop) {
                ownTop = top;
            }
        }
        // Below ownTop the real collider is already doing the job, and stacking a second, much
        // wider one over it would land sword hits on thin air beside the trunk.
        sStasis.extraCollider = (u8)((hiY - ownTop) > 8.0f);
        if (sStasis.extraCollider) {
            sStasis.collider.dim.radius = (s16)maxR;
            sStasis.collider.dim.yShift = (s16)ownTop;
            sStasis.collider.dim.height = (s16)(hiY - ownTop);
        }
        if (sStasis.ownColliderCount > 0) {
            sStasis.collider.base.colType = sStasis.ownCollider[0]->base.colType;
            sStasis.collider.base.acFlags |= (sStasis.ownCollider[0]->base.acFlags & AC_HARD);
        }
    }

    target->update = Stasis_FrozenUpdate;
    target->draw = Stasis_TintDraw;
    target->velocity.x = target->velocity.y = target->velocity.z = 0.0f;
    target->speedXZ = 0.0f;
    target->gravity = 0.0f;
    // Without this the engine culls the actor when the player looks away and stops running OUR
    // update with it — the object would thaw itself off-screen.
    target->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    // Armed here rather than on landing, because the press is registered by the bg check of the
    // frame it touches down — which happens while it is still ours.
    SwitchMagnet_MakePresser(target);
    target->room = -1; // survive a room change while frozen
    // Now that its own OC collider is submitted again, Link walking into a frozen body would shove
    // it around under the normal mass rules. A thing held out of time does not budge.
    target->colChkInfo.mass = MASS_IMMOVABLE;

    // From here the body's motion is OURS — the hold and then the launch — and the recorder's
    // automatic admission only ever sees an actor that moved during its own update. Admitting it
    // by hand is what lets the Phantom Hourglass recall a stasis throw afterwards.
    Rewind_Track(target);

    // The rune's own cue. Enemies get it at double rate — the same sound, straining, which is what
    // sells a living thing fighting the field instead of a rock simply stopping.
    StasisSfx_Play((kind == STASIS_KIND_ENEMY) ? STASIS_SFX_RATE_ENEMY : 1.0f, 0.85f);
}

// ============================================================================
// ENTRY POINTS
// ============================================================================

// Take our wrapper off whatever was being offered last frame. Guarded on the pointer still being
// ours: if the actor died, or something else replaced its draw, we must not write over that.
static void Stasis_ClearOffer(void) {
    if ((sStasisOffer != NULL) && (sStasisOffer->update != NULL) && (sStasisOffer->draw == Stasis_TintDraw) &&
        (sStasisOfferDraw != NULL)) {
        sStasisOffer->draw = sStasisOfferDraw;
    }
    sStasisOffer = NULL;
    sStasisOfferDraw = NULL;
}

// Paint whatever the rune is currently aimed at. Called EVERY frame — `allowed` says whether the
// slate is actually out on this rune. Called unconditionally on purpose: the clear at the top is
// what takes the shimmer off the last target when the tablet goes away.
void Stasis_UpdateOffer(PlayState* play, u8 allowed) {
    Actor* target;

    Stasis_ClearOffer();

    if (!allowed) {
        return;
    }
    if (sStasis.actor != NULL) {
        return; // already holding something — the offer would only confuse the read
    }
    target = TargetSelect_ScanCats(play, sStasisCats, ARRAY_COUNT(sStasisCats), Stasis_TargetFilter, STASIS_RANGE,
                                   TARGETSEL_DEFAULT_CONE);
    if ((target == NULL) || (target->draw == NULL) || (target->draw == Stasis_TintDraw)) {
        return;
    }
    sStasisOffer = target;
    sStasisOfferDraw = target->draw;
    target->draw = Stasis_TintDraw;
}

s32 Stasis_Cast(PlayState* play, Player* player) {
    Actor* target;
    s32 bgId = -1;
    u8 kind;

    // A second cast releases what is already held, whatever it is aimed at — the hold is skipped
    // and the launch happens right now. The cue skips with it: rather than being cut off wherever
    // the recording happened to be, it jumps to its own release so the sound of the thing coming
    // out of stasis lands on the frame it actually does.
    if (sStasis.actor != NULL) {
        if (sStasis.phase != STASIS_PHASE_FLYING) {
            StasisSfx_SeekToTail(STASIS_SFX_RELEASE_SECONDS);
        }
        Stasis_End(play);
        return 1;
    }

    target = TargetSelect_ScanCats(play, sStasisCats, ARRAY_COUNT(sStasisCats), Stasis_TargetFilter, STASIS_RANGE,
                                   TARGETSEL_DEFAULT_CONE);
    if (target == NULL) {
        return 0; // nothing in range — the caller plays the error
    }

    kind = Stasis_Classify(play, target, &bgId);
    if (kind == STASIS_KIND_NONE) {
        return 0;
    }

    Stasis_Begin(play, target, kind, bgId);
    return 1;
}

// Read and bank an incoming blow.
//
// This MUST run before Stasis_Update re-registers the collider. CollisionCheck_SetAC calls the
// shape's AC reset first (Collider_ResetACBase: `ac = NULL; acFlags &= ~AC_HIT`), so registering
// wipes both the hit flag and the attacker. That is the bug that made every arrow read as "no
// attacker" and fall back to "away from Link" — the direction was never captured at all. It also
// cannot live in the frozen actor's own update: that runs in the PROP pass, long after the player
// pass where the tick sits.
static ColliderCylinder* Stasis_TakeHitCollider(void) {
    s32 i;

    for (i = 0; i < sStasis.ownColliderCount; i++) {
        if (sStasis.ownCollider[i]->base.acFlags & AC_HIT) {
            return sStasis.ownCollider[i];
        }
    }
    if (sStasis.colliderReady && (sStasis.collider.base.acFlags & AC_HIT)) {
        return &sStasis.collider;
    }
    return NULL;
}

static void Stasis_CaptureHit(PlayState* play, Actor* actor) {
    ColliderCylinder* hitCollider = Stasis_TakeHitCollider();

    if (hitCollider != NULL) {
        Player* player = GET_PLAYER(play);
        // Who actually landed the blow. For an arrow this is the arrow actor, not Link — which is
        // the whole point: an arrow arriving from below-left must send the object up and to the
        // right, no matter where Link was standing when he loosed it.
        //
        // It is `.ac`, NOT `.at`. On a hit the engine writes `at->at = ac->actor` on the ATTACKER's
        // collider and `ac->ac = at->actor` on the VICTIM's (z_collision_check.c:1755/1764). Ours is
        // the victim, so `.at` here is whatever WE last hit while flying — reading it meant the
        // attacker came back NULL and the direction fell through to "away from Link" every time,
        // which is exactly the bug where the arrow always pointed along Link's line.
        Actor* attacker = hitCollider->base.ac;
        Vec3f dir;
        f32 len;

        if (attacker == NULL) {
            attacker = &player->actor;
        }

        // Melee and projectiles are answered by two different questions, and conflating them is what
        // made the arrow swing back and stare at Link.
        //
        // The old code read `attacker->velocity` first for everybody. For an arrow that is its
        // flight vector — correct. For a SWORD the attacker actor is Link himself, so it was his
        // WALKING velocity: swing while backing up or strafing round a Z-target and the launch
        // direction was wherever his feet happened to be going, which is exactly the "it points
        // back at me" case.
        //
        // So: a hand weapon is aimed by where Link stands relative to the body — the direction the
        // swing pushes. Only a projectile is aimed by its own travel.
        s32 isMelee = (attacker == &player->actor);

        len = isMelee ? 0.0f
                      : sqrtf((attacker->velocity.x * attacker->velocity.x) +
                              (attacker->velocity.y * attacker->velocity.y) +
                              (attacker->velocity.z * attacker->velocity.z));
        if (len > 1.0f) {
            dir.x = attacker->velocity.x / len;
            dir.y = attacker->velocity.y / len;
            dir.z = attacker->velocity.z / len;
        } else {
            // The swing's own travel: from the attacker to the point the blow actually connected
            // at. `bumper.hitPos` is written by CollisionCheck_SetATvsAC (z_collision_check.c:1771)
            // with the real intersection, so this is the contact, not a guess about it.
            //
            // The previous version derived the vertical from the attacker's chest clamped into the
            // body's extent, which is flat whenever Link stands level with the thing — so swinging
            // UP into a tree's crown came out horizontal. Reading the contact point makes that case
            // answer itself, and it costs nothing on the level swings that already worked.
            Vec3f contact;
            f32 chest = attacker->world.pos.y + 30.0f;

            contact.x = (f32)hitCollider->info.bumper.hitPos.x;
            contact.y = (f32)hitCollider->info.bumper.hitPos.y;
            contact.z = (f32)hitCollider->info.bumper.hitPos.z;

            dir.x = contact.x - attacker->world.pos.x;
            dir.y = contact.y - chest;
            dir.z = contact.z - attacker->world.pos.z;

            len = sqrtf((dir.x * dir.x) + (dir.y * dir.y) + (dir.z * dir.z));
            if (len <= 0.001f) {
                // No contact point recorded (some AC paths leave it zeroed): fall back to the line
                // from the attacker to the body, level.
                dir.x = actor->world.pos.x - attacker->world.pos.x;
                dir.y = 0.0f;
                dir.z = actor->world.pos.z - attacker->world.pos.z;
                len = sqrtf((dir.x * dir.x) + (dir.z * dir.z));
            }
            if (len > 0.001f) {
                dir.x /= len;
                dir.y /= len;
                dir.z /= len;
            } else {
                dir.x = 0.0f;
                dir.y = 0.0f;
                dir.z = 1.0f;
            }
        }
        // A rock or a tree takes NO damage from a sword — colChkInfo.damage stays 0 — so keying
        // any of this on damage meant hitting exactly the things Stasis exists for did nothing.
        // Every connected blow charges the launch; damage only decides how MUCH.
        f32 dmg = (f32)actor->colChkInfo.damage;
        f32 charge = (dmg > 0.0f) ? (dmg * STASIS_LAUNCH_PER_DAMAGE) : STASIS_LAUNCH_PER_HIT;

        // Cleared on ALL of them, not just the one we read: the body's own collider and ours
        // overlap in places, so a single swing can register twice and would charge twice.
        {
            s32 i;

            for (i = 0; i < sStasis.ownColliderCount; i++) {
                sStasis.ownCollider[i]->base.acFlags &= ~AC_HIT;
            }
            sStasis.collider.base.acFlags &= ~AC_HIT;
        }

        // The LAST blow's damage type, copied BY VALUE. Never the pointer: an arrow is freed
        // between one and fifty frames after it lands (seed shots the same frame), so holding its
        // ColliderInfo for a ten-second stasis is a dangling read. See Stasis_DeliverStoredHit.
        if (hitCollider->info.acHitInfo != NULL) {
            u32 flags = hitCollider->info.acHitInfo->toucher.dmgFlags;

            // Zero would send the engine's bit search off the end of the 32-byte damage table, so a
            // blow that carries no type is banked for its damage and leaves the type alone.
            if (flags != 0) {
                sStasis.lastDmgFlags = flags;
                sStasis.lastDmgEffect = hitCollider->info.acHitInfo->toucher.effect;
                sStasis.lastDmgAmount = hitCollider->info.acHitInfo->toucher.damage;
            }
        }

        sStasis.accumDamage += actor->colChkInfo.damage;
        sStasis.force += charge;
        // The LAST blow owns the direction outright. Damage only ever adds to the magnitude,
        // which is why you can keep re-aiming a fully charged rock with one light tap.
        sStasis.hitDir = dir;
        sStasis.hasHitDir = 1;
        // Zeroed so the actor never sees it. CollisionCheck_ResetDamage would wipe it a frame
        // later anyway, which is exactly why this has to happen here, inside our update.
        actor->colChkInfo.damage = 0;
        actor->colChkInfo.damageEffect = 0;

        // Recoil, but ONLY when the blow landed on our substitute collider. A body that carries
        // AC_HARD of its own — every rock and tree here does — already got the engine's own bounce
        // and its own strike sound out of CollisionCheck_SetATvsAC, and doubling them up is both a
        // second thud and a second shove.
        if (!(hitCollider->base.acFlags & AC_HARD)) {
            Combat_ApplyKnockbackFromPoint(&player->actor, &actor->world.pos, STASIS_RECOIL_SPEED,
                                           STASIS_RECOIL_HEIGHT);
            Audio_PlaySoundGeneral(NA_SE_IT_SHIELD_REFLECT_MG, &actor->world.pos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
    }
}

void Stasis_Update(PlayState* play, Player* player) {
    static s16 sLastScene = -1;
    Actor* actor;

    // Scene change: the actors are already gone, so the pointer is dropped WITHOUT writing through
    // it. Detected here rather than through a hook, the same way the timestop helper does it.
    if (sLastScene != play->sceneNum) {
        sLastScene = play->sceneNum;
        Stasis_Forget();
        return;
    }

    actor = sStasis.actor;
    if (actor == NULL) {
        return;
    }
    // Died, despawned, or the scene took it: drop it without writing through the pointer.
    if (actor->update == NULL) {
        Stasis_Forget();
        return;
    }

    // Re-claimed every frame, not once on the grab: the hold lasts exactly as long as the
    // recorder's whole ring, so a one-shot claim would expire into "this has been still for its
    // entire history, drop it" on the very frame the launch starts.
    Rewind_Track(actor);

    sStasis.age++;
    if (sStasis.chainTimer > 0) {
        sStasis.chainTimer--;
    }

    if (sStasis.phase == STASIS_PHASE_FLYING) {
        // Measured once for the whole frame: the wall probe, the landing test, the switch press and
        // the flame all describe the same body and must not disagree about its size.
        f32 loY;
        f32 hiY;
        f32 maxR;
        f32 maxRY;

        // Aim assist on the way down: a body heavy enough to press a floor switch leans toward one
        // it could reach. Not for a shove along the floor — that one is not falling at all.
        if (!sStasis.slideLaunch) {
            SwitchMagnet_Steer(play, actor);
        }
        // Velocity only — the position is advanced by the sub-stepped loop below, which needs to
        // own the movement so it can bg-check between hops.
        Actor_UpdateVelocityXZGravity(actor);

        // Scene collision, sized to the body instead of to a stock number.
        //
        // The probe used to be a fixed sphere of radius 15 at 5 units off the ground, which is why
        // a launched body read as passing through the world: a boulder is 51 across, so its centre
        // had to get within 15 of a wall before anything registered and the model was already 36
        // units inside it. A tree is 180 across and simply flew through everything. Now the sphere
        // sits at the height where the body is widest and carries that width, the ceiling check is
        // switched on (flag 2) at the body's real top, and the wall check keeps its own flag 0x80.
        {
            u16 saved = 0;
            // Hide our own shell from the body's own check, for exactly the length of that one
            // call. Without this the thing lands on itself: the carrier sits where the body was
            // last frame, so as soon as gravity pulls it further in one frame than the shell's
            // floor is below it, the engine reports solid ground and the flight ends in mid-air.
            // Every dyna query gates on bit 0 of bgActorFlags (z_bgcheck.c), and DynaPoly_Setup has
            // already run for this frame, so clearing it and putting it straight back is invisible
            // to everything else.
            s32 hide = sStasis.bgIsOurs && (sStasis.bgId >= 0) && (sStasis.bgId < BG_ACTOR_MAX);
            f32 step;
            s32 substeps;
            s32 i;

            f32 probeY;

            Stasis_VisualBounds(actor, &loY, &hiY, &maxR, &maxRY);

            // The wall probe sits at the body's MIDDLE, and getting this wrong is what made a
            // sliding block die on its face at every ledge.
            //
            // `maxRY` is the height of the widest ring, which is the right place to measure a cone
            // but degenerates on anything of even width: a block's rings tie, so it came back as
            // the BOTTOM ring — height 0. That number is handed to the engine as `checkHeight`, and
            // BgCheck_EntitySphVsWall branches on `(checkHeight + dy) < 5.0f` (z_bgcheck.c:1936).
            // With checkHeight at 0, the instant the block tipped over an edge and dy went even
            // slightly negative that branch took over — and unlike the normal path it line-tests
            // against FLOORS as well as walls. The floor it was leaving came back as a wall hit,
            // which teleports the body and raises BGCHECKFLAG_WALL, which ends the shove. Hence
            // full momentum one frame and a dead stop the next, right at the lip.
            //
            // The middle is both a fair single-sphere stand-in for any body and always comfortably
            // positive, so that fallback only fires on a genuine fall.
            // The floor of 25 is the number that branch actually cares about: it needs
            // `checkHeight + dy` to stay above 5 through the small dy of ordinary travel. A body
            // whose origin sits at its own middle (a boulder) has a mid-height near zero and would
            // otherwise be just as exposed as the block was — so the mid is a starting point, not
            // the answer. Capped just under the body's own top, so a flat slab still probes inside
            // itself rather than through the air above it.
            probeY = (loY + hiY) * 0.5f;
            {
                f32 floorY = (hiY - 2.0f);

                if (floorY > 25.0f) {
                    floorY = 25.0f;
                }
                if (probeY < floorY) {
                    probeY = floorY;
                }
            }

            // SUB-STEPPED, because one frame of this is a long way. Every bg query is a swept test
            // from prevPos to pos, and a sweep that jumps 46 units in one go will step clean over a
            // thin wall — which is exactly what "it goes through the scene at speed" was. Splitting
            // the frame into hops no longer than half the body's own width closes that: nothing can
            // pass through geometry thicker than the hop.
            step = sqrtf((actor->velocity.x * actor->velocity.x) + (actor->velocity.y * actor->velocity.y) +
                         (actor->velocity.z * actor->velocity.z));
            {
                f32 maxHop = maxR * 0.5f;

                if (maxHop < 10.0f) {
                    maxHop = 10.0f;
                }
                substeps = (s32)(step / maxHop) + 1;
                if (substeps > 8) {
                    substeps = 8; // a ceiling on the cost; 8 hops covers any speed this rune produces
                }
            }

            if (hide) {
                saved = play->colCtx.dyna.bgActorFlags[sStasis.bgId];
                play->colCtx.dyna.bgActorFlags[sStasis.bgId] &= ~1;
            }
            for (i = 0; i < substeps; i++) {
                // prevPos BY HAND, and this is the fix the rest of it was waiting on. Nothing in the
                // engine maintains prevPos — Actor_Init writes it once (z_actor.c:1264) and after
                // that every actor updates it inside its own update function. Ours has that update
                // replaced by a no-op, so prevPos stayed pinned at the SPAWN POINT for the whole
                // flight. The wall sweep was therefore testing a segment from wherever the boulder
                // was born to wherever it is now, and func_8002E2AC was raycasting for the floor
                // from the spawn's height — which is why it fell through walls, why the landing
                // fired at nonsense moments, and why gravity looked like it had switched off (a
                // bogus ground hit zeroes velocity.y every frame).
                actor->prevPos = actor->world.pos;
                actor->world.pos.x += actor->velocity.x / (f32)substeps;
                actor->world.pos.y += actor->velocity.y / (f32)substeps;
                actor->world.pos.z += actor->velocity.z / (f32)substeps;

                Actor_UpdateBgCheckInfo(play, actor, probeY, maxR, hiY, 0x87);
                // A shove is ALWAYS on the ground, so stopping the loop on BGCHECKFLAG_GROUND cut
                // every frame short after one hop and the block crawled to a halt — worst of all
                // at a step down, where the ground flag re-latches the instant it lands. Only a
                // wall ends a shove early.
                if (actor->bgCheckFlags &
                    (sStasis.slideLaunch ? BGCHECKFLAG_WALL : (BGCHECKFLAG_GROUND | BGCHECKFLAG_WALL))) {
                    break;
                }
            }
            if (hide) {
                play->colCtx.dyna.bgActorFlags[sStasis.bgId] = saved;
            }

            // A ceiling stops the climb, it does not end the throw — the body just loses its upward
            // speed and starts coming back down.
            if ((actor->bgCheckFlags & BGCHECKFLAG_CEILING) && (actor->velocity.y > 0.0f)) {
                actor->velocity.y = 0.0f;
            }

            // And it comes to rest on its own UNDERSIDE. func_8002E2AC snaps `world.pos.y` to the
            // floor, which is right for a tree (its origin is at its base) and wrong for a boulder
            // whose origin is at its middle — that one sank half of itself into the ground before
            // anything called it a landing.
            if ((loY < 0.0f) && (actor->floorHeight > BGCHECK_Y_MIN) &&
                ((actor->world.pos.y + loY) <= actor->floorHeight)) {
                actor->world.pos.y = actor->floorHeight - loY;
                actor->velocity.y = 0.0f;
                actor->bgCheckFlags |= BGCHECKFLAG_GROUND;
            }
        }

        // Final approach. Once the body is inside the column above a floor switch it stops
        // travelling, squares up with the plate and comes straight down onto it. Runs for the shove
        // as well as the throw: a block slid across a switch should settle on it, not over it.
        SwitchMagnet_SnapOnto(play, actor, !Stasis_KeepsItsPose(actor));
        // And press what it is standing on, because the body's own update — the thing that does
        // this in vanilla — is switched off for as long as the rune has it.
        SwitchMagnet_PressUnder(play, actor, maxR, loY);
        Stasis_FireTick(play, actor);

        // Drag the collision along, so a launched boulder is still something Link can be crushed by
        // or ride, not a ghost that happens to be drawn.
        if (sStasis.bgIsOurs) {
            Stasis_CarrierSync(actor);
            // Force the re-expansion rather than trust the prev/cur comparison. The move lands in
            // the PLAYER pass, which is after DynaPoly_Setup has already run for the frame, and
            // DynaPoly_UpdateBgActorTransforms then copies cur into prev at the end of it — so by
            // the next Setup the two can read as unchanged and the box would never be rebuilt.
            play->colCtx.dyna.bitFlag |= DYNAPOLY_INVALIDATE_LOOKUP;
        }
        Stasis_CarryRider(play, actor);

        if (sStasis.colliderReady) {
            Collider_UpdateCylinder(actor, &sStasis.collider);
            CollisionCheck_SetAT(play, &play->colChkCtx, &sStasis.collider.base);
        }
        if (sStasis.timer > 0) {
            sStasis.timer--;
        }
        if (sStasis.slideLaunch) {
            // A shove runs out of steam instead of landing — it is already on the floor, so
            // BGCHECKFLAG_GROUND would end it on the very first frame.
            actor->speedXZ *= STASIS_SLIDE_FRICTION;
            if ((actor->bgCheckFlags & BGCHECKFLAG_WALL) || (actor->speedXZ < 1.0f) || (sStasis.timer <= 0) ||
                (sStasis.collider.base.atFlags & AT_HIT)) {
                Audio_PlaySoundGeneral(NA_SE_EV_BLOCK_BOUND, &actor->world.pos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                Stasis_RestoreActor();
                Stasis_Forget();
            }
            return;
        }
        // Landed, hit a wall, connected with something, or ran out of flight time.
        if ((actor->bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_WALL)) || (sStasis.timer <= 0) ||
            (sStasis.collider.base.atFlags & AT_HIT)) {
            Audio_PlaySoundGeneral(NA_SE_EV_BOMB_DROP_WATER, &actor->world.pos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            Stasis_RestoreActor();
            Stasis_Forget();
        }
        return;
    }

    // Bank any blow from last frame's collision pass FIRST — re-registering below would erase it.
    Stasis_CaptureHit(play, actor);

    // Keep the collision carrier sitting on the body. It never moves while frozen, but this also
    // covers a body that was nudged by something else before it was caught.
    if (sStasis.bgIsOurs) {
        Stasis_CarrierSync(actor);
    }

    // Frozen: keep its collision live so the body stays hittable and solid while its own update
    // does nothing. Nothing is disabled here — the actor's own colliders are simply submitted on
    // its behalf, so what it loses is the break, not the hit.
    Stasis_SubmitColliders(play, actor);
    {
        f32 loY;
        f32 hiY;
        f32 maxR;
        f32 maxRY;

        // A body held out of time still has weight. Freezing one that was already sitting on a
        // plate must not let the plate pop back up.
        Stasis_VisualBounds(actor, &loY, &hiY, &maxR, &maxRY);
        SwitchMagnet_PressUnder(play, actor, maxR, loY);
    }
    Stasis_FireTick(play, actor);

    if (sStasis.timer > 0) {
        sStasis.timer--;
    }
    if (sStasis.timer <= 0) {
        Stasis_End(play);
    }
}

// Only the frozen body's own bgId, and only while it is a climbable kind. Consulted by the wall
// flags getter in z_bgcheck.c, so it reverts by itself the moment the stasis ends.
u8 Stasis_IsClimbableBgId(s32 bgId) {
    return (sStasis.actor != NULL) && (sStasis.kind == STASIS_KIND_CLIMBABLE) && (sStasis.bgId >= 0) &&
           (sStasis.bgId == bgId);
}

// The rune's own sound, plus its PCM. Both are .inc.c rather than headers: mods/*.h is globbed
// with CONFIGURE_DEPENDS in 2ship and a new one there forces a CMake regeneration.
#include "stasis_sfx.inc.c"

#include "stasis_rune_vfx.inc.c"
