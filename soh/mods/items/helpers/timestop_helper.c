/**
 * timestop_helper.c - Shared world-time arbiter (Skijer's NEI) — OoT backend
 *
 * See timestop_helper.h for the contract. OoT specifics:
 *   - Motion scale rides on gChampionSlowFactor, already multiplied into
 *     Actor_UpdatePos' speedRate by soh/src/code/z_actor.c.
 *   - The day/night clock is gTimeSpeed; MM's equivalent is the R_TIME_SPEED reg.
 *   - Actor lists are `.head` in OoT (`.first` in MM).
 */

#include "timestop_helper.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

// Motion scale consumed by z_actor.c. Defined in extended_equipment.c.
extern f32 gChampionSlowFactor;

#define TIMECTL_LIST_HEAD(list) ((list).head)

// Categories worth hard-freezing. BG/SWITCH are included so moving platforms and
// timed switches stop too; PLAYER is deliberately absent (Link always moves).
static const u8 sTimeCtlFreezeCats[] = {
    ACTORCAT_SWITCH, ACTORCAT_BG, ACTORCAT_EXPLOSIVE, ACTORCAT_NPC, ACTORCAT_ENEMY, ACTORCAT_MISC, ACTORCAT_BOSS,
};

typedef struct {
    u8 active;
    u8 freezeClock;
    f32 worldSpeed;
} TimeCtlClaim;

static TimeCtlClaim sTimeCtlClaims[TIMECTL_OWNER_MAX];
static TimeCtlOwner sTimeCtlActiveOwner = TIMECTL_OWNER_NONE;
static f32 sTimeCtlWorldSpeed = 1.0f;
static u8 sTimeCtlClockHeld = 0;      // we are currently holding the clock at 0
static u16 sTimeCtlClockSaved = 0;    // the value we took over from
static s16 sTimeCtlLastSceneNum = -1; // auto-reset across scene changes
static u8 sTimeCtlFrozenLastFrame = 0;

// ---------------------------------------------------------------------------
// AC-collider cache
//
// The whole reason this exists: a frozen actor never runs its update, so it never
// calls CollisionCheck_SetAC, so it is absent from the AC list and Link's sword,
// arrows and items pass straight through it. Remembering the AC colliders each
// actor registers lets the freeze pass re-register them on the actor's behalf.
//
// Entries are deduped by (actor, collider) and capped; the cache is dropped on
// scene change and on a full reset, which is when actor pointers stop meaning
// anything. Entries are only ever consulted for actors found alive in the actor
// lists during the same walk, so a stale pointer is never dereferenced.
// ---------------------------------------------------------------------------
typedef struct {
    Actor* actor;
    Collider* collider;
} TimeCtlAcEntry;

static TimeCtlAcEntry sTimeCtlAcCache[TIMECTL_MAX_AC_TRACKED];
static s32 sTimeCtlAcCount = 0;

// ---------------------------------------------------------------------------
// Arbitration
// ---------------------------------------------------------------------------

static void TimeCtl_ApplyNow(PlayState* play);

/**
 * Recompute which claim wins. Highest owner id with an active claim takes the
 * world; everything else is ignored until it becomes the top claim again.
 */
static void TimeCtl_Recompute(void) {
    s32 i;

    sTimeCtlActiveOwner = TIMECTL_OWNER_NONE;
    for (i = TIMECTL_OWNER_MAX - 1; i > TIMECTL_OWNER_NONE; i--) {
        if (sTimeCtlClaims[i].active) {
            sTimeCtlActiveOwner = (TimeCtlOwner)i;
            break;
        }
    }

    if (sTimeCtlActiveOwner == TIMECTL_OWNER_NONE) {
        sTimeCtlWorldSpeed = 1.0f;
    } else {
        sTimeCtlWorldSpeed = sTimeCtlClaims[sTimeCtlActiveOwner].worldSpeed;
        if (sTimeCtlWorldSpeed < 0.0f) {
            sTimeCtlWorldSpeed = 0.0f;
        } else if (sTimeCtlWorldSpeed > 1.0f) {
            sTimeCtlWorldSpeed = 1.0f;
        }
    }

    gChampionSlowFactor = sTimeCtlWorldSpeed;
}

void TimeCtl_Request(TimeCtlOwner owner, f32 worldSpeed, u8 freezeClock) {
    if ((owner <= TIMECTL_OWNER_NONE) || (owner >= TIMECTL_OWNER_MAX)) {
        return;
    }
    sTimeCtlClaims[owner].active = 1;
    sTimeCtlClaims[owner].worldSpeed = worldSpeed;
    sTimeCtlClaims[owner].freezeClock = freezeClock;
    TimeCtl_Recompute();

    // Apply on THIS frame. Callers sit at very different points inside
    // Player_Update — Zonai Permafrost finishes its cast well after
    // TimeCtl_Update has already run — so deferring to the next frame made the
    // freeze visibly fail to take on the frame it was cast.
    TimeCtl_ApplyNow(gPlayState);
}

void TimeCtl_Release(TimeCtlOwner owner) {
    if ((owner <= TIMECTL_OWNER_NONE) || (owner >= TIMECTL_OWNER_MAX)) {
        return;
    }
    // Cleanup paths call this unconditionally every frame (Champion_Cleanup runs
    // whenever the tunic slot is not 3). Without this early-out the immediate apply
    // below would re-walk every actor list several times a frame for nothing.
    if (!sTimeCtlClaims[owner].active) {
        return;
    }
    sTimeCtlClaims[owner].active = 0;
    sTimeCtlClaims[owner].worldSpeed = 1.0f;
    sTimeCtlClaims[owner].freezeClock = 0;
    TimeCtl_Recompute();
    TimeCtl_ApplyNow(gPlayState);
}

f32 TimeCtl_GetWorldSpeed(void) {
    return sTimeCtlWorldSpeed;
}

u8 TimeCtl_IsActive(void) {
    return (sTimeCtlActiveOwner != TIMECTL_OWNER_NONE) && (sTimeCtlWorldSpeed < 1.0f);
}

u8 TimeCtl_IsFrozen(void) {
    return (sTimeCtlActiveOwner != TIMECTL_OWNER_NONE) && (sTimeCtlWorldSpeed <= 0.0f);
}

TimeCtlOwner TimeCtl_GetOwner(void) {
    return sTimeCtlActiveOwner;
}

s32 TimeCtl_GetStutterFrames(void) {
    s32 hold;

    if ((sTimeCtlWorldSpeed >= 1.0f) || (sTimeCtlWorldSpeed <= 0.0f)) {
        return 0; // full speed, or a hard stop that freezeTimer handles outright
    }
    // Tick 1 frame in N, so hold for N-1. 0.33 -> N=3 -> hold 2 -> a third of normal.
    hold = (s32)(1.0f / sTimeCtlWorldSpeed) - 1;
    if (hold < 1) {
        hold = 1;
    }
    return hold;
}

s32 TimeCtl_IsActorExempt(Actor* actor) {
    Player* player;

    if ((actor == NULL) || (gPlayState == NULL)) {
        return 1;
    }
    if (actor->category == ACTORCAT_PLAYER) {
        return 1;
    }
    // Native projectiles (arrows, seeds, hookshot, boomerang) keep full speed so
    // the player can still act meaningfully inside the effect.
    if (actor->category == ACTORCAT_ITEMACTION) {
        return 1;
    }
    player = GET_PLAYER(gPlayState);
    if ((player != NULL) && (actor->parent == &player->actor)) {
        return 1; // custom-item projectiles are spawned as Link's children
    }
    return 0;
}

// ---------------------------------------------------------------------------
// AC-collider cache
// ---------------------------------------------------------------------------

void TimeCtl_NoteAcCollider(Collider* collider) {
    s32 i;

    if ((collider == NULL) || (collider->actor == NULL)) {
        return;
    }
    // While frozen the only registrations happening are our own re-registrations,
    // and Link's; neither belongs in the cache.
    if (TimeCtl_IsFrozen()) {
        return;
    }
    if (TimeCtl_IsActorExempt(collider->actor)) {
        return;
    }

    for (i = 0; i < sTimeCtlAcCount; i++) {
        if (sTimeCtlAcCache[i].collider == collider) {
            sTimeCtlAcCache[i].actor = collider->actor; // collider may have been re-attached
            return;
        }
    }
    if (sTimeCtlAcCount < TIMECTL_MAX_AC_TRACKED) {
        sTimeCtlAcCache[sTimeCtlAcCount].actor = collider->actor;
        sTimeCtlAcCache[sTimeCtlAcCount].collider = collider;
        sTimeCtlAcCount++;
    }
}

/**
 * Re-register `actor`'s cached AC colliders so it can still be hit while frozen.
 * @return non-zero if last frame's collision pass already landed a hit on it, in
 *         which case the caller must let the actor update for one frame so the
 *         damage is actually processed (it flinches or dies, then re-freezes).
 *
 * Reading acFlags BEFORE re-registering matters: CollisionCheck_SetAC runs the
 * collider's AC reset function, which clears AC_HIT.
 */
static u8 TimeCtl_ReapplyAc(PlayState* play, Actor* actor) {
    u8 wasHit = 0;
    s32 i;

    for (i = 0; i < sTimeCtlAcCount; i++) {
        Collider* col = sTimeCtlAcCache[i].collider;

        if ((sTimeCtlAcCache[i].actor != actor) || (col == NULL) || (col->actor != actor)) {
            continue;
        }
        if (col->acFlags & AC_HIT) {
            wasHit = 1;
            continue; // let the actor's own update consume the hit
        }
        if (col->acFlags & AC_ON) {
            CollisionCheck_SetAC(play, &play->colChkCtx, col);
        }
    }
    return wasHit;
}

void TimeCtl_ClearIframes(Actor* actor) {
    s32 i;

    if ((actor == NULL) || (actor->update == NULL)) {
        return;
    }
    for (i = 0; i < sTimeCtlAcCount; i++) {
        Collider* col = sTimeCtlAcCache[i].collider;

        if ((sTimeCtlAcCache[i].actor != actor) || (col == NULL) || (col->actor != actor)) {
            continue;
        }
        // Re-arm the collider. NOT AC_HIT — the passes above use it to hand a frozen or
        // stuttered actor a live frame so the damage it just took is actually processed.
        col->acFlags |= AC_ON;
    }
    actor->colorFilterTimer = 0; // the damage flash some enemies gate invulnerability on
}

// ---------------------------------------------------------------------------
// Application
// ---------------------------------------------------------------------------

/**
 * Refresh freezeTimer on every non-exempt actor. Called each frame during a hard
 * stop so newly spawned actors are caught too; DECR() in Actor_UpdateAll eats one
 * count per frame, so TIMECTL_FREEZE_REFRESH must stay >= 2.
 */
static void TimeCtl_FreezeAll(PlayState* play, u8 frozen) {
    u32 i;

    for (i = 0; i < ARRAY_COUNT(sTimeCtlFreezeCats); i++) {
        Actor* actor = TIMECTL_LIST_HEAD(play->actorCtx.actorLists[sTimeCtlFreezeCats[i]]);

        while (actor != NULL) {
            if (!TimeCtl_IsActorExempt(actor)) {
                if (!frozen) {
                    actor->freezeTimer = 0;
                } else {
                    // Silence it. Actor_UpdateFlaggedAudio is called from the projection
                    // pass in Actor_UpdateAll, which walks EVERY actor and is NOT gated by
                    // freezeTimer — so a continuous flagged sfx an actor happened to be
                    // holding when it froze keeps being re-emitted every frame forever, and
                    // the actor never runs again to clear it. That is the drone that outlived
                    // the time stop. A stopped world should be silent anyway, and the actor
                    // re-establishes its own sfx on its first live frame.
                    if (actor->sfx != 0) {
                        actor->sfx = 0;
                        Audio_StopSfxByPos(&actor->projectedPos);
                    }
                    // Keep the actor hittable, and give it one live frame whenever
                    // it actually got hit so the damage lands instead of being
                    // silently dropped by the frozen branch's ResetDamage.
                    actor->freezeTimer = TimeCtl_ReapplyAc(play, actor) ? 0 : TIMECTL_FREEZE_REFRESH;
                }
            }
            actor = actor->next;
        }
    }
}

/**
 * Partial-slowdown counterpart of the hittable pass inside TimeCtl_FreezeAll.
 *
 * A partial slow is not expressed by scaling motion — z_actor re-freezes each actor for
 * TimeCtl_GetStutterFrames() after every update, so on most frames the actor does NOT run.
 * An actor that does not run never calls CollisionCheck_SetAC, so it drops out of the AC
 * list entirely and arrows, seeds and the sword pass straight through it. At a third speed
 * that is two frames out of every three with no hitbox at all, which reads in game as
 * ranged attacks doing nothing. The hard stop already re-registers colliders on the actor's
 * behalf; the slow has to do exactly the same.
 *
 * Only actors that will STILL be frozen after this frame's DECR are re-registered
 * (freezeTimer > 1). One that is about to wake up registers its own collider moments later,
 * and doing both would put the same collider into the AC list twice.
 */
static void TimeCtl_KeepStutteredHittable(PlayState* play) {
    u32 i;

    for (i = 0; i < ARRAY_COUNT(sTimeCtlFreezeCats); i++) {
        Actor* actor = TIMECTL_LIST_HEAD(play->actorCtx.actorLists[sTimeCtlFreezeCats[i]]);

        while (actor != NULL) {
            if ((actor->freezeTimer > 1) && !TimeCtl_IsActorExempt(actor)) {
                if (TimeCtl_ReapplyAc(play, actor)) {
                    actor->freezeTimer = 0; // it was hit: give it a live frame to react
                }
            }
            actor = actor->next;
        }
    }
}

/** Take over / hand back the day-night clock, remembering the original speed. */
static void TimeCtl_ApplyClock(u8 wantFrozen) {
    if (wantFrozen) {
        if (!sTimeCtlClockHeld) {
            sTimeCtlClockSaved = gTimeSpeed;
            sTimeCtlClockHeld = 1;
        }
        gTimeSpeed = 0;
    } else if (sTimeCtlClockHeld) {
        gTimeSpeed = sTimeCtlClockSaved;
        sTimeCtlClockHeld = 0;
        sTimeCtlClockSaved = 0;
    }
}

/** The actual per-frame work, shared by TimeCtl_Update and the immediate apply. */
static void TimeCtl_ApplyNow(PlayState* play) {
    u8 frozen;
    u8 wantClockFrozen;

    if (play == NULL) {
        return;
    }

    frozen = TimeCtl_IsFrozen();
    wantClockFrozen = (sTimeCtlActiveOwner != TIMECTL_OWNER_NONE) && sTimeCtlClaims[sTimeCtlActiveOwner].freezeClock;

    if (frozen) {
        TimeCtl_FreezeAll(play, 1);
    } else {
        if (sTimeCtlFrozenLastFrame) {
            // Leaving a hard stop: clear the queued freeze so actors resume on the
            // very next frame instead of coasting for TIMECTL_FREEZE_REFRESH frames.
            TimeCtl_FreezeAll(play, 0);
            sTimeCtlAcCount = 0;
        }
        if (TimeCtl_IsActive()) {
            // Slow motion, not a stop: the actors are being stuttered by z_actor, and
            // stuttered actors are just as absent from the AC list as frozen ones.
            TimeCtl_KeepStutteredHittable(play);
        }
    }
    sTimeCtlFrozenLastFrame = frozen;

    TimeCtl_ApplyClock(wantClockFrozen);

    // Keep the motion scale authoritative: other code may have poked the global.
    gChampionSlowFactor = sTimeCtlWorldSpeed;
}

void TimeCtl_Update(PlayState* play) {
    if (play == NULL) {
        return;
    }

    // Scene change wipes every claim: the actors a claim was freezing are gone,
    // and leaving the clock held across a load would stall the day forever.
    if (sTimeCtlLastSceneNum != play->sceneNum) {
        sTimeCtlLastSceneNum = play->sceneNum;
        sTimeCtlAcCount = 0;
        if (sTimeCtlActiveOwner != TIMECTL_OWNER_NONE || sTimeCtlClockHeld) {
            TimeCtl_Reset(play);
            return;
        }
    }

    TimeCtl_ApplyNow(play);
}

void TimeCtl_Reset(PlayState* play) {
    s32 i;

    for (i = 0; i < TIMECTL_OWNER_MAX; i++) {
        sTimeCtlClaims[i].active = 0;
        sTimeCtlClaims[i].worldSpeed = 1.0f;
        sTimeCtlClaims[i].freezeClock = 0;
    }
    sTimeCtlActiveOwner = TIMECTL_OWNER_NONE;
    sTimeCtlWorldSpeed = 1.0f;
    gChampionSlowFactor = 1.0f;

    if (play != NULL && sTimeCtlFrozenLastFrame) {
        TimeCtl_FreezeAll(play, 0);
    }
    sTimeCtlFrozenLastFrame = 0;
    sTimeCtlAcCount = 0;

    TimeCtl_ApplyClock(0);
}
