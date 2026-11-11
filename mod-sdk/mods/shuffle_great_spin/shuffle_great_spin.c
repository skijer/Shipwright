#include "z64items.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define SPIN_KEY "nei.skill.great_spin"
#define SPIN_OPTION "nei.shuffle.great_spin"

static const SOHModApi* sApi;
static const char* const sHooks[] = { "ShouldActorInit" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sHooks, 1 };
static const char* const sValues[] = { "Off", "On" };
static const SOHModRandoOption sOption = {
    sizeof(SOHModRandoOption), SPIN_OPTION, "Shuffle Great Spin Attack",
    "Find Great Spin Attack before using a magical charged spin. Ordinary sword attacks remain available.",
    sValues, 2, 0,
};
static const SOHCustomItemRandomizer sRandomizer = {
    .structSize = sizeof(SOHCustomItemRandomizer),
    .type = SOH_CUSTOM_ITEM_TYPE_ITEM,
    .poolCount = 1,
    .logicKey = SPIN_KEY,
    .hintKey = SPIN_KEY,
    .poolModeOption = SPIN_OPTION,
    .poolModeValue = 1,
};

static void RestrictMagicSpin(void* instance, bool* should) {
    Actor* actor = instance;
    if (actor->id != ACTOR_EN_M_THUNDER || !IS_RANDO || sApi->GetRandoOption(SPIN_OPTION) == 0 ||
        sApi->IsCustomItemOwned(SPIN_KEY)) {
        return;
    }
    // The skipped initializer never creates its collider or light, so its destructor must also be skipped.
    actor->destroy = NULL;
    *should = false;
}

static void DrawReward(PlayState* play, GetItemEntry* entry) {
    GetItem_Draw(play, GID_SWORD_KOKIRI);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterRandoOption) || !SOH_MOD_API_HAS(sApi, GetRandoOption)) {
        return;
    }
    if (!sApi->RegisterRandoOption(&sOption)) {
        return;
    }
    SOHCustomItemDefinition item = Z64Items_Define(SPIN_KEY,
        "__OTR__textures/icon_item_static/gItemIconSwordKokiriTex", NULL);
    item.displayName = "Great Spin Attack";
    item.pauseText = "%rGreat Spin Attack&%wHold B to charge your sword, then release B for a magical spin.";
    item.getItemText = "You got %rGreat Spin Attack%w!&Hold B and release to unleash a magical charged spin.";
    item.preferredPage = SOH_ITEM_PAGE_QUEST_VANILLA + 7;
    item.preferredSlot = 0;
    item.randomizer = &sRandomizer;
    item.getItemEntry.drawFunc = DrawReward;
    if (!Z64Items_Register(sApi, &item)) {
        return;
    }
    SOH_REGISTER_HOOK(sApi, ShouldActorInit, RestrictMagicSpin);
}
