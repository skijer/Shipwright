#ifndef SOH_MOD_API_FORM_REGISTRY_H
#define SOH_MOD_API_FORM_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "z64.h"

typedef enum {
    SOH_FORM_KIND_LINK,
    SOH_FORM_KIND_TAKEOVER,
} SOHFormKind;

typedef enum {
    SOH_FORM_ACTION_VANILLA,
    SOH_FORM_ACTION_BLOCKED,
    SOH_FORM_ACTION_STARTED,
} SOHFormActionResult;

typedef int32_t (*SOHFormActionFunc)(PlayState* play, Player* player);
typedef void (*SOHFormPlayerFunc)(PlayState* play, Player* player);

typedef struct {
    int32_t action;
    SOHFormActionFunc run;
} SOHFormActionOverride;

#define SOH_FORM_ANIM_TYPE_ALL (-1)

typedef struct {
    int32_t group;
    int32_t animType;
    LinkAnimationHeader* anim;
} SOHFormAnimOverride;

typedef struct {
    uint32_t structSize;
    const char* key;
    const char* label;
    SOHFormKind kind;
    const PlayerAgeProperties* body;
    const SOHFormActionOverride* actions;
    uint32_t actionCount;
    const SOHFormAnimOverride* anims;
    uint32_t animCount;
    float motionScale;
    uint16_t blockedButtons;
    SOHFormPlayerFunc onEnter;
    SOHFormPlayerFunc onExit;
    SOHFormPlayerFunc update;
    SOHFormPlayerFunc draw;
    const char* item;
    LinkAnimationHeader* transformAnim;
    const char* transformMask;
    const char* transformMaskClimax;
    uint16_t transformVoiceSfx;
    const char* modelPath;
    float rootScaleAdult;
    float rootScaleChild;
    float rootDrop;
    float height;
    uint16_t (*resolveEquipment)(int32_t equipType, uint16_t value);
    bool (*allowsButtonItem)(uint16_t item, const char* customKey);
    LinkAnimationHeader* transformOffAnim;
    Vec3f transformGlowOffset;
    float transformGlowScale;
    // Kept for the shared definition layout: Link's age is the game's own here, Majora's Mask reads it from the form.
    bool isAdult;
} SOHFormDefinition;

#define SOH_FORM_DEFINITION_MIN_SIZE (offsetof(SOHFormDefinition, draw) + sizeof(((SOHFormDefinition*)0)->draw))

// Form keys both games share. Link without a form is HUMAN; the others are Majora's Mask's own forms, mods here.
#define SOH_FORM_KEY_HUMAN "nei.human"
#define SOH_FORM_KEY_DEKU "nei.deku"
#define SOH_FORM_KEY_GORON "nei.goron"
#define SOH_FORM_KEY_ZORA "nei.zora"
#define SOH_FORM_KEY_FIERCE_DEITY "nei.fierce_deity"
#define SOH_FORM_KEY_ADULT "nei.adult"

bool FormRegistry_Register(const SOHFormDefinition* definition);
const SOHFormDefinition* FormRegistry_Find(const char* key);
uint32_t FormRegistry_GetCount(void);
const SOHFormDefinition* FormRegistry_GetAt(uint32_t index);
bool FormRegistry_SetActive(const char* key);
bool FormRegistry_Toggle(const char* key);
bool FormRegistry_ApplyActive(const char* key);
const char* FormRegistry_GetActiveKey(void);
bool FormRegistry_IsActive(const char* key);
// The local player's form, never NULL: SOH_FORM_KEY_HUMAN while no form is active.
const char* FormRegistry_GetCurrentKey(void);
// An empty list names no restriction.
bool FormRegistry_IsCurrentFormIn(const char* const* formKeys, uint32_t formKeyCount);

#ifdef __cplusplus
}

#include <string>

void FormRegistry_Init();
void FormRegistry_SetCurrentMod(const std::string& name);
std::string FormRegistry_GetOwner(const char* key);
#endif

#endif
