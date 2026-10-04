#pragma once

#include <stdbool.h>
#include <stdint.h>

struct Player;
struct PlayState;

#ifdef __cplusplus
extern "C" {
#endif
// Presentation-only replacement. False preserves the original lantern draw.
bool NeiLantern_DrawHeld(struct Player* player, struct PlayState* play, uint8_t fireType);
#ifdef __cplusplus
}
#endif
