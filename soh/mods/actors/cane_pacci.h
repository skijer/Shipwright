/**
 * cane_pacci.h — Pacci side of the Dual Cane (Skijer's NEI).
 *
 *   FLIP      Launches the aimed enemy straight up, rolls it onto its back and
 *             drops it. Two vanilla behaviours ported wholesale, without touching
 *             either actor: the flip itself is EnTite_SetupFlipOnBack /
 *             EnTite_FlipOnBack / EnTite_FlipUpright (a hammered Tektite), and the
 *             fall is ObjTsubo_Thrown (a thrown pot) — tumbling, with a live AT+OC
 *             collider the whole way down, so whatever it lands on breaks exactly
 *             as if a pot had hit it. It lies paralysed for PACCI_FLIP_ON_BACK_TIMER
 *             frames — no AI at all, so a Deku Scrub cannot duck away and an
 *             Octorok cannot submerge — then hops back upright and resumes.
 *             The effect OUTLIVES the cane: unequipping does not undo it, because
 *             the point is to flip an enemy and then hit it with a weapon.
 *
 *   STONE     Drains the enemy's colour to grey, deletes its AI outright, and
 *             gives it En_Ishi's behaviour: liftable, throwable, and it shatters
 *             into rock debris + dust with NA_SE_EV_ROCK_BROKEN and a random
 *             collectible drop. The enemy is gone for good once it breaks.
 *
 *   ULTRAHAND Grabs whatever you are aiming at and holds it out in front of you,
 *             and welds pieces together into one solid. C enters the mode; from
 *             there A grabs, welds the offered joint, or drops, and only B leaves.
 *
 *               D-pad alone            push / pull
 *               L + D-pad              rotate (left/right = yaw, up/down = pitch)
 *               R + D-pad              raise / lower and slide left / right
 *               Z                      reset orientation to the grab pose
 *               shake the stick L-R    detach
 *
 *             What is drawn while holding, and nothing else: the actor's own colour
 *             filter as the tint, ONE thin tether pass from Link's hand, a real point
 *             light on the object (Pacci_UhLightAt - the only thing here that makes the
 *             model itself green rather than painting over it), and a Maya-style gizmo
 *             of SOLID arrows and rings that appears only while the control it
 *             describes is held. Link keeps his right arm out toward the object.
 *
 *             No shells, no wireframe boxes, no swept tubes. Everything box-shaped drawn
 *             over an actor was a cube scaled to a bounding box, and almost nothing in
 *             this game is a cube - on a gravestone it read as a green slab floating
 *             through the scenery.
 *
 * Only ACTORCAT_ENEMY actors can be flipped or petrified, and never bosses (see
 * Pacci_IsValidEnemy). Ultrahand is deliberately broader: it takes anything the
 * shared target selector considers a loose object.
 *
 * Consumed via #include from item_cane_of_somaria.c (the .c is NOT in the vcxproj).
 */

#ifndef CANE_PACCI_H
#define CANE_PACCI_H

#include "z64.h"

// ── Flip ─────────────────────────────────────────────────────────────────────
// The launch/rotation/righting constants live next to the code that uses them in
// cane_pacci.c, because they are En_Tite's own values and belong beside the note
// explaining which function each one came from. Only the two knobs worth tuning
// from outside are here.
//
// How long the enemy lies paralysed on its back once it lands — no AI at all, so a
// Deku Scrub cannot duck back into its flower and an Octorok cannot submerge.
// En_Tite's own vOnBackTimer is 500, which at the actors' update rate is about
// twenty-five seconds: far too long for a move the player fires at will. 100 is
// roughly five seconds, which is the "unos segundos" this is meant to be. Turn it
// up toward 500 for vanilla-Tektite behaviour.
#define PACCI_FLIP_ON_BACK_TIMER 100
#define PACCI_FLIP_MIN_VEL_Y -22.0f

// ── Stone ────────────────────────────────────────────────────────────────────
#define PACCI_STONE_GRAVITY -1.7f
#define PACCI_STONE_MIN_VEL_Y -22.0f
#define PACCI_STONE_THROW_SPEED 7.0f
#define PACCI_STONE_THROW_VEL_Y 5.0f
#define PACCI_STONE_BREAK_SPEED 3.0f // impact speed that shatters it

// ── Ultrahand ────────────────────────────────────────────────────────────────
// The REACHABLE sweep: how far, and how wide a cone in front of Link. Deliberately short - this
// route exists for things at your feet that the aim line cannot reach, not as a second grab range.
#define PACCI_UH_REACH_RANGE 180.0f
#define PACCI_UH_REACH_CONE 0x2000 // 45 degrees either side
#define PACCI_UH_DIST_MIN 150.0f
#define PACCI_UH_DIST_MAX 500.0f
#define PACCI_UH_DIST_STEP 25.0f
#define PACCI_UH_ROT_STEP 1000
#define PACCI_UH_FOLLOW_WEIGHT 0.80f // weight of the OLD position (PR-faithful)
// Fastest the carried object may swing around Link, per frame. Roughly 5.6 degrees: quick
// enough to keep up with normal turning, slow enough that a whip-around becomes a visible arc
// rather than a jump across his body.
#define PACCI_UH_TURN_RATE 0x0400
// A dropped object falls like a thrown one, not like a feather. -0.5 was drift: a crate
// released over a ledge took most of a minute to reach the bottom, which read as a bug.
// These are En_Ishi's own numbers, which is the closest vanilla analogue - a heavy prop
// you let go of.
#define PACCI_UH_DROP_GRAVITY -3.2f
#define PACCI_UH_DROP_MIN_VEL_Y -36.0f
// Horizontal decay per frame during the fall, so a release with sideways carry momentum
// arcs and settles instead of sailing off in a straight line forever.
#define PACCI_UH_DROP_DRAG 0.94f
// Ceiling on the inertia a release can inherit from the carry. Whipping the aim around
// builds a large per-frame delta, and without this the drop turned into a catapult.
#define PACCI_UH_THROW_MAX 16.0f
// How far below the body the landing probe looks. Only a watchdog for the case where the
// structure is over a bottomless pit; PACCI_UH_ABANDON_DROP is what actually gives up.
#define PACCI_UH_GROUND_PROBE 600.0f
// NOT a cut-off for the fall any more. It used to be 80, and it ended the drop mid-air on
// anything released from a height, which is what left objects hanging. It is now purely a
// runaway guard for a fall that reports neither ground nor abandonment.
#define PACCI_UH_DROP_TIMEOUT 400
#define PACCI_UH_ABANDON_DROP 250.0f // it fell this far below Link -> forget it

// -- Stored geometry ----------------------------------------------------------
// L + R + A eats the structure you are holding and keeps it; holding the cane's C button
// spends magic to build it again wherever you are standing. One slot.
#define PACCI_UH_SUMMON_COST 48
#define PACCI_UH_SUMMON_HOLD 20 // frames of held C before the recall fires
// Height above Link's feet the rebuilt structure appears at, so it does not
// materialise inside the floor before the carry has taken it.
#define PACCI_UH_SUMMON_RISE 40.0f
// How fast the placement magnet takes the body over, per frame. Low enough to read as a slide you
// could still interrupt, high enough that it is settled before you think to press anything.
#define PACCI_PLACE_PULL 0.12f
// How far into the plate the body is set. See the note in Pacci_PlaceUpdatePreview: resting
// exactly ON a surface is a coin flip to the engine's standing test, and this settles it.
#define PACCI_PLACE_SINK 1.5f
// Blend at which the slide counts as finished and the switch starts being held.
#define PACCI_PLACE_ARRIVED 0.9f
// How far the body may drift from the plate, in any axis, before it stops counting as on it.
#define PACCI_PRESS_HOLD 40.0f
// Where a passenger sits relative to Link's TORSO body part: behind him along his facing, and a
// little up, which is where the shield rides.
#define PACCI_BACKRIDE_BEHIND 9.0f
#define PACCI_BACKRIDE_RISE 6.0f
// How far a SETS_FLAG body has to be carried from where it was grabbed before the room counts it.
// Roughly half the Rising Stone Platform's own middle height, so lifting one is unmistakably a
// lift and not a wobble.
#define PACCI_UH_FLAG_TRAVEL 70.0f
// How far open a LOCKED body swings when its flag is set. Bg_Dodoago's own open pose, and it is the
// only LOCKED actor there is - if a second one turns up with a different angle, this becomes a
// column in the trait table rather than a constant.
#define PACCI_UH_LOCKED_OPEN_X 0x1333
// The cut. Damage 2 is one boomerang hit, and En_Ba has four points of health, so severing a
// tentacle takes four goes exactly as it does with the real thing.
#define PACCI_UH_CUT_DAMAGE 2
#define PACCI_UH_CUT_RADIUS 30.0f
#define PACCI_UH_CUT_HEIGHT 80.0f
#define PACCI_UH_CUT_FRAMES 6 // an AT and an AC only meet inside one frame's lists
// How much one D-pad press grows or shrinks a HEIGHT body, and how tall it may get.
#define PACCI_UH_HEIGHT_STEP 40.0f
#define PACCI_UH_HEIGHT_MAX 600.0f
// Frames the thrown body is held as "lifted" before its parent is cleared. Long enough that its
// own update has certainly seen the parent, whatever order the categories happen to run in.
#define PACCI_UH_THROW_HOLD 4
// Hauling: how far one frame of held D-pad drags a body along the ground.
#define PACCI_UH_PULL_RATE 1.6f
// Throwing. REACH and RISE put the body just past Link's throwing hand while he winds up; SPEED
// and LIFT are the throw itself, a flat hard toss rather than a lob.
#define PACCI_UH_THROW_REACH 18.0f
#define PACCI_UH_THROW_RISE 6.0f
#define PACCI_UH_THROW_SPEED 14.0f
#define PACCI_UH_THROW_LIFT 3.0f
// Horizontal half-extent past which a dynapoly body is a piece of the room rather than an object.
// A crate is ~30, a gravestone ~40, a pushblock ~60, a lift platform under 200; a Forest Temple
// room quadrant is thousands. Only applies to actors the trait table says nothing about.
#define PACCI_UH_MAX_HALF 300.0f
// Frames between the ordinary flames a carried blue fire sheds. Each lives 44 frames.
#define PACCI_UH_ICE_PERIOD 16

// How many enemies may be flipped / petrified at once.
#define PACCI_MAX_AFFECTED 8

// ── API ──────────────────────────────────────────────────────────────────────

/** May this actor be flipped or petrified? (enemy, alive, not a boss/miniboss) */
u8 Pacci_IsValidEnemy(Actor* actor);

/** Cast Flip on the aimed enemy. Returns 1 if something was flipped. */
u8 Pacci_CastFlip(PlayState* play, Player* player);

/** Cast Stone on the aimed enemy. Returns 1 if something was petrified. */
u8 Pacci_CastStone(PlayState* play, Player* player);

/**
 * Ultrahand: grab the aimed actor, or release the one already held.
 * Returns 1 when the grab/release happened.
 */
u8 Pacci_CastUltrahand(PlayState* play, Player* player);

/** Per-frame Ultrahand carry/drop physics. Called while the cane is equipped. */
void Pacci_UpdateUltrahand(PlayState* play, Player* player);

/**
 * Advance a fall in progress. Must be called EVERY frame from somewhere that runs whether or
 * not the cane is in hand (CustomItems_Update), because letting go by unequipping is letting
 * go: the object still has to reach the ground.
 */
void Pacci_UltrahandDropTick(PlayState* play);

/** Highlight the actor Ultrahand would grab (live aim feedback). */
void Pacci_HighlightUltrahandTarget(PlayState* play);

/** Green held-object treatment, energy stream and world-space control indicators. */
void Pacci_UltrahandDrawVfx(PlayState* play, Player* player);

/**
 * Tint the enemy Flip / Stone would hit, in that skill's colour. Re-apply every
 * frame. Pass stone=1 for Stone's tint, 0 for Flip's.
 */
void Pacci_HighlightEnemyTarget(PlayState* play, u8 stone);

/** Is Ultrahand currently holding something? (drives the HUD hint) */
u8 Pacci_IsHoldingUltrahand(void);

/** Is Ultrahand holding this exact actor? Used by cane_ship.cpp to gate the ferry's speed ramp. */

/**
 * ARMED = Ultrahand is the cane's selected skill. Distinct from Pacci_UltrahandModeActive():
 * armed is the resting state that extends Link's arm, marks what the aim would take and offers
 * welds; the mode is only the D-pad control layer C opens on top of it. The cane sets this
 * every frame it is in hand.
 */
void Pacci_SetUltrahandArmed(u8 armed);
u8 Pacci_UltrahandArmed(void);

// ── Ultrahand mode ───────────────────────────────────────────────────────────
// A dedicated input mode entered with C. While it is active it owns A, B and the
// D-pad, so none of those mean what they normally would.
u8 Pacci_UltrahandModeActive(void);
void Pacci_UltrahandModeEnter(PlayState* play, Player* player);
void Pacci_UltrahandModeExit(PlayState* play);
/** One frame of the mode. Returns 1 while it owns the input. */
u8 Pacci_UltrahandModeUpdate(PlayState* play, Player* player);

// -- Fusion -------------------------------------------------------------------
// Pieces glued to the held object form an assembly that moves as one solid: the
// held object is the root, everything else is stored as an offset in its local
// frame and rebuilt from it each frame.
//
// The COLLISION really does merge: Pacci_FuseMergeCollision builds a CollisionHeader
// at runtime and registers it on the ROOT, so the assembly is one surface rather than
// several colliders travelling together. Pieces that own no header of their own get a
// box synthesised from their collision cylinder.
//
// Weld points are the 8 corners and 12 edge midpoints of a box fitted to the actor's
// real collision — face centres and the box centre are deliberately excluded, since
// they win the closest-pair search on overlap and weld pieces INTO each other. A piece
// with no collision at all is restricted to corners, and only onto genuine dynapoly.
void Pacci_FuseUpdatePreview(PlayState* play, Player* player);
u8 Pacci_FuseTryAttach(PlayState* play);

// -- Placing a weight on a floor switch ---------------------------------------
// A pseudo-attach: while a switch-pressing object is held near a floor switch, A snaps it onto
// the plate and lets go instead of dropping it where it floats. Nothing is fused. The weld offer
// takes priority when both are available.
void Pacci_PlaceUpdatePreview(PlayState* play);
u8 Pacci_PlaceOfferValid(void);
/**
 * Hold down the floor switch a placed body is sitting on. MUST be called every frame from
 * somewhere that keeps running with the cane put away: the engine clears interactFlags each frame,
 * so a switch stays pressed only while something keeps re-asserting it, and a block set on a
 * switch is supposed to hold it while you walk off.
 */
void Pacci_PlacePressTick(PlayState* play);

// -- Riding on Link's back ----------------------------------------------------
// Ultrahand on SITTING child Ruto sits her on Link's back instead of carrying her in front, so his
// hands are free to climb. She stays until the cane is used again or the shield goes up.
u8 Pacci_BackRiderActive(void);
void Pacci_BackRiderDrop(void);
/** Must run every frame from somewhere that survives the cane being put away. */
void Pacci_BackRiderTick(PlayState* play);
/** Keep a cut landing for a few frames after it is dealt. Every frame, like the others. */
void Pacci_CutTick(PlayState* play);
/** Finish a throw that was handed to the actor's own state machine. Every frame, like the rest. */
void Pacci_ThrowTick(PlayState* play);
/** Is a body mid-throw? cane_ship.cpp keeps Link out of the lift cutscene while it is. */
u8 Pacci_IsThrowing(void);

// -- Hauling ------------------------------------------------------------------
// Some bodies are dragged along the ground with Link braced against them instead of being carried.
u8 Pacci_PullActive(void);
void Pacci_PullStop(PlayState* play);
void Pacci_FuseHoldDetach(PlayState* play, Player* player, u8 rHeld);
/** Detach by shaking the stick left-right; stands in for TotK's right-stick wiggle. */
void Pacci_FuseWiggleDetach(PlayState* play, Player* player, Input* input);
u8 Pacci_FuseDetachPart(Actor* actor);
void Pacci_FuseDrawPreview(PlayState* play);
u8 Pacci_FusePreviewValid(void);
void Pacci_FuseFollow(PlayState* play);
void Pacci_FuseRelease(void);
void Pacci_FuseForget(void);
u8 Pacci_FuseIsPart(Actor* actor);
/** The assembly this actor belongs to, root or part; NULL if it is in none. */
Actor* Pacci_FuseRootOf(Actor* actor);
u8 Pacci_FuseCount(void);

// -- Stored geometry ----------------------------------------------------------
// What is kept is the RECIPE - actor id, params, and each piece's place in the root's
// local frame - not the merged CollisionHeader. The merged header lives in a single
// static pool that the next weld overwrites, and we own none of the display lists it
// describes, so a copy of it would be a structure that renders nothing and stops
// existing the moment you glue anything else together.
/** Eat and store the held structure. Dynapoly only. Returns 1 if it was stored. */
u8 Pacci_BlueprintSave(PlayState* play);
/** Rebuild the stored structure in front of Link and put it in his hands. Costs magic. */
u8 Pacci_BlueprintSummon(PlayState* play, Player* player);
/** Is there something in the slot? (drives the HUD hint) */
u8 Pacci_BlueprintStored(void);

/** Can this actor be picked up by Pacci's lift? (props, or enemies at low HP) */
u8 Pacci_IsLiftable(Actor* actor);
/**
 * The same test, with the category allow-list skipped when isDyna is set. Pass 1 only for an
 * actor that came out of DynaPoly_GetActor: owning a registered collision surface qualifies it
 * on its own, and dynapoly is spread across more categories than the list can name.
 */
u8 Pacci_IsLiftableEx(Actor* actor, u8 isDyna);

/** Grab what Link is aiming at. Returns 1 if something was lifted. */
u8 Pacci_LiftTryGrab(PlayState* play, Player* player);

/** Throw the held object — at Link's lock-on target if he has one. */
void Pacci_LiftThrow(PlayState* play, Player* player);

/** Per-frame hold/flight physics for the lift. */
void Pacci_LiftUpdate(PlayState* play, Player* player);

/** Is something currently held aloft (not yet thrown)? */
u8 Pacci_IsLifting(void);

/** Drop the held object without throwing it. */
void Pacci_LiftCancel(void);

/** Tint what the lift would grab. */
void Pacci_HighlightLiftTarget(PlayState* play);

/**
 * Drop whatever Ultrahand is holding. This is what unequipping the cane calls —
 * flips and petrifications deliberately SURVIVE it, so you can put the cane away
 * and go hit the enemy you just knocked over.
 */
void Pacci_DropUltrahand(void);

/**
 * Hard teardown: also restores every flipped / petrified enemy. NOT for unequip
 * (see Pacci_DropUltrahand) — this is for resetting the whole subsystem.
 */
void Pacci_ReleaseAll(PlayState* play);
/**
 * End a hold the player did not choose to end: a scene change, a hit, or a void-out. Every frame,
 * like the other ticks, because none of those three waits for the cane to be in hand.
 */
void Pacci_UhAbortTick(PlayState* play);

/** Drop pool entries whose actor died (scene unload, killed by something else). */
void Pacci_CleanupPool(void);

#endif // CANE_PACCI_H
