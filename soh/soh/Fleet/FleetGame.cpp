#include "FleetGame.h"
#include "FleetPortal.h"
#include "FleetRando.h"

#include <cassert>
#include <filesystem>
#include <string_view>
#include <unordered_map>

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

FleetGetFunctionFn sHostFunction = nullptr;
FleetLogFn sLog = nullptr;
FleetRequestSwitchFn sRequestSwitch = nullptr;
FleetFrameEndFn sFrameEnd = nullptr;
std::string sInstallDirectory;
bool sHasRun = false;

template <class Function> Function HostFunction(const char* name) {
    return reinterpret_cast<Function>(sHostFunction(name));
}

const char* Name() {
    return GameName;
}

bool Attach(const FleetAttachInfo* info) {
    if (info == nullptr || info->hostFunction == nullptr || info->installDirectory == nullptr) {
        return false;
    }
    sHostFunction = info->hostFunction;
    sLog = HostFunction<FleetLogFn>("Log");
    const std::string problem = Ship_InstallProblem(info->installDirectory);
    if (!problem.empty()) {
        FleetGame_Log(FLEET_LOG_ERROR, problem.c_str());
        sHostFunction = nullptr;
        return false;
    }
    sRequestSwitch = HostFunction<FleetRequestSwitchFn>("RequestSwitch");
    sFrameEnd = HostFunction<FleetFrameEndFn>("FrameEnd");
    sInstallDirectory = info->installDirectory;
    return true;
}

void Run(const FleetDestination* arrival) {
    if (sHasRun) {
        SPDLOG_ERROR("[Fleet] Ship cannot run its loop twice");
        return;
    }
    sHasRun = true;
    FleetPortal_Arrive(arrival);
    char program[] = "soh";
    char* argv[] = { program, nullptr };
    Soh_RunMain(1, argv);
}

void Detach() {
    sHostFunction = nullptr;
}

const std::unordered_map<std::string_view, void*>& Functions() {
    static const std::unordered_map<std::string_view, void*> functions = {
        { "Name", reinterpret_cast<void*>(Name) },
        { "Attach", reinterpret_cast<void*>(Attach) },
        { "Run", reinterpret_cast<void*>(Run) },
        { "Arrive", reinterpret_cast<void*>(FleetPortal_Arrive) },
        { "Detach", reinterpret_cast<void*>(Detach) },
        { "FreeString", reinterpret_cast<void*>(FleetRando_FreeString) },
        { "DumpRandoStaticData", reinterpret_cast<void*>(FleetRando_DumpStaticData) },
        { "RandoReset", reinterpret_cast<void*>(FleetRando_Reset) },
        { "RandoSetOwnedItems", reinterpret_cast<void*>(FleetRando_SetOwnedItems) },
        { "RandoGetReachableChecks", reinterpret_cast<void*>(FleetRando_GetReachableChecks) },
        { "RandoPlaceItem", reinterpret_cast<void*>(FleetRando_PlaceItem) },
        { "RandoRequest", reinterpret_cast<void*>(FleetRando_Request) },
    };
    return functions;
}

} // namespace

extern "C" void* Fleet_GetFunction(const char* name) {
    auto function = Functions().find(name != nullptr ? name : "");
    return function == Functions().end() ? nullptr : function->second;
}

extern "C" bool FleetGame_IsHosted(void) {
    return sHostFunction != nullptr;
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
    FleetPortal_FrameEnd();
    if (sFrameEnd != nullptr) {
        sFrameEnd(GameName);
    }
}

extern "C" void FleetGame_RequestSwitch(const FleetDestination* destination) {
    if (sRequestSwitch != nullptr) {
        sRequestSwitch(GameName, destination);
    }
}

extern "C" void FleetGame_Log(FleetLogLevel level, const char* message) {
    if (sLog != nullptr) {
        sLog(GameName, level, message);
    }
}
