/**
 * remains_ally_fish.c - Gyorg's friendly fish ally (SoH port, runtime ActorDB id).
 *
 * The third of the four boss-remains summon allies (see remains_ally_common.h).
 * Wearing Gyorg's Remains, ON DRY LAND (out of water), and pressing SHIELD(R)+B spawns a
 * few of these DESBREKO-look fish next to Link (boss_remains.cpp). Design (user spec):
 *
 *   - ON LAND (the summon case): each fish flops ERRATICALLY — random hops + spins,
 *                 lunging vaguely toward the nearest enemy but mostly overshooting — and
 *                 bites only what it flops right onto (a FRIENDLY AT_TYPE_PLAYER contact
 *                 hit at a tight radius: damages every enemy, can never touch Link). It
 *                 suffocates and self-culls after REMAINS_FISH_MAX_OUT_OF_WATER frames.
 *   - IN WATER  : if a flop carries it into water it comes alive — swims with the player,
 *                 darts at the nearest enemy with the same friendly bite, and clamps below
 *                 the surface. In water the out-of-water timer resets, so it survives.
 *
 * Not persistent/auto-maintained anymore: it's a one-shot manual summon per Shield+B.
 *
 * ---- Unity include (NOT in CMake/vcxproj) ----------------------------------------------
 * This .c is #included into boss_remains.cpp inside its `extern "C"` block, so the
 * lifecycle functions / RemainsAllyFish_Spawn get C linkage. Compiled as C++:
 *   - NEVER name a local `this` (reserved word in C++) -> the typed pointer is `self`.
 *   - explicit casts everywhere a C-only implicit conversion would be needed.
 * remains_ally_common.c must be #included BEFORE this file in boss_remains.cpp so the
 * RemainsAlly_* helpers are defined. Registered at runtime by boss_remains_actor_reg.cpp.
 *
 * ---- Art (the actual Gyorg-battle fish, Boss03 coupling severed) -----------------------
 * BASE MODEL: En_Tanron3, "Small fish (Gyorg)" (object_boss03 skeleton gGyorgSmallFishSkel +
 * swim anim gGyorgSmallFishSwimAnim) — the piranha Gyorg spawns in its fight. We reuse ONLY
 * the model: no Boss03 pointer, no arena constants, no die-when-Gyorg-dies. All motion is
 * this file's own friendly flop/swim + the shared remains_ally_common.c helpers.
 *
 * ASSET LOADING (SoH): the skeleton/anim come from mm.o2r through MmAssets_LoadSkeleton /
 * MmAssets_LoadAnimation (mm_asset_loader.h). NULL means mm.o2r isn't mounted yet — the
 * proven MmSoul pattern applies: retry every Update, never latch the failure, and skip
 * skelanime/draw until ready (the flop/swim AI runs regardless).
 */

#include "remains_ally_common.h"

#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "mods/transformation_masks/assets/mm_asset_loader.h" // MmAssets_* + MmSfx_PlayAtPos
#include "mods/sound_translator/mm_sfx_ids.h"

// ---- MM sfx ids not present in mm_sfx_ids.h (values verified in 2ship mm/include/sfx.h) ----
#ifndef MM_NA_SE_EN_PIRANHA_ATTACK
#define MM_NA_SE_EN_PIRANHA_ATTACK 0x39F4 // mm/include/sfx.h:1575
#endif

// ---- object_boss03 limb layout (2ship mm/assets/objects/object_boss03/object_boss03.h:225-235) ----
#define GYORG_SMALL_FISH_LIMB_ROOT 0x01       // whole-body sway
#define GYORG_SMALL_FISH_LIMB_TRUNK_ROOT 0x03 // trunk sway
#define GYORG_SMALL_FISH_LIMB_TAIL_FIN 0x04   // tail sway
#define GYORG_SMALL_FISH_LIMB_MAX 0x0A        // 9 limbs + LIMB_NONE

// mm.o2r asset paths (NO "__OTR__" prefix — the MmAssets bridge handles it).
static const char* const sFishSkelPath = "objects/object_boss03/gGyorgSmallFishSkel";
static const char* const sFishAnimPath = "objects/object_boss03/gGyorgSmallFishSwimAnim";

// ---- Tuning ----------------------------------------------------------------
#define REMAINS_FISH_SCALE 0.018f        // small school member (EnTanron3 uses 0.02f)
#define REMAINS_FISH_ATTACK_RANGE 220.0f // dart at the nearest enemy within this XZ range
#define REMAINS_FISH_TETHER \
    180.0f                               // ~3 Link-heights: the fish is leashed to Link (its home) and
                                         // never strays farther; past this it sprints straight back
#define REMAINS_FISH_ATTACK_CLOSE 110.0f // only break off the orbit to bite an enemy THIS close
#define REMAINS_FISH_FOLLOW_DIST 42.0f   // orbit/surround: hug this close to Link while idle
#define REMAINS_FISH_SWIM_SPEED 4.0f     // idle cruise speed
#define REMAINS_FISH_DART_SPEED 7.5f     // faster charge when attacking
// --- movement-aware school AI ---
#define REMAINS_FISH_LINK_MOVE_SPEED 2.5f // Link's linearVelocity above this = "fast-swimming" (regroup)
#define REMAINS_FISH_CHASE_SPEED \
    14.0f                                     // catch-up to a cruising Link — MUST beat his ~9 swim speed or
                                              // the school falls behind forever (that was the "stay far" bug)
#define REMAINS_FISH_LUNGE_SPEED 13.0f        // idle-Link strike: a fast school lunge from range
#define REMAINS_FISH_IDLE_ATTACK_RANGE 240.0f // when Link is still, hunt this far (>= the whirlpool suck radius)
// --- Pikmin ball (trail behind Link, spaced apart) ---
#define REMAINS_FISH_BALL_DIST 55.0f       // base distance the ball trails BEHIND Link
#define REMAINS_FISH_BALL_SPACING 30.0f    // extra ring depth so they don't all sit at one radius
#define REMAINS_FISH_SEPARATION 30.0f      // boids: min gap between two fish before they push apart
#define REMAINS_FISH_SUBMERGE_MARGIN 25.0f // stay at least this far below the water surface
#define REMAINS_FISH_FLOOR_MARGIN 8.0f     // never sink below the floor by less than this
#define REMAINS_FISH_TARGET_Y_OFFSET 10.0f // aim slightly above the target's anchor
#define REMAINS_FISH_Y_STEP 6.0f           // vertical ease-in cap per frame (Math_ApproachF)
#define REMAINS_FISH_PITCH_REF 200.0f      // XZ reference for the nose-pitch atan2 (gentle tilt)
#define REMAINS_FISH_WIGGLE_SLOW 0x1F40    // procedural body-sway speed while cruising
#define REMAINS_FISH_WIGGLE_FAST 0x4E20    // faster sway while darting (EnTanron3's attack value)
#define REMAINS_FISH_WIGGLE_AMPL 5000.0f
#define REMAINS_FISH_MAX_OUT_OF_WATER \
    85                                     // frames flopping on land before it suffocates (~2.8s) — long
                                           // enough to hop around and land a few close bites, then dies
#define REMAINS_FISH_FLOP_BITE_RANGE 22.0f // AT cylinder radius while beached — only bites what it flops onto
#define REMAINS_FISH_WALL_H 20.0f
#define REMAINS_FISH_WALL_R 10.0f

extern void BossRemains_EnsureActorsRegistered(void); // boss_remains_actor_reg.cpp (lazy ActorDB reg)

// Runtime ActorDB id — filled by BossRemains_EnsureActorsRegistered(); -1 until then.
s16 gRemainsAllyFishId = -1;

typedef struct RemainsAllyFish {
    /* 0x000 */ Actor actor;
    /* 0x14C */ SkelAnime skelAnime;
    /* ..... */ Vec3s jointTable[GYORG_SMALL_FISH_LIMB_MAX];
    /* ..... */ Vec3s morphTable[GYORG_SMALL_FISH_LIMB_MAX];
    /* ..... */ ColliderCylinder atCollider; // FRIENDLY attack toucher only (no bumper: invulnerable)
    /* ..... */ f32 waterSurfaceYPos;        // set from WaterBox_GetSurface1 each frame (NOT hardcoded)
    /* ..... */ s16 timer;
    /* ..... */ s16 outOfWaterTimer;
    /* ..... */ s32 currentRotationAngle; // accumulates nextRotationAngle for the wiggle
    /* ..... */ s32 nextRotationAngle;    // wiggle speed (slow while cruising, fast while darting)
    /* ..... */ s16 trunkRotation;
    /* ..... */ s16 tailRotation;
    /* ..... */ s16 bodyRotation;
    /* -- mm.o2r deferred-load state (SoH-only) -- */
    /* ..... */ u8 mmAssetsReady;
    /* ..... */ FlexSkeletonHeader* fishSkel;
    /* ..... */ AnimationHeader* fishSwimAnim;
} RemainsAllyFish;

// The reg .cpp cannot see this struct (it lives in this unity-included file), so it takes
// the instance size through this global.
size_t gRemainsAllyFishStructSize = sizeof(RemainsAllyFish);

// FRIENDLY attack collider. The crux is the base-type bits, NOT the actor category:
//   AT_ON | AT_TYPE_PLAYER  -> "player-aligned damage". Enemy body bumpers are
//     AC_TYPE_PLAYER, so `acFlags & atFlags & AC_TYPE_ALL` shares bit 3 and the hit
//     lands (z_collision_check.c). Link's own body bumper is AC_TYPE_ENEMY
//     (bit 4), which shares NO bit with AT_TYPE_PLAYER, so the fish can never hit Link.
//   AC_NONE -> no bumper at all; we never call CollisionCheck_SetAC, so the fish is
//     invulnerable (a persistent pet should not be killed by the enemies it harasses).
//   OC1_NONE -> no push collisions, so the school never shoves Link or the enemies.
// Masks copied from Ivan (z_en_partner.c) so the hit actually lands: the gate needs a
// shared TYPE bit AND overlapping dmgFlags. Toucher DMG_DEKU_STICK overlaps every enemy's
// accept-all bumper; the bumper mask is inert here (AC_NONE) but kept at Ivan's value for
// parity with remains_ally_bug.c.
// MM->OoT field renames: COL_MATERIAL_HIT3->COLTYPE_HIT3, ELEM_MATERIAL_UNK3->ELEMTYPE_UNK3,
// ATELEM_*->TOUCH_*, ACELEM_NONE->BUMP_NONE.
static ColliderCylinderInit sFishColliderInit = {
    {
        COLTYPE_HIT3,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK3,
        { DMG_DEKU_STICK, 0x00, 0x02 }, // toucher: Ivan's deku-stick class + small contact damage
        { 0xF7CFFFFF, 0x00, 0x00 },     // bumper: inert (AC off), kept for parity
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { 12, 16, -8, { 0, 0, 0 } },
};

// MM/boss display lists can branch into segment 0x0C for the scene cull list, which is
// unset when we draw a boss model outside its arena. Bind it to no-op gsSPEndDisplayList
// so any such branch just returns. Defensive: the small-fish limb DLs do not reference
// 0x0C, but this mirrors the other allies and costs nothing.
static Gfx sFishSegment0xC_Noop[] = {
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
};

// ============================================================================
// FORWARD DECLARATIONS (non-static: extern "C"-visible for boss_remains_actor_reg.cpp)
// ============================================================================

void RemainsAllyFish_Init(Actor* thisx, PlayState* play);
void RemainsAllyFish_Destroy(Actor* thisx, PlayState* play);
void RemainsAllyFish_Update(Actor* thisx, PlayState* play);
void RemainsAllyFish_Draw(Actor* thisx, PlayState* play);

// ============================================================================
// DEFERRED MM ASSET LOAD (retry-until-ready, per the MmSoul pattern)
// ============================================================================

static s32 RemainsAllyFish_TryLoadAssets(RemainsAllyFish* self, PlayState* play) {
    if (self->mmAssetsReady) {
        return true;
    }

    self->fishSkel = (FlexSkeletonHeader*)MmAssets_LoadSkeleton(sFishSkelPath);
    self->fishSwimAnim = (AnimationHeader*)MmAssets_LoadAnimation(sFishAnimPath);
    if ((self->fishSkel == NULL) || (self->fishSwimAnim == NULL)) {
        return false; // mm.o2r not mounted yet — retry next frame, never latch
    }

    SkelAnime_InitFlex(play, &self->skelAnime, self->fishSkel, self->fishSwimAnim, self->jointTable, self->morphTable,
                       GYORG_SMALL_FISH_LIMB_MAX);
    Animation_MorphToLoop(&self->skelAnime, self->fishSwimAnim, -10.0f);
    self->mmAssetsReady = true;
    return true;
}

// ============================================================================
// INIT / DESTROY
// ============================================================================

void RemainsAllyFish_Init(Actor* thisx, PlayState* play) {
    RemainsAllyFish* self = (RemainsAllyFish*)thisx;
    WaterBox* waterBox;
    f32 surfaceY = self->actor.world.pos.y;

    // No gravity while submerged: the fish holds its swim depth via the Y clamp below,
    // not by falling. minVelocityY 0 keeps velocity.y from accumulating when idle
    // (MM terminalVelocity -> OoT minVelocityY).
    self->actor.gravity = 0.0f;
    self->actor.minVelocityY = 0.0f;
    // Land creature now (summoned beached): cast a circle shadow so the flopping fish grounds visually.
    self->actor.shape.shadowDraw = ActorShadow_DrawCircle;
    self->actor.shape.shadowScale = 20.0f;

    // MM Collider_InitAndSetCylinder -> the two SoH calls.
    Collider_InitCylinder(play, &self->atCollider);
    Collider_SetCylinder(play, &self->atCollider, &self->actor, &sFishColliderInit);

    // En_Tanron3 (Gyorg small fish) skeleton + swim anim from mm.o2r. Deferred/retried until
    // the archive mounts (TryLoadAssets); the flop/swim AI does not need the model.
    self->mmAssetsReady = false;
    self->fishSkel = NULL;
    self->fishSwimAnim = NULL;
    RemainsAllyFish_TryLoadAssets(self, play);

    Actor_SetScale(&self->actor, REMAINS_FISH_SCALE);

    // Seed the water surface from the actual waterbox at spawn (NOT a hardcoded 430.0f).
    // If the fish was spawned out of water this stays at its own Y and the first Update
    // drops straight into the beach/flop branch.
    if (WaterBox_GetSurface1(play, &play->colCtx, self->actor.world.pos.x, self->actor.world.pos.z, &surfaceY,
                             &waterBox)) {
        self->waterSurfaceYPos = surfaceY;
    } else {
        self->waterSurfaceYPos = self->actor.world.pos.y;
    }

    // Per-fish phase so a spawned school does not sway/dart in lockstep. Read params
    // (0..N-1 if the spawner staggers them) and fold it into the random wiggle seed.
    self->currentRotationAngle = (s32)Rand_ZeroFloat(50000.0f) + (self->actor.params * 0x1000);
    self->nextRotationAngle = REMAINS_FISH_WIGGLE_SLOW;
    self->timer = 0;
    self->outOfWaterTimer = 0;
}

void RemainsAllyFish_Destroy(Actor* thisx, PlayState* play) {
    RemainsAllyFish* self = (RemainsAllyFish*)thisx;
    // No Boss03 coupling to unwind; MmAssets skeleton loads are cache-owned. Just the collider.
    Collider_DestroyCylinder(play, &self->atCollider);
}

// ============================================================================
// UPDATE
// ============================================================================

// Keep the fish comfortably submerged and above the floor, then ease its Y toward that
// height. Returns the pre-ease vertical delta so the caller can pitch the nose to match.
static f32 RemainsAllyFish_ClampSwimHeight(RemainsAllyFish* self, f32 desiredY) {
    f32 ceilY = self->waterSurfaceYPos - REMAINS_FISH_SUBMERGE_MARGIN;
    f32 floorY = self->actor.floorHeight;
    f32 yDelta;

    if (desiredY > ceilY) {
        desiredY = ceilY;
    }
    if ((floorY > BGCHECK_Y_MIN) && (desiredY < (floorY + REMAINS_FISH_FLOOR_MARGIN))) {
        desiredY = floorY + REMAINS_FISH_FLOOR_MARGIN;
    }

    yDelta = desiredY - self->actor.world.pos.y;
    Math_ApproachF(&self->actor.world.pos.y, desiredY, 0.5f, REMAINS_FISH_Y_STEP);
    return yDelta;
}

// Boids separation: nudge this fish away from any ally fish that got too close, so the school stays a
// spaced BALL instead of merging into one dot. A gentle direct position push (the fish live in
// ACTORCAT_MISC, so we only ever see our own kind here).
static void RemainsAllyFish_Separate(RemainsAllyFish* self, PlayState* play) {
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_MISC].head; a != NULL; a = a->next) {
        if ((a == &self->actor) || (a->id != gRemainsAllyFishId)) {
            continue;
        }
        f32 d = Math_Vec3f_DistXZ(&self->actor.world.pos, &a->world.pos);
        if ((d < REMAINS_FISH_SEPARATION) && (d > 0.1f)) {
            s16 away = Math_Vec3f_Yaw(&a->world.pos, &self->actor.world.pos); // neighbor -> self
            f32 push = (REMAINS_FISH_SEPARATION - d) * 0.25f;
            self->actor.world.pos.x += Math_SinS(away) * push;
            self->actor.world.pos.z += Math_CosS(away) * push;
        }
    }
}

static void RemainsAllyFish_SwimInWater(RemainsAllyFish* self, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Actor* target;
    f32 desiredY;
    f32 yDelta;
    s16 pitchTarget;

    // The XZ swim is delegated to the shared motion helpers. Zero gravity/velocity.y up
    // front so their Actor_MoveXZGravity leaves Y untouched — we own Y via the clamp.
    self->actor.gravity = 0.0f;
    self->actor.minVelocityY = 0.0f;
    self->actor.velocity.y = 0.0f;
    self->outOfWaterTimer = 0;

    // Pikmin-style: the school trails Link as a loose BALL BEHIND him (his facing reversed), each fish
    // fanned to its own spot by its params and actively separated from neighbors so they never stack.
    // They break formation to attack ONLY while Link holds still — a fast school lunge at the nearest
    // enemy to LINK (so they all converge on the same target, "en banco"). On landing a hit each fish
    // DIES (kamikaze — handled in Update). While Link cruises they just re-form + keep up.
    // MM player->speedXZ -> OoT player->linearVelocity (z64player.h).
    s32 linkMoving = player->linearVelocity > REMAINS_FISH_LINK_MOVE_SPEED;

    // Anchor: behind Link, per-fish fan (angle from params) + varied ring depth so the ball has volume.
    s16 behindYaw = player->actor.shape.rot.y + 0x8000;
    s16 fan = (s16)(((self->actor.params & 3) - 1) * 0x1800);
    f32 ringDist = REMAINS_FISH_BALL_DIST + (f32)((self->actor.params >> 2) & 1) * REMAINS_FISH_BALL_SPACING;
    Vec3f anchor = player->actor.world.pos;
    anchor.x += Math_SinS(behindYaw + fan) * ringDist;
    anchor.z += Math_CosS(behindYaw + fan) * ringDist;

    // Keep the ball spaced.
    RemainsAllyFish_Separate(self, play);

    target = linkMoving ? NULL
                        : RemainsAlly_FindNearestEnemy(play, &player->actor.world.pos, REMAINS_FISH_IDLE_ATTACK_RANGE);

    if (target != NULL) {
        RemainsAlly_HomeTowardPos(play, &self->actor, &target->world.pos, REMAINS_FISH_LUNGE_SPEED);
        desiredY = target->world.pos.y + REMAINS_FISH_TARGET_Y_OFFSET;
        self->nextRotationAngle = REMAINS_FISH_WIGGLE_FAST;
        if (!(self->timer & 0xF) && (Rand_ZeroOne() < 0.5f)) {
            MmSfx_PlayAtPos(MM_NA_SE_EN_PIRANHA_ATTACK, &self->actor.projectedPos);
        }
    } else {
        // Form up: swim to this fish's spot in the ball behind Link. Sprint if far (beat his swim speed
        // so they keep up), ease when near, hold when there.
        f32 distToAnchor = Math_Vec3f_DistXZ(&self->actor.world.pos, &anchor);
        f32 sp = (distToAnchor > REMAINS_FISH_FOLLOW_DIST) ? REMAINS_FISH_CHASE_SPEED
                 : (distToAnchor > 10.0f)                  ? REMAINS_FISH_SWIM_SPEED
                                                           : 0.0f;
        RemainsAlly_HomeTowardPos(play, &self->actor, &anchor, sp);
        desiredY = player->actor.world.pos.y + REMAINS_FISH_TARGET_Y_OFFSET;
        self->nextRotationAngle = (sp > REMAINS_FISH_SWIM_SPEED) ? REMAINS_FISH_WIGGLE_FAST : REMAINS_FISH_WIGGLE_SLOW;
    }

    yDelta = RemainsAllyFish_ClampSwimHeight(self, desiredY);

    // Nose pitch to match the vertical travel (same atan2 form EnTanron3 uses for its
    // world.rot.x). shape.rot.y was already set by the helper.
    // MM Math_Atan2S_XY(x, y) -> OoT Math_Atan2S(x, y) (same argument convention).
    pitchTarget = Math_Atan2S(REMAINS_FISH_PITCH_REF, -yDelta);
    Math_ApproachS(&self->actor.shape.rot.x, pitchTarget, 4, 0x800);

    // The bite is live every in-water frame, so simply swimming into an enemy damages it. Restore the
    // tighter dart radius (the land flop widens it to help it connect on a hop).
    self->atCollider.dim.radius = 12;
    Collider_UpdateCylinder(&self->actor, &self->atCollider);
    CollisionCheck_SetAT(play, &play->colChkCtx, &self->atCollider.base);
}

static void RemainsAllyFish_BeachAndFlop(RemainsAllyFish* self, PlayState* play) {
    // Beached (this is the SUMMONED-ON-LAND behavior): the fish flops ERRATICALLY — random hops + spins,
    // lunging vaguely toward the nearest enemy but mostly overshooting — and bites only what it happens
    // to flop right onto, then suffocates (outOfWaterTimer -> Actor_Kill in the caller).
    Actor* target;

    self->outOfWaterTimer++;
    self->actor.gravity = -1.8f;
    self->actor.minVelocityY = -22.0f;
    self->nextRotationAngle = REMAINS_FISH_WIGGLE_FAST; // agitated flailing, not a calm cruise

    if (self->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        // On land: hop with a lunge. Bias the facing weakly toward the nearest enemy so it TRIES to
        // reach one, but a big random spread dominates — the erratic, mostly-failing flop the user wants.
        target = RemainsAlly_FindNearestEnemy(play, &self->actor.world.pos, REMAINS_FISH_ATTACK_RANGE);
        if (target != NULL) {
            s16 toEnemy = Math_Vec3f_Yaw(&self->actor.world.pos, &target->world.pos);
            self->actor.world.rot.y = toEnemy + (s16)Rand_CenteredFloat(0x5000); // ±~110° scatter
        } else {
            self->actor.world.rot.y += (s16)Rand_CenteredFloat(0x8000); // no enemy: pure random spin
        }
        self->actor.velocity.y = Rand_ZeroFloat(4.0f) + 4.0f;
        self->actor.speedXZ = Rand_ZeroFloat(3.0f) + 2.0f; // a short forward lunge on each hop
        if (!(self->timer & 0x7)) {
            MmSfx_PlayAtPos(MM_NA_SE_EN_PIRANHA_ATTACK, &self->actor.projectedPos);
        }
    }

    Actor_MoveXZGravity(&self->actor);
    // 0x1 = wall, 0x4 = floor/water (same numeric bits as MM's FLAG_1|FLAG_4).
    Actor_UpdateBgCheckInfo(play, &self->actor, REMAINS_FISH_WALL_H, REMAINS_FISH_WALL_R, 0.0f, 0x5);
    self->actor.shape.rot.y = self->actor.world.rot.y;
    Math_ApproachS(&self->actor.shape.rot.x, 0x2000, 4, 0x800); // list while flopping

    // The bite IS live on land, but at a tight radius — it only connects when the flop lands it right
    // next to an enemy ("attacks, but only very close"). AT_TYPE_PLAYER, so it never touches Link.
    self->atCollider.dim.radius = (s16)REMAINS_FISH_FLOP_BITE_RANGE;
    Collider_UpdateCylinder(&self->actor, &self->atCollider);
    CollisionCheck_SetAT(play, &play->colChkCtx, &self->atCollider.base);
}

void RemainsAllyFish_Update(Actor* thisx, PlayState* play) {
    RemainsAllyFish* self = (RemainsAllyFish*)thisx;
    WaterBox* waterBox;
    f32 surfaceY = self->actor.world.pos.y;
    s32 hasWater;
    s32 inWater;

    self->timer++;

    // mm.o2r may mount a few frames after us — keep retrying; AI runs regardless.
    RemainsAllyFish_TryLoadAssets(self, play);

    // KAMIKAZE (Pikmin): the moment our bite connected with an enemy, we spent ourselves — die. AT_HIT
    // is latched by last frame's collision resolution; a fresh spawn hasn't attacked yet so it's clear.
    if (self->atCollider.base.atFlags & AT_HIT) {
        self->atCollider.base.atFlags &= ~AT_HIT;
        MmSfx_PlayAtPos(MM_NA_SE_EN_PIRANHA_ATTACK, &self->actor.projectedPos);
        Actor_Kill(&self->actor);
        return;
    }

    // Re-query the waterbox at the fish's current position every frame. This tracks a
    // moving surface and works in any scene the player wanders into (no arena constant).
    hasWater = WaterBox_GetSurface1(play, &play->colCtx, self->actor.world.pos.x, self->actor.world.pos.z, &surfaceY,
                                    &waterBox);
    if (hasWater) {
        self->waterSurfaceYPos = surfaceY;
    }
    inWater = hasWater && (self->actor.world.pos.y < surfaceY);

    if (inWater) {
        RemainsAllyFish_SwimInWater(self, play);
    } else {
        RemainsAllyFish_BeachAndFlop(self, play);
        if (self->outOfWaterTimer > REMAINS_FISH_MAX_OUT_OF_WATER) {
            Actor_Kill(&self->actor);
            return;
        }
    }

    // Play the swim anim (fins) and layer the procedural tail/body sway on top, so the
    // fish reads as alive whether cruising or darting. Waits on mm.o2r.
    if (self->mmAssetsReady) {
        SkelAnime_Update(&self->skelAnime);
    }
    self->currentRotationAngle += self->nextRotationAngle;
    self->tailRotation = Math_SinS(self->currentRotationAngle) * REMAINS_FISH_WIGGLE_AMPL;
    self->bodyRotation = Math_SinS(self->currentRotationAngle + 0x6978) * REMAINS_FISH_WIGGLE_AMPL;
    self->trunkRotation = Math_SinS(self->currentRotationAngle) * REMAINS_FISH_WIGGLE_AMPL;
}

// ============================================================================
// DRAW
// ============================================================================

// SoH OverrideLimbDrawOpa passes `void* arg` (not Actor*), hence the last param type.
static s32 RemainsAllyFish_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                            void* thisx) {
    RemainsAllyFish* self = (RemainsAllyFish*)thisx;

    // Same procedural sway EnTanron3 applies to the same three limbs of this skeleton.
    if (limbIndex == GYORG_SMALL_FISH_LIMB_ROOT) {
        rot->y += self->bodyRotation;
    }
    if (limbIndex == GYORG_SMALL_FISH_LIMB_TRUNK_ROOT) {
        rot->y += self->trunkRotation;
    }
    if (limbIndex == GYORG_SMALL_FISH_LIMB_TAIL_FIN) {
        rot->y += self->tailRotation;
    }
    return false;
}

void RemainsAllyFish_Draw(Actor* thisx, PlayState* play) {
    RemainsAllyFish* self = (RemainsAllyFish*)thisx;

    if (!self->mmAssetsReady) {
        return; // mm.o2r not mounted yet — invisible this frame, retried in Update
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    // Cast to uintptr_t because this file is compiled as C++ (see header comment).
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sFishSegment0xC_Noop);
    SkelAnime_DrawFlexOpa(play, self->skelAnime.skeleton, self->skelAnime.jointTable, self->skelAnime.dListCount,
                          RemainsAllyFish_OverrideLimbDraw, NULL, &self->actor);

    CLOSE_DISPS(play->state.gfxCtx);
}

// ============================================================================
// SPAWN (runtime ActorDB id — no ActorProfile table in SoH)
// ============================================================================

// Spawn one fish. boss_remains.cpp calls this N times to build a school (see the
// integration notes). The remains index is implicit in the runtime id, so params is
// free — callers may pass a per-fish index via Actor_Spawn directly to stagger the
// school's wiggle phase (read in Init); this convenience entry point passes 0.
Actor* RemainsAllyFish_Spawn(PlayState* play, Vec3f* pos, s16 rotY) {
    if ((play == NULL) || (pos == NULL)) {
        return NULL;
    }
    BossRemains_EnsureActorsRegistered(); // lazy ActorDB registration on first use
    if (gRemainsAllyFishId < 0) {
        return NULL;
    }
    return Actor_Spawn(&play->actorCtx, play, gRemainsAllyFishId, pos->x, pos->y, pos->z, 0, rotY, 0, 0);
}
