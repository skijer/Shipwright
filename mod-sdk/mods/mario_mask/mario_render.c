#include "mario_render.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "libsm64/decomp/tools/libmio0.h"
#include "libsm64/libsm64.h"
#include "mario_config.h"
#include "mario_rom.h"
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
// A get-item keeps its own colours: like the game's get-item models it ignores the room's light.
static void SetupMaterial(PlayState* play, Gfx** displayList, bool isTranslucent, Mtx* modelView, bool isSceneLit) {
    uint8_t tint[3] = { 255, 255, 255 };

    if (isSceneLit) {
        GetSceneTint(play, tint);
    }
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
    gSPMatrix((*displayList)++, modelView, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
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

static void DrawTriangles(PlayState* play, const struct SM64MarioGeometryBuffers* geometry, Vec3f offset,
                          uint32_t marioFlags, Mtx* modelView, bool isSceneLit) {
    if (geometry->numTrianglesUsed == 0 || sAtlas == NULL) {
        return;
    }

    const bool isTranslucent = (marioFlags & SM64_MARIO_VANISH_CAP) != 0;

    OPEN_DISPS(play->state.gfxCtx);

    MarioPass pass = { isTranslucent ? &POLY_XLU_DISP : &POLY_OPA_DISP, isTranslucent ? VANISH_ALPHA : OPAQUE_ALPHA,
                       (marioFlags & SM64_MARIO_METAL_CAP) != 0,
                       !isTranslucent && (marioFlags & SM64_MARIO_WING_CAP) != 0, false };

    SetupMaterial(play, pass.displayList, isTranslucent, modelView, isSceneLit);
    EmitTriangles(play, geometry, offset, &pass);
    if (pass.isWingSplit) {
        pass.isWingOnly = true;
        SetupWingCutout(pass.displayList);
        EmitTriangles(play, geometry, offset, &pass);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void MarioRender_DrawMesh(PlayState* play, const struct SM64MarioGeometryBuffers* geometry, Vec3f offset,
                          uint32_t marioFlags) {
    DrawTriangles(play, geometry, offset, marioFlags, (Mtx*)&MARIO_IDENTITY_MTX, true);
}

void MarioRender_DrawMeshHere(PlayState* play, const struct SM64MarioGeometryBuffers* geometry, uint32_t marioFlags,
                              float scale) {
    Mtx* modelView;

    Matrix_Push();
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    modelView = MARIO_FINALIZE_MATRIX(play->state.gfxCtx);
    Matrix_Pop();
    DrawTriangles(play, geometry, (Vec3f){ 0.0f, 0.0f, 0.0f }, marioFlags, modelView, false);
}

// SM64 (US) packs the cap boxes in the common0 bank, MIO0, loaded as segment 8; offsets are the decomp's.
#define COMMON0_ROM_OFFSET 0x1F2200
#define WING_CAP_BOX_DL 0x19318
#define METAL_CAP_BOX_DL 0x19378
#define VANISH_CAP_BOX_DL 0x193D8
#define SEGMENT_OFFSET(address) ((address) & 0x00FFFFFF)
#define F3D_COMMAND_SIZE 8
#define F3D_VERTEX_SIZE 16
#define F3D_TRIANGLE_INDEX_STEP 10
#define F3D_BRANCH 1
#define F3D_DEPTH_MAX 4
#define CAP_BOX_DRAW_MAX 2
#define CAP_BOX_VERTEX_MAX 16
#define CAP_BOX_TRIANGLE_MAX 8
#define CAP_BOX_GET_ITEM_SIZE 40.0f

enum F3dOpcode {
    F3D_VTX = 0x04,
    F3D_DL = 0x06,
    F3D_ENDDL = 0xB8,
    F3D_TRI1 = 0xBF,
    F3D_SETTILESIZE = 0xF2,
    F3D_SETTIMG = 0xFD,
};

typedef struct CapBoxDraw {
    const uint8_t* texture;
    uint16_t width;
    uint16_t height;
    Vtx vertices[CAP_BOX_VERTEX_MAX];
    uint8_t vertexCount;
    uint8_t triangles[CAP_BOX_TRIANGLE_MAX][3];
    uint8_t triangleCount;
} CapBoxDraw;

typedef struct CapBox {
    uint32_t capFlag;
    uint32_t displayList;
    bool isRead;
    CapBoxDraw draws[CAP_BOX_DRAW_MAX];
    uint8_t drawCount;
    float center[3];
    float size;
} CapBox;

typedef struct CapBoxReader {
    const uint8_t* bank;
    uint32_t bankSize;
    CapBox* box;
    const uint8_t* texture;
    uint16_t width;
    uint16_t height;
} CapBoxReader;

static CapBox sCapBoxes[] = {
    { SM64_MARIO_WING_CAP, WING_CAP_BOX_DL },
    { SM64_MARIO_METAL_CAP, METAL_CAP_BOX_DL },
    { SM64_MARIO_VANISH_CAP, VANISH_CAP_BOX_DL },
};
static uint8_t* sCommon0;
static uint32_t sCommon0Size;

static uint32_t ReadWord(const uint8_t* at) {
    return ((uint32_t)at[0] << 24) | ((uint32_t)at[1] << 16) | ((uint32_t)at[2] << 8) | at[3];
}

static int16_t ReadHalf(const uint8_t* at) {
    return (int16_t)((at[0] << 8) | at[1]);
}

// The bank stays decoded: the boxes' textures are read from it at every draw.
static bool LoadCommon0(void) {
    const uint8_t* rom = MarioRom_Get();
    mio0_header_t header;

    if (sCommon0 != NULL) {
        return true;
    }
    if (rom == NULL || memcmp(rom + COMMON0_ROM_OFFSET, "MIO0", 4) != 0 ||
        !mio0_decode_header(rom + COMMON0_ROM_OFFSET, &header)) {
        return false;
    }
    sCommon0 = malloc(header.dest_size);
    if (sCommon0 == NULL) {
        return false;
    }
    mio0_decode(rom + COMMON0_ROM_OFFSET, sCommon0, NULL);
    sCommon0Size = header.dest_size;
    return true;
}

static bool ReadVertices(CapBoxReader* reader, uint32_t command, uint32_t address) {
    const uint32_t count = ((command >> 20) & 0xF) + 1;
    const uint32_t offset = SEGMENT_OFFSET(address);

    if (reader->box->drawCount == CAP_BOX_DRAW_MAX || count > CAP_BOX_VERTEX_MAX ||
        offset + count * F3D_VERTEX_SIZE > reader->bankSize) {
        return false;
    }

    CapBoxDraw* draw = &reader->box->draws[reader->box->drawCount++];

    draw->texture = reader->texture;
    draw->width = reader->width;
    draw->height = reader->height;
    draw->vertexCount = (uint8_t)count;
    for (uint32_t index = 0; index < count; index++) {
        const uint8_t* source = reader->bank + offset + index * F3D_VERTEX_SIZE;
        Vtx_t* vertex = &draw->vertices[index].v;

        for (int32_t axis = 0; axis < 3; axis++) {
            vertex->ob[axis] = ReadHalf(source + axis * 2);
        }
        vertex->flag = 0;
        vertex->tc[0] = ReadHalf(source + 8);
        vertex->tc[1] = ReadHalf(source + 10);
        memcpy(vertex->cn, source + 12, 4);
    }
    return true;
}

static bool ReadTriangle(CapBoxReader* reader, uint32_t indices) {
    CapBoxDraw* draw = reader->box->drawCount > 0 ? &reader->box->draws[reader->box->drawCount - 1] : NULL;

    if (draw == NULL || draw->triangleCount == CAP_BOX_TRIANGLE_MAX) {
        return false;
    }
    for (int32_t corner = 0; corner < 3; corner++) {
        const uint32_t index = ((indices >> (16 - corner * 8)) & 0xFF) / F3D_TRIANGLE_INDEX_STEP;

        if (index >= draw->vertexCount) {
            return false;
        }
        draw->triangles[draw->triangleCount][corner] = (uint8_t)index;
    }
    draw->triangleCount++;
    return true;
}

// Follows one of the box's F3D lists, keeping only what the box needs: textures, vertices, triangles.
static bool ReadCapBoxList(CapBoxReader* reader, uint32_t offset, int32_t depth) {
    if (depth > F3D_DEPTH_MAX) {
        return false;
    }
    for (; offset + F3D_COMMAND_SIZE <= reader->bankSize; offset += F3D_COMMAND_SIZE) {
        const uint32_t command = ReadWord(reader->bank + offset);
        const uint32_t argument = ReadWord(reader->bank + offset + 4);

        switch (command >> 24) {
            case F3D_SETTIMG:
                if (SEGMENT_OFFSET(argument) >= reader->bankSize) {
                    return false;
                }
                reader->texture = reader->bank + SEGMENT_OFFSET(argument);
                break;
            case F3D_SETTILESIZE:
                reader->width = (uint16_t)(((argument >> 12) & 0xFFF) / 4 + 1);
                reader->height = (uint16_t)((argument & 0xFFF) / 4 + 1);
                break;
            case F3D_VTX:
                if (!ReadVertices(reader, command, argument)) {
                    return false;
                }
                break;
            case F3D_TRI1:
                if (!ReadTriangle(reader, argument)) {
                    return false;
                }
                break;
            case F3D_DL:
                if (!ReadCapBoxList(reader, SEGMENT_OFFSET(argument), depth + 1)) {
                    return false;
                }
                if (((command >> 16) & 0xFF) == F3D_BRANCH) {
                    return true;
                }
                break;
            case F3D_ENDDL:
                return true;
            default:
                break;
        }
    }
    return false;
}

static void MeasureCapBox(CapBox* box) {
    float low[3] = { INFINITY, INFINITY, INFINITY };
    float high[3] = { -INFINITY, -INFINITY, -INFINITY };

    for (uint8_t drawIndex = 0; drawIndex < box->drawCount; drawIndex++) {
        const CapBoxDraw* draw = &box->draws[drawIndex];

        for (uint8_t index = 0; index < draw->vertexCount; index++) {
            for (int32_t axis = 0; axis < 3; axis++) {
                low[axis] = fminf(low[axis], draw->vertices[index].v.ob[axis]);
                high[axis] = fmaxf(high[axis], draw->vertices[index].v.ob[axis]);
            }
        }
    }
    box->size = 0.0f;
    for (int32_t axis = 0; axis < 3; axis++) {
        box->center[axis] = (low[axis] + high[axis]) * 0.5f;
        box->size = fmaxf(box->size, high[axis] - low[axis]);
    }
}

static CapBox* GetCapBox(uint32_t capFlag) {
    for (size_t index = 0; index < sizeof(sCapBoxes) / sizeof(sCapBoxes[0]); index++) {
        CapBox* box = &sCapBoxes[index];

        if (box->capFlag != capFlag) {
            continue;
        }
        if (!box->isRead && LoadCommon0()) {
            CapBoxReader reader = { sCommon0, sCommon0Size, box };

            box->isRead = true;
            if (!ReadCapBoxList(&reader, box->displayList, 0) || box->drawCount == 0) {
                box->drawCount = 0;
            }
            MeasureCapBox(box);
        }
        return box->drawCount > 0 ? box : NULL;
    }
    return NULL;
}

static uint32_t TextureMask(uint16_t length) {
    uint32_t mask = 0;

    while ((1u << mask) < length) {
        mask++;
    }
    return mask;
}

bool MarioRender_DrawCapBox(PlayState* play, uint32_t capFlag) {
    const CapBox* box = GetCapBox(capFlag);

    if (box == NULL || box->size <= 0.0f) {
        return false;
    }

    const float scale = CAP_BOX_GET_ITEM_SIZE / box->size;

    OPEN_DISPS(play->state.gfxCtx);
    MARIO_SETUP_OPA(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    Matrix_Translate(-box->center[0], -box->center[1], -box->center[2], MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MARIO_FINALIZE_MATRIX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    Matrix_Pop();
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATERGB, G_CC_MODULATERGB);
    gSPTexture(POLY_OPA_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetTextureLUT(POLY_OPA_DISP++, G_TT_NONE);
    for (uint8_t drawIndex = 0; drawIndex < box->drawCount; drawIndex++) {
        const CapBoxDraw* draw = &box->draws[drawIndex];

        gDPLoadTextureBlock(POLY_OPA_DISP++, draw->texture, G_IM_FMT_RGBA, G_IM_SIZ_16b, draw->width, draw->height, 0,
                            G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, TextureMask(draw->width),
                            TextureMask(draw->height), G_TX_NOLOD, G_TX_NOLOD);
        gSPVertex(POLY_OPA_DISP++, (uintptr_t)draw->vertices, draw->vertexCount, 0);
        for (uint8_t index = 0; index < draw->triangleCount; index++) {
            gSP1Triangle(POLY_OPA_DISP++, draw->triangles[index][0], draw->triangles[index][1],
                         draw->triangles[index][2], 0);
        }
    }
    CLOSE_DISPS(play->state.gfxCtx);
    return true;
}
