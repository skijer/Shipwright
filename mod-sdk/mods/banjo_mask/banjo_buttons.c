#include "banjo_buttons.h"

#include "banjo_controls.h"
#include "banjo_pads.h"

typedef struct ItemButton {
    uint16_t button;
    uint8_t itemButton;
} ItemButton;

static const ItemButton kItemButtons[] = {
    { BTN_CLEFT, 1 },
    { BTN_CDOWN, 2 },
    { BTN_CRIGHT, 3 },
    { BTN_DUP, BANJO_ITEM_BUTTON_DUP },
    { BTN_DDOWN, BANJO_ITEM_BUTTON_DDOWN },
    { BTN_DLEFT, BANJO_ITEM_BUTTON_DLEFT },
    { BTN_DRIGHT, BANJO_ITEM_BUTTON_DRIGHT },
};

static const SOHModApi* sApi;

void BanjoButtons_Register(const SOHModApi* api) {
    sApi = api;
}

// Banjo has no hands for the game's own items; only his pads work. The mask comes off from its page or with
// Detransform.
bool BanjoButtons_Update(PlayState* play, Player* player) {
    const uint16_t pressed = play->state.input[0].press.button;
    BanjoBindings bindings;

    BanjoControls_ReadBindings(&bindings);
    if (pressed & bindings.buttons[BANJO_ACTION_DETRANSFORM]) {
        sApi->SetActiveForm(NULL);
        return false;
    }
    for (int32_t index = 0; index < ARRAY_COUNT(kItemButtons); index++) {
        const ItemButton* entry = &kItemButtons[index];

        if (!(pressed & entry->button) || !(bindings.buttons[BANJO_ACTION_ITEM] & entry->button)) {
            continue;
        }

        const char* key = sApi->GetEquippedCustomItem(entry->itemButton);

        if (key != NULL) {
            BanjoPads_Place(play, player, key);
        }
    }
    return true;
}
