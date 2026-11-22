#ifndef MARIO_CONFIG_H
#define MARIO_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#include "mario_host.h"

#define MARIO_SYNC_CVAR(name) "gMods.MarioMask.Sync." name
#define MARIO_VOLUME_CVAR MARIO_SYNC_CVAR("Volume")

void MarioConfig_RegisterMenu(const SOHModApi* api);
bool MarioConfig_IsMarioModeOn(void);
int32_t MarioConfig_GetTickRate(void);
float MarioConfig_GetShipShading(void);
// Game frames between rebuilds of Mario's collision while the world moves; 0 keeps the one loaded with the scene.
int32_t MarioConfig_GetCollisionUpdateFrames(void);

#endif
