// Keaton's shield: a Melee-style hexagonal reflector held on R.
#ifndef KEATON_REFLECTOR_H
#define KEATON_REFLECTOR_H

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

void KeatonReflector_Reset(void);
// 1 on the frames the reflector is up, so the caller skips its own action dispatch.
u8 KeatonReflector_Update(Player* player, PlayState* play);
u8 KeatonReflector_IsActive(void);
// Called from the limb draw with the waist matrix current.
void KeatonReflector_Draw(PlayState* play, Player* player);

#ifdef __cplusplus
}
#endif

#endif
