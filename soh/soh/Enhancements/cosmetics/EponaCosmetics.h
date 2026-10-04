#pragma once

#include "z64skin.h"

#ifdef __cplusplus
extern "C" {
#endif

// Bracket only the Skin draw. End must run before drawing a rider or another actor.
// young: 0 = adult Epona, 1 = young Epona. Ingo's horse is not supported.
void EponaCosmetics_BeginDraw(struct PlayState* play, Skin* skin, s32 young);
void EponaCosmetics_EndDraw(struct PlayState* play);

#ifdef __cplusplus
}
#endif
