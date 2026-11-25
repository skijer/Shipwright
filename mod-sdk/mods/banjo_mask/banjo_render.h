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
// At the current matrix, `scale` game units per BK unit: a get-item model.
void BanjoRender_DrawMeshHere(PlayState* play, const BkMesh* mesh, const BkModel* model, float scale);

// A still BK model in its rest pose, built on first use.
typedef struct BanjoProp {
    uint16_t modelId;
    BkModel model;
    BkMesh mesh;
    // In BK units: the middle of its bounds and their largest side.
    float center[3];
    float size;
} BanjoProp;

// False until the ROM is there.
bool BanjoRender_BuildProp(BanjoProp* prop);
// Centred at the current matrix, as large as a native get-item.
void BanjoRender_DrawPropGetItem(PlayState* play, BanjoProp* prop);

// A BK sprite, its frames read on first use: CI4 texels on one square canvas, each with its RGBA16 palette.
typedef struct BanjoSprite {
    uint16_t assetId;
    bool isLoaded;
    uint8_t frameCount;
    uint8_t* pixels;
    uint8_t* palettes;
    Vtx quad[4];
} BanjoSprite;

// Its frames looping on a quad facing the camera at the current matrix, as large as a native get-item.
void BanjoRender_DrawSpriteGetItem(PlayState* play, BanjoSprite* sprite);

#endif
