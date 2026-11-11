#pragma once

#include <fleet/FleetGameApi.h>

struct PlayState;

// Draws one of Ship's items into another game's frame, with Ship's archives.
bool FleetItemDraw_Draw(FleetItemDraw* request);
bool FleetItemDraw_Icon(const char* item, FleetItemIcon* icon);
// Asks `owner` to draw its `item` at the current matrix of `play`; false when it cannot yet.
bool FleetItemDraw_DrawForeign(PlayState* play, const char* owner, const char* item);
