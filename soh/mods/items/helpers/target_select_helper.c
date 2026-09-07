/**
 * target_select_helper.c - Shared remote selector (Skijer's NEI) — OoT backend
 *
 * See target_select_helper.h. OoT specifics: actor lists are `.head`, and
 * Actor_SetColorFilter takes a bare colour index (0 = blue) instead of MM's
 * COLORFILTER_* flag enums.
 */

#include "target_select_helper.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define TARGETSEL_LIST_HEAD(list) ((list).head)

const u8 gTargetSelectDefaultCats[4] = {
    ACTORCAT_ENEMY,
    ACTORCAT_PROP,
    ACTORCAT_CHEST,
    ACTORCAT_NPC,
};

s32 TargetSelect_IsCommonTarget(Actor* actor) {
    return (actor != NULL) && (actor->update != NULL) &&
           ((actor->category == ACTORCAT_ENEMY) || (actor->category == ACTORCAT_PROP) ||
            (actor->category == ACTORCAT_CHEST) || (actor->category == ACTORCAT_NPC));
}

Actor* TargetSelect_ScanCatsFromYaw(PlayState* play, const u8* cats, s32 numCats, TargetSelectFilter filter, f32 range,
                                    f32 minDist, s16 cone, s16 yaw) {
    Player* player;
    Actor* best = NULL;
    s32 bestYawErr;
    s32 i;

    if (play == NULL) {
        return NULL;
    }
    if (cats == NULL) {
        cats = gTargetSelectDefaultCats;
        numCats = TARGETSEL_DEFAULT_CAT_COUNT;
    }
    player = GET_PLAYER(play);
    if (player == NULL) {
        return NULL;
    }
    bestYawErr = cone;

    for (i = 0; i < numCats; i++) {
        Actor* actor = TARGETSEL_LIST_HEAD(play->actorCtx.actorLists[cats[i]]);

        while (actor != NULL) {
            if ((actor->update != NULL) && ((filter == NULL) || filter(actor))) {
                f32 dx = actor->world.pos.x - player->actor.world.pos.x;
                f32 dz = actor->world.pos.z - player->actor.world.pos.z;
                f32 distXZ = sqrtf((dx * dx) + (dz * dz)); // Y ignored on purpose

                if ((distXZ > minDist) && (distXZ <= range)) {
                    // ARGUMENT ORDER IS NOT THE SAME IN BOTH GAMES. OoT declares
                    // Math_Atan2S(f32 x, f32 y) and MM declares Math_Atan2S(f32 y, f32 x) —
                    // literally reversed. To get a world yaw out of an (dx, dz) offset this
                    // must be (dz, dx) here and (dx, dz) in the MM copy of this file. Getting
                    // it backwards mirrors the cone 90 degrees off, which reads in-game as the
                    // selection working "sometimes yes, sometimes no".
                    s32 yawErr = (s16)(Math_Atan2S(dz, dx) - yaw);

                    if (yawErr < 0) {
                        yawErr = -yawErr;
                    }
                    // Ties break toward the smaller yaw error, so "the thing you
                    // are looking at" beats "the thing that happens to be closer".
                    if (yawErr < bestYawErr) {
                        bestYawErr = yawErr;
                        best = actor;
                    }
                }
            }
            actor = actor->next;
        }
    }
    return best;
}

Actor* TargetSelect_ScanCats(PlayState* play, const u8* cats, s32 numCats, TargetSelectFilter filter, f32 range,
                             s16 cone) {
    Player* player = (play != NULL) ? GET_PLAYER(play) : NULL;

    if (player == NULL) {
        return NULL;
    }
    return TargetSelect_ScanCatsFromYaw(play, cats, numCats, filter, range, TARGETSEL_MIN_DIST, cone,
                                        player->actor.shape.rot.y);
}

Actor* TargetSelect_Scan(PlayState* play, TargetSelectFilter filter) {
    return TargetSelect_ScanCats(play, NULL, 0, (filter != NULL) ? filter : TargetSelect_IsCommonTarget,
                                 TARGETSEL_DEFAULT_RANGE, TARGETSEL_DEFAULT_CONE);
}

Actor* TargetSelect_FindNearest(PlayState* play, const u8* cats, s32 numCats, TargetSelectFilter filter, Vec3f* pos,
                                f32 range) {
    f32 rangeSq = range * range;
    s32 i;

    if ((play == NULL) || (pos == NULL)) {
        return NULL;
    }
    if (cats == NULL) {
        cats = gTargetSelectDefaultCats;
        numCats = TARGETSEL_DEFAULT_CAT_COUNT;
    }

    for (i = 0; i < numCats; i++) {
        Actor* actor = TARGETSEL_LIST_HEAD(play->actorCtx.actorLists[cats[i]]);

        while (actor != NULL) {
            if ((actor->update != NULL) && ((filter == NULL) || filter(actor))) {
                f32 dx = actor->world.pos.x - pos->x;
                f32 dy = actor->world.pos.y - pos->y;
                f32 dz = actor->world.pos.z - pos->z;

                if (((dx * dx) + (dy * dy) + (dz * dz)) < rangeSq) {
                    return actor;
                }
            }
            actor = actor->next;
        }
    }
    return NULL;
}

void TargetSelect_Highlight(Actor* actor, s16 duration) {
    if ((actor == NULL) || (actor->update == NULL)) {
        return;
    }
    Actor_SetColorFilter(actor, 0, 255, 0, duration); // 0 = blue in OoT's colour filter
}
