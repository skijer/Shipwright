/**
 * stasis_rune_vfx.inc.c — the Stasis visuals (Skijer's NEI). Included at the tail of
 * stasis_rune.c, so it shares its state and its translation unit.
 *
 * Two pieces:
 *   CHAINS — hookshot chain links pinning the frozen body along the three world axes. They fire
 *            ONCE, for one second, as the stasis takes hold.
 *   ARROW  — a curved gold arrow tracing the arc the object will actually fly, sampled from the
 *            same launch maths the throw uses, so it never lies about where the thing is going.
 *
 * The whole thing is flat-shaded prim colour: no textures, no assets, nothing to rebuild.
 */

// ── Inline geometry (the Pacci gizmo shapes, kept local so this file owns its own art) ───────────
// Modelled at radius/length 100 so the draw helpers can scale by (want / 100).
static Vtx sStasisPrismVtx[] = {
    VTX(100, 0, 0, 0, 0, 0, 0, 0, 255),    VTX(71, 0, 71, 0, 0, 0, 0, 0, 255),
    VTX(0, 0, 100, 0, 0, 0, 0, 0, 255),    VTX(-71, 0, 71, 0, 0, 0, 0, 0, 255),
    VTX(-100, 0, 0, 0, 0, 0, 0, 0, 255),   VTX(-71, 0, -71, 0, 0, 0, 0, 0, 255),
    VTX(0, 0, -100, 0, 0, 0, 0, 0, 255),   VTX(71, 0, -71, 0, 0, 0, 0, 0, 255),
    VTX(100, 100, 0, 0, 0, 0, 0, 0, 255),  VTX(71, 100, 71, 0, 0, 0, 0, 0, 255),
    VTX(0, 100, 100, 0, 0, 0, 0, 0, 255),  VTX(-71, 100, 71, 0, 0, 0, 0, 0, 255),
    VTX(-100, 100, 0, 0, 0, 0, 0, 0, 255), VTX(-71, 100, -71, 0, 0, 0, 0, 0, 255),
    VTX(0, 100, -100, 0, 0, 0, 0, 0, 255), VTX(71, 100, -71, 0, 0, 0, 0, 0, 255),
};

static Gfx sStasisPrismDL[] = {
    gsSPVertex(sStasisPrismVtx, 16, 0),        gsSP2Triangles(0, 8, 9, 0, 0, 9, 1, 0),
    gsSP2Triangles(1, 9, 10, 0, 1, 10, 2, 0),  gsSP2Triangles(2, 10, 11, 0, 2, 11, 3, 0),
    gsSP2Triangles(3, 11, 12, 0, 3, 12, 4, 0), gsSP2Triangles(4, 12, 13, 0, 4, 13, 5, 0),
    gsSP2Triangles(5, 13, 14, 0, 5, 14, 6, 0), gsSP2Triangles(6, 14, 15, 0, 6, 15, 7, 0),
    gsSP2Triangles(7, 15, 8, 0, 7, 8, 0, 0),   gsSPEndDisplayList(),
};

static Vtx sStasisConeVtx[] = {
    VTX(0, 100, 0, 0, 0, 0, 0, 0, 255),   VTX(100, 0, 0, 0, 0, 0, 0, 0, 255),  VTX(71, 0, 71, 0, 0, 0, 0, 0, 255),
    VTX(0, 0, 100, 0, 0, 0, 0, 0, 255),   VTX(-71, 0, 71, 0, 0, 0, 0, 0, 255), VTX(-100, 0, 0, 0, 0, 0, 0, 0, 255),
    VTX(-71, 0, -71, 0, 0, 0, 0, 0, 255), VTX(0, 0, -100, 0, 0, 0, 0, 0, 255), VTX(71, 0, -71, 0, 0, 0, 0, 0, 255),
};

static Gfx sStasisConeDL[] = {
    gsSPVertex(sStasisConeVtx, 9, 0),       gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsSP2Triangles(0, 3, 4, 0, 0, 4, 5, 0), gsSP2Triangles(0, 5, 6, 0, 0, 6, 7, 0),
    gsSP2Triangles(0, 7, 8, 0, 0, 8, 1, 0), gsSPEndDisplayList(),
};

#define STASIS_ARROW_SHAFT_R 4.5f
#define STASIS_ARROW_HEAD_R 13.0f
#define STASIS_ARROW_HEAD_LEN 22.0f
#define STASIS_ARROW_MIN_LEN 34.0f // length with nothing stored
#define STASIS_ARROW_GROWTH 110.0f // ...and how much a full charge adds on top
#define STASIS_CHAIN_COUNT 6

// The yellow the chains end up as. Brighter than the body tint on purpose: a thin chain over open
// scenery needs the extra headroom to still read as glowing.
#define STASIS_CHAIN_R 255
#define STASIS_CHAIN_G 240
#define STASIS_CHAIN_B 110

// Flat unlit prim colour: a gizmo must read as a control, not as a spell.
static void Stasis_SolidBegin(Gfx** gfxP) {
    Gfx* gfx = *gfxP;

    gDPPipeSync(gfx++);
    gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE);
    gSPClearGeometryMode(gfx++, G_LIGHTING | G_CULL_BACK);
    *gfxP = gfx;
}

static void Stasis_SolidEnd(Gfx** gfxP) {
    Gfx* gfx = *gfxP;

    gSPSetGeometryMode(gfx++, G_LIGHTING | G_CULL_BACK);
    *gfxP = gfx;
}

// One shape stretched from `start` to `end`. The model's +Y becomes the segment's axis.
static void Stasis_DrawSolid(PlayState* play, Gfx** gfxP, Gfx* dl, Vec3f* start, Vec3f* end, f32 radius, u8 r, u8 g,
                             u8 b, u8 a) {
    Gfx* gfx = *gfxP;
    f32 dx = end->x - start->x;
    f32 dy = end->y - start->y;
    f32 dz = end->z - start->z;
    f32 xzLen = sqrtf((dx * dx) + (dz * dz));
    f32 len = sqrtf((dx * dx) + (dy * dy) + (dz * dz));

    if (len < 0.01f) {
        return;
    }

    Matrix_Translate(start->x, start->y, start->z, MTXMODE_NEW);
    Matrix_RotateY(Math_FAtan2F(dx, dz), MTXMODE_APPLY);
    Matrix_RotateX(Math_FAtan2F(xzLen, dy), MTXMODE_APPLY);
    Matrix_Scale(radius / 100.0f, len / 100.0f, radius / 100.0f, MTXMODE_APPLY);

    gDPSetPrimColor(gfx++, 0, 0, r, g, b, a);
    gSPMatrix(gfx++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(gfx++, dl);
    *gfxP = gfx;
}

// The BotW indicator: ONE straight arrow out of the body, pointing exactly where the last blow
// will send it, growing longer as the stored energy rises and reddening with it. Straight, not a
// parabola — it is a direction readout, and an arc would imply a landing spot the physics does not
// promise.
static void Stasis_DrawArrow(PlayState* play, Gfx** gfxP) {
    Actor* actor = sStasis.actor;
    Vec3f origin;
    Vec3f neck;
    Vec3f tip;
    Vec3f dir;
    f32 charge = Stasis_ChargeFraction();
    f32 startDist;
    f32 length;
    f32 headLength;
    f32 pulse = 0.75f + (0.25f * Math_SinS((s16)(sStasis.age * 0x1000)));
    u8 r;
    u8 g;
    u8 b;
    u8 a = (u8)(230.0f * pulse);

    Stasis_LaunchDir(play, &dir);
    Stasis_ChargeColor(charge, &r, &g, &b);

    origin = actor->world.pos;
    origin.y += actor->shape.yOffset * actor->scale.y;

    // Starts clear of the body and reaches further the more it is carrying.
    startDist = 20.0f + (actor->colChkInfo.cylRadius * 0.6f);
    length = startDist + STASIS_ARROW_MIN_LEN + (STASIS_ARROW_GROWTH * charge);
    headLength = STASIS_ARROW_HEAD_LEN;

    neck.x = origin.x + (dir.x * (length - headLength));
    neck.y = origin.y + (dir.y * (length - headLength));
    neck.z = origin.z + (dir.z * (length - headLength));
    tip.x = origin.x + (dir.x * length);
    tip.y = origin.y + (dir.y * length);
    tip.z = origin.z + (dir.z * length);
    origin.x += dir.x * startDist;
    origin.y += dir.y * startDist;
    origin.z += dir.z * startDist;

    Stasis_SolidBegin(gfxP);
    Stasis_DrawSolid(play, gfxP, sStasisPrismDL, &origin, &neck, STASIS_ARROW_SHAFT_R, r, g, b, a);
    Stasis_DrawSolid(play, gfxP, sStasisConeDL, &neck, &tip, STASIS_ARROW_HEAD_R, r, g, b, a);
    Stasis_SolidEnd(gfxP);
}

// ── Making the chain read as bright yellow ───────────────────────────────────────────────────────
// The vanilla chain will not take a colour from outside. Its display list sets its OWN state before
// it draws (decomp, object_link_boy.c):
//
//     gsDPSetCombineMode(G_CC_MODULATEIDECALA_PRIM, G_CC_PASS2)
//     gsDPSetPrimColor(0, 0, 255, 255, 255, 255)
//
// so any prim colour set beforehand is overwritten, and the combiner it picks is TEXEL0 * PRIM —
// with prim forced white, the output is just the texture. That texture is dark blue-grey metal:
// measured over its 224 opaque texels, luminance runs 11..248 with a mean of 90/255. Tinting that
// yields a dark olive, which is exactly what a yellow tint looked like before this.
//
// So the DL is copied once and its two state commands are NOP'd out, the way the repo already
// neutralises gGiSmallKeyDL and the skulltula flame. Then we own the combiner:
//
//     colour = TEXEL0 * PRIMITIVE + ENVIRONMENT
//
// ENVIRONMENT is a yellow floor that lifts the near-black links (brightness way up), PRIMITIVE is
// the bright yellow the lit parts multiply toward, and because prim's blue is low the blue-grey
// cast is gone (the "grayscale" half of it). Alpha stays TEXEL0 so the link cut-outs survive.
#define STASIS_CHAIN_DL_MAX 128
#define STASIS_CHAIN_SRC_DL gLinkAdultHookshotChainDL

static Gfx sStasisChainCopy[STASIS_CHAIN_DL_MAX];
static u8 sStasisChainCopyReady = 0;

// Some LUS commands occupy TWO Gfx entries, the second being payload that must never be read as an
// opcode. Same table the randomiser's MmDL_WithScopedVerts walks with.
static u8 Stasis_GfxIsTwoWord(u8 op) {
    return (op == 0x20) || (op == 0x24) || (op == 0x25) || (op == 0x27) || (op == 0x31) || (op == 0x32) ||
           (op == 0x33) || (op == 0x35) || (op == 0x36) || (op == 0x42);
}

static Gfx* Stasis_GetTintableChainDL(Gfx* srcDL) {
    s32 i = 0;

    if (srcDL == NULL) {
        return NULL;
    }
    if (sStasisChainCopyReady) {
        return sStasisChainCopy;
    }

    while (i < STASIS_CHAIN_DL_MAX) {
        u8 op = (u8)((srcDL[i].words.w0 >> 24) & 0xFF);

        sStasisChainCopy[i] = srcDL[i];

        // Drop the DL's own combiner and prim colour so ours survive into the draw.
        if ((op == (u8)G_SETCOMBINE) || (op == (u8)G_SETPRIMCOLOR)) {
            gDPNoOp(&sStasisChainCopy[i]);
        }
        if (op == (u8)G_ENDDL) {
            sStasisChainCopyReady = 1;
            return sStasisChainCopy;
        }
        i++;
        // Copy the payload word verbatim and skip past it, so it is never mistaken for an opcode.
        if (Stasis_GfxIsTwoWord(op) && (i < STASIS_CHAIN_DL_MAX)) {
            sStasisChainCopy[i] = srcDL[i];
            i++;
        }
    }
    return NULL; // longer than the buffer — draw nothing rather than run off the end
}

// One chain, drawn EXACTLY the way the real hookshot and the Switch Hook draw theirs: a single
// stretched display list, XY scale 0.015 for the link thickness and Z scale length*0.01, so the
// links come out the same size as the hookshot's instead of the doll-sized ones a per-link loop
// produced. The only difference is the colour, which is ours (see the copy helper above).
static void Stasis_DrawChain(PlayState* play, Vec3f* start, Vec3f* end, u8 alpha) {
    Gfx* tintable = Stasis_GetTintableChainDL(STASIS_CHAIN_SRC_DL);
    f32 dx = end->x - start->x;
    f32 dy = end->y - start->y;
    f32 dz = end->z - start->z;
    f32 distXZ = sqrtf((dx * dx) + (dz * dz));
    f32 len = sqrtf((dx * dx) + (dy * dy) + (dz * dz));

    if ((tintable == NULL) || (len < 0.01f)) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    gDPPipeSync(POLY_XLU_DISP++);

    // The three steps, in the order the hardware actually runs them.
    //
    // STEP 2 — BRIGHTNESS, in the combiner:  colour = TEXEL0 * PRIMITIVE + ENVIRONMENT
    //   PRIMITIVE stays white so the link shading survives; ENVIRONMENT is a flat lift that drags
    //   the near-black texels up. The chain texture averages 90/255, so without this floor there is
    //   simply not enough light in it for any tint to look bright.
    // STEPS 1 & 3 — GRAYSCALE, then YELLOW, in the shader pass below: it takes the (now bright)
    //   colour, flattens it to a single intensity — which is what kills the blue-grey metal cast —
    //   and multiplies that by our yellow. Confirmed to run AFTER the combiner
    //   (libultraship default.shader.hlsl: intensity = (r+g+b)/3, new_texel = grayscale.rgb * intensity).
    gDPSetCombineLERP(POLY_XLU_DISP++, TEXEL0, 0, PRIMITIVE, ENVIRONMENT, 0, 0, 0, TEXEL0, COMBINED, 0, PRIMITIVE,
                      ENVIRONMENT, 0, 0, 0, COMBINED);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 170, 170, 170, alpha);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, STASIS_CHAIN_R, STASIS_CHAIN_G, STASIS_CHAIN_B, 255);
    gSPGrayscale(POLY_XLU_DISP++, true);

    Matrix_Translate(start->x, start->y, start->z, MTXMODE_NEW);
    Matrix_RotateY(Math_FAtan2F(dx, dz), MTXMODE_APPLY);
    Matrix_RotateX(Math_FAtan2F(-dy, distXZ), MTXMODE_APPLY);
    Matrix_Scale(0.015f, 0.015f, len * 0.01f, MTXMODE_APPLY);

    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, tintable);

    gSPGrayscale(POLY_XLU_DISP++, false);

    CLOSE_DISPS(play->state.gfxCtx);
}

// Chains along the THREE WORLD AXES, both ways: +X/-X, +Y/-Y, +Z/-Z. Six spokes that read as the
// object being pinned in place from every direction rather than as a decorative burst.
static void Stasis_DrawChains(PlayState* play) {
    static const f32 sChainAxis[STASIS_CHAIN_COUNT][3] = {
        { 1.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, // X
        { 0.0f, 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, // Y
        { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, // Z
    };
    Actor* actor = sStasis.actor;
    Vec3f center;
    f32 grow;
    f32 reach;
    u8 alpha;
    s32 i;
    // The burst runs at the same rate as the cue, so its span is per-kind and normalising by the
    // object one would start an enemy burst already half faded.
    f32 span = (f32)((sStasis.kind == STASIS_KIND_ENEMY) ? STASIS_CHAIN_FRAMES_ENEMY : STASIS_CHAIN_FRAMES);

    // Grows out over the first half of the burst, fades over the second.
    grow = 1.0f - ((f32)sStasis.chainTimer / span);
    if (grow > 1.0f) {
        grow = 1.0f;
    }
    alpha = (u8)(255.0f * ((f32)sStasis.chainTimer / span));
    reach = 45.0f + (55.0f * grow);

    center = actor->world.pos;
    center.y += actor->shape.yOffset * actor->scale.y;

    for (i = 0; i < STASIS_CHAIN_COUNT; i++) {
        Vec3f end;

        end.x = center.x + (sChainAxis[i][0] * reach);
        end.y = center.y + (sChainAxis[i][1] * reach);
        end.z = center.z + (sChainAxis[i][2] * reach);
        Stasis_DrawChain(play, &center, &end, alpha);
    }
}

void Stasis_Draw(PlayState* play) {
    Actor* actor = sStasis.actor;
    Gfx* gfx;

    if ((actor == NULL) || (actor->update == NULL)) {
        return;
    }

    if (sStasis.chainTimer > 0) {
        OPEN_DISPS(play->state.gfxCtx);
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        CLOSE_DISPS(play->state.gfxCtx);
        Stasis_DrawChains(play);
    }

    // The arrow only means something while the object is still holding still and can be charged.
    if ((sStasis.phase == STASIS_PHASE_FROZEN) && (sStasis.kind != STASIS_KIND_ENEMY)) {
        OPEN_DISPS(play->state.gfxCtx);
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gfx = POLY_XLU_DISP;
        Stasis_DrawArrow(play, &gfx);
        POLY_XLU_DISP = gfx;
        CLOSE_DISPS(play->state.gfxCtx);
    }
}
