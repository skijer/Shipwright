#include "soh/ModApi/ModDungeons/BossRegistry.h"
#include "soh/ModApi/ModDungeons/ModDungeonsHost.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <map>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>
#include <libultraship/bridge.h>

#include "soh/SaveManager.h"
#include "soh/unbound/SceneDB.h"
#include "soh/ModApi/ModDungeons/LogicExpr.h"
#include "soh/ModApi/RandoLogic/RandoLogic.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/rng.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

extern "C" {
#include "z64.h"
#include "variables.h"
extern SaveContext gSaveContext;
extern PlayState* gPlayState;
}

namespace {

constexpr const char* SaveSection = "unboundBosses";

enum class LogicTarget {
    Event,
    Exit,
    Location,
};

struct SlotInfo {
    const char* name;
    int16_t scene;
    int16_t vanillaActor;
    LogicTarget target;
    int32_t logicEvent;
    int32_t exitFrom;
    int32_t exitTo;
    int32_t check;
};

// clang-format off
const std::array<SlotInfo, SOH_BOSS_SLOT_MAX> kSlots = { {
    { "Gohma",         SCENE_DEKU_TREE_BOSS,       ACTOR_BOSS_GOMA,      LogicTarget::Event,    LOGIC_DEKU_TREE_CLEAR,        -1, -1, -1 },
    { "King Dodongo",  SCENE_DODONGOS_CAVERN_BOSS, ACTOR_BOSS_DODONGO,   LogicTarget::Event,    LOGIC_DODONGOS_CAVERN_CLEAR,  -1, -1, -1 },
    { "Barinade",      SCENE_JABU_JABU_BOSS,       ACTOR_BOSS_VA,        LogicTarget::Event,    LOGIC_JABU_JABUS_BELLY_CLEAR, -1, -1, -1 },
    { "Phantom Ganon", SCENE_FOREST_TEMPLE_BOSS,   ACTOR_BOSS_GANONDROF, LogicTarget::Event,    LOGIC_FOREST_TEMPLE_CLEAR,    -1, -1, -1 },
    { "Volvagia",      SCENE_FIRE_TEMPLE_BOSS,     ACTOR_BOSS_FD2,       LogicTarget::Event,    LOGIC_FIRE_TEMPLE_CLEAR,      -1, -1, -1 },
    { "Morpha",        SCENE_WATER_TEMPLE_BOSS,    ACTOR_BOSS_MO,        LogicTarget::Event,    LOGIC_WATER_TEMPLE_CLEAR,     -1, -1, -1 },
    { "Bongo Bongo",   SCENE_SHADOW_TEMPLE_BOSS,   ACTOR_BOSS_SST,       LogicTarget::Event,    LOGIC_SHADOW_TEMPLE_CLEAR,    -1, -1, -1 },
    { "Twinrova",      SCENE_SPIRIT_TEMPLE_BOSS,   ACTOR_BOSS_TW,        LogicTarget::Event,    LOGIC_SPIRIT_TEMPLE_CLEAR,    -1, -1, -1 },
    { "Ganondorf",     SCENE_GANONDORF_BOSS,       ACTOR_BOSS_GANON,     LogicTarget::Exit,     -1, RR_GANONS_TOWER_GANONDORF_LAIR, RR_GANONS_CASTLE_ESCAPE, -1 },
    { "Ganon",         SCENE_GANON_BOSS,           ACTOR_BOSS_GANON2,    LogicTarget::Location, -1, -1, -1, RC_GANON },
} };
// clang-format on

struct RegisteredBoss {
    std::string owner;
    std::string displayName;
    std::string scene;
    std::string logicText;
    std::string enabledCvar;
    uint32_t allowedSlots = 0;
    int32_t preferredSlot = -1;
    LogicExpr logic;
    bool isLogicCompiled = false;
};

using Assignment = std::array<std::string, SOH_BOSS_SLOT_MAX>;

std::map<std::string, RegisteredBoss> sBosses;
Assignment sSeedAssignment;
Assignment sSaveAssignment;
std::string sCurrentMod;

int32_t FindSlotByName(const std::string& name) {
    for (int32_t slot = 0; slot < SOH_BOSS_SLOT_MAX; slot++) {
        if (name == kSlots[slot].name) {
            return slot;
        }
    }
    return -1;
}

int32_t FindSlotByScene(int32_t sceneNum) {
    for (int32_t slot = 0; slot < SOH_BOSS_SLOT_MAX; slot++) {
        if (kSlots[slot].scene == sceneNum) {
            return slot;
        }
    }
    return -1;
}

bool IsEnabled(const RegisteredBoss& boss) {
    return boss.enabledCvar.empty() || CVarGetInteger(boss.enabledCvar.c_str(), 0) != 0;
}

bool IsSlotAllowed(const RegisteredBoss& boss, int32_t slot) {
    return slot >= 0 && slot < SOH_BOSS_SLOT_MAX && (boss.allowedSlots & SOH_BOSS_SLOT_BIT(slot)) != 0;
}

std::vector<int32_t> FreeAllowedSlots(const RegisteredBoss& boss, const Assignment& assignment) {
    std::vector<int32_t> slots;
    for (int32_t slot = 0; slot < SOH_BOSS_SLOT_MAX; slot++) {
        if (IsSlotAllowed(boss, slot) && assignment[slot].empty()) {
            slots.push_back(slot);
        }
    }
    return slots;
}

Assignment ComputeDefaultAssignment() {
    Assignment assignment;
    for (const auto& [key, boss] : sBosses) {
        if (!IsEnabled(boss)) {
            continue;
        }
        if (IsSlotAllowed(boss, boss.preferredSlot) && assignment[boss.preferredSlot].empty()) {
            assignment[boss.preferredSlot] = key;
            continue;
        }
        std::vector<int32_t> slots = FreeAllowedSlots(boss, assignment);
        if (slots.empty()) {
            SPDLOG_WARN("[BossRegistry] '{}' found no free slot; the vanilla bosses keep them", key);
            continue;
        }
        assignment[slots.front()] = key;
    }
    return assignment;
}

const Assignment& CurrentAssignment() {
    if (IS_RANDO) {
        return sSaveAssignment;
    }
    static Assignment assignment;
    assignment = ComputeDefaultAssignment();
    return assignment;
}

const RegisteredBoss* SlotOwner(int32_t slot) {
    if (slot < 0 || slot >= SOH_BOSS_SLOT_MAX) {
        return nullptr;
    }
    auto boss = sBosses.find(CurrentAssignment()[slot]);
    return boss == sBosses.end() ? nullptr : &boss->second;
}

std::string RuleName(int32_t slot) {
    return std::string("modboss:") + kSlots[slot].name;
}

bool EvaluateBossLogic(void* userData) {
    return static_cast<RegisteredBoss*>(userData)->logic.Evaluate();
}

bool CompileLogic(const std::string& key, RegisteredBoss& boss) {
    if (boss.isLogicCompiled) {
        return !boss.logic.IsEmpty();
    }
    boss.isLogicCompiled = true;
    std::string error;
    if (!boss.logic.Compile(boss.logicText, LogicExpr::Scope{}, error)) {
        SPDLOG_ERROR("[BossRegistry] '{}' logic: {}", key, error);
    }
    return !boss.logic.IsEmpty();
}

SOHLogicSelector SelectorForSlot(int32_t slot, const int32_t* check) {
    const SlotInfo& info = kSlots[slot];
    SOHLogicSelector selector = { sizeof(SOHLogicSelector) };
    selector.checkType = SOH_LOGIC_ANY;
    selector.region = SOH_LOGIC_ANY;
    selector.scene = SOH_LOGIC_ANY;
    selector.entranceTo = SOH_LOGIC_ANY;
    selector.logicEvent = SOH_LOGIC_ANY;
    switch (info.target) {
        case LogicTarget::Event:
            selector.targets = SOH_LOGIC_TARGET_EVENTS;
            selector.logicEvent = info.logicEvent;
            break;
        case LogicTarget::Exit:
            selector.targets = SOH_LOGIC_TARGET_ENTRANCES;
            selector.region = info.exitFrom;
            selector.entranceTo = info.exitTo;
            break;
        case LogicTarget::Location:
            selector.targets = SOH_LOGIC_TARGET_LOCATIONS;
            selector.checks = check;
            selector.checkCount = 1;
            break;
        default:
            SPDLOG_ERROR("[BossRegistry] slot {} has no logic target", info.name);
            break;
    }
    return selector;
}

void ApplyLogicRules(const Assignment& assignment) {
    for (int32_t slot = 0; slot < SOH_BOSS_SLOT_MAX; slot++) {
        RandoLogic_RemoveOwner(RuleName(slot).c_str());
        auto boss = sBosses.find(assignment[slot]);
        if (boss == sBosses.end() || !CompileLogic(boss->first, boss->second)) {
            continue;
        }
        const std::string name = RuleName(slot);
        const int32_t check = kSlots[slot].check;
        SOHLogicSelector selector = SelectorForSlot(slot, &check);
        SOHLogicRule rule = { sizeof(SOHLogicRule) };
        rule.name = name.c_str();
        rule.effect = SOH_LOGIC_REPLACE;
        rule.condition = EvaluateBossLogic;
        rule.userData = &boss->second;
        rule.include = &selector;
        rule.includeCount = 1;
        RandoLogic_RegisterRule(&rule);
    }
}

void ResetSaveState(bool) {
    sSaveAssignment = {};
}

void SaveState(const SaveContext&, int, bool) {
    std::vector<int32_t> slots;
    for (int32_t slot = 0; slot < SOH_BOSS_SLOT_MAX; slot++) {
        if (!sSaveAssignment[slot].empty()) {
            slots.push_back(slot);
        }
    }
    SaveManager::Instance->SaveData("slotCount", slots.size());
    SaveManager::Instance->SaveArray("slots", slots.size(), [&slots](size_t index) {
        SaveManager::Instance->SaveStruct("", [&slots, index]() {
            SaveManager::Instance->SaveData("slot", std::string(kSlots[slots[index]].name));
            SaveManager::Instance->SaveData("boss", sSaveAssignment[slots[index]]);
        });
    });
}

void LoadState() {
    size_t slotCount = 0;
    SaveManager::Instance->LoadData("slotCount", slotCount, static_cast<size_t>(0));
    SaveManager::Instance->LoadArray("slots", std::min<size_t>(slotCount, SOH_BOSS_SLOT_MAX), [](size_t) {
        SaveManager::Instance->LoadStruct("", []() {
            std::string slotName;
            std::string boss;
            SaveManager::Instance->LoadData("slot", slotName, std::string());
            SaveManager::Instance->LoadData("boss", boss, std::string());
            int32_t slot = FindSlotByName(slotName);
            if (slot < 0) {
                SPDLOG_WARN("[BossRegistry] save names unknown boss slot '{}'", slotName);
                return;
            }
            if (!sBosses.contains(boss)) {
                SPDLOG_WARN("[BossRegistry] save gives {} to '{}', which no mod registers; the vanilla boss fights",
                            slotName, boss);
            }
            sSaveAssignment[slot] = boss;
        });
    });
}

void SyncAssignmentOnLoad(int32_t) {
    if (!IS_RANDO) {
        return;
    }
    sSeedAssignment = sSaveAssignment;
    ApplyLogicRules(sSaveAssignment);
}

} // namespace

bool BossRegistry_Register(const SOHBossDefinition* definition) {
    if (definition == nullptr || definition->structSize < sizeof(SOHBossDefinition) || definition->key == nullptr ||
        definition->key[0] == '\0') {
        SPDLOG_ERROR("[BossRegistry] '{}' registered a boss with no key or an older struct", sCurrentMod);
        return false;
    }
    if ((definition->allowedSlots & ((1u << SOH_BOSS_SLOT_MAX) - 1)) == 0) {
        SPDLOG_ERROR("[BossRegistry] boss '{}' allows no slot", definition->key);
        return false;
    }
    if (sBosses.contains(definition->key)) {
        SPDLOG_ERROR("[BossRegistry] boss '{}' is already registered", definition->key);
        return false;
    }
    RegisteredBoss boss;
    boss.owner = sCurrentMod;
    boss.displayName = definition->displayName != nullptr ? definition->displayName : definition->key;
    boss.scene = definition->scene != nullptr ? definition->scene : "";
    boss.logicText = definition->logic != nullptr ? definition->logic : "";
    boss.enabledCvar = definition->enabledCvar != nullptr ? definition->enabledCvar : "";
    boss.allowedSlots = definition->allowedSlots;
    boss.preferredSlot = definition->preferredSlot;
    sBosses.emplace(definition->key, std::move(boss));
    return true;
}

int32_t BossRegistry_GetSlot(const char* key) {
    if (key == nullptr) {
        return -1;
    }
    const Assignment& assignment = CurrentAssignment();
    for (int32_t slot = 0; slot < SOH_BOSS_SLOT_MAX; slot++) {
        if (assignment[slot] == key) {
            return slot;
        }
    }
    return -1;
}

const char* BossRegistry_GetSceneBoss(int16_t sceneNum) {
    int32_t slot = FindSlotByScene(sceneNum);
    if (slot < 0 || SlotOwner(slot) == nullptr) {
        return nullptr;
    }
    return CurrentAssignment()[slot].c_str();
}

int16_t BossRegistry_GetSlotScene(int32_t slot) {
    return slot >= 0 && slot < SOH_BOSS_SLOT_MAX ? kSlots[slot].scene : -1;
}

int32_t BossRegistry_ResolveSceneSource(int32_t sceneNum) {
    const RegisteredBoss* boss = SlotOwner(FindSlotByScene(sceneNum));
    if (boss == nullptr || boss->scene.empty()) {
        return sceneNum;
    }
    int32_t arena = SceneDB::Instance->RetrieveId(boss->scene);
    if (arena < 0) {
        SPDLOG_ERROR("[BossRegistry] arena scene '{}' is not registered", boss->scene);
        return sceneNum;
    }
    return arena;
}

void BossRegistry_ReportDefeated(Actor* boss) {
    assert(boss != nullptr);
    int32_t slot = gPlayState != nullptr ? FindSlotByScene(gPlayState->sceneNum) : -1;
    if (slot < 0) {
        GameInteractor_ExecuteOnBossDefeat(boss);
        return;
    }
    // The slot's actor id reaches the hooks filtered by boss (timestamps, game completion, Anchor).
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnBossDefeat>(boss);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnBossDefeat>(kSlots[slot].vanillaActor, boss);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnBossDefeat>((uintptr_t)boss, boss);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnBossDefeat>(boss);
}

void BossRegistry_Init() {
    SaveManager::Instance->AddInitFunction(ResetSaveState);
    SaveManager::Instance->AddSaveFunction(SaveSection, 1, SaveState, true, SECTION_PARENT_NONE);
    SaveManager::Instance->AddLoadFunction(SaveSection, 1, LoadState);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>(SyncAssignmentOnLoad);
}

void BossRegistry_SetCurrentMod(const std::string& name) {
    sCurrentMod = name;
}

void BossRegistry_AssignForSeed() {
    Assignment assignment;
    for (const auto& [key, boss] : sBosses) {
        if (!IsEnabled(boss)) {
            continue;
        }
        std::vector<int32_t> slots = FreeAllowedSlots(boss, assignment);
        if (slots.empty()) {
            SPDLOG_WARN("[BossRegistry] '{}' found no free slot in this seed", key);
            continue;
        }
        assignment[slots[Random(0, static_cast<uint32_t>(slots.size()))]] = key;
    }
    sSeedAssignment = assignment;
    ApplyLogicRules(sSeedAssignment);
}

void BossRegistry_InitSaveFile() {
    sSaveAssignment = sSeedAssignment;
}

void BossRegistry_WriteSpoiler(nlohmann::ordered_json& spoiler) {
    for (int32_t slot = 0; slot < SOH_BOSS_SLOT_MAX; slot++) {
        if (!sSeedAssignment[slot].empty()) {
            spoiler["customBosses"][kSlots[slot].name] = sSeedAssignment[slot];
        }
    }
}

void BossRegistry_ParseSpoiler(const nlohmann::json& spoiler) {
    sSeedAssignment = {};
    if (!spoiler.contains("customBosses") || !spoiler["customBosses"].is_object()) {
        ApplyLogicRules(sSeedAssignment);
        return;
    }
    for (const auto& [slotName, boss] : spoiler["customBosses"].items()) {
        int32_t slot = FindSlotByName(slotName);
        if (slot >= 0 && boss.is_string()) {
            sSeedAssignment[slot] = boss.get<std::string>();
        }
    }
    ApplyLogicRules(sSeedAssignment);
}
