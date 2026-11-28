#pragma once

#include <string>

// Fleet runs the game once with the player's ROMs to build every archive; choosing them in Fleet is the consent,
// so the run asks nothing. Fleet reads the exit code: keep it in step with Fleet's AssetSetup.cpp.
enum class AssetSetupExit : int {
    Done = 0,
    Failed = 1,
    RunAgain = 3,
};

// Removes the setup flags, so the extractor does not read them as ROM paths.
void AssetSetup_TakeArguments(int& argc, char* argv[]);
bool AssetSetup_IsRunning();
const std::string& AssetSetup_GetOwnGameRom();
const std::string& AssetSetup_GetOtherGameRom();
void AssetSetup_NoteRestart(const std::string& modName);
[[noreturn]] void AssetSetup_Exit(AssetSetupExit code, const std::string& reason);
// Assumes GameInteractor::Instance exists.
void AssetSetup_ExitAfterFirstScene();
