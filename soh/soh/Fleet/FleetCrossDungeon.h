#pragma once

#include <cstdint>
#include <list>
#include <optional>

#include <nlohmann/json.hpp>

#include "soh/Enhancements/randomizer/entrance.h"

// Every call throws std::exception on a malformed request.
nlohmann::json FleetCrossDungeon_DumpCatalogue(const nlohmann::json& options);
nlohmann::json FleetCrossDungeon_DumpCatalogueForSeed(const nlohmann::json& spoiler);
void FleetCrossDungeon_SetSeed(const nlohmann::json& seed);
void FleetCrossDungeon_SetAccess(const nlohmann::json& access);
nlohmann::json FleetCrossDungeon_GetExits();
uint16_t FleetCrossDungeon_GetBossDefeated();
void FleetCrossDungeon_Reset();

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
