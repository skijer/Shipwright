#ifndef UNBOUND_Z64ITEMS_H
#define UNBOUND_Z64ITEMS_H

#include "soh/ModApi/ModApi.h"

#define Z64ITEMS_MELEE_NONE 0
#define Z64ITEMS_MELEE_MASTER_SWORD 1
#define Z64ITEMS_MELEE_KOKIRI_SWORD 2
#define Z64ITEMS_MELEE_BIGGORON_SWORD 3
#define Z64ITEMS_MELEE_DEKU_STICK 4
#define Z64ITEMS_MELEE_HAMMER 5

static inline SOHCustomItemDefinition Z64Items_Define(const char* key, const char* iconPath,
                                                      const char* itemNameTexturePath) {
    SOHCustomItemDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = key;
    definition.iconPath = iconPath;
    definition.namePath = itemNameTexturePath;
    return definition;
}

static inline void Z64Items_SetTextbox(SOHCustomItemDefinition* definition, const char* getItemText) {
    definition->getItemText = getItemText;
}

static inline void Z64Items_SetPauseText(SOHCustomItemDefinition* definition, const char* pauseText) {
    definition->pauseText = pauseText;
}

static inline bool Z64Items_UpdateText(const SOHModApi* api, const char* key, SOHItemTextKind kind,
                                       const char* text) {
    return SOH_MOD_API_HAS(api, SetCustomItemText) && api->SetCustomItemText(key, kind, text);
}

static inline void Z64Items_SetHeldModel(SOHCustomItemDefinition* definition, const char* displayListPath,
                                         uint8_t modelGroup) {
    definition->heldModelPath = displayListPath;
    definition->modelGroup = modelGroup;
}

static inline void Z64Items_SetGetItemModel(SOHCustomItemDefinition* definition, const char* displayListPath) {
    definition->getItemModelPath = displayListPath;
}

static inline void Z64Items_SetFirstPersonModel(SOHCustomItemDefinition* definition, const char* displayListPath) {
    definition->firstPersonModelPath = displayListPath;
}

static inline void Z64Items_KeepHeld(const SOHModApi* api) {
    if (SOH_MOD_API_HAS(api, KeepCustomItemHeld)) {
        api->KeepCustomItemHeld();
    }
}

static inline void Z64Items_SetMeleeWeapon(SOHCustomItemDefinition* definition, uint8_t vanillaWeapon) {
    definition->meleeWeapon = vanillaWeapon;
}

static inline void Z64Items_SetAmmo(SOHCustomItemDefinition* definition, SOHCustomItemAmmoFunc getAmmo) {
    definition->getAmmo = getAmmo;
}

static inline void Z64Items_SetAction(SOHCustomItemDefinition* definition, SOHCustomItemInitFunc init,
                                      SOHCustomItemUpdateFunc upperAction) {
    definition->init = init;
    definition->update = upperAction;
}

static inline void Z64Items_SetCanUse(SOHCustomItemDefinition* definition, SOHCustomItemCanUseFunc canUse) {
    definition->canUse = canUse;
}

static inline void Z64Items_SetHeldCallbacks(SOHCustomItemDefinition* definition, SOHCustomItemPlayerFunc use,
                                             SOHCustomItemPlayerFunc putAway, SOHCustomItemPlayerFunc drawHeld) {
    definition->use = use;
    definition->putAway = putAway;
    definition->drawHeld = drawHeld;
}

static inline void Z64Items_SetButtons(SOHCustomItemDefinition* definition, uint32_t buttonFlags) {
    definition->flags |= SOH_CUSTOM_ITEM_EQUIPPABLE | buttonFlags;
}

static inline void Z64Items_SetAge(SOHCustomItemDefinition* definition, SOHCustomItemAge age) {
    definition->ageRequirement = (uint8_t)age;
}

static inline void Z64Items_SetReplaces(SOHCustomItemDefinition* definition, uint16_t vanillaItem) {
    definition->flags |= SOH_CUSTOM_ITEM_REPLACES_VANILLA;
    definition->replacesItem = vanillaItem;
}

static inline void Z64Items_SetVanillaMode(SOHCustomItemDefinition* definition, SOHVanillaItemMode mode,
                                           int32_t randoItem) {
    definition->vanillaMode = (uint8_t)mode;
    definition->randoItem = randoItem;
}

static inline void Z64Items_SetPlacement(SOHCustomItemDefinition* definition, uint8_t page, uint8_t slot,
                                         int32_t wheelPriority) {
    definition->preferredPage = page;
    definition->preferredSlot = slot;
    definition->priority = wheelPriority;
}

static inline void Z64Items_SetLogic(SOHCustomItemDefinition* definition, const SOHCustomItemRandomizer* logic) {
    definition->randomizer = logic;
}

static inline bool Z64Items_Register(const SOHModApi* api, const SOHCustomItemDefinition* definition) {
    return SOH_MOD_API_HAS(api, RegisterCustomItem) && api->RegisterCustomItem(definition);
}

static inline const char* Z64Items_Texture(const SOHModApi* api, const char* key, SOHItemIconSurface surface) {
    return SOH_MOD_API_HAS(api, ResolveCustomItemTexture) ? api->ResolveCustomItemTexture(key, surface) : NULL;
}

#endif
