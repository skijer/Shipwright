#ifndef SOH_MOD_MENU_H
#define SOH_MOD_MENU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SOH_MOD_MENU_CHECKBOX,
    SOH_MOD_MENU_INT_SLIDER,
    SOH_MOD_MENU_FLOAT_SLIDER,
    SOH_MOD_MENU_SEPARATOR,
    SOH_MOD_MENU_TEXT,
    SOH_MOD_MENU_CUSTOM,
} SOHModMenuWidgetType;

typedef void (*SOHModMenuCallback)(void);

typedef struct {
    uint32_t structSize;
    const char* section;
    const char* sidebar;
    uint8_t column;
    SOHModMenuWidgetType type;
    const char* label;
    const char* cvar;
    const char* tooltip;
    int32_t defaultInt;
    int32_t minInt;
    int32_t maxInt;
    float defaultFloat;
    float minFloat;
    float maxFloat;
    SOHModMenuCallback onChange;
} SOHModMenuWidget;

#define SOH_MOD_MENU_WIDGET_MIN_SIZE (offsetof(SOHModMenuWidget, onChange) + sizeof(((SOHModMenuWidget*)0)->onChange))

bool ModMenu_RegisterWidget(const SOHModMenuWidget* widget);
bool ModMenu_RegisterSidebar(const char* section, const char* sidebar, const char* selectionCvar, uint8_t columns);
bool ModMenu_RegisterWidgetAt(const SOHModMenuWidget* widget, const char* anchor, bool after);
bool ModUI_BeginTab(const char* label);
void ModUI_EndTab(void);
bool ModUI_BeginSection(const char* label);
void ModUI_EndSection(void);
void ModUI_Text(const char* text);
bool ModUI_Checkbox(const char* label, bool* value);
bool ModUI_InputInt(const char* label, int32_t* value);

#ifdef __cplusplus
}
#endif

#endif
