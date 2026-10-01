#ifndef SOH_MOD_API_H
#define SOH_MOD_API_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#include <memory>
extern "C" {
#endif
#include "z64.h"
#ifdef __cplusplus
}
#endif

#include "soh/Enhancements/game-interactor/vanilla-behavior/GIVanillaBehavior.h"
#include "soh/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "soh/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"
#include "soh/ModApi/VanillaItems/VanillaItems.h"
#include "soh/ModApi/ModStorage/ModStorage.h"
#include "soh/ModApi/ModMenu/ModMenu.h"
#include "soh/ModApi/Forms/FormRegistry.h"
#include "soh/ModApi/Player/PlayerHookTypes.h"
#include "soh/ModApi/TimeControl/TimeControl.h"
#include "soh/ModApi/AudioMix/AudioMix.h"
#include "soh/ModApi/Timers/Timers.h"
#include "soh/ModApi/PlayerInput/PlayerInput.h"
#include "soh/ModApi/RandoLogic/RandoLogic.h"
#include "soh/ModApi/RandoInfCustom/RandoInfCustom.h"
#include "soh/ModApi/RandoItems/RandoItems.h"
#include "soh/ModApi/RandoOptions/RandoOptions.h"
#include "soh/ModApi/O2rExtractor/O2rExtractor.h"
#include "soh/ModApi/ActorRegistry/ActorRegistry.h"
#include "soh/ModApi/ModMessages/ModMessages.h"
#include "soh/ModApi/Layout/ModLayout.h"

#ifdef _WIN32
#define SOH_MOD_EXPORT __declspec(dllexport)
#else
#define SOH_MOD_EXPORT __attribute__((visibility("default")))
#endif

#define DEFINE_HOOK(name, args) typedef void(*SOHCb_##name) args;
#include "soh/Enhancements/game-interactor/GameInteractor_HookTable.h"
#undef DEFINE_HOOK

typedef void (*SOHVbCallback)(bool* should, va_list args);

typedef struct {
    uint32_t structSize;
    const char* const* requiredHooks;
    uint32_t requiredHookCount;
} SOHModRequirements;

typedef struct {
    uint32_t tableSize;
    bool (*RegisterHookByName)(const char* name, void* callback);
    bool (*RegisterVB)(GIVanillaBehavior flag, SOHVbCallback callback);
    bool (*HasHook)(const char* name);
    bool (*RegisterCustomItem)(const SOHCustomItemDefinition* definition);
    const SOHCustomItemDefinition* (*FindCustomItem)(const char* key);
    uint16_t (*GetCustomItemRuntimeId)(const char* key);
    const SOHCustomItemDefinition* (*FindCustomItemByRuntimeId)(uint16_t runtimeId);
    uint32_t (*GetCustomItemCount)(void);
    const SOHCustomItemDefinition* (*GetCustomItemAt)(uint32_t index);
    bool (*SetCustomItemOwned)(const char* key, bool owned);
    bool (*IsCustomItemOwned)(const char* key);
    bool (*EquipCustomItem)(uint8_t button, const char* key);
    void (*UnequipCustomItem)(uint8_t button);
    const char* (*GetEquippedCustomItem)(uint8_t button);
    bool (*SelectPauseCustomItem)(const char* key);
    bool (*BindActorCustomItem)(Actor* actor, const char* key);
    void (*UnbindActorCustomItem)(Actor* actor);
    const char* (*GetActorCustomItem)(Actor* actor);
    bool (*GiveCustomItem)(PlayState* play, const char* key);
    bool (*RegisterMenuWidget)(const SOHModMenuWidget* widget);
    bool (*StorageHas)(const char* mod, const char* key);
    uint32_t (*StorageGetSize)(const char* mod, const char* key);
    uint32_t (*StorageGet)(const char* mod, const char* key, void* output, uint32_t capacity);
    bool (*StorageSet)(const char* mod, const char* key, const void* data, uint32_t size);
    bool (*StorageRemove)(const char* mod, const char* key);
    uint32_t (*StorageGetString)(const char* mod, const char* key, char* output, uint32_t capacity);
    bool (*StorageSetString)(const char* mod, const char* key, const char* value);
    bool (*RegisterMenuSidebar)(const char* section, const char* sidebar, const char* selectionCvar, uint8_t columns);
    bool (*RegisterMenuWidgetAt)(const SOHModMenuWidget* widget, const char* anchor, bool after);
    bool (*RegisterItemPool)(const char* key, const SOHItemPoolEntry* entries, uint32_t count);
    uint32_t (*GetItemPoolCount)(void);
    const char* (*GetItemPoolKey)(uint32_t index);
    uint32_t (*GetItemPoolSize)(const char* key);
    bool (*GetItemPoolEntry)(const char* key, uint32_t index, SOHItemPoolEntry* entry);
    bool (*GetItemPlacement)(const char* key, SOHCustomItemPlacement* placement);
    bool (*SetItemPlacement)(const char* key, const SOHCustomItemPlacement* placement);
    const char* (*ResolveCustomItemTexture)(const char* key, SOHItemIconSurface surface);
    bool (*IsCustomItemEditorVisible)(const char* key);
    const SOHCustomItemDefinition* (*FindCustomItemReplacement)(uint16_t vanillaItem);
    bool (*IsCustomItemAgeAllowed)(const char* key);
    uint32_t (*GetSlotItemCount)(uint16_t page, uint8_t slot);
    const char* (*GetSlotItemAt)(uint16_t page, uint8_t slot, uint32_t index);
    bool (*RegisterHookForIdByName)(const char* name, int32_t id, void* callback);
    bool (*RegisterHookForPtrByName)(const char* name, uintptr_t ptr, void* callback);
    bool (*RegisterForm)(const SOHFormDefinition* definition);
    const SOHFormDefinition* (*FindForm)(const char* key);
    bool (*SetActiveForm)(const char* key);
    const char* (*GetActiveForm)(void);
    bool (*IsFormActive)(const char* key);
    bool (*BlockPlayerInput)(const char* owner, uint16_t buttons, bool blockStick);
    void (*ReleasePlayerInput)(const char* owner);
    bool (*ShowTextbox)(PlayState* play, const char* text, bool autoFormat);
    bool (*RequestTimeControl)(const char* owner, int32_t priority, float worldSpeed, bool freezeClock);
    void (*ReleaseTimeControl)(const char* owner);
    float (*GetWorldSpeed)(void);
    bool (*ToggleForm)(const char* key);
    bool (*BlockVanillaItem)(uint16_t vanillaItem, int32_t randoItem);
    const char* (*GetVanillaItemCustomKey)(uint16_t vanillaItem);
    bool (*GrantLogicCapability)(const char* owner, const char* capability, SOHLogicCondition condition,
                                 void* userData);
    bool (*RegisterLogicRule)(const SOHLogicRule* rule);
    void (*RemoveLogicOwner)(const char* name);
    bool (*LogicCanUseItem)(int32_t randoGet);
    bool (*LogicHasItem)(int32_t randoGet);
    bool (*LogicIsChild)(void);
    bool (*LogicIsAdult)(void);
    bool (*LogicIsAtDay)(void);
    bool (*LogicIsAtNight)(void);
    bool (*LogicHasCapability)(const char* capability);
    int32_t (*GetRandoItem)(const char* key);
    uint16_t (*RegisterRandoFlag)(const char* key);
    bool (*GetRandoFlag)(const char* key);
    void (*SetRandoFlag)(const char* key, bool state);
    bool (*RequestHazard)(const char* owner, int32_t priority, int16_t hazard, uint8_t requiredTunic);
    void (*ReleaseHazard)(const char* owner);
    int16_t (*GetHazard)(void);
    bool (*StartCountdown)(const char* owner, uint16_t seconds);
    void (*StopCountdown)(const char* owner);
    uint16_t (*GetCountdown)(const char* owner);
    bool (*IsCountdownRunning)(const char* owner);
    void (*KeepCustomItemHeld)(void);
    int16_t (*RegisterActor)(const SOHActorDefinition* definition);
    int16_t (*GetActorId)(const char* key);
    uint8_t (*HasResource)(const char* path);
    bool (*SetCustomItemText)(const char* key, SOHItemTextKind kind, const char* text);
    bool (*RegisterAudioMix)(SOHAudioMixFunc callback);
    bool (*RegisterRandoOption)(const SOHModRandoOption* option);
    uint8_t (*GetRandoOption)(const char* key);
    uint32_t (*ExportArchiveFiles)(const char* searchMask, const char* destinationDir);
    bool (*ExtractRom)(const SOHO2rExtractRequest* request);
    void (*Log)(const char* message);
    bool (*RegisterCustomEquipment)(const SOHCustomEquipDefinition* definition);
    const char* (*GetWornEquipment)(uint8_t slot);
    bool (*IsEquipmentWorn)(const char* key);
    bool (*IsEquipmentOwned)(const char* key);
    bool (*SetEquipmentOwned)(const char* key, bool owned);
    bool (*WearEquipment)(const char* key);
    void (*TakeOffEquipment)(uint8_t slot);
    bool (*IsEquipmentToggleOn)(const char* key);
    bool (*SetEquipmentToggle)(const char* key, bool on);
    bool (*RegisterAudioMixInGroup)(SOHAudioMixFunc callback, uint8_t group, const char* volumeCvar);
    uint16_t (*RegisterSequence)(const char* path);
    int32_t (*RegisterSoundFont)(const char* path);
    uint16_t (*GetSequenceId)(const char* path);
    bool (*RegisterLayoutPage)(const SOHLayoutPage* page);
    bool (*PlaceLayoutItem)(const char* itemKey, const char* pageKey, uint8_t cell);
    bool (*RegisterLayoutVanilla)(const SOHLayoutVanilla* entry);
    bool (*PublishService)(const char* name, void* function);
    void* (*FindService)(const char* name);
    uint16_t (*RegisterMessage)(const SOHModMessage* message);
    uint16_t (*GetMessageId)(const char* key);
} SOHModApi;

#define SOH_REGISTER_HOOK_FOR_ID(api, name, id, callback) \
    ((api)->RegisterHookForIdByName(#name, (id), (void*)(SOHCb_##name)(callback)))

#define SOH_REGISTER_HOOK(api, name, callback) ((api)->RegisterHookByName(#name, (void*)(SOHCb_##name)(callback)))
#define SOH_MOD_API_MEMBER_SIZE(member) (offsetof(SOHModApi, member) + sizeof(((SOHModApi*)0)->member))
#define SOH_MOD_API_MATCHES(api) ((api) != NULL && (api)->tableSize >= SOH_MOD_API_MEMBER_SIZE(HasHook))
#define SOH_MOD_API_HAS(api, member) ((api) != NULL && (api)->tableSize >= SOH_MOD_API_MEMBER_SIZE(member))

typedef void (*SOHModSetApiFunc)(const SOHModApi* api);
typedef const SOHModRequirements* (*SOHModGetRequirementsFunc)(void);
typedef void (*SOHModInitFunc)(void);

#ifdef __cplusplus
extern "C" {
#endif

const SOHModApi* ModApi_Get(void);
bool ModApi_HasHook(const char* name);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include <string>

void ModApi_Init();
void ModLoader_LoadMods();
void ModApi_SetCurrentMod(const std::string& name);
void ModApi_ClearCurrentMod();
std::string ModApi_GetCatalogFingerprint();

#endif

#endif
