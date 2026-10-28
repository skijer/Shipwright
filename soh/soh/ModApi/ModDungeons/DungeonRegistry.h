#ifndef SOH_MOD_API_DUNGEON_REGISTRY_H
#define SOH_MOD_API_DUNGEON_REGISTRY_H

#include <stdbool.h>
#include <stdint.h>

// Mod dungeons come from the layer-merged unbound/dungeons.json; see mod-sdk/docs/DUNGEONS.md.
#ifdef __cplusplus
extern "C" {
#endif

// The scene the HUD's small-key switch should see: a mod dungeon scene reads as a keyed vanilla dungeon.
int32_t DungeonRegistry_ResolveKeyScene(int32_t sceneNum);

#ifdef __cplusplus
}

#include <functional>
#include <string>
#include <vector>

#include "soh/Enhancements/randomizer/randomizerTypes.h"

namespace Rando {
class Location;
class ItemLocation;
} // namespace Rando

struct ModDungeonFillInfo {
    std::vector<RandomizerCheck> locations;
    std::vector<RandomizerCheck> vanillaSmallKeyLocations;
    RandomizerCheck vanillaBossKeyLocation = RC_UNKNOWN_CHECK;
    RandomizerCheck vanillaMapLocation = RC_UNKNOWN_CHECK;
    RandomizerCheck vanillaCompassLocation = RC_UNKNOWN_CHECK;
    RandomizerGet smallKey = RG_NONE;
    RandomizerGet bossKey = RG_NONE;
    RandomizerGet map = RG_NONE;
    RandomizerGet compass = RG_NONE;
};

void DungeonRegistry_Init();
void DungeonRegistry_Load();

size_t DungeonRegistry_GetRegionCount();
void DungeonRegistry_AddRegions();

int32_t DungeonRegistry_FindEvent(int32_t dungeon, const std::string& name);
bool DungeonRegistry_IsModEvent(int32_t logicVal);
bool DungeonRegistry_GetEvent(int32_t logicVal);
void DungeonRegistry_SetEvent(int32_t logicVal, bool state);
void DungeonRegistry_ResetLogic(bool resetItems);
bool DungeonRegistry_HasSmallKeys(int32_t dungeon, int32_t count);
bool DungeonRegistry_HasBossKey(int32_t dungeon);

bool DungeonRegistry_EvaluateCheck(int32_t check);
bool DungeonRegistry_EvaluateEvent(int32_t logicVal);
bool DungeonRegistry_EvaluateExit(int32_t fromRegion, int32_t toRegion);

bool DungeonRegistry_IsDungeonItem(int32_t randoGet);
bool DungeonRegistry_HasItem(int32_t randoGet, bool fromSave);
void DungeonRegistry_ApplyLogicItem(int32_t randoGet, bool state);
bool DungeonRegistry_GrantItem(int32_t randoGet);
void DungeonRegistry_AddToPool();
std::vector<ModDungeonFillInfo> DungeonRegistry_GetFillInfo();

bool DungeonRegistry_IsModCheck(int32_t check);
// SOH [Unbound] A vanilla-region check's own flag, above RAND_INF_MAX (RandoInfCustom); RAND_INF_MAX when it has none.
int32_t DungeonRegistry_GetCheckRandomizerInf(int32_t check);
Rando::Location* DungeonRegistry_GetLocation(int32_t check);
Rando::ItemLocation* DungeonRegistry_GetItemLocation(int32_t check);
void DungeonRegistry_AddLocations(std::vector<RandomizerCheck>& allLocations);
void DungeonRegistry_ForEachItemLocation(const std::function<void(Rando::ItemLocation&)>& visit);
RandomizerCheck DungeonRegistry_FindCheckBySceneFlag(int16_t sceneNum, int16_t flagType, int16_t flag);
#endif

#endif
