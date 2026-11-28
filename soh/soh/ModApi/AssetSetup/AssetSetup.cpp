#include "AssetSetup.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <spdlog/spdlog.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"

namespace {

constexpr const char* kSetupFlag = "--fleet-asset-setup";
// Written in the working directory, the game folder Fleet starts the run in; the log may not exist yet.
constexpr const char* kResultFileName = "fleet-asset-setup.txt";
constexpr const char* kOwnGameRomFlag = "--oot-rom";
constexpr const char* kOtherGameRomFlag = "--mm-rom";
// A game set to boot into file select never starts a scene.
constexpr int kMaxFramesWithoutScene = 600;

bool sIsRunning = false;
bool sIsRestartNeeded = false;
bool sHasSceneStarted = false;
int sFramesWaited = 0;
std::string sOwnGameRom;
std::string sOtherGameRom;

bool TakeFlagValue(const char* flag, int& index, int argc, char* argv[], std::string& value) {
    if (std::strcmp(argv[index], flag) != 0 || index + 1 >= argc) {
        return false;
    }
    index++;
    value = argv[index];
    return true;
}

void ExitWhenSettled() {
    sFramesWaited++;
    if (sIsRestartNeeded) {
        AssetSetup_Exit(AssetSetupExit::RunAgain, "a mod saved archives that load at the next start");
    }
    if (sHasSceneStarted || sFramesWaited >= kMaxFramesWithoutScene) {
        AssetSetup_Exit(AssetSetupExit::Done, sHasSceneStarted ? "the first scene started" : "no scene started");
    }
}

} // namespace

void AssetSetup_TakeArguments(int& argc, char* argv[]) {
    int kept = 1;
    for (int index = 1; index < argc; index++) {
        if (std::strcmp(argv[index], kSetupFlag) == 0) {
            sIsRunning = true;
            continue;
        }
        if (TakeFlagValue(kOwnGameRomFlag, index, argc, argv, sOwnGameRom) ||
            TakeFlagValue(kOtherGameRomFlag, index, argc, argv, sOtherGameRom)) {
            continue;
        }
        argv[kept] = argv[index];
        kept++;
    }
    argc = kept;
    argv[argc] = nullptr;
}

bool AssetSetup_IsRunning() {
    return sIsRunning;
}

const std::string& AssetSetup_GetOwnGameRom() {
    return sOwnGameRom;
}

const std::string& AssetSetup_GetOtherGameRom() {
    return sOtherGameRom;
}

void AssetSetup_NoteRestart(const std::string& modName) {
    SPDLOG_INFO("[AssetSetup] '{}' needs another start", modName);
    sIsRestartNeeded = true;
}

void AssetSetup_Exit(AssetSetupExit code, const std::string& reason) {
    SPDLOG_INFO("[AssetSetup] Finished with code {}: {}", static_cast<int>(code), reason);
    spdlog::shutdown();
    std::ofstream(kResultFileName) << reason << '\n';
    // The archives are on disk and the game threads still run, so no destructor may run. On Windows even _Exit
    // unloads the DLLs, whose static destructors meet a live std::thread and abort with 0xC0000409.
#ifdef _WIN32
    TerminateProcess(GetCurrentProcess(), static_cast<UINT>(code));
#endif
    std::_Exit(static_cast<int>(code));
}

void AssetSetup_ExitAfterFirstScene() {
    if (!sIsRunning) {
        return;
    }
    SPDLOG_INFO("[AssetSetup] Fleet asset setup with OoT ROM '{}' and MM ROM '{}'", sOwnGameRom, sOtherGameRom);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(
        [](int16_t sceneNum) { sHasSceneStarted = true; });
    // The mods' scene hooks may run after the one above, so the run ends a frame later.
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(ExitWhenSettled);
}
