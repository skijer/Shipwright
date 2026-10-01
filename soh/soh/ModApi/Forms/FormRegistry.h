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
} SOHFormDefinition;

#define SOH_FORM_DEFINITION_MIN_SIZE (offsetof(SOHFormDefinition, draw) + sizeof(((SOHFormDefinition*)0)->draw))

bool FormRegistry_Register(const SOHFormDefinition* definition);
const SOHFormDefinition* FormRegistry_Find(const char* key);
uint32_t FormRegistry_GetCount(void);
const SOHFormDefinition* FormRegistry_GetAt(uint32_t index);
bool FormRegistry_SetActive(const char* key);
bool FormRegistry_Toggle(const char* key);
bool FormRegistry_ApplyActive(const char* key);
const char* FormRegistry_GetActiveKey(void);
bool FormRegistry_IsActive(const char* key);

#ifdef __cplusplus
}

void FormRegistry_Init();
#endif

#endif
