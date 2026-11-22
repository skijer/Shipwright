#include "banjo_rando.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include "banjo_menu.h"

#define BANJO_RANDO_OPTION(key, label, tooltip) \
    { "banjo_mask." key, BANJO_RANDO_CVAR_PREFIX "banjo_mask." key, label, tooltip }
#define BANJO_MENU_LINK_LABEL "Banjo Mask Options"

typedef struct BanjoRandoOption {
    const char* key;
    const char* cvar;
    const char* label;
    const char* tooltip;
} BanjoRandoOption;

static const char* const kOffOn[] = { "Off", "On" };

static const BanjoRandoOption kRandoOptions[] = {
    BANJO_RANDO_OPTION("shuffle_mask", "Shuffle Banjo Mask", "Places the Banjo Mask somewhere in the seed."),
    BANJO_RANDO_OPTION("shuffle_moves", "Shuffle Banjo Moves", "Off gives every move Bottles teaches with the mask."),
    BANJO_RANDO_OPTION("shuffle_pads", "Shuffle Flight and Shock Spring Pads",
                       "Off gives the Flight Pad and Shock Spring Pad items with the mask."),
};

static void DrawHeading(void) {
    ModUI_Text("Randomizer");
}

static void RegisterOption(const SOHModApi* api, const BanjoRandoOption* entry) {
    SOHModRandoOption option = { sizeof(SOHModRandoOption) };
    SOHModMenuWidget widget = BanjoMenu_MakeWidget(BANJO_COLUMN_RANDOMIZER, SOH_MOD_MENU_COMBOBOX, entry->label);

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

void BanjoRando_RegisterOptions(const SOHModApi* api) {
    BanjoMenu_AddDrawn(api, BANJO_COLUMN_RANDOMIZER, "banjo_mask.heading.randomizer", DrawHeading);
    for (int32_t index = 0; index < ARRAY_COUNT(kRandoOptions); index++) {
        RegisterOption(api, &kRandoOptions[index]);
    }
}

static void DrawMenuLink(void) {
    if (ModUI_Button(BANJO_MENU_LINK_LABEL, 0.0f, 0.0f)) {
        CVarSetString("gSettings.Menu.ActiveHeader", BANJO_MENU_SECTION);
        CVarSetString(BANJO_MENU_PAGE_CVAR, BANJO_MENU_SIDEBAR);
    }
}

void BanjoRando_RegisterMenuLink(const SOHModApi* api) {
    SOHModMenuWidget widget = { sizeof(SOHModMenuWidget) };

    widget.section = BANJO_RANDO_SECTION;
    widget.sidebar = "Mods";
    widget.type = SOH_MOD_MENU_CUSTOM;
    widget.label = BANJO_MENU_LINK_LABEL;
    widget.onChange = DrawMenuLink;
    api->RegisterMenuSidebar(widget.section, widget.sidebar, BANJO_RANDO_SIDEBAR_CVAR, 1);
    api->RegisterMenuWidget(&widget);
}
