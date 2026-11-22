#ifndef BK_MODEL_H
#define BK_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#include "bk_animation.h"

typedef float BkMatrix[4][4];

typedef enum BkTextureFormat {
    BK_TEXTURE_CI4,
    BK_TEXTURE_CI8,
    BK_TEXTURE_RGBA16,
    BK_TEXTURE_RGBA32,
} BkTextureFormat;

// Kept in the ROM's own format and byte order, which is what fits TMEM and what the RDP loads.
typedef struct BkTexture {
    uint32_t imageOffset;
    uint16_t width;
    uint16_t height;
    BkTextureFormat format;
    const uint8_t* pixels;
    // RGBA16 colours for the CI formats, NULL otherwise.
    const uint8_t* palette;
} BkTexture;

typedef struct BkModel {
    uint8_t* data;
    uint32_t size;
    uint32_t geoOffset;
    uint32_t gfxOffset;
    uint32_t verticesOffset;
    uint32_t bonesOffset;
    float boneScale;
    uint16_t boneCount;
    uint16_t textureCount;
    BkTexture* textures;
} BkModel;

bool BkModel_Load(BkModel* model, uint16_t assetId);
void BkModel_Free(BkModel* model);
// Each bone's matrix (row vectors, model space) for one pose, as anim_defrag.c builds them.
void BkModel_ComputeBoneMatrices(const BkModel* model, const BkBoneChannels channels[BK_BONE_MAX], BkMatrix* out);
// The texture whose pixels start at `imageOffset` in segment 2, or -1.
int32_t BkModel_FindTexture(const BkModel* model, uint32_t imageOffset);

#endif
