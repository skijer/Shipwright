#ifndef BANJO_PHYSICS_H
#define BANJO_PHYSICS_H

#include <stdbool.h>

#include "banjo_host.h"
#include "libbk/src/libbk_port/libbk.h"

// Starts Banjo-Kazooie's player where Link stands; false until the ROM is there.
bool BanjoPhysics_Spawn(Player* player);
// One BK frame from the controller and the camera, against the collision around Link; Link follows Banjo.
void BanjoPhysics_Drive(PlayState* play, Player* player);
// While the game drives Link, BK's player stands where Link is, so he resumes from there.
void BanjoPhysics_Park(Player* player);
const LibBkState* BanjoPhysics_GetState(void);

#endif
