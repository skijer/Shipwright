#ifndef SOH_MOD_API_VANILLA_ITEMS_H
#define SOH_MOD_API_VANILLA_ITEMS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SOH_VANILLA_ITEM_UPGRADE = 0,
    SOH_VANILLA_ITEM_REPLACE,
    SOH_VANILLA_ITEM_BLOCK,
} SOHVanillaItemMode;

bool VanillaItems_Block(uint16_t vanillaItem, int32_t randoItem);
const char* VanillaItems_GetCustomKey(uint16_t vanillaItem);
const char* VanillaItems_GetCustomKeyForRandoItem(int32_t randoItem);
bool VanillaItems_GrantRandoItem(int32_t randoItem);
bool VanillaItems_Give(struct PlayState* play, uint16_t vanillaItem);
bool VanillaItems_Drop(struct PlayState* play, uint16_t vanillaItem);
bool VanillaItems_IsVanillaSuppressed(uint16_t vanillaItem);
bool VanillaItems_IsRandoItemSuppressed(int32_t randoItem);

#ifdef __cplusplus
}

void VanillaItems_Init();
#endif

#endif
