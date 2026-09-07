/**
 * snap.c - Pictograph Box engine (Skijer's NEI). Ported from Majora's Mask: mm/src/code/z_snap.c.
 *
 * Faithful to MM's validation + flag layout. Port substitutions (all real OoT functions — no
 * invented code):
 *   - OLib_Vec3fDist                              -> Math_Vec3f_DistXYZ
 *   - Actor_GetProjectedPos + PROJECTED_TO_SCREEN -> Actor_GetScreenPos (OoT gives screen x/y)
 *   - gSaveContext.save.saveInfo.pictoFlags0/1    -> Nei_Save()->pictoFlags0/1 (MM-format NEI save)
 *   - per-actor PictoActor.validationFunc         -> central actor->id subject table (below)
 * BgCheck_ProjectileLineTest, CollisionCheck_LineOCCheck, Camera_GetCamDirPitch/Yaw, BINANG_SUB,
 * GET_ACTIVE_CAM are identical to MM and used as-is.
 *
 * #included into custom_items.c (host TU); the CMake mods glob is *.cpp/*.h only, not *.c.
 */
#include "snap.h"
#include "../../nei_save.h" // Skijer's NEI

// === Flag bit-ops (MM Snap_SetFlag/UnsetFlag/CheckFlag, retargeted to the NEI save) ===

void Snap_SetFlag(s32 flag) {
    if (flag < 0x20) {
        Nei_Save()->pictoFlags0 |= (1 << flag);
    } else {
        flag &= 0x1F;
        Nei_Save()->pictoFlags1 |= (1 << flag);
    }
}

void Snap_UnsetFlag(s32 flag) {
    if (flag < 0x20) {
        Nei_Save()->pictoFlags0 &= ~(1 << flag);
    } else {
        flag &= 0x1F;
        Nei_Save()->pictoFlags1 &= ~(1 << flag);
    }
}

u32 Snap_CheckFlag(s32 flag) {
    if (flag < 0x20) {
        return Nei_Save()->pictoFlags0 & (1 << flag);
    } else {
        flag &= 0x1F;
        return Nei_Save()->pictoFlags1 & (1 << flag);
    }
}

// === Validation (verbatim MM logic; MM helpers -> OoT equivalents) ===

s32 Snap_ValidatePictograph(PlayState* play, Actor* actor, s32 flag, Vec3f* pos, Vec3s* rot, f32 distanceMin,
                            f32 distanceMax, s16 angleRange) {
    Camera* camera = GET_ACTIVE_CAM(play);
    Vec3f projectedPos;
    CollisionPoly* poly;
    Actor* actors[2];
    s32 bgId;
    s16 x;
    s16 y;
    s16 sx;
    s16 sy;
    f32 distance;
    s32 ret = 0;

    // Distance (MM: OLib_Vec3fDist)
    distance = Math_Vec3f_DistXYZ(pos, &camera->eye);
    if ((distance < distanceMin) || (distanceMax < distance)) {
        Snap_SetFlag(PICTO_VALID_BAD_DISTANCE);
        ret = PICTO_VALID_BAD_DISTANCE;
    }

    // Facing the camera within angleRange (-1 = any)
    x = ABS((s16)(Camera_GetCamDirPitch(camera) + rot->x));
    y = ABS((s16)(Camera_GetCamDirYaw(camera) - BINANG_SUB(rot->y, 0x7FFF)));
    if ((0 < angleRange) && ((angleRange < x) || (angleRange < y))) {
        Snap_SetFlag(PICTO_VALID_BAD_ANGLE);
        ret |= PICTO_VALID_BAD_ANGLE;
    }

    // Inside the capture region (MM: Actor_GetProjectedPos + PROJECTED_TO_SCREEN; OoT: Actor_GetScreenPos)
    Actor_GetScreenPos(play, actor, &sx, &sy);
    sx -= PICTO_VALID_TOPLEFT_X;
    sy -= PICTO_VALID_TOPLEFT_Y;
    if ((sx < 0) || (sx > PICTO_VALID_WIDTH) || (sy < 0) || (sy > PICTO_VALID_HEIGHT)) {
        Snap_SetFlag(PICTO_VALID_NOT_IN_VIEW);
        ret |= PICTO_VALID_NOT_IN_VIEW;
    }

    // Not obscured by bg collision
    if (BgCheck_ProjectileLineTest(&play->colCtx, pos, &camera->eye, &projectedPos, &poly, true, true, true, true,
                                   &bgId)) {
        Snap_SetFlag(PICTO_VALID_BEHIND_BG);
        ret |= PICTO_VALID_BEHIND_BG;
    }

    // Not obscured by actor collision (exclude the subject + the player)
    actors[0] = actor;
    actors[1] = &GET_PLAYER(play)->actor;
    if (CollisionCheck_LineOCCheck(play, &play->colChkCtx, pos, &camera->eye, actors, 2)) {
        Snap_SetFlag(PICTO_VALID_BEHIND_COLLISION);
        ret |= PICTO_VALID_BEHIND_COLLISION;
    }

    if (ret == 0) {
        Snap_SetFlag(flag);
    }
    return ret;
}

// === Subject table (OoT port of MM's per-actor PictoActor.validationFunc) ===
// One row per (actor id [+ optional exact params], PICTO_VALID_* flag, distance window, angleRange).
// Rows may share an actor id: Lulu validates 3 body-part flags; the pirates validate good + too-far.
// pos = actor->focus.pos, rot = actor->shape.rot (what MM passes for these subjects). Distance/angle
// windows are MM's where known (Tingle/Scarecrow/Deku King/Pirate/Lulu); Monkey/Big Octo use sane
// defaults (MM validates those via scene-specific funcs we don't replicate 1:1).
#define PICTO_PARAMS_ANY ((s16)-1)

typedef struct {
    s16 actorId;
    s16 params; // PICTO_PARAMS_ANY, or an exact actor->params match
    u8 flag;    // PICTO_VALID_*
    f32 distMin;
    f32 distMax;
    s16 angleRange;
} PictoSubject;

static const PictoSubject sPictoSubjects[] = {
    // Monkey -> En_Skj
    { ACTOR_EN_SKJ, PICTO_PARAMS_ANY, PICTO_VALID_MONKEY, 10.0f, 400.0f, 0x4000 },
    // Big Octo -> En_Bigokuta
    { ACTOR_EN_BIGOKUTA, PICTO_PARAMS_ANY, PICTO_VALID_BIG_OCTO, 10.0f, 800.0f, -1 },
    // Tingle -> Kokiri kids / Mido / Saria, + Kokiri-Forest shop Ossan (params 0 only)
    { ACTOR_EN_KO, PICTO_PARAMS_ANY, PICTO_VALID_TINGLE, 10.0f, 400.0f, 0x4000 },
    { ACTOR_EN_MD, PICTO_PARAMS_ANY, PICTO_VALID_TINGLE, 10.0f, 400.0f, 0x4000 },
    { ACTOR_EN_SA, PICTO_PARAMS_ANY, PICTO_VALID_TINGLE, 10.0f, 400.0f, 0x4000 },
    { ACTOR_EN_OSSAN, 0, PICTO_VALID_TINGLE, 10.0f, 400.0f, 0x4000 },
    // Deku King -> Owl / Deku Tree Sprout
    { ACTOR_EN_OWL, PICTO_PARAMS_ANY, PICTO_VALID_DEKU_KING, 120.0f, 480.0f, 0x38E3 },
    { ACTOR_OBJ_DEKUJR, PICTO_PARAMS_ANY, PICTO_VALID_DEKU_KING, 120.0f, 480.0f, 0x38E3 },
    // Lulu -> child + adult Ruto. MM uses separate head/arm body-part positions; we validate at
    // focus.pos with MM's windows (head 10-300 any; arms 50-160 0x3000) and set each flag that passes.
    { ACTOR_EN_RU1, PICTO_PARAMS_ANY, PICTO_VALID_LULU_HEAD, 10.0f, 300.0f, -1 },
    { ACTOR_EN_RU1, PICTO_PARAMS_ANY, PICTO_VALID_LULU_RIGHT_ARM, 50.0f, 160.0f, 0x3000 },
    { ACTOR_EN_RU1, PICTO_PARAMS_ANY, PICTO_VALID_LULU_LEFT_ARM, 50.0f, 160.0f, 0x3000 },
    { ACTOR_EN_RU2, PICTO_PARAMS_ANY, PICTO_VALID_LULU_HEAD, 10.0f, 300.0f, -1 },
    { ACTOR_EN_RU2, PICTO_PARAMS_ANY, PICTO_VALID_LULU_RIGHT_ARM, 50.0f, 160.0f, 0x3000 },
    { ACTOR_EN_RU2, PICTO_PARAMS_ANY, PICTO_VALID_LULU_LEFT_ARM, 50.0f, 160.0f, 0x3000 },
    // Scarecrow -> Pierre / spawn / Bonooru
    { ACTOR_EN_KAKASI, PICTO_PARAMS_ANY, PICTO_VALID_SCARECROW, 280.0f, 1800.0f, -1 },
    { ACTOR_EN_KAKASI2, PICTO_PARAMS_ANY, PICTO_VALID_SCARECROW, 280.0f, 1800.0f, -1 },
    { ACTOR_EN_KAKASI3, PICTO_PARAMS_ANY, PICTO_VALID_SCARECROW, 280.0f, 1800.0f, -1 },
    // Pirates -> Gerudos. Good (10-400) + too-far (10-1200).
    { ACTOR_EN_GE1, PICTO_PARAMS_ANY, PICTO_VALID_PIRATE_GOOD, 10.0f, 400.0f, -1 },
    { ACTOR_EN_GE1, PICTO_PARAMS_ANY, PICTO_VALID_PIRATE_TOO_FAR, 10.0f, 1200.0f, -1 },
    { ACTOR_EN_GELDB, PICTO_PARAMS_ANY, PICTO_VALID_PIRATE_GOOD, 10.0f, 400.0f, -1 },
    { ACTOR_EN_GELDB, PICTO_PARAMS_ANY, PICTO_VALID_PIRATE_TOO_FAR, 10.0f, 1200.0f, -1 },
    { ACTOR_EN_GE2, PICTO_PARAMS_ANY, PICTO_VALID_PIRATE_GOOD, 10.0f, 400.0f, -1 },
    { ACTOR_EN_GE2, PICTO_PARAMS_ANY, PICTO_VALID_PIRATE_TOO_FAR, 10.0f, 1200.0f, -1 },
    { ACTOR_EN_GE3, PICTO_PARAMS_ANY, PICTO_VALID_PIRATE_GOOD, 10.0f, 400.0f, -1 },
    { ACTOR_EN_GE3, PICTO_PARAMS_ANY, PICTO_VALID_PIRATE_TOO_FAR, 10.0f, 1200.0f, -1 },
};

// MM clears both registers, then re-validates every in-view subject. Keyed by actor->id (+ params).
s32 Snap_RecordPictographedActors(PlayState* play) {
    Actor* actor;
    s32 category;
    s32 validCount = 0;
    size_t i;

    Nei_Save()->pictoFlags0 = 0;
    Nei_Save()->pictoFlags1 = 0;

    for (category = 0; category < ACTORCAT_MAX; category++) {
        for (actor = play->actorCtx.actorLists[category].head; actor != NULL; actor = actor->next) {
            for (i = 0; i < ARRAY_COUNT(sPictoSubjects); i++) {
                const PictoSubject* subject = &sPictoSubjects[i];

                if (subject->actorId != actor->id) {
                    continue;
                }
                if ((subject->params != PICTO_PARAMS_ANY) && (subject->params != actor->params)) {
                    continue;
                }
                if (Snap_ValidatePictograph(play, actor, subject->flag, &actor->focus.pos, &actor->shape.rot,
                                            subject->distMin, subject->distMax, subject->angleRange) == 0) {
                    validCount++;
                }
            }
        }
    }
    return validCount;
}
