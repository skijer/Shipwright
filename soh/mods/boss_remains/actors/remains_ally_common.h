/**
 * remains_ally_common.h - Shared behavior helpers for the boss-remains summon allies (SoH port).
 *
 * Each boss remains (Odolwa/Goht/Gyorg/Twinmold) summons a small FRIENDLY ally actor.
 * Those actors are authored one-per-boss in this same actors/ folder and unity-#included
 * into boss_remains.cpp; this header exposes the three behaviors they all share: pick the
 * nearest enemy to attack, home toward a world position, and idle-follow the real player
 * when there is nothing to fight.
 *
 * Declared with C linkage so the definitions in remains_ally_common.c (also
 * unity-#included into boss_remains.cpp's extern "C" block) match. Not in vcxproj.
 *
 * Ported MM -> OoT/SoH. All three are implemented with verified SoH engine symbols only:
 *   Actor_WorldDistXZToPoint / Actor_WorldDistXZToActor  (z_actor.c)
 *   Actor_WorldYawTowardPoint / Actor_WorldYawTowardActor (z_actor.c)
 *   Math_SmoothStepToS (z_lib.c) / Actor_MoveXZGravity / Actor_UpdateBgCheckInfo
 *   GET_PLAYER + actorCtx.actorLists[cat].head / Actor.next (z64.h / z64actor.h)
 */

#ifndef REMAINS_ALLY_COMMON_H
#define REMAINS_ALLY_COMMON_H

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// Nearest live enemy (scans play->actorCtx.actorLists[ACTORCAT_ENEMY] and
// ACTORCAT_BOSS) to 'from', within maxDist (XZ). Skips actors that are being
// killed (Actor_Kill nulls update) or already dead (colChkInfo.health == 0).
// Returns NULL if none in range or on NULL args.
Actor* RemainsAlly_FindNearestEnemy(PlayState* play, Vec3f* from, f32 maxDist);

// Smoothly steer 'actor' yaw toward target world pos and step forward at 'speed'
// (Actor_MoveXZGravity + Actor_UpdateBgCheckInfo). Returns the remaining yaw
// error (targetYaw - actor->world.rot.y) after the step. Used by ground allies.
s16 RemainsAlly_HomeTowardPos(PlayState* play, Actor* actor, Vec3f* targetPos, f32 speed);

// Idle-follow the real player: when farther than 'followDist' (XZ), home toward
// the player at 'speed'; when closer, face the player, stop, and settle to the
// floor. Call this each frame the ally has no enemy target.
void RemainsAlly_FollowPlayer(PlayState* play, Actor* actor, f32 followDist, f32 speed);

#ifdef __cplusplus
}
#endif

#endif // REMAINS_ALLY_COMMON_H
