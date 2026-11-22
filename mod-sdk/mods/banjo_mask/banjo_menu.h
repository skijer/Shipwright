#ifndef BANJO_MENU_H
#define BANJO_MENU_H

#include "banjo_host.h"

#define BANJO_MENU_SECTION "Mods"
#define BANJO_MENU_SIDEBAR "Banjo Mask"
#define BANJO_MENU_PAGE_CVAR "gMods.Menu.Page"

typedef enum BanjoMenuColumn {
    BANJO_COLUMN_CONTROLS,
    BANJO_COLUMN_RANDOMIZER,
    BANJO_COLUMN_CONFIGURATION,
    BANJO_COLUMN_COUNT
} BanjoMenuColumn;

static inline SOHModMenuWidget BanjoMenu_MakeWidget(BanjoMenuColumn column, SOHModMenuWidgetType type,
                                                    const char* label) {
    SOHModMenuWidget widget = { sizeof(SOHModMenuWidget) };

    widget.section = BANJO_MENU_SECTION;
    widget.sidebar = BANJO_MENU_SIDEBAR;
    widget.column = (uint8_t)column;
    widget.type = type;
    widget.label = label;
    return widget;
}

// Text goes through a custom widget: Ship's SOH_MOD_MENU_TEXT aborts the mod's init. The id finds the callback, so it
// must be unique among every mod's widgets.
static inline void BanjoMenu_AddDrawn(const SOHModApi* api, BanjoMenuColumn column, const char* id,
                                      SOHModMenuCallback draw) {
    SOHModMenuWidget widget = BanjoMenu_MakeWidget(column, SOH_MOD_MENU_CUSTOM, id);

    widget.onChange = draw;
    api->RegisterMenuWidget(&widget);
}

#endif
