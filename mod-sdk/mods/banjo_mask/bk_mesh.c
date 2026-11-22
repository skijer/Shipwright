#include "bk_mesh.h"

#include <stdlib.h>
#include <string.h>

#include "bk_bytes.h"

#define VERTEX_CACHE_SIZE 32
#define VERTEX_SIZE 16
#define COMMAND_SIZE 8
#define GFX_LIST_HEADER_SIZE 8
#define TEXEL_FRACTION 32.0f
#define TEXTURE_SCALE_ONE 65536.0f
#define LOD_NEAR_DISTANCE 1.0f
#define MATRIX_STACK_SIZE 32

enum GeoCommand {
    GEO_BILLBOARD = 0x00,
    GEO_SORT = 0x01,
    GEO_BONE = 0x02,
    GEO_LOAD_DL = 0x03,
    GEO_SKINNING = 0x05,
    GEO_CALL = 0x06,
    GEO_LOAD_DL_2 = 0x07,
    GEO_LOD = 0x08,
    GEO_SELECTOR = 0x0C,
    GEO_DRAW_DISTANCE = 0x0D,
    GEO_UNK_E = 0x0E,
    GEO_CAMERA = 0x0F,
};

enum F3dexOpcode {
    F3DEX_VERTEX = 0x04,
    F3DEX_DISPLAY_LIST = 0x06,
    F3DEX_POP_MATRIX = 0xBD,
    F3DEX_TRIANGLE_2 = 0xB1,
    F3DEX_END_DISPLAY_LIST = 0xB8,
    F3DEX_TEXTURE = 0xBB,
    F3DEX_TRIANGLE_1 = 0xBF,
    F3DEX_SET_TILE_SIZE = 0xF2,
    F3DEX_SET_TILE = 0xF5,
    F3DEX_SET_TEXTURE_IMAGE = 0xFD,
};

#define RENDER_TILE 0
#define TEXTURE_SEGMENT 0x02

typedef struct CachedVertex {
    bool isLoaded;
    BkVertex vertex;
} CachedVertex;

// The geo walk and the RSP keep separate matrix stacks: a skinning list pops the RSP's back to the parent bone, so
// the joint's vertices load under the parent while the walk is still inside the child.
typedef struct MatrixStack {
    const BkMatrix* matrices[MATRIX_STACK_SIZE];
    int32_t depth;
} MatrixStack;

typedef struct Skinner {
    BkMesh* mesh;
    const BkModel* model;
    const BkMatrix* bones;
    const int16_t* selectors;
    MatrixStack walk;
    MatrixStack rsp;
    CachedVertex cache[VERTEX_CACHE_SIZE];
    int16_t texture;
    bool isTextureOn;
    float scaleS;
    float scaleT;
    float tileS;
    float tileT;
    BkMaterial tile;
} Skinner;

static const BkMatrix kIdentity = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } };

static bool PushMatrix(MatrixStack* stack, const BkMatrix* matrix) {
    if (stack->depth + 1 >= MATRIX_STACK_SIZE) {
        return false;
    }
    stack->matrices[++stack->depth] = matrix;
    return true;
}

static void PopMatrix(MatrixStack* stack) {
    if (stack->depth > 0) {
        stack->depth--;
    }
}

static const uint8_t* At(const Skinner* skinner, uint32_t offset) {
    return skinner->model->data + offset;
}

static uint8_t FindMaterial(Skinner* skinner) {
    BkMesh* mesh = skinner->mesh;
    BkMaterial wanted = skinner->tile;

    wanted.texture = skinner->isTextureOn ? skinner->texture : BK_NO_TEXTURE;
    for (uint32_t index = 0; index < mesh->materialCount; index++) {
        if (memcmp(&mesh->materials[index], &wanted, sizeof(wanted)) == 0) {
            return (uint8_t)index;
        }
    }
    if (mesh->materialCount == BK_MATERIAL_MAX) {
        return 0;
    }
    mesh->materials[mesh->materialCount] = wanted;
    return (uint8_t)mesh->materialCount++;
}

static void AddTriangle(Skinner* skinner, int32_t a, int32_t b, int32_t c) {
    BkMesh* mesh = skinner->mesh;
    const int32_t corners[3] = { a / 2, b / 2, c / 2 };

    for (int32_t i = 0; i < 3; i++) {
        if (corners[i] >= VERTEX_CACHE_SIZE || !skinner->cache[corners[i]].isLoaded) {
            return;
        }
    }
    if (mesh->triangleCount == mesh->capacity) {
        const uint32_t capacity = (mesh->capacity == 0) ? 1024 : mesh->capacity * 2;
        BkTriangle* triangles = realloc(mesh->triangles, capacity * sizeof(BkTriangle));

        if (triangles == NULL) {
            return;
        }
        mesh->triangles = triangles;
        mesh->capacity = capacity;
    }

    BkTriangle* triangle = &mesh->triangles[mesh->triangleCount++];

    for (int32_t i = 0; i < 3; i++) {
        triangle->corners[i] = skinner->cache[corners[i]].vertex;
    }
    triangle->material = FindMaterial(skinner);
}

// The RSP transforms a vertex when it is loaded, so skinned joints keep vertices loaded under the previous bone.
static void LoadVertices(Skinner* skinner, uint32_t word0, uint32_t word1) {
    const int32_t first = ((word0 >> 16) & 0xFF) / 2;
    const int32_t count = (word0 >> 10) & 0x3F;
    const BkMatrix* matrix = skinner->rsp.matrices[skinner->rsp.depth];
    const uint8_t* source = At(skinner, skinner->model->verticesOffset + (word1 & 0xFFFFFF));

    for (int32_t i = 0; i < count && first + i < VERTEX_CACHE_SIZE; i++) {
        const uint8_t* raw = source + i * VERTEX_SIZE;
        const float local[3] = { BkBytes_S16(raw), BkBytes_S16(raw + 2), BkBytes_S16(raw + 4) };
        CachedVertex* cached = &skinner->cache[first + i];

        for (int32_t axis = 0; axis < 3; axis++) {
            cached->vertex.position[axis] = local[0] * (*matrix)[0][axis] + local[1] * (*matrix)[1][axis] +
                                            local[2] * (*matrix)[2][axis] + (*matrix)[3][axis];
        }
        cached->vertex.uv[0] = BkBytes_S16(raw + 8) * skinner->scaleS / TEXEL_FRACTION - skinner->tileS;
        cached->vertex.uv[1] = BkBytes_S16(raw + 10) * skinner->scaleT / TEXEL_FRACTION - skinner->tileT;
        memcpy(cached->vertex.color, raw + 12, 4);
        cached->isLoaded = true;
    }
}

static void SetTile(Skinner* skinner, uint32_t word1) {
    if (((word1 >> 24) & 7) != RENDER_TILE) {
        return;
    }
    skinner->tile.clampT = (word1 >> 18) & 3;
    skinner->tile.maskT = (word1 >> 14) & 0xF;
    skinner->tile.clampS = (word1 >> 8) & 3;
    skinner->tile.maskS = (word1 >> 4) & 0xF;
}

// Palette loads point before the pixels of their texture, so only the pixel load matches one.
static void SetTextureImage(Skinner* skinner, uint32_t word1) {
    const int32_t texture = BkModel_FindTexture(skinner->model, word1 & 0xFFFFFF);

    if ((word1 >> 24) == TEXTURE_SEGMENT && texture >= 0) {
        skinner->texture = (int16_t)texture;
    }
}

// Calls into segment 3 reach per-frame material lists the game builds, not geometry.
static void RunDisplayList(Skinner* skinner, int32_t index) {
    const uint32_t listStart = skinner->model->gfxOffset + GFX_LIST_HEADER_SIZE;
    const uint32_t commandCount = BkBytes_U32(At(skinner, skinner->model->gfxOffset));

    for (uint32_t at = (uint32_t)index; at < commandCount; at++) {
        const uint8_t* command = At(skinner, listStart + at * COMMAND_SIZE);
        const uint32_t word0 = BkBytes_U32(command);
        const uint32_t word1 = BkBytes_U32(command + 4);

        switch (word0 >> 24) {
            case F3DEX_VERTEX:
                LoadVertices(skinner, word0, word1);
                break;
            case F3DEX_TRIANGLE_1:
                AddTriangle(skinner, (word1 >> 16) & 0xFF, (word1 >> 8) & 0xFF, word1 & 0xFF);
                break;
            case F3DEX_TRIANGLE_2:
                AddTriangle(skinner, (word0 >> 16) & 0xFF, (word0 >> 8) & 0xFF, word0 & 0xFF);
                AddTriangle(skinner, (word1 >> 16) & 0xFF, (word1 >> 8) & 0xFF, word1 & 0xFF);
                break;
            case F3DEX_TEXTURE:
                skinner->isTextureOn = (word0 & 0xFF) != 0;
                skinner->scaleS = (word1 >> 16) / TEXTURE_SCALE_ONE;
                skinner->scaleT = (word1 & 0xFFFF) / TEXTURE_SCALE_ONE;
                break;
            case F3DEX_SET_TEXTURE_IMAGE:
                SetTextureImage(skinner, word1);
                break;
            case F3DEX_SET_TILE:
                SetTile(skinner, word1);
                break;
            case F3DEX_SET_TILE_SIZE:
                skinner->tileS = ((word0 >> 12) & 0xFFF) / 4.0f;
                skinner->tileT = (word0 & 0xFFF) / 4.0f;
                break;
            case F3DEX_POP_MATRIX:
                PopMatrix(&skinner->rsp);
                break;
            case F3DEX_DISPLAY_LIST:
                if ((word0 >> 16) & 0xFF) {
                    return;
                }
                break;
            case F3DEX_END_DISPLAY_LIST:
                return;
            default:
                break;
        }
    }
}

static void RunGeo(Skinner* skinner, uint32_t offset);

static void RunBranch(Skinner* skinner, uint32_t offset, int32_t branch) {
    if (branch != 0) {
        RunGeo(skinner, offset + branch);
    }
}

static void RunBone(Skinner* skinner, uint32_t offset) {
    const uint8_t branch = *At(skinner, offset + 8);
    const int8_t matrixId = (int8_t)*At(skinner, offset + 9);
    const bool hasMatrix = matrixId >= 0 && matrixId < skinner->model->boneCount;
    const bool isPushed = hasMatrix && PushMatrix(&skinner->walk, &skinner->bones[matrixId]);

    if (isPushed) {
        PushMatrix(&skinner->rsp, &skinner->bones[matrixId]);
    }
    RunBranch(skinner, offset, branch);
    if (isPushed) {
        PopMatrix(&skinner->walk);
        PopMatrix(&skinner->rsp);
    }
}

// modelRender_geoCmd_SKINNING: the first list runs as is, each later one after loading the walk's matrix again.
static void RunSkinning(Skinner* skinner, uint32_t offset) {
    uint32_t at = offset + 8;

    RunDisplayList(skinner, BkBytes_S16(At(skinner, at)));
    for (at += 2; BkBytes_S16(At(skinner, at)) != 0; at += 2) {
        PushMatrix(&skinner->rsp, skinner->walk.matrices[skinner->walk.depth]);
        RunDisplayList(skinner, BkBytes_S16(At(skinner, at)));
    }
}

static void RunSelector(Skinner* skinner, uint32_t offset) {
    const int16_t count = BkBytes_S16(At(skinner, offset + 8));
    const int16_t index = BkBytes_S16(At(skinner, offset + 10));
    const int16_t selection = (index > 0 && index < BK_SELECTOR_COUNT) ? skinner->selectors[index] : 0;

    if (selection > 0 && selection <= count) {
        RunBranch(skinner, offset, BkBytes_S32(At(skinner, offset + 12 + (selection - 1) * 4)));
        return;
    }
    for (int32_t bits = -selection, branch = 0; selection < 0 && branch < count; bits >>= 1, branch++) {
        if (bits & 1) {
            RunBranch(skinner, offset, BkBytes_S32(At(skinner, offset + 12 + branch * 4)));
        }
    }
}

// The closest level of detail, as the port draws with LOD disabled.
static void RunLod(Skinner* skinner, uint32_t offset) {
    const float farthest = BkBytes_F32(At(skinner, offset + 8));
    const float nearest = BkBytes_F32(At(skinner, offset + 12));

    if (nearest < LOD_NEAR_DISTANCE && LOD_NEAR_DISTANCE <= farthest) {
        RunBranch(skinner, offset, BkBytes_S32(At(skinner, offset + 0x1C)));
    }
}

static void RunCommand(Skinner* skinner, uint32_t offset) {
    switch (BkBytes_U32(At(skinner, offset))) {
        case GEO_BILLBOARD:
        case GEO_CAMERA:
            RunBranch(skinner, offset, BkBytes_S16(At(skinner, offset + 8)));
            break;
        case GEO_SORT:
            RunBranch(skinner, offset, BkBytes_S16(At(skinner, offset + 0x22)));
            RunBranch(skinner, offset, BkBytes_S32(At(skinner, offset + 0x24)));
            break;
        case GEO_BONE:
            RunBone(skinner, offset);
            break;
        case GEO_LOAD_DL:
            RunDisplayList(skinner, BkBytes_S16(At(skinner, offset + 8)));
            break;
        case GEO_SKINNING:
            RunSkinning(skinner, offset);
            break;
        case GEO_CALL:
            RunBranch(skinner, offset, BkBytes_S32(At(skinner, offset + 8)));
            break;
        case GEO_LOAD_DL_2:
            RunDisplayList(skinner, BkBytes_S16(At(skinner, offset + 10)));
            break;
        case GEO_LOD:
            RunLod(skinner, offset);
            break;
        case GEO_SELECTOR:
            RunSelector(skinner, offset);
            break;
        case GEO_DRAW_DISTANCE:
            RunBranch(skinner, offset, BkBytes_S16(At(skinner, offset + 0x14)));
            break;
        case GEO_UNK_E:
            RunBranch(skinner, offset, BkBytes_S16(At(skinner, offset + 0x10)));
            break;
        default:
            break;
    }
}

static void RunGeo(Skinner* skinner, uint32_t offset) {
    while (offset + 8 <= skinner->model->size) {
        const int32_t next = BkBytes_S32(At(skinner, offset + 4));

        RunCommand(skinner, offset);
        if (next == 0) {
            return;
        }
        offset += next;
    }
}

void BkMesh_Build(BkMesh* mesh, const BkModel* model, const BkMatrix* bones, const int16_t selectors[BK_SELECTOR_COUNT]) {
    Skinner skinner = { 0 };

    mesh->triangleCount = 0;
    mesh->materialCount = 0;
    skinner.mesh = mesh;
    skinner.model = model;
    skinner.bones = bones;
    skinner.selectors = selectors;
    skinner.walk.matrices[0] = &kIdentity;
    skinner.rsp.matrices[0] = &kIdentity;
    skinner.texture = BK_NO_TEXTURE;
    skinner.scaleS = 1.0f;
    skinner.scaleT = 1.0f;
    RunGeo(&skinner, model->geoOffset);
}

void BkMesh_Free(BkMesh* mesh) {
    free(mesh->triangles);
    *mesh = (BkMesh){ 0 };
}
