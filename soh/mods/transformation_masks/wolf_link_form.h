#ifndef WOLF_LINK_FORM_H
#define WOLF_LINK_FORM_H

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

u8 WolfLinkForm_IsEnabled(void);
u8 WolfLinkForm_IsSelected(void);
void WolfLinkForm_Select(u8 selected);
f32 WolfLinkForm_SpeedMultiplier(void);
u8 WolfLinkForm_LoadSkeleton(PlayState* play);
void WolfLinkForm_Update(Player* player, PlayState* play);
void WolfLinkForm_Draw(PlayState* play, Player* player);
void WolfLinkForm_DrawShadow(Actor* actor, Lights* lights, PlayState* play);
void WolfLinkForm_Cleanup(void);

#ifdef __cplusplus
}
#endif

#endif
