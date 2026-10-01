#include "CustomEquipRegistry.h"

#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <spdlog/spdlog.h>
#include <ship/Context.h>
#include <ship/debug/Console.h>

#include "soh/SaveManager.h"
#include "soh/ModApi/ConsoleArguments.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

extern "C" {
#include "macros.h"
#include "z64.h"
#include "functions.h"
#include "variables.h"
extern SaveContext gSaveContext;
extern PlayState* gPlayState;
}

namespace {

constexpr const char* SaveSection = "customEquipment";
constexpr size_t SaveKeyLimit = 128;

struct RegisteredEquip {
    std::string owner;
    std::string key;
    SOHCustomEquipDefinition definition = {};
};

std::vector<std::unique_ptr<RegisteredEquip>> sEquipment;
std::unordered_map<std::string, RegisteredEquip*> sByKey;
std::unordered_set<std::string> sOwned;
std::unordered_set<std::string> sToggled;
std::array<std::string, SOH_EQUIP_SLOT_MAX> sWorn;
std::array<uint16_t, SOH_EQUIP_SLOT_MAX> sAppliedValue;
std::string sCurrentMod;

RegisteredEquip* Find(const char* key) {
    if (key == nullptr) {
        return nullptr;
    }
    auto entry = sByKey.find(key);
    return entry == sByKey.end() ? nullptr : entry->second;
}

bool IsVanillaSlot(uint8_t slot) {
    return slot < SOH_EQUIP_SLOT_UPGRADE;
}

uint16_t ReadVanillaValue(uint8_t slot) {
    return (gSaveContext.equips.equipment >> (slot * 4)) & 0xF;
}

void SyncSwordButton(uint16_t value) {
    if (value == EQUIP_VALUE_SWORD_NONE) {
        gSaveContext.equips.buttonItems[0] = ITEM_NONE;
        Flags_SetInfTable(INFTABLE_SWORDLESS);
    } else {
        gSaveContext.equips.buttonItems[0] = ITEM_SWORD_KOKIRI + value - EQUIP_VALUE_SWORD_KOKIRI;
        Flags_UnsetInfTable(INFTABLE_SWORDLESS);
    }
    if (gPlayState != nullptr) {
        Interface_LoadItemIcon1(gPlayState, 0);
    }
}

void ApplyVanillaValue(uint8_t slot, uint16_t value) {
    Inventory_ChangeEquipment(slot, value);
    sAppliedValue[slot] = value;
    if (slot == SOH_EQUIP_SLOT_SWORD) {
        SyncSwordButton(value);
    }
    if (gPlayState != nullptr) {
        Player_SetEquipmentData(gPlayState, GET_PLAYER(gPlayState));
    }
}

bool IsAgeAllowed(const SOHCustomEquipDefinition* definition) {
    if (definition == nullptr) {
        return false;
    }
    if (definition->ageRequirement == 0) {
        return true;
    }
    return (definition->ageRequirement == 1) == (gSaveContext.linkAge == LINK_AGE_CHILD);
}

void SetWorn(uint8_t slot, RegisteredEquip* entry, bool writesVanilla) {
    if (slot >= SOH_EQUIP_SLOT_MAX) {
        return;
    }

    RegisteredEquip* outgoing = Find(sWorn[slot].c_str());
    if (outgoing == entry) {
        return;
    }

    sWorn[slot].clear();
    if (outgoing != nullptr && outgoing->definition.onUnequip != nullptr) {
        outgoing->definition.onUnequip(outgoing->key.c_str());
    }

    if (entry != nullptr) {
        sWorn[slot] = entry->key;
    }

    if (IsVanillaSlot(slot) && writesVanilla) {
        ApplyVanillaValue(slot, entry != nullptr ? entry->definition.vanillaBase : 0);
    } else if (IsVanillaSlot(slot)) {
        sAppliedValue[slot] = ReadVanillaValue(slot);
    }

    if (entry != nullptr && entry->definition.onEquip != nullptr) {
        entry->definition.onEquip(entry->key.c_str());
    }
}

void WatchWornEquipment() {
    for (uint8_t slot = 0; slot < SOH_EQUIP_SLOT_MAX; slot++) {
        RegisteredEquip* worn = Find(sWorn[slot].c_str());
        if (worn == nullptr) {
            continue;
        }
        if (!CustomEquipRegistry_IsOwned(worn->key.c_str())) {
            SetWorn(slot, nullptr, true);
            continue;
        }
        const bool vanillaTookSlot = IsVanillaSlot(slot) && ReadVanillaValue(slot) != sAppliedValue[slot];
        if (!IsAgeAllowed(&worn->definition) || vanillaTookSlot) {
            SetWorn(slot, nullptr, false);
        }
    }
}

void ResetSaveState(bool isDebug) {
    sOwned.clear();
    sToggled.clear();
    for (auto& key : sWorn) {
        key.clear();
    }
    sAppliedValue.fill(0);
}

void SaveKeySet(const char* name, const std::unordered_set<std::string>& keys) {
    std::vector<std::string> ordered(keys.begin(), keys.end());
    SaveManager::Instance->SaveArray(name, ordered.size(),
                                     [&ordered](size_t index) { SaveManager::Instance->SaveData("", ordered[index]); });
}

void LoadKeySet(const char* name, std::unordered_set<std::string>& keys) {
    keys.clear();
    SaveManager::Instance->LoadArray(name, SaveKeyLimit, [&keys](size_t index) {
        std::string key;
        SaveManager::Instance->LoadData("", key, std::string());
        if (!key.empty()) {
            keys.insert(key);
        }
    });
}

void SaveState(SaveContext* saveContext, int sectionId, bool fullSave) {
    SaveKeySet("owned", sOwned);
    SaveKeySet("toggled", sToggled);
    SaveManager::Instance->SaveArray("worn", sWorn.size(),
                                     [](size_t index) { SaveManager::Instance->SaveData("", sWorn[index]); });
}

void RecomputeAppliedValues() {
    for (uint8_t slot = 0; slot < SOH_EQUIP_SLOT_MAX; slot++) {
        RegisteredEquip* worn = Find(sWorn[slot].c_str());
        sAppliedValue[slot] = worn != nullptr ? worn->definition.vanillaBase : ReadVanillaValue(slot);
    }
}

void LoadState() {
    LoadKeySet("owned", sOwned);
    LoadKeySet("toggled", sToggled);
    SaveManager::Instance->LoadArray("worn", sWorn.size(), [](size_t index) {
        std::string key;
        SaveManager::Instance->LoadData("", key, std::string());
        if (index < sWorn.size()) {
            sWorn[index] = key;
        }
    });
    RecomputeAppliedValues();
}

void GiveFromConsole(PlayState* play, const char* type, const char* key, bool* handled, bool* given) {
    if (type == nullptr || std::string(type) != "custom_equipment") {
        return;
    }
    *handled = true;
    *given = CustomEquipRegistry_SetOwned(key, true);
}

void MarkOwnedFromPickup(const char* key) {
    CustomEquipRegistry_SetOwned(key, true);
}

bool RegisterCompanionItem(const SOHCustomEquipDefinition& equip) {
    if (equip.getItemText == nullptr && equip.getItemModelPath == nullptr) {
        return true;
    }
    SOHCustomItemDefinition item = {};
    item.structSize = sizeof(item);
    item.key = equip.key;
    item.iconPath = equip.iconPath;
    item.namePath = equip.namePath;
    item.getItemEntry = equip.getItemEntry;
    item.getItemText = equip.getItemText;
    item.getItemModelPath = equip.getItemModelPath;
    item.randomizer = equip.randomizer;
    item.presentationFlags = SOH_ITEM_HIDE_FROM_SAVE_EDITOR | SOH_ITEM_HIDE_FROM_KALEIDO;
    item.onReceive = MarkOwnedFromPickup;
    return CustomItemRegistry_Register(&item);
}

int32_t WearCommand(std::shared_ptr<Ship::Console>, std::vector<std::string> args, std::string* output) {
    if (args.size() != 2) {
        if (output != nullptr) {
            *output = "Usage: wear \"<custom equipment key>\"";
        }
        return 1;
    }
    const std::string key = ModApi_UnquoteConsoleArgument(args[1]);
    const bool worn = CustomEquipRegistry_Wear(key.c_str());
    if (output != nullptr) {
        *output = worn ? "Wearing " + key : "Not owned, wrong age or unknown key";
    }
    return worn ? 0 : 1;
}

} // namespace

bool CustomEquipRegistry_Register(const SOHCustomEquipDefinition* definition) {
    if (definition == nullptr || definition->key == nullptr || definition->slot >= SOH_EQUIP_SLOT_MAX) {
        SPDLOG_ERROR("[CustomEquipRegistry] Mod '{}' passed an unusable equipment definition", sCurrentMod);
        return false;
    }
    if (sByKey.count(definition->key) != 0) {
        SPDLOG_ERROR("[CustomEquipRegistry] Key '{}' is already registered", definition->key);
        return false;
    }

    auto entry = std::make_unique<RegisteredEquip>();
    entry->owner = sCurrentMod;
    entry->key = definition->key;
    std::memcpy(&entry->definition, definition, std::min<size_t>(definition->structSize, sizeof(entry->definition)));
    entry->definition.structSize = sizeof(entry->definition);
    entry->definition.key = entry->key.c_str();

    if (!RegisterCompanionItem(entry->definition)) {
        return false;
    }
    sByKey[entry->key] = entry.get();
    sEquipment.push_back(std::move(entry));
    return true;
}

const char* CustomEquipRegistry_GetWorn(uint8_t slot) {
    if (slot >= SOH_EQUIP_SLOT_MAX || sWorn[slot].empty()) {
        return nullptr;
    }
    return sWorn[slot].c_str();
}

bool CustomEquipRegistry_IsWorn(const char* key) {
    if (key == nullptr) {
        return false;
    }
    for (const auto& worn : sWorn) {
        if (worn == key) {
            return true;
        }
    }
    return false;
}

bool CustomEquipRegistry_IsOwned(const char* key) {
    return key != nullptr && sOwned.count(key) != 0;
}

bool CustomEquipRegistry_SetOwned(const char* key, bool owned) {
    RegisteredEquip* entry = Find(key);
    if (entry == nullptr) {
        return false;
    }
    if (CustomItemRegistry_Find(entry->key.c_str()) != nullptr) {
        CustomItemRegistry_SetOwned(entry->key.c_str(), owned);
    }
    if (owned) {
        sOwned.insert(entry->key);
        return true;
    }
    sOwned.erase(entry->key);
    if (CustomEquipRegistry_IsWorn(entry->key.c_str())) {
        SetWorn(entry->definition.slot, nullptr, true);
    }
    sToggled.erase(entry->key);
    return true;
}

bool CustomEquipRegistry_Wear(const char* key) {
    RegisteredEquip* entry = Find(key);
    if (entry == nullptr || !CustomEquipRegistry_IsOwned(entry->key.c_str()) || !IsAgeAllowed(&entry->definition)) {
        return false;
    }
    if (entry->definition.slot == SOH_EQUIP_SLOT_UPGRADE) {
        return CustomEquipRegistry_SetToggle(entry->key.c_str(), true);
    }
    SetWorn(entry->definition.slot, entry, true);
    return true;
}

void CustomEquipRegistry_TakeOff(uint8_t slot) {
    SetWorn(slot, nullptr, true);
}

bool CustomEquipRegistry_IsToggleOn(const char* key) {
    return key != nullptr && sToggled.count(key) != 0;
}

bool CustomEquipRegistry_SetToggle(const char* key, bool on) {
    RegisteredEquip* entry = Find(key);
    if (entry == nullptr || !CustomEquipRegistry_IsOwned(entry->key.c_str())) {
        return false;
    }
    if (on && !entry->definition.toggles) {
        return false;
    }
    if (on) {
        sToggled.insert(entry->key);
        if (entry->definition.onEquip != nullptr) {
            entry->definition.onEquip(entry->key.c_str());
        }
        return true;
    }
    sToggled.erase(entry->key);
    if (entry->definition.onUnequip != nullptr) {
        entry->definition.onUnequip(entry->key.c_str());
    }
    return true;
}

uint32_t CustomEquipRegistry_GetCount(void) {
    return static_cast<uint32_t>(sEquipment.size());
}

const SOHCustomEquipDefinition* CustomEquipRegistry_GetAt(uint32_t index) {
    return index < sEquipment.size() ? &sEquipment[index]->definition : nullptr;
}

const SOHCustomEquipDefinition* CustomEquipRegistry_Find(const char* key) {
    RegisteredEquip* entry = Find(key);
    return entry == nullptr ? nullptr : &entry->definition;
}

bool CustomEquipRegistry_IsAgeAllowed(const char* key) {
    return IsAgeAllowed(CustomEquipRegistry_Find(key));
}

void CustomEquipRegistry_Init(void) {
    SaveManager::Instance->AddInitFunction(ResetSaveState);
    SaveManager::Instance->AddSaveFunction(SaveSection, 1, SaveState, true, SECTION_PARENT_NONE);
    SaveManager::Instance->AddLoadFunction(SaveSection, 1, LoadState);

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(WatchWornEquipment);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnConsoleGive>(GiveFromConsole);

    Ship::Context::GetInstance()->GetConsole()->AddCommand(
        "wear", { WearCommand, "Wears an owned piece of custom equipment", { { "key", Ship::ArgumentType::TEXT } } });
}

void CustomEquipRegistry_SetCurrentMod(const std::string& name) {
    sCurrentMod = name;
}
