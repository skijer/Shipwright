#pragma once

#include "NeiHeldPresentation.h"

#ifdef __cplusplus
extern "C" {
#endif
// A complete component bundle is required before replacing any native piece.
bool NeiArticulated_UsesSwitchHook(const Player* player);
bool NeiArticulated_ApplySwitchHookHand(PlayState* play, Player* player, Gfx** limb, Gfx* resolvedHand);
// Returns true when the replacement owns the tip, including docked suppression.
bool NeiArticulated_DrawSwitchHookTip(PlayState* play, Player* player, const Actor* hook);
bool NeiArticulated_HasWhip(void);
// Draw the coil or grip in the same wrist pose; optionally return its rope socket.
bool NeiArticulated_DrawWhipGrip(Player* player, PlayState* play, bool coiled, Vec3f* ropeSocket);
#ifdef __cplusplus
}
#endif
