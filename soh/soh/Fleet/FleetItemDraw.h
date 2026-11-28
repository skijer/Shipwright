#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <fleet/FleetGameApi.h>

struct PlayState;

bool FleetItemDraw_Draw(FleetItemDraw* request);
bool FleetItemDraw_Icon(const char* item, FleetItemIcon* icon);
const char* FleetItemDraw_TextboxIconPath(const char* item);
// The icon resource and tint the launcher shows, for the asset regression; null when the game has no item.
nlohmann::json FleetItemDraw_DescribeIcon(const char* item);
bool FleetItemDraw_DrawForeign(PlayState* play, const char* owner, const char* item);
std::string FleetItemDraw_ForeignTextboxIconPath(const char* owner, const char* item);
