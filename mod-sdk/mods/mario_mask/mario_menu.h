#ifndef MARIO_MENU_H
#define MARIO_MENU_H

#include "mario_host.h"

#define MARIO_MENU_SECTION "Mods"
#define MARIO_MENU_SIDEBAR "Mario Mask"
#define MARIO_MENU_PAGE_CVAR "gMods.Menu.Page"

typedef enum MarioMenuColumn {
    MARIO_COLUMN_CONTROLS,
    MARIO_COLUMN_RANDOMIZER,
    MARIO_COLUMN_CONFIGURATION,
    MARIO_COLUMN_COUNT
} MarioMenuColumn;

static inline SOHModMenuWidget MarioMenu_MakeWidget(MarioMenuColumn column, SOHModMenuWidgetType type,
                                                    const char* label) {
    SOHModMenuWidget widget = { sizeof(SOHModMenuWidget) };

    widget.section = MARIO_MENU_SECTION;
    widget.sidebar = MARIO_MENU_SIDEBAR;
    widget.column = (uint8_t)column;
    widget.type = type;
    widget.label = label;
    return widget;
}

// Text goes through a custom widget: Ship's SOH_MOD_MENU_TEXT aborts the mod's init. The id finds the callback, so it
// must be unique among every mod's widgets.
static inline void MarioMenu_AddDrawn(const SOHModApi* api, MarioMenuColumn column, const char* id,
                                      SOHModMenuCallback draw) {
    SOHModMenuWidget widget = MarioMenu_MakeWidget(column, SOH_MOD_MENU_CUSTOM, id);

    widget.onChange = draw;
    api->RegisterMenuWidget(&widget);
}

#endif
