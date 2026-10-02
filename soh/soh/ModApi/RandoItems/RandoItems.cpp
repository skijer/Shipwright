#include "soh/ModApi/RandoItems/RandoItems.h"

#include <string>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

#include "soh/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "soh/ModApi/RandoOptions/RandoOptions.h"
#include "soh/Enhancements/custom-message/CustomMessageTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/3drando/item_pool.hpp"

namespace {

constexpr int32_t ModRandoGetBase = RG_MAX + 1;

struct ModItem {
    std::string key;
    int32_t randoGet;
    uint16_t flag;
};

std::vector<ModItem> sItems;
std::unordered_map<std::string, size_t> sKeysToIndex;

const ModItem* Find(int32_t randoGet) {
    if (randoGet < ModRandoGetBase) {
        return nullptr;
    }
    size_t index = static_cast<size_t>(randoGet - ModRandoGetBase);
    return index < sItems.size() ? &sItems[index] : nullptr;
}

const SOHCustomItemRandomizer* GetRandomizerBlock(const ModItem& item) {
    const SOHCustomItemDefinition* definition = CustomItemRegistry_Find(item.key.c_str());
    return definition != nullptr ? definition->randomizer : nullptr;
}

Rando::Item BuildItem(const ModItem& item, const SOHCustomItemDefinition& definition) {
    const SOHCustomItemRandomizer& block = *definition.randomizer;
    bool advancement = (block.flags & SOH_CUSTOM_ITEM_RANDO_ADVANCEMENT) != 0;
    bool progressive = (block.flags & SOH_CUSTOM_ITEM_RANDO_PROGRESSIVE) != 0;
    GetItemCategory category = advancement ? ITEM_CATEGORY_MAJOR : ITEM_CATEGORY_LESSER;

    Rando::Item built(static_cast<RandomizerGet>(item.randoGet), Text{ item.key, item.key, item.key },
                      static_cast<ItemType>(block.type), GI_NONE, advancement, LOGIC_NONE, RHT_NONE, ITEM_CUSTOM,
                      OBJECT_INVALID, GID_MAXIMUM, TEXT_RANDOMIZER_CUSTOM_ITEM, 0x80, CHEST_ANIM_LONG, category,
                      MOD_RANDOMIZER, {}, "%g", progressive, block.price);
    *built.GetGIEntry() = definition.getItemEntry;
    return built;
}

void BuildRow(const ModItem& item) {
    const SOHCustomItemDefinition* definition = CustomItemRegistry_Find(item.key.c_str());
    if (definition == nullptr || definition->randomizer == nullptr) {
        SPDLOG_ERROR("Randomizer item {} lost its definition", item.key);
        return;
    }
    std::vector<Rando::Item>& itemTable = Rando::StaticData::GetItemTable();
    if (itemTable.size() <= static_cast<size_t>(item.randoGet)) {
        itemTable.resize(static_cast<size_t>(item.randoGet) + 1);
    }
    itemTable[item.randoGet] = BuildItem(item, *definition);
}

bool IsItemTableBuilt() {
    return Rando::StaticData::GetItemTable().size() >= RG_MAX;
}

} // namespace

int32_t RandoItems_Register(const char* key) {
    if (key == nullptr || key[0] == '\0') {
        SPDLOG_ERROR("Rejected a randomizer item with no key");
        return 0;
    }
    std::string name = key;
    auto found = sKeysToIndex.find(name);
    if (found != sKeysToIndex.end()) {
        return sItems[found->second].randoGet;
    }

    uint16_t flag = RandoInfCustom_Register(key);
    if (flag == 0) {
        return 0;
    }
    int32_t randoGet = ModRandoGetBase + static_cast<int32_t>(sItems.size());
    sKeysToIndex[name] = sItems.size();
    sItems.push_back({ name, randoGet, flag });
    if (IsItemTableBuilt()) {
        BuildRow(sItems.back());
    }
    return randoGet;
}

int32_t RandoItems_GetRandoGet(const char* key) {
    if (key == nullptr) {
        return 0;
    }
    auto found = sKeysToIndex.find(key);
    return found != sKeysToIndex.end() ? sItems[found->second].randoGet : 0;
}

const char* RandoItems_GetKey(int32_t randoGet) {
    const ModItem* item = Find(randoGet);
    return item != nullptr ? item->key.c_str() : nullptr;
}

bool RandoItems_IsModItem(int32_t randoGet) {
    return Find(randoGet) != nullptr;
}

uint32_t RandoItems_GetCount(void) {
    return static_cast<uint32_t>(sItems.size());
}

int32_t RandoItems_GetAt(uint32_t index) {
    return index < sItems.size() ? sItems[index].randoGet : 0;
}

bool RandoItems_IsOwned(int32_t randoGet, SOHRandoInfBank bank) {
    const ModItem* item = Find(randoGet);
    if (item == nullptr) {
        return false;
    }
    if (bank == SOH_RANDO_INF_SAVE) {
        return CustomItemRegistry_IsOwned(item->key.c_str());
    }
    return RandoInfCustom_Get(bank, item->flag);
}

void RandoItems_SetOwned(int32_t randoGet, SOHRandoInfBank bank, bool state) {
    const ModItem* item = Find(randoGet);
    if (item == nullptr) {
        return;
    }
    if (bank == SOH_RANDO_INF_SAVE) {
        CustomItemRegistry_SetOwned(item->key.c_str(), state);
        return;
    }
    RandoInfCustom_Set(bank, item->flag, state);
}

void RandoItems_BuildItemTable() {
    for (const ModItem& item : sItems) {
        BuildRow(item);
    }
}

void RandoItems_AddToPool() {
    for (const ModItem& item : sItems) {
        const SOHCustomItemRandomizer* block = GetRandomizerBlock(item);
        if (block == nullptr) {
            continue;
        }
        const uint32_t count =
            block->poolCount +
            (block->poolCountOption != nullptr ? RandoOptions_GetForSeed(block->poolCountOption) : 0);
        if (count == 0) {
            continue;
        }
        itemPool.insert(itemPool.end(), count, static_cast<RandomizerGet>(item.randoGet));
    }
}
