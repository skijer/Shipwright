#include "FleetSharedItems.h"
#include "FleetComboItems.h"
#include "FleetComboItemsGlue.h"
#include "FleetComboRando.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

#include <cstring>
#include <spdlog/spdlog.h>
#ifdef _WIN32
#include <windows.h>
#endif

extern "C" {
#include "z64.h"
#include "variables.h"
extern PlayState* gPlayState;
void FleetSync_ApplySharedState(const char* json); // FleetSync.cpp
}

static int sReceiveDepth = 0;
static bool sPullPending = false;

extern "C" void FleetShared_RequestPullFromPeer(void) {
    sPullPending = true;
}

extern "C" void FleetShared_BeginReceive(void) {
    sReceiveDepth++;
}

extern "C" void FleetShared_EndReceive(void) {
    if (sReceiveDepth > 0) {
        sReceiveDepth--;
    }
}

extern "C" int FleetShared_IsReceiving(void) {
    return sReceiveDepth > 0;
}

#ifdef COMBO_BUILD
typedef void (*FnGrantSharedItem)(const char*);

// 2ship.dll is loaded in this process by the launcher before either game boots; resolve its export
// lazily so this module never depends on link order.
static FnGrantSharedItem ResolvePeerGrant() {
    static FnGrantSharedItem sGrant = nullptr;
    static bool sTried = false;
    if (!sTried) {
        sTried = true;
        if (HMODULE peer = GetModuleHandleA("2ship.dll")) {
            sGrant = (FnGrantSharedItem)GetProcAddress(peer, "MM_GrantSharedItem");
        }
    }
    return sGrant;
}

extern "C" void FleetShared_OnNativeObtained(int nativeId) {
    if (sReceiveDepth > 0) {
        return;
    }
    int chain = FleetCombo_ChainAliasFor(nativeId);
    int fcId = FcCombo_ItemForNative(chain != 0 ? chain : nativeId);
    if (fcId == FCI_NO_ITEM) {
        return;
    }
    // soh's peer token is the "RI_*" enum name, which is exactly 2ship's spoiler name.
    const char* peerName = FcCombo_PeerNameForItem(fcId);
    if (std::strcmp(peerName, "FCI_NO_ITEM") == 0) {
        return;
    }
    FnGrantSharedItem grant = ResolvePeerGrant();
    if (grant == nullptr) {
        SPDLOG_WARN("[FleetShared] 2ship.dll has no MM_GrantSharedItem; '{}' not shared", peerName);
        return;
    }
    SPDLOG_INFO("[FleetShared] OoT obtained fc {} -> MM '{}'", fcId, peerName);
    grant(peerName);
}

typedef const char* (*FnExtractSharedState)(void);

// Deferred to a tick with a loaded file: ApplyShared writes the live save, and the switch that
// requested the pull happens before this game's PlayState exists again.
static void PullFromPeerWhenReady() {
    if (!sPullPending || gPlayState == NULL || gSaveContext.fileNum > 2) {
        return;
    }
    sPullPending = false;
    static FnExtractSharedState sExtract = nullptr;
    static bool sTried = false;
    if (!sTried) {
        sTried = true;
        if (HMODULE peer = GetModuleHandleA("2ship.dll")) {
            sExtract = (FnExtractSharedState)GetProcAddress(peer, "MM_ExtractSharedState");
        }
    }
    if (sExtract == nullptr) {
        SPDLOG_WARN("[FleetShared] 2ship.dll has no MM_ExtractSharedState; no reconciliation");
        return;
    }
    FleetShared_BeginReceive();
    FleetSync_ApplySharedState(sExtract());
    FleetShared_EndReceive();
    SPDLOG_INFO("[FleetShared] pulled MM's shared state into OoT");
}

static void RegisterFleetSharedItems() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(PullFromPeerWhenReady);
}

static RegisterShipInitFunc initFleetSharedItems(RegisterFleetSharedItems, {});
#else
extern "C" void FleetShared_OnNativeObtained(int nativeId) {
    (void)nativeId;
}
#endif
