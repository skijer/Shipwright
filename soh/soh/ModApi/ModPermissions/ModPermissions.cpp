#include "ModPermissions.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <unordered_set>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <SDL2/SDL_messagebox.h>

#include "soh/ModApi/ModApi.h"
#include "soh/SohGui/SohGui.hpp"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

using Json = nlohmann::json;

struct PermissionName {
    ModPermission permission;
    const char* key;
};

constexpr PermissionName kPermissionNames[] = {
    { ModPermission::Microphone, "microphone" },
    { ModPermission::Files, "files" },
};

enum class ConfirmChoice : int {
    Refuse,
    Allow,
};

std::vector<std::unique_ptr<ModIdentity>> sMods;
std::unordered_set<std::string> sQuestionsAsked;

const char* GetPermissionKey(ModPermission permission) {
    for (const PermissionName& name : kPermissionNames) {
        if (name.permission == permission) {
            return name.key;
        }
    }
    return "unknown";
}

std::filesystem::path GetDecisionsPath() {
    return ModLoader_GetModsDirectory() / ".unbound-mod-permissions.json";
}

Json& GetDecisions() {
    static Json decisions = [] {
        std::ifstream input(GetDecisionsPath());
        Json loaded = input ? Json::parse(input, nullptr, false) : Json::object();
        return loaded.is_object() ? loaded : Json::object();
    }();
    return decisions;
}

void RecordDecision(const std::string& trustKey, const char* key, bool granted) {
    GetDecisions()[trustKey][key] = granted;
    std::ofstream output(GetDecisionsPath(), std::ios::trunc);
    output << GetDecisions().dump(2) << '\n';
    SPDLOG_INFO("[ModApi] {} {} the '{}' permission", trustKey, granted ? "was granted" : "was refused", key);
}

const void* FindModuleBase(const void* address) {
#if defined(_WIN32)
    HMODULE module = nullptr;
    const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    if (!GetModuleHandleExW(flags, static_cast<LPCWSTR>(address), &module)) {
        return nullptr;
    }
    return module;
#else
    Dl_info info;
    return dladdr(address, &info) != 0 ? info.dli_fbase : nullptr;
#endif
}

std::string DescribeMod(const ModIdentity& mod) {
    return "'" + mod.name + "' (" + mod.trustLabel + ")";
}

} // namespace

std::vector<ModPermission> ModPermissions_Parse(const std::vector<std::string>& names, const std::string& modName) {
    std::vector<ModPermission> permissions;
    for (const std::string& name : names) {
        auto known = std::find_if(std::begin(kPermissionNames), std::end(kPermissionNames),
                                  [&](const PermissionName& entry) { return name == entry.key; });
        if (known == std::end(kPermissionNames)) {
            SPDLOG_WARN("[ModApi] Mod '{}' declares the unknown permission '{}'", modName, name);
            continue;
        }
        permissions.push_back(known->permission);
    }
    return permissions;
}

void ModPermissions_RegisterMod(ModIdentity identity) {
    sMods.push_back(std::make_unique<ModIdentity>(std::move(identity)));
}

const ModIdentity* ModPermissions_FindCaller(const void* address) {
    const void* base = FindModuleBase(address);
    if (base == nullptr) {
        return nullptr;
    }
    for (const auto& mod : sMods) {
        if (mod->moduleBase == base) {
            return mod.get();
        }
    }
    return nullptr;
}

bool ModPermissions_HasDeclared(const ModIdentity& mod, ModPermission permission) {
    if (std::find(mod.declared.begin(), mod.declared.end(), permission) != mod.declared.end()) {
        return true;
    }
    SPDLOG_ERROR("[ModApi] Mod '{}' did not declare the '{}' permission in its manifest", mod.name,
                 GetPermissionKey(permission));
    return false;
}

// Answers from the saved decisions; otherwise queues one in-game question and reports Pending until it is answered.
PermissionAnswer ModPermissions_AskOnce(const ModIdentity& mod, ModPermission permission, const std::string& purpose) {
    const char* key = GetPermissionKey(permission);
    const Json& decisions = GetDecisions();
    const auto saved = decisions.find(mod.trustKey);
    if (saved != decisions.end() && saved->is_object()) {
        const auto answer = saved->find(key);
        if (answer != saved->end() && answer->is_boolean()) {
            return answer->get<bool>() ? PermissionAnswer::Granted : PermissionAnswer::Denied;
        }
    }

    const std::string question = mod.trustKey + "/" + key;
    if (!sQuestionsAsked.insert(question).second) {
        return PermissionAnswer::Pending;
    }
    const std::string trustKey = mod.trustKey;
    SohGui::RegisterPopup(
        "Mod permission##" + question, DescribeMod(mod) + " " + purpose, "Allow", "Don't allow",
        [trustKey, key]() { RecordDecision(trustKey, key, true); },
        [trustKey, key]() { RecordDecision(trustKey, key, false); });
    return PermissionAnswer::Pending;
}

bool ModPermissions_ConfirmNow(const ModIdentity& mod, const std::string& title, const std::string& message) {
    const std::string fullTitle = mod.name + ": " + title;
    const std::string fullMessage = DescribeMod(mod) + " " + message;
    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, static_cast<int>(ConfirmChoice::Refuse), "Don't allow" },
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, static_cast<int>(ConfirmChoice::Allow), "Allow" },
    };
    const SDL_MessageBoxData box = {
        SDL_MESSAGEBOX_WARNING, nullptr, fullTitle.c_str(), fullMessage.c_str(),
        SDL_arraysize(buttons), buttons, nullptr,
    };
    int choice = static_cast<int>(ConfirmChoice::Refuse);
    const bool allowed = SDL_ShowMessageBox(&box, &choice) == 0 && choice == static_cast<int>(ConfirmChoice::Allow);
    SPDLOG_INFO("[ModApi] Player {} '{}': {}", allowed ? "allowed" : "refused", mod.name, title);
    return allowed;
}
