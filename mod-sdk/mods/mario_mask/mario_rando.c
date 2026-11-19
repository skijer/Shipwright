#include "mario_rando.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include "mario_menu.h"

#define MARIO_RANDO_OPTION(key, label, tooltip) \
    { "mario_mask." key, MARIO_RANDO_CVAR_PREFIX "mario_mask." key, label, tooltip }
#define MARIO_MENU_LINK_LABEL "Mario Mask Options"

typedef struct MarioRandoOption {
    const char* key;
    const char* cvar;
    const char* label;
    const char* tooltip;
} MarioRandoOption;

static const char* const kOffOn[] = { "Off", "On" };

static const MarioRandoOption kRandoOptions[] = {
    MARIO_RANDO_OPTION("shuffle_mask", "Shuffle Mario Mask", "Places the Mario Mask somewhere in the seed."),
    MARIO_RANDO_OPTION("shuffle_items", "Shuffle Mario Items", "Off gives every Mario item with the mask."),
    MARIO_RANDO_OPTION("shuffle_punch", "Shuffle Punch", "Mario cannot punch until he finds this ability."),
    MARIO_RANDO_OPTION("shuffle_crouch", "Shuffle Crouch", "Mario cannot crouch or ground pound until he finds this."),
};

static void DrawHeading(void) {
    ModUI_Text("Randomizer");
}

static void RegisterOption(const SOHModApi* api, const MarioRandoOption* entry) {
    SOHModRandoOption option = { sizeof(SOHModRandoOption) };
    SOHModMenuWidget widget = MarioMenu_MakeWidget(MARIO_COLUMN_RANDOMIZER, SOH_MOD_MENU_COMBOBOX, entry->label);

    option.key = entry->key;
    option.label = entry->label;
    option.tooltip = entry->tooltip;
    option.values = kOffOn;
    option.valueCount = ARRAY_COUNT(kOffOn);
    option.widget = SOH_MOD_RANDO_WIDGET_COMBOBOX;
    option.flags = SOH_MOD_RANDO_OPTION_HIDDEN_FROM_RANDO_MENU;
    api->RegisterRandoOption(&option);

    widget.cvar = entry->cvar;
    widget.tooltip = entry->tooltip;
    widget.values = kOffOn;
    widget.valueCount = ARRAY_COUNT(kOffOn);
    api->RegisterMenuWidget(&widget);
}

void MarioRando_RegisterOptions(const SOHModApi* api) {
    MarioMenu_AddDrawn(api, MARIO_COLUMN_RANDOMIZER, "mario_mask.heading.randomizer", DrawHeading);
    for (int32_t index = 0; index < ARRAY_COUNT(kRandoOptions); index++) {
        RegisterOption(api, &kRandoOptions[index]);
    }
}

static void DrawMenuLink(void) {
    if (ModUI_Button(MARIO_MENU_LINK_LABEL, 0.0f, 0.0f)) {
        CVarSetString("gSettings.Menu.ActiveHeader", MARIO_MENU_SECTION);
        CVarSetString(MARIO_MENU_PAGE_CVAR, MARIO_MENU_SIDEBAR);
    }
}

void MarioRando_RegisterMenuLink(const SOHModApi* api) {
    SOHModMenuWidget widget = { sizeof(SOHModMenuWidget) };

    widget.section = MARIO_RANDO_SECTION;
    widget.sidebar = "Mods";
    widget.type = SOH_MOD_MENU_CUSTOM;
    widget.label = MARIO_MENU_LINK_LABEL;
    widget.onChange = DrawMenuLink;
    api->RegisterMenuSidebar(widget.section, widget.sidebar, MARIO_RANDO_SIDEBAR_CVAR, 1);
    api->RegisterMenuWidget(&widget);
}
