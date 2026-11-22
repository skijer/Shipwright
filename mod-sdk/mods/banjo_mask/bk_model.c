#include "bk_model.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "bk_assets.h"
#include "bk_bytes.h"

#define HEADER_GEO 0x04
#define HEADER_TEXTURES 0x08
#define HEADER_GFX 0x0C
#define HEADER_VERTICES 0x10
#define HEADER_BONES 0x18
#define VERTEX_LIST_HEADER_SIZE 0x18
#define TEXTURE_LIST_HEADER_SIZE 8
#define TEXTURE_INFO_SIZE 16
#define BONE_LIST_HEADER_SIZE 8
#define BONE_SIZE 16
#define DEGREES_TO_RADIANS (3.14159265f / 180.0f)

enum TextureType {
    TEXTURE_CI4 = 0x01,
    TEXTURE_CI8 = 0x02,
    TEXTURE_RGBA16 = 0x04,
    TEXTURE_RGBA32 = 0x08,
};

// Colour-indexed textures carry their RGBA16 palette right before the pixels.
static uint32_t GetPaletteSize(uint16_t type) {
    if (type & TEXTURE_CI4) {
        return 16 * 2;
    }
    return (type & TEXTURE_CI8) ? 256 * 2 : 0;
}

static BkTextureFormat GetFormat(uint16_t type) {
    if (type & TEXTURE_CI4) {
        return BK_TEXTURE_CI4;
    }
    if (type & TEXTURE_CI8) {
        return BK_TEXTURE_CI8;
    }
    return (type & TEXTURE_RGBA16) ? BK_TEXTURE_RGBA16 : BK_TEXTURE_RGBA32;
}

static bool LoadTextures(BkModel* model) {
    const uint32_t listOffset = BkBytes_U16(model->data + HEADER_TEXTURES);

    if (listOffset == 0) {
        return true;
    }
    model->textureCount = BkBytes_U16(model->data + listOffset + 4);
    model->textures = calloc(model->textureCount, sizeof(BkTexture));
    if (model->textures == NULL) {
        return false;
    }

    const uint32_t pixelsStart = listOffset + TEXTURE_LIST_HEADER_SIZE + model->textureCount * TEXTURE_INFO_SIZE;

    for (uint16_t index = 0; index < model->textureCount; index++) {
        const uint8_t* info = model->data + listOffset + TEXTURE_LIST_HEADER_SIZE + index * TEXTURE_INFO_SIZE;
        const uint32_t offset = BkBytes_U32(info);
        const uint16_t type = BkBytes_U16(info + 4);
        BkTexture* texture = &model->textures[index];

        texture->width = info[8];
        texture->height = info[9];
        texture->imageOffset = offset + GetPaletteSize(type);
        texture->format = GetFormat(type);
        if (pixelsStart + texture->imageOffset >= model->size) {
            return false;
        }
        texture->palette = (GetPaletteSize(type) != 0) ? model->data + pixelsStart + offset : NULL;
        texture->pixels = model->data + pixelsStart + texture->imageOffset;
    }
    return true;
}

bool BkModel_Load(BkModel* model, uint16_t assetId) {
    *model = (BkModel){ 0 };
    model->data = BkAssets_Load(assetId, &model->size);
    if (model->data == NULL) {
        return false;
    }
    model->geoOffset = BkBytes_U32(model->data + HEADER_GEO);
    model->gfxOffset = BkBytes_U32(model->data + HEADER_GFX);
    model->verticesOffset = BkBytes_U32(model->data + HEADER_VERTICES) + VERTEX_LIST_HEADER_SIZE;
    model->bonesOffset = BkBytes_U32(model->data + HEADER_BONES);
    if (model->bonesOffset != 0) {
        model->boneScale = BkBytes_F32(model->data + model->bonesOffset);
        model->boneCount = BkBytes_U16(model->data + model->bonesOffset + 4);
    }
    if (!LoadTextures(model)) {
        BkModel_Free(model);
        return false;
    }
    return true;
}

void BkModel_Free(BkModel* model) {
    free(model->textures);
    free(model->data);
    *model = (BkModel){ 0 };
}

int32_t BkModel_FindTexture(const BkModel* model, uint32_t imageOffset) {
    for (uint16_t index = 0; index < model->textureCount; index++) {
        if (model->textures[index].imageOffset == imageOffset) {
            return index;
        }
    }
    return -1;
}

static void SetIdentity(BkMatrix out) {
    memset(out, 0, sizeof(BkMatrix));
    for (int32_t i = 0; i < 4; i++) {
        out[i][i] = 1.0f;
    }
}

// ml matrices take row vectors and every operation goes before the current one: out = op * m.
static void PreMultiply(BkMatrix m, BkMatrix op) {
    BkMatrix result;

    for (int32_t row = 0; row < 4; row++) {
        for (int32_t column = 0; column < 4; column++) {
            result[row][column] = op[row][0] * m[0][column] + op[row][1] * m[1][column] + op[row][2] * m[2][column] +
                                  op[row][3] * m[3][column];
        }
    }
    memcpy(m, result, sizeof(BkMatrix));
}

static void Translate(BkMatrix m, float x, float y, float z) {
    BkMatrix op;

    SetIdentity(op);
    op[3][0] = x;
    op[3][1] = y;
    op[3][2] = z;
    PreMultiply(m, op);
}

static void Scale(BkMatrix m, float x, float y, float z) {
    BkMatrix op;

    SetIdentity(op);
    op[0][0] = x;
    op[1][1] = y;
    op[2][2] = z;
    PreMultiply(m, op);
}

// mlMtxRotRoll, then mlMtxRotYaw, then mlMtxRotPitch: the pitch ends up applied first.
static void Rotate(BkMatrix m, float pitch, float yaw, float roll) {
    const float cosX = cosf(pitch * DEGREES_TO_RADIANS);
    const float sinX = sinf(pitch * DEGREES_TO_RADIANS);
    const float cosY = cosf(yaw * DEGREES_TO_RADIANS);
    const float sinY = sinf(yaw * DEGREES_TO_RADIANS);
    const float cosZ = cosf(roll * DEGREES_TO_RADIANS);
    const float sinZ = sinf(roll * DEGREES_TO_RADIANS);
    BkMatrix rollMatrix = { { cosZ, sinZ, 0, 0 }, { -sinZ, cosZ, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } };
    BkMatrix yawMatrix = { { cosY, 0, -sinY, 0 }, { 0, 1, 0, 0 }, { sinY, 0, cosY, 0 }, { 0, 0, 0, 1 } };
    BkMatrix pitchMatrix = { { 1, 0, 0, 0 }, { 0, cosX, sinX, 0 }, { 0, -sinX, cosX, 0 }, { 0, 0, 0, 1 } };

    PreMultiply(m, rollMatrix);
    PreMultiply(m, yawMatrix);
    PreMultiply(m, pitchMatrix);
}

void BkModel_ComputeBoneMatrices(const BkModel* model, const BkBoneChannels channels[BK_BONE_MAX], BkMatrix* out) {
    for (uint16_t index = 0; index < model->boneCount; index++) {
        const uint8_t* bone = model->data + model->bonesOffset + BONE_LIST_HEADER_SIZE + index * BONE_SIZE;
        const float pivot[3] = { BkBytes_F32(bone), BkBytes_F32(bone + 4), BkBytes_F32(bone + 8) };
        const int16_t boneId = BkBytes_S16(bone + 12);
        const int16_t parent = BkBytes_S16(bone + 14);
        const float* values = (boneId >= 0 && boneId < BK_BONE_MAX) ? channels[boneId].values : NULL;
        const float k = model->boneScale;
        BkMatrix rotation;

        if (parent < 0 || parent >= index) {
            SetIdentity(out[index]);
        } else {
            memcpy(out[index], out[parent], sizeof(BkMatrix));
        }
        if (values == NULL) {
            continue;
        }
        Translate(out[index], pivot[0] + k * values[6], pivot[1] + k * values[7], pivot[2] + k * values[8]);
        SetIdentity(rotation);
        Rotate(rotation, values[0], values[1], values[2]);
        PreMultiply(out[index], rotation);
        Scale(out[index], values[3], values[4], values[5]);
        Translate(out[index], -pivot[0], -pivot[1], -pivot[2]);
    }
}
