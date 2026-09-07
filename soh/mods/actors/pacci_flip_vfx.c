/**
 * pacci_flip_vfx.c - Cane of Pacci Flip cast visual. Skijer's NEI
 */

#include "pacci_flip_vfx.h"
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/object_dy_obj/object_dy_obj.h"
#include <math.h>

#define PACCI_FLIP_VFX_MAX_FRAMES 120
#define PACCI_FLIP_VFX_REACH_FRAMES 7
#define PACCI_FLIP_VFX_RELEASE_FRAMES 10
#define PACCI_FLIP_VFX_RIBBON_POINTS 13
#define PACCI_FLIP_VFX_GRIP_POINTS 8

typedef struct {
    Actor* target;
    s16 age;
    s16 releaseTimer;
    s16 startRotZ;
    f32 grabHeight;
    Vec3f lastTargetPos;
    u8 releasing;
    u8 persistent;
    u8 hasLastTargetPos;
} PacciFlipVfxState;

static PacciFlipVfxState sFlipVfx = { 0 };

static Color_RGBA8 sPacciPrim = { 255, 255, 190, 255 };
static Color_RGBA8 sPacciEnv = { 255, 150, 0, 255 };

static void PacciFlipVfx_GetHandPos(Player* player, Vec3f* pos) {
    Vec3f forearm = player->bodyPartsPos[PLAYER_BODYPART_R_FOREARM];
    Vec3f hand = player->bodyPartsPos[PLAYER_BODYPART_R_HAND];
    f32 dx = hand.x - forearm.x;
    f32 dy = hand.y - forearm.y;
    f32 dz = hand.z - forearm.z;
    f32 length = sqrtf((dx * dx) + (dy * dy) + (dz * dz));

    *pos = hand;
    if (length > 0.001f) {
        pos->x += (dx / length) * 16.0f;
        pos->y += (dy / length) * 16.0f;
        pos->z += (dz / length) * 16.0f;
    }
}

static void PacciFlipVfx_GetTargetPos(Vec3f* pos) {
    *pos = sFlipVfx.target->world.pos;
    pos->y += sFlipVfx.grabHeight;
}

static void PacciFlipVfx_GetCurvePoint(Vec3f* pos, Vec3f* start, Vec3f* end, f32 t, f32 sideOffset, u32 frame) {
    f32 inv = 1.0f - t;
    f32 dx = end->x - start->x;
    f32 dz = end->z - start->z;
    f32 xzLength = sqrtf((dx * dx) + (dz * dz));
    Vec3f control;
    f32 sideX = 1.0f;
    f32 sideZ = 0.0f;
    s16 archAngle = (s16)(t * 0x7FFF);
    s16 waveAngle = (s16)((frame * 0x1200) + (s32)(t * 0x6000));

    if (xzLength > 0.001f) {
        sideX = dz / xzLength;
        sideZ = -dx / xzLength;
    }

    control.x = (start->x + end->x) * 0.5f;
    control.y = ((start->y + end->y) * 0.5f) + 45.0f;
    control.z = (start->z + end->z) * 0.5f;

    pos->x = (inv * inv * start->x) + (2.0f * inv * t * control.x) + (t * t * end->x);
    pos->y = (inv * inv * start->y) + (2.0f * inv * t * control.y) + (t * t * end->y);
    pos->z = (inv * inv * start->z) + (2.0f * inv * t * control.z) + (t * t * end->z);

    sideOffset *= Math_SinS(archAngle);
    sideOffset += Math_SinS(waveAngle) * 2.5f * Math_SinS(archAngle);
    pos->x += sideX * sideOffset;
    pos->z += sideZ * sideOffset;
}

static void PacciFlipVfx_SpawnSparkles(PlayState* play, Vec3f* pos, f32 spread, u8 count, s16 life) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };

    for (u8 i = 0; i < count; i++) {
        Vec3f spark = *pos;
        spark.x += Rand_CenteredFloat(spread);
        spark.y += Rand_CenteredFloat(spread);
        spark.z += Rand_CenteredFloat(spread);
        EffectSsKiraKira_SpawnDispersed(play, &spark, &zero, &zero, &sPacciPrim, &sPacciEnv, 700, life);
    }
}

static void PacciFlipVfx_SpawnCastBurst(PlayState* play, Vec3f* hand, Vec3f* target) {
    PacciFlipVfx_SpawnSparkles(play, hand, 12.0f, 8, 16);
    PacciFlipVfx_SpawnSparkles(play, target, 42.0f, 20, 18);
}

static void PacciFlipVfx_DrawSprite(PlayState* play, Gfx** gfxP, Vec3f* pos, f32 scale, const void* dList) {
    Gfx* gfx = *gfxP;

    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    gSPMatrix(gfx++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(gfx++, dList);
    *gfxP = gfx;
}

static void PacciFlipVfx_DrawBeamSegment(PlayState* play, Gfx** gfxP, Vec3f* start, Vec3f* end, f32 radius, u8 r, u8 g,
                                         u8 b, u8 alpha) {
    f32 dx = end->x - start->x;
    f32 dy = end->y - start->y;
    f32 dz = end->z - start->z;
    f32 xzLength = sqrtf((dx * dx) + (dz * dz));
    f32 length = sqrtf((dx * dx) + (dy * dy) + (dz * dz));
    s16 yaw;
    Gfx* gfx = *gfxP;

    if (length < 0.5f) {
        return;
    }

    yaw = Math_Vec3f_Yaw(start, end);
    Matrix_Translate(start->x, start->y, start->z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(yaw), MTXMODE_APPLY);
    Matrix_RotateX(atan2f(xzLength, dy), MTXMODE_APPLY);
    Matrix_Scale(radius / 1200.0f, length / 8000.0f, radius / 1200.0f, MTXMODE_APPLY);
    gSPMatrix(gfx++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPSetPrimColor(gfx++, 0, 0x80, r, g, b, alpha);
    gDPSetEnvColor(gfx++, r / 3, g / 3, b / 3, alpha);
    gSPDisplayList(gfx++, gGreatFairySpiralBeamDL);
    *gfxP = gfx;
}

static void PacciFlipVfx_DrawBeamPass(PlayState* play, Gfx** gfxP, Vec3f* hand, Vec3f* target, f32 reach, f32 width,
                                      f32 sideBias, u8 r, u8 g, u8 b, u8 alpha) {
    Vec3f centers[PACCI_FLIP_VFX_RIBBON_POINTS];

    if (sFlipVfx.persistent) {
        centers[0] = *hand;
        centers[1].x = hand->x + ((target->x - hand->x) * reach);
        centers[1].y = hand->y + ((target->y - hand->y) * reach);
        centers[1].z = hand->z + ((target->z - hand->z) * reach);
        PacciFlipVfx_DrawBeamSegment(play, gfxP, &centers[0], &centers[1], width, r, g, b, alpha);
        return;
    }
    for (u8 i = 0; i < PACCI_FLIP_VFX_RIBBON_POINTS; i++) {
        f32 t = ((f32)i / (PACCI_FLIP_VFX_RIBBON_POINTS - 1)) * reach;
        PacciFlipVfx_GetCurvePoint(&centers[i], hand, target, t, sideBias, play->gameplayFrames);
    }
    for (u8 i = 0; i < PACCI_FLIP_VFX_RIBBON_POINTS - 1; i++) {
        f32 edge = Math_SinS((s16)((i * 0x7FFF) / (PACCI_FLIP_VFX_RIBBON_POINTS - 2)));
        PacciFlipVfx_DrawBeamSegment(play, gfxP, &centers[i], &centers[i + 1], width * (0.65f + (edge * 0.35f)), r, g,
                                     b, alpha);
    }
}

u8 PacciFlipVfx_IsActive(void) {
    return sFlipVfx.target != NULL;
}

void PacciFlipVfx_Stop(void) {
    sFlipVfx.target = NULL;
    sFlipVfx.age = 0;
    sFlipVfx.releaseTimer = 0;
    sFlipVfx.releasing = 0;
    sFlipVfx.persistent = 0;
    sFlipVfx.hasLastTargetPos = 0;
}

static void PacciFlipVfx_StartInternal(PlayState* play, Player* player, Actor* target, u8 persistent) {
    Vec3f hand;
    Vec3f targetPos;
    f32 focusHeight;

    if (target == NULL) {
        return;
    }

    PacciFlipVfx_Stop();
    sFlipVfx.target = target;
    sFlipVfx.persistent = persistent;
    sFlipVfx.startRotZ = target->shape.rot.z;
    focusHeight = target->focus.pos.y - target->world.pos.y;
    sFlipVfx.grabHeight = (focusHeight >= 10.0f && focusHeight <= 120.0f) ? focusHeight : 30.0f;

    PacciFlipVfx_GetHandPos(player, &hand);
    PacciFlipVfx_GetTargetPos(&targetPos);
    sFlipVfx.lastTargetPos = targetPos;
    sFlipVfx.hasLastTargetPos = 1;
    PacciFlipVfx_SpawnCastBurst(play, &hand, &targetPos);
}

void PacciFlipVfx_Start(PlayState* play, Player* player, Actor* target) {
    PacciFlipVfx_StartInternal(play, player, target, 0);
}

void PacciFlipVfx_StartLift(PlayState* play, Player* player, Actor* target) {
    PacciFlipVfx_StartInternal(play, player, target, 1);
}

void PacciFlipVfx_Release(void) {
    if (PacciFlipVfx_IsActive() && !sFlipVfx.releasing) {
        sFlipVfx.persistent = 0;
        sFlipVfx.releasing = 1;
        sFlipVfx.releaseTimer = PACCI_FLIP_VFX_RELEASE_FRAMES;
    }
}

void PacciFlipVfx_Update(PlayState* play, Player* player) {
    Vec3f hand;
    Vec3f target;

    if (!PacciFlipVfx_IsActive()) {
        return;
    }
    if (sFlipVfx.target->update == NULL) {
        PacciFlipVfx_Stop();
        return;
    }

    sFlipVfx.age++;
    if (!sFlipVfx.persistent && !sFlipVfx.releasing &&
        ((sFlipVfx.age >= PACCI_FLIP_VFX_MAX_FRAMES) ||
         ((sFlipVfx.age > PACCI_FLIP_VFX_REACH_FRAMES) &&
          (sFlipVfx.target->bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_GROUND_TOUCH))))) {
        sFlipVfx.releasing = 1;
        sFlipVfx.releaseTimer = PACCI_FLIP_VFX_RELEASE_FRAMES;
        PacciFlipVfx_GetTargetPos(&target);
        PacciFlipVfx_SpawnSparkles(play, &target, 28.0f, 10, 14);
    }

    if (sFlipVfx.releasing) {
        if (sFlipVfx.releaseTimer > 0) {
            sFlipVfx.releaseTimer--;
        } else {
            PacciFlipVfx_Stop();
            return;
        }
    }

    PacciFlipVfx_GetHandPos(player, &hand);
    PacciFlipVfx_GetTargetPos(&target);

    if (sFlipVfx.hasLastTargetPos) {
        Vec3f targetTrail;
        targetTrail.x = (sFlipVfx.lastTargetPos.x + target.x) * 0.5f;
        targetTrail.y = (sFlipVfx.lastTargetPos.y + target.y) * 0.5f;
        targetTrail.z = (sFlipVfx.lastTargetPos.z + target.z) * 0.5f;
        PacciFlipVfx_SpawnSparkles(play, &targetTrail, 8.0f, 1, 10);
    }
    sFlipVfx.lastTargetPos = target;
    sFlipVfx.hasLastTargetPos = 1;

    if ((sFlipVfx.age & 1) == 0) {
        Vec3f trail;
        f32 t = Rand_ZeroOne();
        PacciFlipVfx_GetCurvePoint(&trail, &hand, &target, t, Rand_CenteredFloat(8.0f), play->gameplayFrames);
        PacciFlipVfx_SpawnSparkles(play, &trail, 4.0f, 1, 8);
        PacciFlipVfx_SpawnSparkles(play, &hand, 5.0f, 1, 8);
        PacciFlipVfx_SpawnSparkles(play, &target, 12.0f, 1, 10);
    }
}

void PacciFlipVfx_Draw(PlayState* play, Player* player) {
    Vec3f hand;
    Vec3f target;
    Vec3f point;
    f32 reach;
    f32 pulse;
    f32 gripRadius;
    f32 dx;
    f32 dz;
    f32 xzLength;
    f32 sideX = 1.0f;
    f32 sideZ = 0.0f;
    u8 alpha = 255;

    if (!PacciFlipVfx_IsActive()) {
        return;
    }

    PacciFlipVfx_GetHandPos(player, &hand);
    PacciFlipVfx_GetTargetPos(&target);
    reach = (sFlipVfx.age < PACCI_FLIP_VFX_REACH_FRAMES) ? (f32)sFlipVfx.age / (f32)PACCI_FLIP_VFX_REACH_FRAMES : 1.0f;
    if (sFlipVfx.releasing) {
        alpha = (u8)((255 * sFlipVfx.releaseTimer) / PACCI_FLIP_VFX_RELEASE_FRAMES);
    }
    pulse = 1.0f + (Math_SinS((s16)(play->gameplayFrames * 0x1000)) * 0.18f);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, play->gameplayFrames * 2, 0, 0x20, 0x40, 1,
                                  play->gameplayFrames, play->gameplayFrames * -8, 0x10, 0x10, 2, 0, 1, -8));
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 190, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 150, 0, alpha);

    PacciFlipVfx_DrawSprite(play, &POLY_XLU_DISP, &hand, 0.030f * pulse, gEffFlash1DL);
    PacciFlipVfx_DrawSprite(play, &POLY_XLU_DISP, &hand, 0.016f, gEffSparklesDL);

    PacciFlipVfx_DrawBeamPass(play, &POLY_XLU_DISP, &hand, &target, reach, 8.0f * pulse, 0.0f, 25, 210, 255,
                              (u8)(alpha * 0.78f));
    PacciFlipVfx_DrawBeamPass(play, &POLY_XLU_DISP, &hand, &target, reach, 3.8f * pulse, 2.5f, 255, 190, 25, alpha);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 190, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 150, 0, alpha);

    dx = target.x - hand.x;
    dz = target.z - hand.z;
    xzLength = sqrtf((dx * dx) + (dz * dz));
    if (xzLength > 0.001f) {
        sideX = dz / xzLength;
        sideZ = -dx / xzLength;
    }
    gripRadius = 42.0f - (18.0f * reach);
    for (u8 i = 0; i < PACCI_FLIP_VFX_GRIP_POINTS; i++) {
        s16 angle =
            (s16)((i * (0x10000 / PACCI_FLIP_VFX_GRIP_POINTS)) + (sFlipVfx.target->shape.rot.z - sFlipVfx.startRotZ));
        f32 side = Math_CosS(angle) * gripRadius;
        f32 up = Math_SinS(angle) * gripRadius;
        point.x = target.x + (sideX * side);
        point.y = target.y + up;
        point.z = target.z + (sideZ * side);
        PacciFlipVfx_DrawSprite(play, &POLY_XLU_DISP, &point, 0.014f * pulse, gEffSparklesDL);
    }
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetCombineLERP(POLY_XLU_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, TEXEL0, 0,
                      PRIMITIVE, 0);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 30, 225, 255, alpha);
    PacciFlipVfx_DrawSprite(play, &POLY_XLU_DISP, &target, 0.065f * pulse, gLensFlareRingDL);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 220, 80, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 120, 0, alpha);
    PacciFlipVfx_DrawSprite(play, &POLY_XLU_DISP, &target, 0.034f * pulse, gEffFlash2DL);

    CLOSE_DISPS(play->state.gfxCtx);
}
