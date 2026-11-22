#include "bk_assets.h"

#include <stdlib.h>
#include <string.h>

#include "bk_bytes.h"
#include "bk_inflate.h"

#define ASSET_TABLE 0x5E90
#define ASSET_TABLE_HEADER_SIZE 8
#define ASSET_META_SIZE 8
#define ASSET_FLAG_COMPRESSED 0x0001
#define RAREZIP_HEADER_SIZE 6

static const uint8_t* sRom;
static uint32_t sRomSize;

void BkAssets_SetRom(const uint8_t* rom, uint32_t size) {
    sRom = rom;
    sRomSize = size;
}

static uint32_t GetAssetCount(void) {
    return BkBytes_U32(sRom + ASSET_TABLE);
}

static const uint8_t* GetMeta(uint16_t assetId) {
    return sRom + ASSET_TABLE + ASSET_TABLE_HEADER_SIZE + (uint32_t)assetId * ASSET_META_SIZE;
}

static uint8_t* LoadCompressed(const uint8_t* packed, uint32_t packedSize, uint32_t* outSize) {
    if (packedSize < RAREZIP_HEADER_SIZE) {
        return NULL;
    }

    const uint32_t size = BkBytes_U32(packed + 2);
    uint8_t* data = malloc(size);

    if (data == NULL) {
        return NULL;
    }
    if (!BkInflate_Decode(packed + RAREZIP_HEADER_SIZE, packedSize - RAREZIP_HEADER_SIZE, data, size)) {
        free(data);
        return NULL;
    }
    *outSize = size;
    return data;
}

uint8_t* BkAssets_Load(uint16_t assetId, uint32_t* outSize) {
    if (sRom == NULL || assetId + 1u >= GetAssetCount()) {
        return NULL;
    }

    const uint32_t dataStart = ASSET_TABLE + ASSET_TABLE_HEADER_SIZE + GetAssetCount() * ASSET_META_SIZE;
    const uint32_t offset = BkBytes_U32(GetMeta(assetId));
    const uint32_t packedSize = BkBytes_U32(GetMeta(assetId + 1)) - offset;
    const uint16_t flags = BkBytes_U16(GetMeta(assetId) + 4);

    if (dataStart + offset + packedSize > sRomSize) {
        return NULL;
    }
    if (flags & ASSET_FLAG_COMPRESSED) {
        return LoadCompressed(sRom + dataStart + offset, packedSize, outSize);
    }

    uint8_t* data = malloc(packedSize);

    if (data != NULL) {
        memcpy(data, sRom + dataStart + offset, packedSize);
        *outSize = packedSize;
    }
    return data;
}
