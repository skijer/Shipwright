#include "ProgressiveItems.h"
#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <string>
#include <vector>
#include "soh/SaveManager.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/3drando/item_pool.hpp"
#include "soh/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "soh/ModApi/RandoItems/RandoItems.h"
extern "C" {
#include "variables.h"
}
namespace {
struct Counter {
    uint32_t copies = 0;
    uint16_t baseline = 0;
    bool initialized = false;
};
struct Chain {
    std::string key, pickup;
    std::vector<std::string> levels;
    std::vector<SOHProgressiveAlias> aliases;
    uint16_t first = 1, seedFirst = 1;
    SOHProgressiveReadLevel read = nullptr;
    SOHProgressiveApplyLevel apply = nullptr;
    uint16_t (*readFirst)(void) = nullptr;
    std::array<Counter, 2> counters;
};
std::map<std::string, Chain> chains;
bool applying = false;
constexpr const char* section = "progressiveItems";
SaveContext* BankSave(SOHRandoInfBank bank) {
    if (bank == SOH_RANDO_INF_SAVE)
        return &gSaveContext;
    auto ctx = Rando::Context::GetInstance();
    return ctx && ctx->GetLogic() ? ctx->GetLogic()->GetSaveContext() : nullptr;
}
uint16_t Level(const Chain& c, SOHRandoInfBank bank, SaveContext* save) {
    const auto& counter = c.counters[bank];
    uint64_t level = counter.initialized ? counter.baseline : (save ? c.read(save) : 0);
    if (counter.initialized && counter.copies)
        level = std::max<uint64_t>(level + counter.copies, static_cast<uint64_t>(c.seedFirst) + counter.copies - 1);
    if (bank == SOH_RANDO_INF_SAVE && save)
        level = std::max<uint64_t>(level, c.read(save));
    return static_cast<uint16_t>(std::min<uint64_t>(c.levels.size(), level));
}
Chain* ForKey(const char* key, uint16_t* explicitLevel = nullptr) {
    if (!key)
        return nullptr;
    for (auto& [_, c] : chains) {
        if (c.key == key || c.pickup == key) {
            if (explicitLevel)
                *explicitLevel = 0;
            return &c;
        }
        for (size_t i = 0; i < c.levels.size(); ++i)
            if (c.levels[i] == key) {
                if (explicitLevel)
                    *explicitLevel = static_cast<uint16_t>(i + 1);
                return &c;
            }
    }
    return nullptr;
}
Chain* ForRando(int32_t id, uint16_t* floor = nullptr) {
    if (const char* key = RandoItems_GetKey(id))
        return ForKey(key, floor);
    for (auto& [_, c] : chains)
        for (const auto& a : c.aliases)
            if (a.randoItem == id) {
                if (floor)
                    *floor = std::min(a.firstLevel, c.seedFirst);
                return &c;
            }
    return nullptr;
}
uint16_t Next(const Chain& c, SOHRandoInfBank bank, SaveContext* save, uint16_t floor = 0) {
    return static_cast<uint16_t>(
        std::min<size_t>(c.levels.size(), std::max<uint32_t>(Level(c, bank, save) + 1, floor ? floor : c.seedFirst)));
}
void Change(Chain& c, SOHRandoInfBank bank, SaveContext* save, bool add, uint16_t floor = 0) {
    if (!save)
        return;
    auto& n = c.counters[bank];
    if (!n.initialized) {
        n.baseline = c.read(save);
        n.initialized = true;
    }
    if (add && bank == SOH_RANDO_INF_SAVE) {
        uint16_t native = c.read(save);
        uint64_t own = n.baseline + (uint64_t)n.copies;
        if (n.copies)
            own = std::max<uint64_t>(own, (uint64_t)c.seedFirst + n.copies - 1);
        if (native > own)
            n.baseline = static_cast<uint16_t>(n.baseline + native - own);
    }
    if (add) {
        if (n.copies < UINT32_MAX)
            n.copies++;
        uint16_t actual = Level(c, bank, save);
        if (floor > actual)
            n.copies += floor - actual;
    } else if (n.copies)
        n.copies--;
    applying = true;
    c.apply(save, Level(c, bank, save));
    applying = false;
}
void Reset(bool) {
    for (auto& [_, c] : chains)
        c.counters[SOH_RANDO_INF_SAVE] = {};
}
void Save(const SaveContext&, int, bool) {
    SaveManager::Instance->SaveArray("chains", chains.size(), [](size_t i) {
        auto it = chains.begin();
        std::advance(it, i);
        auto& c = it->second;
        SaveManager::Instance->SaveStruct("", [&c] {
            SaveManager::Instance->SaveData("key", c.key);
            auto n = c.counters[SOH_RANDO_INF_SAVE];
            SaveManager::Instance->SaveData("copies", n.initialized ? n.copies : 0u);
            SaveManager::Instance->SaveData("baseline", n.initialized ? n.baseline : c.read(&gSaveContext));
            SaveManager::Instance->SaveData("first", c.seedFirst);
        });
    });
}
void Load() {
    Reset(false);
    SaveManager::Instance->LoadArray("chains", 1024, [](size_t) {
        SaveManager::Instance->LoadStruct("", [] {
            std::string key;
            uint32_t copies = 0;
            uint16_t first = 1, baseline = 0;
            SaveManager::Instance->LoadData("key", key, std::string());
            SaveManager::Instance->LoadData("copies", copies, 0u);
            SaveManager::Instance->LoadData("baseline", baseline, static_cast<uint16_t>(0));
            SaveManager::Instance->LoadData("first", first, static_cast<uint16_t>(1));
            auto it = chains.find(key);
            if (it == chains.end())
                return;
            it->second.counters[SOH_RANDO_INF_SAVE] = { copies, baseline, true };
            it->second.seedFirst = std::clamp<uint16_t>(first, 1, static_cast<uint16_t>(it->second.levels.size()));
            // Vanilla save data is already loaded by the parent section.
        });
    });
}
} // namespace
bool ProgressiveItems_Register(const SOHProgressiveChain* d) {
    if (!d || d->structSize < offsetof(SOHProgressiveChain, readFirstLevel) || !d->key || !d->key[0] || !d->pickupKey ||
        !d->pickupKey[0] || !d->levels || !d->readLevel || !d->applyLevel || !d->levelCount || d->levelCount > 256 ||
        !d->firstLevel || d->firstLevel > d->levelCount || (d->aliasCount && !d->aliases) || chains.contains(d->key))
        return false;
    if (ForKey(d->key) || ForKey(d->pickupKey) || std::string(d->key) == d->pickupKey)
        return false;
    Chain c;
    c.key = d->key;
    c.pickup = d->pickupKey;
    c.first = c.seedFirst = d->firstLevel;
    c.read = d->readLevel;
    c.apply = d->applyLevel;
    if (d->structSize >= sizeof(*d))
        c.readFirst = d->readFirstLevel;
    for (uint16_t i = 0; i < d->levelCount; ++i) {
        const char* key = d->levels[i].itemKey;
        if (!key || !key[0] || c.key == key || c.pickup == key || !CustomItemRegistry_Find(key) || ForKey(key))
            return false;
        if (std::find(c.levels.begin(), c.levels.end(), key) != c.levels.end())
            return false;
        c.levels.emplace_back(key);
    }
    if (!CustomItemRegistry_Find(c.pickup.c_str()))
        return false;
    for (uint16_t i = 0; i < d->aliasCount; ++i) {
        const auto& a = d->aliases[i];
        if (!a.firstLevel || a.firstLevel > d->levelCount || ForRando(a.randoItem))
            return false;
        for (const auto& prior : c.aliases)
            if (prior.randoItem == a.randoItem || (a.vanillaItem != UINT16_MAX && prior.vanillaItem == a.vanillaItem))
                return false;
        for (const auto& [_, other] : chains)
            for (const auto& prior : other.aliases)
                if (a.vanillaItem != UINT16_MAX && prior.vanillaItem == a.vanillaItem)
                    return false;
        c.aliases.push_back(a);
    }
    std::string key = c.key;
    chains.emplace(key, std::move(c));
    return true;
}
bool ProgressiveItems_IsSource(int32_t item) {
    return ForRando(item) != nullptr;
}
bool ProgressiveItems_CheckObtainability(int32_t item, bool* result) {
    auto* chain = ForRando(item);
    if (!chain || !result)
        return false;
    *result = Level(*chain, SOH_RANDO_INF_SAVE, &gSaveContext) < chain->levels.size();
    return true;
}

uint16_t ProgressiveItems_GetLevel(const char* key, SOHRandoInfBank bank) {
    auto* c = ForKey(key);
    return c && bank >= SOH_RANDO_INF_SAVE && bank < SOH_RANDO_INF_BANK_MAX ? Level(*c, bank, BankSave(bank)) : 0;
}
const char* ProgressiveItems_GetNextItem(const char* key, SOHRandoInfBank bank) {
    uint16_t floor = 0;
    auto* c = ForKey(key, &floor);
    if (!c || bank < SOH_RANDO_INF_SAVE || bank >= SOH_RANDO_INF_BANK_MAX)
        return nullptr;
    return c->levels[Next(*c, bank, BankSave(bank), floor) - 1].c_str();
}
const char* ProgressiveItems_ResolvePickup(const char* key, uint16_t vanillaItem) {
    uint16_t floor = 0;
    auto* c = ForKey(key, &floor);
    if (!c && vanillaItem != UINT16_MAX)
        for (auto& [_, candidate] : chains)
            for (const auto& a : candidate.aliases)
                if (a.vanillaItem == vanillaItem) {
                    c = &candidate;
                    floor = a.firstLevel;
                    break;
                }
    return c ? c->levels[Next(*c, SOH_RANDO_INF_SAVE, &gSaveContext, floor) - 1].c_str() : nullptr;
}
bool ProgressiveItems_ReceiveCustom(const char* key) {
    uint16_t floor = 0;
    auto* c = ForKey(key, &floor);
    if (!c || applying)
        return false;
    Change(*c, SOH_RANDO_INF_SAVE, &gSaveContext, true, floor);
    return true;
}
bool ProgressiveItems_GiveVanilla(uint16_t item) {
    if (applying)
        return false;
    for (auto& [_, c] : chains)
        for (const auto& a : c.aliases)
            if (a.vanillaItem == item) {
                Change(c, SOH_RANDO_INF_SAVE, &gSaveContext, true, a.firstLevel);
                return true;
            }
    return false;
}
bool ProgressiveItems_GiveRando(int32_t item) {
    if (applying)
        return false;
    uint16_t floor = 0;
    auto* c = ForRando(item, &floor);
    if (!c)
        return false;
    Change(*c, SOH_RANDO_INF_SAVE, &gSaveContext, true, floor);
    return true;
}
bool ProgressiveItems_ApplyLogic(int32_t item, Rando::Logic* logic, bool state) {
    if (applying || !logic)
        return false;
    uint16_t floor = 0;
    auto* c = ForRando(item, &floor);
    if (!c)
        return false;
    SaveContext* save = logic->GetSaveContext();
    Change(*c, save == &gSaveContext ? SOH_RANDO_INF_SAVE : SOH_RANDO_INF_LOGIC, save, state, floor);
    return true;
}
bool ProgressiveItems_HasLogicItem(int32_t item, Rando::Logic* logic, bool* result) {
    // Native aliases use their real inventory/equipment checks, populated by applyLevel.
    if (!RandoItems_IsModItem(item))
        return false;
    uint16_t floor = 0;
    auto* c = ForRando(item, &floor);
    if (!c)
        return false;
    auto* save = logic->GetSaveContext();
    auto level = Level(*c, save == &gSaveContext ? SOH_RANDO_INF_SAVE : SOH_RANDO_INF_LOGIC, save);
    *result = level >= (floor ? floor : 1);
    return true;
}
std::shared_ptr<GetItemEntry> ProgressiveItems_ResolveEntry(int32_t item, SOHRandoInfBank bank) {
    uint16_t floor = 0;
    auto* c = ForRando(item, &floor);
    if (!c)
        return nullptr;
    const auto* d = CustomItemRegistry_Find(c->levels[Next(*c, bank, BankSave(bank), floor) - 1].c_str());
    return d ? std::make_shared<GetItemEntry>(d->getItemEntry) : nullptr;
}
void ProgressiveItems_ConfigurePool() {
    for (auto& [_, c] : chains) {
        c.seedFirst = c.first;
        for (const auto& a : c.aliases)
            if (std::find(itemPool.begin(), itemPool.end(), a.randoItem) != itemPool.end())
                c.seedFirst = std::min(c.seedFirst, a.firstLevel);
        if (c.readFirst) {
            uint16_t first = c.readFirst();
            if (first)
                c.seedFirst = std::clamp<uint16_t>(first, 1, static_cast<uint16_t>(c.levels.size()));
        }
    }
}
void ProgressiveItems_ResetLogic() {
    for (auto& [_, c] : chains)
        c.counters[SOH_RANDO_INF_LOGIC] = {};
}
void ProgressiveItems_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadFile>([](int32_t) {
        auto ctx = Rando::Context::GetInstance();
        // Loaded spoilers may never have run AddToPool in this process. Reconstruct
        // the seed's first level from its actual placed native aliases, independently
        // of whichever file or generated pool was active previously.
        for (auto& [_, c] : chains) {
            c.seedFirst = c.first;
            if (!IS_RANDO || !ctx)
                continue;
            if (c.readFirst) {
                uint16_t first = c.readFirst();
                if (first) {
                    c.seedFirst = std::clamp<uint16_t>(first, 1, static_cast<uint16_t>(c.levels.size()));
                    continue;
                }
            }
            for (size_t rc = 0; rc < RC_MAX; ++rc) {
                auto* location = ctx->GetItemLocation(rc);
                if (!location || location->IsHidden())
                    continue;
                int32_t item = location->GetPlacedRandomizerGet();
                for (const auto& a : c.aliases)
                    if (a.randoItem == item)
                        c.seedFirst = std::min(c.seedFirst, a.firstLevel);
            }
        }
    });
    SaveManager::Instance->AddInitFunction(Reset);
    SaveManager::Instance->AddSaveFunction(section, 1, Save, true, SECTION_PARENT_NONE);
    SaveManager::Instance->AddLoadFunction(section, 1, Load);
}
