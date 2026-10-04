#pragma once

#include <stdbool.h>
#include "soh/Enhancements/item-tables/ItemTableTypes.h"

#define CVAR_NEI_GI_EFFECTS "gEnhancements.SkijerNEI.ItemEffects"

#ifdef __cplusplus
extern "C" {
#endif
// Returns false for unrelated items. Called at the common GI/shop/world draw boundary.
bool NeiGi_Draw(PlayState* play, GetItemEntry* entry);
// Uses shelf clearance only for replacement meshes; preserves original fallback poses.
bool NeiGi_DrawShop(PlayState* play, GetItemEntry* entry);
#ifdef __cplusplus
}
#endif
