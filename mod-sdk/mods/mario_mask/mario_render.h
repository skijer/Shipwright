#ifndef MARIO_RENDER_H
#define MARIO_RENDER_H

#include <stdint.h>

#include "mario_host.h"

struct SM64MarioGeometryBuffers;

// Keeps the atlas libsm64 decoded from the ROM; it must outlive every draw.
void MarioRender_SetTextureAtlas(const uint8_t* atlas);
// Draws the triangles of Mario's last tick moved by `offset` (game units). `marioFlags` picks the cap looks.
void MarioRender_DrawMesh(PlayState* play, const struct SM64MarioGeometryBuffers* geometry, Vec3f offset,
                          uint32_t marioFlags);

#endif
