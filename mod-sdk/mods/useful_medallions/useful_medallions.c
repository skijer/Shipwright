// SW97 Medallion Spells, ported 1:1 from Skijer's NEI: the six Spaceworld '97 spell actors cast from the
// medallions, the six elemental shots for the bow and the slingshot, Shadow stealth and the Spirit fairy.

#include "z64items.h"

#include <string.h>

#include "sw97_mod.h"

#define SW97_CUCCO_MODE_SERVICE "sw97.cucco_mode.start"
#define STEALTH_DISTANCE 32000.0f
#define BLIND_TABLE_SIZE 32

typedef struct {
    Actor* actor;
    s16 framesRemaining;
} BlindEntry;

static const Sw97ActorInfo* const sSpellActorInfos[SW97_SPELL_COUNT] = {
    &gSw97MagicWindInfo, &gSw97MagicSoulInfo,  &gSw97MagicDarkInfo,
    &gSw97MagicIceInfo,  &gSw97MagicLightInfo, &gSw97MagicFireInfo,
};

static const Sw97ActorInfo* const sArrowActorInfos[SW97_ELEM_COUNT] = {
    NULL,
    &gSw97ArrowFireInfo,
    &gSw97ArrowIceInfo,
    &gSw97ArrowLightInfo,
    &gSw97ArrowDarkInfo,
    &gSw97ArrowSoulInfo,
    &gSw97ArrowWindInfo,
};

static const char* const sRequiredHooks[] = {
    "OnPlayerUpdate",
    "OnPlayDestroy",
    "OnLoadFile",
    "OnActorInit",
    "OnActorUpdate",
    "OnActorDestroy",
    "OnActorDrawEnd",
    "ShouldActorUpdate",
    "OnPlayerResolveItemAction",
    "OnActorResolvePlayerRelation",
    "OnInterfaceResolveButtonIcon",
    "OnKaleidoItemCursor",
    "OnKaleidoItemDraw",
};
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

// z_bg_ice_shelter internals: the melt its blue fire starts.
void func_808911BC(BgIceShelter* ice);

const SOHModApi* gSw97Api;

static s16 sSpellActorIds[SW97_SPELL_COUNT];
static s16 sArrowActorIds[SW97_ELEM_COUNT];
static BlindEntry sBlinded[BLIND_TABLE_SIZE];

s16 Sw97_GetSpellActorId(Sw97Spell spell) {
    return spell < SW97_SPELL_COUNT ? sSpellActorIds[spell] : -1;
}

s16 Sw97_GetArrowActorId(Sw97Element element) {
    return (element > SW97_ELEM_NONE && element < SW97_ELEM_COUNT) ? sArrowActorIds[element] : -1;
}

void Sw97_MeltRedIce(Actor* thisx, PlayState* play) {
    BgIceShelter* ice = (BgIceShelter*)thisx;

    // King Zora's block holds him frozen until it melts.
    if (((ice->dyna.actor.params >> 8) & 7) == 4 && ice->dyna.actor.parent != NULL) {
        ice->dyna.actor.parent->freezeTimer = 50;
    }
    func_808911BC(ice);
    ice->alpha = 25;
    Audio_PlayActorSound2(&ice->dyna.actor, NA_SE_EV_ICE_MELT);
}

// The Soul arrow's Cucco transformation lives in its own mod; without it a Cucco hit is just a hit.
void Sw97_StartCuccoMode(void) {
    void (*startCuccoMode)(void) = (void (*)(void))gSw97Api->FindService(SW97_CUCCO_MODE_SERVICE);

    if (startCuccoMode != NULL) {
        startCuccoMode();
    }
}

void Sw97_TagBlinded(Actor* actor, s16 frames) {
    BlindEntry* empty = NULL;

    if (actor == NULL || actor->update == NULL) {
        return;
    }
    for (uint8_t i = 0; i < BLIND_TABLE_SIZE; i++) {
        if (sBlinded[i].actor == actor) {
            sBlinded[i].framesRemaining = MAX(sBlinded[i].framesRemaining, frames);
            return;
        }
        if (sBlinded[i].actor == NULL && empty == NULL) {
            empty = &sBlinded[i];
        }
    }
    if (empty != NULL) {
        empty->actor = actor;
        empty->framesRemaining = frames;
    }
}

static bool IsBlinded(Actor* actor) {
    for (uint8_t i = 0; i < BLIND_TABLE_SIZE; i++) {
        if (sBlinded[i].actor == actor && sBlinded[i].framesRemaining > 0) {
            return true;
        }
    }
    return false;
}

static void TickBlindness(void) {
    for (uint8_t i = 0; i < BLIND_TABLE_SIZE; i++) {
        if (sBlinded[i].actor == NULL) {
            continue;
        }
        if (sBlinded[i].actor->update == NULL || --sBlinded[i].framesRemaining <= 0) {
            sBlinded[i].actor = NULL;
            sBlinded[i].framesRemaining = 0;
        }
    }
}

static void ForgetBlinded(void) {
    memset(sBlinded, 0, sizeof(sBlinded));
}

// The Shadow spell drives the Nayru's Love timer for its whole life, so a running timer is the stealth.
static bool IsHiddenFrom(Actor* actor) {
    bool isWatcher = actor->category == ACTORCAT_ENEMY || actor->category == ACTORCAT_NPC ||
                     (actor->flags & ACTOR_FLAG_HOSTILE);

    return (gSaveContext.nayrusLoveTimer > 0 && isWatcher) || IsBlinded(actor);
}

static void HidePlayerFromActor(Actor* actor, float* xzDist, float* yDist, int16_t* yawTowards) {
    if (!IsHiddenFrom(actor)) {
        return;
    }
    *xzDist = STEALTH_DISTANCE;
    *yDist = STEALTH_DISTANCE;
}

static s16 RegisterActor(const Sw97ActorInfo* info) {
    SOHActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = info->key;
    definition.description = info->description;
    definition.category = ACTORCAT_ITEMACTION;
    definition.actorFlags =
        ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED | ACTOR_FLAG_SFX_TIMER;
    definition.objectId = OBJECT_GAMEPLAY_KEEP;
    definition.instanceSize = (uint32_t)info->instanceSize;
    definition.init = (SOHActorFunc)info->init;
    definition.destroy = (SOHActorFunc)info->destroy;
    definition.update = (SOHActorFunc)info->update;
    definition.draw = (SOHActorFunc)info->draw;
    return gSw97Api->RegisterActor(&definition);
}

static bool RegisterActors(void) {
    for (uint8_t spell = 0; spell < SW97_SPELL_COUNT; spell++) {
        sSpellActorIds[spell] = RegisterActor(sSpellActorInfos[spell]);
        if (sSpellActorIds[spell] < 0) {
            return false;
        }
    }
    sArrowActorIds[SW97_ELEM_NONE] = -1;
    for (uint8_t element = SW97_ELEM_FIRE; element < SW97_ELEM_COUNT; element++) {
        sArrowActorIds[element] = RegisterActor(sArrowActorInfos[element]);
        if (sArrowActorIds[element] < 0) {
            return false;
        }
    }
    return true;
}

static void TickMod(void) {
    TickBlindness();
}

static void ForgetScene(void) {
    ForgetBlinded();
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    gSw97Api = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(gSw97Api, FindService) || !RegisterActors() || !Sw97_RegisterSpells()) {
        return;
    }
    Sw97_RegisterArrows();
    Sw97_RegisterArrowUi();
    Sw97_RegisterFairyMode();
    SOH_REGISTER_HOOK(gSw97Api, OnPlayerUpdate, TickMod);
    SOH_REGISTER_HOOK(gSw97Api, OnPlayDestroy, ForgetScene);
    SOH_REGISTER_HOOK(gSw97Api, OnActorResolvePlayerRelation, HidePlayerFromActor);
}
