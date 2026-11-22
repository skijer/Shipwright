#ifndef BANJO_CONFIG_H
#define BANJO_CONFIG_H

#include <stdbool.h>

#include "banjo_host.h"

#define BANJO_CONFIG_CVAR(name) "gMods.BanjoMask.Config." name
#define BANJO_VOLUME_CVAR BANJO_CONFIG_CVAR("Volume")

void BanjoConfig_RegisterMenu(const SOHModApi* api);
bool BanjoConfig_IsBanjoModeOn(void);
float BanjoConfig_GetShipShading(void);

#endif
