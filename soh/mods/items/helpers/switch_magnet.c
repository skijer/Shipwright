/**
 * Switch magnet — Skijer's NEI.
 *
 * A small piece of aim assist for heavy things in free fall. A Somaria statue, a pushable block, an
 * Armos, anything with weight: while it is on its way down it looks for a floor switch it could
 * press, and leans into it. Land one on a switch from across the room and it reads as a good shot,
 * which is the point — the assist is quiet enough that the credit still feels like the player's.
 *
 * It is deliberately generic and stateless. Anything with a falling body can call
 * SwitchMagnet_Steer once per frame while it falls: Ultrahand throws, Stasis launches, Somaria
 * placements, and whatever else grows a free fall later.
 *
 * Four parts, and each answers a different question:
 *   - SwitchMagnet_Steer aims the arc from far off, by solving the fall and correcting course.
 *   - SwitchMagnet_SnapOnto is the final approach: once the body is over the plate it goes down on
 *     it, squarely, rather than being left to gravity.
 *   - SwitchMagnet_PressUnder does the press itself, the way a pushable block does it in vanilla.
 *     THIS is the one that actually latches a floor switch; see its own comment.
 *   - SwitchMagnet_MakePresser arms the engine's own route as well. A floor switch of the
 *     weight-driven kind reads DYNA_INTERACT_ACTOR_SWITCH_PRESSED, which func_80043334 sets only
 *     for actors carrying ACTOR_FLAG_CAN_PRESS_SWITCHES — and in vanilla that flag is on almost
 *     nothing (En_Am, En_Ru1, En_Partner, Obj_Kibako). Belt and braces with PressUnder: this one
 *     keeps working after the body has been handed back and is moving under its own steam.
 *
 * Consumed via #include from custom_items.c. No header on purpose — 2ship globs mods/ *.h with
 * CONFIGURE_DEPENDS, and a new one there forces a CMake regeneration its vcpkg cannot do.
 */

#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include <math.h>

// How far out it will look at all. Beyond this the shot is the player's problem.
#define SWITCHMAGNET_RANGE 700.0f
// The two bands, both measured on the PARABOLA — how far from the switch this throw was going to
// land if nobody touched it. That single number already carries both "is it pointed at it" and "is
// it travelling about the right speed", which is why neither is tested separately.
//   LOCK   — a near miss. The fall is solved and flown onto the switch outright.
//   ASSIST — a wider miss, nudged frame by frame; if it converges it crosses into LOCK on its own.
#define SWITCHMAGNET_LOCK_MISS 110.0f
#define SWITCHMAGNET_ASSIST_MISS 260.0f
// Correction per frame in the ASSIST band. Small on purpose: enough to pull a wide shot in, never
// enough to turn a body around and fly it somewhere the player did not point it.
#define SWITCHMAGNET_TURN 0x500
#define SWITCHMAGNET_SPEED_STEP 1.0f
#define SWITCHMAGNET_MAX_SPEED 45.0f
// Under this many frames of fall left there is no time to correct anything, and yanking the body
// at the last instant looks like a magnet rather than aim.
#define SWITCHMAGNET_MIN_TIME 6.0f

// The final approach, which is a different question from the arc above: not "where will this land"
// but "is it directly over the plate RIGHT NOW". Three Link-heights up and a 20-unit column, so it
// only ever fires on a body already passing through the space above the switch.
#define SWITCHMAGNET_SNAP_HEIGHT 100.0f
#define SWITCHMAGNET_SNAP_RADIUS 20.0f
// How much higher than the switch the floor may be before we call it blocked.
#define SWITCHMAGNET_SNAP_CLEARANCE 20.0f

// Bodies that ought to press a switch when they land on one, but that vanilla never gave the flag.
// Armos (En_Am) and the small crate already carry it themselves and pass the check below without
// being listed.
static const s16 sSwitchMagnetPressers[] = {
    ACTOR_OBJ_OSHIHIKI,   // pushable block, and the Cane of Somaria's BLOCK summon
    ACTOR_OBJ_LIFT,       // ...and its PLATFORM summon
    ACTOR_EN_LIGHTBOX,    // ...and its Elegy statue
    ACTOR_OBJ_KIBAKO2,    // large crate
    ACTOR_BG_HEAVY_BLOCK, // the silver-gauntlet pillar
    ACTOR_BG_PUSHBOX,     ACTOR_OBJ_HSBLOCK, ACTOR_OBJ_HAMISHI, ACTOR_OBJ_BOMBIWA, ACTOR_EN_ISHI, ACTOR_EN_WOOD02,
};

u8 SwitchMagnet_IsPresser(Actor* actor) {
    s32 i;

    if (actor == NULL) {
        return 0;
    }
    if (actor->flags & ACTOR_FLAG_CAN_PRESS_SWITCHES) {
        return 1;
    }
    for (i = 0; i < (s32)ARRAY_COUNT(sSwitchMagnetPressers); i++) {
        if (sSwitchMagnetPressers[i] == actor->id) {
            return 1;
        }
    }
    return 0;
}

/** Arm the body so landing on a weight-driven switch actually presses it. */
void SwitchMagnet_MakePresser(Actor* actor) {
    if (SwitchMagnet_IsPresser(actor)) {
        actor->flags |= ACTOR_FLAG_CAN_PRESS_SWITCHES;
    }
}

// Is this switch one that weight can press, and is it still waiting to be pressed?
//
// Obj_Switch packs its kind into params: type in bits 0..2, subtype in bits 4..6. Only type 0
// (OBJSWITCH_TYPE_FLOOR) runs the weight path at all — the rusty one wants a hammer, and the eye
// and crystal ones want a projectile. Of its four subtypes, 0 and 1 test specifically for the
// PLAYER on top; 2 and 3 are the ones that go through DynaPolyActor_IsSwitchPressed and so accept
// any heavy actor. Subtype 3 turns its flag back OFF, so it is left alone: landing on one would
// undo a puzzle rather than solve it, which is the opposite of a reward.
//
// (The OBJSWITCH_* enums live in the actor's own overlay header, which mods cannot include.)
static u8 SwitchMagnet_IsPressable(Actor* actor) {
    if (actor->id != ACTOR_OBJ_SWITCH) {
        return 0;
    }
    if ((actor->params & 7) != 0) {
        return 0;
    }
    return (((actor->params >> 4) & 7) == 2);
}

// The top face of a dynapoly actor's own collision, in world Y. Returns 0 if it has none.
//
// Read from the registered CollisionHeader rather than probed with a ray, and that distinction is
// the difference between a block that presses a switch and one that does not. A floor switch
// latches when the engine reports something standing on its dynapoly, which means genuinely resting
// on the plate's collision surface — and an Obj_Switch's ORIGIN is not the top of its plate, so a
// body placed relative to the origin sits slightly inside it or slightly above, and either way
// nothing is standing on anything. A downward ray is no good either: once the body is already on
// the plate the cast starts inside it, misses its top face and reports the floor underneath.
// (Ultrahand learned all of this the hard way; see Pacci_UhDynaTopY.)
static u8 SwitchMagnet_DynaTopY(PlayState* play, Actor* actor, f32* outY) {
    s32 bg;

    for (bg = 0; bg < BG_ACTOR_MAX; bg++) {
        BgActor* bgActor = &play->colCtx.dyna.bgActors[bg];

        if (!(play->colCtx.dyna.bgActorFlags[bg] & 1) || (bgActor->actor != actor) || (bgActor->colHeader == NULL)) {
            continue;
        }
        *outY = actor->world.pos.y + ((f32)bgActor->colHeader->maxBounds.y * actor->scale.y);
        return 1;
    }
    return 0;
}

// How far below its own origin a body's underside sits. Same source, same reason.
static f32 SwitchMagnet_BodyBottom(PlayState* play, Actor* actor) {
    s32 bg;

    for (bg = 0; bg < BG_ACTOR_MAX; bg++) {
        BgActor* bgActor = &play->colCtx.dyna.bgActors[bg];

        if (!(play->colCtx.dyna.bgActorFlags[bg] & 1) || (bgActor->actor != actor) || (bgActor->colHeader == NULL)) {
            continue;
        }
        return (f32)bgActor->colHeader->minBounds.y * actor->scale.y;
    }
    // No collision of its own: the cylinder is measured from the actor's feet, so its underside is
    // the origin.
    return 0.0f;
}

/**
 * How long until this body falls to `targetY`, in frames. Returns -1 if it never gets there.
 *
 * The body is on a plain ballistic arc: y(t) = y0 + vy*t + 0.5*g*t^2. Setting that equal to the
 * target height gives 0.5*g*t^2 + vy*t + dy = 0, and the root that lies in the future is the answer.
 * With gravity negative and the target below, the discriminant is always positive.
 */
static f32 SwitchMagnet_TimeToFall(Actor* actor, f32 targetY) {
    f32 dy = actor->world.pos.y - targetY;
    f32 disc = (actor->velocity.y * actor->velocity.y) - (2.0f * actor->gravity * dy);
    f32 t;

    if (disc <= 0.0f) {
        return -1.0f;
    }
    t = (-actor->velocity.y - sqrtf(disc)) / actor->gravity;
    return (t > 0.0f) ? t : -1.0f;
}

/**
 * Steer `actor` toward a switch it could press. Call once per frame while it falls.
 * @return non-zero if it is currently flying at one.
 *
 * The whole judgement is one question, asked of the arc rather than of the aim: FLY THIS PARABOLA
 * OUT and see where it crosses the switch's height. How far that lands from the switch is the miss,
 * and the miss is what decides whether this shot deserves help.
 *
 * That is strictly better than testing heading and speed separately, which is what this did first.
 * A body pointed straight at a switch but travelling twice too fast sails over it, and the old cone
 * test called that a good shot; a body drifting in sideways from a lazy drop was often outside the
 * cone and got no help at all, even though it was about to land right beside the thing.
 */
s32 SwitchMagnet_Steer(PlayState* play, Actor* actor) {
    Actor* best = NULL;
    f32 bestMiss = SWITCHMAGNET_ASSIST_MISS;
    f32 bestTime = 0.0f;
    f32 bestDist = 0.0f;
    s16 bestYaw = 0;
    Actor* it;
    f32 needSpeed;

    if ((play == NULL) || (actor == NULL) || !SwitchMagnet_IsPresser(actor)) {
        return 0;
    }
    // On the way DOWN only. On the way up the shot is still being thrown, and steering it then
    // would take the throw away from the player.
    if ((actor->velocity.y >= 0.0f) || (actor->gravity >= 0.0f)) {
        return 0;
    }

    for (it = play->actorCtx.actorLists[ACTORCAT_SWITCH].head; it != NULL; it = it->next) {
        f32 dx;
        f32 dz;
        f32 dist;
        f32 t;
        f32 travel;
        f32 landX;
        f32 landZ;
        f32 miss;

        if (!SwitchMagnet_IsPressable(it)) {
            continue;
        }
        // It has to be below us, or there is no fall that reaches it.
        if (it->world.pos.y >= actor->world.pos.y) {
            continue;
        }
        dx = it->world.pos.x - actor->world.pos.x;
        dz = it->world.pos.z - actor->world.pos.z;
        dist = sqrtf((dx * dx) + (dz * dz));
        if (dist > SWITCHMAGNET_RANGE) {
            continue;
        }

        t = SwitchMagnet_TimeToFall(actor, it->world.pos.y);
        if (t < SWITCHMAGNET_MIN_TIME) {
            continue; // already too late to correct anything without it looking like a magnet
        }

        // Where the arc actually puts it, left alone.
        travel = actor->speedXZ * t;
        landX = actor->world.pos.x + (Math_SinS(actor->world.rot.y) * travel);
        landZ = actor->world.pos.z + (Math_CosS(actor->world.rot.y) * travel);
        miss = sqrtf(((landX - it->world.pos.x) * (landX - it->world.pos.x)) +
                     ((landZ - it->world.pos.z) * (landZ - it->world.pos.z)));

        // The switch this throw came CLOSEST to, not the nearest one — a shot sailing past a switch
        // at its feet toward one across the room was aimed at the far one.
        if (miss < bestMiss) {
            bestMiss = miss;
            bestTime = t;
            bestDist = dist;
            bestYaw = (s16)(Math_FAtan2F(dx, dz) * (0x8000 / M_PI));
            best = it;
        }
    }
    if (best == NULL) {
        return 0;
    }

    // The heading and speed that put it ON the switch exactly as it arrives. Out of reach is left
    // out of reach — this corrects aim, it does not add range the throw never had.
    needSpeed = bestDist / bestTime;
    if (needSpeed > SWITCHMAGNET_MAX_SPEED) {
        return 0;
    }

    if (bestMiss <= SWITCHMAGNET_LOCK_MISS) {
        // Near miss: fly the solved arc outright. Re-solved every frame, so it stays true through
        // the whole descent instead of drifting off a solution computed once at the top — and
        // because the values it writes are the ones it will read back next frame, the lock holds
        // itself without any state to keep.
        actor->world.rot.y = bestYaw;
        actor->speedXZ = needSpeed;
        return 1;
    }

    // Wide, but not hopeless: pull it in. If it converges it crosses into the lock band by itself.
    Math_SmoothStepToS(&actor->world.rot.y, bestYaw, 3, SWITCHMAGNET_TURN, 1);
    Math_StepToF(&actor->speedXZ, needSpeed, SWITCHMAGNET_SPEED_STEP);
    return 1;
}

/**
 * The final approach: drop `actor` squarely onto a switch it is already passing over.
 *
 * SwitchMagnet_Steer shapes the arc from far away and is happy to land near the plate.
 * This is the last step, and it is exact — once the body is inside the column above a switch it
 * stops travelling, squares up with the plate, and comes straight down onto it.
 *
 * The order is deliberate and is the order it reads in: X and Z FIRST, so it is over the plate,
 * and only then Y, so all the motion it has left is the fall. Doing it the other way would drop it
 * beside the switch and then slide it across, which looks like the object being dragged.
 *
 * `alignToSwitch` squares the body up with the plate. Right for a statue or a boulder, wrong for
 * anything the player lined up by hand — a block spun to match a switch no longer fits the slot it
 * was aimed at — so the caller decides.
 *
 * @return non-zero if it committed to a switch.
 */
s32 SwitchMagnet_SnapOnto(PlayState* play, Actor* actor, u8 alignToSwitch) {
    Actor* it;

    if ((play == NULL) || (actor == NULL) || !SwitchMagnet_IsPresser(actor)) {
        return 0;
    }

    for (it = play->actorCtx.actorLists[ACTORCAT_SWITCH].head; it != NULL; it = it->next) {
        f32 dx;
        f32 dy;
        f32 dz;

        if (!SwitchMagnet_IsPressable(it)) {
            continue;
        }
        // Straight down, and not too far down: three Link-heights of column above the plate.
        dy = actor->world.pos.y - it->world.pos.y;
        if ((dy < 0.0f) || (dy > SWITCHMAGNET_SNAP_HEIGHT)) {
            continue;
        }
        dx = actor->world.pos.x - it->world.pos.x;
        dz = actor->world.pos.z - it->world.pos.z;
        if ((fabsf(dx) > SWITCHMAGNET_SNAP_RADIUS) || (fabsf(dz) > SWITCHMAGNET_SNAP_RADIUS)) {
            continue;
        }
        // The downward ray, for free: the caller has already run the body's bg check this frame, so
        // `floorHeight` IS what is directly underneath. If that is well above the switch then
        // something solid is in between and the plate is not really below us at all.
        if (actor->floorHeight > (it->world.pos.y + SWITCHMAGNET_SNAP_CLEARANCE)) {
            continue;
        }

        // X and Z first — put it over the plate, and bring prevPos along so the next bg check does
        // not sweep the gap and snag it on the switch's own edge.
        actor->world.pos.x = it->world.pos.x;
        actor->world.pos.z = it->world.pos.z;
        actor->prevPos.x = actor->world.pos.x;
        actor->prevPos.z = actor->world.pos.z;

        // Square up with the switch so it settles flush on it rather than cocked across a corner.
        if (alignToSwitch) {
            actor->shape.rot.y = it->shape.rot.y;
        }
        actor->world.rot.y = it->shape.rot.y;

        // Then Y — and SET it rather than leaving it to fall. Waiting for gravity is what made this
        // read as vague next to Ultrahand, which puts the body down on the plate the moment it
        // commits. The body's underside goes on the plate's top face; anything else is resting
        // inside the switch or hovering over it, and neither one presses anything.
        actor->speedXZ = 0.0f;
        actor->velocity.x = 0.0f;
        actor->velocity.z = 0.0f;
        actor->velocity.y = 0.0f;
        {
            f32 plateTop;

            if (SwitchMagnet_DynaTopY(play, it, &plateTop)) {
                actor->world.pos.y = plateTop - SwitchMagnet_BodyBottom(play, actor);
                actor->prevPos.y = actor->world.pos.y;
                actor->bgCheckFlags |= BGCHECKFLAG_GROUND;
            }
        }
        return 1;
    }
    return 0;
}

/**
 * Press whatever the body is standing on, the way the body would press it itself.
 *
 * This is vanilla's OWN mechanism, borrowed rather than reinvented. A pushable block does not press
 * floor switches through ACTOR_FLAG_CAN_PRESS_SWITCHES — it does not carry that flag at all. It
 * calls DynaPolyActor_SetActorOnTop and DynaPolyActor_SetSwitchPressed by hand, every frame, on
 * whatever dynapoly it is resting on (z_obj_oshihiki.c:512-513). And it finds that dynapoly with a
 * FIVE-POINT probe — the four corners of its footprint plus the middle, sColCheckPoints — which is
 * how a block that only half overlaps a plate still counts as standing on it.
 *
 * All of which lives in the block's UPDATE. Stasis replaces that update with a no-op, so the moment
 * a block is frozen it stops pressing anything, and a block moved onto a switch by the rune sat
 * there doing nothing. Ultrahand never hit this because it hands the block back before it matters:
 * its whole job is to leave the body genuinely resting on the plate's collision surface and then
 * let vanilla take over. Stasis holds on for ten seconds, so it has to do the press itself.
 *
 * `halfWidth` is the body's own footprint and `bottomY` how far its underside sits below its origin.
 */
static void SwitchMagnet_Hold(Actor* body, Actor* sw);

void SwitchMagnet_PressUnder(PlayState* play, Actor* actor, f32 halfWidth, f32 bottomY) {
    // Corners first, centre last — same shape as sColCheckPoints, and pulled in slightly so a probe
    // at the very lip of the footprint does not reach past the plate it is meant to be testing.
    static const f32 sProbeX[5] = { 0.9f, -0.9f, -0.9f, 0.9f, 0.0f };
    static const f32 sProbeZ[5] = { -0.9f, -0.9f, 0.9f, 0.9f, 0.0f };
    f32 soleY;
    s32 i;

    if ((play == NULL) || (actor == NULL)) {
        return;
    }
    soleY = actor->world.pos.y + bottomY;

    for (i = 0; i < 5; i++) {
        Vec3f probe;
        CollisionPoly* poly;
        s32 bgId;
        DynaPolyActor* dyna;
        f32 floorY;

        probe.x = actor->world.pos.x + (sProbeX[i] * halfWidth);
        // Cast from just ABOVE the sole. Starting at or below it would begin the ray inside the
        // very plate we are looking for, miss its top face, and report the floor underneath.
        probe.y = soleY + 10.0f;
        probe.z = actor->world.pos.z + (sProbeZ[i] * halfWidth);

        floorY = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &poly, &bgId, actor, &probe);
        if (floorY <= BGCHECK_Y_MIN) {
            continue;
        }
        // Standing ON it, not hovering above it.
        if ((soleY - floorY) > SWITCHMAGNET_SNAP_CLEARANCE) {
            continue;
        }
        dyna = DynaPoly_GetActor(&play->colCtx, bgId);
        if (dyna == NULL) {
            continue; // plain scene collision
        }
        DynaPolyActor_SetActorOnTop(dyna);
        DynaPolyActor_SetSwitchPressed(dyna);
        // And take out a lease, so the plate stays down once we hand the body back. Its own logic
        // will not keep the press alive: a pushable block only re-asserts it from
        // ObjOshihiki_OnActor, and one that was standing on the room floor when it was frozen
        // resumes in ObjOshihiki_OnScene — a state that never looks down again.
        if (SwitchMagnet_IsPressable(&dyna->actor)) {
            SwitchMagnet_Hold(actor, &dyna->actor);
        }
    }
}

// ── Keeping a switch down after we let go ────────────────────────────────────
//
// The press has to be RE-ASSERTED, not latched: the engine wipes interactFlags every frame
// (DynaPolyActor_UnsetAllInteractFlags), so a switch is pressed only for as long as something keeps
// saying so. Normally the body itself keeps saying so — but a pushable block only does that from
// its ObjOshihiki_OnActor state, and a block that was standing on the room's own floor when it got
// frozen resumes in ObjOshihiki_OnScene, which never looks down again. So the block is handed back
// sitting squarely on a plate and quietly stops pressing it, and six frames later (the switch's own
// releaseTimer) it pops back up.
//
// Hence a lease, held here and ticked from CustomItems_Update — which keeps running long after the
// rune or the cane is put away, and has to, since the whole point is a switch that stays down while
// you walk off and use the door. Ultrahand solved this the same way; this is the shared version,
// with room for several bodies because a cane can place six statues on six plates.
#define SWITCHMAGNET_LEASES 8
#define SWITCHMAGNET_HOLD_RANGE 40.0f

typedef struct {
    Actor* body;
    Actor* sw;
} SwitchMagnetLease;

static SwitchMagnetLease sLeases[SWITCHMAGNET_LEASES] = { { 0 } };

static void SwitchMagnet_Hold(Actor* body, Actor* sw) {
    s32 i;
    s32 free = -1;

    for (i = 0; i < SWITCHMAGNET_LEASES; i++) {
        if ((sLeases[i].body == body) && (sLeases[i].sw == sw)) {
            return; // already held
        }
        if ((free < 0) && (sLeases[i].body == NULL)) {
            free = i;
        }
    }
    if (free >= 0) {
        sLeases[free].body = body;
        sLeases[free].sw = sw;
    }
}

/**
 * Re-assert every held press. Call once per frame, unconditionally — it is a cheap no-op when
 * nothing is held, and it must NOT be tied to any item being equipped.
 */
void SwitchMagnet_PressTick(PlayState* play) {
    s32 i;

    if (play == NULL) {
        return;
    }
    for (i = 0; i < SWITCHMAGNET_LEASES; i++) {
        Actor* body = sLeases[i].body;
        Actor* sw = sLeases[i].sw;
        DynaPolyActor* dyna;
        f32 dx;
        f32 dz;
        f32 plateTop;

        if ((body == NULL) || (sw == NULL)) {
            continue;
        }
        // Either one dying drops the lease. Checked before anything is read through them.
        if ((body->update == NULL) || (sw->update == NULL)) {
            sLeases[i].body = sLeases[i].sw = NULL;
            continue;
        }
        // Still on it? XZ against the switch, Y against the plate's own top — lifting the body off
        // has to release the switch, and so does sliding it away.
        dx = body->world.pos.x - sw->world.pos.x;
        dz = body->world.pos.z - sw->world.pos.z;
        if (((dx * dx) + (dz * dz)) > (SWITCHMAGNET_HOLD_RANGE * SWITCHMAGNET_HOLD_RANGE)) {
            sLeases[i].body = sLeases[i].sw = NULL;
            continue;
        }
        if (SwitchMagnet_DynaTopY(play, sw, &plateTop)) {
            f32 dy = (body->world.pos.y + SwitchMagnet_BodyBottom(play, body)) - plateTop;

            if ((dy > SWITCHMAGNET_HOLD_RANGE) || (dy < -SWITCHMAGNET_HOLD_RANGE)) {
                sLeases[i].body = sLeases[i].sw = NULL;
                continue;
            }
        }
        dyna = DynaPoly_GetActor(&play->colCtx, ((DynaPolyActor*)sw)->bgId);
        if ((dyna == NULL) || (&dyna->actor != sw)) {
            sLeases[i].body = sLeases[i].sw = NULL;
            continue;
        }
        DynaPolyActor_SetActorOnTop(dyna);
        DynaPolyActor_SetSwitchPressed(dyna);
    }
}
