#ifndef MARIO_FORM_H
#define MARIO_FORM_H

#include <stdbool.h>

#include "mario_host.h"

#define MARIO_FORM_KEY "mario_mask.mario"

void MarioForm_Register(const SOHModApi* api);
bool MarioForm_GetPeerColor(const SOHModApi* api, Player* player, Color_RGB8* color);

#endif
