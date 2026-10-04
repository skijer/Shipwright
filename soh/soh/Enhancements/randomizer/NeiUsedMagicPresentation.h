#pragma once
#include <stdbool.h>
#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif
// Element 0 fire, 1 ice, 2 light. These calls affect presentation only, and
// preserve the caller's CPU matrix.
void NeiUsedMagic_DrawProjectile(PlayState* play, int element, const Vec3f* position, const Vec3f* velocity,
                                 float scale, unsigned phase);
void NeiUsedMagic_DrawTrail(PlayState* play, int element, const Vec3f* positions, unsigned count, float scale);
void NeiUsedMagic_DrawCharge(PlayState* play, Player* player, int element, float charge);
// Called at the approved held model's actual focus matrix, after GI energy.
void NeiUsedMagic_DrawChargeFocus(PlayState* play, int element);
void NeiUsedMagic_DrawSpin(PlayState* play, Player* player, int element, float radius, bool big);
void NeiUsedMagic_DrawBurst(PlayState* play, int element, const Vec3f* position, float size, float life);
void NeiUsedMagic_DrawPortal(PlayState* play, Player* player, float scale, float alpha);
#ifdef __cplusplus
}
#endif
