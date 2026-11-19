#include "global.h"
#include "soh/OTRGlobals.h"

typedef struct {
    GraphicsContext* gfxCtx;
    s32 step;
    u8 pass;
} MatAnimDraw;

// Scene_DrawMaterialAnims holds the DISPS open for the whole list, so this appends to the buffer heads directly.
static void MatAnim_BindSegment(MatAnimDraw* d, s32 segment, void* data) {
    if (d->pass & ANIM_MAT_PASS_OPA) {
        gSPSegment(d->gfxCtx->polyOpa.p++, segment, data);
    }
    if (d->pass & ANIM_MAT_PASS_XLU) {
        gSPSegment(d->gfxCtx->polyXlu.p++, segment, data);
    }
}

#define MAT_ANIM_SCROLL_PERIOD 32768.0

static f32 MatAnim_ScrollOffset(f64 rate, f64 frame) {
    f64 offset = fmod(rate * frame, MAT_ANIM_SCROLL_PERIOD);

    return (f32)(offset < 0.0 ? offset + MAT_ANIM_SCROLL_PERIOD : offset);
}

static Gfx* MatAnim_WriteScrollTile(Gfx* gfx, s32 tile, const AnimatedMatTexScrollParams* p, f64 frame) {
    f64 xRate = (f64)p->xStep + p->xSpeed;
    f64 yRate = -((f64)p->yStep + p->ySpeed);
    f32 x0 = MatAnim_ScrollOffset(xRate, frame);
    f32 y0 = MatAnim_ScrollOffset(yRate, frame);
    // The offset at the next frame would wrap, and a lerp across the wrap would sweep the whole period backwards.
    f32 x1 = (f32)(x0 + xRate);
    f32 y1 = (f32)(y0 + yRate);
    f32 w = (f32)((p->width - 1) << 2);
    f32 h = (f32)((p->height - 1) << 2);

    gDPSetTileSizeLerp(gfx, tile, x0, y0, x0 + w, y0 + h, x1, y1, x1 + w, y1 + h);
    return gfx + 5;
}

static Gfx* MatAnim_ScrollList(MatAnimDraw* d, const AnimatedMatTexScrollParams* layers, s32 layerCount) {
    Gfx* list = Graph_Alloc(d->gfxCtx, (2 + 5 * layerCount) * sizeof(Gfx));
    Gfx* gfx = list;
    s32 tile;

    gDPTileSync(gfx++);
    for (tile = 0; tile < layerCount; tile++) {
        gfx = MatAnim_WriteScrollTile(gfx, tile, &layers[tile], d->step);
    }
    gSPEndDisplayList(gfx);

    return list;
}

static void MatAnim_DrawTexScroll(MatAnimDraw* d, s32 segment, void* params) {
    MatAnim_BindSegment(d, segment, MatAnim_ScrollList(d, params, 1));
}

static void MatAnim_DrawTwoTexScroll(MatAnimDraw* d, s32 segment, void* params) {
    MatAnim_BindSegment(d, segment, MatAnim_ScrollList(d, params, 2));
}

static void MatAnim_SetColor(MatAnimDraw* d, s32 segment, F3DPrimColor* prim, F3DEnvColor* env) {
    Gfx* gfx = Graph_Alloc(d->gfxCtx, 3 * sizeof(Gfx));
    Gfx* head = gfx;

    gDPSetPrimColor(gfx++, 0, prim->lodFrac, prim->r, prim->g, prim->b, prim->a);
    if (env != NULL) {
        gDPSetEnvColor(gfx++, env->r, env->g, env->b, env->a);
    }
    gSPEndDisplayList(gfx++);

    MatAnim_BindSegment(d, segment, head);
}

static s32 MatAnim_CurKeyFrame(AnimatedMatColorParams* p, s32 curFrame) {
    s32 k = 0;

    while ((k + 1 < p->keyFrameCount) && (curFrame >= p->keyFrames[k + 1])) {
        k++;
    }
    return k;
}

static void MatAnim_DrawColor(MatAnimDraw* d, s32 segment, void* params) {
    AnimatedMatColorParams* p = params;
    s32 k = MatAnim_CurKeyFrame(p, d->step % p->keyFrameLength);
    F3DPrimColor* prim = p->primColors + k;
    F3DEnvColor* env = (p->envColors != NULL) ? p->envColors + k : NULL;

    MatAnim_SetColor(d, segment, prim, env);
}

static s32 MatAnim_Lerp(s32 min, s32 max, f32 norm) {
    return (s32)((max - min) * norm) + min;
}

static void MatAnim_DrawColorLerp(MatAnimDraw* d, s32 segment, void* params) {
    AnimatedMatColorParams* p = params;
    s32 curFrame = d->step % p->keyFrameLength;
    s32 k = MatAnim_CurKeyFrame(p, curFrame);
    s32 next = (k + 1 < p->keyFrameCount) ? k + 1 : k;
    s32 startFrame = p->keyFrames[k];
    s32 endFrame = p->keyFrames[next] - startFrame;
    f32 norm = (endFrame != 0) ? (f32)(curFrame - startFrame) / (f32)endFrame : 0.0f;
    F3DPrimColor* primMin = p->primColors + k;
    F3DPrimColor* primMax = p->primColors + next;
    F3DPrimColor primResult;
    F3DEnvColor envResult;

    primResult.r = MatAnim_Lerp(primMin->r, primMax->r, norm);
    primResult.g = MatAnim_Lerp(primMin->g, primMax->g, norm);
    primResult.b = MatAnim_Lerp(primMin->b, primMax->b, norm);
    primResult.a = MatAnim_Lerp(primMin->a, primMax->a, norm);
    primResult.lodFrac = MatAnim_Lerp(primMin->lodFrac, primMax->lodFrac, norm);

    if (p->envColors != NULL) {
        F3DEnvColor* envMin = p->envColors + k;
        F3DEnvColor* envMax = p->envColors + next;

        envResult.r = MatAnim_Lerp(envMin->r, envMax->r, norm);
        envResult.g = MatAnim_Lerp(envMin->g, envMax->g, norm);
        envResult.b = MatAnim_Lerp(envMin->b, envMax->b, norm);
        envResult.a = MatAnim_Lerp(envMin->a, envMax->a, norm);
    }

    MatAnim_SetColor(d, segment, &primResult, (p->envColors != NULL) ? &envResult : NULL);
}

// Evaluated in double: the basis products would overflow f32 and turn the result into NaN.
static f64 MatAnim_LagrangeInterp(s32 n, f64 x[], f64 fx[], f64 xp) {
    f64 weights[ANIM_MAT_MAX_KEY_FRAMES];
    f64 intp = 0.0;
    s32 i;
    s32 j;

    for (i = 0; i < n; i++) {
        f64 m = 1.0;

        for (j = 0; j < n; j++) {
            if (j != i) {
                m *= x[i] - x[j];
            }
        }
        weights[i] = fx[i] / m;
    }

    for (i = 0; i < n; i++) {
        f64 m = 1.0;

        for (j = 0; j < n; j++) {
            if (j != i) {
                m *= xp - x[j];
            }
        }
        intp += weights[i] * m;
    }

    return intp;
}

static u8 MatAnim_LagrangeInterpColor(s32 n, f64 x[], f64 fx[], f64 xp) {
    f64 intp = MatAnim_LagrangeInterp(n, x, fx, xp);

    return (intp >= 255.0) ? 255 : (intp > 0.0) ? (u8)intp : 0;
}

static void MatAnim_DrawColorNonLinearInterp(MatAnimDraw* d, s32 segment, void* params) {
    AnimatedMatColorParams* p = params;
    f64 curFrame = d->step % p->keyFrameLength;
    s32 n = p->keyFrameCount;
    f64 x[ANIM_MAT_MAX_KEY_FRAMES];
    f64 fxPrim[5][ANIM_MAT_MAX_KEY_FRAMES];
    f64 fxEnv[4][ANIM_MAT_MAX_KEY_FRAMES];
    F3DPrimColor primResult;
    F3DEnvColor envResult;
    s32 i;

    for (i = 0; i < n; i++) {
        x[i] = p->keyFrames[i];
        fxPrim[0][i] = p->primColors[i].r;
        fxPrim[1][i] = p->primColors[i].g;
        fxPrim[2][i] = p->primColors[i].b;
        fxPrim[3][i] = p->primColors[i].a;
        fxPrim[4][i] = p->primColors[i].lodFrac;
        if (p->envColors != NULL) {
            fxEnv[0][i] = p->envColors[i].r;
            fxEnv[1][i] = p->envColors[i].g;
            fxEnv[2][i] = p->envColors[i].b;
            fxEnv[3][i] = p->envColors[i].a;
        }
    }

    primResult.r = MatAnim_LagrangeInterpColor(n, x, fxPrim[0], curFrame);
    primResult.g = MatAnim_LagrangeInterpColor(n, x, fxPrim[1], curFrame);
    primResult.b = MatAnim_LagrangeInterpColor(n, x, fxPrim[2], curFrame);
    primResult.a = MatAnim_LagrangeInterpColor(n, x, fxPrim[3], curFrame);
    primResult.lodFrac = MatAnim_LagrangeInterpColor(n, x, fxPrim[4], curFrame);

    if (p->envColors != NULL) {
        envResult.r = MatAnim_LagrangeInterpColor(n, x, fxEnv[0], curFrame);
        envResult.g = MatAnim_LagrangeInterpColor(n, x, fxEnv[1], curFrame);
        envResult.b = MatAnim_LagrangeInterpColor(n, x, fxEnv[2], curFrame);
        envResult.a = MatAnim_LagrangeInterpColor(n, x, fxEnv[3], curFrame);
    }

    MatAnim_SetColor(d, segment, &primResult, (p->envColors != NULL) ? &envResult : NULL);
}

static void MatAnim_DrawTexCycle(MatAnimDraw* d, s32 segment, void* params) {
    AnimatedMatTexCycleParams* p = params;
    s32 curFrame = d->step % p->keyFrameLength;

    MatAnim_BindSegment(d, segment, p->textureList[p->textureIndexList[curFrame]]);
}

void Scene_DrawMaterialAnims(PlayState* play) {
    static void (*sHandlers[ANIM_MAT_TYPE_MAX])(MatAnimDraw*, s32, void*) = {
        MatAnim_DrawTexScroll, MatAnim_DrawTwoTexScroll,         MatAnim_DrawColor,
        MatAnim_DrawColorLerp, MatAnim_DrawColorNonLinearInterp, MatAnim_DrawTexCycle,
    };
    MatAnimDraw d;
    u32 i;

    if (play->sceneMaterialAnims == NULL || play->sceneMaterialAnimCount == 0) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    d.gfxCtx = play->state.gfxCtx;
    d.step = play->gameplayFrames;

    for (i = 0; i < play->sceneMaterialAnimCount; i++) {
        AnimatedMaterial* anim = &play->sceneMaterialAnims[i];

        if (anim->type >= ANIM_MAT_TYPE_MAX || anim->params == NULL) {
            continue;
        }
        d.pass = anim->pass;
        sHandlers[anim->type](&d, anim->segment, anim->params);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
