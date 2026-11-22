#include "mario_buttons.h"

#include "mario_controls.h"
#include "mario_powers.h"

typedef struct ItemButton {
    uint16_t button;
    uint8_t itemButton;
} ItemButton;

static const ItemButton kItemButtons[] = {
    { BTN_CLEFT, 1 },
    { BTN_CDOWN, 2 },
    { BTN_CRIGHT, 3 },
    { BTN_DUP, MARIO_ITEM_BUTTON_DUP },
    { BTN_DDOWN, MARIO_ITEM_BUTTON_DDOWN },
    { BTN_DLEFT, MARIO_ITEM_BUTTON_DLEFT },
    { BTN_DRIGHT, MARIO_ITEM_BUTTON_DRIGHT },
};

static const SOHModApi* sApi;

void MarioButtons_Register(const SOHModApi* api) {
    sApi = api;
}

// Mario has no hands for the game's own items; only his caps work. The mask comes off from its page or with
// Detransform.
bool MarioButtons_Update(PlayState* play) {
    const uint16_t pressed = play->state.input[0].press.button;
    MarioBindings bindings;

    MarioControls_ReadBindings(&bindings);
    if (pressed & bindings.buttons[MARIO_ACTION_DETRANSFORM]) {
        sApi->SetActiveForm(NULL);
        return false;
    }
    for (int32_t index = 0; index < ARRAY_COUNT(kItemButtons); index++) {
        const ItemButton* entry = &kItemButtons[index];

        if (!(pressed & entry->button) || !(bindings.buttons[MARIO_ACTION_ITEM] & entry->button)) {
            continue;
        }

        const char* key = sApi->GetEquippedCustomItem(entry->itemButton);

        if (key != NULL) {
            MarioPowers_UseCap(play, key);
        }
    }
    return true;
}
