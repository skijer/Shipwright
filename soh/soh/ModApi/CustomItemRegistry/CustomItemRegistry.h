#ifndef SOH_CUSTOM_ITEM_REGISTRY_H
#define SOH_CUSTOM_ITEM_REGISTRY_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "soh/Enhancements/item-tables/ItemTableTypes.h"
#include "soh/ModApi/VanillaItems/VanillaItems.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SOH_CUSTOM_ITEM_RANDO_ADVANCEMENT = 1 << 0,
    SOH_CUSTOM_ITEM_RANDO_PROGRESSIVE = 1 << 1,
} SOHCustomItemRandomizerFlags;

typedef enum {
    SOH_CUSTOM_ITEM_EQUIPPABLE = 1 << 0,
    SOH_CUSTOM_ITEM_INSTANT = 1 << 1,
    SOH_CUSTOM_ITEM_C_BUTTON = 1 << 2,
    SOH_CUSTOM_ITEM_DPAD = 1 << 3,
    SOH_CUSTOM_ITEM_B_BUTTON = 1 << 4,
    SOH_CUSTOM_ITEM_REPLACES_VANILLA = 1 << 5,
    SOH_CUSTOM_ITEM_WEARABLE = 1 << 6,
} SOHCustomItemFlags;

typedef enum {
    SOH_CUSTOM_ITEM_AGE_ANY = 0,
    SOH_CUSTOM_ITEM_AGE_CHILD = 1,
    SOH_CUSTOM_ITEM_AGE_ADULT = 2,
} SOHCustomItemAge;

typedef struct Player Player;
typedef enum {
    SOH_ITEM_ICON_TEXTBOX = 1 << 0,
    SOH_ITEM_ICON_SAVE_EDITOR = 1 << 1,
    SOH_ITEM_ICON_INVENTORY = 1 << 2,
    SOH_ITEM_ICON_B_BUTTON = 1 << 3,
    SOH_ITEM_ICON_C_BUTTON = 1 << 4,
    SOH_ITEM_ICON_DPAD = 1 << 5,
    SOH_ITEM_NAME_TEXTURE = 1 << 6,
} SOHItemIconSurface;

typedef enum {
    SOH_ITEM_HIDE_FROM_SAVE_EDITOR = 1 << 0,
    SOH_ITEM_HIDE_FROM_KALEIDO = 1 << 1,
} SOHItemPresentationFlags;

typedef enum {
    SOH_ITEM_TEXT_GET_ITEM = 0,
    SOH_ITEM_TEXT_PAUSE = 1,
} SOHItemTextKind;
typedef struct Actor Actor;

typedef bool (*SOHCustomItemCanUseFunc)(Player* player, PlayState* play);
typedef void (*SOHCustomItemInitFunc)(PlayState* play, Player* player);
typedef int32_t (*SOHCustomItemUpdateFunc)(Player* player, PlayState* play);
typedef void (*SOHCustomItemPlayerFunc)(Player* player, PlayState* play);
typedef void (*SOHCustomItemStateFunc)(const char* key);
typedef int32_t (*SOHCustomItemAmmoFunc)(const char* key);

typedef enum {
    SOH_CUSTOM_ITEM_TYPE_ITEM = 0,
    SOH_CUSTOM_ITEM_TYPE_EQUIP = 1,
    SOH_CUSTOM_ITEM_TYPE_SONG = 11,
    SOH_CUSTOM_ITEM_TYPE_SHOP = 12,
} SOHCustomItemPoolType;

typedef struct {
    uint32_t structSize;
    uint32_t flags;
    uint32_t type;
    uint16_t price;
    uint16_t poolCount;
    const char* logicKey;
    const char* hintKey;
    const char* progressionGroup;
    const char* poolCountOption;
} SOHCustomItemRandomizer;

typedef struct {
    uint32_t structSize;
    const char* key;
    const char* iconPath;
    const char* namePath;
    const char* pauseText;
    uint32_t flags;
    int32_t priority;
    uint8_t preferredPage;
    uint8_t preferredSlot;
    uint8_t ageRequirement;
    uint8_t modelGroup;
    GetItemEntry getItemEntry;
    SOHCustomItemCanUseFunc canUse;
    SOHCustomItemInitFunc init;
    SOHCustomItemUpdateFunc update;
    SOHCustomItemPlayerFunc drawHeld;
    SOHCustomItemStateFunc onAcquire;
    SOHCustomItemStateFunc onRemove;
    const SOHCustomItemRandomizer* randomizer;
    uint32_t disabledIconSurfaces;
    uint32_t presentationFlags;
    uint16_t replacesItem;
    const char* getItemText;
    const char* heldModelPath;
    const char* getItemModelPath;
    SOHCustomItemPlayerFunc use;
    SOHCustomItemPlayerFunc putAway;
    uint8_t vanillaMode;
    int32_t randoItem;
    const char* firstPersonModelPath;
    SOHCustomItemStateFunc onReceive;
    uint8_t meleeWeapon;
    SOHCustomItemAmmoFunc getAmmo;
} SOHCustomItemDefinition;

#define SOH_CUSTOM_ITEM_RANDOMIZER_MIN_SIZE \
    (offsetof(SOHCustomItemRandomizer, progressionGroup) + sizeof(((SOHCustomItemRandomizer*)0)->progressionGroup))
#define SOH_CUSTOM_ITEM_DEFINITION_MIN_SIZE \
    (offsetof(SOHCustomItemDefinition, randomizer) + sizeof(((SOHCustomItemDefinition*)0)->randomizer))
#define SOH_CUSTOM_ITEM_REPLACEMENT_MIN_SIZE \
    (offsetof(SOHCustomItemDefinition, replacesItem) + sizeof(((SOHCustomItemDefinition*)0)->replacesItem))

static inline bool CustomItemRegistry_IsVanillaUpgrade(const SOHCustomItemDefinition* definition) {
    return definition != NULL && (definition->flags & SOH_CUSTOM_ITEM_REPLACES_VANILLA) != 0 &&
           definition->vanillaMode == SOH_VANILLA_ITEM_UPGRADE;
}

bool CustomItemRegistry_Register(const SOHCustomItemDefinition* definition);
const SOHCustomItemDefinition* CustomItemRegistry_FindReplacement(uint16_t vanillaItem);
bool CustomItemRegistry_IsAgeAllowed(const char* key);
bool CustomItemRegistry_IsPauseItemAgeAllowed(void);
const char* CustomItemRegistry_ResolveTexture(const char* key, SOHItemIconSurface surface);
bool CustomItemRegistry_IsEditorVisible(const char* key);
typedef struct {
    uint16_t vanillaItem;
    const char* customKey;
} SOHItemPoolEntry;

#define SOH_ITEM_PAGE_VANILLA 0xFF
#define SOH_ITEM_PAGE_QUEST_VANILLA 0xE0

static inline bool CustomItemRegistry_IsQuestPage(uint16_t page) {
    return page >= SOH_ITEM_PAGE_QUEST_VANILLA && page != SOH_ITEM_PAGE_VANILLA;
}

typedef struct {
    uint16_t page;
    uint8_t slot;
} SOHCustomItemPlacement;

bool KaleidoItemManager_RegisterPool(const char* key, const SOHItemPoolEntry* entries, uint32_t count);
uint32_t KaleidoItemManager_GetPoolCount(void);
const char* KaleidoItemManager_GetPoolKey(uint32_t index);
uint32_t KaleidoItemManager_GetPoolSize(const char* key);
bool KaleidoItemManager_GetPoolEntry(const char* key, uint32_t index, SOHItemPoolEntry* entry);
bool KaleidoItemManager_GetPlacement(const char* key, SOHCustomItemPlacement* placement);
bool KaleidoItemManager_SetPlacement(const char* key, const SOHCustomItemPlacement* placement);
uint32_t KaleidoItemManager_GetSlotItemCount(uint16_t page, uint8_t slot);
const char* KaleidoItemManager_GetSlotItemAt(uint16_t page, uint8_t slot, uint32_t index);
bool KaleidoItemManager_IsVanillaPageShown(void);
uint16_t KaleidoItemManager_GetSlotItem(uint8_t slot);
uint8_t KaleidoItemManager_GetVanillaSlot(uint8_t cell);

bool KaleidoQuestManager_IsVanillaPageShown(void);
bool KaleidoQuestManager_IsPointClaimed(uint8_t point);
bool KaleidoQuestManager_CycleQuestPage(PlayState* play);
bool KaleidoQuestManager_EquipPoint(PlayState* play);
void KaleidoQuestManager_DrawPoints(PlayState* play);
void KaleidoQuestManager_Init(void);
void* KaleidoItemManager_GetSlotIcon(uint8_t slot);
bool KaleidoItemManager_IsSlotAgeAllowed(uint8_t slot);
bool KaleidoItemManager_CycleItemPage(PlayState* play);
void KaleidoItemManager_HandleWheel(PlayState* play);
void KaleidoItemManager_DrawWheels(PlayState* play);
const SOHCustomItemDefinition* CustomItemRegistry_Find(const char* key);
const GetItemEntry* CustomItemRegistry_FindGetItemEntry(const char* key);
uint16_t CustomItemRegistry_GetRuntimeId(const char* key);
const SOHCustomItemDefinition* CustomItemRegistry_FindByRuntimeId(uint16_t runtimeId);
uint32_t CustomItemRegistry_GetCount(void);
const SOHCustomItemDefinition* CustomItemRegistry_GetAt(uint32_t index);
bool CustomItemRegistry_SetOwned(const char* key, bool owned);
void CustomItemRegistry_ReceiveGetItem(const char* key);
bool CustomItemRegistry_IsOwned(const char* key);
bool CustomItemRegistry_Equip(uint8_t button, const char* key);
void CustomItemRegistry_Unequip(uint8_t button);
void CustomItemRegistry_CopyEquippedKey(uint8_t fromButton, uint8_t toButton);
const char* CustomItemRegistry_GetEquippedKey(uint8_t button);
uint16_t CustomItemRegistry_GetEquippedRuntimeId(uint8_t button);
bool CustomItemRegistry_PressButtonItem(PlayState* play, Player* player, uint8_t button, uint16_t item);
bool CustomItemRegistry_SelectPauseItem(const char* key);
const SOHCustomItemDefinition* CustomItemRegistry_GetPauseItem(void);
void CustomItemRegistry_PreparePauseMessage(void);
const SOHCustomItemDefinition* CustomItemRegistry_GetMessageItem(void);
const SOHCustomItemDefinition* CustomItemRegistry_GetActive(void);
const char* CustomItemRegistry_GetHeldKey(void);
bool CustomItemRegistry_SetIconPath(const char* key, const char* path);
bool CustomItemRegistry_SetText(const char* key, SOHItemTextKind kind, const char* text);
uint8_t CustomItemRegistry_GetActiveMeleeWeapon(void);
void CustomItemRegistry_InitHeld(PlayState* play, Player* player);
int32_t CustomItemRegistry_UpdateHeld(Player* player, PlayState* play);
void CustomItemRegistry_DrawHeld(Player* player, PlayState* play);
const char* CustomItemRegistry_GetFirstPersonModel(void);
void CustomItemRegistry_HoldHeld(void);
bool CustomItemRegistry_AttachDrop(Actor* actor, const char* key);
bool CustomItemRegistry_BindActor(Actor* actor, const char* key);
void CustomItemRegistry_UnbindActor(Actor* actor);
const char* CustomItemRegistry_GetActorItem(Actor* actor);
bool CustomItemRegistry_Give(PlayState* play, const char* key);
bool CustomItemRegistry_ShowTextbox(PlayState* play, const char* text, bool autoFormat);
bool CustomItemRegistry_IsCustomGetItem(const GetItemEntry* entry);
bool CustomItemRegistry_HasMessageItemIcon(void);
bool CustomItemRegistry_PrepareGetItem(Actor* actor, PlayState* play, GetItemEntry* entry);
const SOHCustomItemDefinition* CustomItemRegistry_GetPendingGetItem(void);
void CustomItemRegistry_ClearPendingGetItem(void);
bool CustomItemRegistry_UpdateDrop(Actor* actor, PlayState* play);
bool CustomItemRegistry_DrawDrop(Actor* actor, PlayState* play);
void CustomItemRegistry_RemoveDrop(Actor* actor);

#ifdef __cplusplus
}

#include <string>

void CustomItemRegistry_Init();
void KaleidoItemManager_Init();
void CustomItemRegistry_SetCurrentMod(const std::string& name);
void CustomItemRegistry_ClearCurrentMod();
#endif

#endif
