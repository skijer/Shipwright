#ifndef BANJO_PADS_H
#define BANJO_PADS_H

#include <stdbool.h>

#include "banjo_host.h"

#define BANJO_FLIGHT_PAD_KEY "banjo_mask.flight_pad"
#define BANJO_SHOCK_SPRING_PAD_KEY "banjo_mask.shock_spring_pad"

void BanjoPads_Register(const SOHModApi* api);
// Without pad shuffle the mask brings both pads.
void BanjoPads_GrantWithMask(void);
// Sets the pad down at Banjo's feet; false when `itemKey` is not a pad or Banjo is not on the ground.
bool BanjoPads_Place(PlayState* play, Player* player, const char* itemKey);
// Tells BK Banjo stands on a pad this frame when he does, so A takes off or springs from it.
void BanjoPads_MarkStanding(PlayState* play, Player* player);
void BanjoPads_Draw(PlayState* play);

#endif
