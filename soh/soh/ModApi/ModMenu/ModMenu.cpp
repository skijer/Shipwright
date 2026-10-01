#include "ModMenu.h"

#include <string>
#include <utility>
#include <vector>
#include <deque>
#include <spdlog/spdlog.h>

#include "soh/SohGui/SohMenu.h"

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
        default:
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

bool StoreWidget(const SOHModMenuWidget* widget, const char* anchor, bool after) {
    if (widget == nullptr || widget->structSize < SOH_MOD_MENU_WIDGET_MIN_SIZE || widget->section == nullptr ||
        widget->sidebar == nullptr || widget->label == nullptr || widget->section[0] == '\0' ||
        widget->sidebar[0] == '\0' || widget->label[0] == '\0' || widget->column > 2 ||
        widget->type > SOH_MOD_MENU_CUSTOM ||
        (widget->type <= SOH_MOD_MENU_FLOAT_SLIDER && (widget->cvar == nullptr || widget->cvar[0] == '\0'))) {
        return false;
    }
    RegisteredWidget registered = {};
    registered.definition = *widget;
    registered.section = widget->section;
    registered.sidebar = widget->sidebar;
    registered.label = widget->label;
    registered.cvar = widget->cvar == nullptr ? "" : widget->cvar;
    registered.tooltip = widget->tooltip == nullptr ? "" : widget->tooltip;
    registered.anchor = anchor == nullptr ? "" : anchor;
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
    return label != nullptr && value != nullptr && ImGui::Checkbox(label, value);
}

bool ModUI_InputInt(const char* label, int32_t* value) {
    return label != nullptr && value != nullptr && ImGui::InputInt(label, value);
}
