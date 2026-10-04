#pragma once

#include <stdbool.h>
#include "z64.h"

#define NEI_HELD_PATH(slug) "__OTR__objects/nei_held_redesign/" slug "/gi_dl"

#ifdef __cplusplus
extern "C" {
#endif
// Paths must have static lifetime: the graphics interpreter consumes them later.
bool NeiHeld_HasResources(const char* opaque, const char* translucent);
// Draw in the caller's current model frame without changing the CPU matrix.
// A partial archive falls back as a whole, before any graphics are queued.
bool NeiHeld_DrawModel(PlayState* play, const char* opaque, const char* translucent);
bool NeiHeld_DrawRod(PlayState* play, int element); // 0 fire, 1 ice, 2 light
bool NeiHeld_DrawMitts(Player* player, PlayState* play);
bool NeiHeld_DrawGustJar(Player* player, PlayState* play, int direction, float heat);
#ifdef __cplusplus
}
#endif
