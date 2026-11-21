#include "FleetPortal.h"
#include "FleetGame.h"
#include "FleetPassages.h"
#include "FleetRuntime.h"
#include "FleetRando.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ModApi/ModApi.h"
#include "soh/ShipInit.hpp"
#include "soh/SaveManager.h"
#include "soh/Enhancements/randomizer/SeedContext.h"

extern "C" {
#include "z64.h"
#include "global.h"
#include "variables.h"
#include "functions.h"
#include "soh/Enhancements/randomizer/randomizer_entrance.h"
extern PlayState* gPlayState;
void FileChoose_PrepareLoadedSave(GameState* state);
void Opening_Main(GameState* state);
void Title_Main(GameState* state);
void Sram_InitNewSave(void);
}

namespace {

constexpr const char* kMarketPortal = "market";

struct WarpSongPortal {
    const char* portal;
    int16_t warpPad;
};

constexpr WarpSongPortal kWarpSongPortals[] = {
    { "song.minuet", ENTR_SACRED_FOREST_MEADOW_WARP_PAD }, { "song.bolero", ENTR_DEATH_MOUNTAIN_CRATER_WARP_PAD },
    { "song.serenade", ENTR_LAKE_HYLIA_WARP_PAD },         { "song.requiem", ENTR_DESERT_COLOSSUS_WARP_PAD },
    { "song.nocturne", ENTR_GRAVEYARD_WARP_PAD },          { "song.prelude", ENTR_TEMPLE_OF_TIME_WARP_PAD },
};

std::optional<std::string> sDeparture;
struct Arrival {
    int32_t entrance;
    int32_t slot;
    bool cross = false;
    bool warpSong = false;
};
std::optional<Arrival> sArrival;

std::optional<int16_t> WarpPadForPortal(const std::string& portal) {
    for (const WarpSongPortal& song : kWarpSongPortals) {
        if (portal == song.portal) {
            return song.warpPad;
        }
    }
    return std::nullopt;
}

std::optional<Arrival> ArrivalForPortal(const std::string& portal, int32_t slot) {
    if (portal == kMarketPortal) {
        return Arrival{ ENTR_MARKET_DAY_OUTSIDE_HAPPY_MASK_SHOP, slot };
    }
    if (const auto warpPad = WarpPadForPortal(portal)) {
        return Arrival{ .entrance = *warpPad, .slot = slot, .warpSong = true };
    }
    if (const auto entrance = FleetPassages_Entrance(portal)) {
        return Arrival{ *entrance, slot };
    }
    return std::nullopt;
}

int32_t ArrivalEntrance(const Arrival& arrival) {
    if (arrival.cross) {
        return FleetRuntime_ConsumeArrival();
    }
    if (arrival.warpSong && IS_RANDO) {
        return Entrance_OverrideNextIndex(static_cast<int16_t>(arrival.entrance));
    }
    return FleetRuntime_Materialize(arrival.entrance);
}

bool IsPlaying() {
    return gPlayState != nullptr && gGameState == &gPlayState->state && gSaveContext.gameMode == GAMEMODE_NORMAL;
}

void Depart(const std::string& portal) {
    if (FleetRuntime_HasSession()) {
        FleetRuntime_Save();
    }
    const std::string payload = nlohmann::json{ { "portal", portal }, { "slot", gSaveContext.fileNum } }.dump();
    const FleetDestination destination = { .game = nullptr, .entrance = 0, .payloadJson = payload.c_str() };
    FleetGame_RequestSwitch(&destination);
}

void BootIntoPlay(const Arrival& arrival) {
    FleetRuntime_Initialize();
    SaveManager::Instance->Init();
    gSaveContext.fileNum = arrival.slot;
    if (!SaveManager::Instance->SaveFile_Exist(arrival.slot)) {
        FleetAutomaticSave automatic;
        if (FleetRuntime_HasSession()) {
            FleetRando_CreateCombinedSave(FleetRuntime_Seed(), arrival.slot);
        } else {
            Sram_InitNewSave();
            gSaveContext.fileNum = arrival.slot;
            const uint8_t name[8] = { 0x15, 0x2C, 0x31, 0x2E, 0x3E, 0x3E, 0x3E, 0x3E };
            std::memcpy(gSaveContext.playerName, name, sizeof(name));
            std::memcpy(gSaveContext.newf, "ZELDAZ", sizeof(gSaveContext.newf));
            gSaveContext.entranceIndex = ENTR_LINKS_HOUSE_CHILD_SPAWN;
            gSaveContext.cutsceneIndex = 0;
            FleetRuntime_Save();
        }
    }
    Sram_OpenSave();
    if (FleetRuntime_HasSession() && (!IS_RANDO || Rando::Context::GetInstance()->GetSeed() !=
                                                       FleetRuntime_Seed().at("masterSeed").get<uint32_t>())) {
        throw std::runtime_error("OoT save does not belong to this Fleet seed");
    }
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    SET_NEXT_GAMESTATE(gGameState, Play_Init, PlayState);
    FileChoose_PrepareLoadedSave(gGameState);
    if (arrival.entrance >= 0) {
        gSaveContext.respawnFlag = 0;
        gSaveContext.nextCutsceneIndex = 0xFFEF;
        if (arrival.cross && !FleetRuntime_QueueArrival(arrival.entrance)) {
            throw std::runtime_error("OoT rejected an arrival absent from this seed");
        }
        gSaveContext.entranceIndex = ArrivalEntrance(arrival);
        gSaveContext.cutsceneIndex = 0;
        gSaveContext.nextTransitionType = arrival.warpSong ? TRANS_TYPE_FADE_WHITE : TRANS_TYPE_FADE_BLACK;
    }
}

void EnterArrival() {
    if (gGameState != nullptr && gGameState->running) {
        gGameState->running = false;
        SET_NEXT_GAMESTATE(gGameState, TitleSetup_Init, GameState);
    }
}

bool ReadMmWorldMap(uint16_t* owlStatues, uint32_t* visibleClouds) {
    using GetDirectory = const char* (*)(const char*);
    const auto getDirectory = reinterpret_cast<GetDirectory>(FleetGame_GetHostFunction("GetSaveDirectory"));
    const int slot = FleetRuntime_Slot();
    const char* directory = getDirectory != nullptr ? getDirectory("mm") : nullptr;
    if (directory == nullptr || slot < 0 || owlStatues == nullptr || visibleClouds == nullptr) {
        return false;
    }
    std::ifstream file(std::filesystem::path(directory) / "saves" / ("file" + std::to_string(slot + 1) + ".json"));
    const nlohmann::json saveFile = nlohmann::json::parse(file, nullptr, false);
    const char* sectionName = saveFile.is_object() && saveFile.contains("owlSave") ? "owlSave" : "newCycleSave";
    const nlohmann::json section =
        saveFile.is_object() ? saveFile.value(sectionName, nlohmann::json::object()) : nlohmann::json::object();
    *owlStatues = section.value("/save/saveInfo/playerData/owlActivationFlags"_json_pointer, 0);
    *visibleClouds = section.value("/save/saveInfo/worldMapCloudVisibility"_json_pointer, 0u);
    return true;
}

void RegisterPortalHooks() {
    if (!FleetGame_IsHosted()) {
        return;
    }
    FleetRuntime_Initialize();
    if (FleetRuntime_HasSession() && !sArrival) {
        sArrival = Arrival{ -1, FleetRuntime_Slot() };
    }
    ModApi_PublishHostService("fleet.depart", reinterpret_cast<void*>(FleetPortal_RequestDeparture));
    ModApi_PublishHostService("fleet.mm_world_map", reinterpret_cast<void*>(ReadMmWorldMap));
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t sceneNum) {
        FleetRuntime_ObserveScene(sceneNum);
        if (sceneNum == SCENE_HAPPY_MASK_SHOP && gSaveContext.gameMode == GAMEMODE_NORMAL) {
            sDeparture = kMarketPortal;
        }
    });
}

static RegisterShipInitFunc sPortalInit(RegisterPortalHooks, {});

} // namespace

extern "C" void FleetPortal_RequestDeparture(const char* portal) {
    sDeparture = portal;
}

extern "C" bool FleetPortal_LoadPending(void) {
    if (!sArrival) {
        return false;
    }
    BootIntoPlay(*sArrival);
    sArrival.reset();
    return true;
}

extern "C" void FleetPortal_Arrive(const FleetDestination* arrival) {
    if (arrival == nullptr || arrival->payloadJson == nullptr) {
        return;
    }
    const nlohmann::json payload = nlohmann::json::parse(arrival->payloadJson, nullptr, false);
    if (payload.is_object()) {
        const int slot = payload.value("slot", 0);
        if (slot < 0 || slot > 2) {
            return;
        }
        if (payload.value("session", false)) {
            sArrival = Arrival{ -1, slot };
        } else if (payload.value("cross", false)) {
            sArrival = Arrival{ payload.at("entrance").get<int32_t>(), slot, true };
        } else {
            sArrival = ArrivalForPortal(payload.value("portal", ""), slot);
        }
        if (sArrival) {
            EnterArrival();
        }
    }
}

extern "C" void FleetPortal_FrameEnd(void) {
    const auto hasFiles = reinterpret_cast<bool (*)()>(FleetGame_GetHostFunction("HasFileSelect"));
    if (!FleetRuntime_HasSession() && hasFiles != nullptr && hasFiles() && gGameState != nullptr &&
        (gGameState->main == Opening_Main || gGameState->main == Title_Main)) {
        gGameState->running = false;
        SET_NEXT_GAMESTATE(gGameState, FileChoose_Init, FileChooseContext);
    }
    FleetDestination destination = {};
    std::string payload;
    if (FleetRuntime_TakeDeparture(destination, payload)) {
        FleetGame_RequestSwitch(&destination);
        return;
    }
    if (sDeparture) {
        const std::string portal = *sDeparture;
        sDeparture.reset();
        Depart(portal);
    }
    if (sArrival) {
        EnterArrival();
    }
}
