#include "mario_render.h"

#include <math.h>
#include <stdbool.h>

#include "libsm64/libsm64.h"
#include "mario_config.h"
#include "mario_world.h"

#define ATLAS_TILE_COUNT (SM64_TEXTURE_WIDTH / 64)
#define WING_TILE_FIRST 9
#define WING_TILE_LAST 10
#define METAL_TEXTURE_WIDTH 64
#define METAL_TEXTURE_HEIGHT 32
#define VERTICES_PER_BATCH 30
#define VANISH_ALPHA 100
#define OPAQUE_ALPHA 255
#define TEXEL_FRACTION_BITS 32.0f
#define WING_VERTICES_MIN 2

typedef struct MarioPass {
    Gfx** displayList;
    uint8_t alpha;
    bool isMetal;
    bool isWingSplit;
    bool isWingOnly;
} MarioPass;

typedef struct SceneLight {
    float ambient[3];
    float color[2][3];
} SceneLight;

static const uint8_t* sAtlas;

void MarioRender_SetTextureAtlas(const uint8_t* atlas) {
    sAtlas = atlas;
}

// UV (1, 1) is the untextured body's sentinel corner; any other UV in tiles 9 and 10 is a wing.
static bool IsWingUv(float u, float v) {
    if (u >= 0.999f && v >= 0.999f) {
        return false;
    }
    const int32_t tile = (int32_t)(u * ATLAS_TILE_COUNT);

    return tile >= WING_TILE_FIRST && tile <= WING_TILE_LAST;
}

static bool IsWingTriangle(const float* uv, uint32_t triangle) {
    int32_t wingVertices = 0;

    for (uint32_t corner = 0; corner < 3; corner++) {
        const uint32_t at = (triangle * 3 + corner) * 2;

        wingVertices += IsWingUv(uv[at], uv[at + 1]);
    }
    return wingVertices >= WING_VERTICES_MIN;
}

// The metal cap is a sphere map: the normal picks the texel of the metal texture, tile 0 of the atlas.
static void SampleChrome(const float* normal, uint8_t* outColor) {
    const float u = fminf(fmaxf((normal[0] + 1.0f) * 0.5f, 0.0f), 1.0f);
    const float v = fminf(fmaxf((normal[1] + 1.0f) * 0.5f, 0.0f), 1.0f);
    const int32_t x = (int32_t)(u * (METAL_TEXTURE_WIDTH - 1));
    const int32_t y = (int32_t)(v * (METAL_TEXTURE_HEIGHT - 1));
    const uint8_t* texel = &sAtlas[(y * SM64_TEXTURE_WIDTH + x) * 4];

    outColor[0] = texel[0];
    outColor[1] = texel[1];
    outColor[2] = texel[2];
}

static void WriteVertex(Vtx* vertex, const struct SM64MarioGeometryBuffers* geometry, uint32_t at, Vec3f offset,
                        const MarioPass* pass) {
    const float* position = &geometry->position[at * 3];
    const float* uv = &geometry->uv[at * 2];

    vertex->v.ob[0] = (s16)(position[0] / MARIO_WORLD_SCALE + offset.x);
    vertex->v.ob[1] = (s16)(position[1] / MARIO_WORLD_SCALE + offset.y);
    vertex->v.ob[2] = (s16)(position[2] / MARIO_WORLD_SCALE + offset.z);
    vertex->v.flag = 0;
    vertex->v.tc[0] = (s16)(uv[0] * SM64_TEXTURE_WIDTH * TEXEL_FRACTION_BITS);
    vertex->v.tc[1] = (s16)(uv[1] * SM64_TEXTURE_HEIGHT * TEXEL_FRACTION_BITS);
    if (pass->isMetal) {
        SampleChrome(&geometry->normal[at * 3], vertex->v.cn);
    } else {
        for (int32_t channel = 0; channel < 3; channel++) {
            vertex->v.cn[channel] = (uint8_t)(geometry->color[at * 3 + channel] * 255.0f);
        }
    }
    vertex->v.cn[3] = pass->isWingOnly ? 0 : pass->alpha;
}

static void FlushBatch(Gfx** displayList, Vtx* vertices, uint32_t count) {
    gSPVertex((*displayList)++, (uintptr_t)vertices, count, 0);
    for (uint32_t first = 0; first < count; first += 3) {
        gSP1Triangle((*displayList)++, first, first + 1, first + 2, 0);
    }
}

static void EmitTriangles(PlayState* play, const struct SM64MarioGeometryBuffers* geometry, Vec3f offset,
                          const MarioPass* pass) {
    Vtx* batch = MARIO_ALLOC_GFX(play->state.gfxCtx, VERTICES_PER_BATCH * sizeof(Vtx));
    uint32_t count = 0;

    for (uint32_t triangle = 0; triangle < geometry->numTrianglesUsed; triangle++) {
        if (pass->isWingSplit && IsWingTriangle(geometry->uv, triangle) != pass->isWingOnly) {
            continue;
        }
        for (uint32_t corner = 0; corner < 3; corner++) {
            WriteVertex(&batch[count++], geometry, triangle * 3 + corner, offset, pass);
        }
        if (count == VERTICES_PER_BATCH) {
            FlushBatch(pass->displayList, batch, count);
            batch = MARIO_ALLOC_GFX(play->state.gfxCtx, VERTICES_PER_BATCH * sizeof(Vtx));
            count = 0;
        }
    }
    if (count > 0) {
        FlushBatch(pass->displayList, batch, count);
    }
}

static void ReadSceneLight(PlayState* play, SceneLight* light) {
    const LightInfo* directional[2] = { &play->envCtx.dirLight1, &play->envCtx.dirLight2 };

    for (int32_t channel = 0; channel < 3; channel++) {
        light->ambient[channel] = play->lightCtx.ambientColor[channel];
        for (int32_t index = 0; index < 2; index++) {
            light->color[index][channel] = directional[index]->params.dir.color[channel];
        }
    }
}

// The scene's light as one colour, multiplied over the whole model by the combiner's second cycle: that is the
// only way it reaches the eyes, the cap's M and the buttons, whose texels replace the vertex colour.
static void GetSceneTint(PlayState* play, uint8_t* outTint) {
    const float strength = MarioConfig_GetShipShading();
    SceneLight light;

    ReadSceneLight(play, &light);
    for (int32_t channel = 0; channel < 3; channel++) {
        const float lit = fminf(light.ambient[channel] + (light.color[0][channel] + light.color[1][channel]) * 0.5f,
                                255.0f);

        outTint[channel] = (uint8_t)(255.0f * (1.0f + (lit / 255.0f - 1.0f) * strength));
    }
}

// Cycle 2 reads no texel: there TEXEL0 means tile 1, which nothing loads, and Mario turned black.
static void SetupMaterial(PlayState* play, Gfx** displayList, bool isTranslucent) {
    uint8_t tint[3];

    GetSceneTint(play, tint);
    gDPPipeSync((*displayList)++);
    gDPSetCycleType((*displayList)++, G_CYC_2CYCLE);
    if (isTranslucent) {
        gDPSetRenderMode((*displayList)++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
    } else {
        gDPSetRenderMode((*displayList)++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
    }
    gDPSetAlphaCompare((*displayList)++, G_AC_NONE);
    gSPClearGeometryMode((*displayList)++, G_LIGHTING | G_CULL_BOTH | G_FOG);
    gSPSetGeometryMode((*displayList)++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gSPMatrix((*displayList)++, &MARIO_IDENTITY_MTX, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPTexture((*displayList)++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetTexturePersp((*displayList)++, G_TP_PERSP);
    gDPSetTextureFilter((*displayList)++, G_TF_BILERP);
    gDPSetTileCustom((*displayList)++, G_IM_FMT_RGBA, G_IM_SIZ_32b, SM64_TEXTURE_WIDTH, SM64_TEXTURE_HEIGHT, 0,
                     G_TX_CLAMP, G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPSetTextureImage((*displayList)++, G_IM_FMT_RGBA, G_IM_SIZ_32b, SM64_TEXTURE_WIDTH, sAtlas);
    gDPLoadSync((*displayList)++);
    gDPLoadTile((*displayList)++, G_TX_LOADTILE, 0, 0, (SM64_TEXTURE_WIDTH - 1) << 2,
                (SM64_TEXTURE_HEIGHT - 1) << 2);
    gDPSetEnvColor((*displayList)++, tint[0], tint[1], tint[2], 255);
    if (isTranslucent) {
        gDPSetCombineLERP((*displayList)++, TEXEL0, SHADE, TEXEL0_ALPHA, SHADE, 0, 0, 0, SHADE, COMBINED, 0,
                          ENVIRONMENT, 0, 0, 0, 0, COMBINED);
    } else {
        gDPSetCombineLERP((*displayList)++, TEXEL0, SHADE, TEXEL0_ALPHA, SHADE, 0, 0, 0, 1, COMBINED, 0, ENVIRONMENT,
                          0, 0, 0, 0, COMBINED);
    }
}

static void SetupWingCutout(Gfx** displayList) {
    gDPPipeSync((*displayList)++);
    gDPSetRenderMode((*displayList)++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
    gDPSetAlphaCompare((*displayList)++, G_AC_THRESHOLD);
    gDPSetBlendColor((*displayList)++, 0, 0, 0, 128);
    gDPSetCombineLERP((*displayList)++, TEXEL0, SHADE, TEXEL0_ALPHA, SHADE, TEXEL0, 0, ENVIRONMENT, SHADE, COMBINED,
                      0, ENVIRONMENT, 0, 0, 0, 0, COMBINED);
}

void MarioRender_DrawMesh(PlayState* play, const struct SM64MarioGeometryBuffers* geometry, Vec3f offset,
                          uint32_t marioFlags) {
    if (geometry->numTrianglesUsed == 0 || sAtlas == NULL) {
        return;
    }

    const bool isTranslucent = (marioFlags & SM64_MARIO_VANISH_CAP) != 0;

    OPEN_DISPS(play->state.gfxCtx);

    MarioPass pass = { isTranslucent ? &POLY_XLU_DISP : &POLY_OPA_DISP, isTranslucent ? VANISH_ALPHA : OPAQUE_ALPHA,
                       (marioFlags & SM64_MARIO_METAL_CAP) != 0,
                       !isTranslucent && (marioFlags & SM64_MARIO_WING_CAP) != 0, false };

    SetupMaterial(play, pass.displayList, isTranslucent);
    EmitTriangles(play, geometry, offset, &pass);
    if (pass.isWingSplit) {
        pass.isWingOnly = true;
        SetupWingCutout(pass.displayList);
        EmitTriangles(play, geometry, offset, &pass);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
