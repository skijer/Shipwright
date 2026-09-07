/**
 * object_tornado.cpp - Shared wind-cone visual (Skijer's NEI)
 *
 * See object_tornado.h. Mesh + material live in assets/custom/objects/object_nei_tornado;
 * the material is loaded separately from the triangles so the caller's primitive colour can be
 * slotted in between them (the material carries Fast64's default white prim).
 *
 * This is a .cpp purely so the mods/*.cpp glob compiles it as its OWN translation unit — the
 * code is plain C. It must stay OUT of the z_player.c unity build, which is already at the size
 * where adding to it resurrects `z_player.obj : LNK1179 duplicate COMDAT`.
 */
// OPEN_DISPS / CLOSE_DISPS redeclare these two symbols inline at each call site; in a C++ TU that
// in-block redeclaration takes C++ linkage unless a C declaration exists at file scope. Force the
// C symbols (same trick as spiritual_stones.cpp / garo_form.cpp) so the macro's redeclaration
// matches and the link succeeds.
// At GLOBAL scope, before the extern "C" block below: z64.h pulls in <memory> under C++, and a
// template cannot have C linkage. Getting it in first makes the include inside that block a no-op.
#include "z64.h"

extern "C" {
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
}

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "object_tornado.h"
#include "../helpers/fx_helper.h"

u8 ResourceMgr_FileExists(const char* resName);
Gfx* ResourceMgr_LoadGfxByName(const char* path);
}

#define TORNADO_MAT_PATH "__OTR__objects/object_nei_tornado/mat_tornado_f3dlite_tornado"
#define TORNADO_TRI_PATH "__OTR__objects/object_nei_tornado/tornado_mesh_tri_0"

static Gfx* sTornadoMatDL = NULL;
static Gfx* sTornadoTriDL = NULL;
static u8 sTornadoTried = 0;

// Returns 1 once both DLs are resident. A missing archive just disables the effect.
static s32 Tornado_LoadDLs(void) {
    if (!sTornadoTried) {
        sTornadoTried = 1;
        if (ResourceMgr_FileExists(TORNADO_MAT_PATH) && ResourceMgr_FileExists(TORNADO_TRI_PATH)) {
            sTornadoMatDL = ResourceMgr_LoadGfxByName(TORNADO_MAT_PATH);
            sTornadoTriDL = ResourceMgr_LoadGfxByName(TORNADO_TRI_PATH);
        }
    }
    return (sTornadoMatDL != NULL) && (sTornadoTriDL != NULL);
}

// Unit vector the cone points along, matching how the gust jar derives its nozzle offset
// (x = sin(yaw)cos(pitch), y = -sin(pitch), z = cos(yaw)cos(pitch)).
void Tornado_GetAxis(s16 yaw, s16 pitch, Vec3f* axis) {
    f32 cp = Math_CosS(pitch);

    axis->x = Math_SinS(yaw) * cp;
    axis->y = -Math_SinS(pitch);
    axis->z = Math_CosS(yaw) * cp;
}

void Tornado_Draw(PlayState* play, const TornadoParams* p) {
    if (!Tornado_LoadDLs()) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    // The matrix below is built with MTXMODE_NEW, so it REPLACES whatever the caller had.
    // Push/pop is not decorative: a caller that draws the cone before its own model — the
    // Rito's updraft goes up before the form skeleton — otherwise inherits the cone's
    // transform and comes out the wrong size.
    Matrix_Push();

    // Model +Y must land on the aim direction. Composing Ry(yaw) * Rx(a) * Ry(spin) sends
    // (0,1,0) to (sin(yaw)sin(a), cos(a), cos(yaw)sin(a)), so a = 90 deg + pitch gives exactly
    // the axis above. The trailing Ry(spin) then rolls the cone about its own axis.
    Matrix_Translate(p->origin.x, p->origin.y, p->origin.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(p->yaw), MTXMODE_APPLY);
    Matrix_RotateX(DEG_TO_RAD(90.0f) + BINANG_TO_RAD(p->pitch), MTXMODE_APPLY);
    Matrix_RotateY(BINANG_TO_RAD(p->spin), MTXMODE_APPLY);
    Matrix_Scale(p->radius / TORNADO_MODEL_RADIUS, p->length / TORNADO_MODEL_LENGTH, p->radius / TORNADO_MODEL_RADIUS,
                 MTXMODE_APPLY);

    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    gSPDisplayList(POLY_XLU_DISP++, sTornadoMatDL);
    // Texture is intensity-only, so this is what actually colours the tornado.
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, p->color.r, p->color.g, p->color.b, p->color.a);
    // Slide the streaks over the surface. Gfx_TexScroll retiles tile 0 with an ULS/ULT offset in
    // quarter-texels; the sampled texel is (vertex coord - offset), so a RISING offset makes the
    // pattern travel toward +T, i.e. out of the tip and toward the mouth. Both axes wrap in the
    // material and the texture's first/last rows are fully transparent, so this never seams.
    gSPDisplayList(POLY_XLU_DISP++, Gfx_TexScroll(play->state.gfxCtx, (u32)p->scrollS, (u32)p->scrollT,
                                                  TORNADO_TEX_WIDTH, TORNADO_TEX_HEIGHT));
    gSPDisplayList(POLY_XLU_DISP++, sTornadoTriDL);

    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

// Keep the offsets inside one texture period. Gfx_TexScroll takes u32 and does its own `% 2048`,
// so a negative step has to be wrapped up into positive range here rather than passed through.
void Tornado_AdvanceScroll(TornadoParams* p, s16 stepS, s16 stepT) {
    s32 s = p->scrollS + stepS;
    s32 t = p->scrollT + stepT;

    s %= TORNADO_SCROLL_S_WRAP;
    t %= TORNADO_SCROLL_T_WRAP;
    if (s < 0) {
        s += TORNADO_SCROLL_S_WRAP;
    }
    if (t < 0) {
        t += TORNADO_SCROLL_T_WRAP;
    }
    p->scrollS = (s16)s;
    p->scrollT = (s16)t;
}

// =============================================================================
// Spiral ribbons
// =============================================================================
//
// Each ribbon is an EffectBlure (the same weapon-trail system the rods and ball & chain use)
// fed one segment per frame along a parabolic spiral: the radius grows as t^2 so the line
// leaves the cone's tip and flares outward as it climbs, while the angle sweeps
// TORNADO_RIBBON_TURNS over the length. The blure's own element lifetime means only the most
// recent segments are drawn, which is what makes it read as a streak chasing up the funnel
// rather than a static wireframe.

static void Tornado_RibbonsStart(PlayState* play, TornadoRibbons* rb, u8 count, const Color_RGBA8* color) {
    RodColor trailColor;

    if (count > TORNADO_RIBBON_MAX) {
        count = TORNADO_RIBBON_MAX;
    }

    trailColor.primR = color->r;
    trailColor.primG = color->g;
    trailColor.primB = color->b;
    trailColor.primA = 255;
    // Envelope is a dimmed version of the same hue so the streak keeps the damage-type colour
    // instead of fading through white.
    trailColor.envR = color->r / 2;
    trailColor.envG = color->g / 2;
    trailColor.envB = color->b / 2;
    trailColor.envA = 0;

    for (u8 i = 0; i < count; i++) {
        rb->blureIdx[i] = FX_InitSwordTrail(play, &trailColor);
        // Stagger the phase so the ribbons chase each other instead of moving as one band.
        rb->t[i] = (f32)i / (f32)count;
    }
    rb->count = count;
    rb->active = 1;
    rb->color = *color;
}

void Tornado_RibbonsStop(PlayState* play, TornadoRibbons* rb) {
    if (!rb->active) {
        return;
    }
    for (u8 i = 0; i < rb->count; i++) {
        if (rb->blureIdx[i] >= 0) {
            FX_KillSwordTrail(play, rb->blureIdx[i]);
            rb->blureIdx[i] = -1;
        }
    }
    rb->count = 0;
    rb->active = 0;
}

void Tornado_RibbonsUpdate(PlayState* play, TornadoRibbons* rb, const TornadoParams* p, u8 count) {
    Vec3f axis;
    Vec3f right;
    Vec3f up;

    if (count == 0) {
        Tornado_RibbonsStop(play, rb);
        return;
    }
    // Clamp BEFORE the count comparison below — otherwise an over-large request would never
    // match the clamped rb->count and would tear the ribbons down and rebuild them every frame.
    if (count > TORNADO_RIBBON_MAX) {
        count = TORNADO_RIBBON_MAX;
    }
    // Restart on a colour change — blure colours are fixed at Effect_Add time, so switching
    // element mid-blow would otherwise keep the old damage type's streaks.
    if (rb->active && ((rb->color.r != p->color.r) || (rb->color.g != p->color.g) || (rb->color.b != p->color.b) ||
                       (rb->count != count))) {
        Tornado_RibbonsStop(play, rb);
    }
    if (!rb->active) {
        Tornado_RibbonsStart(play, rb, count, &p->color);
    }

    // Orthonormal frame around the cone axis. `right` is horizontal (the axis is never
    // vertical here — pitch is clamped well short of straight up/down by the aim code).
    Tornado_GetAxis(p->yaw, p->pitch, &axis);
    right.x = Math_CosS(p->yaw);
    right.y = 0.0f;
    right.z = -Math_SinS(p->yaw);
    up.x = (axis.y * right.z) - (axis.z * right.y);
    up.y = (axis.z * right.x) - (axis.x * right.z);
    up.z = (axis.x * right.y) - (axis.y * right.x);

    // Ribbon thickness: scales with the cone so a big blow gets fat streaks, with a floor so
    // the small suck cone still shows them.
    f32 width = (p->radius * 0.16f) + 3.0f;

    for (u8 i = 0; i < rb->count; i++) {
        f32 t = rb->t[i];
        f32 along = p->length * t;
        f32 r = p->radius * t * t; // parabolic flare out of the tip
        // Go through s32 first: TORNADO_RIBBON_TURNS is 1.5 revolutions, so t * TURNS reaches
        // 98304 and a direct float->s16 cast of an out-of-range value is undefined. The s32
        // value is then truncated to s16, which is the binary-angle wrap we actually want.
        s16 ang = (s16)((s32)p->spin + (s32)(i * (0x10000 / rb->count)) + (s32)(t * TORNADO_RIBBON_TURNS));
        f32 c = Math_CosS(ang);
        f32 s = Math_SinS(ang);
        Vec3f base;
        Vec3f tip;

        base.x = p->origin.x + (axis.x * along) + (((right.x * c) + (up.x * s)) * r);
        base.y = p->origin.y + (axis.y * along) + (((right.y * c) + (up.y * s)) * r);
        base.z = p->origin.z + (axis.z * along) + (((right.z * c) + (up.z * s)) * r);
        // Width runs along the axis so the band lies on the funnel's surface.
        tip.x = base.x + (axis.x * width);
        tip.y = base.y + (axis.y * width);
        tip.z = base.z + (axis.z * width);

        FX_AddSwordTrailVertex(rb->blureIdx[i], &base, &tip);

        rb->t[i] = t + TORNADO_RIBBON_STEP;
        if (rb->t[i] >= 1.0f) {
            rb->t[i] -= 1.0f; // wrap: the streak restarts from the tip
        }
    }
}
