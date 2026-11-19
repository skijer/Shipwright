#include "FleetGame.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Network/Anchor/Anchor.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"

namespace {

void FollowAnchorSwitch() {
    if (!FleetGame_IsHosted() || Anchor::Instance == nullptr) {
        return;
    }
    const bool wanted = CVarGetInteger(CVAR_REMOTE_ANCHOR("Enabled"), 0) != 0;
    if (wanted && !Anchor::Instance->isEnabled) {
        Anchor::Instance->Enable();
    } else if (!wanted && Anchor::Instance->isEnabled) {
        Anchor::Instance->Disable();
    }
}

static RegisterShipInitFunc sAnchorSwitch(FollowAnchorSwitch, { CVAR_REMOTE_ANCHOR("Enabled") });

} // namespace
