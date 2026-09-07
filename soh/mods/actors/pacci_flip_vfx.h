/**
 * pacci_flip_vfx.h - Cane of Pacci Flip cast visual. Skijer's NEI
 *
 * A golden energy whip runs from the cane to the flipped enemy. Its grip follows
 * the target through the launch and shape.rot.z roll, then releases on landing.
 * Consumed via #include from item_cane_of_somaria.c; this .c is not in the vcxproj.
 */

#ifndef PACCI_FLIP_VFX_H
#define PACCI_FLIP_VFX_H

#include "z64.h"

void PacciFlipVfx_Start(PlayState* play, Player* player, Actor* target);
void PacciFlipVfx_StartLift(PlayState* play, Player* player, Actor* target);
void PacciFlipVfx_Release(void);
void PacciFlipVfx_Update(PlayState* play, Player* player);
void PacciFlipVfx_Draw(PlayState* play, Player* player);
u8 PacciFlipVfx_IsActive(void);
void PacciFlipVfx_Stop(void);

#endif // PACCI_FLIP_VFX_H
