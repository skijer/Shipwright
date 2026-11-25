#ifndef MARIO_RENDER_H
#define MARIO_RENDER_H

#include <stdbool.h>
#include <stdint.h>

#include "mario_host.h"

struct SM64MarioGeometryBuffers;

// Keeps the atlas libsm64 decoded from the ROM; it must outlive every draw.
void MarioRender_SetTextureAtlas(const uint8_t* atlas);
// Draws the triangles of Mario's last tick moved by `offset` (game units). `marioFlags` picks the cap looks.
void MarioRender_DrawMesh(PlayState* play, const struct SM64MarioGeometryBuffers* geometry, Vec3f offset,
                          uint32_t marioFlags);
// The same triangles at the current matrix, `scale` per game unit: a get-item model.
void MarioRender_DrawMeshHere(PlayState* play, const struct SM64MarioGeometryBuffers* geometry, uint32_t marioFlags,
                              float scale);
// SM64's box of the cap `capFlag` at the current matrix, as large as a native get-item; false without the ROM.
bool MarioRender_DrawCapBox(PlayState* play, uint32_t capFlag);

#endif
