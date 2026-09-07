/**
 * target_select_helper.h - Shared "look at it to pick it" remote selector (Skijer's NEI)
 *
 * Extracted from the Switch Hook's Ultrahand-style live selection, which had an
 * identical private copy in each game's z_arms_hook.c. Any item that needs the
 * player to point at a distant actor and act on it reuses this: Switch Hook,
 * Phantom Hourglass, Dominion Rod, Cane of Somaria...
 *
 * The selection rule is intentionally forgiving: among the actors that pass the
 * filter, the winner is the one whose XZ direction from Link best matches the
 * direction Link is FACING. Height is ignored, so you never have to line up a
 * vertical angle — you just look at the thing.
 */

#ifndef TARGET_SELECT_HELPER_H
#define TARGET_SELECT_HELPER_H

#include "z64.h"

/** Return non-zero to let `actor` be selectable. */
typedef s32 (*TargetSelectFilter)(struct Actor* actor);

#define TARGETSEL_DEFAULT_RANGE 520.0f // longshot reach (20 speed * 26 frames)
#define TARGETSEL_DEFAULT_CONE 0x1800  // +-33.75 deg around Link's facing
#define TARGETSEL_MIN_DIST 30.0f       // ignore whatever is basically on top of Link

/** The categories the Switch Hook scans; the default set when `cats` is NULL. */
extern const u8 gTargetSelectDefaultCats[4];
#define TARGETSEL_DEFAULT_CAT_COUNT 4

/**
 * The Switch Hook's own filter, exported so other items can reuse the same
 * notion of "a loose object you may manipulate": enemies, props, chests, NPCs.
 * Bosses, the player, and scene/background actors never qualify.
 */
s32 TargetSelect_IsCommonTarget(struct Actor* actor);

/**
 * Live selection scan: the best-aligned actor inside `cone` and `range`.
 * @param cats     array of ACTORCAT_* to scan, or NULL for gTargetSelectDefaultCats
 * @param numCats  entries in `cats` (ignored when cats is NULL)
 * @param filter   extra predicate, or NULL to accept everything in those categories
 * @return the selected actor, or NULL
 */
struct Actor* TargetSelect_ScanCats(PlayState* play, const u8* cats, s32 numCats, TargetSelectFilter filter, f32 range,
                                    s16 cone);

/**
 * TargetSelect_ScanCats with the cone centred on `yaw` instead of Link's facing (first-person aim),
 * and its own near limit. Pass TARGETSEL_MIN_DIST for the default; something smaller when the item
 * must be able to pick what is right at Link's feet, like a block he has just set down.
 */
struct Actor* TargetSelect_ScanCatsFromYaw(PlayState* play, const u8* cats, s32 numCats, TargetSelectFilter filter,
                                           f32 range, f32 minDist, s16 cone, s16 yaw);

/** Convenience wrapper over TargetSelect_ScanCats with the default categories/range/cone. */
struct Actor* TargetSelect_Scan(PlayState* play, TargetSelectFilter filter);

/**
 * Nearest passing actor within `range` of `pos` (proximity, not aim). Used to
 * resolve a hit before a projectile physically reaches the actor and breaks it.
 */
struct Actor* TargetSelect_FindNearest(PlayState* play, const u8* cats, s32 numCats, TargetSelectFilter filter,
                                       Vec3f* pos, f32 range);

/**
 * Tint the current selection so the player can see it. `duration` is in frames;
 * pass a short value (~4) when re-applied every frame, longer for a one-shot.
 * The colour is the per-game blue used by the Switch Hook.
 */
void TargetSelect_Highlight(struct Actor* actor, s16 duration);

#endif // TARGET_SELECT_HELPER_H
