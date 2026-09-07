/**
 * Master Cycle Zero — the Sheikah Slate's fourth rune. Skijer's NEI.
 *
 * A rideable motorcycle, built out of three things this codebase already had:
 *
 *   THE MOUNT is Epona's. The bike is a real En_Horse, spawned and then hijacked — its update and
 *   draw are replaced, its struct is kept. That is not a shortcut, it is the only sane route: the
 *   player's entire riding stack (Player_Action_8084CC98, the mount/dismount handlers, the horse
 *   camera, the seated poses, the bow-from-the-saddle path) reads EnHorse fields by name —
 *   riderPos, animationIdx, curFrame, action, stateFlags — and re-implementing all of that inside
 *   z_player.c for a second vehicle would be a fork of the player. So the horse struct stays and
 *   the horse behaviour goes. Link sits on it exactly the way he sits on Epona, in Epona's own
 *   riding animations, driven by the bike's speed.
 *
 *   THE PHYSICS follow Mario Kart Wii's bike model. The mkw decomp in this workspace does not
 *   contain KartMove (the file is a bare include), so the drift / mini-turbo / wheelie logic
 *   below is built to MKW's published shape rather than transcribed: the mini-turbo charges
 *   faster with the stick pushed INTO the drift, releases into a fixed-length boost, and a bike
 *   trades steering for speed on the rear wheel. What IS ported from KartDynamics is the part it
 *   does have — the drag, the angular damping, the speed cap and, most of all, the one line that
 *   makes a kart a bike: `KartDynamicsBike::forceUpright() { angVel0.z = 0 }`. A bike does not
 *   tip; its lean is a pose, not a state.
 *
 *   THE MODEL is four rigid display lists (frame, lights, front wheel, rear wheel), each in its
 *   own local frame around its own pivot, exported by apps/cycle_blend_to_c.py. That is what
 *   makes the steering, the wheel spin and the wheelie pitch a matrix each — no skeleton.
 *
 * Controls (BotW's, on the N64 pad):
 *   A         accelerate                    B + stick brake, then reverse; B, stick centred, stopped: DISMOUNT
 *   stick     steer (tighter at low speed)  R (hold)  hop, then drift; release for the mini-turbo
 *   D-pad up  wheelie                       D-pad dn  end the wheelie
 *
 * All timings are in ticks at the 20 Hz the actor system runs at (R_UPDATE_RATE = 3). Where a
 * constant is MKW's, its 60 fps value is quoted next to it.
 *
 * Consumed via #include from item_sheikah_slate.c, right after stasis_rune.c. No header.
 */

#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include <math.h>
#include <string.h>
#include "soh/ActorDB.h"
#include "overlays/actors/ovl_En_Horse/z_en_horse.h"
#include "objects/object_horse/object_horse.h"
#include "objects/gameplay_keep/gameplay_keep.h" // gEffFire1DL — the En_Light flame
#include "../items/helpers/combat_helper.h"

extern u8 ResourceMgr_FileExists(const char* resName);
extern Gfx* ResourceMgr_LoadGfxByName(const char* path);
// Epona's own update/draw, to hand back if a horse we stopped tracking is ever still running ours.
void EnHorse_Update(Actor* thisx, PlayState* play);
void EnHorse_Draw(Actor* thisx, PlayState* play);

// ── Public API ───────────────────────────────────────────────────────────────
s32 MasterCycle_Cast(PlayState* play, Player* player);
void MasterCycle_Tick(PlayState* play, Player* player);
s16 MasterCycle_RideYaw(Actor* ride);
void MasterCycle_Forget(void);
u8 MasterCycle_IsRiding(void);
u8 MasterCycle_IsActor(Actor* actor);

// ── Model ────────────────────────────────────────────────────────────────────
// Emitted by apps/cycle_blend_to_c.py, in game units, wheels touching y=0 with the nose at +Z.
#define MC_DL_BODY "__OTR__objects/object_master_cycle/gMasterCycleBodyDL"
#define MC_DL_LIGHTS "__OTR__objects/object_master_cycle/gMasterCycleLigthsDL"
#define MC_DL_WHEEL_F "__OTR__objects/object_master_cycle/gMasterCycleFrontWhellDL"
#define MC_DL_WHEEL_B "__OTR__objects/object_master_cycle/gMasterCycleBackWheelDL"
// Where each wheel's hub sits in the frame's space (from the exporter's pivots, times its scale).
#define MC_HUB_F_Z 35.0f
#define MC_HUB_B_Z -39.0f
#define MC_HUB_Y 15.0f
#define MC_WHEEL_RADIUS 15.0f
#define MC_WHEELBASE (MC_HUB_F_Z - MC_HUB_B_Z)
#define MC_BODY_HALF_WIDTH 13.0f

// ── The body, MKW-shaped ─────────────────────────────────────────────────────
// MKW does not give a vehicle one round hitbox. Its BSP strings a CHAIN OF SPHERES down the body
// (BspHitbox: a position, a radius, a "walls only" flag) plus the wheels' own spheres. Read out of
// la_bike.bsp on the disc, the standard bike is five of them at z = +79, +55, -10, -60, -75 on a
// body 140 long. A bike is long and narrow, and its collision says so too.
//
// One fat cylinder is the opposite of that, and it is what felt wrong: a sweep of radius 35 on a
// body 26 wide is a circle half again wider than the bike, catching walls the model never touched.
// So the sweep is narrowed to the body's own half-width and the LENGTH is covered by probing the
// nose and the tail — the same arrangement, in the API this engine has.
#define MC_HULL_RADIUS 15.0f  // one sphere of the chain: the body's half-width, plus a little
#define MC_HULL_NOSE_Z 42.0f  // where the front sphere sits, from the origin
#define MC_HULL_TAIL_Z -40.0f // ...and the rear one
#define MC_BODY_TOP 63.0f

// Where Link sits. Player_Action_8084CC98 places him at riderPos - 27 in Y, so the seat value
// here is the seat's height plus 27. Dialled against ADULT Link and baked — child rides at the same
// world-space seat, which is the one thing to re-check if the bike is ever handed to him.
#define MC_SEAT_X 0.0f
#define MC_SEAT_Y 50.0f
#define MC_SEAT_Z -6.0f
#define MC_MODEL_SCALE 1.06f // dialled against adult Link, now the bike's own size
// The model is DRAWN this much lower than its origin. Only the drawing: the origin stays where the
// suspension and the collision want it, so nothing about how the bike behaves changes — it just
// sits down on the road the way it should.
#define MC_MODEL_Y_OFFSET -8.0f
#define MC_SHADOW_SCALE 0.248f // x actor scale (1.0): a blob a little wider than the wheelbase

// ── Physics ──────────────────────────────────────────────────────────────────
//
// READ, not remembered. The MKW decomp in this workspace has no C++ for KartMove, but it does have
// the game's own code as symbol-named PowerPC (build/RMCP01/StaticR/asm/kart/KartMove.s) and the
// disc's kartParam.bin (Race/Common.szs), and every MKW_* number below was taken from one of the
// two. Where a value is per-vehicle it is the MACH BIKE's — driftType 2, MKW's inside-drift bike.
//
//   updateVehicleSpeed   coast x0.98 (fwd) / x0.95 (rev) per frame; steering scrubs speed by
//                        baseHandling + (1-baseHandling)(1 - |turn| * speedRatio), NOT while drifting
//   get_acceleration_from_speed   piecewise-linear over speedRatio, from the accAs/accTs stats;
//                        a different pair (driftAccAs/Ts) while drifting
//   updateTurn           smoothed = raw*resp + smoothed*(1-resp); drifting: 0.4*stick + 0.6*dir
//   updateRotation       yaw += turn * handling; non-drift scaled 40%..100%..50% by speed
//                        (ramps at MKW speed 20 and 70 of base 81.89); ×0.35 raw stick in a wheelie
//   updateMtCharge       +2/frame, +3 more with the stick past ±0.4 INTO the drift; 270 = MT
//   releaseMt            boost for the vehicle's mtDuration frames (Mach Bike 37) at soft-limit
//                        ×1.25 with acceleration 3.0/frame — i.e. instantly at the boosted cap
//   Bike wheelie         +0.15 soft speed limit; 20-frame cooldown on start AND on cancel;
//                        cancelled by |stick| > 0.85 for 15 frames, or speedRatio < 0.3
//
// The MKW frame is 1/60 s and the actor tick here is 1/20 s, so per-frame rates are applied three
// times per tick (MKW_FPT). Speeds are kept in game units per tick and mapped onto MKW's scale
// through the speed RATIO, which is what all of MKW's curves are written against anyway.
#define MKW_FPT 3.0f
#define MKW_BASE_SPEED 81.89f // Mach Bike baseSpeed, kartParam.bin
#define MC_MAX_SPEED 36.0f    // that ratio 1.0, in game units per tick (user: x3 over 12)
#define MC_UNIT_PER_MKW (MC_MAX_SPEED / MKW_BASE_SPEED)

// get_acceleration_from_speed, Mach Bike: accAs / accTs and driftAccAs / driftAccTs.
#define MKW_ACC_A0 0.65056f
#define MKW_ACC_A1 0.29360f
#define MKW_ACC_A2 0.37680f
#define MKW_ACC_A3 0.04768f
#define MKW_ACC_T0 0.200f
#define MKW_ACC_T1 0.800f
#define MKW_ACC_T2 0.925f
#define MKW_DRIFT_ACC_A0 2.35f
#define MKW_DRIFT_ACC_A1 0.15f
#define MKW_DRIFT_ACC_T0 0.98f

#define MKW_COAST_FWD 0.98f         // updateVehicleSpeed, data+0x230
#define MKW_COAST_REV 0.95f         // data+0x234
#define MKW_BRAKE_ACCEL 3.0f        // data+0xec: the deceleration B applies against forward motion
#define MKW_REVERSE_LIMIT 20.0f     // reverse caps around this (speed floor + 0.5/frame past it)
#define MKW_BASE_HANDLING 0.9933f   // Mach Bike; the speed-scrub of steering
#define MKW_MANUAL_HANDLING 0.0232f // rad/frame at full stick (Mach Bike manualHandling)
#define MKW_MANUAL_DRIFT 0.0164f    // rad/frame in a drift (manualDrifting)
#define MKW_HANDLING_RESP 0.9025f   // per frame, handlingResponsiveness
#define MKW_DRIFT_RESP 0.98f        // driftingResponsiveness
// These four described MKW's yaw rate directly. The bike now steers through a front wheel instead
// (the bicycle model), which produces the same falling-off-with-speed by itself — the bars close up
// as MC_STEER_MAX_* interpolates. Kept for reference against the disassembly.
#define MKW_TURN_SPEED_LO 20.0f   // updateRotation: below this the turn ramps up from...
#define MKW_TURN_FRAC_LO 0.40f    // ...this fraction at rest to 100% at SPEED_LO...
#define MKW_TURN_SPEED_HI 70.0f   // ...then down to...
#define MKW_TURN_FRAC_HI 0.50f    // ...this at SPEED_HI and above
#define MKW_DRIFT_TURN_STICK 0.4f // updateTurn while drifting: turn = 0.4*stick + 0.6*dir
#define MKW_DRIFT_TURN_DIR 0.6f
#define MKW_WHEELIE_TURN 0.35f // updateTurn: raw stick ×0.35 in a wheelie (data+0xa0)
#define MKW_HOP_TURN 1.4f      // updateRotation: ×1.4 through the hop (data+0x9c)

#define MKW_MT_BASE 2      // updateMtCharge, data+0x3d0
#define MKW_MT_INSIDE 3    // data+0x3d8, added when the stick is past MT_STICK into the drift
#define MKW_MT_STICK 0.4f  // data+0x3dc
#define MKW_MT_MAX 270     // data+0x3d2 (bikes: no super mini-turbo)
#define MKW_MT_DURATION 37 // Mach Bike mtDuration (frames); Standard Bike 28, Spear 16
// These two are kept for the record only — see MC_MT_BONUS below for what the bike actually uses.
#define MKW_MT_SPEED_BONUS 0.25f // boost type 5 soft-limit bonus, data 0x30A4
#define MKW_BOOST_ACCEL 3.0f     // boost acceleration per frame, data 0x30BC

// MKW's +25% is a quarter of the top speed handed over in a couple of frames, and on a bike already
// running at three times Epona's it is a shove, not a mini-turbo. This is the one deliberately
// un-MKW number on the bike: a small lift you feel and then give back.
#define MC_MT_BONUS 0.08f   // what a mini-turbo is actually worth here
#define MC_BOOST_ACCEL 0.9f // ...and it eases in over the boost instead of snapping to it

#define MKW_WHEELIE_SPEED_BONUS 0.15f  // Bike getWheelieSoftSpeedLimitBonus, data 0x2F08
#define MKW_WHEELIE_COOLDOWN 20        // data 0x2F1C, set on start and on cancel
#define MKW_WHEELIE_CANCEL_STICK 0.85f // data+0xb8
#define MKW_WHEELIE_CANCEL_FRAMES 15   // data+0x100
#define MKW_WHEELIE_MIN_RATIO 0.3f     // Bike checkWheelieSpeed, data 0x2F10
#define MKW_WHEELIE_MAX_FRAMES 180     // the one number here NOT found in this asm; MKW's documented cap

// Ours: the parts MKW does not model because it has no rider to get on and off, no OoT bgcheck,
// and no reason to look pretty from a third-person camera.
#define MC_STOPPED 0.35f       // below this it is "stopped": B dismounts, pose is idle
#define MC_REVERSE_STICK 0.30f // stick deflection that turns B from "get off" into "reverse"
#define MC_GRAVITY -3.5f       // Epona's
#define MC_MIN_VEL_Y -20.0f
// ── Two wheels ───────────────────────────────────────────────────────────────
// The front wheel steers and the rear wheel drives, the way a motorcycle actually works. That is
// the kinematic bicycle model, and it is one equation:
//
//     yawRate = (speed / wheelbase) * tan(steerAngle)
//
// What falls out of it is everything that makes a bike feel like a bike rather than a box that
// spins on the spot. The turn RADIUS is wheelbase / tan(steer) — it depends on how far the bars are
// turned and on NOTHING ELSE, so the same lock traces the same circle at any speed, and speed only
// decides how fast you go round it. Stopped, tan(steer) times zero is zero: the bars turn and the
// bike does not, because a stationary bike cannot steer.
//
// And the body swings about the REAR AXLE, not its middle. That is why the back end tracks inside
// the front through a corner, and why the nose is what moves when you turn.
#define MC_STEER_MAX_LOW 0x1C00    // full lock, at a crawl (~39 degrees)
#define MC_STEER_MAX_HIGH 0x0500   // ...and what is left of it at top speed (~7)
#define MC_STEER_RATE 0x260        // how fast the bars themselves turn, per tick
#define MC_STEER_RETURN 0x1A0      // ...and how fast they centre when let go
#define MC_SLOPE_ACCEL 0.55f       // gravity along the ground: downhill gains, uphill costs
#define MC_LEAN_MAX 0x1400         // visual roll into a turn (~28 deg)
#define MC_DRIFT_LEAN 0x1900       // ...and further over in a drift (~35 deg)
#define MC_WALL_PROBE_HEIGHT 22.0f // the height the hull probes test at, off the bike's origin
#define MC_WALL_BOUNCE 0.40f       // speed kept after a wall hit
#define MC_WALL_MIN_SFX 12.0f
#define MC_TERRAIN_SLOPE_LIMIT 0.55f

// ── Suspension ───────────────────────────────────────────────────────────────
// A kart in MKW never touches the ground. Its WHEELS do, each on a spring-mass-damper, and the
// body is held up by them: KartWheelPhysics computes
//     force = -springStiffness * (maxTravel - travel) + dampingFactor * travelSpeed
// and nothing anywhere snaps the chassis onto the floor polygon.
//
// Snapping is what OoT does — func_8002E2AC assigns `world.pos.y = floorHeight` outright — and it
// is why the bike hopped along the ground: every polygon edge, every pebble, teleported the whole
// body up and the rider with it. It also made the hops inconsistent, because the same assignment
// competes with a launch that has not cleared the floor yet.
//
// So the body's height is taken away from the floor check and given to a spring across the two
// wheels. Values are mb_bike.bsp's own (Mach Bike): stiffness 0.22/0.23, damping the same, and
// 25 units of travel. Past that travel the wheel is simply off the ground and gravity has it —
// which is exactly what makes a hop leave cleanly and land once.
// mb_bike.bsp says stiffness 0.22 and damping 0.22, but those are MKW's numbers for MKW's
// integrator at 60 fps — copied straight into this one at 20 they ring: a 20-unit step overshoots
// to 28 and wobbles for a dozen ticks, which is the bouncing all over again in a new costume. So
// the MODEL is MKW's and the damping is solved for THIS integrator, at the value where the
// overshoot first reaches zero. Stiffness is raised to match, so it still settles quickly.
#define MC_SUS_STIFFNESS 0.45f // rear
#define MC_SUS_STIFFNESS_F \
    0.52f                    // front: mb_bike.bsp makes the front the firmer end, and a
                             // soft front is exactly what reads as "floaty"
#define MC_SUS_DAMPING 0.95f // critically damped here; 2*sqrt(k) is the same answer
#define MC_SUS_TRAVEL 25.0f

// ── Mini-turbo charge flames ─────────────────────────────────────────────────
// MKW puts its charge sparks at the back of the vehicle and changes their colour as it builds. Here
// they are flames off the REAR WHEEL, and the colour IS the readout: blue while it is building, red
// most of the way, purple the moment the mini-turbo is banked and waiting for you to let go of R.
//
// The flame is En_Light's — gEffFire1DL, with the same prim/env pairing its own table uses (the
// blue is that table's entry 2 and the purple its entry 13). It is drawn here rather than by
// spawning the actor: three colour changes per drift would mean spawning and killing an
// ACTORCAT_ITEMACTION actor three times a corner, and En_Light picks its colour from an index in
// its params, so there would be no way to fade between them anyway.
#define MC_MT_FLAME_COUNT 2     // one either side of the wheel
#define MC_MT_FLAME_SPREAD 6.0f // how far out from the wheel's plane
// Laid on their side ABOVE the rear tyre, blowing backwards: exhausts pushing the bike along rather
// than a fire burning under it. The flame DL grows along its own +Y, so a quarter turn about X lays
// it down the body's -Z, which is backwards - the front hub is at +Z.
#define MC_MT_PIPE_Y (MC_HUB_Y + MC_WHEEL_RADIUS + 2.0f)
#define MC_MT_PIPE_Z (MC_HUB_B_Z - 2.0f)
// ── Speed ribbons ────────────────────────────────────────────────────────────
// The same effect the sword's swing trail is made of — EFFECT_BLURE, a strip built from a pair of
// world points fed in every frame (EffectBlure_AddVertex). Bg_Haka_Sgami's scythe is the model:
// two of them, driven straight off the actor's own geometry. Here each ribbon spans one wheel, top
// to ground, so what smears out behind the bike is the wheels' own path.
// They come and go, the way a tyre only bites in patches: each wheel keeps its own on/off timer,
// and an off stretch is fed a SPACE so the ribbon breaks instead of drawing across the gap.
#define MC_TRAIL_MIN_RATIO 0.35f // below this it is not going fast enough to smear
#define MC_TRAIL_LIFE 8          // ticks a segment lives; the ribbon's length, in effect
#define MC_TRAIL_HALF_Y 5.0f     // how tall the ribbon is, up and down from the hub
#define MC_TRAIL_SIDE 6.0f       // in a wheelie: out to the edges of the rear tyre
#define MC_TRAIL_ON_TICKS 7.0f   // longest burst...
#define MC_TRAIL_OFF_TICKS 6.0f  // ...and longest gap between them

#define MC_MT_FLAME_SCALE 0.0008f // small: a jet off a pipe, not a fire
// Stage 2 used to sit at 0.99, which is why only two colours were ever visible: full charge and
// the last stage arrived together. Three even thirds, and the whole charge takes twice as long.
#define MC_MT_STAGE1 0.33f       // of a full charge: orange below this...
#define MC_MT_STAGE2 0.70f       // ...blue below this, purple once it is banked
#define MC_MT_CHARGE_SCALE 0.5f  // half MKW's rate: long enough to watch the colours climb
#define MC_DRIFT_MIN_RATIO 0.30f // no drift from a standstill

// ── The drift angle, read out of MKW rather than guessed ─────────────────────
// fn_1_6E704 is the whole model, and it is three lines: an accumulator (KartMove+0x9C) ramped
// toward ±kartStats->driftAngle at a rate proportional to manualDrifting, decayed when the stick
// asks for the other way. The angle is how far the BODY is turned out of the direction it is
// actually travelling — the slide — and it is separate from the turning itself.
//
// And the number that matters, straight out of kartParam.bin:
//
//     Standard Kart M   driftType 0 (outside)   driftAngle 45.0
//     Standard Bike M   driftType 1 (outside)   driftAngle 45.0
//     Mach Bike         driftType 2 (INSIDE)    driftAngle  0.0
//     Flame Runner      driftType 2 (INSIDE)    driftAngle  0.0
//     Bullet Bike, Spear  driftType 2           driftAngle  0.0
//
// So the 45 degrees is real — it is the OUTSIDE-drift number, karts and the heavy bikes. An
// inside-drift bike's drift angle is ZERO: it does not slide out at all, it just turns, which is
// exactly what "goes almost straight" means. The Master Cycle is an inside-drift bike, so it gets a
// small angle rather than none — enough to read as a drift, nowhere near a kart's.
#define MC_DRIFT_ANGLE 0x0A00      // ~14 degrees of slide. MKW inside bikes are 0, karts 45.
#define MC_DRIFT_ANGLE_RATE 0x0120 // ramps on at this per tick...
#define MC_DRIFT_ANGLE_DECR 0x01C0 // ...and lets go at this (MKW's driftAngleDecr is 0.80 of it)
#define MC_RIDER_DRIFT_SHARE 0.35f // how much of the slide the RIDER takes; the body takes it all
#define MC_DRIFT_TURN_SCALE 0.55f  // the drift's own turn: GENTLE. The stick shapes it, not it you.
// ── The hop ─────────────────────────────────────────────────────────────────
// R is the small hop, MKW's, the one a drift starts from. It is given its upward speed straight
// rather than solved from a height: a shove off the ground with a number the rider can feel, and
// the arc it makes is whatever gravity says.
//
// Link's pose comes free with it. Player_Action_8084CC98 picks his riding animation straight out of
// the horse's `animationIdx`, and entry 7 of that table is gPlayerAnim_link_uma_anim_jump100 — his
// own horseback jump. Setting the index is the whole job.
#define MC_HOP_VEL 10.0f

#define MC_WHEELIE_PITCH 0x1C00 // nose-up angle (~40 deg)
#define MC_WHEELIE_PITCH_STEP 0x300

// Trample. Hitting an enemy at speed hurts it; the harder the faster.
#define MC_HIT_MIN_SPEED 10.0f
#define MC_HIT_DAMAGE_LOW 1
#define MC_HIT_DAMAGE_HIGH 4

// Summon / dismiss.
#define MC_MATERIALIZE_TICKS 16
#define MC_DISMISS_TICKS 12
#define MC_SUMMON_AHEAD 90.0f
#define MC_MOUNT_RANGE 55.0f
#define MC_MOUNT_MAX_DY 30.0f
#define MC_DEEP_WATER 28.0f

// The horse actions the player-side code keys off. MOUNTED_WALK is "any mounted action that is
// not MOUNTED_IDLE": EN_HORSE_CHECK_4 is what lets A dismount, and A is the throttle here, so the
// bike is never left in an action that would let A mean two things.
#define MC_ACTION_PARKED ENHORSE_ACT_IDLE
#define MC_ACTION_RIDDEN ENHORSE_ACT_MOUNTED_WALK

typedef enum {
    MC_PHASE_NONE = 0,
    MC_PHASE_MATERIALIZE,
    MC_PHASE_ACTIVE,
    MC_PHASE_DISMISS,
} McPhase;

typedef enum {
    MC_DRIFT_NONE = 0,
    MC_DRIFT_HOP,
    MC_DRIFT_ON,
} McDrift;

typedef struct {
    Actor* actor; // the hijacked En_Horse
    u8 phase;
    s16 age;
    s16 phaseTimer;

    f32 speed; // signed, along the heading. Positive = forward.
    s16 steer; // front wheel angle, binang, + = left (yaw increases)
    s16 lean;  // visual roll, follows the same sign convention as EnHorse_TiltBody
    s16 pitch; // nose-up angle from terrain + wheelie
    s16 terrainPitch;
    s16 floorPitch; // pitch of the floor poly under the bike ALONG THE HEADING, + = uphill
    f32 wheelSpin;  // radians, both wheels
    s16 lastYaw;
    Vec3f prevPos;

    u8 drift;
    s8 driftDir; // +1 left, -1 right
    s16 mtCharge;
    s16 boostTimer;

    // The two speed ribbons, one trailing each wheel. TOTAL_EFFECT_COUNT is the engine's own "no
    // effect" marker (Player_InitCommon uses it for the sword's).
    s32 trailIdx[2];
    u8 trailOn[2];
    u8 trailWheelie; // which layout the ribbons were drawn in last tick
    s16 trailTimer[2];

    u8 wheelie;
    s16 wheelieTimer;
    s16 wheelieCooldown;
    s16 wheelieStickTimer; // frames the stick has been hard over during a wheelie
    s16 wheeliePitch;
    f32 turnSmooth; // updateTurn's smoothed stick, -1..1

    // One spring per wheel: [0] front, [1] rear. `susY` is where each wheel's end of the chassis
    // currently sits, `susVel` how fast it is moving. The body's height and pitch are read OUT of
    // these two, never the other way round.
    f32 susY[2];
    f32 susVel[2];
    u8 susReady;
    f32 wheelY[2]; // ground under the front and rear hubs, from the last probe

    // Set the moment anything throws the bike upward on purpose — a hop, a ramp. While
    // it is up, the suspension keeps its hands off; otherwise the springs simply pull the bike back
    // down inside the same tick and nothing ever leaves the ground.
    u8 launched;

    u8 wasRiding;
    u8 airborne;
    s16 hopTimer;

    s16 driftAngle; // how far the body is turned out of its own direction of travel

    ColliderCylinder hitCol;
    u8 hitColReady;

    // Which side Link approached from, kept only long enough to answer Actor_SetRideActor.
    s32 mountSide;
    u8 riderTilted; // Link's shape.rot.x/z are ours right now and must be put back on dismount
} MasterCycleState;

static MasterCycleState sMc = { 0 };
static Gfx* sMcDlBody = NULL;
static Gfx* sMcDlLights = NULL;
static Gfx* sMcDlWheelF = NULL;
static Gfx* sMcDlWheelB = NULL;
static u8 sMcDlTried = 0;

static void MasterCycle_BodyPoint(Actor* actor, f32 lx, f32 ly, f32 lz, Vec3f* out);
static void MasterCycle_TrailsInit(PlayState* play);
static void MasterCycle_TrailsFree(PlayState* play);
static void MasterCycle_Update(Actor* thisx, PlayState* play);
static void MasterCycle_Draw(Actor* thisx, PlayState* play);

// ── Small helpers ────────────────────────────────────────────────────────────

static void MasterCycle_LoadDLs(void) {
    if (sMcDlTried) {
        return;
    }
    sMcDlTried = 1;
    // Each gated on existence: ResourceMgr_LoadGfxByName on a missing path is a crash, and the
    // archive may simply predate the export.
    if (ResourceMgr_FileExists(MC_DL_BODY)) {
        sMcDlBody = ResourceMgr_LoadGfxByName(MC_DL_BODY);
    }
    if (ResourceMgr_FileExists(MC_DL_LIGHTS)) {
        sMcDlLights = ResourceMgr_LoadGfxByName(MC_DL_LIGHTS);
    }
    if (ResourceMgr_FileExists(MC_DL_WHEEL_F)) {
        sMcDlWheelF = ResourceMgr_LoadGfxByName(MC_DL_WHEEL_F);
    }
    if (ResourceMgr_FileExists(MC_DL_WHEEL_B)) {
        sMcDlWheelB = ResourceMgr_LoadGfxByName(MC_DL_WHEEL_B);
    }
}

u8 MasterCycle_IsActor(Actor* actor) {
    return (actor != NULL) && (actor == sMc.actor);
}

u8 MasterCycle_IsRiding(void) {
    Player* player;

    if ((sMc.actor == NULL) || (gPlayState == NULL)) {
        return 0;
    }
    player = GET_PLAYER(gPlayState);
    return (player->stateFlags1 & PLAYER_STATE1_ON_HORSE) && (player->rideActor == sMc.actor);
}

/** Drop the pointer without touching the actor — scene change, or the actor died under us. */
void MasterCycle_Forget(void) {
    memset(&sMc, 0, sizeof(sMc));
    sMc.actor = NULL;
    sMc.phase = MC_PHASE_NONE;
    sMc.mountSide = 0;
}

static void MasterCycle_Sfx(u16 id) {
    if (sMc.actor != NULL) {
        Audio_PlaySoundGeneral(id, &sMc.actor->projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }
}

// ── Speed ribbons ────────────────────────────────────────────────────────────

static void MasterCycle_TrailsInit(PlayState* play) {
    static const u8 sP1Start[] = { 190, 235, 255, 190 }; // at the top of the wheel
    static const u8 sP2Start[] = { 120, 200, 255, 120 }; // ...and down at the road
    static const u8 sP1End[] = { 120, 190, 255, 60 };
    static const u8 sP2End[] = { 90, 150, 255, 0 };
    EffectBlureInit1 init;
    s32 i;

    for (i = 0; i < 4; i++) {
        init.p1StartColor[i] = sP1Start[i];
        init.p2StartColor[i] = sP2Start[i];
        init.p1EndColor[i] = sP1End[i];
        init.p2EndColor[i] = sP2End[i];
    }
    init.elemDuration = MC_TRAIL_LIFE;
    init.unkFlag = false;
    init.calcMode = 2;
    init.trailType = TRAIL_TYPE_REST; // ours, not the cosmetics menu's sword trail

    for (i = 0; i < 2; i++) {
        sMc.trailIdx[i] = TOTAL_EFFECT_COUNT;
        Effect_Add(play, &sMc.trailIdx[i], EFFECT_BLURE1, 0, 0, &init);
    }
}

static void MasterCycle_TrailsFree(PlayState* play) {
    s32 i;

    for (i = 0; i < 2; i++) {
        if (sMc.trailIdx[i] != TOTAL_EFFECT_COUNT) {
            Effect_Delete(play, sMc.trailIdx[i]);
            sMc.trailIdx[i] = TOTAL_EFFECT_COUNT;
        }
    }
}

/**
 * One tick of both ribbons. A blure is fed a PAIR of points per frame and joins consecutive pairs
 * into quads, so handing it the top and the bottom of a wheel draws the strip that wheel sweeps.
 *
 * Below MC_TRAIL_MIN_RATIO it is fed a SPACE instead: that breaks the strip rather than ending it,
 * so slowing down and speeding up again starts a fresh ribbon instead of drawing a long smear
 * across the gap.
 */
static void MasterCycle_Trails(PlayState* play, Actor* actor) {
    static const f32 sHubZ[2] = { MC_HUB_F_Z, MC_HUB_B_Z };
    f32 ratio = fabsf(sMc.speed) / MC_MAX_SPEED;
    s32 i;

    (void)play;

    for (i = 0; i < 2; i++) {
        EffectBlure* blure;
        Vec3f hub;
        Vec3f top;
        Vec3f bottom;
        f32 lx;
        f32 lz;

        if (sMc.trailIdx[i] == TOTAL_EFFECT_COUNT) {
            continue;
        }
        blure = Effect_GetByIndex(sMc.trailIdx[i]);
        if (blure == NULL) {
            continue;
        }

        if (--sMc.trailTimer[i] <= 0) {
            sMc.trailOn[i] = !sMc.trailOn[i];
            sMc.trailTimer[i] =
                (s16)(2.0f + (Rand_ZeroOne() * (sMc.trailOn[i] ? MC_TRAIL_ON_TICKS : MC_TRAIL_OFF_TICKS)));
        }

        // In a wheelie only the rear tyre is on the road, so both ribbons go there, one off each
        // EDGE of it, and they run unbroken: that is one long tyre-mark rather than the snatches of
        // grip the two wheels take when the bike is flat.
        // Changing layout teleports the ribbon's mouth from one wheel to the other, and a blure
        // would happily join those two points with a quad right across the bike. Break it instead.
        if (sMc.trailWheelie != (sMc.wheelie ? 1 : 0)) {
            EffectBlure_AddSpace(blure);
            if (i == 1) {
                sMc.trailWheelie = sMc.wheelie ? 1 : 0;
            }
            continue;
        }

        if (sMc.wheelie) {
            lx = (i == 0) ? MC_TRAIL_SIDE : -MC_TRAIL_SIDE;
            lz = MC_HUB_B_Z;
        } else {
            lx = 0.0f;
            lz = sHubZ[i];
            if (!sMc.trailOn[i]) {
                EffectBlure_AddSpace(blure);
                continue;
            }
        }

        if ((ratio < MC_TRAIL_MIN_RATIO) || (sMc.phase != MC_PHASE_ACTIVE)) {
            EffectBlure_AddSpace(blure);
            continue;
        }

        // The wheel's own hub, carried through the same transform the wheel is DRAWN with — so the
        // ribbon leans, pitches and lifts with it instead of tracking a point on the ground.
        MasterCycle_BodyPoint(actor, lx, MC_HUB_Y, lz, &hub);
        hub.x += actor->world.pos.x;
        hub.y += actor->world.pos.y + MC_MODEL_Y_OFFSET;
        hub.z += actor->world.pos.z;

        bottom.x = top.x = hub.x;
        bottom.z = top.z = hub.z;
        top.y = hub.y + MC_TRAIL_HALF_Y;
        bottom.y = hub.y - MC_TRAIL_HALF_Y;

        EffectBlure_AddVertex(blure, &top, &bottom);
    }
}

// The rider's seat as an OFFSET from the bike, from the tunable values.
//
// riderPos is relative, not absolute: Player_Action_8084CC98 places Link at
// `rideActor->world.pos + riderPos` (and Player_ActionHandler_3 mounts him the same way), and
// EnHorse itself writes it as `limbPos - world.pos` (z_en_horse.c:3703). Writing a world position
// here put Link at twice the bike's distance from the origin — a green speck out in the field
// while the camera followed an empty bike.
//
// The seat is carried through the SAME transform the frame is drawn with — pitch about the rear
// hub, then roll, then yaw — so when the bike leans into a corner or lifts its nose in a wheelie,
// the saddle (and Link on it) goes with it instead of staying level over a tilted frame.
static void MasterCycle_BodyPoint(Actor* actor, f32 lx, f32 ly, f32 lz, Vec3f* out) {
    f32 x;
    f32 y;
    f32 z;
    f32 t;
    f32 sn;
    f32 cs;

    // Pitch about the rear hub (Matrix_RotateX convention: y' = y cos - z sin, z' = y sin + z cos).
    y = ly - MC_HUB_Y;
    z = lz - MC_HUB_B_Z;
    sn = Math_SinS(actor->shape.rot.x);
    cs = Math_CosS(actor->shape.rot.x);
    t = (y * cs) - (z * sn);
    z = (y * sn) + (z * cs);
    y = t + MC_HUB_Y;
    z += MC_HUB_B_Z;
    x = lx;

    // Roll (Matrix_RotateZ: x' = x cos - y sin, y' = x sin + y cos).
    sn = Math_SinS(actor->shape.rot.z);
    cs = Math_CosS(actor->shape.rot.z);
    t = (x * cs) - (y * sn);
    y = (x * sn) + (y * cs);
    x = t;

    // Yaw (Matrix_RotateY: x' = x cos + z sin, z' = -x sin + z cos).
    sn = Math_SinS(actor->shape.rot.y);
    cs = Math_CosS(actor->shape.rot.y);
    out->x = (x * cs) + (z * sn);
    out->y = y;
    out->z = (-x * sn) + (z * cs);
}

static void MasterCycle_SeatPos(Actor* actor, Vec3f* out) {
    MasterCycle_BodyPoint(actor, MC_SEAT_X, MC_SEAT_Y, MC_SEAT_Z, out);
}

/**
 * The yaw to sit the RIDER at. Not the body's.
 *
 * `shape.rot.y` is where the bike POINTS, and in a drift that is `world.rot.y + driftAngle` — the
 * body swung out of its own line of travel. Handing that straight to Link (which is what
 * Player_Action_8084CC98 does) turned him by the slide as well as by the corner, so he ended up
 * facing further round than the bike was actually going. A rider looks down the road: he takes the
 * DIRECTION OF TRAVEL, and only a share of the slide, the way you sit a bike that is stepping out.
 */
s16 MasterCycle_RideYaw(Actor* ride) {
    if ((sMc.actor == NULL) || (ride != sMc.actor)) {
        return ride->shape.rot.y; // every other mount: unchanged
    }
    return (s16)(ride->world.rot.y + (s16)(sMc.driftAngle * MC_RIDER_DRIFT_SHARE));
}

// ── Spawn / kill ─────────────────────────────────────────────────────────────

// Spawn a real En_Horse and take it over. `underLink` puts it at his feet, formed and ready — the
// cross-scene carry — instead of a few steps ahead materializing.
static Actor* MasterCycle_Spawn(PlayState* play, Player* player, u8 underLink) {
    ActorDBEntry* db = ActorDB_Retrieve(ACTOR_EN_HORSE);
    s32 savedObjectId = 0;
    u8 swappedObject = 0;
    Actor* actor;
    EnHorse* horse;
    f32 s;
    f32 c;
    Vec3f pos;
    s16 yaw = player->actor.shape.rot.y;

    // Actor_Spawn refuses to spawn an actor whose object is not in the scene's bank, and
    // OBJECT_HORSE is only ever loaded in Epona's own scenes. The bike never draws Epona's skin —
    // Skin_Init resolves the skeleton by name through the resource manager, so the bank is not
    // actually needed for anything — so the entry is pointed at gameplay_keep, which every scene
    // has, for exactly the length of the spawn.
    if ((db != NULL) && (Object_GetIndex(&play->objectCtx, OBJECT_HORSE) < 0)) {
        savedObjectId = db->objectId;
        db->objectId = OBJECT_GAMEPLAY_KEEP;
        swappedObject = 1;
    }

    // BotW puts it down a few steps ahead of Link, facing the way he faces.
    s = Math_SinS(yaw);
    c = Math_CosS(yaw);
    pos.x = player->actor.world.pos.x + (underLink ? 0.0f : (s * MC_SUMMON_AHEAD));
    pos.y = player->actor.world.pos.y;
    pos.z = player->actor.world.pos.z + (underLink ? 0.0f : (c * MC_SUMMON_AHEAD));

    // params 0: a plain ridable Epona, no cutscene, no race, no Ingo.
    actor = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_HORSE, pos.x, pos.y, pos.z, 0, yaw, 0, 0);

    if (swappedObject) {
        db->objectId = savedObjectId;
    }
    if (actor == NULL) {
        return NULL;
    }
    // EnHorse_Init may have killed itself (ranch/stable rules) — that leaves update NULL.
    if (actor->update == NULL) {
        return NULL;
    }

    horse = (EnHorse*)actor;
    actor->update = MasterCycle_Update;
    actor->draw = MasterCycle_Draw;
    actor->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    actor->room = -1;
    actor->gravity = MC_GRAVITY;
    actor->minVelocityY = MC_MIN_VEL_Y;
    actor->speedXZ = 0.0f;
    actor->velocity.x = actor->velocity.y = actor->velocity.z = 0.0f;
    // Epona's shadow is a horse-shaped blob; a plain circle reads better under a bike. The shadow
    // is drawn at actor->scale * shadowScale (ActorShadow_Draw), and Epona's 20 works because she
    // is a 0.01-scale actor. The bike is scale 1, so the same size is 0.2 — 20 here was a shadow
    // the size of a house.
    // The elongated shadow, not the circle: it is the one built for a body that is far longer than
    // it is wide, which is what a bike is and what a horse was.
    ActorShape_Init(&actor->shape, 0.0f, ActorShadow_DrawHorse, MC_SHADOW_SCALE);
    actor->shape.rot.x = actor->shape.rot.z = 0;
    actor->world.rot = actor->shape.rot;

    horse->action = MC_ACTION_PARKED;
    horse->stateFlags = 0;
    horse->animationIdx = ENHORSE_ANIM_IDLE;
    horse->curFrame = 0.0f;
    horse->playerControlled = false;
    Animation_PlayLoop(&horse->skin.skelAnime, (AnimationHeader*)gEponaIdleAnim);

    memset(&sMc, 0, sizeof(sMc));
    sMc.actor = actor;
    sMc.prevPos = actor->world.pos;
    sMc.lastYaw = actor->shape.rot.y;
    if (underLink) {
        sMc.phase = MC_PHASE_ACTIVE;
        Actor_SetScale(actor, MC_MODEL_SCALE);
    } else {
        sMc.phase = MC_PHASE_MATERIALIZE;
        sMc.phaseTimer = MC_MATERIALIZE_TICKS;
        Actor_SetScale(actor, 0.001f);
    }
    MasterCycle_SeatPos(actor, &horse->riderPos);

    MasterCycle_TrailsInit(play);
    MasterCycle_LoadDLs();
    return actor;
}

// The player-side clean-up for a rider who is about to lose the vehicle. Only the FORCED path is
// used: setting ENHORSE_FLAG_6 makes Player_Action_8084CC98 play its own dismount, so Link never
// has the ground yanked from under him mid-pose.
static void MasterCycle_ForceDismount(void) {
    EnHorse* horse = (EnHorse*)sMc.actor;

    if (horse != NULL) {
        horse->stateFlags |= ENHORSE_FLAG_6;
    }
}

static void MasterCycle_BeginDismiss(void) {
    if (sMc.actor == NULL) {
        return;
    }
    sMc.phase = MC_PHASE_DISMISS;
    sMc.phaseTimer = MC_DISMISS_TICKS;
    MasterCycle_Sfx(NA_SE_EV_TRE_BOX_APPEAR);
}

// ── Cast ─────────────────────────────────────────────────────────────────────

/**
 * The rune. Nothing summoned: summon. Something summoned: send it away — the second cast is the
 * dismiss, as it is in the game this comes from. Returns 1 if anything happened.
 */
s32 MasterCycle_Cast(PlayState* play, Player* player) {
    if ((sMc.actor != NULL) && (sMc.actor->update == NULL)) {
        MasterCycle_Forget();
    }
    if (sMc.actor != NULL) {
        if (sMc.phase == MC_PHASE_DISMISS) {
            return 0;
        }
        if (MasterCycle_IsRiding()) {
            // Cannot happen — the slate stows while ON_HORSE — but if it ever does, get him off
            // first and let the tick finish the job once he is standing.
            MasterCycle_ForceDismount();
            sMc.wasRiding = 2; // "dismiss as soon as he is off"
            return 1;
        }
        MasterCycle_BeginDismiss();
        return 1;
    }
    if (MasterCycle_Spawn(play, player, 0) == NULL) {
        return 0;
    }
    MasterCycle_Sfx(NA_SE_EV_TRE_BOX_APPEAR);
    return 1;
}

// ── Per-frame tick, from the slate (runs at the end of Player_Update) ─────────

// The ride carries across a loading zone. Set during the fade, consumed on the first tick of the
// next scene. Static rather than in sMc because sMc is wiped with the actor it describes.
static u8 sMcCarry = 0;
static f32 sMcCarrySpeed = 0.0f;

void MasterCycle_Tick(PlayState* play, Player* player) {
    static s16 sLastScene = -1;

    if (sLastScene != play->sceneNum) {
        sLastScene = play->sceneNum;
        MasterCycle_Forget();

        // Arriving on the bike. This is Horse_SetupInGameplay's job done for the bike, and done
        // for ANY scene rather than the five Epona is allowed in: spawn it under Link, formed, and
        // hand him to it with Actor_MountHorse. The player's own resume path then does the rest —
        // next Player_Update sees ON_HORSE with no parent and drops straight into the seated pose
        // (z_player.c:13640), exactly as it does for Epona after a transition.
        if (sMcCarry) {
            Actor* bike;

            sMcCarry = 0;
            bike = MasterCycle_Spawn(play, player, 1);
            if (bike != NULL) {
                Actor_MountHorse(play, player, bike);
                Actor_RequestHorseCameraSetting(play, player);
                sMc.speed = sMcCarrySpeed;
                sMc.lastYaw = bike->shape.rot.y;
            }
        }
        return;
    }
    // Cleared here, ahead of the returns below, so dismissing the bike drops the latch too —
    // otherwise an armed carry survives to the next loading zone and remounts a bike he put away.
    if (!MasterCycle_IsRiding()) {
        sMcCarry = 0;
    }

    if (sMc.actor == NULL) {
        return;
    }
    if (sMc.actor->update == NULL) {
        MasterCycle_Forget();
        return;
    }

    // Riding into a loading zone. Two things happen during the fade:
    //   - The player sets AREG(6) when he leaves a scene ON_HORSE, and Horse_SetupInGameplay reads
    //     it on the far side to spawn EPONA under him — which would turn the bike into a horse.
    //     Cleared every frame of the fade.
    //   - The ride is latched so the next scene rebuilds it under him (above). Not on a void-out:
    //     falling into a pit puts him back on foot at the respawn point, like Epona does.
    if ((play->transitionTrigger != TRANS_TRIGGER_OFF) || (play->transitionMode != TRANS_MODE_OFF)) {
        if (MasterCycle_IsRiding()) {
            AREG(6) = 0;
            if (gSaveContext.respawnFlag != -2) {
                sMcCarry = 1;
                // Some of the momentum survives; the rest is the world reloading around him.
                sMcCarrySpeed = sMc.speed * 0.6f;
            }
        }
    }

    // Deferred dismiss: he had to get off first.
    if ((sMc.wasRiding == 2) && !MasterCycle_IsRiding() && !(player->stateFlags1 & PLAYER_STATE1_ON_HORSE)) {
        sMc.wasRiding = 0;
        MasterCycle_BeginDismiss();
    }
}

// ── Physics ──────────────────────────────────────────────────────────────────

// The soft speed limit: base, plus MKW's additive bonuses. A boost and a wheelie do not stack
// multiplicatively in MKW either — the boost's bonus simply wins.
static f32 MasterCycle_TopSpeed(void) {
    f32 bonus = 0.0f;

    if (sMc.boostTimer > 0) {
        bonus = MC_MT_BONUS;
    } else if (sMc.wheelie) {
        bonus = MKW_WHEELIE_SPEED_BONUS;
    }
    return MC_MAX_SPEED * (1.0f + bonus);
}

// get_acceleration_from_speed, transcribed. Piecewise-linear over the speed ratio; the drift pair
// while drifting. Returned in MKW units per frame — the caller converts.
static f32 MasterCycle_MkwAccel(f32 ratio, u8 drifting) {
    if (ratio < 0.0f) {
        return 1.0f; // the function's own answer for a reversing body (rodata 1.0)
    }
    if (drifting) {
        if (ratio < MKW_DRIFT_ACC_T0) {
            return MKW_DRIFT_ACC_A0 + ((MKW_DRIFT_ACC_A1 - MKW_DRIFT_ACC_A0) * (ratio / MKW_DRIFT_ACC_T0));
        }
        return MKW_DRIFT_ACC_A1;
    }
    if (ratio < MKW_ACC_T0) {
        return MKW_ACC_A0 + ((MKW_ACC_A1 - MKW_ACC_A0) * (ratio / MKW_ACC_T0));
    }
    if (ratio < MKW_ACC_T1) {
        return MKW_ACC_A1 + ((MKW_ACC_A2 - MKW_ACC_A1) * ((ratio - MKW_ACC_T0) / (MKW_ACC_T1 - MKW_ACC_T0)));
    }
    if (ratio < MKW_ACC_T2) {
        return MKW_ACC_A2 + ((MKW_ACC_A3 - MKW_ACC_A2) * ((ratio - MKW_ACC_T1) / (MKW_ACC_T2 - MKW_ACC_T1)));
    }
    return MKW_ACC_A3;
}

// A per-frame multiplier applied MKW_FPT times, in one step.
static f32 MasterCycle_PerTick(f32 perFrame) {
    return perFrame * perFrame * perFrame;
}

// The pitch the ground under the wheels wants: probe under each hub and take the angle between.
static void MasterCycle_ProbeTerrain(PlayState* play, Actor* actor) {
    Vec3f pf;
    Vec3f pb;
    CollisionPoly* poly;
    s32 bgId;
    f32 yf;
    f32 yb;
    f32 s = Math_SinS(actor->shape.rot.y);
    f32 c = Math_CosS(actor->shape.rot.y);

    pf.x = actor->world.pos.x + (s * MC_HUB_F_Z);
    pf.y = actor->world.pos.y + 40.0f;
    pf.z = actor->world.pos.z + (c * MC_HUB_F_Z);
    pb.x = actor->world.pos.x + (s * MC_HUB_B_Z);
    pb.y = actor->world.pos.y + 40.0f;
    pb.z = actor->world.pos.z + (c * MC_HUB_B_Z);

    yf = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &pf);
    yb = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &pb);
    // Kept for the suspension: these two are where the WHEELS are, and the wheels are what the
    // bike stands on.
    sMc.wheelY[0] = yf;
    sMc.wheelY[1] = yb;
    if ((yf <= BGCHECK_Y_MIN) || (yb <= BGCHECK_Y_MIN)) {
        Math_SmoothStepToS(&sMc.terrainPitch, 0, 4, 0x200, 0x10);
        return;
    }
    // In the air the pose follows the flight, nose along the velocity — that is what sells a jump.
    if (sMc.airborne) {
        f32 sp = fabsf(actor->speedXZ);
        s16 want = (sp > 1.0f) ? (s16)(Math_FAtan2F(actor->velocity.y, sp) * (0x8000 / M_PI)) : 0;

        if (want > 0x2000) {
            want = 0x2000;
        }
        if (want < -0x2800) {
            want = -0x2800;
        }
        Math_SmoothStepToS(&sMc.terrainPitch, want, 4, 0x300, 0x10);
        return;
    }
    // Nose up when the front is higher. shape.rot.x positive tips the nose DOWN (Matrix_RotateX
    // convention), so the terrain pitch is negated when applied.
    {
        f32 dy = yf - yb;

        if (dy > (MC_WHEELBASE * MC_TERRAIN_SLOPE_LIMIT)) {
            dy = MC_WHEELBASE * MC_TERRAIN_SLOPE_LIMIT;
        }
        if (dy < -(MC_WHEELBASE * MC_TERRAIN_SLOPE_LIMIT)) {
            dy = -(MC_WHEELBASE * MC_TERRAIN_SLOPE_LIMIT);
        }
        // Nothing to do with the pitch here any more: on the ground it is the line between the two
        // suspension springs (MasterCycle_Suspension), which is both more accurate and not a frame
        // behind. This probe's only job now is feeding those springs their ground heights.
        (void)dy;
    }
}

// Push the body out of anything its NOSE or TAIL is inside.
//
// The bg check only ever tests one sphere at the origin, so on its own it lets a 108-unit bike bury
// either end in a wall it meets at an angle. This is the rest of MKW's chain: a probe out to each
// end, resolved along the wall's OWN normal — which is what makes a glancing hit slide along the
// wall instead of stopping the bike dead.
static void MasterCycle_ResolveHull(PlayState* play, Actor* actor) {
    static const f32 sHullZ[2] = { MC_HULL_NOSE_Z, MC_HULL_TAIL_Z };
    s32 i;

    for (i = 0; i < 2; i++) {
        Vec3f a;
        Vec3f b;
        Vec3f hit;
        CollisionPoly* poly = NULL;
        s32 bgId = BGCHECK_SCENE;
        f32 sn = Math_SinS(actor->shape.rot.y);
        f32 cs = Math_CosS(actor->shape.rot.y);
        f32 reach = sHullZ[i] + ((sHullZ[i] > 0.0f) ? MC_HULL_RADIUS : -MC_HULL_RADIUS);

        a.x = actor->world.pos.x;
        a.y = actor->world.pos.y + MC_WALL_PROBE_HEIGHT;
        a.z = actor->world.pos.z;
        b.x = a.x + (sn * reach);
        b.y = a.y;
        b.z = a.z + (cs * reach);

        if (!BgCheck_EntityLineTest1(&play->colCtx, &a, &b, &hit, &poly, true, false, false, true, &bgId)) {
            continue;
        }
        if (poly == NULL) {
            continue;
        }
        {
            // How far past the surface that end reached, measured along the wall's normal.
            f32 nx = COLPOLY_GET_NORMAL(poly->normal.x);
            f32 nz = COLPOLY_GET_NORMAL(poly->normal.z);
            f32 over = ((b.x - hit.x) * nx) + ((b.z - hit.z) * nz);

            if (over > 0.0f) {
                actor->world.pos.x += nx * over;
                actor->world.pos.z += nz * over;
                actor->wallYaw = Math_Atan2S(nz, nx);
                actor->bgCheckFlags |= BGCHECKFLAG_WALL;
            }
        }
    }
}

// Move along the heading with the scene collision, sub-stepped so a top-speed bike cannot cross a
// wall in one hop (the same lesson stasis_rune.c learned: nothing maintains prevPos for us).
static void MasterCycle_Move(PlayState* play, Actor* actor) {
    f32 dist;
    s32 substeps;
    s32 i;
    u8 hitWall = 0;

    // The Goron ball's slope model, and the reason a ramp is a JUMP: on the ground the speed is
    // split along the floor — cos(pitch) of it along the ground, sin(pitch) of it UP — from the
    // pitch of the actual floor poly along the heading (mm_player_form.cpp:7632, and the player's
    // own floorPitch derivation at z_player.c:13248). Then when the ramp ends under the wheels the
    // upward part is simply still there, and the bike leaves the lip on the arc it was already on.
    // Without this the ground check flattens velocity.y every frame and every ramp is a cliff.
    if (!sMc.airborne && (actor->floorPoly != NULL)) {
        f32 nx = COLPOLY_GET_NORMAL(actor->floorPoly->normal.x);
        f32 ny = COLPOLY_GET_NORMAL(actor->floorPoly->normal.y);
        f32 nz = COLPOLY_GET_NORMAL(actor->floorPoly->normal.z);
        f32 sn = Math_SinS(actor->world.rot.y);
        f32 cs = Math_CosS(actor->world.rot.y);

        if (ny > 0.05f) {
            // Uphill along the direction of travel: rise per unit run is -(n.d)/ny.
            s16 want = Math_Atan2S(1.0f, (-(nx * sn) - (nz * cs)) / ny);

            if (sMc.speed < 0.0f) {
                want = -want; // reversing up the same slope is going downhill
            }
            Math_SmoothStepToS(&sMc.floorPitch, want, 2, 0x600, 0x40);
        }
    } else {
        Math_SmoothStepToS(&sMc.floorPitch, 0, 3, 0x300, 0x40);
    }

    // Gravity along the ground. Downhill the bike GAINS and uphill it pays, which is what carrying
    // momentum over a slope means — the split below only redirects speed, it never creates any.
    if (!sMc.airborne && !sMc.launched) {
        sMc.speed -= Math_SinS(sMc.floorPitch) * fabsf(MC_GRAVITY) * MC_SLOPE_ACCEL;
    }

    // floorPitch is already measured along the direction of TRAVEL (see the reverse flip above),
    // so the split is the plain one: the signed speed along the ground, the magnitude up the slope.
    // "Never hug small drops". func_8002E234 keeps an actor glued to the floor for any gap under 11
    // units — which is most of a hop's first frames and the whole lip of a ramp — UNLESS it carries
    // this flag. The repo already added it for MM's Goron roll, and a bike wants exactly the same
    // deal: leave the ground the instant the ground leaves, so ledges launch instead of sticking.
    actor->bgCheckFlags |= 0x800;

    actor->speedXZ = sMc.speed * Math_CosS(sMc.floorPitch);
    // ...but ONLY when nothing is deliberately throwing the bike upward. This assignment is why the
    // hop never happened: it runs before the move, `airborne` is still false on the frame the hop
    // is fired, and on flat ground sin(floorPitch) is 0 — so the hop's velocity was overwritten
    // with zero every single time, along with the ledge hop's and the ramp launch's.
    if (!sMc.airborne && !sMc.launched && (actor->velocity.y <= 0.0f)) {
        actor->velocity.y = fabsf(sMc.speed) * Math_SinS(sMc.floorPitch);
    }
    Actor_UpdateVelocityXZGravity(actor);
    // The +4-per-tick shove that flag 8 of the bg check gives a grounded body would cancel the
    // slope's upward part; the ramp is what wants that velocity, not the ground.

    dist = sqrtf((actor->velocity.x * actor->velocity.x) + (actor->velocity.y * actor->velocity.y) +
                 (actor->velocity.z * actor->velocity.z));
    substeps = (s32)(dist / 10.0f) + 1;
    if (substeps > 8) {
        substeps = 8; // top speed plus a fall is ~50 units a tick; 8 hops keeps each under the radius
    }

    for (i = 0; i < substeps; i++) {
        actor->prevPos = actor->world.pos;
        actor->world.pos.x += actor->velocity.x / (f32)substeps;
        actor->world.pos.y += actor->velocity.y / (f32)substeps;
        actor->world.pos.z += actor->velocity.z / (f32)substeps;
        // The sweep is ONE sphere of the chain, at the body's own half-width — not the whole
        // silhouette. A circle of radius 35 on a body 26 wide is what made the collision feel
        // round: it caught walls the model never touched. The LENGTH is covered by
        // MasterCycle_ResolveHull instead, the way MKW covers a long body.
        //
        // 0x15 and not Epona's 0x1D: bit 8 makes func_8002E2AC clamp a grounded body's velocity.y
        // to -4 every tick, which would swallow the ramp launch above.
        {
            // The floor check is kept for its wall, water and floorPoly work, but its VERTICAL
            // answer is thrown away: func_8002E2AC assigns `world.pos.y = floorHeight` outright,
            // and that assignment is the hopping. Height belongs to MasterCycle_Suspension now, so
            // the snap is undone the instant it happens. A ceiling clamp is left alone.
            f32 keepY = actor->world.pos.y;

            Actor_UpdateBgCheckInfo(play, actor, 30.0f, MC_HULL_RADIUS, MC_BODY_TOP, 0x15);
            if (actor->bgCheckFlags & BGCHECKFLAG_GROUND) {
                actor->world.pos.y = keepY;
            }
        }
        MasterCycle_ResolveHull(play, actor);
        if (actor->bgCheckFlags & BGCHECKFLAG_WALL) {
            hitWall = 1;
            break;
        }
    }

    if (hitWall) {
        // Head-on takes most of the speed; a graze takes some. Cos of the angle between the
        // heading and the wall normal says which — the same test EnHorse_UpdateBgCheckInfo makes.
        f32 facing = Math_CosS(actor->wallYaw - actor->world.rot.y);

        if (facing < -0.3f) {
            if (fabsf(sMc.speed) > MC_WALL_MIN_SFX) {
                MasterCycle_Sfx(NA_SE_EV_BOMB_BOUND);
            }
            sMc.speed *= MC_WALL_BOUNCE;
            // A wall ends a wheelie and a drift; the bike is standing on both wheels again.
            sMc.wheelie = 0;
            sMc.drift = MC_DRIFT_NONE;
            sMc.mtCharge = 0;
        }
    }

    sMc.airborne = !(actor->bgCheckFlags & BGCHECKFLAG_GROUND);
}

// Everything the throttle, brake, stick, R and the D-pad do to the numbers.
static void MasterCycle_Drive(PlayState* play, Actor* actor, Player* player) {
    Input* input = &play->state.input[0];
    u16 held = input->cur.button;
    u16 pressed = input->press.button;
    f32 stickX = (f32)input->rel.stick_x / 60.0f;
    f32 stickY = (f32)input->rel.stick_y / 60.0f;
    f32 stickMag;
    f32 top = MasterCycle_TopSpeed();
    f32 ratio;
    f32 accel;
    s16 turn = 0;
    s16 wantSteer = 0;

    if (stickX > 1.0f) {
        stickX = 1.0f;
    }
    if (stickX < -1.0f) {
        stickX = -1.0f;
    }
    if (stickY > 1.0f) {
        stickY = 1.0f;
    }
    if (stickY < -1.0f) {
        stickY = -1.0f;
    }
    stickMag = sqrtf((stickX * stickX) + (stickY * stickY));
    // Stick left is negative x. Yaw increases turning LEFT (rot.y = 0x4000 faces +X, which is on
    // the left of a body facing +Z), so left stick -> positive turn.
    stickX = -stickX;

    // Talking, cutscenes and the like: coast to a stop, no input.
    if (Player_InCsMode(play) || (player->stateFlags1 & (PLAYER_STATE1_TALKING | PLAYER_STATE1_IN_CUTSCENE))) {
        held = 0;
        pressed = 0;
        stickX = 0.0f;
        stickMag = 0.0f;
    }

    // ── throttle / brake / coast: updateVehicleSpeed + get_acceleration_from_speed ──
    ratio = sMc.speed / MC_MAX_SPEED; // signed; MKW's curves are written against this
    if (ratio > 1.0f + MC_MT_BONUS) {
        ratio = 1.0f + MC_MT_BONUS;
    }
    // MKW units per frame -> game units per tick.
    accel = MasterCycle_MkwAccel(ratio, sMc.drift == MC_DRIFT_ON) * MC_UNIT_PER_MKW * MKW_FPT;

    if (sMc.boostTimer > 0) {
        // releaseMt's shape, with our own numbers: accelerate toward a soft limit raised by
        // MC_MT_BONUS. Gently enough that the boost is a lift through its whole length rather
        // than an instant jump to the cap.
        sMc.boostTimer--;
        sMc.speed += MC_BOOST_ACCEL * MC_UNIT_PER_MKW * MKW_FPT;
        if (sMc.speed > top) {
            sMc.speed = top;
        }
    } else if (held & BTN_A) {
        if (sMc.speed < 0.0f) {
            sMc.speed += MKW_BRAKE_ACCEL * MC_UNIT_PER_MKW * MKW_FPT; // throttle against reverse
        } else {
            sMc.speed += accel;
        }
    } else if (held & BTN_B) {
        // B is two things, told apart by the STICK: with the stick pushed it is the brake and
        // then reverse; with the stick centred and the bike stopped, it is "get off". That is
        // BotW's read of it, and it means a rider never reverses by accident while getting down
        // and never gets down by accident while backing out of a corner.
        if (sMc.speed > 0.0f) {
            sMc.speed -= MKW_BRAKE_ACCEL * MC_UNIT_PER_MKW * MKW_FPT;
            if (sMc.speed < 0.0f) {
                sMc.speed = 0.0f;
            }
        } else if (stickMag > MC_REVERSE_STICK) {
            if (!sMc.wheelie) {
                // Reverse builds with the same curve, at the stick's pressure.
                sMc.speed -= accel * 0.6f * stickMag;
            }
        } else if (fabsf(sMc.speed) < MC_STOPPED) {
            // Stopped, stick centred, and B: dismount. This is the ONLY dismount — see
            // MC_ACTION_RIDDEN. Held rather than pressed, so a B that arrived a frame before the
            // bike quite stopped still counts.
            MasterCycle_ForceDismount();
        } else {
            sMc.speed *= MasterCycle_PerTick(MKW_COAST_REV);
        }
    } else {
        // Coasting: x0.98 a frame forward, x0.95 backward. That is a bike that slows down when you
        // let go — MKW never rolls on for free.
        sMc.speed *= MasterCycle_PerTick((sMc.speed >= 0.0f) ? MKW_COAST_FWD : MKW_COAST_REV);
        if (fabsf(sMc.speed) < 0.05f) {
            sMc.speed = 0.0f;
        }
    }

    // Steering costs speed — but a drift does not, and neither does a wheelie. This is
    // updateVehicleSpeed's baseHandling term, and it is a large part of why drifting through a
    // corner is faster than steering through it: at full lock and full speed the Mach Bike loses
    // 0.67% a frame just for turning.
    if ((sMc.drift != MC_DRIFT_ON) && !sMc.wheelie) {
        f32 r = fabsf(ratio);
        f32 scrub = MKW_BASE_HANDLING +
                    ((1.0f - MKW_BASE_HANDLING) * (1.0f - (fabsf(sMc.turnSmooth) * ((r > 1.0f) ? 1.0f : r))));

        sMc.speed *= MasterCycle_PerTick(scrub);
    }

    // Caps. Above the soft limit (a boost that just ended, a wheelie that just dropped) the speed
    // bleeds back down rather than snapping.
    if (sMc.speed > top) {
        sMc.speed = (sMc.speed - top > 0.5f) ? (sMc.speed - 0.5f) : top;
    }
    if (sMc.speed < -(MKW_REVERSE_LIMIT * MC_UNIT_PER_MKW)) {
        sMc.speed = -(MKW_REVERSE_LIMIT * MC_UNIT_PER_MKW);
    }

    // ── wheelie: Bike_tryStartWheelie / fn_1_78040 / checkWheelieSpeed ──
    if (sMc.wheelieCooldown > 0) {
        sMc.wheelieCooldown--;
    }
    if (!sMc.wheelie) {
        if ((pressed & BTN_DUP) && (sMc.wheelieCooldown == 0) && (sMc.drift == MC_DRIFT_NONE) && !sMc.airborne &&
            (ratio >= MKW_WHEELIE_MIN_RATIO)) {
            sMc.wheelie = 1;
            sMc.wheelieTimer = 0;
            sMc.wheelieStickTimer = 0;
            sMc.wheelieCooldown = (s16)(MKW_WHEELIE_COOLDOWN / MKW_FPT); // set on START too
            MasterCycle_Sfx(NA_SE_EV_HORSE_JUMP);
        }
    } else {
        u8 cancel = 0;

        sMc.wheelieTimer++;
        // Holding the stick hard over for 15 frames drops the front wheel — the bike wants to
        // turn, and it cannot on one wheel.
        if (fabsf(stickX) > MKW_WHEELIE_CANCEL_STICK) {
            sMc.wheelieStickTimer++;
            if (sMc.wheelieStickTimer > (s16)(MKW_WHEELIE_CANCEL_FRAMES / MKW_FPT)) {
                cancel = 1;
            }
        } else {
            sMc.wheelieStickTimer = 0;
        }
        if ((pressed & BTN_DDOWN) || (ratio < MKW_WHEELIE_MIN_RATIO) || (sMc.drift != MC_DRIFT_NONE) ||
            (sMc.wheelieTimer > (s16)(MKW_WHEELIE_MAX_FRAMES / MKW_FPT))) {
            cancel = 1;
        }
        if (cancel) {
            sMc.wheelie = 0;
            sMc.wheelieCooldown = (s16)(MKW_WHEELIE_COOLDOWN / MKW_FPT);
            MasterCycle_Sfx(NA_SE_EV_HORSE_LAND);
        }
    }
    Math_SmoothStepToS(&sMc.wheeliePitch, sMc.wheelie ? MC_WHEELIE_PITCH : 0, 3, MC_WHEELIE_PITCH_STEP, 0x40);

    // ── drift / mini-turbo: hop, startManualDrift, updateMtCharge, releaseMt ──
    if (sMc.hopTimer > 0) {
        sMc.hopTimer--;
    }
    switch (sMc.drift) {
        case MC_DRIFT_NONE:
            // Rolling at all is enough to hop — MKW's own canHop tests no speed whatever, only
            // that the vehicle is down and able to drift. The DRIFT still needs real speed; that
            // gate lives on the landing, below.
            if ((pressed & BTN_R) && !sMc.airborne && (fabsf(sMc.speed) > 0.5f)) {
                // The drift itself is decided on landing, from the stick (updateHopAndSlipdrift).
                actor->velocity.y = MC_HOP_VEL;
                sMc.drift = MC_DRIFT_HOP;
                sMc.hopTimer = 6;
                sMc.wheelie = 0;
                MasterCycle_Sfx(NA_SE_EV_HORSE_JUMP);
                actor->bgCheckFlags &= ~BGCHECKFLAG_GROUND;
                sMc.launched = 1;
            }
            break;
        case MC_DRIFT_HOP:
            if (!(held & BTN_R)) {
                sMc.drift = MC_DRIFT_NONE; // let go before landing: just a hop
            } else if (!sMc.airborne && (sMc.hopTimer <= 3)) {
                // Landing ALWAYS breaks into a drift, whichever way the stick happens to be — hold
                // R through a hop and you come down sideways, and then you steer it. With the stick
                // centred it picks the way the bars were already pointing, so it never stalls
                // waiting for an input.
                sMc.drift = MC_DRIFT_ON;
                sMc.driftDir = (stickX > 0.05f) ? 1 : ((stickX < -0.05f) ? -1 : ((sMc.steer >= 0) ? 1 : -1));
                sMc.mtCharge = 0;
            }
            break;
        case MC_DRIFT_ON:
            if (!(held & BTN_R) || (ratio < MC_DRIFT_MIN_RATIO * 0.6f)) {
                // releaseMt: a charged mini-turbo becomes a boost for the vehicle's own duration.
                if (sMc.mtCharge >= MKW_MT_MAX) {
                    sMc.boostTimer = (s16)(MKW_MT_DURATION / MKW_FPT);
                    MasterCycle_Sfx(NA_SE_IT_SWORD_SWING_HARD); // the boost going off
                }
                sMc.drift = MC_DRIFT_NONE;
                sMc.mtCharge = 0;
            } else {
                // updateMtCharge, per frame: +2, and +3 more with the stick past 0.4 INTO the
                // drift. Three frames a tick.
                s32 into = (stickX * (f32)sMc.driftDir) > MKW_MT_STICK;
                s32 before = sMc.mtCharge;

                sMc.mtCharge += (s16)((MKW_MT_BASE + (into ? MKW_MT_INSIDE : 0)) * (s32)MKW_FPT * MC_MT_CHARGE_SCALE);
                if (sMc.mtCharge > MKW_MT_MAX) {
                    sMc.mtCharge = MKW_MT_MAX;
                }
                if (sMc.mtCharge < MKW_MT_MAX) {
                    // SFX_FLAG marks a CONTINUOUS sound: you play it MINUS the flag every tick and
                    // it stops of its own accord the moment you stop asking for it. Played with the
                    // flag still on, which is what this was doing, it starts and never ends.
                    MasterCycle_Sfx(NA_SE_IT_SWORD_CHARGE - SFX_FLAG);
                } else if (before < MKW_MT_MAX) {
                    MasterCycle_Sfx(NA_SE_IT_HOOKSHOT_READY); // banked: the flames turn purple
                }
            }
            break;
    }

    // ── steering: the front wheel turns the bars, the bicycle model does the rest ──
    {
        f32 raw = stickX;
        f32 resp;
        f32 maxSteer;
        f32 r = fabsf(ratio);
        f32 tanDelta;
        f32 yawRad;
        s16 wantAngle;

        if (r > 1.0f) {
            r = 1.0f;
        }
        if (sMc.wheelie) {
            raw *= MKW_WHEELIE_TURN; // on one wheel there is no front tyre to steer with
        }

        // How far the bars will go, which closes up with speed exactly as they do on a real bike —
        // full lock is a thing you have at walking pace and not at all flat out.
        maxSteer = (f32)MC_STEER_MAX_LOW + (((f32)MC_STEER_MAX_HIGH - (f32)MC_STEER_MAX_LOW) * r);

        if (sMc.drift == MC_DRIFT_ON) {
            // A drift is the bars held over: 0.4 * stick + 0.6 * direction, MKW's own blend, but
            // spent on a steering angle instead of a yaw rate — so the drift goes through the same
            // front wheel as everything else and the model stays one model.
            f32 t = (MKW_DRIFT_TURN_STICK * sMc.turnSmooth) + (MKW_DRIFT_TURN_DIR * (f32)sMc.driftDir);

            if (t > 1.0f) {
                t = 1.0f;
            }
            if (t < -1.0f) {
                t = -1.0f;
            }
            // GENTLE. A drift is something you hold and shape, not something that whips the bike
            // round: full lock into the corner is still only a little more than the bars alone.
            wantAngle = (s16)(t * maxSteer * MC_DRIFT_TURN_SCALE);
        } else if (sMc.drift == MC_DRIFT_HOP) {
            wantAngle = (s16)(((stickX > 0.2f) ? 1.0f : ((stickX < -0.2f) ? -1.0f : 0.0f)) * maxSteer);
        } else {
            wantAngle = (s16)(raw * maxSteer);
        }

        // The bars themselves have a rate: they are not a switch, and they come back to centre when
        // you stop asking. This is what `turnSmooth` used to do, moved onto the wheel it belongs to.
        resp = (sMc.drift == MC_DRIFT_ON) ? MKW_DRIFT_RESP : MKW_HANDLING_RESP;
        resp = 1.0f - MasterCycle_PerTick(1.0f - resp);
        sMc.turnSmooth += (raw - sMc.turnSmooth) * resp;
        if (sMc.turnSmooth > 1.0f) {
            sMc.turnSmooth = 1.0f;
        }
        if (sMc.turnSmooth < -1.0f) {
            sMc.turnSmooth = -1.0f;
        }
        Math_SmoothStepToS(&sMc.steer, wantAngle, 2, (wantAngle == 0) ? MC_STEER_RETURN : MC_STEER_RATE, 0x20);

        // THE MODEL: yawRate = (v / wheelbase) * tan(steer). Nothing else turns the bike.
        tanDelta = Math_SinS(sMc.steer) / Math_CosS(sMc.steer);
        yawRad = (sMc.speed / MC_WHEELBASE) * tanDelta;
        if (sMc.airborne) {
            yawRad *= 0.35f; // no tyre on the ground to push against
        }
        turn = (s16)(yawRad * (0x8000 / M_PI));
        wantSteer = sMc.steer; // the drawn front wheel IS the steering angle now
    }

    // Swinging about the REAR AXLE. Rotating a bike about its middle is what makes it read as a
    // spinning box; a real one pivots on the driven wheel, so the tail tracks the corner and the
    // nose is what swings out. The rear hub is held still across the turn and the body rebuilt
    // from it.
    //
    // All of this is world.rot.y, the direction of TRAVEL. shape.rot.y — where the bike is pointed
    // — is derived from it in the update, because in a drift the two differ by the drift angle and
    // the pivot must follow the path, not the pose.
    {
        f32 rearX = actor->world.pos.x + (Math_SinS(actor->world.rot.y) * MC_HUB_B_Z);
        f32 rearZ = actor->world.pos.z + (Math_CosS(actor->world.rot.y) * MC_HUB_B_Z);

        actor->world.rot.y += turn;

        actor->world.pos.x = rearX - (Math_SinS(actor->world.rot.y) * MC_HUB_B_Z);
        actor->world.pos.z = rearZ - (Math_CosS(actor->world.rot.y) * MC_HUB_B_Z);
    }

    // ── the drift angle: fn_1_6E704, transcribed ──
    // The body swings out of its own line of travel and stays there, and lets go when it is asked
    // to. This is the SLIDE, and it is a different thing from the turning above — which is why an
    // inside-drift bike can have a drift with no slide in it at all.
    {
        s16 want = (sMc.drift == MC_DRIFT_ON) ? (s16)(-sMc.driftDir * MC_DRIFT_ANGLE) : 0;

        if (sMc.driftAngle < want) {
            sMc.driftAngle += (want > 0) ? MC_DRIFT_ANGLE_RATE : MC_DRIFT_ANGLE_DECR;
            if (sMc.driftAngle > want) {
                sMc.driftAngle = want;
            }
        } else if (sMc.driftAngle > want) {
            sMc.driftAngle -= (want < 0) ? MC_DRIFT_ANGLE_RATE : MC_DRIFT_ANGLE_DECR;
            if (sMc.driftAngle < want) {
                sMc.driftAngle = want;
            }
        }
    }

    // ── lean, the EnHorse_TiltBody way: from the yaw actually turned this frame ──
    {
        s16 turnVel = actor->world.rot.y - sMc.lastYaw;
        f32 sp = fabsf(sMc.speed) / MC_MAX_SPEED;
        s16 want = (s16)(-(f32)MC_LEAN_MAX * sp * ((f32)turnVel / 0x400));

        if (sMc.drift == MC_DRIFT_ON) {
            f32 into = 0.6f + (0.4f * stickX * (f32)sMc.driftDir);

            if (into < 0.2f) {
                into = 0.2f;
            }
            want = (s16)(-(f32)sMc.driftDir * (f32)MC_DRIFT_LEAN * into);
        } else {
            if (want > MC_LEAN_MAX) {
                want = MC_LEAN_MAX;
            }
            if (want < -MC_LEAN_MAX) {
                want = -MC_LEAN_MAX;
            }
        }
        Math_SmoothStepToS(&sMc.lean, want, 3, 0x280, 0x20);
        sMc.lastYaw = actor->world.rot.y;
    }

    // Wheels turn with the ground covered.
    sMc.wheelSpin += sMc.speed / MC_WHEEL_RADIUS;
    if (sMc.wheelSpin > (2.0f * M_PI)) {
        sMc.wheelSpin -= 2.0f * M_PI;
    }
    if (sMc.wheelSpin < 0.0f) {
        sMc.wheelSpin += 2.0f * M_PI;
    }
}

// Which of Epona's poses Link should be sitting in, from the speed, and drive the horse's own
// SkelAnime so curFrame advances — Player_Action_8084CC98 copies that frame straight into Link's
// riding animation.
static void MasterCycle_UpdateRiderPose(EnHorse* horse) {
    f32 sp = fabsf(sMc.speed);
    s32 want;
    AnimationHeader* anim;
    f32 playSpeed;

    // In the air Link takes his own horseback jump pose, free: the player's ride action reads the
    // index straight out of here. (Anything in D_80854944 is reachable if another pose reads better.)
    if (sMc.airborne) {
        want = ENHORSE_ANIM_LOW_JUMP;
        anim = (AnimationHeader*)gEponaJumpingAnim;
        playSpeed = 1.0f;
    } else if (sp < MC_STOPPED) {
        want = ENHORSE_ANIM_IDLE;
        anim = (AnimationHeader*)gEponaIdleAnim;
        playSpeed = 1.0f;
    } else if (sp < MC_MAX_SPEED * 0.33f) {
        want = ENHORSE_ANIM_WALK;
        anim = (AnimationHeader*)gEponaWalkingAnim;
        playSpeed = 0.5f + (sp / (MC_MAX_SPEED * 0.33f));
    } else if (sp < MC_MAX_SPEED * 0.66f) {
        want = ENHORSE_ANIM_TROT;
        anim = (AnimationHeader*)gEponaTrottingAnim;
        playSpeed = 0.75f + ((sp - (MC_MAX_SPEED * 0.33f)) / (MC_MAX_SPEED * 0.66f));
    } else {
        want = ENHORSE_ANIM_GALLOP;
        anim = (AnimationHeader*)gEponaGallopingAnim;
        playSpeed = 0.9f + ((sp - (MC_MAX_SPEED * 0.66f)) / (MC_MAX_SPEED * 1.3f));
    }

    if (horse->animationIdx != want) {
        horse->animationIdx = want;
        Animation_PlayLoop(&horse->skin.skelAnime, anim);
    }
    horse->skin.skelAnime.playSpeed = playSpeed;
    SkelAnime_Update(&horse->skin.skelAnime);
    horse->curFrame = horse->skin.skelAnime.curFrame;
}

// Trample collider: hurts what the bike hits at speed. Sized to the frame, positioned at its
// centre, damage scaled with speed.
static void MasterCycle_UpdateHitCollider(PlayState* play, Actor* actor) {
    CombatColliderConfig cfg;
    Vec3f pos;
    f32 sp = fabsf(sMc.speed);
    f32 t;
    u8 dmg;

    if (sp < MC_HIT_MIN_SPEED) {
        return;
    }
    t = (sp - MC_HIT_MIN_SPEED) / (MC_MAX_SPEED * (1.0f + MC_MT_BONUS) - MC_HIT_MIN_SPEED);
    if (t > 1.0f) {
        t = 1.0f;
    }
    dmg = (u8)(MC_HIT_DAMAGE_LOW + (s32)((MC_HIT_DAMAGE_HIGH - MC_HIT_DAMAGE_LOW) * t + 0.5f));

    cfg.dmgFlags = DMG_HAMMER;
    cfg.damage = dmg;
    cfg.effect = 0;
    cfg.radius = MC_WHEELBASE * 0.5f + 8.0f;
    cfg.height = MC_BODY_TOP;
    if (!sMc.hitColReady) {
        Combat_InitCylinder(play, &sMc.hitCol, actor, &cfg);
        sMc.hitColReady = 1;
    }
    pos = actor->world.pos;
    Combat_UpdateCylinder(&sMc.hitCol, &pos, &cfg);
    Combat_RegisterCollider(play, &sMc.hitCol);
    if (Combat_CheckHit(&sMc.hitCol)) {
        sMc.hitCol.base.atFlags &= ~AT_HIT;
        // Running something over costs a little speed, like a kart clipping an item box.
        sMc.speed *= 0.85f;
    }
}

// Hold the chassis up on its two wheels instead of pasting it onto the floor.
//
// The contact the bike rides is the HIGHER of the two wheels' ground — the one actually carrying —
// so cresting a rise lifts the body smoothly instead of the rear wheel dragging it through the
// slope. Beyond the suspension's travel the wheels are off the ground and this does nothing at all,
// which is how a hop stays a clean parabola.
static void MasterCycle_Suspension(Actor* actor) {
    f32 stiff[2];
    f32 land;
    f32 mid;
    f32 tilt;
    s32 i;
    s32 grounded = 0;

    stiff[0] = MC_SUS_STIFFNESS_F;
    stiff[1] = MC_SUS_STIFFNESS;

    // Jumping. The springs would happily haul a 5-unit hop straight back down — that whole hop is
    // well inside their 25 units of travel — so while a launch is in the air they do nothing at all
    // and gravity has the bike. The landing is the higher of the two wheels' ground.
    if (sMc.launched) {
        land = (sMc.wheelY[0] > sMc.wheelY[1]) ? sMc.wheelY[0] : sMc.wheelY[1];
        sMc.airborne = 1;
        actor->bgCheckFlags &= ~BGCHECKFLAG_GROUND;
        sMc.susReady = 0;
        if ((actor->velocity.y <= 0.0f) && (land > BGCHECK_Y_MIN) && (actor->world.pos.y <= land)) {
            actor->world.pos.y = land;
            actor->velocity.y = 0.0f;
            sMc.launched = 0;
            sMc.airborne = 0;
            actor->bgCheckFlags |= BGCHECKFLAG_GROUND;
        }
        return;
    }

    // Each wheel is sprung against ITS OWN ground. One spring for the whole bike is what made the
    // front end float: with the height taken from whichever wheel happened to be higher, the front
    // wheel was never following anything — it hung wherever the rear put it.
    for (i = 0; i < 2; i++) {
        f32 target = sMc.wheelY[i];
        f32 err;

        if (target <= BGCHECK_Y_MIN) {
            // Nothing under this wheel: it hangs, and only gravity has it.
            sMc.susY[i] = actor->world.pos.y;
            sMc.susVel[i] = 0.0f;
            continue;
        }
        if (!sMc.susReady) {
            sMc.susY[i] = target;
            sMc.susVel[i] = 0.0f;
        }
        err = target - sMc.susY[i];
        if (err < -MC_SUS_TRAVEL) {
            // This end is further off the ground than the suspension reaches — it is in the air.
            sMc.susY[i] = actor->world.pos.y;
            sMc.susVel[i] = 0.0f;
            continue;
        }
        if (err > MC_SUS_TRAVEL) {
            sMc.susY[i] = target; // driven into a step: put that end straight on it
            sMc.susVel[i] = 0.0f;
        } else {
            // The damper works against the spring's own velocity — MKW's `+ damping * travelSpeed`.
            sMc.susVel[i] += (err * stiff[i]) - (sMc.susVel[i] * MC_SUS_DAMPING);
            sMc.susY[i] += sMc.susVel[i];
        }
        grounded++;
    }
    sMc.susReady = 1;

    if (grounded == 0) {
        sMc.airborne = 1;
        actor->bgCheckFlags &= ~BGCHECKFLAG_GROUND;
        return;
    }

    // The body sits between its two wheels, and its PITCH is the line between them — a consequence
    // of the suspension rather than a separately smoothed guess, which is why it no longer lags
    // behind what the wheels are doing.
    mid = (sMc.susY[0] + sMc.susY[1]) * 0.5f;
    if ((mid - actor->world.pos.y) < -MC_SUS_TRAVEL) {
        sMc.airborne = 1;
        actor->bgCheckFlags &= ~BGCHECKFLAG_GROUND;
        return;
    }
    actor->world.pos.y = mid;
    if (actor->velocity.y < 0.0f) {
        actor->velocity.y = 0.0f; // landed; the springs have it from here
    }

    tilt = sMc.susY[0] - sMc.susY[1];
    if (tilt > (MC_WHEELBASE * MC_TERRAIN_SLOPE_LIMIT)) {
        tilt = MC_WHEELBASE * MC_TERRAIN_SLOPE_LIMIT;
    }
    if (tilt < -(MC_WHEELBASE * MC_TERRAIN_SLOPE_LIMIT)) {
        tilt = -(MC_WHEELBASE * MC_TERRAIN_SLOPE_LIMIT);
    }
    sMc.terrainPitch = (s16)(Math_FAtan2F(tilt, MC_WHEELBASE) * (0x8000 / M_PI));

    sMc.airborne = 0;
    actor->bgCheckFlags |= BGCHECKFLAG_GROUND;
}

// ── The actor's update ───────────────────────────────────────────────────────

static void MasterCycle_Update(Actor* thisx, PlayState* play) {
    EnHorse* horse = (EnHorse*)thisx;
    Player* player = GET_PLAYER(play);
    u8 riding;

    if (thisx != sMc.actor) {
        // A horse we no longer track running our update: put Epona's own back.
        thisx->update = EnHorse_Update;
        thisx->draw = EnHorse_Draw;
        return;
    }
    sMc.age++;

    // ── materialize / dismiss ──
    if (sMc.phase == MC_PHASE_MATERIALIZE) {
        f32 t = 1.0f - ((f32)sMc.phaseTimer / (f32)MC_MATERIALIZE_TICKS);
        f32 sc = MC_MODEL_SCALE * (0.05f + (0.95f * t));

        Actor_SetScale(thisx, sc);
        // Fall the last few units onto the ground while it forms.
        thisx->speedXZ = 0.0f;
        Actor_MoveXZGravity(thisx);
        Actor_UpdateBgCheckInfo(play, thisx, 30.0f, MC_BODY_HALF_WIDTH, MC_BODY_TOP, 0x1D);
        thisx->prevPos = thisx->world.pos;
        if (--sMc.phaseTimer <= 0) {
            sMc.phase = MC_PHASE_ACTIVE;
            Actor_SetScale(thisx, MC_MODEL_SCALE);
        }
        MasterCycle_SeatPos(thisx, &horse->riderPos);
        return;
    }
    if (sMc.phase == MC_PHASE_DISMISS) {
        f32 t = (f32)sMc.phaseTimer / (f32)MC_DISMISS_TICKS;

        Actor_SetScale(thisx, MC_MODEL_SCALE * (0.05f + (0.95f * t)));
        if (--sMc.phaseTimer <= 0) {
            thisx->bgCheckFlags &= ~0x800; // ours only; do not leave it on a recycled actor
            MasterCycle_TrailsFree(play);
            Actor_Kill(thisx);
            MasterCycle_Forget();
        }
        return;
    }

    // `child` is the player while he is genuinely in the saddle: Actor_MountHorse sets it, and the
    // player clears it the frame he STARTS a dismount — so the controls let go the moment the
    // dismount animation begins rather than when it ends.
    riding = (player->stateFlags1 & PLAYER_STATE1_ON_HORSE) && (player->rideActor == thisx) &&
             (thisx->child == &player->actor);

    // ── offer the mount ──
    // The player mounts by pressing A with rideActor set (Player_ActionHandler_3), and rideActor is
    // reset every frame he is not on it — so it has to be offered every frame he is close enough.
    if (!riding && (sMc.phase == MC_PHASE_ACTIVE)) {
        f32 dx = player->actor.world.pos.x - thisx->world.pos.x;
        f32 dz = player->actor.world.pos.z - thisx->world.pos.z;
        f32 dy = player->actor.world.pos.y - thisx->world.pos.y;
        f32 dist = sqrtf((dx * dx) + (dz * dz));

        horse->action = MC_ACTION_PARKED;
        if ((dist < MC_MOUNT_RANGE) && (fabsf(dy) < MC_MOUNT_MAX_DY) && (player->rideActor == NULL) &&
            (fabsf(sMc.speed) < MC_STOPPED)) {
            // Which side he is on, in the bike's own frame: sign of his local X.
            f32 localX = (dx * Math_CosS(thisx->shape.rot.y)) - (dz * Math_SinS(thisx->shape.rot.y));

            sMc.mountSide = (localX >= 0.0f) ? 1 : -1;
            Actor_SetRideActor(play, thisx, sMc.mountSide);
        }
        // Parked: roll to a stop, still obey gravity.
        sMc.speed *= 0.9f;
        if (fabsf(sMc.speed) < 0.05f) {
            sMc.speed = 0.0f;
        }
        sMc.wheelie = 0;
        sMc.drift = MC_DRIFT_NONE;
        sMc.boostTimer = 0;
        Math_SmoothStepToS(&sMc.wheeliePitch, 0, 3, MC_WHEELIE_PITCH_STEP, 0x40);
        Math_SmoothStepToS(&sMc.lean, 0, 3, 0x280, 0x20);
        Math_SmoothStepToS(&sMc.steer, 0, 3, MC_STEER_RETURN, 0x40);
    } else if (riding && (player->av2.actionVar2 != 0)) {
        // SEATED, not merely mounting. Actor_MountHorse hands us the rider the instant A is pressed,
        // and that same A held for the climb-on animation was driving the bike out from under him.
        // Player_SetupAction zeroes av2 when the ride action starts, and Player_Action_8084CC98
        // leaves it at 0 for the whole mount animation, so av2 != 0 is "he is in the saddle".
        horse->action = MC_ACTION_RIDDEN;
        MasterCycle_Drive(play, thisx, player);
    } else if (riding) {
        // Climbing on: hold still.
        horse->action = MC_ACTION_RIDDEN;
        sMc.speed = 0.0f;
    } else {
        // Mid-dismount: coast, no input.
        sMc.speed *= 0.9f;
    }
    // FLAG_6 is the forced-dismount request. The player consumes it by starting his dismount, and
    // it has to be gone once he is off, or the next mount would step straight back down.
    if (!riding && (horse->stateFlags & ENHORSE_FLAG_6)) {
        horse->stateFlags &= ~ENHORSE_FLAG_6;
    }
    sMc.wasRiding = riding ? 1 : (sMc.wasRiding == 2 ? 2 : 0);

    // ── move ──
    // A climb does its own moving (it is glued to a vertical face and travelling UP it; the ground
    // model would pull it straight back down). Everything else goes through the ballistic move.
    MasterCycle_Move(play, thisx);
    MasterCycle_ProbeTerrain(play, thisx);
    MasterCycle_Suspension(thisx);
    MasterCycle_Trails(play, thisx);

    // Deep water kills the ride: the bike stops and Link is put off.
    if ((thisx->bgCheckFlags & BGCHECKFLAG_WATER) && (thisx->yDistToWater > MC_DEEP_WATER)) {
        sMc.speed = 0.0f;
        if (riding) {
            MasterCycle_ForceDismount();
        }
    }

    // ── pose ──
    // Nose-up pitch is negative shape.rot.x; the terrain wants the front raised when it is higher.
    sMc.pitch = (s16)(-sMc.terrainPitch - sMc.wheeliePitch);
    // Standing on a wall is a POSE, not a lean: nose-up is negative rot.x here, so 90 degrees of it
    // puts the bike flat against the face, wheels on the wall and nose to the sky. Link comes with
    // it because the rider tilt below copies this same rot.x.
    // The drift angle turns the BODY out of its line of travel — world.rot.y is where the bike is
    // going, shape.rot.y is where it is pointed, and in a drift those are not the same thing.
    thisx->shape.rot.y = (s16)(thisx->world.rot.y + sMc.driftAngle);
    thisx->shape.rot.x = sMc.pitch;
    thisx->shape.rot.z = sMc.lean;
    thisx->world.rot.x = 0;
    thisx->world.rot.z = 0;

    MasterCycle_SeatPos(thisx, &horse->riderPos);
    MasterCycle_UpdateRiderPose(horse);

    // Link tilts WITH the bike. The player's ride action copies only the yaw
    // (Player_Action_8084CC98: shape.rot.y = rideActor's), so the lean and the wheelie pitch are
    // handed to his own shape.rot here. Player_Draw goes through Actor_Draw's
    // Matrix_SetTranslateRotateYXZ, which honours all three, and nothing in the ride action writes
    // rot.x/z back — but they are zeroed the moment he is off, or he would walk away crooked.
    if (riding) {
        player->actor.shape.rot.x = thisx->shape.rot.x;
        player->actor.shape.rot.z = thisx->shape.rot.z;
        sMc.riderTilted = 1;
    } else if (sMc.riderTilted) {
        player->actor.shape.rot.x = 0;
        player->actor.shape.rot.z = 0;
        sMc.riderTilted = 0;
    }

    // ── collision with the world's actors ──
    Collider_UpdateCylinder(thisx, &horse->cyl1);
    Collider_UpdateCylinder(thisx, &horse->cyl2);
    horse->cyl1.dim.pos.x += (s16)(Math_SinS(thisx->shape.rot.y) * 22.0f);
    horse->cyl1.dim.pos.z += (s16)(Math_CosS(thisx->shape.rot.y) * 22.0f);
    horse->cyl2.dim.pos.x += (s16)(Math_SinS(thisx->shape.rot.y) * -24.0f);
    horse->cyl2.dim.pos.z += (s16)(Math_CosS(thisx->shape.rot.y) * -24.0f);
    CollisionCheck_SetOC(play, &play->colChkCtx, &horse->cyl1.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &horse->cyl2.base);
    if (riding) {
        MasterCycle_UpdateHitCollider(play, thisx);
    }

    thisx->focus.pos = thisx->world.pos;
    thisx->focus.pos.y += 45.0f;
    thisx->speedXZ = fabsf(sMc.speed); // what the player-side code reads to gate talking etc.
}

// The charge flames, at the rear hub, in the bike's own frame (the caller has already set that up).
static void MasterCycle_DrawChargeFlames(PlayState* play, Actor* actor) {
    static const Color_RGBA8 sPrim[3] = {
        { 255, 200, 60, 255 },  // orange  (En_Light's flame table, entry 0)
        { 0, 170, 255, 255 },   // blue    (entry 2)
        { 255, 170, 255, 255 }, // purple  (entry 13)
    };
    static const Color_RGB8 sEnv[3] = {
        { 255, 80, 0 },
        { 0, 0, 255 },
        { 100, 0, 255 },
    };
    f32 charge;
    s32 stage;
    f32 grow;
    s32 i;

    if ((sMc.drift != MC_DRIFT_ON) || (MKW_MT_MAX <= 0)) {
        return;
    }
    charge = (f32)sMc.mtCharge / (f32)MKW_MT_MAX;
    if (charge <= 0.02f) {
        return;
    }
    stage = (charge < MC_MT_STAGE1) ? 0 : ((charge < MC_MT_STAGE2) ? 1 : 2);
    // The flames grow with the charge, and the banked mini-turbo burns biggest of all.
    grow = 0.55f + (0.45f * charge) + ((stage == 2) ? 0.35f : 0.0f);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    // Same scrolling texture setup En_Light's own draw uses, so it reads as that flame and not as
    // a static decal.
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 32, 64, 1, 0, (play->gameplayFrames * -20) & 511, 32,
                                  128, 0, 0, 0, -20));
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, sPrim[stage].r, sPrim[stage].g, sPrim[stage].b, sPrim[stage].a);
    gDPSetEnvColor(POLY_XLU_DISP++, sEnv[stage].r, sEnv[stage].g, sEnv[stage].b, 0);

    for (i = 0; i < MC_MT_FLAME_COUNT; i++) {
        f32 side = (i == 0) ? MC_MT_FLAME_SPREAD : -MC_MT_FLAME_SPREAD;
        f32 sc = MC_MT_FLAME_SCALE * grow;

        Matrix_Push();
        Matrix_Translate(side, MC_MT_PIPE_Y, MC_MT_PIPE_Z, MTXMODE_APPLY);
        Matrix_RotateX(-M_PI / 2.0f, MTXMODE_APPLY);
        // ...and now the frame's Y axis IS the pipe, so this is a ROLL about it rather than a yaw:
        // the flame turns around its own jet to keep facing the camera - flat when you are behind
        // it, edge-up when you are beside it - instead of vanishing.
        Matrix_RotateY((s16)((Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) - actor->shape.rot.y) + 0x8000) *
                           (M_PI / 32768.0f),
                       MTXMODE_APPLY);
        Matrix_Scale(sc, sc, sc, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, gEffFire1DL);
        Matrix_Pop();
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// ── Draw ─────────────────────────────────────────────────────────────────────

static void MasterCycle_Draw(Actor* thisx, PlayState* play) {
    f32 sc = thisx->scale.x * MC_MODEL_SCALE;
    f32 steer = BINANG_TO_RAD(sMc.steer);

    MasterCycle_LoadDLs();
    if (sMcDlBody == NULL) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    // Frame: position, yaw, pitch (terrain + wheelie), lean. Wheelie pivots about the REAR hub so
    // the front lifts and the back stays planted, which is what a wheelie is.
    Matrix_Translate(thisx->world.pos.x, thisx->world.pos.y + MC_MODEL_Y_OFFSET, thisx->world.pos.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(thisx->shape.rot.y), MTXMODE_APPLY);
    Matrix_RotateZ(BINANG_TO_RAD(thisx->shape.rot.z), MTXMODE_APPLY);
    Matrix_Translate(0.0f, MC_HUB_Y, MC_HUB_B_Z, MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD(thisx->shape.rot.x), MTXMODE_APPLY);
    Matrix_Translate(0.0f, -MC_HUB_Y, -MC_HUB_B_Z, MTXMODE_APPLY);
    Matrix_Scale(sc, sc, sc, MTXMODE_APPLY);

    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, sMcDlBody);
    if (sMcDlLights != NULL) {
        gSPDisplayList(POLY_OPA_DISP++, sMcDlLights);
    }

    // Rear wheel: spins about its hub.
    if (sMcDlWheelB != NULL) {
        Matrix_Push();
        Matrix_Translate(0.0f, MC_HUB_Y, MC_HUB_B_Z, MTXMODE_APPLY);
        Matrix_RotateX(sMc.wheelSpin, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, sMcDlWheelB);
        Matrix_Pop();
    }
    // The mini-turbo charge, off the rear wheel. Drawn in the body's frame so it leans, pitches and
    // climbs with the bike.
    MasterCycle_DrawChargeFlames(play, thisx);

    // Front wheel: steers about the vertical, then spins.
    if (sMcDlWheelF != NULL) {
        Matrix_Push();
        Matrix_Translate(0.0f, MC_HUB_Y, MC_HUB_F_Z, MTXMODE_APPLY);
        Matrix_RotateY(steer, MTXMODE_APPLY);
        Matrix_RotateX(sMc.wheelSpin, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, sMcDlWheelF);
        Matrix_Pop();
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
