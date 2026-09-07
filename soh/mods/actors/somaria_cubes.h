/**
 * Cane summon system (Somaria side of the Dual Cane) — Skijer's NEI.
 *
 * Three summon kinds share ONE pool of CANE_MAX_SUMMONS slots; summoning past the
 * cap destroys the oldest.
 *
 *   STATUE   — an MM Elegy of Emptiness shell (drawn from mm.o2r, hosted on a
 *              hijacked En_Lightbox). Placed AT Link's feet. NOT liftable — the
 *              old Somaria cube could be picked up and thrown, an Elegy statue
 *              cannot. Presses switches, including the heavy ones vanilla OoT
 *              needs Ruto for (Bg_Bdan_Switch YELLOW_HEAVY).
 *   BLOCK    — the real pushable block (ACTOR_OBJ_OSHIHIKI). Aimed placement.
 *   PLATFORM — a square textured slab (ACTOR_OBJ_LIFT, frozen so it never
 *              collapses) tinted red and parked in mid-air. Aimed placement, and
 *              it may be placed ANYWHERE — geometry is not a blocker.
 *
 * The file keeps its historical name and the SomariaCube_* symbols because
 * item_dominionrod.c targets summons through SomariaCube_IsSomariaCube().
 *
 * Consumed via #include from item_cane_of_somaria.c (the .c is NOT in the vcxproj).
 */

#ifndef SOMARIA_CUBES_H
#define SOMARIA_CUBES_H

#include "z64.h"
#include "elegy_shell_assets.h"

// ============================================================================
// SUMMON KINDS
// ============================================================================

typedef enum {
    CANE_SUMMON_STATUE = 0,
    CANE_SUMMON_BLOCK = 1,
    CANE_SUMMON_PLATFORM = 2,
    CANE_SUMMON_MAX,
} CaneSummonKind;

// ============================================================================
// PROPERTIES
// ============================================================================

// Each summon kind has its OWN budget; they do not compete for slots. Placing a
// fourth statue evicts the oldest STATUE and leaves your blocks and platforms
// alone. The pool is simply big enough to hold every kind at once.
#define CANE_MAX_STATUES 6
#define CANE_MAX_BLOCKS 3
#define CANE_MAX_PLATFORMS 2
#define SOMARIA_MAX_CUBES (CANE_MAX_STATUES + CANE_MAX_BLOCKS + CANE_MAX_PLATFORMS)

#define SOMARIA_CUBE_SCALE 0.01f // Elegy shells are large models, scale down
#define SOMARIA_SPAWN_FRAMES 20
#define SOMARIA_GRAVITY -2.0f
#define SOMARIA_MIN_VEL_Y -20.0f
#define SOMARIA_CYL_RADIUS 20
#define SOMARIA_CYL_HEIGHT 60
#define SOMARIA_MAX_COLLIDERS 16 // one per pooled statue, plus remote ghosts and slack

// Obj_Oshihiki params: FF00 >= 0x80 makes ObjOshihiki_Init skip the switch-flag
// "kill me on load" branch entirely (there is no scene switch flag behind a
// summoned block), and F == 0 is the small block anyone can push.
#define CANE_BLOCK_PARAMS 0x8000
#define CANE_BLOCK_HALF_WIDTH 30.0f // the pushable block is 60x60x60
#define CANE_BLOCK_HEIGHT 60.0f

// Obj_Lift's slab at scale 0.1: a wide, flat square. The preview ghost matches it.
#define CANE_PLATFORM_RADIUS 60.0f
#define CANE_PLATFORM_HEIGHT 12.0f

// The platform keeps the slab's own texture with env 210,70,70,255 over it so it
// reads as a Somaria construct. Written out at the call site — a multi-value
// #define does not survive MSVC's function-like macro expansion.

// ============================================================================
// STATE MACROS (statue only — it rides on hijacked actor fields)
// ============================================================================

#define SOMARIA_GET_STATE(actor) ((actor)->home.rot.x)
#define SOMARIA_SET_STATE(actor, s) ((actor)->home.rot.x = (s))
#define SOMARIA_GET_TIMER(actor) ((actor)->home.rot.z)
#define SOMARIA_SET_TIMER(actor, t) ((actor)->home.rot.z = (t))
#define SOMARIA_GET_FORM(actor) ((actor)->home.rot.y)
#define SOMARIA_SET_FORM(actor, f) ((actor)->home.rot.y = (f))

typedef enum {
    SOMARIA_STATUE_SPAWNING = 0,
    SOMARIA_STATUE_IDLE = 1,
} SomariaStatueState;

// Flag for switchhook compatibility.
#define ACTOR_FLAG_SWITCHHOOKABLE (1 << 28)

#ifndef BGCHECKFLAG_GROUND
#define BGCHECKFLAG_GROUND 0x0001
#define BGCHECKFLAG_WALL 0x0008
#endif

// ============================================================================
// FUNCTIONS
// ============================================================================

/** Summon at `pos` facing `yaw`. Returns the actor, or NULL if it could not spawn. */
Actor* CaneSummon_Spawn(PlayState* play, CaneSummonKind kind, Vec3f* pos, s16 yaw);

/** Drop dead entries from the pool (actors killed by scene unload, etc.). */
void CaneSummon_CleanupPool(void);

/** Kill every live summon. */
void CaneSummon_KillAll(PlayState* play);

/**
 * Is the aimed placement legal for `kind`? Checks that the spot is clear of the
 * player, of other summons, and is not behind geometry.
 */
u8 CaneSummon_PlacementValid(PlayState* play, CaneSummonKind kind, Vec3f* pos);

/** Draw the translucent placement preview (blue = valid, red = blocked). */
void CaneSummon_DrawPreview(PlayState* play, CaneSummonKind kind, Vec3f* pos, s16 yaw, u8 valid);

/** Is this actor one of ours? (used by the Dominion Rod's target filter) */
u8 SomariaCube_IsSomariaCube(Actor* actor);
u8 SomariaCube_IsSwitchable(Actor* actor);
void SomariaCube_PlaySound(Actor* actor, u16 sfxId);
u8 SomariaCube_GetForm(Actor* actor);

// ============================================================================
// REMOTE STATUES (Harpoon multiplayer)
// ============================================================================
// Visual-only copies of another player's statues, driven entirely by the network
// snapshot: no physics, no switch pressing, no pool slot — they are ghosts. Used
// by soh/Network/Harpoon/HarpoonDummyPlayer.cpp.

Actor* SomariaCube_SpawnRemote(PlayState* play, Vec3f* pos, s16 yaw, u8 form);
void SomariaCube_UpdateRemotePos(Actor* cube, Vec3f* pos, f32 scale, s16 rotY);
u8 SomariaCube_IsRemoteCube(Actor* actor);

#endif // SOMARIA_CUBES_H
