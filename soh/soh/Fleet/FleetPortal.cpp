#include "FleetPortal.h"
#include "FleetGame.h"

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "z64.h"
#include "variables.h"
extern PlayState* gPlayState;
}

namespace {

constexpr const char* kMarketPortal = "market";

std::optional<std::string> sDeparture;
std::optional<int16_t> sArrivalEntrance;

std::optional<int16_t> EntranceForPortal(const std::string& portal) {
    if (portal == kMarketPortal) {
        return ENTR_MARKET_DAY_OUTSIDE_HAPPY_MASK_SHOP;
    }
    return std::nullopt;
}

bool IsPlaying() {
    return gPlayState != nullptr && gGameState == &gPlayState->state && gSaveContext.gameMode == GAMEMODE_NORMAL;
}

void Depart(const std::string& portal) {
    const std::string payload = nlohmann::json{ { "portal", portal }, { "slot", gSaveContext.fileNum } }.dump();
    const FleetDestination destination = { .game = nullptr, .entrance = 0, .payloadJson = payload.c_str() };
    FleetGame_RequestSwitch(&destination);
}

// The arrival waits until a file is in play, so a parked title screen picks it up once the player loads a file.
void EnterArrival() {
    if (!IsPlaying() || gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
        return;
    }
    gPlayState->nextEntranceIndex = *sArrivalEntrance;
    gPlayState->transitionTrigger = TRANS_TRIGGER_START;
    gPlayState->transitionType = TRANS_TYPE_FADE_BLACK;
    sArrivalEntrance.reset();
}

void RegisterPortalHooks() {
    if (!FleetGame_IsHosted()) {
        return;
    }
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t sceneNum) {
        if (sceneNum == SCENE_HAPPY_MASK_SHOP && gSaveContext.gameMode == GAMEMODE_NORMAL) {
            sDeparture = kMarketPortal;
        }
    });
}

static RegisterShipInitFunc sPortalInit(RegisterPortalHooks, {});

} // namespace

extern "C" void FleetPortal_Arrive(const FleetDestination* arrival) {
    if (arrival == nullptr || arrival->payloadJson == nullptr) {
        return;
    }
    const nlohmann::json payload = nlohmann::json::parse(arrival->payloadJson, nullptr, false);
    if (payload.is_object()) {
        sArrivalEntrance = EntranceForPortal(payload.value("portal", ""));
    }
}

extern "C" void FleetPortal_FrameEnd(void) {
    if (sDeparture) {
        const std::string portal = *sDeparture;
        sDeparture.reset();
        Depart(portal);
    }
    if (sArrivalEntrance) {
        EnterArrival();
    }
}
