#ifndef MARIO_SURFACES_H
#define MARIO_SURFACES_H

#include <stdbool.h>
#include <stdint.h>

#include "mario_host.h"

// Hands the scene, its moving platforms and the props that block Link to libsm64; returns how many surfaces.
// Floor-only leaves walls and ceilings out, except the ones that lead to another scene.
uint32_t MarioSurfaces_Load(PlayState* play, bool isFloorOnly);
// Cylinders of the props that block Link, read once the frame's collision checks are all registered.
void MarioSurfaces_CaptureActorColliders(PlayState* play);
void MarioSurfaces_ClearActorColliders(void);
// Changes whenever a moving platform or a captured prop moves.
uint32_t MarioSurfaces_GetLiveSignature(PlayState* play);
// Water surface height under (x, z) in game units, or MARIO_NO_WATER.
float MarioSurfaces_GetWaterLevel(PlayState* play, float x, float z);

#define MARIO_NO_WATER (-11000.0f)

#endif
