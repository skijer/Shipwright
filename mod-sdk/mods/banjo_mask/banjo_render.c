#include "banjo_render.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "banjo_config.h"
#include "bk_assets.h"
#include "bk_bytes.h"

#define VERTICES_PER_BATCH 30
#define TEXEL_FRACTION 32.0f
#define DEGREES_TO_RADIANS (3.14159265f / 180.0f)
// Vertices stay in model space at four steps per BK unit; whole game units would bend the face and the seams.
#define MODEL_PRECISION 4.0f
#define BONE_SCALE_CHANNEL 3
// Units of the get-item frame a held item spans; MM's camera cuts off much taller ones.
#define PROP_GET_ITEM_SIZE 55.0f
// BK's sprite layout (the decomp's sprite.h): a header, frame offsets, then per frame a header, a palette and chunks.
#define SPRITE_HEADER_SIZE 0x10
#define SPRITE_FRAME_HEADER_SIZE 0x14
#define SPRITE_CHUNK_HEADER_SIZE 8
#define SPRITE_FORMAT_CI4 0x1
#define SPRITE_PALETTE_COLORS 16
#define SPRITE_PALETTE_SIZE (SPRITE_PALETTE_COLORS * 2)
#define SPRITE_PIXELS_SIZE (SPRITE_CANVAS * SPRITE_CANVAS / 2)
#define SPRITE_FRAME_MAX 32
#define SPRITE_CANVAS 64
#define SPRITE_CANVAS_MASK 6
#define SPRITE_FRAME_TICKS 2
#define SPRITE_GET_ITEM_SIZE 70.0f
#define ALIGN8(value) (((value) + 7u) & ~7u)

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

static void SetupDraw(PlayState* play, Gfx** displayList, MtxF* model) {
    gDPPipeSync((*displayList)++);
    gDPSetCycleType((*displayList)++, G_CYC_1CYCLE);
    // modelRender.c draws the player opaque: texture alpha marks no holes in Banjo.
    gDPSetRenderMode((*displayList)++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
    gDPSetAlphaCompare((*displayList)++, G_AC_NONE);
    gSPClearGeometryMode((*displayList)++, G_LIGHTING | G_FOG | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR | G_CULL_BOTH);
    gSPSetGeometryMode((*displayList)++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    Matrix_Push();
    Matrix_Put(model);
    gSPMatrix((*displayList)++, BANJO_FINALIZE_MATRIX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    Matrix_Pop();
    gDPSetTexturePersp((*displayList)++, G_TP_PERSP);
    gDPSetTextureFilter((*displayList)++, G_TF_BILERP);
}

// A get-item keeps its own colours: like the game's get-item models it ignores the room's light.
static void DrawTriangles(PlayState* play, const BkMesh* mesh, const BkModel* model, MtxF* matrix, bool isSceneLit) {
    uint8_t tint[3];

    if (mesh->triangleCount == 0) {
        return;
    }
    if (isSceneLit) {
        ComputeTint(play, tint);
    } else {
        for (int32_t channel = 0; channel < 3; channel++) {
            tint[channel] = 255;
        }
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx** displayList = &POLY_OPA_DISP;

    SetupDraw(play, displayList, matrix);
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

void BanjoRender_DrawMesh(PlayState* play, const BkMesh* mesh, const BkModel* model, const BanjoPlacement* placement) {
    MtxF matrix;

    MakeModelMatrix(placement, &matrix);
    DrawTriangles(play, mesh, model, &matrix, true);
}

void BanjoRender_DrawMeshHere(PlayState* play, const BkMesh* mesh, const BkModel* model, float scale) {
    const float modelScale = scale / MODEL_PRECISION;
    MtxF matrix;

    Matrix_Push();
    Matrix_Scale(modelScale, modelScale, modelScale, MTXMODE_APPLY);
    Matrix_Get(&matrix);
    Matrix_Pop();
    DrawTriangles(play, mesh, model, &matrix, false);
}

static void MeasureProp(BanjoProp* prop) {
    float low[3] = { INFINITY, INFINITY, INFINITY };
    float high[3] = { -INFINITY, -INFINITY, -INFINITY };

    for (uint32_t index = 0; index < prop->mesh.triangleCount; index++) {
        for (int32_t corner = 0; corner < 3; corner++) {
            const float* position = prop->mesh.triangles[index].corners[corner].position;

            for (int32_t axis = 0; axis < 3; axis++) {
                low[axis] = fminf(low[axis], position[axis]);
                high[axis] = fmaxf(high[axis], position[axis]);
            }
        }
    }
    prop->size = 0.0f;
    for (int32_t axis = 0; axis < 3; axis++) {
        prop->center[axis] = (low[axis] + high[axis]) * 0.5f;
        prop->size = fmaxf(prop->size, high[axis] - low[axis]);
    }
}

bool BanjoRender_BuildProp(BanjoProp* prop) {
    static BkBoneChannels sRestPose[BK_BONE_MAX];
    static const int16_t kNoSelectors[BK_SELECTOR_COUNT];

    if (prop->mesh.triangleCount > 0) {
        return true;
    }
    if (prop->model.data == NULL && !BkModel_Load(&prop->model, prop->modelId)) {
        return false;
    }

    BkMatrix* bones = calloc(prop->model.boneCount + 1, sizeof(BkMatrix));

    if (bones == NULL) {
        return false;
    }
    for (int32_t bone = 0; bone < BK_BONE_MAX; bone++) {
        for (int32_t axis = 0; axis < 3; axis++) {
            sRestPose[bone].values[BONE_SCALE_CHANNEL + axis] = 1.0f;
        }
    }
    BkModel_ComputeBoneMatrices(&prop->model, sRestPose, bones);
    BkMesh_Build(&prop->mesh, &prop->model, bones, kNoSelectors);
    free(bones);
    MeasureProp(prop);
    return prop->mesh.triangleCount > 0;
}

void BanjoRender_DrawPropGetItem(PlayState* play, BanjoProp* prop) {
    if (!BanjoRender_BuildProp(prop) || prop->size <= 0.0f) {
        return;
    }

    const float scale = PROP_GET_ITEM_SIZE / prop->size;

    Matrix_Push();
    Matrix_Translate(-prop->center[0] * scale, -prop->center[1] * scale, -prop->center[2] * scale, MTXMODE_APPLY);
    BanjoRender_DrawMeshHere(play, &prop->mesh, &prop->model, scale);
    Matrix_Pop();
}

static uint8_t FindTransparentIndex(const uint8_t* palette) {
    for (uint8_t index = 0; index < SPRITE_PALETTE_COLORS; index++) {
        if ((BkBytes_U16(palette + index * 2) & 1) == 0) {
            return index;
        }
    }
    return 0;
}

static void PutTexel(uint8_t* pixels, int32_t x, int32_t y, uint8_t index) {
    uint8_t* pair = &pixels[(y * SPRITE_CANVAS + x) / 2];

    *pair = (x % 2 == 0) ? (uint8_t)((*pair & 0x0F) | (index << 4)) : (uint8_t)((*pair & 0xF0) | index);
}

// Copies one frame's palette and chunks onto its square canvas, centred, as CI4 like the ROM.
static bool DecodeSpriteFrame(const uint8_t* asset, uint32_t size, uint32_t start, uint8_t* pixels,
                              uint8_t* palette) {
    if (start + SPRITE_FRAME_HEADER_SIZE > size) {
        return false;
    }

    const int32_t width = BkBytes_S16(asset + start + 4);
    const int32_t height = BkBytes_S16(asset + start + 6);
    const int32_t chunkCount = BkBytes_S16(asset + start + 8);
    const int32_t left = (SPRITE_CANVAS - width) / 2;
    const int32_t top = (SPRITE_CANVAS - height) / 2;
    uint32_t cursor = ALIGN8(start + SPRITE_FRAME_HEADER_SIZE);

    if (cursor + SPRITE_PALETTE_SIZE > size) {
        return false;
    }
    memcpy(palette, asset + cursor, SPRITE_PALETTE_SIZE);

    const uint8_t clear = FindTransparentIndex(palette);

    memset(pixels, clear | (clear << 4), SPRITE_PIXELS_SIZE);
    cursor = ALIGN8(cursor + SPRITE_PALETTE_SIZE);
    for (int32_t chunk = 0; chunk < chunkCount; chunk++) {
        if (cursor + SPRITE_CHUNK_HEADER_SIZE > size) {
            return false;
        }

        const int32_t chunkX = BkBytes_S16(asset + cursor);
        const int32_t chunkY = BkBytes_S16(asset + cursor + 2);
        const int32_t chunkWidth = BkBytes_S16(asset + cursor + 4);
        const int32_t chunkHeight = BkBytes_S16(asset + cursor + 6);

        cursor = ALIGN8(cursor + SPRITE_CHUNK_HEADER_SIZE);
        if (chunkWidth <= 0 || chunkHeight <= 0 || cursor + (uint32_t)(chunkWidth * chunkHeight) / 2 > size) {
            return false;
        }
        for (int32_t texel = 0; texel < chunkWidth * chunkHeight; texel++) {
            const int32_t x = left + chunkX + texel % chunkWidth;
            const int32_t y = top + chunkY + texel / chunkWidth;
            const uint8_t pair = asset[cursor + texel / 2];

            if (x >= 0 && y >= 0 && x < SPRITE_CANVAS && y < SPRITE_CANVAS) {
                PutTexel(pixels, x, y, (texel % 2 == 0) ? (pair >> 4) : (pair & 0xF));
            }
        }
        cursor = ALIGN8(cursor + (uint32_t)(chunkWidth * chunkHeight) / 2);
    }
    return true;
}

static void BuildSpriteQuad(BanjoSprite* sprite) {
    static const int16_t kCorners[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
    const int16_t half = SPRITE_CANVAS / 2;

    for (int32_t corner = 0; corner < 4; corner++) {
        Vtx_t* vertex = &sprite->quad[corner].v;

        vertex->ob[0] = (int16_t)(kCorners[corner][0] * half);
        vertex->ob[1] = (int16_t)(kCorners[corner][1] * half);
        vertex->ob[2] = 0;
        vertex->flag = 0;
        vertex->tc[0] = (int16_t)((kCorners[corner][0] + 1) * half * TEXEL_FRACTION);
        vertex->tc[1] = (int16_t)((1 - kCorners[corner][1]) * half * TEXEL_FRACTION);
        memset(vertex->cn, 255, sizeof(vertex->cn));
    }
}

// Only BK's CI4 sprites, which the collectibles all are.
static bool LoadSprite(BanjoSprite* sprite) {
    uint32_t size = 0;
    uint8_t* asset = BkAssets_Load(sprite->assetId, &size);
    bool isDecoded = asset != NULL && size >= SPRITE_HEADER_SIZE && BkBytes_S16(asset + 2) == SPRITE_FORMAT_CI4;
    const int32_t frameCount = isDecoded ? BkBytes_S16(asset) : 0;
    const uint32_t frameBase = SPRITE_HEADER_SIZE + (uint32_t)frameCount * 4;

    isDecoded = isDecoded && frameCount > 0 && frameCount <= SPRITE_FRAME_MAX && frameBase <= size;
    if (isDecoded) {
        sprite->pixels = malloc((size_t)frameCount * SPRITE_PIXELS_SIZE);
        sprite->palettes = malloc((size_t)frameCount * SPRITE_PALETTE_SIZE);
        isDecoded = sprite->pixels != NULL && sprite->palettes != NULL;
    }
    for (int32_t frame = 0; isDecoded && frame < frameCount; frame++) {
        const uint32_t start = frameBase + BkBytes_U32(asset + SPRITE_HEADER_SIZE + frame * 4);

        isDecoded = DecodeSpriteFrame(asset, size, start, sprite->pixels + frame * SPRITE_PIXELS_SIZE,
                                      sprite->palettes + frame * SPRITE_PALETTE_SIZE);
    }
    free(asset);
    if (!isDecoded) {
        free(sprite->pixels);
        free(sprite->palettes);
        sprite->pixels = NULL;
        sprite->palettes = NULL;
        return false;
    }
    sprite->frameCount = (uint8_t)frameCount;
    BuildSpriteQuad(sprite);
    return true;
}

void BanjoRender_DrawSpriteGetItem(PlayState* play, BanjoSprite* sprite) {
    if (!sprite->isLoaded) {
        sprite->isLoaded = true;
        LoadSprite(sprite);
    }
    if (sprite->pixels == NULL) {
        return;
    }

    const uint32_t frame = (play->state.frames / SPRITE_FRAME_TICKS) % sprite->frameCount;
    const float scale = SPRITE_GET_ITEM_SIZE / SPRITE_CANVAS;

    OPEN_DISPS(play->state.gfxCtx);
    BANJO_SETUP_OPA(play->state.gfxCtx);
    Matrix_Push();
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, BANJO_FINALIZE_MATRIX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    Matrix_Pop();
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
    gSPClearGeometryMode(POLY_OPA_DISP++, G_LIGHTING | G_CULL_BOTH);
    gSPTexture(POLY_OPA_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetTextureLUT(POLY_OPA_DISP++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(POLY_OPA_DISP++, 0, sprite->palettes + frame * SPRITE_PALETTE_SIZE);
    gDPLoadTextureBlock_4b(POLY_OPA_DISP++, sprite->pixels + frame * SPRITE_PIXELS_SIZE, G_IM_FMT_CI, SPRITE_CANVAS,
                           SPRITE_CANVAS, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP,
                           SPRITE_CANVAS_MASK, SPRITE_CANVAS_MASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPVertex(POLY_OPA_DISP++, (uintptr_t)sprite->quad, 4, 0);
    gSP2Triangles(POLY_OPA_DISP++, 0, 1, 2, 0, 0, 2, 3, 0);
    gDPSetTextureLUT(POLY_OPA_DISP++, G_TT_NONE);
    gSPSetGeometryMode(POLY_OPA_DISP++, G_LIGHTING | G_CULL_BACK);
    CLOSE_DISPS(play->state.gfxCtx);
}
