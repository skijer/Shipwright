#ifndef FLEET_GAME_H
#define FLEET_GAME_H

#include <stdbool.h>

#include <fleet/FleetGameApi.h>

#ifdef __cplusplus
#include <memory>
#include <string>

namespace Ship {
class ResourceManager;
}

std::shared_ptr<Ship::ResourceManager> FleetGame_CreateResources(const std::string& archivePath);
std::string Ship_InstallProblem(const std::string& directory);
std::string FleetGame_NetworkRoom(const std::string& room);

class FleetAutomaticSave {
  public:
    FleetAutomaticSave();
    ~FleetAutomaticSave();
    FleetAutomaticSave(const FleetAutomaticSave&) = delete;
    FleetAutomaticSave& operator=(const FleetAutomaticSave&) = delete;
};

extern "C" {
#endif

bool FleetGame_IsHosted(void);
void* FleetGame_GetHostFunction(const char* name);
const char* FleetGame_GetSaveDirectory(void);
const char* FleetGame_GetModsDirectory(void);
bool FleetGame_FileSelect(void);
bool FleetGame_SelectFile(int slot);
bool FleetGame_ConfigureRandomizer(int slot);
bool FleetGame_GenerateRandomizer(int slot);
int FleetGame_RandomizerGenerationState(void);
bool FleetGame_HasGeneratedSeed(int slot);
bool FleetGame_CreateNamedSave(int slot, const unsigned char* name, int language);
bool FleetGame_HasSaveSlot(int slot);
bool FleetGame_ManageSave(const char* operation, int from, int to);
const char* FleetGame_GetSaveFilePath(int slot, bool backup);
void FleetGame_JoinHostContext(void);
void FleetGame_LeaveHostContext(void);
void FleetGame_FrameEnd(void);
void FleetGame_RequestSwitch(const FleetDestination* destination);
void FleetGame_Log(FleetLogLevel level, const char* message);
bool FleetGame_ResetActiveGame(void);
void FleetGame_CommitSave(void);
bool FleetGame_ShouldRollCredits(void);
void Soh_ResetToTitle(void);

#ifdef __cplusplus
}
#endif

#endif
