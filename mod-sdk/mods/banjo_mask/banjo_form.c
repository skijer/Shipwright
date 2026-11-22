#include "banjo_form.h"

#include "banjo_body.h"
#include "banjo_item.h"

// Banjo-Kazooie never hangs from or climbs onto a ledge, so the host does not start it for him either.
static int32_t BlockLedge(PlayState* play, Player* player) {
    return SOH_FORM_ACTION_BLOCKED;
}

static const SOHFormActionOverride kBanjoActions[] = {
    { SOH_PLAYER_ACTION_LEDGE, BlockLedge },
};

void BanjoForm_Register(const SOHModApi* api) {
    SOHFormDefinition form = { 0 };

    form.structSize = sizeof(form);
    form.key = BANJO_FORM_KEY;
    form.label = "Banjo";
    form.kind = SOH_FORM_KIND_TAKEOVER;
    form.item = BANJO_MASK_KEY;
    form.actions = kBanjoActions;
    form.actionCount = ARRAY_COUNT(kBanjoActions);
    form.motionScale = 1.0f;
    form.onEnter = BanjoBody_Reset;
    form.onExit = BanjoBody_Reset;
    form.update = BanjoBody_Update;
    form.draw = BanjoBody_Draw;
    api->RegisterForm(&form);
}
