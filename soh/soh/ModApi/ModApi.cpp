#include "ModApi.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <spdlog/spdlog.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"

#include "variables.h"
#include <libultraship/libultraship.h>
#include "soh/ResourceManagerHelpers.h"
#include "soh/unbound/UnboundAudio.h"

extern "C" PlayState* gPlayState;

namespace {

struct HookBinders {
    void (*global)(void* callback);
    void (*forId)(int32_t id, void* callback);
    void (*forPtr)(uintptr_t ptr, void* callback);
};

std::string sCurrentMod;
std::unordered_set<std::string> sReportedMissing;

uint8_t HasResource(const char* path) {
    if (path == nullptr || path[0] == '\0') {
        return 0;
    }
    std::string name = path;
    if (name.rfind("__OTR__", 0) == 0) {
        name = name.substr(7);
    }
    auto archives = Ship::Context::GetInstance()->GetResourceManager()->GetArchiveManager();

    return archives != nullptr && archives->HasFile(name);
}

std::unordered_map<std::string, void*> sServices;

bool PublishService(const char* name, void* function) {
    if (name == nullptr || name[0] == '\0' || function == nullptr) {
        return false;
    }
    const bool published = sServices.emplace(name, function).second;
    if (!published) {
        SPDLOG_ERROR("[ModApi] Mod '{}' tried to publish '{}', which another mod already provides", sCurrentMod, name);
    }
    return published;
}

void* FindService(const char* name) {
    if (name == nullptr) {
        return nullptr;
    }
    auto service = sServices.find(name);
    return service == sServices.end() ? nullptr : service->second;
}

void LogFromMod(const char* message) {
    if (message == nullptr) {
        return;
    }
    SPDLOG_INFO("[{}] {}", sCurrentMod.empty() ? "mod" : sCurrentMod, message);
}

uint64_t HashText(uint64_t hash, const std::string& text) {
    for (unsigned char value : text) {
        hash ^= value;
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string Hex(uint64_t value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result(16, '0');
    for (size_t i = 0; i < result.size(); ++i) {
        result[result.size() - i - 1] = digits[value & 0xF];
        value >>= 4;
    }
    return result;
}

std::unordered_map<std::string, HookBinders> BuildHookEntries() {
    std::unordered_map<std::string, HookBinders> entries;

#define DEFINE_HOOK(name, args)                                                                                  \
    entries[#name] = {                                                                                           \
        [](void* callback) {                                                                                     \
            GameInteractor::Instance->RegisterGameHook<GameInteractor::name>((SOHCb_##name)callback);            \
        },                                                                                                       \
        [](int32_t id, void* callback) {                                                                         \
            GameInteractor::Instance->RegisterGameHookForID<GameInteractor::name>(id, (SOHCb_##name)callback);   \
        },                                                                                                       \
        [](uintptr_t ptr, void* callback) {                                                                      \
            GameInteractor::Instance->RegisterGameHookForPtr<GameInteractor::name>(ptr, (SOHCb_##name)callback); \
        },                                                                                                       \
    };
#include "soh/Enhancements/game-interactor/GameInteractor_HookTable.h"
#undef DEFINE_HOOK

    return entries;
}

const std::unordered_map<std::string, HookBinders>& HookEntries() {
    static const auto entries = BuildHookEntries();
    return entries;
}

void ReportMissingHook(const char* name) {
    std::string owner = sCurrentMod.empty() ? "unknown" : sCurrentMod;
    std::string key = owner + '\n' + name;
    if (sReportedMissing.insert(key).second) {
        SPDLOG_ERROR("[ModApi] Mod '{}' requested hook '{}', which was not found", owner, name);
    }
}

const HookBinders* FindHookBinders(const char* name, void* callback) {
    if (name == nullptr || callback == nullptr) {
        return nullptr;
    }
    auto entry = HookEntries().find(name);
    if (entry == HookEntries().end()) {
        ReportMissingHook(name);
        return nullptr;
    }
    return &entry->second;
}

bool RegisterHookByName(const char* name, void* callback) {
    const HookBinders* binders = FindHookBinders(name, callback);
    if (binders == nullptr) {
        return false;
    }
    binders->global(callback);
    return true;
}

bool RegisterHookForIdByName(const char* name, int32_t id, void* callback) {
    const HookBinders* binders = FindHookBinders(name, callback);
    if (binders == nullptr) {
        return false;
    }
    binders->forId(id, callback);
    return true;
}

bool RegisterHookForPtrByName(const char* name, uintptr_t ptr, void* callback) {
    const HookBinders* binders = FindHookBinders(name, callback);
    if (binders == nullptr) {
        return false;
    }
    binders->forPtr(ptr, callback);
    return true;
}

bool RegisterVB(GIVanillaBehavior flag, SOHVbCallback callback) {
    if (callback == nullptr) {
        return false;
    }

    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnModVanillaBehavior>(
        flag, [callback](GIVanillaBehavior, bool* should, va_list originalArgs) {
            va_list args;
            va_copy(args, originalArgs);
            callback(should, args);
            va_end(args);
        });
    return true;
}

bool HasHook(const char* name) {
    return name != nullptr && HookEntries().contains(name);
}

SOHModApi BuildTable() {
    SOHModApi api = {};
    api.tableSize = sizeof(SOHModApi);
    api.RegisterHookByName = RegisterHookByName;
    api.RegisterVB = RegisterVB;
    api.HasHook = HasHook;
    api.RegisterCustomItem = CustomItemRegistry_Register;
    api.FindCustomItem = CustomItemRegistry_Find;
    api.GetCustomItemRuntimeId = CustomItemRegistry_GetRuntimeId;
    api.FindCustomItemByRuntimeId = CustomItemRegistry_FindByRuntimeId;
    api.GetCustomItemCount = CustomItemRegistry_GetCount;
    api.GetCustomItemAt = CustomItemRegistry_GetAt;
    api.SetCustomItemOwned = CustomItemRegistry_SetOwned;
    api.IsCustomItemOwned = CustomItemRegistry_IsOwned;
    api.EquipCustomItem = CustomItemRegistry_Equip;
    api.UnequipCustomItem = CustomItemRegistry_Unequip;
    api.GetEquippedCustomItem = CustomItemRegistry_GetEquippedKey;
    api.SelectPauseCustomItem = CustomItemRegistry_SelectPauseItem;
    api.BindActorCustomItem = CustomItemRegistry_BindActor;
    api.UnbindActorCustomItem = CustomItemRegistry_UnbindActor;
    api.GetActorCustomItem = CustomItemRegistry_GetActorItem;
    api.GiveCustomItem = CustomItemRegistry_Give;
    api.RegisterMenuWidget = ModMenu_RegisterWidget;
    api.StorageHas = ModStorage_Has;
    api.StorageGetSize = ModStorage_GetSize;
    api.StorageGet = ModStorage_Get;
    api.StorageSet = ModStorage_Set;
    api.StorageRemove = ModStorage_Remove;
    api.StorageGetString = ModStorage_GetString;
    api.StorageSetString = ModStorage_SetString;
    api.RegisterMenuSidebar = ModMenu_RegisterSidebar;
    api.RegisterMenuWidgetAt = ModMenu_RegisterWidgetAt;
    api.RegisterItemPool = KaleidoItemManager_RegisterPool;
    api.GetItemPoolCount = KaleidoItemManager_GetPoolCount;
    api.GetItemPoolKey = KaleidoItemManager_GetPoolKey;
    api.GetItemPoolSize = KaleidoItemManager_GetPoolSize;
    api.GetItemPoolEntry = KaleidoItemManager_GetPoolEntry;
    api.GetItemPlacement = KaleidoItemManager_GetPlacement;
    api.SetItemPlacement = KaleidoItemManager_SetPlacement;
    api.ResolveCustomItemTexture = CustomItemRegistry_ResolveTexture;
    api.IsCustomItemEditorVisible = CustomItemRegistry_IsEditorVisible;
    api.FindCustomItemReplacement = CustomItemRegistry_FindReplacement;
    api.IsCustomItemAgeAllowed = CustomItemRegistry_IsAgeAllowed;
    api.GetSlotItemCount = KaleidoItemManager_GetSlotItemCount;
    api.GetSlotItemAt = KaleidoItemManager_GetSlotItemAt;
    api.RegisterHookForIdByName = RegisterHookForIdByName;
    api.RegisterHookForPtrByName = RegisterHookForPtrByName;
    api.RegisterForm = FormRegistry_Register;
    api.FindForm = FormRegistry_Find;
    api.SetActiveForm = FormRegistry_SetActive;
    api.GetActiveForm = FormRegistry_GetActiveKey;
    api.IsFormActive = FormRegistry_IsActive;
    api.BlockPlayerInput = PlayerInput_Block;
    api.ReleasePlayerInput = PlayerInput_Release;
    api.ShowTextbox = CustomItemRegistry_ShowTextbox;
    api.RequestTimeControl = TimeControl_Request;
    api.ReleaseTimeControl = TimeControl_Release;
    api.GetWorldSpeed = TimeControl_GetWorldSpeed;
    api.ToggleForm = FormRegistry_Toggle;
    api.BlockVanillaItem = VanillaItems_Block;
    api.GetVanillaItemCustomKey = VanillaItems_GetCustomKey;
    api.GrantLogicCapability = RandoLogic_GrantCapability;
    api.RegisterLogicRule = RandoLogic_RegisterRule;
    api.RemoveLogicOwner = RandoLogic_RemoveOwner;
    api.LogicCanUseItem = RandoLogic_CanUseItem;
    api.LogicHasItem = RandoLogic_HasLogicItem;
    api.LogicIsChild = RandoLogic_IsChild;
    api.LogicIsAdult = RandoLogic_IsAdult;
    api.LogicIsAtDay = RandoLogic_IsAtDay;
    api.LogicIsAtNight = RandoLogic_IsAtNight;
    api.LogicHasCapability = RandoLogic_HasCapability;
    api.GetRandoItem = RandoItems_GetRandoGet;
    api.RegisterRandoFlag = RandoInfCustom_Register;
    api.GetRandoFlag = RandoInfCustom_GetByKey;
    api.SetRandoFlag = RandoInfCustom_SetByKey;
    api.KeepCustomItemHeld = CustomItemRegistry_HoldHeld;
    api.RequestHazard = Timers_RequestHazard;
    api.ReleaseHazard = Timers_ReleaseHazard;
    api.GetHazard = Timers_GetHazard;
    api.StartCountdown = Timers_StartCountdown;
    api.StopCountdown = Timers_StopCountdown;
    api.GetCountdown = Timers_GetCountdown;
    api.IsCountdownRunning = Timers_IsCountdownRunning;
    api.RegisterActor = ActorRegistry_Register;
    api.GetActorId = ActorRegistry_GetId;
    api.HasResource = HasResource;
    api.SetCustomItemText = CustomItemRegistry_SetText;
    api.RegisterAudioMix = AudioMix_Register;
    api.RegisterRandoOption = RandoOptions_Register;
    api.GetRandoOption = RandoOptions_Get;
    api.ExportArchiveFiles = O2rExtractor_ExportFiles;
    api.ExtractRom = O2rExtractor_Run;
    api.Log = LogFromMod;
    api.RegisterCustomEquipment = CustomEquipRegistry_Register;
    api.GetWornEquipment = CustomEquipRegistry_GetWorn;
    api.IsEquipmentWorn = CustomEquipRegistry_IsWorn;
    api.IsEquipmentOwned = CustomEquipRegistry_IsOwned;
    api.SetEquipmentOwned = CustomEquipRegistry_SetOwned;
    api.WearEquipment = CustomEquipRegistry_Wear;
    api.TakeOffEquipment = CustomEquipRegistry_TakeOff;
    api.IsEquipmentToggleOn = CustomEquipRegistry_IsToggleOn;
    api.SetEquipmentToggle = CustomEquipRegistry_SetToggle;
    api.RegisterAudioMixInGroup = AudioMix_RegisterInGroup;
    api.RegisterSequence = Unbound_RegisterSequence;
    api.RegisterSoundFont = Unbound_RegisterSoundFont;
    api.GetSequenceId = Unbound_SequenceIdForPath;
    api.RegisterLayoutPage = ModLayout_RegisterPage;
    api.PlaceLayoutItem = ModLayout_PlaceItem;
    api.RegisterLayoutVanilla = ModLayout_RegisterVanilla;
    api.PublishService = PublishService;
    api.FindService = FindService;
    api.RegisterMessage = ModMessages_Register;
    api.GetMessageId = ModMessages_GetId;
    return api;
}

} // namespace

void ModApi_Init() {
    ModStorage_Init();
    RandoInfCustom_Init();
    CustomItemRegistry_Init();
    CustomEquipRegistry_Init();
    KaleidoEquipManager_Init();
    AudioMix_Init();
    KaleidoItemManager_Init();
    KaleidoQuestManager_Init();
    ModLayout_Init();
    PlayerInput_Init();
    FormRegistry_Init();
    VanillaItems_Init();
    TimeControl_Init();
    Timers_Init();
    ModLoader_LoadMods();
}

const SOHModApi* ModApi_Get() {
    static const SOHModApi api = BuildTable();
    return &api;
}

void ModApi_SetCurrentMod(const std::string& name) {
    sCurrentMod = name;
    CustomItemRegistry_SetCurrentMod(name);
    CustomEquipRegistry_SetCurrentMod(name);
}

void ModApi_ClearCurrentMod() {
    sCurrentMod.clear();
    CustomItemRegistry_ClearCurrentMod();
    CustomEquipRegistry_SetCurrentMod(std::string());
}

bool ModApi_HasHook(const char* name) {
    return HasHook(name);
}

std::string ModApi_GetCatalogFingerprint() {
    std::vector<std::string> entries;
    entries.reserve(HookEntries().size());

    for (const auto& [name, binder] : HookEntries()) {
        entries.push_back(name);
    }

    std::sort(entries.begin(), entries.end());
    uint64_t hash = 1469598103934665603ULL;
    for (const auto& entry : entries) {
        hash = HashText(hash, entry);
    }
    hash = HashText(hash, gGitCommitHash);
    hash = HashText(hash, __DATE__);
    hash = HashText(hash, __TIME__);
    return Hex(hash);
}
