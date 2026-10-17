#include "ModMenu.h"

#include <algorithm>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>

#include "soh/SohGui/SohGui.hpp"
#include "soh/SohGui/SohMenu.h"
#include "soh/SohGui/UIWidgets.hpp"

namespace SohGui {
extern std::shared_ptr<SohMenu> mSohMenu;
}

namespace {

struct RegisteredWidget {
    SOHModMenuWidget definition;
    std::string section;
    std::string sidebar;
    std::string label;
    std::string cvar;
    std::string tooltip;
    std::string anchor;
    std::vector<std::string> values;
    bool after = false;
};

std::deque<RegisteredWidget> sWidgets;

struct RegisteredSidebar {
    std::string section;
    std::string sidebar;
    std::string cvar;
    uint8_t columns;
};

std::deque<RegisteredSidebar> sSidebars;

// The map points into the registered strings, which a deque element never moves.
std::map<int32_t, const char*> BuildComboMap(const std::vector<std::string>& values) {
    std::map<int32_t, const char*> comboMap;
    for (size_t index = 0; index < values.size(); index++) {
        comboMap[static_cast<int32_t>(index)] = values[index].c_str();
    }
    return comboMap;
}

void AddWidget(const RegisteredWidget& registered) {
    const auto& definition = registered.definition;
    WidgetPath path = { registered.section, registered.sidebar, (SectionColumns)definition.column };
    if (!SohGui::mSohMenu->HasModWidgetPath(path)) {
        SPDLOG_ERROR("[ModMenu] Invalid path for widget '{}': {}/{} column {}", registered.label, registered.section,
                     registered.sidebar, definition.column);
        return;
    }
    WidgetType type;
    switch (definition.type) {
        case SOH_MOD_MENU_CHECKBOX:
            type = WIDGET_CVAR_CHECKBOX;
            break;
        case SOH_MOD_MENU_INT_SLIDER:
            type = WIDGET_CVAR_SLIDER_INT;
            break;
        case SOH_MOD_MENU_FLOAT_SLIDER:
            type = WIDGET_CVAR_SLIDER_FLOAT;
            break;
        case SOH_MOD_MENU_SEPARATOR:
            type = WIDGET_SEPARATOR;
            break;
        case SOH_MOD_MENU_TEXT:
            type = WIDGET_TEXT;
            break;
        case SOH_MOD_MENU_CUSTOM:
            type = WIDGET_CUSTOM;
            break;
        case SOH_MOD_MENU_COMBOBOX:
            type = WIDGET_CVAR_COMBOBOX;
            break;
        default:
            SPDLOG_ERROR("[ModMenu] Unknown widget type {} for '{}'", (int32_t)definition.type, registered.label);
            return;
    }

    auto& widget = SohGui::mSohMenu->AddWidget(path, registered.label, type);
    if (!registered.cvar.empty()) {
        widget.CVar(registered.cvar.c_str());
    }
    switch (definition.type) {
        case SOH_MOD_MENU_CHECKBOX:
            widget.Options(UIWidgets::CheckboxOptions()
                               .DefaultValue(definition.defaultInt != 0)
                               .Tooltip(registered.tooltip.c_str()));
            break;
        case SOH_MOD_MENU_INT_SLIDER:
            widget.Options(UIWidgets::IntSliderOptions()
                               .DefaultValue(definition.defaultInt)
                               .Min(definition.minInt)
                               .Max(definition.maxInt)
                               .Tooltip(registered.tooltip.c_str()));
            break;
        case SOH_MOD_MENU_FLOAT_SLIDER:
            widget.Options(UIWidgets::FloatSliderOptions()
                               .DefaultValue(definition.defaultFloat)
                               .Min(definition.minFloat)
                               .Max(definition.maxFloat)
                               .Tooltip(registered.tooltip.c_str()));
            break;
        case SOH_MOD_MENU_TEXT:
            widget.Options(UIWidgets::TextOptions().Tooltip(registered.tooltip.c_str()));
            break;
        case SOH_MOD_MENU_COMBOBOX:
            widget.Options(UIWidgets::ComboboxOptions()
                               .ComboMap(BuildComboMap(registered.values))
                               .DefaultIndex(static_cast<uint32_t>(definition.defaultInt))
                               .Tooltip(registered.tooltip.c_str()));
            break;
        default:
            break;
    }
    if (definition.onChange != nullptr) {
        if (definition.type == SOH_MOD_MENU_CUSTOM) {
            widget.customFunction = [callback = definition.onChange](WidgetInfo&) { callback(); };
        } else {
            widget.Callback([callback = definition.onChange](WidgetInfo&) { callback(); });
        }
    }
    if (!registered.anchor.empty()) {
        if (!SohGui::mSohMenu->MoveModWidget(path, registered.label, registered.anchor, registered.after)) {
            SPDLOG_WARN("[ModMenu] Anchor '{}' not found for widget '{}'", registered.anchor, registered.label);
        }
    }
}

void AddSidebar(const RegisteredSidebar& sidebar) {
    SohGui::mSohMenu->EnsureModSidebar(sidebar.section, sidebar.sidebar, sidebar.cvar.c_str(), sidebar.columns);
}

bool IsMenuBuilt() {
    return SohGui::mSohMenu != nullptr && SohGui::mSohMenu->AreMenuElementsInitialized();
}

void RegisterWidgets() {
    for (const auto& sidebar : sSidebars) {
        AddSidebar(sidebar);
    }
    for (const auto& widget : sWidgets) {
        AddWidget(widget);
    }
}

RegisterMenuInitFunc sMenuInit(RegisterWidgets);

bool IsFilled(const char* text) {
    return text != nullptr && text[0] != '\0';
}

bool HasComboValues(const SOHModMenuWidget* widget) {
    return widget->structSize >= SOH_MOD_MENU_WIDGET_COMBOBOX_SIZE && widget->values != nullptr &&
           widget->valueCount > 0;
}

bool IsValidWidget(const SOHModMenuWidget* widget) {
    if (widget == nullptr || widget->structSize < SOH_MOD_MENU_WIDGET_MIN_SIZE) {
        return false;
    }
    if (!IsFilled(widget->section) || !IsFilled(widget->sidebar) || !IsFilled(widget->label) || widget->column > 2) {
        return false;
    }
    switch (widget->type) {
        case SOH_MOD_MENU_CHECKBOX:
        case SOH_MOD_MENU_INT_SLIDER:
        case SOH_MOD_MENU_FLOAT_SLIDER:
            return IsFilled(widget->cvar);
        case SOH_MOD_MENU_COMBOBOX:
            return IsFilled(widget->cvar) && HasComboValues(widget);
        case SOH_MOD_MENU_SEPARATOR:
        case SOH_MOD_MENU_TEXT:
        case SOH_MOD_MENU_CUSTOM:
            return true;
        default:
            return false;
    }
}

std::vector<std::string> CopyComboValues(const SOHModMenuWidget* widget) {
    std::vector<std::string> values;
    if (widget->type != SOH_MOD_MENU_COMBOBOX) {
        return values;
    }
    for (uint32_t index = 0; index < widget->valueCount; index++) {
        values.emplace_back(widget->values[index] == nullptr ? "" : widget->values[index]);
    }
    return values;
}

bool StoreWidget(const SOHModMenuWidget* widget, const char* anchor, bool after) {
    if (!IsValidWidget(widget)) {
        return false;
    }
    RegisteredWidget registered = {};
    // A mod built against an older header passes a shorter struct; the missing tail stays zero.
    std::memcpy(&registered.definition, widget, std::min<size_t>(widget->structSize, sizeof(SOHModMenuWidget)));
    registered.section = widget->section;
    registered.sidebar = widget->sidebar;
    registered.label = widget->label;
    registered.cvar = widget->cvar == nullptr ? "" : widget->cvar;
    registered.tooltip = widget->tooltip == nullptr ? "" : widget->tooltip;
    registered.anchor = anchor == nullptr ? "" : anchor;
    registered.values = CopyComboValues(widget);
    registered.after = after;
    sWidgets.push_back(std::move(registered));
    if (IsMenuBuilt()) {
        AddWidget(sWidgets.back());
    }
    return true;
}

} // namespace

bool ModMenu_RegisterWidget(const SOHModMenuWidget* widget) {
    return StoreWidget(widget, nullptr, false);
}

bool ModMenu_RegisterSidebar(const char* section, const char* sidebar, const char* selectionCvar, uint8_t columns) {
    if (section == nullptr || section[0] == '\0' || sidebar == nullptr || sidebar[0] == '\0' ||
        selectionCvar == nullptr || selectionCvar[0] == '\0' || columns < 1 || columns > 3) {
        return false;
    }
    sSidebars.push_back({ section, sidebar, selectionCvar, columns });
    if (IsMenuBuilt()) {
        AddSidebar(sSidebars.back());
    }
    return true;
}

bool ModMenu_RegisterWidgetAt(const SOHModMenuWidget* widget, const char* anchor, bool after) {
    if (anchor == nullptr || anchor[0] == '\0') {
        return false;
    }
    return StoreWidget(widget, anchor, after);
}

bool ModUI_BeginTab(const char* label) {
    return label != nullptr && ImGui::BeginTabItem(label);
}

void ModUI_EndTab(void) {
    ImGui::EndTabItem();
}

bool ModUI_BeginSection(const char* label) {
    return label != nullptr && ImGui::TreeNode(label);
}

void ModUI_EndSection(void) {
    ImGui::TreePop();
}

void ModUI_Text(const char* text) {
    if (text != nullptr) {
        ImGui::TextUnformatted(text);
    }
}

bool ModUI_Checkbox(const char* label, bool* value) {
    if (label == nullptr || value == nullptr) {
        return false;
    }
    UIWidgets::PushStyleCheckbox(THEME_COLOR);
    const bool changed = ImGui::Checkbox(label, value);
    UIWidgets::PopStyleCheckbox();
    return changed;
}

bool ModUI_InputInt(const char* label, int32_t* value) {
    if (label == nullptr || value == nullptr) {
        return false;
    }
    UIWidgets::PushStyleInput(THEME_COLOR);
    const bool changed = ImGui::InputInt(label, value);
    UIWidgets::PopStyleInput();
    return changed;
}
