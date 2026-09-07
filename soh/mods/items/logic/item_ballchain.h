/**
 * Ball and Chain Item
 *
 * Heavy weapon that Link spins above his head and throws.
 * Deals Giant's Knife damage, double to Skulltulas/Freezards.
 * Can destroy Spirit Temple iron objects and red ice.
 */

#ifndef ITEM_BALLCHAIN_H
#define ITEM_BALLCHAIN_H

#include "z64.h"
#include "../custom_items.h"

// =============================================================================
// States
// =============================================================================
#define BALLCHAIN_STATE_INACTIVE 0 // Not equipped
#define BALLCHAIN_STATE_EQUIP 1    // Holding ball, can walk slowly
#define BALLCHAIN_STATE_SPINNING 2 // Spinning above head, charging
#define BALLCHAIN_STATE_THROWN 3   // Ball flying, Link frozen

// =============================================================================
// Physics Constants
// =============================================================================

// Throw — TP ballistic ARC (velocity + gravity + bounces), per 20fps logic frame. Skijer's NEI
#define BALLCHAIN_LAUNCH_SPEED_MIN 22.0f // Launch speed with no wind-up
#define BALLCHAIN_LAUNCH_SPEED_MAX 32.0f // Launch speed fully wound up
#define BALLCHAIN_LAUNCH_VY 7.0f         // Extra upward kick at launch (arc height)
#define BALLCHAIN_GRAVITY (-1.5f)        // Gravity on the flying ball
#define BALLCHAIN_TERMINAL_VY 30.0f      // Fall speed clamp
#define BALLCHAIN_RETRACT_SPEED 34.0f    // Reel-in speed back to Link's hand

// Ground bounce / rest
#define BALLCHAIN_BOUNCE_FACTOR 0.45f      // velocity.y kept (inverted) on a floor bounce
#define BALLCHAIN_BOUNCE_XZ_KEEP 0.65f     // XZ speed kept on a floor bounce
#define BALLCHAIN_MAX_BOUNCES 2            // Hard floor bounces before the ball settles
#define BALLCHAIN_REST_FRAMES 12           // Beat on the ground before retracting (~0.6s)
#define BALLCHAIN_WALL_BOUNCE_FACTOR 0.55f // XZ speed kept after reflecting off a wall normal

// Chain / safety
#define BALLCHAIN_CHAIN_MAX 380.0f   // Chain length — the ball can never fly past this
#define BALLCHAIN_THROWN_TIMEOUT 200 // Hard safety: force the ball back after ~10s

// Ballistic thrown sub-phases (bcPhase) — TP arc lifecycle. Skijer's NEI
#define BALLCHAIN_PHASE_FLY 0     // Ballistic flight (gravity + bounces)
#define BALLCHAIN_PHASE_REST 1    // Resting a beat on the ground after the last bounce
#define BALLCHAIN_PHASE_RETRACT 2 // Reeling back along the chain to Link's hand

// Throw direction
#define BALLCHAIN_THROW_YAW_MAX 0x2000   // ~45 deg horizontal offset
#define BALLCHAIN_THROW_PITCH_MAX 0x1800 // ~30 deg vertical offset
#define BALLCHAIN_THROW_LEAN 3000        // Upper body forward lean

// Spin orbit
#define BALLCHAIN_SPIN_RADIUS 20.0f     // Orbit radius around Link
#define BALLCHAIN_SPIN_HEIGHT_MIN 50.0f // Starting height (low charge)
#define BALLCHAIN_SPIN_HEIGHT_MAX 55.0f // Final height (full charge)
#define BALLCHAIN_SPIN_SPEED_MIN 0x1000 // Starting spin angular velocity
#define BALLCHAIN_SPIN_SPEED_MAX 0x2000 // Max spin angular velocity
#define BALLCHAIN_CHARGE_MAX 60         // Frames to full charge

// Equip state — TWO-HANDED grip: the held ball sits at the MIDPOINT of both hands (same point the
// chain is drawn from), so it stays centered between Link's hands for every facing. Skijer's NEI
#define BALLCHAIN_EQUIP_HEIGHT 20.0f
#define BALLCHAIN_EQUIP_FORWARD 10.0f
#define BALLCHAIN_EQUIP_Y_OFFSET 5.0f // small drop below the two-hand midpoint
#define BALLCHAIN_EQUIP_SCALE 0.06f
#define BALLCHAIN_SPIN_SCALE 0.1f

// Movement penalties
#define BALLCHAIN_SPEED_MULT 0.4f     // Walk speed multiplier (just holding)
#define BALLCHAIN_SPIN_WALK_MULT 0.3f // Walk speed multiplier WHILE spinning (MM lets you shuffle) — Skijer's NEI
#define BALLCHAIN_LEAN_MULT 3500.0f   // Upper body lean factor
#define BALLCHAIN_LEAN_TILT 40.0f     // Orbit tilt from stick
#define BALLCHAIN_STICK_DEADZONE 5.0f

// =============================================================================
// Collision
// =============================================================================
#define BALLCHAIN_COL_RADIUS 20
#define BALLCHAIN_COL_HEIGHT 20
#define BALLCHAIN_BALL_RADIUS 12.0f // Visual/floor-contact radius of the ball
#define BALLCHAIN_WALL_RADIUS 20.0f
#define BALLCHAIN_WALL_HEIGHT 20.0f
#define BALLCHAIN_RETURN_DIST 65.0f // Distance to consider "returned"
#define BALLCHAIN_DAMAGE 8          // 2 hearts

// Proximity reach for shattering ICE actors — the fast throw/retract tunnels past thin ice, so ice
// is destroyed by proximity instead of relying on the collider overlapping. Skijer's NEI
#define BALLCHAIN_ICE_REACH 140.0f       // icicles + ice enemies (freezard)
#define BALLCHAIN_BIGICE_REACH 240.0f    // LARGE blocks (red ice) — huge actors, need a bigger reach
#define BALLCHAIN_BREAKABLE_REACH 100.0f // pots / iron objects

#ifndef DMG_JUMP_GIANT
#define DMG_JUMP_GIANT (1 << 0x1A)
#endif

// =============================================================================
// Sound Effects
// =============================================================================
#define BALLCHAIN_SFX_SWING NA_SE_IT_HAMMER_SWING
#define BALLCHAIN_SFX_HIT NA_SE_IT_HAMMER_HIT
#define BALLCHAIN_SFX_WHOOSH (NA_SE_IT_SWORD_SWING - SFX_FLAG)
#define BALLCHAIN_SFX_RETRACT (NA_SE_PL_WALK_GROUND - SFX_FLAG)
#define BALLCHAIN_SFX_WALL_BOUNCE NA_SE_IT_SHIELD_BOUND
#define BALLCHAIN_SFX_VOICE_ADULT NA_SE_VO_LI_SWORD_N
#define BALLCHAIN_SFX_VOICE_CHILD NA_SE_VO_LI_SWORD_N_KID

// =============================================================================
// State Aliases (mapped to gCustomItemState fields)
// =============================================================================
#define bcActive gCustomItemState.ballAndChainThrown       // u8: Item is active
#define bcState gCustomItemState.timer2                    // s16: Current state (INACTIVE/EQUIP/SPINNING/THROWN)
#define bcCharge gCustomItemState.timer1                   // s16: Charge frames (0 to CHARGE_MAX)
#define bcSpinAngle gCustomItemState.somariaCooldown       // s16: Current spin angle (binary angle)
#define bcThrowDist gCustomItemState.globalCooldownTimer   // s32: Remaining throw distance
#define bcThrowYaw gCustomItemState.sharedYaw              // s16: Throw direction yaw
#define bcThrowPitch gCustomItemState.sharedPitch          // s16: Throw direction pitch
#define bcBallPos gCustomItemState.sharedProjectilePos     // Vec3f: Ball world position
#define bcBallVel gCustomItemState.ballAndChainVel         // Vec3f: Ball velocity (thrown) — Skijer's NEI
#define bcPhase gCustomItemState.ballAndChainPhase         // u8: Thrown sub-phase — Skijer's NEI
#define bcBounces gCustomItemState.ballAndChainBounces     // u8: Floor bounces this throw — Skijer's NEI
#define bcRestTimer gCustomItemState.ballAndChainRestTimer // s16: Rest beat / retract clink counter — Skijer's NEI
#define bcCollider gCustomItemState.ballAndChainCollider   // ColliderCylinder: Damage collider
#define bcFirstPerson gCustomItemState.ballAndChainFirstPersonActive // u8: First person aim mode
#define bcTrailIndex gCustomItemState.ballAndChainTrailIndex         // s32: EffectBlure trail index — Skijer's NEI
#define bcTrailActive gCustomItemState.ballAndChainTrailActive       // u8: trail allocated
#define bcTrailTick gCustomItemState.ballAndChainTrailTick           // u8: sparse-feed frame counter

#endif
