#ifndef SOH_CUSTOM_EQUIP_REGISTRY_H
#define SOH_CUSTOM_EQUIP_REGISTRY_H

#include <stdbool.h>
#include <stdint.h>

#include "soh/Enhancements/item-tables/ItemTableTypes.h"
#include "soh/ModApi/CustomItemRegistry/CustomItemRegistry.h"

#ifdef __cplusplus
#include <string>
extern "C" {
#endif

struct PlayState;

typedef enum {
    SOH_EQUIP_SLOT_SWORD,
    SOH_EQUIP_SLOT_SHIELD,
    SOH_EQUIP_SLOT_TUNIC,
    SOH_EQUIP_SLOT_BOOTS,
    SOH_EQUIP_SLOT_UPGRADE,
    SOH_EQUIP_SLOT_MAX,
} SOHEquipSlot;

typedef void (*SOHEquipStateFunc)(const char* key);

typedef struct {
    uint32_t structSize;
    const char* key;
    const char* iconPath;
    const char* namePath;
    const char* pauseText;
    const char* getItemText;
    uint8_t slot;
    uint8_t page;
    uint8_t row;
    uint8_t column;
    uint8_t ageRequirement;
    uint16_t vanillaBase;
    uint8_t toggles;
    SOHEquipStateFunc onEquip;
    SOHEquipStateFunc onUnequip;
    GetItemEntry getItemEntry;
    const char* getItemModelPath;
    const SOHCustomItemRandomizer* randomizer;
} SOHCustomEquipDefinition;

bool CustomEquipRegistry_Register(const SOHCustomEquipDefinition* definition);
const char* CustomEquipRegistry_GetWorn(uint8_t slot);
bool CustomEquipRegistry_IsWorn(const char* key);
bool CustomEquipRegistry_IsOwned(const char* key);
bool CustomEquipRegistry_SetOwned(const char* key, bool owned);
bool CustomEquipRegistry_Wear(const char* key);
void CustomEquipRegistry_TakeOff(uint8_t slot);
bool CustomEquipRegistry_IsToggleOn(const char* key);
bool CustomEquipRegistry_SetToggle(const char* key, bool on);

#define SOH_EQUIP_PAGE_VANILLA 0xFF

bool KaleidoEquipManager_IsVanillaPageShown(void);
bool KaleidoEquipManager_CycleEquipPage(struct PlayState* play);
bool KaleidoEquipManager_IsCellClaimed(uint8_t row, uint8_t column);
void* KaleidoEquipManager_GetCellIcon(uint8_t row, uint8_t column);
bool KaleidoEquipManager_IsCellAgeAllowed(uint8_t row, uint8_t column);
bool KaleidoEquipManager_IsCellEquipped(uint8_t row, uint8_t column);
bool KaleidoEquipManager_EquipCell(struct PlayState* play, uint8_t row, uint8_t column);
void KaleidoEquipManager_Init(void);

uint32_t CustomEquipRegistry_GetCount(void);
const SOHCustomEquipDefinition* CustomEquipRegistry_GetAt(uint32_t index);
const SOHCustomEquipDefinition* CustomEquipRegistry_Find(const char* key);
bool CustomEquipRegistry_IsAgeAllowed(const char* key);
void CustomEquipRegistry_Init(void);

#ifdef __cplusplus
}

void CustomEquipRegistry_SetCurrentMod(const std::string& name);
#endif

#endif
