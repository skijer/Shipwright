#ifndef BK_ASSETS_H
#define BK_ASSETS_H

#include <stdint.h>

// Assets of the user's Banjo-Kazooie (USA 1.0) ROM. The ROM must stay alive while assets are read.
void BkAssets_SetRom(const uint8_t* rom, uint32_t size);
// A new buffer with the asset's bytes, decompressed; the caller frees it. NULL when the id is not an asset.
uint8_t* BkAssets_Load(uint16_t assetId, uint32_t* outSize);

#endif
