#include "FleetGame.h"
#include "FleetRando.h"

#include <cassert>
#include <cstddef>
#include <filesystem>

#include <fleet/FleetGameApi.h>
#include <ship/Context.h>
#include <ship/GameRegistry.h>
#include <ship/config/Config.h>
#include <ship/config/ConsoleVariable.h>
#include <ship/resource/ResourceManager.h>
#include <spdlog/spdlog.h>

extern "C" int Soh_RunMain(int argc, char* argv[]);

namespace {

constexpr const char* GameName = "oot";
constexpr const char* SettingsFileName = "shipofharkinian.json";
constexpr uint32_t ResourceThreadReserve = 3;

const FleetHostApi* sHost = nullptr;
std::string sInstallDirectory;
bool sHasRun = false;

bool Attach(const FleetAttachInfo* info) {
    if (info == nullptr || info->structSize < sizeof(FleetAttachInfo) || info->host == nullptr ||
        info->installDirectory == nullptr) {
        return false;
    }
    const std::string problem = Ship_InstallProblem(info->installDirectory);
    if (!problem.empty()) {
        info->host->Log(GameName, FLEET_LOG_ERROR, problem.c_str());
        return false;
    }
    sHost = info->host;
    sInstallDirectory = info->installDirectory;
    return true;
}

void Run(const FleetDestination* arrival) {
    if (sHasRun) {
        SPDLOG_ERROR("[Fleet] Ship cannot resume after its loop ended yet");
        return;
    }
    sHasRun = true;
    char program[] = "soh";
    char* argv[] = { program, nullptr };
    Soh_RunMain(1, argv);
}

void Detach() {
    sHost = nullptr;
}

const FleetGameApi sGameApi = {
    .structSize = sizeof(FleetGameApi),
    .major = FLEET_GAME_API_MAJOR,
    .minor = FLEET_GAME_API_MINOR,
    .name = GameName,
    .Attach = Attach,
    .Run = Run,
    .Detach = Detach,
    .DumpRandoStaticData = FleetRando_DumpStaticData,
    .RandoReset = FleetRando_Reset,
    .RandoSetOwnedItems = FleetRando_SetOwnedItems,
    .RandoGetReachableChecks = FleetRando_GetReachableChecks,
    .RandoPlaceItem = FleetRando_PlaceItem,
    .FreeString = FleetRando_FreeString,
    .RandoRequest = FleetRando_Request,
};

} // namespace

extern "C" const FleetGameApi* Fleet_GetGameApi(uint16_t major) {
    return major == FLEET_GAME_API_MAJOR ? &sGameApi : nullptr;
}

extern "C" bool FleetGame_IsHosted(void) {
    return sHost != nullptr;
}

extern "C" void FleetGame_JoinHostContext(void) {
    Ship::Context* context = Ship::Context::GetRawInstance();
    // Assumes Fleet created the Context, its settings and its console variables before Run.
    assert(context != nullptr && context->GetHostConfig() != nullptr);
    std::shared_ptr<Ship::ConsoleVariable> hostConsoleVariables = context->GetConsoleVariables();

    std::shared_ptr<Ship::GameServices> game = Ship::GameRegistry::Register(GameName);
    game->AppDirectory = sInstallDirectory;
    game->Settings =
        std::make_shared<Ship::Config>((std::filesystem::path(sInstallDirectory) / SettingsFileName).string());
    game->ConsoleVariables = std::make_shared<Ship::ConsoleVariable>(game->Settings);
    game->ConsoleVariables->DelegateToHost(hostConsoleVariables, Ship::ConsoleVariable::GetEngineKeys());
    Ship::GameRegistry::SetActive(GameName);
}

std::shared_ptr<Ship::ResourceManager> FleetGame_CreateResources(const std::string& archivePath) {
    std::shared_ptr<Ship::GameServices> game = Ship::GameRegistry::Get(GameName);
    assert(game != nullptr);
    // Assigned before Init so the manager's own workers already run as this game.
    game->Resources = std::make_shared<Ship::ResourceManager>();
    game->Resources->Init({ archivePath }, {}, ResourceThreadReserve);
    return game->Resources;
}

extern "C" void FleetGame_LeaveHostContext(void) {
    Ship::GameRegistry::Unregister(GameName);
}

extern "C" void FleetGame_FrameEnd(void) {
    const bool offered =
        sHost != nullptr && sHost->structSize >= offsetof(FleetHostApi, FrameEnd) + sizeof(sHost->FrameEnd);
    if (offered && sHost->FrameEnd != nullptr) {
        sHost->FrameEnd(GameName);
    }
}
