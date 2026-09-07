/**
 * kafei_form.h — Kafei's behaviour: SW97's standalone shield plus a
 * stamina-wheel sprint on A. He is a full form that keeps vanilla Link's draw
 * path, so this hangs off ordinary OoT code, not the MmForm state machine.
 */

#ifndef KAFEI_FORM_H
#define KAFEI_FORM_H

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// SW97 guards while standing still, with no R press: its global `gShieldOn` is
// cleared by `speedXZ > 0.1f` and read in Player_SetShieldCollision.
u8 KafeiForm_ShieldIsPassive(Player* player);

void KafeiForm_Tick(Player* player, PlayState* play);
void KafeiForm_Reset(void);

// A is sprint, so OoT must not spend the press on a roll.
u8 KafeiForm_SuppressRoll(Player* player);

// A under Z-target jumps instead of jump-slashing.
u8 KafeiForm_ReplacesJumpslash(void);

// B while moving: the SW97 clip drives the sword arm alone and OOT keeps walking him, so the
// vanilla attack must be refused rather than corrected afterwards. 1 = this swing is ours.
u8 KafeiForm_StartMovingSlash(Player* player);

// SW97's En_Bom_Chu is a landmine, not a homing mouse: different model and icon.
u8 KafeiForm_ReplacesBombchu(void);

// Multiplies every stick-driven speed target, the way the Gerudo sprint does.
f32 KafeiForm_RunSpeedMul(void);
f32 KafeiForm_RunAnimRateMul(void);

// HUD: how many wheels exist, and how full the current one is (0..1).
u8 KafeiForm_WheelCount(void);
f32 KafeiForm_WheelFill(u8 wheel);
u8 KafeiForm_MeterVisible(void);
u8 KafeiForm_IsWinded(void);

#ifdef __cplusplus
}
#endif

#endif // KAFEI_FORM_H
