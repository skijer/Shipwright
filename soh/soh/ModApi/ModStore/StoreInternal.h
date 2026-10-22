#pragma once

#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "StoreCatalog.h"

enum class StoreModState {
    NotInstalled,
    Installed,
    Outdated,
    Disabled,
    InstallsOnRestart,
    DisabledOnRestart,
    RemovedOnRestart,
    Unavailable,
};

struct StoreView {
    bool busy = false;
    std::string status;
    bool hasCatalog = false;
    StoreCatalog catalog;
    std::map<std::string, StoreModState> states;
    std::optional<StoreRelease> newerRelease;
    bool restartNeeded = false;
};

std::unique_lock<std::mutex> ModStore_LockView();
// Read or write only while holding ModStore_LockView().
StoreView& ModStore_View();

void ModStore_Refresh();
void ModStore_Install(const std::vector<std::string>& ids);
void ModStore_Disable(const std::string& id);
void ModStore_Enable(const std::string& id);
void ModStore_Remove(const std::string& id);
void ModStore_SkipRelease(const std::string& version);
bool ModStore_CanInstallRelease(const StoreRelease& release);
void ModStore_InstallRelease();

bool ModStore_IsInstalled(StoreModState state);
std::vector<std::string> ModStore_FindDependents(const StoreView& view, const std::string& id);
std::filesystem::path ModStore_ModsDirectory();

// Worker thread only. Puts the new exe in place and leaves the rest as .update files for the next launch.
bool ModStore_StageRelease(const StoreRelease& release, std::string& error);
