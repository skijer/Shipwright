#include "mario_controls.h"

#include <stdbool.h>

#include <libultraship/bridge/consolevariablebridge.h>

#include "mario_menu.h"

#define MARIO_BUTTON_CVAR(button) "gMods.MarioMask.Controls." button
#define ACTION_BIT(action) (1u << (action))

typedef enum MarioButtonChoice {
    CHOICE_NONE,
    CHOICE_JUMP,
    CHOICE_PUNCH_AND_INTERACT,
    CHOICE_PUNCH,
    CHOICE_INTERACT,
    CHOICE_CROUCH,
    CHOICE_TARGET,
    CHOICE_DETRANSFORM,
    CHOICE_FIRST_PERSON,
    CHOICE_ITEM,
    CHOICE_COUNT
} MarioButtonChoice;

static const char* const kChoiceLabels[CHOICE_COUNT] = {
    "None",   "Jump",        "Punch + Interact", "Punch", "Interact", "Crouch / Ground Pound",
    "Target", "Detransform", "First Person",     "Item",
};

static const uint32_t kChoiceActions[CHOICE_COUNT] = {
    0,
    ACTION_BIT(MARIO_ACTION_JUMP),
    ACTION_BIT(MARIO_ACTION_PUNCH) | ACTION_BIT(MARIO_ACTION_INTERACT),
    ACTION_BIT(MARIO_ACTION_PUNCH),
    ACTION_BIT(MARIO_ACTION_INTERACT),
    ACTION_BIT(MARIO_ACTION_CROUCH),
    ACTION_BIT(MARIO_ACTION_TARGET),
    ACTION_BIT(MARIO_ACTION_DETRANSFORM),
    ACTION_BIT(MARIO_ACTION_FIRST_PERSON),
    ACTION_BIT(MARIO_ACTION_ITEM),
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

typedef struct MarioButtonRow {
    const char* label;
    const char* noteId;
    SOHModMenuCallback drawNote;
    const char* cvar;
    uint32_t button;
    MarioButtonChoice defaultChoice;
    bool canHoldItem;
} MarioButtonRow;

static const MarioButtonRow kButtonRows[] = {
    { "A", NULL, NULL, MARIO_BUTTON_CVAR("A"), BTN_A, CHOICE_JUMP, false },
    { "B", NULL, NULL, MARIO_BUTTON_CVAR("B"), BTN_B, CHOICE_PUNCH_AND_INTERACT, false },
    { "C-Up", NULL, NULL, MARIO_BUTTON_CVAR("CUp"), BTN_CUP, CHOICE_FIRST_PERSON, false },
    { "C-Left", NULL, NULL, MARIO_BUTTON_CVAR("CLeft"), BTN_CLEFT, CHOICE_ITEM, true },
    { "C-Right", NULL, NULL, MARIO_BUTTON_CVAR("CRight"), BTN_CRIGHT, CHOICE_ITEM, true },
    { "C-Down", NULL, NULL, MARIO_BUTTON_CVAR("CDown"), BTN_CDOWN, CHOICE_ITEM, true },
    { "D-Pad Up", NULL, NULL, MARIO_BUTTON_CVAR("DUp"), BTN_DUP, CHOICE_ITEM, true },
    { "D-Pad Left", NULL, NULL, MARIO_BUTTON_CVAR("DLeft"), BTN_DLEFT, CHOICE_ITEM, true },
    { "D-Pad Right", NULL, NULL, MARIO_BUTTON_CVAR("DRight"), BTN_DRIGHT, CHOICE_ITEM, true },
    { "D-Pad Down", NULL, NULL, MARIO_BUTTON_CVAR("DDown"), BTN_DDOWN, CHOICE_ITEM, true },
    { "L", "mario_mask.note.detransform", DrawDetransformNote, MARIO_BUTTON_CVAR("L"), BTN_L, CHOICE_DETRANSFORM,
      false },
    { "Z", NULL, NULL, MARIO_BUTTON_CVAR("Z"), BTN_Z, CHOICE_CROUCH, false },
    { "R", "mario_mask.note.target", DrawTargetNote, MARIO_BUTTON_CVAR("R"), BTN_R, CHOICE_TARGET, false },
};

static int32_t GetChoiceCount(const MarioButtonRow* row) {
    return row->canHoldItem ? CHOICE_COUNT : CHOICE_ITEM;
}

static MarioButtonChoice ReadChoice(const MarioButtonRow* row) {
    const int32_t choice = CVarGetInteger(row->cvar, row->defaultChoice);
    const bool isOffered = choice >= 0 && choice < GetChoiceCount(row);

    return isOffered ? (MarioButtonChoice)choice : row->defaultChoice;
}

void MarioControls_ReadBindings(MarioBindings* bindings) {
    *bindings = (MarioBindings){ 0 };
    for (int32_t rowIndex = 0; rowIndex < ARRAY_COUNT(kButtonRows); rowIndex++) {
        const MarioButtonRow* row = &kButtonRows[rowIndex];
        const uint32_t actions = kChoiceActions[ReadChoice(row)];

        for (int32_t action = 0; action < MARIO_ACTION_COUNT; action++) {
            if (actions & ACTION_BIT(action)) {
                bindings->buttons[action] |= row->button;
            }
        }
    }
}

static void RegisterButtonPicker(const SOHModApi* api, const MarioButtonRow* row) {
    SOHModMenuWidget widget = MarioMenu_MakeWidget(MARIO_COLUMN_CONTROLS, SOH_MOD_MENU_COMBOBOX, row->label);

    widget.cvar = row->cvar;
    widget.defaultInt = row->defaultChoice;
    widget.values = kChoiceLabels;
    widget.valueCount = (uint32_t)GetChoiceCount(row);
    api->RegisterMenuWidget(&widget);
}

void MarioControls_RegisterMenu(const SOHModApi* api) {
    MarioMenu_AddDrawn(api, MARIO_COLUMN_CONTROLS, "mario_mask.heading.controls", DrawHeading);
    for (int32_t rowIndex = 0; rowIndex < ARRAY_COUNT(kButtonRows); rowIndex++) {
        const MarioButtonRow* row = &kButtonRows[rowIndex];

        RegisterButtonPicker(api, row);
        if (row->drawNote != NULL) {
            MarioMenu_AddDrawn(api, MARIO_COLUMN_CONTROLS, row->noteId, row->drawNote);
        }
    }
}
