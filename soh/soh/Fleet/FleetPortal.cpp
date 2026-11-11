#include "FleetPortal.h"
#include "FleetGame.h"
#include "FleetPassages.h"
#include "FleetRuntime.h"
#include "FleetRando.h"

#include <optional>
#include <cstring>
#include <string>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/SaveManager.h"
#include "soh/Enhancements/randomizer/SeedContext.h"

extern "C" {
#include "z64.h"
#include "global.h"
#include "variables.h"
#include "functions.h"
extern PlayState* gPlayState;
void FileChoose_PrepareLoadedSave(GameState* state);
void Opening_Main(GameState* state);
void Title_Main(GameState* state);
void Sram_InitNewSave(void);
}

namespace {

constexpr const char* kMarketPortal = "market";

std::optional<std::string> sDeparture;
struct Arrival {
    int32_t entrance;
    int32_t slot;
    bool cross = false;
};
std::optional<Arrival> sArrival;

std::optional<int32_t> EntranceForPortal(const std::string& portal) {
    if (portal == kMarketPortal) {
        return ENTR_MARKET_DAY_OUTSIDE_HAPPY_MASK_SHOP;
    }
    return FleetPassages_Entrance(portal);
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
        gSaveContext.entranceIndex =
            arrival.cross ? FleetRuntime_ConsumeArrival() : FleetRuntime_Materialize(arrival.entrance);
        gSaveContext.cutsceneIndex = 0;
        gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
    }
}

void EnterArrival() {
    if (gGameState != nullptr && gGameState->running) {
        // Going through TitleSetup destroys the parked scene before its destination loads.
        gGameState->running = false;
        SET_NEXT_GAMESTATE(gGameState, TitleSetup_Init, GameState);
    }
}

void RegisterPortalHooks() {
    if (!FleetGame_IsHosted()) {
        return;
    }
    FleetRuntime_Initialize();
    if (FleetRuntime_HasSession() && !sArrival) {
        sArrival = Arrival{ -1, FleetRuntime_Slot() };
    }
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
        } else if (const auto entrance = EntranceForPortal(payload.value("portal", ""))) {
            sArrival = Arrival{ *entrance, slot };
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
