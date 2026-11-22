#include "banjo_render.h"

#include <math.h>

#include "banjo_config.h"

#define VERTICES_PER_BATCH 30
#define TEXEL_FRACTION 32.0f
#define DEGREES_TO_RADIANS (3.14159265f / 180.0f)
// Vertices stay in model space at four steps per BK unit; whole game units would bend the face and the seams.
#define MODEL_PRECISION 4.0f

static void ComputeTint(PlayState* play, uint8_t* outTint) {
    const float strength = BanjoConfig_GetShipShading();

    for (int32_t channel = 0; channel < 3; channel++) {
        const float lit = fminf(play->lightCtx.ambientColor[channel] +
                                    (play->envCtx.dirLight1.params.dir.color[channel] +
                                     play->envCtx.dirLight2.params.dir.color[channel]) * 0.5f,
                                255.0f);

        outTint[channel] = (uint8_t)(255.0f * (1.0f + (lit / 255.0f - 1.0f) * strength));
    }
}

static void MultiplyRotations(const float left[3][3], const float right[3][3], float out[3][3]) {
    for (int32_t row = 0; row < 3; row++) {
        for (int32_t column = 0; column < 3; column++) {
            out[row][column] = left[row][0] * right[0][column] + left[row][1] * right[1][column] +
                               left[row][2] * right[2][column];
        }
    }
}

// mlMtxRotYaw, then pitch, then roll, as BK poses its models; model space faces +z like Link. MtxF keeps column
// vectors in its rows, with the translation in the last one.
static void MakeModelMatrix(const BanjoPlacement* placement, MtxF* matrix) {
    const float pitch = placement->pitchYawRoll[0] * DEGREES_TO_RADIANS;
    const float yaw = placement->pitchYawRoll[1] * DEGREES_TO_RADIANS;
    const float roll = placement->pitchYawRoll[2] * DEGREES_TO_RADIANS;
    const float yawRotation[3][3] = { { cosf(yaw), 0.0f, sinf(yaw) }, { 0.0f, 1.0f, 0.0f },
                                      { -sinf(yaw), 0.0f, cosf(yaw) } };
    const float pitchRotation[3][3] = { { 1.0f, 0.0f, 0.0f }, { 0.0f, cosf(pitch), -sinf(pitch) },
                                        { 0.0f, sinf(pitch), cosf(pitch) } };
    const float rollRotation[3][3] = { { cosf(roll), -sinf(roll), 0.0f }, { sinf(roll), cosf(roll), 0.0f },
                                       { 0.0f, 0.0f, 1.0f } };
    const float scale = placement->scale / MODEL_PRECISION;
    const float* position = &placement->position.x;
    float yawPitch[3][3];
    float rotation[3][3];

    MultiplyRotations(yawRotation, pitchRotation, yawPitch);
    MultiplyRotations(yawPitch, rollRotation, rotation);
    for (int32_t axis = 0; axis < 3; axis++) {
        for (int32_t column = 0; column < 3; column++) {
            matrix->mf[column][axis] = rotation[axis][column] * scale;
        }
        matrix->mf[axis][3] = 0.0f;
        matrix->mf[3][axis] = position[axis];
    }
    matrix->mf[3][3] = 1.0f;
}

static void WriteVertex(Vtx* vertex, const BkVertex* source, const uint8_t* tint) {
    for (int32_t axis = 0; axis < 3; axis++) {
        vertex->v.ob[axis] = (s16)lroundf(source->position[axis] * MODEL_PRECISION);
    }
    vertex->v.flag = 0;
    vertex->v.tc[0] = (s16)(source->uv[0] * TEXEL_FRACTION);
    vertex->v.tc[1] = (s16)(source->uv[1] * TEXEL_FRACTION);
    for (int32_t channel = 0; channel < 3; channel++) {
        vertex->v.cn[channel] = (uint8_t)(source->color[channel] * tint[channel] / 255);
    }
    vertex->v.cn[3] = source->color[3];
}

static void FlushBatch(Gfx** displayList, Vtx* vertices, uint32_t count) {
    gSPVertex((*displayList)++, (uintptr_t)vertices, count, 0);
    for (uint32_t first = 0; first < count; first += 3) {
        gSP1Triangle((*displayList)++, first, first + 1, first + 2, 0);
    }
}

// The ROM's formats fit TMEM; decoded to RGBA32 the 32x64 and 64x64 textures did not, and lost half their texels.
static void LoadTexture(Gfx** displayList, const BkTexture* texture, const BkMaterial* material) {
    const uint32_t width = texture->width;
    const uint32_t height = texture->height;

    switch (texture->format) {
        case BK_TEXTURE_CI4:
            gDPSetTextureLUT((*displayList)++, G_TT_RGBA16);
            gDPLoadTLUT_pal16((*displayList)++, 0, texture->palette);
            gDPLoadTextureBlock_4b((*displayList)++, texture->pixels, G_IM_FMT_CI, width, height, 0, material->clampS,
                                   material->clampT, material->maskS, material->maskT, G_TX_NOLOD, G_TX_NOLOD);
            break;
        case BK_TEXTURE_CI8:
            gDPSetTextureLUT((*displayList)++, G_TT_RGBA16);
            gDPLoadTLUT_pal256((*displayList)++, texture->palette);
            gDPLoadTextureBlock((*displayList)++, texture->pixels, G_IM_FMT_CI, G_IM_SIZ_8b, width, height, 0,
                                material->clampS, material->clampT, material->maskS, material->maskT, G_TX_NOLOD,
                                G_TX_NOLOD);
            break;
        case BK_TEXTURE_RGBA16:
            gDPSetTextureLUT((*displayList)++, G_TT_NONE);
            gDPLoadTextureBlock((*displayList)++, texture->pixels, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, height, 0,
                                material->clampS, material->clampT, material->maskS, material->maskT, G_TX_NOLOD,
                                G_TX_NOLOD);
            break;
        case BK_TEXTURE_RGBA32:
            gDPSetTextureLUT((*displayList)++, G_TT_NONE);
            gDPLoadTextureBlock((*displayList)++, texture->pixels, G_IM_FMT_RGBA, G_IM_SIZ_32b, width, height, 0,
                                material->clampS, material->clampT, material->maskS, material->maskT, G_TX_NOLOD,
                                G_TX_NOLOD);
            break;
        default:
            gDPSetTextureLUT((*displayList)++, G_TT_NONE);
            break;
    }
}

static void SetupMaterial(Gfx** displayList, const BkModel* model, const BkMaterial* material) {
    gDPPipeSync((*displayList)++);
    if (material->texture == BK_NO_TEXTURE || material->texture >= model->textureCount) {
        gSPTexture((*displayList)++, 0, 0, 0, G_TX_RENDERTILE, G_OFF);
        gDPSetCombineMode((*displayList)++, G_CC_SHADE, G_CC_SHADE);
        return;
    }

    gSPTexture((*displayList)++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    LoadTexture(displayList, &model->textures[material->texture], material);
    gDPSetCombineMode((*displayList)++, G_CC_MODULATEIDECALA, G_CC_MODULATEIDECALA);
}

static void SetupDraw(PlayState* play, Gfx** displayList, const BanjoPlacement* placement) {
    MtxF model;

    gDPPipeSync((*displayList)++);
    gDPSetCycleType((*displayList)++, G_CYC_1CYCLE);
    // modelRender.c draws the player opaque: texture alpha marks no holes in Banjo.
    gDPSetRenderMode((*displayList)++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
    gDPSetAlphaCompare((*displayList)++, G_AC_NONE);
    gSPClearGeometryMode((*displayList)++, G_LIGHTING | G_FOG | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR | G_CULL_BOTH);
    gSPSetGeometryMode((*displayList)++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    MakeModelMatrix(placement, &model);
    Matrix_Push();
    Matrix_Put(&model);
    gSPMatrix((*displayList)++, BANJO_FINALIZE_MATRIX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    Matrix_Pop();
    gDPSetTexturePersp((*displayList)++, G_TP_PERSP);
    gDPSetTextureFilter((*displayList)++, G_TF_BILERP);
}

void BanjoRender_DrawMesh(PlayState* play, const BkMesh* mesh, const BkModel* model, const BanjoPlacement* placement) {
    uint8_t tint[3];

    if (mesh->triangleCount == 0) {
        return;
    }
    ComputeTint(play, tint);

    OPEN_DISPS(play->state.gfxCtx);

    Gfx** displayList = &POLY_OPA_DISP;

    SetupDraw(play, displayList, placement);
    for (uint32_t material = 0; material < mesh->materialCount; material++) {
        Vtx* batch = BANJO_ALLOC_GFX(play->state.gfxCtx, VERTICES_PER_BATCH * sizeof(Vtx));
        uint32_t count = 0;

        SetupMaterial(displayList, model, &mesh->materials[material]);
        for (uint32_t index = 0; index < mesh->triangleCount; index++) {
            const BkTriangle* triangle = &mesh->triangles[index];

            if (triangle->material != material) {
                continue;
            }
            for (int32_t corner = 0; corner < 3; corner++) {
                WriteVertex(&batch[count++], &triangle->corners[corner], tint);
            }
            if (count == VERTICES_PER_BATCH) {
                FlushBatch(displayList, batch, count);
                batch = BANJO_ALLOC_GFX(play->state.gfxCtx, VERTICES_PER_BATCH * sizeof(Vtx));
                count = 0;
            }
        }
        if (count > 0) {
            FlushBatch(displayList, batch, count);
        }
    }
    gDPPipeSync((*displayList)++);
    gDPSetTextureLUT((*displayList)++, G_TT_NONE);

    CLOSE_DISPS(play->state.gfxCtx);
}
