#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>

#include "SohMenu.h"
#include <nlohmann/json.hpp>
#include "soh/Fleet/FleetGame.h"
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>

extern "C" {
extern PlayState* gPlayState;
}

extern std::unordered_map<s16, const char*> warpPointSceneList;

namespace SohGui {
extern std::shared_ptr<SohMenu> mSohMenu;

using namespace UIWidgets;

bool SohMenu::EnsureModSidebar(const std::string& section, const std::string& sidebar, const char* cvar,
                               uint32_t columns) {
    if (!menuEntries.contains(section)) {
        AddMenuEntry(section, cvar);
    }
    auto& entry = menuEntries.at(section);
    if (!entry.sidebars.contains(sidebar)) {
        AddSidebarEntry(section, sidebar, columns);
    }
    return entry.sidebars.at(sidebar).columnCount >= columns;
}

bool SohMenu::MoveModWidget(const WidgetPath& path, const std::string& label, const std::string& anchor, bool after) {
    if (!menuEntries.contains(path.sectionName)) {
        return false;
    }
    auto& sidebars = menuEntries.at(path.sectionName).sidebars;
    if (!sidebars.contains(path.sidebarName) || sidebars.at(path.sidebarName).columnWidgets.size() <= path.column) {
        return false;
    }
    auto& widgets = sidebars.at(path.sidebarName).columnWidgets.at(path.column);
    auto source =
        std::find_if(widgets.begin(), widgets.end(), [&](const auto& widget) { return widget.name == label; });
    auto target =
        std::find_if(widgets.begin(), widgets.end(), [&](const auto& widget) { return widget.name == anchor; });
    if (source == widgets.end() || target == widgets.end() || source == target) {
        return false;
    }
    auto moved = std::move(*source);
    widgets.erase(source);
    target = std::find_if(widgets.begin(), widgets.end(), [&](const auto& widget) { return widget.name == anchor; });
    widgets.insert(target + (after ? 1 : 0), std::move(moved));
    return true;
}

bool SohMenu::HasModWidgetPath(const WidgetPath& path) const {
    auto section = menuEntries.find(path.sectionName);
    if (section == menuEntries.end()) {
        return false;
    }
    auto sidebar = section->second.sidebars.find(path.sidebarName);
    return sidebar != section->second.sidebars.end() &&
           static_cast<uint32_t>(path.column) < sidebar->second.columnCount;
}

void SohMenu::AddSidebarEntry(std::string sectionName, std::string sidebarName, uint32_t columnCount) {
    assert(!sectionName.empty());
    assert(!sidebarName.empty());
    menuEntries.at(sectionName).sidebars.emplace(sidebarName, SidebarEntry{ .columnCount = columnCount });
    menuEntries.at(sectionName).sidebarOrder.push_back(sidebarName);
}

WidgetInfo& SohMenu::AddWidget(WidgetPath& pathInfo, std::string widgetName, WidgetType widgetType) {
    assert(!widgetName.empty());                        // Must be unique
    assert(menuEntries.contains(pathInfo.sectionName)); // Section/header must already exist
    assert(menuEntries.at(pathInfo.sectionName).sidebars.contains(pathInfo.sidebarName)); // Sidebar must already exist
    std::unordered_map<std::string, SidebarEntry>& sidebar = menuEntries.at(pathInfo.sectionName).sidebars;
    uint8_t column = pathInfo.column;
    if (sidebar.contains(pathInfo.sidebarName)) {
        while (sidebar.at(pathInfo.sidebarName).columnWidgets.size() < static_cast<size_t>(column) + 1) {
            sidebar.at(pathInfo.sidebarName).columnWidgets.push_back({});
        }
    }
    SidebarEntry& entry = sidebar.at(pathInfo.sidebarName);
    entry.columnWidgets.at(column).push_back({ .name = widgetName, .type = widgetType });
    WidgetInfo& widget = entry.columnWidgets.at(column).back();
    switch (widgetType) {
        case WIDGET_CHECKBOX:
        case WIDGET_CVAR_CHECKBOX:
            widget.options = std::make_shared<CheckboxOptions>();
            break;
        case WIDGET_SLIDER_FLOAT:
        case WIDGET_CVAR_SLIDER_FLOAT:
            widget.options = std::make_shared<FloatSliderOptions>();
            break;
        case WIDGET_CVAR_BTN_SELECTOR:
            widget.options = std::make_shared<BtnSelectorOptions>();
            break;
        case WIDGET_SLIDER_INT:
        case WIDGET_CVAR_SLIDER_INT:
            widget.options = std::make_shared<IntSliderOptions>();
            break;
        case WIDGET_COMBOBOX:
        case WIDGET_CVAR_COMBOBOX:
        case WIDGET_AUDIO_BACKEND:
        case WIDGET_VIDEO_BACKEND:
            widget.options = std::make_shared<ComboboxOptions>();
            break;
        case WIDGET_BUTTON:
            widget.options = std::make_shared<ButtonOptions>();
            break;
        case WIDGET_WINDOW_BUTTON:
            widget.options = std::make_shared<WindowButtonOptions>();
            break;
        case WIDGET_CVAR_COLOR_PICKER:
        case WIDGET_COLOR_PICKER:
            widget.options = std::make_shared<ColorPickerOptions>();
            break;
        case WIDGET_SEPARATOR_TEXT:
        case WIDGET_TEXT:
            widget.options = std::make_shared<TextOptions>();
            break;
        case WIDGET_SEARCH:
        case WIDGET_SEPARATOR:
        default:
            widget.options = std::make_shared<WidgetOptions>();
    }
    return widget;
}

SohMenu::SohMenu(const std::string& consoleVariable, const std::string& name)
    : Menu(consoleVariable, name, 0, UIWidgets::Colors::LightBlue) {
}

void SohMenu::AddMenuElements() {
    AddMenuSettings();
    AddMenuEnhancements();
    AddMenuRandomizer();
    AddMenuNetwork();
    AddMenuDevTools();

    if (CVarGetInteger(CVAR_SETTING("Menu.SidebarSearch"), 0)) {
        InsertSidebarSearch();
    }

    for (auto& initFunc : MenuInit::GetInitFuncs()) {
        initFunc();
    }

    mMenuElementsInitialized = true;
}

void SohMenu::InitElement() {
    Ship::Menu::InitElement();

    disabledMap = {
        { DISABLE_FOR_NO_VSYNC,
          { [](disabledInfo& info) -> bool {
               return !Ship::Context::GetRawInstance()->GetWindow()->CanDisableVerticalSync();
           },
            "Disabling VSync not supported" } },
        { DISABLE_FOR_NO_WINDOWED_FULLSCREEN,
          { [](disabledInfo& info) -> bool {
               return !Ship::Context::GetRawInstance()->GetWindow()->SupportsWindowedFullscreen();
           },
            "Windowed Fullscreen not supported" } },
        { DISABLE_FOR_NO_MULTI_VIEWPORT,
          { [](disabledInfo& info) -> bool {
               return !Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SupportsViewports();
           },
            "Multi-viewports not supported" } },
        { DISABLE_FOR_NOT_DIRECTX,
          { [](disabledInfo& info) -> bool {
               return Ship::Context::GetRawInstance()->GetWindow()->GetWindowBackend() !=
                      Fast::WindowBackend::FAST3D_DXGI_DX11;
           },
            "Available Only on DirectX" } },
        { DISABLE_FOR_DIRECTX,
          { [](disabledInfo& info) -> bool {
               return Ship::Context::GetRawInstance()->GetWindow()->GetWindowBackend() ==
                      Fast::WindowBackend::FAST3D_DXGI_DX11;
           },
            "Not Available on DirectX" } },
        { DISABLE_FOR_MATCH_REFRESH_RATE_ON,
          { [](disabledInfo& info) -> bool { return CVarGetInteger(CVAR_SETTING("MatchRefreshRate"), 0); },
            "Match Refresh Rate is Enabled" } },
        { DISABLE_FOR_ADVANCED_RESOLUTION_ON,
          { [](disabledInfo& info) -> bool { return CVarGetInteger(CVAR_PREFIX_ADVANCED_RESOLUTION ".Enabled", 0); },
            "Advanced Resolution Enabled" } },
        { DISABLE_FOR_VERTICAL_RES_TOGGLE_ON,
          { [](disabledInfo& info) -> bool {
               return CVarGetInteger(CVAR_PREFIX_ADVANCED_RESOLUTION ".VerticalResolutionToggle", 0);
           },
            "Vertical Resolution Toggle Enabled" } },
        { DISABLE_FOR_LOW_RES_MODE_ON,
          { [](disabledInfo& info) -> bool { return CVarGetInteger(CVAR_LOW_RES_MODE, 0); }, "N64 Mode Enabled" } },
        { DISABLE_FOR_NULL_PLAY_STATE,
          { [](disabledInfo& info) -> bool { return gPlayState == NULL; }, "Save Not Loaded" } },
        { DISABLE_FOR_DEBUG_MODE_OFF,
          { [](disabledInfo& info) -> bool { return !CVarGetInteger(CVAR_DEVELOPER_TOOLS("DebugEnabled"), 0); },
            "Debug Mode is Disabled" } },
        { DISABLE_FOR_FRAME_ADVANCE_OFF,
          { [](disabledInfo& info) -> bool { return !(gPlayState != nullptr && gPlayState->frameAdvCtx.enabled); },
            "Frame Advance is Disabled" } },
        { DISABLE_FOR_ADVANCED_RESOLUTION_OFF,
          { [](disabledInfo& info) -> bool { return !CVarGetInteger(CVAR_PREFIX_ADVANCED_RESOLUTION ".Enabled", 0); },
            "Advanced Resolution is Disabled" } },
        { DISABLE_FOR_VERTICAL_RESOLUTION_OFF,
          { [](disabledInfo& info) -> bool {
               return !CVarGetInteger(CVAR_PREFIX_ADVANCED_RESOLUTION ".VerticalResolutionToggle", 0);
           },
            "Vertical Resolution Toggle is Off" } },
    };
}

void SohMenu::UpdateElement() {
    Ship::Menu::UpdateElement();
}

std::string SohMenu::FleetMenuModel() {
    if (!mMenuElementsInitialized) {
        return "null";
    }
    nlohmann::json sections = nlohmann::json::array();
    for (const auto& name : menuOrder) {
        const auto& section = menuEntries.at(name);
        nlohmann::json sidebars = nlohmann::json::array();
        for (const auto& label : section.sidebarOrder) {
            if (label == "Search") {
                continue;
            }
            const auto& sidebar = section.sidebars.at(label);
            nlohmann::json widgets = nlohmann::json::array();
            for (size_t column = 0; column < sidebar.columnWidgets.size(); column++) {
                const auto& entries = sidebar.columnWidgets[column];
                for (size_t index = 0; index < entries.size(); index++) {
                    const auto& widget = entries[index];
                    if (widget.type == WIDGET_SEARCH) {
                        continue;
                    }
                    widgets.push_back({ { "name", widget.name }, { "cvar", widget.cVar ? widget.cVar : "" },
                        { "tooltip", widget.options ? std::string(widget.options->tooltip) : "" },
                        { "search", !widget.hideInSearch }, { "column", column }, { "index", index } });
                }
            }
            sidebars.push_back({ { "name", label }, { "columns", sidebar.columnCount }, { "widgets", widgets } });
        }
        sections.push_back({ { "name", name }, { "sidebars", sidebars } });
    }
    return sections.dump();
}

SidebarEntry* SohMenu::FindFleetPage(const std::string& section, const std::string& sidebar) {
    const auto entry = menuEntries.find(section);
    if (entry == menuEntries.end()) {
        return nullptr;
    }
    const auto side = entry->second.sidebars.find(sidebar);
    return side != entry->second.sidebars.end() ? &side->second : nullptr;
}

// Fleet draws a page outside DrawElement: run what DrawElement runs first, once per page and frame.
void SohMenu::PrepareFleetPage(const std::string& section, const std::string& sidebar) {
    ImGui::SetCurrentContext(Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetImGuiContext());
    static std::map<std::pair<std::string, std::string>, int> updatedPages;
    const int frame = ImGui::GetFrameCount();
    auto& updatedFrame = updatedPages[std::make_pair(section, sidebar)];
    if (updatedFrame != frame) {
        updatedFrame = frame;
        auto& updates = MenuInit::GetUpdateFuncs();
        const auto sectionUpdates = updates.find(menuEntries.at(section).label);
        if (sectionUpdates != updates.end()) {
            const auto pageUpdates = sectionUpdates->second.find(sidebar);
            if (pageUpdates != sectionUpdates->second.end()) {
                for (auto& update : pageUpdates->second) {
                    update();
                }
            }
        }
    }
    static int previousFrame = -1;
    if (previousFrame != frame) {
        previousFrame = frame;
        for (auto& [reason, info] : disabledMap) {
            info.active = info.evaluation(info);
        }
        UpdateElement();
    }
}

// A combobox whose CVar holds a value it no longer lists would assert inside ImGui.
bool SohMenu::CanFleetDrawWidget(const WidgetInfo& widget) const {
    if (widget.type != WIDGET_CVAR_COMBOBOX || widget.cVar == nullptr || widget.cVar[0] == '\0') {
        return true;
    }
    const auto options = std::static_pointer_cast<UIWidgets::ComboboxOptions>(widget.options);
    return options && options->comboMap.contains(CVarGetInteger(widget.cVar, options->defaultIndex));
}

void SohMenu::FleetDrawWidget(const std::string& section, const std::string& sidebar, int column, int index) {
    SidebarEntry* page = FindFleetPage(section, sidebar);
    if (page == nullptr || column < 0 || column >= static_cast<int>(page->columnWidgets.size())) {
        return;
    }
    auto& widgets = page->columnWidgets[column];
    if (index < 0 || index >= static_cast<int>(widgets.size())) {
        return;
    }
    PrepareFleetPage(section, sidebar);
    if (CanFleetDrawWidget(widgets[index])) {
        MenuDrawItem(widgets[index], GetMenuThemeColor());
    }
}

// The column block of DrawElement, so a page Fleet shows looks exactly as it does in Ship's own menu.
void SohMenu::FleetDrawPage(const std::string& section, const std::string& sidebar) {
    SidebarEntry* page = FindFleetPage(section, sidebar);
    if (page == nullptr) {
        return;
    }
    PrepareFleetPage(section, sidebar);
    const ImGuiStyle& style = ImGui::GetStyle();
    const size_t columns = ImGui::GetMainViewport()->WorkSize.x < 800 ? 1 : std::max<size_t>(page->columnCount, 1);
    const bool useColumns = columns > 1;
    const float columnWidth = (ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * columns) / columns;
    for (size_t column = 0; column < page->columnWidgets.size(); column++) {
        if (useColumns) {
            ImGui::BeginChild((sidebar + " Column " + std::to_string(column)).c_str(), { columnWidth, 0 },
                              ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoTitleBar);
        }
        for (auto& widget : page->columnWidgets[column]) {
            if (CanFleetDrawWidget(widget)) {
                MenuDrawItem(widget, GetMenuThemeColor());
            }
        }
        if (useColumns) {
            ImGui::EndChild();
        }
        if (column + 1 < columns) {
            ImGui::SameLine();
        }
    }
}

void SohMenu::Draw() {
    if (FleetGame_IsHosted()) {
        ImGui::SetCurrentContext(Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetImGuiContext());
    }
    Ship::Menu::Draw();
}

void SohMenu::DrawElement() {
    if (mMenuElementsInitialized) {
        Ship::Menu::DrawElement();
    }
}
} // namespace SohGui
