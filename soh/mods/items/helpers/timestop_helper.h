/**
 * timestop_helper.h - Shared world-time arbiter (Skijer's NEI)
 *
 * One owner of the world clock. Champion's Tunic (slow-motion), Zonai Permafrost
 * (hard stop) and the Phantom Hourglass (rewind scrub) all want to alter how fast
 * the world runs; without an arbiter the one that finishes FIRST restores the
 * world to 1.0f and silently breaks the one still running.
 *
 * Requests are prioritized by owner id (higher wins). Releasing a lower-priority
 * owner never disturbs a higher-priority one that is still active.
 *
 * IMPORTANT — this module only ever touches MOTION and the day/night clock.
 * It never reads or writes health, magic, rupees, actor damage state, scene
 * flags or save data. Damage dealt while time is stopped therefore persists,
 * by construction.
 *
 * OoT backend: gChampionSlowFactor (read by z_actor.c Actor_UpdatePos) + gTimeSpeed.
 */

#ifndef TIMESTOP_HELPER_H
#define TIMESTOP_HELPER_H

#include "z64.h" // brings in z64collision_check.h for Collider

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Owner ids double as priority: a higher value overrides a lower one.
 * Keep PERMAFROST last — a hard stop must always beat a partial slowdown.
 */
typedef enum {
    TIMECTL_OWNER_NONE = 0,
    TIMECTL_OWNER_CHAMPION,   // Champion's Tunic: flurry rush / bullet time (partial slow)
    TIMECTL_OWNER_HOURGLASS,  // Phantom Hourglass: rewind scrub (full stop)
    TIMECTL_OWNER_PERMAFROST, // Zonai Permafrost: full stop
    TIMECTL_OWNER_MAX
} TimeCtlOwner;

/** Actors keep this many frames of freeze queued; DECR() eats one per frame. */
#define TIMECTL_FREEZE_REFRESH 3

/**
 * How many frames an actor stays frozen between updates during a PARTIAL slowdown.
 * A partial slow is expressed as "tick 1 frame in N" rather than by scaling motion,
 * because scaling alone would leave animations and AI timers running at full speed.
 * Returns 0 when nothing is claiming or the world is fully stopped.
 */
s32 TimeCtl_GetStutterFrames(void);

/** How many (actor, AC collider) pairs stay remembered for the hittable-while-frozen pass. */
#define TIMECTL_MAX_AC_TRACKED 64

/**
 * Claim (or update) the world speed for `owner`.
 * The effect is applied immediately, on this very frame — callers run at wildly
 * different points inside Player_Update, and waiting for the next TimeCtl_Update
 * made a freeze visibly miss its first frame.
 * @param worldSpeed 1.0f = normal, 0.0f = fully frozen, in between = slow motion.
 * @param freezeClock non-zero also halts the day/night cycle.
 */
void TimeCtl_Request(TimeCtlOwner owner, f32 worldSpeed, u8 freezeClock);

/** Drop `owner`'s claim. Safe to call when it never had one. */
void TimeCtl_Release(TimeCtlOwner owner);

/** Effective world speed right now (1.0f when nobody is claiming). */
f32 TimeCtl_GetWorldSpeed(void);

/** Non-zero while any owner is slowing or stopping the world. */
u8 TimeCtl_IsActive(void);

/** Non-zero only for a FULL stop (worldSpeed == 0). */
u8 TimeCtl_IsFrozen(void);

/** Which owner currently drives the world speed. */
TimeCtlOwner TimeCtl_GetOwner(void);

/**
 * Non-zero if `actor` must keep running at full speed regardless of the effect:
 * Link himself, native projectiles (arrows, seeds, hookshot, boomerang), and
 * anything Link spawned as a child (custom item projectiles). This is what keeps
 * Link's own weapons usable while the rest of the world is stopped.
 */
s32 TimeCtl_IsActorExempt(struct Actor* actor);

/**
 * Called from CollisionCheck_SetAC for every AC collider an actor registers.
 *
 * A frozen actor never runs its update, so it never re-registers its AC collider
 * and Link's sword or arrows would pass straight through it. Remembering the
 * colliders here lets the freeze pass re-register them on the actor's behalf, so
 * enemies stay hittable with time stopped.
 */
// NOTE: `Collider` (not `struct Collider`) — it is an anonymous struct typedef in
// both games, so the elaborated form would be a different, incomplete type.
void TimeCtl_NoteAcCollider(Collider* collider);

/**
 * Strip an actor's post-hit invulnerability so the next swing lands.
 *
 * Enemies implement "you already hit me, wait your turn" in two ways and both are undone
 * here: some drop AC_ON on their collider while flinching — which makes them unhittable
 * AND makes the re-registration passes skip them — and others gate on the damage-flash
 * timer. AC_HIT is deliberately left alone: the freeze/stutter passes read it to decide
 * when to give a frozen actor a live frame to actually process the damage.
 *
 * Only affects colliders already seen via TimeCtl_NoteAcCollider, so an actor that has
 * never registered an AC collider is simply left alone.
 */
void TimeCtl_ClearIframes(struct Actor* actor);

/**
 * Per-frame apply. Call unconditionally from CustomItems_Update — it is a cheap
 * no-op when nobody is claiming, and it self-resets across scene changes.
 */
void TimeCtl_Update(PlayState* play);

/** Drop every claim and restore the world immediately (scene change, death, unequip). */
void TimeCtl_Reset(PlayState* play);

#ifdef __cplusplus
}
#endif

#endif // TIMESTOP_HELPER_H
