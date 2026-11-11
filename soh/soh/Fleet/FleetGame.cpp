#include "FleetGame.h"
#include "FleetPortal.h"
#include "FleetRuntime.h"
#include "FleetRando.h"
#include "FleetItemDraw.h"
#include "soh/Enhancements/controls/SohInputEditorWindow.h"
#include "soh/cvar_prefixes.h"
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>

#include <atomic>
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
        { "DrawItem", reinterpret_cast<void*>(FleetItemDraw_Draw) },
        { "ItemIcon", reinterpret_cast<void*>(FleetItemDraw_Icon) },
        { "Reset", reinterpret_cast<void*>(Soh_ResetToTitle) },
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

std::string FleetGame_NetworkRoom(const std::string& room) {
    return FleetGame_IsHosted() && !room.empty() ? room + "@" + GameName : room;
}

extern "C" void* FleetGame_GetHostFunction(const char* name) {
    return sHostFunction != nullptr ? sHostFunction(name) : nullptr;
}

extern "C" const char* FleetGame_GetSaveDirectory(void) {
    using GetDirectory = const char* (*)(const char*);
    const auto getDirectory = reinterpret_cast<GetDirectory>(FleetGame_GetHostFunction("GetSaveDirectory"));
    return getDirectory != nullptr ? getDirectory(GameName) : nullptr;
}

extern "C" bool FleetGame_FileSelect(void) {
    const auto show = reinterpret_cast<bool (*)()>(FleetGame_GetHostFunction("FileSelect"));
    return show != nullptr && show();
}

extern "C" bool FleetGame_SelectFile(int slot) {
    const auto select = reinterpret_cast<bool (*)(int)>(FleetGame_GetHostFunction("SelectFile"));
    return select != nullptr && select(slot);
}

extern "C" bool FleetGame_ConfigureRandomizer(int slot) {
    const auto configure = reinterpret_cast<bool (*)(int)>(FleetGame_GetHostFunction("ConfigureRandomizer"));
    return configure != nullptr && configure(slot);
}

extern "C" bool FleetGame_HasGeneratedSeed(int slot) {
    const auto has = reinterpret_cast<bool (*)(int)>(FleetGame_GetHostFunction("HasGeneratedSeed"));
    return has != nullptr && has(slot);
}

extern "C" bool FleetGame_GenerateRandomizer(int slot) {
    const auto generate = reinterpret_cast<bool (*)(int)>(FleetGame_GetHostFunction("GenerateRandomizer"));
    return generate != nullptr && generate(slot);
}

extern "C" int FleetGame_RandomizerGenerationState(void) {
    const auto state = reinterpret_cast<int (*)()>(FleetGame_GetHostFunction("RandomizerGenerationState"));
    return state != nullptr ? state() : 0;
}

extern "C" bool FleetGame_HasSaveSlot(int slot) {
    const auto exists = reinterpret_cast<bool (*)(int)>(FleetGame_GetHostFunction("HasSaveSlot"));
    return exists != nullptr && exists(slot);
}

extern "C" bool FleetGame_ManageSave(const char* operation, int from, int to) {
    const auto manage = reinterpret_cast<bool (*)(const char*, int, int)>(FleetGame_GetHostFunction("ManageSave"));
    return manage != nullptr && manage(operation, from, to);
}

extern "C" const char* FleetGame_GetSaveFilePath(int slot, bool backup) {
    const auto path =
        reinterpret_cast<const char* (*)(const char*, int, bool)>(FleetGame_GetHostFunction("GetSaveFilePath"));
    return path != nullptr ? path(GameName, slot, backup) : nullptr;
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
    if (context->GetWindow()) {
        auto gui = context->GetWindow()->GetGui();
        if (!gui->GetGuiWindow("Configure Controller")) {
            gui->AddGuiWindow(
                std::make_shared<SohInputEditorWindow>(CVAR_WINDOW("ControllerConfiguration"), "Configure Controller"));
        }
    }
}

std::shared_ptr<Ship::ResourceManager> FleetGame_CreateResources(const std::string& archivePath) {
    std::shared_ptr<Ship::GameServices> game = Ship::GameRegistry::Get(GameName);
    assert(game != nullptr);
    // Assigned before Init so the manager's own workers already run as this game.
    game->Resources = std::make_shared<Ship::ResourceManager>();
    game->Resources->Init({ archivePath, Ship::Context::GetPathRelativeToAppDirectory("mods") }, {},
                          ResourceThreadReserve);
    return game->Resources;
}

extern "C" void FleetGame_LeaveHostContext(void) {
    Ship::GameRegistry::Unregister(GameName);
}

extern "C" void FleetGame_FrameEnd(void) {
    FleetRuntime_FrameEnd();
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

namespace {
std::atomic<int> sAutomaticSaves = 0;
}

FleetAutomaticSave::FleetAutomaticSave() {
    sAutomaticSaves++;
}

FleetAutomaticSave::~FleetAutomaticSave() {
    sAutomaticSaves--;
}

extern "C" void FleetGame_CommitSave(void) {
    const auto commit = reinterpret_cast<void (*)()>(FleetGame_GetHostFunction("CommitSave"));
    if (commit != nullptr && sAutomaticSaves == 0) {
        commit();
    }
}

extern "C" bool FleetGame_ResetActiveGame(void) {
    const auto reset = reinterpret_cast<FleetResetActiveGameFn>(FleetGame_GetHostFunction("ResetActiveGame"));
    if (reset == nullptr) {
        return false;
    }
    reset();
    return true;
}

extern "C" void FleetGame_Log(FleetLogLevel level, const char* message) {
    if (sLog != nullptr) {
        sLog(GameName, level, message);
    }
}
