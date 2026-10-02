#include "ModDialogs.h"

#include <filesystem>
#include <string>

#include <libultraship/libultraship.h>
#include <spdlog/spdlog.h>
#include <SDL2/SDL_messagebox.h>

#include "soh/ModApi/ModPermissions/ModPermissions.h"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

enum class DialogChoice : int {
    No,
    Yes,
};

// Every dialog is titled with the mod's name, so a mod cannot pass its questions off as the game's.
bool AskFor(const ModIdentity* mod, const char* title, const std::string& message, const char* yes, const char* no) {
    if (mod == nullptr) {
        return false;
    }
    const std::string fullTitle = mod->name + ": " + (title != nullptr ? title : "");
    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, static_cast<int>(DialogChoice::No), no },
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, static_cast<int>(DialogChoice::Yes), yes },
    };
    const SDL_MessageBoxData box = {
        SDL_MESSAGEBOX_INFORMATION, nullptr, fullTitle.c_str(), message.c_str(),
        SDL_arraysize(buttons),     buttons, nullptr,
    };
    int choice = static_cast<int>(DialogChoice::No);
    return SDL_ShowMessageBox(&box, &choice) == 0 && choice == static_cast<int>(DialogChoice::Yes);
}

void TellFor(const ModIdentity* mod, const char* title, const std::string& message) {
    if (mod == nullptr) {
        return;
    }
    const std::string fullTitle = mod->name + ": " + (title != nullptr ? title : "");
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, fullTitle.c_str(), message.c_str(), nullptr);
}

#if defined(_WIN32)
std::wstring GetOriginalArguments() {
    const wchar_t* line = GetCommandLineW();
    if (*line == L'"') {
        line = wcschr(line + 1, L'"');
        return line == nullptr ? L"" : line + 1;
    }
    line = wcschr(line, L' ');
    return line == nullptr ? L"" : line;
}

// The new process starts once this one has had time to close, so it finds the mods folder free.
bool ScheduleRelaunch() {
    wchar_t executable[MAX_PATH];
    if (GetModuleFileNameW(nullptr, executable, MAX_PATH) == 0) {
        return false;
    }
    std::wstring command = L"cmd.exe /c ping -n 6 127.0.0.1 >nul & start \"\" \"" + std::wstring(executable) + L"\"" +
                           GetOriginalArguments();
    const std::wstring directory = std::filesystem::path(executable).parent_path().wstring();
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr,
                        directory.c_str(), &startup, &process)) {
        return false;
    }
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    return true;
}
#endif

bool RequestRestartFor(const ModIdentity* mod, const char* reason) {
    if (mod == nullptr) {
        return false;
    }
    const std::string message = "'" + mod->name + "' (" + mod->trustLabel + ") needs Unbound to restart" +
                                (reason != nullptr ? std::string(": ") + reason : std::string(".")) +
                                "\n\nRestart now?";
    if (!AskFor(mod, "Restart", message, "Restart now", "Later")) {
        return false;
    }
#if defined(_WIN32)
    if (ScheduleRelaunch()) {
        SPDLOG_INFO("[ModApi] Restarting for '{}'", mod->name);
        Ship::Context::GetInstance()->GetWindow()->Close();
        return true;
    }
#endif
    TellFor(mod, "Restart", "Close Unbound and open it again to finish.");
    return false;
}

} // namespace

extern "C" bool ModDialogs_Ask(const char* title, const char* message) {
    return AskFor(ModPermissions_FindCaller(MOD_CALLER_ADDRESS()), title, message != nullptr ? message : "", "Yes",
                  "No");
}

extern "C" void ModDialogs_Tell(const char* title, const char* message) {
    TellFor(ModPermissions_FindCaller(MOD_CALLER_ADDRESS()), title, message != nullptr ? message : "");
}

extern "C" bool ModDialogs_RequestRestart(const char* reason) {
    return RequestRestartFor(ModPermissions_FindCaller(MOD_CALLER_ADDRESS()), reason);
}
