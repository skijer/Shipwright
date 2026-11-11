#ifndef SOH_MOD_API_DUNGEON_ENTRANCE_PAIRS_H
#define SOH_MOD_API_DUNGEON_ENTRANCE_PAIRS_H

#include <vector>

#include "soh/Enhancements/randomizer/entrance.h"

// Adds each mod dungeon's door as a two-way Dungeon pair of the entrance shuffle table.
void DungeonRegistry_AppendEntrancePairs(std::vector<Rando::EntranceInfoPair>& table);

#endif
