#ifndef BANJO_SURFACES_H
#define BANJO_SURFACES_H

#include <stdint.h>

#include "banjo_host.h"
#include "libbk/src/libbk_port/libbk.h"

// The scene, its moving platforms, water surfaces and the props that block Link around `center`, in BK units.
int32_t BanjoSurfaces_Gather(PlayState* play, Vec3f center, LibBkTriangle* triangles, int32_t capacity);
// Cylinders of the props that block Link, read once the frame's collision checks are all registered.
void BanjoSurfaces_CaptureActorColliders(PlayState* play);
void BanjoSurfaces_ClearActorColliders(void);

#endif
