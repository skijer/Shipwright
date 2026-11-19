#pragma once

#include <string>
#include <fleet/FleetGameApi.h>

struct PlayState;

bool FleetItemDraw_Draw(FleetItemDraw* request);
bool FleetItemDraw_Icon(const char* item, FleetItemIcon* icon);
const char* FleetItemDraw_TextboxIconPath(const char* item);
bool FleetItemDraw_DrawForeign(PlayState* play, const char* owner, const char* item);
std::string FleetItemDraw_ForeignTextboxIconPath(const char* owner, const char* item);
