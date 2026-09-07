/**
 * rewind_helper.h - Shared trajectory recorder / rewinder (Skijer's NEI)
 *
 * Backs the Phantom Hourglass' Tears-of-the-Kingdom style Recall.
 *
 * DESIGN — this is deliberately NOT a savestate. Only the MOTION of an actor is
 * recorded: world position, rotation, velocity, speed and the pose it was drawn in
 * (its SkelAnime joint table, so the animation runs backwards too). Health, magic, rupees,
 * inventory, actor damage state, chest/switch/scene flags and anything in
 * gSaveContext are never touched, so rewinding an object leaves all of that
 * exactly as it is. Kill an enemy, rewind the rock that killed it — the enemy
 * stays dead. That is the intended behaviour, and it comes for free from
 * recording transforms only.
 *
 * Recording is a fixed static pool — no allocation. Slot 0 is permanently
 * reserved for Link (needed by the L+R self-rewind, which must be able to look
 * backwards at history the player never explicitly selected). The remaining
 * slots go to the nearest actors that are MOVING: a still actor has nothing to
 * recall, so it is never admitted, and a tracked one is dropped once its whole
 * ring is a single pose. That is what keeps the slots on the rolling boulder
 * instead of the seven NPCs standing closer to Link. A body somebody has TAKEN
 * (Rewind_Track) is exempt from all of it for as long as they hold it — being
 * held still is the one case where stillness says nothing about what comes next.
 *
 * Everything is a no-op until Rewind_SetEnabled(1); the Phantom Hourglass turns
 * it on while it rides a C button.
 */

#ifndef REWIND_HELPER_H
#define REWIND_HELPER_H

#include "z64.h"

#define REWIND_SLOTS 8            // 1 for Link + 7 world actors
#define REWIND_FRAMES 200         // ~10 s of history at 20 fps gameplay
#define REWIND_TRACK_RANGE 700.0f // how far out world actors get recorded
#define REWIND_LINK_SLOT 0
#define REWIND_PIN_FRAMES 5   // how long a Rewind_Track claim outlives the call that made it
#define REWIND_IDLE_EVICT 200 // a slot idle this long may be reused by something that IS moving

/** The ACTORCAT_* set the pool records, for callers that scan for a recall target. */
const u8* Rewind_TrackedCats(s32* count);

#define REWIND_MAX_LIMBS 32       // wider joint tables are not recorded: the actor still rewinds, minus its pose
#define REWIND_MAX_INSTANCE 16384 // an actor struct bigger than this is a bad table entry, not an actor

/** One recorded frame — see the header comment. */
typedef struct {
    Vec3f pos;
    Vec3f velocity;
    Vec3s rot;
    s16 pad;
    f32 speedXZ;
    f32 animFrame;
    Vec3s joints[REWIND_MAX_LIMBS];
} RewindFrame;

/** Master switch. Off (the default) makes Rewind_Tick a single compare. */
void Rewind_SetEnabled(u8 enabled);
u8 Rewind_IsEnabled(void);

/**
 * Per-frame recorder. Call unconditionally from CustomItems_Update.
 * Recording pauses while the world is frozen (nothing meaningful moves, and it
 * would otherwise flood the ring with duplicate frames) and for any actor that
 * is currently being scrubbed.
 */
void Rewind_Tick(PlayState* play);

/**
 * Drop the slots whose actor no longer exists. Rewind_Tick does this itself; an
 * item that scrubs BEFORE the tick runs in its frame must call it first, or it
 * could drive an actor that died in the previous frame's later passes.
 */
void Rewind_Validate(PlayState* play);

/**
 * Put `actor` in the pool now, evicting the stillest unclaimed slot if it is full, and claim it
 * for REWIND_PIN_FRAMES. A claimed slot is never evicted, neither for being still nor to make
 * room for another Rewind_Track.
 *
 * The automatic admission test is "it moved during its own update", which is blind to an actor
 * being carried by something else: the engine syncs prevPos to world.pos immediately before each
 * actor's update, so a body moved from the PLAYER's update (Ultrahand's carry) always reads as
 * still by the time its own turn comes. Whoever takes a body over calls this instead.
 *
 * CALL IT EVERY FRAME you hold the body, not once on the grab: the claim expires on its own, so a
 * held body that is never released can never strand a slot. Stasis is the case that makes this
 * necessary — it holds for exactly REWIND_FRAMES, so a one-shot claim would run out of history at
 * the very moment the launch begins.
 */
s32 Rewind_Track(struct Actor* actor);

/**
 * Non-zero if `actor` has at least `minFrames` of usable history — frames in
 * which it actually moved. The still tail at the end of the ring does not count.
 */
s32 Rewind_HasHistory(struct Actor* actor, s32 minFrames);

/** How many recorded frames `actor` has (0 if untracked). */
s32 Rewind_GetLength(struct Actor* actor);

/**
 * Enter scrub mode for `actor` and stop recording it until Rewind_End. The read
 * cursor starts at the last frame the actor moved, so a boulder that came to rest
 * five seconds ago starts rolling back at once instead of sitting still for five
 * seconds of magic first. Returns 0 if it has no history.
 */
s32 Rewind_Begin(struct Actor* actor);

/**
 * Move the read cursor by `dir` frames (negative = into the past, positive =
 * back toward the present) and write that transform onto the actor.
 * @return 0 when the cursor is already at the requested end of the buffer.
 */
s32 Rewind_Scrub(struct Actor* actor, s32 dir);

/**
 * Leave scrub mode.
 * @param keepMomentum non-zero restores the velocity recorded at the cursor
 *                     (object carries on as it was); zero drops it to rest,
 *                     which is what ToTK's Recall does when it lets go.
 */
void Rewind_End(struct Actor* actor, u8 keepMomentum);

/** Non-zero while `actor` is being scrubbed (NULL asks "is anything scrubbing?"). */
s32 Rewind_IsScrubbing(struct Actor* actor);

/** The read cursor while scrubbing, else the end of the still tail — where a recall would start. */
s32 Rewind_GetPathStart(struct Actor* actor);

/** Recorded position `back` frames behind the newest one. Returns 0 outside the history. */
s32 Rewind_GetPathPos(struct Actor* actor, s32 back, Vec3f* out);

/** Drop the whole pool. Called automatically on scene change. */
void Rewind_Reset(void);

#endif // REWIND_HELPER_H
