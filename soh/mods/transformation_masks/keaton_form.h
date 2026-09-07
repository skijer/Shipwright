// Keaton's moveset: a three-step B combo and a Z-targeted projectile on A.
#ifndef KEATON_FORM_H
#define KEATON_FORM_H

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

void KeatonForm_Reset(PlayState* play);
// Must be called ahead of the OOT-yield block: climbing yields, and that path returns.
void KeatonForm_TickClimb(Player* player, PlayState* play);
// Must run before Player_UpdateCommon resets the melee quads, which wipes AT_BOUNCED.
void KeatonForm_ScanBlock(Player* player);
// 1 while the combo's fists may damage; fills in the flags and reach for the quads.
u8 KeatonForm_GetQuadGate(u32* dmgFlags, f32* reach);
// Returns 1 on the frames it is driving the body, so the caller skips its own
// action dispatch. Same contract as the Gerudo and Rito controllers.
u8 KeatonForm_Update(Player* player, PlayState* play);

#ifdef __cplusplus
}
#endif

#endif
