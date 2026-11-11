/**
 * Shuffle Speak for Majora's Mask forms in Ocarina of Time: a form speaks its own language without its
 * Jabber Nut. Mod behaviours run after Ship's own VB_SPEAK, so they may reopen a talk Ship just refused.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include "z64items.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Ossan/z_en_ossan.h"

static const char* const sRequiredHooks[] = { "OnModVanillaBehavior" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;

static bool IsFormActive(const char* key) {
    const char* form = sApi->GetActiveForm();
    return form != NULL && strcmp(form, key) == 0;
}

static const char* GetNativeForm(Actor* actor) {
    switch (actor->id) {
        case ACTOR_EN_DNS:
        case ACTOR_EN_DNT_JIJI:
        case ACTOR_EN_HINTNUTS:
        case ACTOR_EN_KAKASI:
        case ACTOR_EN_KAKASI2:
        case ACTOR_EN_KAKASI3:
        case ACTOR_OBJ_DEKUJR:
            return "nei.deku";
        case ACTOR_EN_GE1:
        case ACTOR_EN_GE3:
        case ACTOR_EN_NB:
            return "nei.gerudo";
        case ACTOR_EN_GO:
        case ACTOR_EN_GO2:
        case ACTOR_EN_GM:
        case ACTOR_EN_DU:
            return "nei.goron";
        case ACTOR_EN_DIVING_GAME:
        case ACTOR_EN_KZ:
        case ACTOR_EN_RU1:
        case ACTOR_EN_RU2:
        case ACTOR_EN_ZO:
            return "nei.zora";
        case ACTOR_EN_OSSAN:
            if (actor->params == OSSAN_TYPE_GORON) {
                return "nei.goron";
            }
            return actor->params == OSSAN_TYPE_ZORA ? "nei.zora" : NULL;
        default:
            return NULL;
    }
}

static void SpeakNatively(bool* should, va_list args) {
    if (*should || gPlayState == NULL) {
        return;
    }
    Actor* talkActor = GET_PLAYER(gPlayState)->talkActor;
    if (talkActor == NULL) {
        return;
    }
    const char* nativeForm = GetNativeForm(talkActor);
    if (nativeForm != NULL && IsFormActive(nativeForm)) {
        *should = true;
    }
}

// The vanilla result is "a business scrub stands within 130 units", which Ship's refusal hides.
static bool IsBusinessScrubNearby(void) {
    for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_BG].head; actor != NULL; actor = actor->next) {
        if (actor->id == ACTOR_EN_DNS && actor->xzDistToPlayer < 130.0f) {
            return true;
        }
    }
    return false;
}

static void SpeakDekuNatively(bool* should, va_list args) {
    if (!*should && gPlayState != NULL && IsFormActive("nei.deku") && IsBusinessScrubNearby()) {
        *should = true;
    }
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterVB) || !SOH_MOD_API_HAS(sApi, GetActiveForm)) {
        return;
    }
    sApi->RegisterVB(VB_SPEAK, SpeakNatively);
    sApi->RegisterVB(VB_BUSINESS_SCRUB_SPEAK, SpeakDekuNatively);
}
