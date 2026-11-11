#ifndef SOH_PROGRESSIVE_ITEMS_H
#define SOH_PROGRESSIVE_ITEMS_H
#include <stdint.h>
#include <stdbool.h>
#include "z64.h"
#include "soh/ModApi/RandoInfCustom/RandoInfCustom.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct GetItemEntry GetItemEntry;
typedef uint16_t (*SOHProgressiveReadLevel)(const SaveContext* save);
typedef void (*SOHProgressiveApplyLevel)(SaveContext* save, uint16_t level);
typedef struct {
    const char* itemKey; // Registered custom presentation for this level.
} SOHProgressiveLevel;
typedef struct {
    int32_t randoItem;
    uint16_t vanillaItem; // UINT16_MAX when this alias has no Item_Give identity.
    uint16_t firstLevel;  // BGS can start at level 2; Knife at level 1.
} SOHProgressiveAlias;
typedef struct {
    uint32_t structSize;
    const char* key;
    const char* pickupKey; // Repeated custom pool/drop pickup; copies always advance.
    const SOHProgressiveLevel* levels;
    uint16_t levelCount;
    uint16_t firstLevel;
    const SOHProgressiveAlias* aliases;
    uint16_t aliasCount;
    SOHProgressiveReadLevel readLevel;
    SOHProgressiveApplyLevel applyLevel;
    // Optional seed policy; 0 retains automatic alias/pool inference.
    uint16_t (*readFirstLevel)(void);
} SOHProgressiveChain;
bool ProgressiveItems_Register(const SOHProgressiveChain* chain);
uint16_t ProgressiveItems_GetLevel(const char* key, SOHRandoInfBank bank);
const char* ProgressiveItems_GetNextItem(const char* key, SOHRandoInfBank bank);
const char* ProgressiveItems_ResolvePickup(const char* key, uint16_t vanillaItem);
bool ProgressiveItems_ReceiveCustom(const char* key);
bool ProgressiveItems_GiveVanilla(uint16_t item);
bool ProgressiveItems_GiveRando(int32_t item);
void ProgressiveItems_ResetLogic(void);
#ifdef __cplusplus
}
#include <memory>
namespace Rando {
class Logic;
}
void ProgressiveItems_Init();
bool ProgressiveItems_ApplyLogic(int32_t item, Rando::Logic* logic, bool state);
std::shared_ptr<GetItemEntry> ProgressiveItems_ResolveEntry(int32_t item, SOHRandoInfBank bank);
bool ProgressiveItems_HasLogicItem(int32_t item, Rando::Logic* logic, bool* result);
void ProgressiveItems_ConfigurePool();
bool ProgressiveItems_IsSource(int32_t item);
// False means no chain owns this source; otherwise result reflects remaining levels.
bool ProgressiveItems_CheckObtainability(int32_t item, bool* result);
#endif
#endif
