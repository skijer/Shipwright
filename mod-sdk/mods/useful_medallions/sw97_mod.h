// What the mod's translation units share: the actor table, the elements, and the few services the
// ported SW97 actors call back into.

#ifndef SW97_MOD_H
#define SW97_MOD_H

#include "soh/ModApi/ModApi.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include "sw97_compat.h"
#include "align_asset_macro.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
#include "overlays/actors/ovl_Bg_Ice_Shelter/z_bg_ice_shelter.h"

#define SW97_MOD_NAME "useful_medallions"

// The SDK does not receive the exe's CVAR_PREFIX_* defines, so the names are spelled out.
#define SW97_CVAR_FAST_FARORES "gEnhancements.FastFarores"
#define SW97_CVAR_DPAD_ON_PAUSE "gSettings.DPadOnPause"
#define SW97_CVAR_IVAN_COOP "gEnhancements.IvanCoopModeEnabled"
#define SW97_CVAR_NO_CLIP "gCheats.NoClip"
#define SW97_CVAR_SUNLIGHT_ARROWS "gEnhancements.SunlightArrows"

static const ALIGN_ASSET(2) char gEffFlash1DL[] = "__OTR__objects/gameplay_keep/gEffFlash1DL";
static const ALIGN_ASSET(2) char gEffUnknown10Tex[] = "__OTR__objects/gameplay_keep/gEffUnknown10Tex";

typedef struct {
    const char* key;
    const char* description;
    size_t instanceSize;
    ActorFunc init;
    ActorFunc destroy;
    ActorFunc update;
    ActorFunc draw;
} Sw97ActorInfo;

extern const Sw97ActorInfo gSw97MagicWindInfo;
extern const Sw97ActorInfo gSw97MagicSoulInfo;
extern const Sw97ActorInfo gSw97MagicDarkInfo;
extern const Sw97ActorInfo gSw97MagicIceInfo;
extern const Sw97ActorInfo gSw97MagicLightInfo;
extern const Sw97ActorInfo gSw97MagicFireInfo;
extern const Sw97ActorInfo gSw97ArrowFireInfo;
extern const Sw97ActorInfo gSw97ArrowIceInfo;
extern const Sw97ActorInfo gSw97ArrowLightInfo;
extern const Sw97ActorInfo gSw97ArrowDarkInfo;
extern const Sw97ActorInfo gSw97ArrowSoulInfo;
extern const Sw97ActorInfo gSw97ArrowWindInfo;

// NEI's element order: FIRE..WIND map one to one onto ARROW_FIRE..ARROW_0E and PLAYER_IA_BOW_FIRE..0E.
typedef enum {
    SW97_ELEM_NONE,
    SW97_ELEM_FIRE,
    SW97_ELEM_ICE,
    SW97_ELEM_LIGHT,
    SW97_ELEM_DARK,
    SW97_ELEM_SOUL,
    SW97_ELEM_WIND,
    SW97_ELEM_COUNT,
} Sw97Element;

// NEI's spell index: Player_ActionToMagicSpell of the medallion's item action (IA_15, 16, 17, Farore, Nayru, Din).
typedef enum {
    SW97_SPELL_WIND,
    SW97_SPELL_SOUL,
    SW97_SPELL_DARK,
    SW97_SPELL_ICE,
    SW97_SPELL_LIGHT,
    SW97_SPELL_FIRE,
    SW97_SPELL_COUNT,
} Sw97Spell;

extern const SOHModApi* gSw97Api;

s16 Sw97_GetSpellActorId(Sw97Spell spell);
s16 Sw97_GetArrowActorId(Sw97Element element);

bool Sw97_RegisterSpells(void);
void Sw97_RegisterArrows(void);
void Sw97_RegisterArrowUi(void);
void Sw97_RegisterFairyMode(void);

Sw97Element Sw97_GetElement(bool isSling);
void Sw97_SetElement(bool isSling, Sw97Element element);
uint8_t Sw97_ElementCount(void);
Sw97Element Sw97_ElementAt(uint8_t index);

void Sw97_StartFairyMode(void);
bool Sw97_IsFairyModeActive(void);

void Sw97_MeltRedIce(Actor* thisx, PlayState* play);
void Sw97_TagBlinded(Actor* actor, s16 frames);
void Sw97_StartCuccoMode(void);

#endif // SW97_MOD_H
