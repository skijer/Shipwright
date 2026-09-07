/**
 * rewind_helper.c - Shared trajectory recorder / rewinder (Skijer's NEI) — OoT backend
 *
 * See rewind_helper.h for the contract. OoT specifics: actor lists are `.head`,
 * the explosives category is ACTORCAT_EXPLOSIVE, and horizontal speed lives in
 * Actor.speedXZ (MM calls it Actor.speed).
 */

#include "rewind_helper.h"
#include "timestop_helper.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ActorDB.h" // instanceSize, to walk an actor's own struct for its SkelAnime
#include <string.h>

#define REWIND_LIST_HEAD(list) ((list).head)

// Everything but Link himself and the bosses. Not a curated list on purpose: what makes a body
// worth recording is that it MOVED, not what drawer the game files it in — a temple elevator is
// BG, a king who steps aside is NPC, and picking categories is how the first version came to miss
// half of what the player actually watches move. OoT: MM spells it ACTORCAT_EXPLOSIVES.
static const u8 sRewindCats[] = {
    ACTORCAT_SWITCH, ACTORCAT_BG,         ACTORCAT_EXPLOSIVE, ACTORCAT_NPC,  ACTORCAT_ENEMY,
    ACTORCAT_PROP,   ACTORCAT_ITEMACTION, ACTORCAT_MISC,      ACTORCAT_DOOR, ACTORCAT_CHEST,
};

const u8* Rewind_TrackedCats(s32* count) {
    *count = ARRAY_COUNT(sRewindCats);
    return sRewindCats;
}

typedef struct {
    Actor* actor;
    SkelAnime* skelAnime; // the actor's own, found by Rewind_FindSkelAnime; NULL = motion only
    RewindFrame frames[REWIND_FRAMES];
    s16 count;      // valid frames (<= REWIND_FRAMES)
    s16 writeIdx;   // next slot to write
    s16 cursor;     // frames behind newest while scrubbing (0 = present)
    s16 idleFrames; // consecutive frames the body has not moved (nothing was recorded for those)
    s16 pinTimer;   // frames left on an owner's Rewind_Track claim
    u8 used;
    u8 scrubbing;
} RewindSlot;

static RewindSlot sRewindSlots[REWIND_SLOTS];
static u8 sRewindEnabled = 0;
static u8 sRewindSeen[REWIND_SLOTS];
static s16 sRewindLastSceneNum = -1;

// ---------------------------------------------------------------------------
// Slot bookkeeping
// ---------------------------------------------------------------------------

static s32 Rewind_FindSlot(Actor* actor) {
    s32 i;

    if (actor == NULL) {
        return -1;
    }
    for (i = 0; i < REWIND_SLOTS; i++) {
        if (sRewindSlots[i].used && (sRewindSlots[i].actor == actor)) {
            return i;
        }
    }
    return -1;
}

static void Rewind_ClearSlot(s32 idx) {
    sRewindSlots[idx].actor = NULL;
    sRewindSlots[idx].skelAnime = NULL;
    sRewindSlots[idx].count = 0;
    sRewindSlots[idx].writeIdx = 0;
    sRewindSlots[idx].cursor = 0;
    sRewindSlots[idx].idleFrames = 0;
    sRewindSlots[idx].pinTimer = 0;
    sRewindSlots[idx].used = 0;
    sRewindSlots[idx].scrubbing = 0;
}

static s32 Rewind_PosChanged(const Vec3f* a, const Vec3f* b) {
    return (a->x != b->x) || (a->y != b->y) || (a->z != b->z);
}

static size_t Rewind_GetInstanceSize(Actor* actor) {
    ActorDBEntry* dbEntry = ActorDB_Retrieve(actor->id);

    return (dbEntry != NULL) ? dbEntry->instanceSize : 0;
}

// Every actor keeps its SkelAnime at its own offset, so the instance is walked for the one struct
// whose jointTable points back inside it — the way Stasis finds a frozen body's colliders.
//
// The joint table must fit ENTIRELY inside the instance, not merely start inside it: this is a
// pattern match on arbitrary memory, and a false positive near the tail would otherwise make the
// pose memcpy run off the end of the actor block and corrupt the pool behind it.
static SkelAnime* Rewind_FindSkelAnime(Actor* actor) {
    u8* base = (u8*)actor;
    size_t size = Rewind_GetInstanceSize(actor);
    size_t offset;

    // An actor with no draw has no pose to rewind, so there is nothing here worth the walk.
    if (actor->draw == NULL) {
        return NULL;
    }
    // The size comes from a table, and reading past the end of a real allocation because that table
    // disagrees with it is not a risk worth taking for a cosmetic feature. Anything outside the
    // range every vanilla actor lives in is refused outright.
    if ((size < (sizeof(Actor) + sizeof(SkelAnime))) || (size > REWIND_MAX_INSTANCE)) {
        return NULL;
    }
    for (offset = sizeof(Actor); offset <= (size - sizeof(SkelAnime)); offset += 4) {
        SkelAnime* candidate = (SkelAnime*)(base + offset);
        u8* joints = (u8*)candidate->jointTable;
        u8* jointsEnd = joints + (candidate->limbCount * sizeof(Vec3s));

        // A real skeleton has limbs to spare; 1-3 is almost always three bytes of something else.
        if ((candidate->limbCount >= 4) && (candidate->limbCount <= REWIND_MAX_LIMBS) &&
            (candidate->skeleton != NULL) && (candidate->animation != NULL) && (joints > base) &&
            (jointsEnd <= (base + size)) && (candidate->animLength > 0.0f) && (candidate->animLength < 10000.0f) &&
            (candidate->curFrame >= 0.0f) && (candidate->curFrame <= candidate->animLength)) {
            return candidate;
        }
    }
    return NULL;
}

static void Rewind_BindSlot(s32 idx, Actor* actor) {
    Rewind_ClearSlot(idx);
    sRewindSlots[idx].actor = actor;
    // Link's pose is the engine's business: it drives his skeleton from his own state machine every
    // frame, so writing joints back would only fight it. He rewinds as motion alone.
    if (actor->category != ACTORCAT_PLAYER) {
        sRewindSlots[idx].skelAnime = Rewind_FindSkelAnime(actor);
    }
    sRewindSlots[idx].used = 1;
}

static s32 Rewind_HasUsablePose(RewindSlot* slot) {
    SkelAnime* skelAnime = slot->skelAnime;

    if ((skelAnime == NULL) || (skelAnime->limbCount > REWIND_MAX_LIMBS) || (skelAnime->jointTable == NULL)) {
        slot->skelAnime = NULL;
        return 0;
    }
    return 1;
}

static void Rewind_RecordPose(RewindSlot* slot, RewindFrame* f) {
    if (!Rewind_HasUsablePose(slot)) {
        return;
    }
    f->animFrame = slot->skelAnime->curFrame;
    memcpy(f->joints, slot->skelAnime->jointTable, slot->skelAnime->limbCount * sizeof(Vec3s));
}

static void Rewind_ApplyPose(RewindSlot* slot, RewindFrame* f) {
    if (!Rewind_HasUsablePose(slot)) {
        return;
    }
    slot->skelAnime->curFrame = f->animFrame;
    memcpy(slot->skelAnime->jointTable, f->joints, slot->skelAnime->limbCount * sizeof(Vec3s));
}

static RewindFrame* Rewind_FrameAt(RewindSlot* slot, s32 back);

/**
 * Push the actor's current transform onto its ring — but ONLY if it moved.
 *
 * Standing still records nothing at all. A ring that banked the still frames too would push the
 * real motion out of its own history just by waiting: a temple elevator that came down during a
 * cutscene would stop being recallable about ten seconds later, for no reason the player can see.
 * Recording motion only means a body stays recallable until it moves again, however long that is.
 */
static void Rewind_Record(RewindSlot* slot) {
    RewindFrame* f = &slot->frames[slot->writeIdx];
    Actor* actor = slot->actor;
    RewindFrame* newest = Rewind_FrameAt(slot, 0);

    if ((newest != NULL) && !Rewind_PosChanged(&newest->pos, &actor->world.pos)) {
        if (slot->idleFrames < REWIND_FRAMES) {
            slot->idleFrames++;
        }
        return;
    }
    slot->idleFrames = 0;

    f->pos = actor->world.pos;
    f->velocity = actor->velocity;
    f->rot = actor->world.rot;
    Rewind_RecordPose(slot, f);
    f->speedXZ = actor->speedXZ;

    slot->writeIdx = (slot->writeIdx + 1) % REWIND_FRAMES;
    if (slot->count < REWIND_FRAMES) {
        slot->count++;
    }
}

/** The frame `back` steps behind the newest one (back == 0 is the present). */
static RewindFrame* Rewind_FrameAt(RewindSlot* slot, s32 back) {
    s32 idx;

    if ((back < 0) || (back >= slot->count)) {
        return NULL;
    }
    idx = slot->writeIdx - 1 - back;
    while (idx < 0) {
        idx += REWIND_FRAMES;
    }
    return &slot->frames[idx % REWIND_FRAMES];
}

/**
 * Write a recorded transform back onto the actor.
 * prevPos is set alongside world.pos and bgCheckFlags cleared so the engine does
 * not treat the jump as a collision sweep and snap the actor to a wall — the same
 * trick the Switch Hook's swap uses. home.pos is deliberately left alone: it is
 * an actor's patrol/spawn anchor, and moving it would corrupt AI.
 */
static void Rewind_Apply(RewindSlot* slot, RewindFrame* f, u8 restoreVelocity) {
    Actor* actor = slot->actor;

    Rewind_ApplyPose(slot, f);
    actor->world.pos = f->pos;
    actor->prevPos = f->pos;
    actor->world.rot = f->rot;
    actor->shape.rot = f->rot;
    actor->bgCheckFlags = 0;

    if (restoreVelocity) {
        actor->velocity = f->velocity;
        actor->speedXZ = f->speedXZ;
    } else {
        actor->velocity.x = 0.0f;
        actor->velocity.y = 0.0f;
        actor->velocity.z = 0.0f;
        actor->speedXZ = 0.0f;
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void Rewind_SetEnabled(u8 enabled) {
    if (!enabled && sRewindEnabled) {
        Rewind_Reset();
    }
    sRewindEnabled = enabled;
}

u8 Rewind_IsEnabled(void) {
    return sRewindEnabled;
}

s32 Rewind_GetLength(Actor* actor) {
    s32 idx = Rewind_FindSlot(actor);

    return (idx < 0) ? 0 : sRewindSlots[idx].count;
}

s32 Rewind_HasHistory(Actor* actor, s32 minFrames) {
    return Rewind_GetLength(actor) >= minFrames;
}

s32 Rewind_Begin(Actor* actor) {
    s32 idx = Rewind_FindSlot(actor);

    if ((idx < 0) || (sRewindSlots[idx].count < 2)) {
        return 0;
    }
    // The newest frame IS the last one it moved on — still frames are never banked.
    sRewindSlots[idx].cursor = 0;
    sRewindSlots[idx].scrubbing = 1;
    return 1;
}

s32 Rewind_Scrub(Actor* actor, s32 dir) {
    s32 idx = Rewind_FindSlot(actor);
    RewindSlot* slot;
    RewindFrame* f;
    s32 newCursor;

    if ((idx < 0) || (actor == NULL) || (actor->update == NULL)) {
        return 0;
    }
    slot = &sRewindSlots[idx];
    if (!slot->scrubbing) {
        return 0;
    }

    // dir < 0 walks into the past, which means a LARGER cursor.
    newCursor = slot->cursor - dir;
    if (newCursor < 0) {
        newCursor = 0;
    } else if (newCursor > (slot->count - 1)) {
        newCursor = slot->count - 1;
    }
    if (newCursor == slot->cursor) {
        // Already pinned at an end — still hold the actor at that frame so it
        // does not drift, but report "no movement" so the caller can stop.
        f = Rewind_FrameAt(slot, slot->cursor);
        if (f != NULL) {
            Rewind_Apply(slot, f, 0);
            if (actor->category != ACTORCAT_PLAYER) {
                actor->freezeTimer = TIMECTL_FREEZE_REFRESH;
            }
        }
        return 0;
    }

    slot->cursor = (s16)newCursor;
    f = Rewind_FrameAt(slot, slot->cursor);
    if (f == NULL) {
        return 0;
    }
    Rewind_Apply(slot, f, 0);
    // The actor must not run its own update while we drive it, or its AI would
    // immediately fight the position we just wrote.
    //
    // Never Link, though: his update is what reads the button holding the rewind down, so freezing
    // him would freeze the hand on the switch and the scrub could never be released.
    if (actor->category != ACTORCAT_PLAYER) {
        actor->freezeTimer = TIMECTL_FREEZE_REFRESH;
    }
    return 1;
}

void Rewind_End(Actor* actor, u8 keepMomentum) {
    s32 idx = Rewind_FindSlot(actor);
    RewindSlot* slot;
    RewindFrame* f;

    if (idx < 0) {
        return;
    }
    slot = &sRewindSlots[idx];
    slot->scrubbing = 0;

    if ((actor != NULL) && (actor->update != NULL)) {
        f = Rewind_FrameAt(slot, slot->cursor);
        if (f != NULL) {
            Rewind_Apply(slot, f, keepMomentum);
        }
        actor->freezeTimer = 0;
    }

    // History after the release point is now fiction — drop it so a second
    // rewind does not replay a future that never happened.
    if (slot->cursor > 0) {
        s32 drop = slot->cursor;

        slot->writeIdx = slot->writeIdx - drop;
        while (slot->writeIdx < 0) {
            slot->writeIdx += REWIND_FRAMES;
        }
        slot->writeIdx %= REWIND_FRAMES;
        slot->count -= (s16)drop;
        if (slot->count < 0) {
            slot->count = 0;
        }
    }
    slot->cursor = 0;
}

s32 Rewind_IsScrubbing(Actor* actor) {
    s32 i;

    if (actor != NULL) {
        s32 idx = Rewind_FindSlot(actor);
        return (idx >= 0) && sRewindSlots[idx].scrubbing;
    }
    for (i = 0; i < REWIND_SLOTS; i++) {
        if (sRewindSlots[i].used && sRewindSlots[i].scrubbing) {
            return 1;
        }
    }
    return 0;
}

s32 Rewind_Track(Actor* actor) {
    s32 best = -1;
    s32 bestIdle = -1;
    s32 idx;
    s32 i;

    if ((actor == NULL) || (actor->update == NULL) || !sRewindEnabled) {
        return 0;
    }
    idx = Rewind_FindSlot(actor);
    if (idx >= 0) {
        sRewindSlots[idx].pinTimer = REWIND_PIN_FRAMES;
        return 1;
    }
    for (i = 1; i < REWIND_SLOTS; i++) {
        if (sRewindSlots[i].scrubbing || (sRewindSlots[i].pinTimer > 0)) {
            continue; // mid-recall, or somebody else is holding it
        }
        if (!sRewindSlots[i].used) {
            best = i;
            break;
        }
        if (sRewindSlots[i].idleFrames > bestIdle) {
            bestIdle = sRewindSlots[i].idleFrames;
            best = i;
        }
    }
    if (best < 0) {
        return 0;
    }
    Rewind_BindSlot(best, actor);
    sRewindSlots[best].pinTimer = REWIND_PIN_FRAMES;
    return 1;
}

s32 Rewind_GetPathStart(Actor* actor) {
    s32 idx = Rewind_FindSlot(actor);

    if (idx < 0) {
        return 0;
    }
    return sRewindSlots[idx].scrubbing ? sRewindSlots[idx].cursor : 0;
}

s32 Rewind_GetPathPos(Actor* actor, s32 back, Vec3f* out) {
    s32 idx = Rewind_FindSlot(actor);
    RewindFrame* f;

    if (idx < 0) {
        return 0;
    }
    f = Rewind_FrameAt(&sRewindSlots[idx], back);
    if (f == NULL) {
        return 0;
    }
    *out = f->pos;
    return 1;
}

void Rewind_Reset(void) {
    s32 i;

    for (i = 0; i < REWIND_SLOTS; i++) {
        Rewind_ClearSlot(i);
    }
}

// ---------------------------------------------------------------------------
// Per-frame pool maintenance + recording
// ---------------------------------------------------------------------------

/** The slot to reuse when a moving candidate turns up and none are free, or -1 to keep them all. */
static s32 Rewind_PickStalest(void) {
    s32 best = -1;
    s32 bestIdle = REWIND_IDLE_EVICT;
    s32 i;

    for (i = 1; i < REWIND_SLOTS; i++) {
        if (sRewindSlots[i].scrubbing || (sRewindSlots[i].pinTimer > 0)) {
            continue;
        }
        if (sRewindSlots[i].idleFrames > bestIdle) {
            bestIdle = sRewindSlots[i].idleFrames;
            best = i;
        }
    }
    return best;
}

/**
 * Fill free slots with the nearest untracked actors that moved on their last update.
 * The engine copies prevPos right before each update, so world.pos != prevPos is
 * "moved last tick" for every category, whether it already ran this frame or not.
 *
 * A full pool gives up its stalest slot rather than turning the candidate away: history is only
 * worth keeping while there is a chance the player wants it back, and something moving right now
 * beats something that stopped a long time ago.
 */
static void Rewind_RefillPool(PlayState* play, Player* player) {
    u32 c;
    s32 i;

    for (i = 1; i < REWIND_SLOTS; i++) {
        Actor* best = NULL;
        f32 bestDistSq = REWIND_TRACK_RANGE * REWIND_TRACK_RANGE;

        if (sRewindSlots[i].used) {
            continue;
        }
        for (c = 0; c < ARRAY_COUNT(sRewindCats); c++) {
            Actor* actor = REWIND_LIST_HEAD(play->actorCtx.actorLists[sRewindCats[c]]);

            while (actor != NULL) {
                if ((actor->update != NULL) && Rewind_PosChanged(&actor->world.pos, &actor->prevPos) &&
                    (Rewind_FindSlot(actor) < 0)) {
                    f32 dx = actor->world.pos.x - player->actor.world.pos.x;
                    f32 dy = actor->world.pos.y - player->actor.world.pos.y;
                    f32 dz = actor->world.pos.z - player->actor.world.pos.z;
                    f32 distSq = (dx * dx) + (dy * dy) + (dz * dz);

                    if (distSq < bestDistSq) {
                        bestDistSq = distSq;
                        best = actor;
                    }
                }
                actor = actor->next;
            }
        }
        if (best == NULL) {
            break; // nothing left in range — the remaining slots stay free
        }
        Rewind_BindSlot(i, best);
    }

    // Nothing free left, but something out there is moving: give it the stalest slot.
    for (i = 1; i < REWIND_SLOTS; i++) {
        if (!sRewindSlots[i].used) {
            return;
        }
    }
    for (c = 0; c < ARRAY_COUNT(sRewindCats); c++) {
        Actor* actor = REWIND_LIST_HEAD(play->actorCtx.actorLists[sRewindCats[c]]);

        while (actor != NULL) {
            if ((actor->update != NULL) && Rewind_PosChanged(&actor->world.pos, &actor->prevPos) &&
                (Rewind_FindSlot(actor) < 0) &&
                (Math_Vec3f_DistXYZ(&actor->world.pos, &player->actor.world.pos) < REWIND_TRACK_RANGE)) {
                s32 victim = Rewind_PickStalest();

                if (victim < 0) {
                    return; // everything in the pool is fresher than the newcomer
                }
                Rewind_BindSlot(victim, actor);
            }
            actor = actor->next;
        }
    }
}

// Actor memory is pooled, so a stale pointer could alias a freshly spawned actor:
// validate by presence in the live lists, never by dereferencing.
//
// EVERY category is swept, not just the recorded ones: Rewind_Track admits a body by hand from
// wherever its owner found it, and validating against sRewindCats alone would drop anything
// outside that set on the very next frame.
void Rewind_Validate(PlayState* play) {
    u32 c;
    s32 i;

    for (i = 1; i < REWIND_SLOTS; i++) {
        sRewindSeen[i] = 0;
    }
    for (c = 0; c < ACTORCAT_MAX; c++) {
        Actor* actor = REWIND_LIST_HEAD(play->actorCtx.actorLists[c]);

        while (actor != NULL) {
            s32 idx = Rewind_FindSlot(actor);

            if (idx > 0) {
                sRewindSeen[idx] = 1;
            }
            actor = actor->next;
        }
    }
    for (i = 1; i < REWIND_SLOTS; i++) {
        if (sRewindSlots[i].used && !sRewindSeen[i]) {
            Rewind_ClearSlot(i);
        }
    }
}

void Rewind_Tick(PlayState* play) {
    Player* player;
    s32 i;

    if (!sRewindEnabled || (play == NULL)) {
        return;
    }

    // Actor pointers do not survive a scene load, and neither does the history.
    if (sRewindLastSceneNum != play->sceneNum) {
        sRewindLastSceneNum = play->sceneNum;
        Rewind_Reset();
        return;
    }

    player = GET_PLAYER(play);
    if (player == NULL) {
        return;
    }

    // Slot 0 is Link's, always. Rebinding wipes it, which is correct: a different
    // Player actor means a different life.
    if (sRewindSlots[REWIND_LINK_SLOT].actor != &player->actor) {
        Rewind_BindSlot(REWIND_LINK_SLOT, &player->actor);
    }

    Rewind_Validate(play);

    // A frozen actor keeps a stale prevPos, so it would read as moving forever; and
    // nothing moves anyway, so recording would only fill the ring with duplicates.
    if (TimeCtl_IsFrozen()) {
        return;
    }

    Rewind_RefillPool(play, player);

    for (i = 0; i < REWIND_SLOTS; i++) {
        RewindSlot* slot = &sRewindSlots[i];

        if (!slot->used || slot->scrubbing || (slot->actor == NULL) || (slot->actor->update == NULL)) {
            continue;
        }
        Rewind_Record(slot);
        if (slot->pinTimer > 0) {
            slot->pinTimer--;
        }
    }
}
