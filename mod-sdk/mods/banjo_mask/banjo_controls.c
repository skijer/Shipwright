#include "banjo_controls.h"

#include <stdbool.h>

#include <libultraship/bridge/consolevariablebridge.h>

#include "banjo_menu.h"

#define BANJO_BUTTON_CVAR(button) "gMods.BanjoMask.Controls." button
#define ACTION_BIT(action) (1u << (action))

typedef enum BanjoButtonChoice {
    CHOICE_NONE,
    CHOICE_JUMP,
    CHOICE_ATTACK_AND_INTERACT,
    CHOICE_ATTACK,
    CHOICE_INTERACT,
    CHOICE_CROUCH,
    CHOICE_TARGET,
    CHOICE_DETRANSFORM,
    CHOICE_FIRST_PERSON,
    CHOICE_EGGS,
    CHOICE_TALON_TROT,
    CHOICE_WONDER_WING,
    CHOICE_ITEM,
    CHOICE_COUNT
} BanjoButtonChoice;

static const char* const kChoiceLabels[CHOICE_COUNT] = {
    "None",        "Jump",         "Attack + Interact", "Attack", "Interact",   "Crouch",
    "Target",      "Detransform",  "First Person",      "Eggs",   "Talon Trot", "Wonder Wing",
    "Item",
};

static const uint32_t kChoiceActions[CHOICE_COUNT] = {
    0,
    ACTION_BIT(BANJO_ACTION_JUMP),
    ACTION_BIT(BANJO_ACTION_ATTACK) | ACTION_BIT(BANJO_ACTION_INTERACT),
    ACTION_BIT(BANJO_ACTION_ATTACK),
    ACTION_BIT(BANJO_ACTION_INTERACT),
    ACTION_BIT(BANJO_ACTION_CROUCH),
    ACTION_BIT(BANJO_ACTION_TARGET),
    ACTION_BIT(BANJO_ACTION_DETRANSFORM),
    ACTION_BIT(BANJO_ACTION_FIRST_PERSON),
    ACTION_BIT(BANJO_ACTION_EGGS),
    ACTION_BIT(BANJO_ACTION_TALON_TROT),
    ACTION_BIT(BANJO_ACTION_WONDER_WING),
    ACTION_BIT(BANJO_ACTION_ITEM),
};

static void DrawHeading(void) {
    ModUI_Text("Controls");
}

static void DrawDetransformNote(void) {
    ModUI_Text("Default: Detransform");
}

static void DrawTargetNote(void) {
    ModUI_Text("Default: Target");
}

typedef struct BanjoButtonRow {
    const char* label;
    const char* noteId;
    SOHModMenuCallback drawNote;
    const char* cvar;
    uint32_t button;
    BanjoButtonChoice defaultChoice;
    bool canHoldItem;
} BanjoButtonRow;

static const BanjoButtonRow kButtonRows[] = {
    { "A", NULL, NULL, BANJO_BUTTON_CVAR("A"), BTN_A, CHOICE_JUMP, false },
    { "B", NULL, NULL, BANJO_BUTTON_CVAR("B"), BTN_B, CHOICE_ATTACK_AND_INTERACT, false },
    { "C-Up", NULL, NULL, BANJO_BUTTON_CVAR("CUp"), BTN_CUP, CHOICE_FIRST_PERSON, false },
    { "C-Left", NULL, NULL, BANJO_BUTTON_CVAR("CLeft"), BTN_CLEFT, CHOICE_TALON_TROT, true },
    { "C-Right", NULL, NULL, BANJO_BUTTON_CVAR("CRight"), BTN_CRIGHT, CHOICE_WONDER_WING, true },
    { "C-Down", NULL, NULL, BANJO_BUTTON_CVAR("CDown"), BTN_CDOWN, CHOICE_EGGS, true },
    { "D-Pad Up", NULL, NULL, BANJO_BUTTON_CVAR("DUp"), BTN_DUP, CHOICE_ITEM, true },
    { "D-Pad Left", NULL, NULL, BANJO_BUTTON_CVAR("DLeft"), BTN_DLEFT, CHOICE_ITEM, true },
    { "D-Pad Right", NULL, NULL, BANJO_BUTTON_CVAR("DRight"), BTN_DRIGHT, CHOICE_ITEM, true },
    { "D-Pad Down", NULL, NULL, BANJO_BUTTON_CVAR("DDown"), BTN_DDOWN, CHOICE_ITEM, true },
    { "L", "banjo_mask.note.detransform", DrawDetransformNote, BANJO_BUTTON_CVAR("L"), BTN_L, CHOICE_DETRANSFORM,
      false },
    { "Z", NULL, NULL, BANJO_BUTTON_CVAR("Z"), BTN_Z, CHOICE_CROUCH, false },
    { "R", "banjo_mask.note.target", DrawTargetNote, BANJO_BUTTON_CVAR("R"), BTN_R, CHOICE_TARGET, false },
};

static int32_t GetChoiceCount(const BanjoButtonRow* row) {
    return row->canHoldItem ? CHOICE_COUNT : CHOICE_ITEM;
}

static BanjoButtonChoice ReadChoice(const BanjoButtonRow* row) {
    const int32_t choice = CVarGetInteger(row->cvar, row->defaultChoice);
    const bool isOffered = choice >= 0 && choice < GetChoiceCount(row);

    return isOffered ? (BanjoButtonChoice)choice : row->defaultChoice;
}

void BanjoControls_ReadBindings(BanjoBindings* bindings) {
    *bindings = (BanjoBindings){ 0 };
    for (int32_t rowIndex = 0; rowIndex < ARRAY_COUNT(kButtonRows); rowIndex++) {
        const BanjoButtonRow* row = &kButtonRows[rowIndex];
        const uint32_t actions = kChoiceActions[ReadChoice(row)];

        for (int32_t action = 0; action < BANJO_ACTION_COUNT; action++) {
            if (actions & ACTION_BIT(action)) {
                bindings->buttons[action] |= row->button;
            }
        }
    }
}

static void RegisterButtonPicker(const SOHModApi* api, const BanjoButtonRow* row) {
    SOHModMenuWidget widget = BanjoMenu_MakeWidget(BANJO_COLUMN_CONTROLS, SOH_MOD_MENU_COMBOBOX, row->label);

    widget.cvar = row->cvar;
    widget.defaultInt = row->defaultChoice;
    widget.values = kChoiceLabels;
    widget.valueCount = (uint32_t)GetChoiceCount(row);
    api->RegisterMenuWidget(&widget);
}

void BanjoControls_RegisterMenu(const SOHModApi* api) {
    BanjoMenu_AddDrawn(api, BANJO_COLUMN_CONTROLS, "banjo_mask.heading.controls", DrawHeading);
    for (int32_t rowIndex = 0; rowIndex < ARRAY_COUNT(kButtonRows); rowIndex++) {
        const BanjoButtonRow* row = &kButtonRows[rowIndex];

        RegisterButtonPicker(api, row);
        if (row->drawNote != NULL) {
            BanjoMenu_AddDrawn(api, BANJO_COLUMN_CONTROLS, row->noteId, row->drawNote);
        }
    }
}
