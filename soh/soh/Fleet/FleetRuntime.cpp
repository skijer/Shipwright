#include "FleetRuntime.h"
#include "FleetRuntimeBridge.h"
#include "FleetGame.h"
#include "FleetForeignItems.h"

#include <filesystem>
#include <algorithm>
#include <fstream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <fleet/rando/CrossDungeonRuntime.h>
#include <spdlog/spdlog.h>

#include "soh/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "soh/ModApi/RandoItems/RandoItems.h"
#include "soh/ModApi/RandoInfCustom/RandoInfCustom.h"
#include "soh/SaveManager.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/savefile.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/randomizer_grotto.h"
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

void StartingItemGive(GetItemEntry entry, RandomizerCheck check);

extern "C" {
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
}

namespace {

using namespace ComboRando::CrossDungeon;
constexpr const char* kGame = "oot";
nlohmann::json sSession;
std::set<std::string> sReported;
RuntimeState sRoutes(0);
std::optional<RuntimeState::Transition> sDeparture;
bool sTransitionComplete = false;
bool sLoading = false;
int32_t sCheckpoint = -1;
bool sSaveCommitted = false;

bool IsKnownCheckpoint(int32_t entrance) {
    if (sRoutes.IsRecoveryEntrance(entrance)) {
        return true;
    }
    const auto cross = sSession.at("seed").value("crossDungeonEntrances", nlohmann::json{});
    if (!cross.is_object()) {
        return false;
    }
    const auto dungeons = cross.value("dungeons", nlohmann::json::array());
    for (const auto& entry : dungeons) {
        const auto dungeon = ParseDungeon(entry);
        if (dungeon.returnTo == Endpoint{ 0, entrance }) {
            return true;
        }
    }
    return false;
}

template <class Function> Function Host(const char* name) {
    return reinterpret_cast<Function>(FleetGame_GetHostFunction(name));
}

bool IsPlaying() {
    return gPlayState != nullptr && gGameState == &gPlayState->state && gGameState->running &&
           gSaveContext.gameMode == GAMEMODE_NORMAL && IS_RANDO && gSaveContext.fileNum == FleetRuntime_Slot();
}

void Grant(const nlohmann::json& delivery) {
    const std::string item = delivery.value("trap", false) ? "Ice Trap" : delivery.at("item").get<std::string>();
    const auto found = Rando::StaticData::itemNameToEnum.find(item);
    if (found == Rando::StaticData::itemNameToEnum.end()) {
        throw std::runtime_error("unknown OoT delivery: " + item);
    }
    const char* key = RandoItems_GetKey(found->second);
    if (key != nullptr) {
        CustomItemRegistry_ReceiveGetItem(key);
    } else {
        const GetItemEntry entry = Rando::StaticData::RetrieveItem(found->second).GetGIEntry_Copy();
        StartingItemGive(entry, RC_MAX);
        GameInteractor_ExecuteOnItemReceiveHooks(entry);
    }
}

void GrantSharedTier(const std::string& key, int current, const nlohmann::json& item) {
    RandomizerGet grant = RG_NONE;
    if (key == "wallets") {
        grant = current == 0 ? RG_ADULT_WALLET : current == 1 ? RG_GIANT_WALLET : RG_TYCOON_WALLET;
    } else if (key == "bows") {
        grant = current == 0 ? RG_FAIRY_BOW : current == 1 ? RG_BIG_QUIVER : RG_BIGGEST_QUIVER;
    } else if (key == "bombBags") {
        grant = current == 0 ? RG_BOMB_BAG : current == 1 ? RG_BIG_BOMB_BAG : RG_BIGGEST_BOMB_BAG;
    } else if (key == "magic") {
        grant = current == 0 ? RG_MAGIC_SINGLE : RG_MAGIC_DOUBLE;
    } else if (key == "hookshot") {
        grant = current == 0 ? RG_HOOKSHOT : RG_LONGSHOT;
    }
    if (grant == RG_NONE) {
        Grant({ { "item", item } });
        return;
    }
    const GetItemEntry entry = Rando::StaticData::RetrieveItem(grant).GetGIEntry_Copy();
    StartingItemGive(entry, RC_MAX);
    GameInteractor_ExecuteOnItemReceiveHooks(entry);
}

nlohmann::json ReadSharedTiers() {
    return { { "bows", static_cast<int>(CUR_UPG_VALUE(UPG_QUIVER)) },
             { "bombBags", static_cast<int>(CUR_UPG_VALUE(UPG_BOMB_BAG)) },
             { "bombchuBags", static_cast<int>(INV_CONTENT(ITEM_BOMBCHU) != ITEM_NONE) },
             { "magic", static_cast<int>(gSaveContext.isMagicAcquired + gSaveContext.isDoubleMagicAcquired) },
             { "wallets", static_cast<int>(CUR_UPG_VALUE(UPG_WALLET)) },
             { "fireArrows", static_cast<int>(INV_CONTENT(ITEM_ARROW_FIRE) == ITEM_ARROW_FIRE) },
             { "iceArrows", static_cast<int>(INV_CONTENT(ITEM_ARROW_ICE) == ITEM_ARROW_ICE) },
             { "lightArrows", static_cast<int>(INV_CONTENT(ITEM_ARROW_LIGHT) == ITEM_ARROW_LIGHT) },
             { "eponasSong", static_cast<int>(CHECK_QUEST_ITEM(QUEST_SONG_EPONA) != 0) },
             { "songOfStorms", static_cast<int>(CHECK_QUEST_ITEM(QUEST_SONG_STORMS) != 0) },
             { "hookshot", static_cast<int>(INV_CONTENT(ITEM_HOOKSHOT) == ITEM_NONE
                                                ? 0
                                                : (INV_CONTENT(ITEM_HOOKSHOT) == ITEM_LONGSHOT ? 2 : 1)) },
             { "lens", static_cast<int>(INV_CONTENT(ITEM_LENS) != ITEM_NONE) },
             { "goronMask", static_cast<int>(Flags_GetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_GORON)) },
             { "zoraMask", static_cast<int>(Flags_GetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_ZORA)) },
             { "keatonMask", static_cast<int>(Flags_GetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_KEATON)) },
             { "bunnyHood", static_cast<int>(Flags_GetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_BUNNY)) },
             { "maskOfTruth", static_cast<int>(Flags_GetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_TRUTH)) } };
}

void SynchronizeSharedTiers() {
    const auto report = Host<bool (*)(const char*, const char*)>("ReportSharedTiers");
    const auto get = Host<const char* (*)(const char*)>("GetSharedTiers");
    if (report == nullptr || get == nullptr) {
        return;
    }
    const std::string native = ReadSharedTiers().dump();
    if (!report(kGame, native.c_str())) {
        return;
    }
    const char* raw = get(kGame);
    if (raw == nullptr) {
        return;
    }
    const auto shared = nlohmann::json::parse(raw);
    bool changed = false;
    for (const auto& entry : shared) {
        const std::string key = entry.at("key");
        const int desired = entry.at("tier");
        for (int attempt = 0; attempt < 16; attempt++) {
            const int previous = ReadSharedTiers().value(key, 0);
            if (previous >= desired) {
                break;
            }
            GrantSharedTier(key, previous, entry.at("item"));
            changed = true;
            if (ReadSharedTiers().value(key, 0) <= previous) {
                break;
            }
        }
    }
    if (changed) {
        FleetRuntime_Save();
    }
}

void ReceiveItems() {
    const auto get = Host<const char* (*)(const char*)>("GetDeliveries");
    const char* raw = get != nullptr ? get(kGame) : nullptr;
    if (raw == nullptr) {
        return;
    }
    const auto deliveries = nlohmann::json::parse(raw);
    bool changed = false;
    for (const auto& delivery : deliveries) {
        const std::string key = "fleet:received:" + delivery.at("id").get<std::string>();
        const uint16_t flag = RandoInfCustom_Register(key.c_str());
        if (flag == 0) {
            throw std::runtime_error("could not register Fleet receipt");
        }
        if (RandoInfCustom_Get(SOH_RANDO_INF_SAVE, flag)) {
            continue;
        }
        // Logged before the grant: after a crash, the last line names the item.
        SPDLOG_INFO("[Fleet] Receiving {} from {}", delivery.at("item").get<std::string>(),
                    delivery.at("id").get<std::string>());
        Grant(delivery);
        RandoInfCustom_Set(SOH_RANDO_INF_SAVE, flag, true);
        changed = true;
    }
    if (changed) {
        FleetRuntime_Save();
    }
}

void ReportChecks() {
    const auto report = Host<bool (*)(const char*, const char*)>("ReportObtained");
    if (report == nullptr) {
        return;
    }
    const auto& placements = FleetRuntime_Seed().at(kGame).at("placements");
    bool saved = false;
    for (const auto& [check, item] : placements.items()) {
        if (sReported.contains(check)) {
            continue;
        }
        auto found = Rando::StaticData::locationNameToEnum.find(check);
        const bool obtained = found != Rando::StaticData::locationNameToEnum.end() &&
                              Rando::Context::GetInstance()->GetItemLocation(found->second)->HasObtained();
        if (!obtained) {
            continue;
        }
        if (!saved) {
            FleetRuntime_Save();
            saved = true;
        }
        if (report(kGame, check.c_str())) {
            sReported.insert(check);
        }
    }
}

} // namespace

void FleetRuntime_Initialize() {
    if (!sSession.is_null()) {
        return;
    }
    const auto get = Host<const char* (*)()>("GetSession");
    const char* raw = get != nullptr ? get() : nullptr;
    if (raw == nullptr) {
        return;
    }
    sSession = nlohmann::json::parse(raw);
    SOHCustomItemRandomizer randomizer = {};
    randomizer.structSize = sizeof(randomizer);
    SOHCustomItemDefinition definition = {};
    definition.structSize = sizeof(definition);
    definition.key = "fleet:foreign";
    definition.randomizer = &randomizer;
    definition.presentationFlags = SOH_ITEM_HIDE_FROM_SAVE_EDITOR | SOH_ITEM_HIDE_FROM_KALEIDO;
    definition.getItemText = "You found an item for the other game!";
    definition.displayName = "Cross-game item";
    definition.getItemEntry = Rando::StaticData::RetrieveItem(RG_BLUE_RUPEE).GetGIEntry_Copy();
    definition.getItemEntry.modIndex = MOD_RANDOMIZER;
    if (!CustomItemRegistry_Register(&definition)) {
        throw std::runtime_error("could not register Fleet foreign item");
    }
    FleetForeignItems_Register(FleetRuntime_Seed());
    FleetRuntime_InstallRoutes();
    SaveManager::Instance->AddSaveFunction(
        "fleet", 1, [](const SaveContext&, int, bool) { SaveManager::Instance->SaveData("checkpoint", sCheckpoint); },
        true, -1);
    SaveManager::Instance->AddLoadFunction("fleet", 1, [] {
        SaveManager::Instance->LoadData("checkpoint", sCheckpoint, int32_t(-1));
        if (!IsKnownCheckpoint(sCheckpoint)) {
            sCheckpoint = -1;
        }
    });
    SaveManager::Instance->AddInitFunction([](bool) { sCheckpoint = -1; });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveFile>([](int32_t slot, int32_t section) {
        if (slot == FleetRuntime_Slot() && section == SECTION_ID_BASE) {
            sSaveCommitted = true;
            FleetGame_CommitSave();
        }
    });
}

bool FleetRuntime_HasSession() {
    FleetRuntime_Initialize();
    return sSession.is_object();
}

const nlohmann::json& FleetRuntime_Seed() {
    return sSession.at("seed");
}

int FleetRuntime_Slot() {
    return sSession.is_object() ? sSession.at("slot").get<int>() : -1;
}

void FleetRuntime_Save() {
    if (IsPlaying()) {
        gSaveContext.rupees = static_cast<int16_t>(
            std::clamp<int>(gSaveContext.rupees + gSaveContext.rupeeAccumulator, 0, CUR_CAPACITY(UPG_WALLET)));
        gSaveContext.rupeeAccumulator = 0;
        Play_SaveSceneFlags(gPlayState);
        gSaveContext.savedSceneNum = gPlayState->sceneNum;
    }
    SaveManager::Instance->ThreadPoolWait();
    sSaveCommitted = false;
    FleetAutomaticSave automatic;
    SaveManager::Instance->SaveSection(gSaveContext.fileNum, SECTION_ID_BASE, false);
    if (sSession.is_object() && !sSaveCommitted) {
        throw std::runtime_error("could not commit OoT Fleet save");
    }
}

void FleetRuntime_FrameEnd() {
    if (!FleetRuntime_HasSession() || !IsPlaying() || gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
        return;
    }
    ReportChecks();
    if (gPlayState->msgCtx.msgMode == MSGMODE_NONE) {
        ReceiveItems();
        SynchronizeSharedTiers();
    }
}

void FleetRuntime_InstallRoutes() {
    sRoutes.Install(FleetRuntime_Seed().value("crossDungeonEntrances", nlohmann::json{}));
}

nlohmann::json FleetRuntime_Status() {
    auto received = nlohmann::json::array();
    if (sSession.is_object() && IsPlaying()) {
        const auto get = Host<const char* (*)(const char*)>("GetDeliveries");
        const char* raw = get != nullptr ? get(kGame) : nullptr;
        if (raw != nullptr) {
            for (const auto& delivery : nlohmann::json::parse(raw)) {
                const std::string id = delivery.at("id");
                const std::string key = "fleet:received:" + id;
                if (RandoInfCustom_Get(SOH_RANDO_INF_SAVE, RandoInfCustom_Find(key.c_str()))) {
                    received.push_back(id);
                }
            }
        }
    }
    return { { "received", received },
             { "sharedTiers", sSession.is_object() && IsPlaying() ? ReadSharedTiers() : nlohmann::json{} },
             { "playing", sSession.is_object() && IsPlaying() },
             { "slot", gSaveContext.fileNum },
             { "scene", sSession.is_object() && IsPlaying() ? static_cast<int>(gPlayState->sceneNum) : -1 } };
}

bool FleetRuntime_CollectForSmoke(const std::string& check) {
    if (!sSession.is_object() || !sSession.value("runtimeTests", false) || !IsPlaying() ||
        !FleetRuntime_Seed().at(kGame).at("placements").contains(check)) {
        return false;
    }
    const auto found = Rando::StaticData::locationNameToEnum.find(check);
    if (found == Rando::StaticData::locationNameToEnum.end()) {
        throw std::runtime_error("unknown OoT smoke check: " + check);
    }
    auto* location = Rando::Context::GetInstance()->GetItemLocation(found->second);
    const std::string expected = FleetForeignItems_KeyForCheck(FleetRuntime_Seed(), check);
    if (location->GetPlacedRandomizerGet() != RandoItems_GetRandoGet(expected.c_str())) {
        throw std::runtime_error("OoT smoke check has item " + std::to_string(location->GetPlacedRandomizerGet()) +
                                 ", expected " + expected + ": " + check);
    }
    location->SetCheckStatus(RCSHOW_COLLECTED);
    ReportChecks();
    return true;
}

int32_t FleetRuntime_Route(int32_t entrance, bool beforeNative) {
    if (!sSession.is_object() || !IsPlaying()) {
        return -1;
    }
    const auto transition = sRoutes.Lookup(beforeNative ? Phase::BeforeNative : Phase::AfterNative, entrance);
    if (!transition) {
        return -1;
    }
    if (transition->to.game == 0) {
        sCheckpoint = sRoutes.RecoveryForArrival(transition->to.index);
        return transition->to.index;
    }
    sDeparture = transition;
    sTransitionComplete = sLoading;
    return entrance;
}

bool FleetRuntime_TakeDeparture(FleetDestination& destination, std::string& payload) {
    if (!sDeparture || !sTransitionComplete) {
        return false;
    }
    const auto transition = *sDeparture;
    sCheckpoint = transition.checkpoint.index;
    sDeparture.reset();
    sTransitionComplete = false;
    gPlayState->transitionTrigger = TRANS_TRIGGER_OFF;
    gPlayState->transitionMode = TRANS_MODE_OFF;
    gSaveContext.entranceIndex = FleetRuntime_Materialize(transition.checkpoint.index);
    if (gSaveContext.entranceIndex >= 0 && gSaveContext.entranceIndex < ENTRANCE_TABLE_SIZE) {
        gSaveContext.savedSceneNum = gEntranceTable[gSaveContext.entranceIndex].scene;
    }
    FleetRuntime_Save();
    payload = nlohmann::json{
        { "cross", true }, { "slot", FleetRuntime_Slot() }, { "entrance", transition.to.index }
    }.dump();
    destination = { .game = "mm",
                    .entrance = static_cast<uint32_t>(transition.to.index),
                    .payloadJson = payload.c_str() };
    return true;
}

bool FleetRuntime_QueueArrival(int32_t entrance) {
    const bool queued = sRoutes.QueueArrival(entrance);
    if (queued) {
        sCheckpoint = sRoutes.RecoveryForArrival(entrance);
    }
    return queued;
}

int32_t FleetRuntime_Materialize(int32_t entrance) {
    return Grotto_OverrideSpecialEntrance(static_cast<int16_t>(entrance));
}

int32_t FleetRuntime_ConsumeArrival() {
    const int32_t entrance = sRoutes.ConsumeArrival();
    if (entrance < 0) {
        return -1;
    }
    sCheckpoint = sRoutes.RecoveryForArrival(entrance);
    return FleetRuntime_Materialize(entrance);
}

void FleetRuntime_ObserveScene(int32_t scene) {
    const auto* owner = sRoutes.FindEntrance(sCheckpoint);
    if (owner == nullptr) {
        sCheckpoint = -1;
        return;
    }
    if (owner->pool == Pool::Dungeon) {
        const bool dungeon = (scene >= SCENE_DEKU_TREE && scene <= SCENE_GANONS_TOWER_COLLAPSE_INTERIOR &&
                              scene != SCENE_THIEVES_HIDEOUT) ||
                             scene == SCENE_INSIDE_GANONS_CASTLE_COLLAPSE ||
                             (scene >= SCENE_DEKU_TREE_BOSS && scene <= SCENE_GANONS_TOWER_COLLAPSE_EXTERIOR) ||
                             scene == SCENE_GANON_BOSS;
        if (!dungeon) {
            sCheckpoint = -1;
        }
        return;
    }
    if (owner->pool == Pool::Grotto) {
        if (scene != SCENE_GROTTOS && scene != SCENE_FAIRYS_FOUNTAIN) {
            sCheckpoint = -1;
        }
        return;
    }
    bool matches = false;
    for (int layer = 0; layer < 4 && sCheckpoint + layer < ENTRANCE_TABLE_SIZE; layer++) {
        matches = matches || gEntranceTable[sCheckpoint + layer].scene == scene;
    }
    if (!matches) {
        sCheckpoint = -1;
    }
}

extern "C" int32_t FleetRuntime_RecoveryEntrance(void) {
    return sCheckpoint >= 0 && IsKnownCheckpoint(sCheckpoint) ? FleetRuntime_Materialize(sCheckpoint) : -1;
}

extern "C" int32_t FleetRuntime_ResolveExit(int32_t entrance, int beforeNative) {
    return FleetRuntime_Route(entrance, beforeNative != 0);
}

extern "C" int FleetRuntime_CompleteTransition(void) {
    if (!sDeparture) {
        return 0;
    }
    sTransitionComplete = true;
    return 1;
}

extern "C" void FleetRuntime_CancelTransition(void) {
    sDeparture.reset();
    sTransitionComplete = false;
}

extern "C" void FleetRuntime_SetLoading(int loading) {
    sLoading = loading != 0;
}

extern "C" int FleetRuntime_EntrancesEnabled(void) {
    return sSession.is_object() && sRoutes.Enabled();
}
