#ifndef BANJO_RENDER_H
#define BANJO_RENDER_H

#include "banjo_host.h"
#include "bk_mesh.h"

typedef struct BanjoPlacement {
    Vec3f position;
    // BK's own angles in degrees: yaw turns +z towards +x, applied after pitch and roll.
    float pitchYawRoll[3];
    // Game units per BK unit.
    float scale;
} BanjoPlacement;

void BanjoRender_DrawMesh(PlayState* play, const BkMesh* mesh, const BkModel* model, const BanjoPlacement* placement);

#endif
