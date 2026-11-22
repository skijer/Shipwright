#ifndef ASSETS_H
#define ASSETS_H

#include "../shim/ultra64.h"
#include "../include/structs.h"

typedef struct asset_rom_table_head_s{
    u32 count;
    u32 unk4;
} AssetROMHead;

typedef struct asset_file_meta_s{
    u32 offset;
    s16 compFlag;
    s16 unk6;
} AssetFileMeta;


extern u8 assets_ROM_START[]; //rom file asset bin;

#endif
