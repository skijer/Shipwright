#pragma once

#include <cstdint>
#include <list>
#include <optional>

#include <nlohmann/json.hpp>

#include "soh/Enhancements/randomizer/entrance.h"

// Cross-game entrance routing for a combined seed: the entrances Ship offers to the shared pools, and the
// routes Fleet installs back. Every call throws std::exception on a malformed request.
nlohmann::json FleetCrossDungeon_DumpCatalogue(const nlohmann::json& options);
nlohmann::json FleetCrossDungeon_DumpCatalogueForSeed(const nlohmann::json& spoiler);
void FleetCrossDungeon_SetSeed(const nlohmann::json& seed);
void FleetCrossDungeon_SetAccess(const nlohmann::json& access);
nlohmann::json FleetCrossDungeon_GetExits();

// While alive, logic follows the installed routes and starts from their arrivals in Ship.
class FleetCrossDungeonSearch {
  public:
    FleetCrossDungeonSearch();
    ~FleetCrossDungeonSearch();
    FleetCrossDungeonSearch(const FleetCrossDungeonSearch&) = delete;
    FleetCrossDungeonSearch& operator=(const FleetCrossDungeonSearch&) = delete;

  private:
    bool mPreviousSearching;
    std::optional<uint8_t> mDungeonRule;
    size_t mAddedArrivals = 0;
    std::list<Rando::Entrance> mDetachedRootExits;
};
