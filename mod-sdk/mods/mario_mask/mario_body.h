#ifndef MARIO_BODY_H
#define MARIO_BODY_H

#include <stdint.h>

#include "mario_host.h"

void MarioBody_Register(const SOHModApi* api);
void MarioBody_Enter(PlayState* play, Player* player);
void MarioBody_Exit(PlayState* play, Player* player);
void MarioBody_Update(PlayState* play, Player* player);
void MarioBody_Draw(PlayState* play, Player* player);
// SM64's cap pickup: `ticks` at its 30 Hz, with the cap music.
void MarioBody_PutOnCap(uint32_t capFlag, uint16_t ticks);

#endif
