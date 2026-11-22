#include "banjo_config.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include "banjo_menu.h"

#define BANJO_MODE_CVAR "gMods.BanjoMask.BanjoMode"
#define BANJO_SHIP_SHADING_CVAR BANJO_CONFIG_CVAR("ShipShading")

static void DrawHeading(void) {
    ModUI_Text("Configuration");
}

static void RegisterBanjoMode(const SOHModApi* api) {
    SOHModMenuWidget widget = BanjoMenu_MakeWidget(BANJO_COLUMN_CONFIGURATION, SOH_MOD_MENU_CHECKBOX, "Banjo Mode");

    widget.cvar = BANJO_MODE_CVAR;
    widget.tooltip = "Play as Banjo and Kazooie all the time, without the mask.";
    api->RegisterMenuWidget(&widget);
}

static void RegisterFloatSlider(const SOHModApi* api, const char* label, const char* cvar, const char* tooltip,
                                float defaultValue) {
    SOHModMenuWidget widget = BanjoMenu_MakeWidget(BANJO_COLUMN_CONFIGURATION, SOH_MOD_MENU_FLOAT_SLIDER, label);

    widget.cvar = cvar;
    widget.tooltip = tooltip;
    widget.defaultFloat = defaultValue;
    widget.minFloat = 0.0f;
    widget.maxFloat = 1.0f;
    api->RegisterMenuWidget(&widget);
}

void BanjoConfig_RegisterMenu(const SOHModApi* api) {
    BanjoMenu_AddDrawn(api, BANJO_COLUMN_CONFIGURATION, "banjo_mask.heading.configuration", DrawHeading);
    RegisterBanjoMode(api);
    RegisterFloatSlider(api, "Ship Shading", BANJO_SHIP_SHADING_CVAR,
                        "How much the game's lighting and shading affect Banjo. 0 keeps his original colors.", 1.0f);
    RegisterFloatSlider(api, "Banjo Volume", BANJO_VOLUME_CVAR, "Volume of Banjo and Kazooie's voices and sounds.",
                        0.8f);
}

bool BanjoConfig_IsBanjoModeOn(void) {
    return CVarGetInteger(BANJO_MODE_CVAR, 0) != 0;
}

float BanjoConfig_GetShipShading(void) {
    return CVarGetFloat(BANJO_SHIP_SHADING_CVAR, 1.0f);
}
