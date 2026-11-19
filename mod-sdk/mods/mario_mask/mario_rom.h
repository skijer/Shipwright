#ifndef MARIO_ROM_H
#define MARIO_ROM_H

#include <stdint.h>

#include "mario_host.h"

void MarioRom_RegisterMenu(const SOHModApi* api);
void MarioRom_LoadOrAskOnce(void);
const uint8_t* MarioRom_Get(void);

#endif
