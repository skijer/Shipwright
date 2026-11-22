#ifndef MARIO_POSES_H
#define MARIO_POSES_H

#include <stdint.h>

#include "mario_host.h"

struct SM64MarioGeometryBuffers;

// Poses the puppet Mario as the game poses Link this frame (animation, eyes, hands) and skins him into `geometry`.
// Mario must already stand where Link is.
void MarioPoses_Follow(Player* player, int32_t marioId, struct SM64MarioGeometryBuffers* geometry);
void MarioPoses_Reset(void);

#endif
