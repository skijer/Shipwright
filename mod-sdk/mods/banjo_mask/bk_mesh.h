#ifndef BK_MESH_H
#define BK_MESH_H

#include <stdint.h>

#include "bk_model.h"

#define BK_SELECTOR_COUNT 64
#define BK_MATERIAL_MAX 64
#define BK_NO_TEXTURE (-1)

typedef struct BkVertex {
    float position[3];
    // In texels of the triangle's texture.
    float uv[2];
    uint8_t color[4];
} BkVertex;

typedef struct BkMaterial {
    int16_t texture;
    // G_TX_WRAP / G_TX_MIRROR / G_TX_CLAMP bits and masks of the render tile.
    uint8_t clampS;
    uint8_t clampT;
    uint8_t maskS;
    uint8_t maskT;
} BkMaterial;

typedef struct BkTriangle {
    BkVertex corners[3];
    uint8_t material;
} BkTriangle;

typedef struct BkMesh {
    BkTriangle* triangles;
    uint32_t triangleCount;
    uint32_t capacity;
    BkMaterial materials[BK_MATERIAL_MAX];
    uint32_t materialCount;
} BkMesh;

// Skins `model` posed by `bones`; `selectors` picks the geo selector branches (eyes, Kazooie, shoes...).
void BkMesh_Build(BkMesh* mesh, const BkModel* model, const BkMatrix* bones, const int16_t selectors[BK_SELECTOR_COUNT]);
void BkMesh_Free(BkMesh* mesh);

#endif
