#pragma once

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// Compose the selected alternate Master Sword with the current age's hand.
// Does not read or change the equipped sword; returns false if no custom asset exists.
s32 CustomEquipment_OverrideMasterSwordHand(PlayState* play, Gfx** dList);
// Weapon-only source shared by the custom pedestal and its handoff animation.
Gfx* CustomEquipment_GetTimePedestalSwordDL(void);

#ifdef __cplusplus
}
#endif
