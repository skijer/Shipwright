#include "mario_form.h"

#include "mario_body.h"
#include "mario_item.h"

void MarioForm_Register(const SOHModApi* api) {
    SOHFormDefinition form = { 0 };

    form.structSize = sizeof(form);
    form.key = MARIO_FORM_KEY;
    form.label = "Mario";
    form.kind = SOH_FORM_KIND_TAKEOVER;
    form.item = MARIO_MASK_KEY;
    form.motionScale = 1.0f;
    form.onEnter = MarioBody_Enter;
    form.onExit = MarioBody_Exit;
    form.update = MarioBody_Update;
    form.draw = MarioBody_Draw;
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
