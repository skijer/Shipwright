#ifndef BANJO_ROM_H
#define BANJO_ROM_H

#include <stdint.h>

#include "banjo_host.h"

#define BANJO_ROM_SIZE 0x1000000u

void BanjoRom_RegisterMenu(const SOHModApi* api);
void BanjoRom_LoadOrAskOnce(void);
const uint8_t* BanjoRom_Get(void);

#endif
