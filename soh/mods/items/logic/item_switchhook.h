/**
 * item_switchhook.h - Switch Hook from Oracle of Ages
 *
 * Controls (hold-to-aim like Bomb Arrows):
 *   Hold C Button:   First-person aiming mode
 *   Release C:       Fire hook projectile
 *   Z-targeting:     Third-person aiming at target
 *
 * Features:
 *   - Swaps positions with swappable actors (pots, crates, certain enemies)
 *   - Deals hookshot damage to non-swappable actors and bounces back
 *   - Uses targeted actor (Z-target) for instant swap when available
 *   - Longshot distance (26 frames)
 *   - Usable by both child and adult Link
 *   - Hook tip rotated 180 degrees (reversed appearance)
 *   - Blue reticle during aiming (like Gust Jar suck mode)
 */

#ifndef ITEM_SWITCHHOOK_H
#define ITEM_SWITCHHOOK_H

#include "z64.h"
#include "../custom_items.h"

// ============================================================================
// CONFIGURATION
// ============================================================================

#define SWITCHHOOK_TIMER \
    26                            // Longshot distance (frames). Skijer's NEI: reach now lives in
                                  // z_arms_hook.c's variant table (20 * 26); this define is legacy.
#define SWITCHHOOK_SPEED 20.0f    // Projectile speed
#define SWITCHHOOK_SWAP_FRAMES 15 // Frames for swap animation
#define SWITCHHOOK_DAMAGE 1       // Damage dealt to non-swappable actors

// ============================================================================
// STATES
// ============================================================================

#define SWITCHHOOK_STATE_IDLE 0       // Waiting for input
#define SWITCHHOOK_STATE_AIMING 1     // First-person aiming (hold C button)
#define SWITCHHOOK_STATE_SHOOTING 2   // Projectile flying forward
#define SWITCHHOOK_STATE_HIT_SWAP 3   // Hit swappable actor, performing swap
#define SWITCHHOOK_STATE_HIT_DAMAGE 4 // Hit non-swappable actor, damage + bounce
#define SWITCHHOOK_STATE_RETRACT 5    // Retracting back to player

// ============================================================================
// STATE ALIASES (shortcuts to gCustomItemState fields)
// ============================================================================

#define shActive gCustomItemState.switchHookActive
#define shState gCustomItemState.switchHookState
#define shFirstPerson gCustomItemState.switchHookFirstPerson
#define shProjPos gCustomItemState.switchHookProjPos
#define shProjYaw gCustomItemState.switchHookProjYaw
#define shProjPitch gCustomItemState.switchHookProjPitch
#define shTimer gCustomItemState.switchHookTimer
#define shTarget gCustomItemState.switchHookTarget
#define shLinkStartPos gCustomItemState.switchHookLinkStartPos
#define shTargetStartPos gCustomItemState.switchHookTargetStartPos
#define shSwapTimer gCustomItemState.switchHookSwapTimer
#define shCollider gCustomItemState.switchHookCollider
#define shButtonMask gCustomItemState.switchHookButtonMask
#define shVortexTimer gCustomItemState.switchHookVortexTimer

// ============================================================================
// ACTOR FLAG (for custom actors like Somaria Cubes)
// ============================================================================

#ifndef ACTOR_FLAG_SWITCHHOOKABLE
#define ACTOR_FLAG_SWITCHHOOKABLE (1 << 28)
#endif

// ============================================================================
// COLLIDER INIT
// ============================================================================

static ColliderQuadInit sSwitchHookQuadInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_TYPE_PLAYER,
        COLSHAPE_QUAD,
    },
    {
        ELEMTYPE_UNK2,
        { 0x00000080, 0x00, 0x01 }, // Hookshot damage flags
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * Teleporting an actor by writing world.pos moves its model and bg checks but leaves its HITBOXES
 * behind: colliders store world-space geometry that only the owning actor refreshes inside its own
 * update, and many actors build theirs once at init.
 *
 * Capture BEFORE the teleport (slot 0 = Link, slot 1 = the actor he swaps with), then re-anchor once
 * per frame until both have settled — the actors keep moving after the teleport (pushed out of walls,
 * dropped onto floors) and only an absolute reposition per frame follows that. Defined in
 * item_switchhook.c; z_arms_hook.c drives it for the switch-hook swap.
 */
void SwitchHook_CaptureSwapColliders(PlayState* play, s32 slot, Actor* actor);
void SwitchHook_ReanchorSwapColliders(PlayState* play);
void SwitchHook_ClearSwapColliders(void);

/** One-shot rigid translation of every collider `actor` has live this frame. */
void SwitchHook_ShiftActorColliders(PlayState* play, Actor* actor, Vec3f* delta);

/**
 * The two primitives the re-anchoring is built out of, exported because Ultrahand needs the same
 * treatment for a different number of actors (a built structure is up to seven) and there is no
 * second correct way to write either of them.
 *
 * SwitchHook_ShiftCollider translates one collider rigidly, covering all four shapes including
 * the two easy to miss: a ColliderTris' cached plane distance and a ColliderQuad's cached edge
 * midpoints. SwitchHook_GetColliderRefPos reads back the one point that stands in for "where
 * this collider is". Returns 0 for a shape it cannot read.
 */
void SwitchHook_ShiftCollider(Collider* col, Vec3f* delta);
s32 SwitchHook_GetColliderRefPos(Collider* col, Vec3f* out);

/**
 * Check if an actor can be swapped with.
 * @param actor The actor to check
 * @return 1 if swappable, 0 otherwise
 */
static inline s32 SwitchHook_CanSwap(Actor* actor) {
    if (actor == NULL)
        return 0;

    // Check custom flag first (for Somaria Cubes, etc.)
    if (actor->flags & ACTOR_FLAG_SWITCHHOOKABLE) {
        return 1;
    }

    // Skijer's NEI switchhook overhaul: no more hand-picked item list — switch positions with ANY
    // prop (pots, crates, torches, signs...) or NON-BOSS enemy. ACTORCAT_ENEMY excludes bosses;
    // props/enemies are the only categories that swap here — player, background, items are ignored.
    // (z_arms_hook.c's own ArmsHook_IsSwappable additionally accepts chests/NPCs.)
    if (actor->category == ACTORCAT_ENEMY || actor->category == ACTORCAT_PROP) {
        return 1;
    }

    return 0;
}

#endif // ITEM_SWITCHHOOK_H
