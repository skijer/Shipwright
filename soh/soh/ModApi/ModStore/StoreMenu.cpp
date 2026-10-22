#include "ModStore.h"
#include "StoreHttp.h"
#include "StoreInternal.h"

#include <algorithm>
#include <functional>
#include <set>

#include <SDL2/SDL_misc.h>
#include <ship/Context.h>

#include "soh/cvar_prefixes.h"
#include "soh/SohGui/SohGui.hpp"
#include "soh/SohGui/UIWidgets.hpp"
#include "variables.h"

namespace {

#define CVAR_STORE_CHECK_AT_STARTUP CVAR_SETTING("ModStore.CheckAtStartup")
constexpr int StartupCheckUnasked = 0;
constexpr int StartupCheckOn = 1;
constexpr int StartupCheckOff = 2;
constexpr const char* OtherModsPack = "other";

using Actions = std::vector<std::function<void()>>;

std::map<std::string, std::set<std::string>> sUncheckedByPack;
bool sRequestedCatalog = false;

UIWidgets::ButtonOptions InlineButton() {
    return UIWidgets::ButtonOptions().Size(UIWidgets::Sizes::Inline).Color(THEME_COLOR);
}

void SetStartupCheck(int value) {
    CVarSetInteger(CVAR_STORE_CHECK_AT_STARTUP, value);
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

const char* DescribeState(StoreModState state) {
    switch (state) {
        case StoreModState::NotInstalled:
            return "not installed";
        case StoreModState::Installed:
            return "installed";
        case StoreModState::Outdated:
            return "update available";
        case StoreModState::Disabled:
            return "disabled";
        case StoreModState::InstallsOnRestart:
            return "installs on restart";
        case StoreModState::DisabledOnRestart:
            return "disabled on restart";
        case StoreModState::RemovedOnRestart:
            return "removed on restart";
        case StoreModState::Unavailable:
            return "not built for this Unbound yet";
        default:
            return "unknown";
    }
}

ImVec4 StateColor(StoreModState state) {
    switch (state) {
        case StoreModState::Installed:
            return ImVec4(0.4f, 0.85f, 0.4f, 1.0f);
        case StoreModState::Outdated:
            return UIWidgets::ColorValues.at(UIWidgets::Colors::Yellow);
        case StoreModState::Unavailable:
            return ImVec4(0.9f, 0.4f, 0.4f, 1.0f);
        case StoreModState::InstallsOnRestart:
        case StoreModState::DisabledOnRestart:
        case StoreModState::RemovedOnRestart:
            return UIWidgets::ColorValues.at(UIWidgets::Colors::Cyan);
        default:
            return UIWidgets::ColorValues.at(UIWidgets::Colors::Gray);
    }
}

StoreModState StateOf(const StoreView& view, const std::string& id) {
    const auto state = view.states.find(id);
    return state != view.states.end() ? state->second : StoreModState::NotInstalled;
}

std::string Join(const std::vector<std::string>& values) {
    std::string text;
    for (const std::string& value : values) {
        text += (text.empty() ? "" : ", ") + value;
    }
    return text;
}

void DrawModTooltip(const StoreMod& mod) {
    std::string text = mod.description.empty() ? mod.name : mod.description;
    text += "\n\nBy " + mod.author + " (" + mod.repository + ")";
    if (!mod.permissions.empty()) {
        text += "\nPermissions: " + Join(mod.permissions);
    }
    if (!mod.dependencies.empty()) {
        text += "\nAlso installs: " + Join(mod.dependencies);
    }
    UIWidgets::Tooltip(text);
}

void DrawModLabel(const StoreView& view, const StoreMod& mod) {
    ImGui::SameLine();
    ImGui::TextDisabled("v%s", mod.version.c_str());
    ImGui::SameLine();
    const StoreModState state = StateOf(view, mod.id);
    ImGui::TextColored(StateColor(state), "%s", DescribeState(state));
}

// Warns before a change that would leave other installed mods without something they need.
void ConfirmIfNeeded(const StoreView& view, const StoreMod& mod, const char* verb, Actions& actions,
                     std::function<void()> change) {
    const std::vector<std::string> dependents = ModStore_FindDependents(view, mod.id);
    if (dependents.empty()) {
        actions.push_back(std::move(change));
        return;
    }
    const std::string title = std::string(verb) + " " + mod.name;
    actions.push_back([title, dependents, change] {
        SohGui::RegisterPopup(title, Join(dependents) + " need it and will stop working.", "Continue", "Cancel",
                              change);
    });
}

std::vector<StorePack> PacksWithOtherMods(const StoreCatalog& catalog) {
    std::vector<StorePack> packs = catalog.packs;
    std::set<std::string> packed;
    for (const StorePack& pack : packs) {
        packed.insert(pack.mods.begin(), pack.mods.end());
    }
    StorePack other = { OtherModsPack, "Other mods", "", {} };
    for (const auto& [id, mod] : catalog.mods) {
        if (packed.count(id) == 0) {
            other.mods.push_back(id);
        }
    }
    if (!other.mods.empty()) {
        packs.push_back(std::move(other));
    }
    return packs;
}

void DrawPack(const StoreView& view, const StorePack& pack, Actions& actions) {
    std::set<std::string>& unchecked = sUncheckedByPack[pack.id];
    std::vector<std::string> selected;
    std::vector<std::string> outdated;
    for (const std::string& id : pack.mods) {
        const auto mod = view.catalog.mods.find(id);
        if (mod == view.catalog.mods.end()) {
            continue;
        }
        bool checked = unchecked.count(id) == 0;
        if (UIWidgets::Checkbox((mod->second.name + "##" + pack.id + id).c_str(), &checked,
                                UIWidgets::CheckboxOptions().Color(THEME_COLOR))) {
            if (checked) {
                unchecked.erase(id);
            } else {
                unchecked.insert(id);
            }
        }
        DrawModTooltip(mod->second);
        DrawModLabel(view, mod->second);
        if (checked) {
            selected.push_back(id);
        }
        if (StateOf(view, id) == StoreModState::Outdated) {
            outdated.push_back(id);
        }
    }

    ImGui::BeginDisabled(view.busy || selected.empty());
    if (UIWidgets::Button(("Install selected##" + pack.id).c_str(), InlineButton())) {
        actions.push_back([selected] { ModStore_Install(selected); });
    }
    ImGui::EndDisabled();
    if (!outdated.empty()) {
        ImGui::SameLine();
        ImGui::BeginDisabled(view.busy);
        if (UIWidgets::Button(("Update " + std::to_string(outdated.size()) + "##" + pack.id).c_str(), InlineButton())) {
            actions.push_back([outdated] { ModStore_Install(outdated); });
        }
        ImGui::EndDisabled();
    }
}

void DrawPacks(const StoreView& view, Actions& actions) {
    for (const StorePack& pack : PacksWithOtherMods(view.catalog)) {
        const std::string header = pack.name + " (" + std::to_string(pack.mods.size()) + " mods)##" + pack.id;
        if (!ImGui::CollapsingHeader(header.c_str())) {
            continue;
        }
        ImGui::Indent();
        if (!pack.author.empty()) {
            ImGui::TextDisabled("by %s", pack.author.c_str());
        }
        DrawPack(view, pack, actions);
        ImGui::Unindent();
    }
}

void DrawInstalledRow(const StoreView& view, const StoreMod& mod, StoreModState state, Actions& actions) {
    ImGui::TextUnformatted(mod.name.c_str());
    DrawModTooltip(mod);
    DrawModLabel(view, mod);
    const std::string id = mod.id;
    ImGui::BeginDisabled(view.busy);
    if (state == StoreModState::Outdated) {
        ImGui::SameLine();
        if (UIWidgets::Button(("Update##" + id).c_str(), InlineButton())) {
            actions.push_back([id] { ModStore_Install({ id }); });
        }
    }
    const bool disabled = state == StoreModState::Disabled || state == StoreModState::DisabledOnRestart;
    ImGui::SameLine();
    if (UIWidgets::Button(((disabled ? "Enable##" : "Disable##") + id).c_str(), InlineButton())) {
        if (disabled) {
            actions.push_back([id] { ModStore_Enable(id); });
        } else {
            ConfirmIfNeeded(view, mod, "Disable", actions, [id] { ModStore_Disable(id); });
        }
    }
    if (state != StoreModState::RemovedOnRestart) {
        ImGui::SameLine();
        if (UIWidgets::Button(("Remove##" + id).c_str(), InlineButton())) {
            ConfirmIfNeeded(view, mod, "Remove", actions, [id] { ModStore_Remove(id); });
        }
    }
    ImGui::EndDisabled();
}

void DrawInstalled(const StoreView& view, Actions& actions) {
    std::vector<std::string> outdated;
    for (const auto& [id, state] : view.states) {
        if (state == StoreModState::Outdated) {
            outdated.push_back(id);
        }
    }
    if (!outdated.empty()) {
        ImGui::BeginDisabled(view.busy);
        if (UIWidgets::Button(("Update all (" + std::to_string(outdated.size()) + ")").c_str(), InlineButton())) {
            actions.push_back([outdated] { ModStore_Install(outdated); });
        }
        ImGui::EndDisabled();
    }

    bool any = false;
    for (const auto& [id, state] : view.states) {
        if (state == StoreModState::NotInstalled || state == StoreModState::Unavailable) {
            continue;
        }
        any = true;
        DrawInstalledRow(view, view.catalog.mods.at(id), state, actions);
    }
    if (!any) {
        ImGui::TextDisabled("No mod from the store is installed yet.");
    }
}

void DrawLinks(const StoreView& view) {
    if (view.catalog.links.empty()) {
        ImGui::TextDisabled("No links yet.");
    }
    for (const StoreLink& link : view.catalog.links) {
        ImGui::TextUnformatted(link.name.c_str());
        if (!link.description.empty()) {
            ImGui::TextWrapped("%s", link.description.c_str());
        }
        if (UIWidgets::Button(("Open##" + link.url).c_str(), InlineButton().Tooltip(link.url))) {
            SDL_OpenURL(link.url.c_str());
        }
        ImGui::Separator();
    }
}

void DrawReleaseBanner(const StoreView& view, Actions& actions) {
    if (!view.newerRelease) {
        return;
    }
    const StoreRelease& release = *view.newerRelease;
    ImGui::TextColored(UIWidgets::ColorValues.at(UIWidgets::Colors::Yellow), "Unbound %s is available (you have %s).",
                       release.version.c_str(), gBuildForkVersion);

    int installed = 0;
    int ready = 0;
    for (const auto& [id, state] : view.states) {
        if (ModStore_IsInstalled(state)) {
            ++installed;
            ready += view.catalog.mods.at(id).builds.count(release.modApiMajor) != 0;
        }
    }
    if (installed > 0) {
        ImGui::Text("%d of %d installed mods are ready for it; the rest are disabled until their authors update them.",
                    ready, installed);
    }

    ImGui::BeginDisabled(view.busy);
    if (ModStore_CanInstallRelease(release)) {
        if (UIWidgets::Button("Update Unbound", InlineButton())) {
            actions.push_back(ModStore_InstallRelease);
        }
    } else if (UIWidgets::Button("Open the release page", InlineButton())) {
        SDL_OpenURL(release.pageUrl.c_str());
    }
    ImGui::SameLine();
    if (UIWidgets::Button("Skip this version", InlineButton())) {
        const std::string version = release.version;
        actions.push_back([version] { ModStore_SkipRelease(version); });
    }
    ImGui::EndDisabled();
    ImGui::Separator();
}

void DrawRestartBanner(const StoreView& view) {
    if (!view.restartNeeded) {
        return;
    }
    ImGui::TextColored(UIWidgets::ColorValues.at(UIWidgets::Colors::Cyan),
                       "Changes apply the next time Unbound starts.");
    ImGui::SameLine();
    if (UIWidgets::Button("Close Unbound now", InlineButton())) {
        Ship::Context::GetRawInstance()->GetWindow()->Close();
    }
    ImGui::Separator();
}

void DrawStartupChoice() {
    const int choice = CVarGetInteger(CVAR_STORE_CHECK_AT_STARTUP, StartupCheckUnasked);
    if (choice == StartupCheckUnasked) {
        ImGui::TextWrapped("Check for new versions of Unbound and of your mods every time the game starts?");
        if (UIWidgets::Button("Yes##StoreStartup", InlineButton())) {
            SetStartupCheck(StartupCheckOn);
        }
        ImGui::SameLine();
        if (UIWidgets::Button("No##StoreStartup", InlineButton())) {
            SetStartupCheck(StartupCheckOff);
        }
        ImGui::Separator();
        return;
    }
    bool enabled = choice == StartupCheckOn;
    if (UIWidgets::Checkbox("Check for updates at startup", &enabled,
                            UIWidgets::CheckboxOptions().Color(THEME_COLOR))) {
        SetStartupCheck(enabled ? StartupCheckOn : StartupCheckOff);
    }
}

void DrawToolbar(const StoreView& view, Actions& actions) {
    ImGui::BeginDisabled(view.busy);
    if (UIWidgets::Button("Refresh##Store", InlineButton())) {
        actions.push_back(ModStore_Refresh);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (view.busy) {
        ImGui::Text("%s", view.status.empty() ? "Working..." : view.status.c_str());
    } else {
        ImGui::TextDisabled("%s", view.status.c_str());
    }
}

} // namespace

void ModStore_DrawMenu() {
    if (!ModStore_HasHttps()) {
        ImGui::TextWrapped("This build of Unbound has no HTTPS client, so the store is not available.");
        return;
    }
    DrawStartupChoice();

    Actions actions;
    {
        auto lock = ModStore_LockView();
        const StoreView& view = ModStore_View();
        if (!view.hasCatalog && !view.busy && !sRequestedCatalog) {
            sRequestedCatalog = true;
            actions.push_back(ModStore_Refresh);
        }
        DrawToolbar(view, actions);
        DrawReleaseBanner(view, actions);
        DrawRestartBanner(view);
        if (view.hasCatalog && ImGui::BeginTabBar("##NeiStoreTabs")) {
            if (ImGui::BeginTabItem("Packs")) {
                DrawPacks(view, actions);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Installed")) {
                DrawInstalled(view, actions);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Links")) {
                DrawLinks(view);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    // Actions take the view lock themselves, so they run once it is released.
    for (const auto& action : actions) {
        action();
    }
}
