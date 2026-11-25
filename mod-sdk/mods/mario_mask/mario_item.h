#ifndef MARIO_ITEM_H
#define MARIO_ITEM_H

#include "mario_host.h"

#define MARIO_MASK_KEY "mario_mask.mask"

void MarioItem_Register(const SOHModApi* api);
// The Mario Mask's model, at the current matrix.
void MarioItem_DrawMaskModel(PlayState* play);

#endif
