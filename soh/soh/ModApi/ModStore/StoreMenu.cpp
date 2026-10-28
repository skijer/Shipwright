#include "ModStore.h"
#include "StoreHttp.h"
#include "StoreInternal.h"
#include "StorePackCard.h"
#include "StorePackDefaults.h"

#include <algorithm>
#include <functional>
#include <cctype>
#include <nlohmann/json.hpp>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <set>
#include <sstream>

#include <SDL2/SDL_misc.h>
#include <ship/Context.h>
#include <fast/Fast3dGui.h>

#include "soh/cvar_prefixes.h"
#include "soh/SohGui/SohGui.hpp"
#include "soh/SohGui/UIWidgets.hpp"
#include "variables.h"
#include "soh/ModApi/ModApi.h"

namespace {

#define CVAR_STORE_CHECK_AT_STARTUP CVAR_SETTING("ModStore.CheckAtStartup")
constexpr int StartupCheckUnasked = 0;
constexpr int StartupCheckOn = 1;
constexpr int StartupCheckOff = 2;

using Actions = std::vector<std::function<void()>>;

std::map<std::string, std::set<std::string>> sUncheckedByPack;
bool sRequestedCatalog = false;
std::string sConfiguringPack;
ImGuiTextFilter sPackFilter;
bool sOptionsChanged = false;
// Icons use the game's existing texture cache; catalog metadata cannot open local paths.
ImTextureID PackIcon(const std::string& icon) {
    const auto gui = std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
    const std::string key = icon.rfind("ITEM_", 0) == 0 && gui->HasTextureByName(icon) ? icon : "ITEM_SONG_TIME";
    return gui->HasTextureByName(key) ? gui->GetTextureByName(key) : static_cast<ImTextureID>(0);
}

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
    std::vector<StorePack> packs;
    std::set<std::string> curatedIds;
    for (auto pack : ModStore_DefaultPacks()) {
        curatedIds.insert(pack.mods.begin(), pack.mods.end());
        pack.mods.erase(std::remove_if(pack.mods.begin(), pack.mods.end(), [&](const std::string& id) {
            return catalog.mods.count(id) == 0;
        }), pack.mods.end());
        if (!pack.mods.empty()) packs.push_back(std::move(pack));
    }
    // Older catalogs group these same mods differently. Preserve unrelated community packs.
    for (const auto& pack : catalog.packs) {
        if (std::none_of(pack.mods.begin(), pack.mods.end(), [&](const std::string& id) {
            return curatedIds.count(id) != 0 || id == "mm_assets";
        })) packs.push_back(pack);
    }
    std::set<std::string> packed = { "mm_assets" };
    for (const StorePack& pack : packs) {
        packed.insert(pack.mods.begin(), pack.mods.end());
    }
    std::map<std::string, StorePack> byAuthor;
    for (const auto& [id, mod] : catalog.mods) {
        if (packed.count(id) != 0 || mod.dependencyOnly)
            continue;
        const std::string author = !mod.owner.empty() ? mod.owner : !mod.author.empty() ? mod.author : "Unknown author";
        auto& group = byAuthor[author];
        if (group.id.empty()) {
            // Encode the author instead of using credits text as a CVar path.
            group.id = "other-";
            constexpr char hex[] = "0123456789abcdef";
            for (unsigned char c : author) {
                group.id += hex[c >> 4];
                group.id += hex[c & 15];
            }
            group.name = "Other mods - " + author;
            group.author = author;
            group.description = "Mods by this author outside a curated pack.";
            group.icon = "ITEM_BOMB";
        }
        group.mods.push_back(id);
    }
    for (auto& [author, group] : byAuthor) {
        packs.push_back(std::move(group));
    }
    return packs;
}

std::string PackSelectionKey(const std::string& id) {
    return CVAR_SETTING("ModStore.PackSelection.") + id;
}

std::set<std::string>& PackSelection(const std::string& id) {
    const auto [entry, inserted] = sUncheckedByPack.try_emplace(id);
    if (inserted) {
        std::istringstream saved(CVarGetString(PackSelectionKey(id).c_str(), ""));
        std::string mod;
        while (std::getline(saved, mod, ',')) {
            entry->second.insert(mod);
        }
    }
    return entry->second;
}

void SavePackSelection(const std::string& id, const std::set<std::string>& unchecked) {
    std::string saved;
    for (const auto& mod : unchecked) {
        saved += (saved.empty() ? "" : ",") + mod;
    }
    CVarSetString(PackSelectionKey(id).c_str(), saved.c_str());
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

// Installed manifests can expose options before a new remote catalog is published.
// Only read mounted package metadata; never change download URLs, hashes or trust decisions.
const std::vector<StoreOption>& InstalledOptions(const std::string& id) {
    static const auto optionsByMod = [] {
        std::map<std::string, std::vector<StoreOption>> result;
        auto archives = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->GetArchives();
        for (const auto& archive : *archives) {
            const auto packageId = std::filesystem::path(archive->GetPath()).stem().string();
            try {
                const auto file = archive->LoadFile("manifest.json");
                if (!file || !file->IsLoaded || !file->Buffer || file->Buffer->size() > 1024 * 1024) continue;
                const auto manifest = nlohmann::json::parse(file->Buffer->begin(), file->Buffer->end());
                const auto options = manifest.find("options");
                if (options == manifest.end() || !options->is_array() || options->size() > 64) continue;
                std::set<std::string> seen;
                for (const auto& option : *options) {
                    if (!option.is_object() || !option.contains("id") || !option["id"].is_string() ||
                        !option.contains("name") || !option["name"].is_string()) continue;
                    const auto key = option["id"].get<std::string>();
                    const auto name = option["name"].get<std::string>();
                    const bool valid = !key.empty() && key.size() <= 64 &&
                        std::all_of(key.begin(), key.end(), [](unsigned char c) {
                            return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
                        });
                    if (valid && !name.empty() && name.size() <= 160 && seen.insert(key).second)
                        result[packageId].push_back({ key, name });
                }
            } catch (const std::exception&) {
                // Malformed optional UI metadata must not prevent browsing the store.
            }
        }
        return result;
    }();
    static const std::vector<StoreOption> empty;
    const auto found = optionsByMod.find(id);
    return found == optionsByMod.end() ? empty : found->second;
}

void DrawModOptions(const StoreMod& mod) {
    const auto& installedOptions = InstalledOptions(mod.id);
    const auto& options = installedOptions.empty() ? mod.options : installedOptions;
    if (options.empty()) return;
    ImGui::PushID(mod.id.c_str());
    if (ImGui::TreeNodeEx("Items and features", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped("Enable individual features. Changes apply after restarting Unbound.");
        for (const auto& option : options) {
            ImGui::PushID(option.id.c_str());
            const std::string cvar = "gMods.Store." + mod.id + "." + option.id;
            bool enabled = CVarGetInteger(cvar.c_str(), 1) != 0;
            if (UIWidgets::Checkbox(option.name.c_str(), &enabled, UIWidgets::CheckboxOptions().Color(THEME_COLOR))) {
                CVarSetInteger(cvar.c_str(), enabled ? 1 : 0);
                Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
                sOptionsChanged = true;
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void DrawPackTile(const StoreView& view, const StorePack& pack, Actions& actions) {
    ImGui::PushID(pack.id.c_str());
    const std::string version = pack.version.empty() ? "Versions: per mod" : "Version " + pack.version;
    if (StorePackCard::Draw(pack.name, pack.author.empty() ? "Community" : pack.author, version, PackIcon(pack.icon),
                            sConfiguringPack == pack.id, ImGui::GetContentRegionAvail().x)) {
        sConfiguringPack = sConfiguringPack == pack.id ? "" : pack.id;
    }
    int installed = 0;
    bool compatible = true;
    for (const auto& id : pack.mods) {
        installed += ModStore_IsInstalled(StateOf(view, id));
        compatible &= view.catalog.mods.at(id).builds.count(SOH_MOD_API_MAJOR) != 0;
    }
    ImGui::BeginDisabled(view.busy || pack.mods.empty() || !compatible);
    if (UIWidgets::Button(
            "Install pack",
            InlineButton().Tooltip("Install every mod in this pack and its dependencies. Applies after restart."))) {
        const auto ids = pack.mods;
        actions.push_back([ids] { ModStore_Install(ids); });
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("%d/%zu installed", installed, pack.mods.size());
    ImGui::PopID();
}

void DrawPackDetails(const StoreView& view, const StorePack& pack, Actions& actions) {
    ImGui::PushID(pack.id.c_str());
    ImGui::Spacing();
    ImGui::SeparatorText(pack.name.c_str());
    ImGui::TextWrapped("%s", pack.description.empty() ? "No description provided." : pack.description.c_str());
    if (ImGui::SmallButton("Collapse details")) {
        sConfiguringPack.clear();
    }
    auto& unchecked = PackSelection(pack.id);
    ImGui::Indent();
    ImGui::TextWrapped("Choose which mods to install. Required dependencies are included automatically. This does "
                       "not remove installed mods.");
    if (ImGui::SmallButton("Select all")) {
        unchecked.clear();
        SavePackSelection(pack.id, unchecked);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Select none")) {
        unchecked = std::set<std::string>(pack.mods.begin(), pack.mods.end());
        SavePackSelection(pack.id, unchecked);
    }
    std::vector<std::string> selected;
    for (const auto& id : pack.mods) {
        const auto& mod = view.catalog.mods.at(id);
        const bool available = mod.builds.count(SOH_MOD_API_MAJOR) != 0;
        bool checked = available && unchecked.count(id) == 0;
        ImGui::PushID(id.c_str());
        ImGui::BeginDisabled(!available);
        if (UIWidgets::Checkbox("##selected", &checked, UIWidgets::CheckboxOptions().Color(THEME_COLOR))) {
            if (checked) {
                unchecked.erase(id);
            } else {
                unchecked.insert(id);
            }
            SavePackSelection(pack.id, unchecked);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextUnformatted(ModStore_DisplayName(mod).c_str());
        DrawModTooltip(mod);
        DrawModLabel(view, mod);
        DrawModOptions(mod);
        ImGui::PopID();
        if (checked)
            selected.push_back(id);
    }
    ImGui::TextDisabled("%zu selected", selected.size());
    ImGui::BeginDisabled(view.busy || selected.empty());
    if (UIWidgets::Button("Install selection", InlineButton())) {
        actions.push_back([selected] { ModStore_Install(selected); });
    }
    ImGui::EndDisabled();
    ImGui::Unindent();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::PopID();
}

void DrawPacks(const StoreView& view, Actions& actions) {
    sPackFilter.Draw("Search packs", 240.0f);
    const auto packs = PacksWithOtherMods(view.catalog);
    std::vector<const StorePack*> visible;
    for (const auto& pack : packs) {
        const std::string searchable = pack.name + " " + pack.author + " " + pack.description;
        if (sPackFilter.PassFilter(searchable.c_str())) {
            visible.push_back(&pack);
        }
    }
    if (visible.empty()) {
        ImGui::TextDisabled("No packs found.");
        return;
    }
    constexpr size_t Columns = 3;
    // Keep three readable columns on narrow menus; the child supplies horizontal scrolling.
    const float contentWidth = std::max(ImGui::GetContentRegionAvail().x, ImGui::GetFontSize() * 60.0f);
    ImGui::SetNextWindowContentSize(ImVec2(contentWidth, 0));
    if (ImGui::BeginChild("##PackGridScroll", ImVec2(0, 0), ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        for (size_t row = 0; row < visible.size(); row += Columns) {
            ImGui::PushID(static_cast<int>(row));
            if (ImGui::BeginTable("##PackGrid", Columns,
                                  ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX,
                                  ImVec2(contentWidth, 0))) {
                for (size_t column = 0; column < Columns && row + column < visible.size(); ++column) {
                    ImGui::TableNextColumn();
                    DrawPackTile(view, *visible[row + column], actions);
                }
                ImGui::EndTable();
            }
            for (size_t column = 0; column < Columns && row + column < visible.size(); ++column) {
                const auto& pack = *visible[row + column];
                if (sConfiguringPack == pack.id) {
                    DrawPackDetails(view, pack, actions);
                    break;
                }
            }
            ImGui::Spacing();
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

void DrawInstalledRow(const StoreView& view, const StoreMod& mod, StoreModState state, Actions& actions) {
    ImGui::TextUnformatted(ModStore_DisplayName(mod).c_str());
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
    DrawModOptions(mod);
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
    if (!view.restartNeeded && !sOptionsChanged) {
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
