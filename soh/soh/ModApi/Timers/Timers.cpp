#include "Timers.h"

#include <map>
#include <string>

#include "soh/Enhancements/game-interactor/GameInteractor.h"

extern "C" {
#include "macros.h"
#include "z64.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
}

namespace {

struct HazardClaim {
    int32_t priority;
    int16_t hazard;
    uint8_t requiredTunic;
};

std::map<std::string, HazardClaim> sHazardClaims;
std::string sCountdownOwner;
int16_t sHazard = PLAYER_ENV_HAZARD_NONE;

bool WearsTunic(uint8_t tunic) {
    Player* player = gPlayState != nullptr ? GET_PLAYER(gPlayState) : nullptr;

    return CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC) == tunic || (player != nullptr && player->currentTunic == tunic - 1);
}

const HazardClaim* WinningClaim() {
    const HazardClaim* winner = nullptr;

    for (const auto& [owner, claim] : sHazardClaims) {
        if (claim.requiredTunic != EQUIP_VALUE_TUNIC_KOKIRI && WearsTunic(claim.requiredTunic)) {
            continue;
        }
        if (winner == nullptr || claim.priority > winner->priority) {
            winner = &claim;
        }
    }
    return winner;
}

void ApplyHazard(PlayState* play, s16* hazard) {
    const HazardClaim* claim = WinningClaim();

    if (claim != nullptr) {
        *hazard = claim->hazard;
    }
    sHazard = *hazard;
}

void DropCountdownOnSceneChange(int16_t sceneNum) {
    sCountdownOwner.clear();
}

} // namespace

extern "C" bool Timers_RequestHazard(const char* owner, int32_t priority, int16_t hazard, uint8_t requiredTunic) {
    if (owner == nullptr) {
        return false;
    }
    sHazardClaims[owner] = { priority, hazard, requiredTunic };
    return true;
}

extern "C" void Timers_ReleaseHazard(const char* owner) {
    if (owner != nullptr) {
        sHazardClaims.erase(owner);
    }
}

extern "C" int16_t Timers_GetHazard(void) {
    return sHazard;
}

extern "C" bool Timers_StartCountdown(const char* owner, uint16_t seconds) {
    if (owner == nullptr || (!sCountdownOwner.empty() && sCountdownOwner != owner) ||
        (sCountdownOwner.empty() && gSaveContext.timerState != TIMER_STATE_OFF)) {
        return false;
    }
    sCountdownOwner = owner;
    gSaveContext.timerSeconds = seconds;
    gSaveContext.timerState = TIMER_STATE_DOWN_INIT;
    return true;
}

extern "C" void Timers_StopCountdown(const char* owner) {
    if (owner == nullptr || sCountdownOwner != owner) {
        return;
    }
    gSaveContext.timerState = TIMER_STATE_OFF;
    gSaveContext.timerSeconds = 0;
    sCountdownOwner.clear();
}

extern "C" uint16_t Timers_GetCountdown(const char* owner) {
    return Timers_IsCountdownRunning(owner) ? gSaveContext.timerSeconds : 0;
}

extern "C" bool Timers_IsCountdownRunning(const char* owner) {
    return owner != nullptr && sCountdownOwner == owner && gSaveContext.timerState != TIMER_STATE_OFF;
}

void Timers_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnResolveEnvHazard>(ApplyHazard);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(DropCountdownOnSceneChange);
}
