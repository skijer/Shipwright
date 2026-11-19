#include "mario_form.h"

void MarioForm_Register(const SOHModApi* api) {
    SOHFormDefinition form = { 0 };

    form.structSize = sizeof(form);
    form.key = MARIO_FORM_KEY;
    form.label = "Mario";
    // Becomes SOH_FORM_KIND_TAKEOVER with the libsm64 body: a takeover form without one freezes the player.
    form.kind = SOH_FORM_KIND_LINK;
    form.motionScale = 1.0f;
    api->RegisterForm(&form);
}

bool MarioForm_GetPeerColor(const SOHModApi* api, Player* player, Color_RGB8* color) {
    const uint32_t peerId = api->GetNetPeerIdForActor(&player->actor);
    SOHNetPeer peer = { sizeof(SOHNetPeer) };

    if (peerId == 0 || !api->FindNetPeer(peerId, &peer)) {
        return false;
    }
    *color = peer.color;
    return true;
}
