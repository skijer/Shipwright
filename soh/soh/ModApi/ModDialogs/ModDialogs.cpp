#include "ModDialogs.h"

#include <cstdio>
#include <string>

#include <libultraship/libultraship.h>
#include <spdlog/spdlog.h>
#include <SDL2/SDL_messagebox.h>

#include "soh/ModApi/ModPermissions/ModPermissions.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <climits>
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace {

// Set when the game closes for a mod's restart: main() starts it again once everything is shut down.
bool sRelaunch = false;

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

// The game closes as usual, and main() starts it again with no helper process: on macOS and Linux the process
// replaces itself, on Windows it starts soh.exe as a normal launch does (ModRestart_RelaunchIfRequested).
void CloseToRestart(const ModIdentity* mod, const char* reason) {
    SPDLOG_INFO("[ModApi] Restarting for '{}'{}{}", mod->name, reason != nullptr ? ": " : "",
                reason != nullptr ? reason : "");
    sRelaunch = true;
    Ship::Context::GetRawInstance()->GetWindow()->Close();
}

bool RequestRestartFor(const ModIdentity* mod, const char* reason) {
    if (mod == nullptr) {
        return false;
    }
    const std::string message = "'" + mod->name + "' (" + mod->trustLabel + ") needs Unbound to restart" +
                                (reason != nullptr ? std::string(": ") + reason : std::string(".")) +
                                "\n\nRestart Unbound now?";
    if (!AskFor(mod, "Restart", message, "Restart now", "Later")) {
        return false;
    }
    CloseToRestart(mod, reason);
    return true;
}

// No dialog: for a mod that has just told the player it restarts (after its own question, say). The log names it.
bool RestartFor(const ModIdentity* mod, const char* reason) {
    if (mod == nullptr) {
        return false;
    }
    CloseToRestart(mod, reason);
    return true;
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

extern "C" bool ModDialogs_Restart(const char* reason) {
    return RestartFor(ModPermissions_FindCaller(MOD_CALLER_ADDRESS()), reason);
}

// Windows starts the new soh.exe while the old one is still exiting, and a mounted archive can't be replaced until it
// has: the new one waits for it here, before the mods folder's pending files are installed.
extern "C" void ModRestart_WaitForPrevious(void) {
#ifdef _WIN32
    wchar_t previous[32];
    if (GetEnvironmentVariableW(L"SOH_RESTART_AFTER", previous, ARRAYSIZE(previous)) == 0) {
        return;
    }
    SetEnvironmentVariableW(L"SOH_RESTART_AFTER", nullptr);
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(wcstoul(previous, nullptr, 10)));
    if (process != nullptr) {
        WaitForSingleObject(process, 15000);
        CloseHandle(process);
    }
#endif
}

// After the game has shut down: starts it again if a mod asked to restart. Failures go to stderr.
extern "C" void ModRestart_RelaunchIfRequested(int argc, char* argv[]) {
    if (!sRelaunch) {
        return;
    }
#ifdef _WIN32
    (void)argc;
    (void)argv;
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0 || GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        return;
    }
    std::wstring commandLine = GetCommandLineW();
    SetEnvironmentVariableW(L"SOH_RESTART_AFTER", std::to_wstring(GetCurrentProcessId()).c_str());
    STARTUPINFOW startup = { sizeof(startup) };
    PROCESS_INFORMATION started = {};
    if (CreateProcessW(path, commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &started)) {
        CloseHandle(started.hThread);
        CloseHandle(started.hProcess);
    }
#else
    char path[PATH_MAX];
#ifdef __APPLE__
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) {
        return;
    }
#else
    const ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (length <= 0) {
        return;
    }
    path[length] = '\0';
#endif
    (void)argc;
    execv(path, argv);
    fprintf(stderr, "Could not restart %s\n", path);
#endif
}
