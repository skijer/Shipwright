#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ModApi/ModApi.h"
#include <libultraship/bridge/consolevariablebridge.h>

#define JUNK_CYCLE_OPTION "junk_cycle.mode"
#define JUNK_CYCLE_CVAR "gRandoSettings.Mods." JUNK_CYCLE_OPTION
#define JUNK_CYCLE_STATIC 1
#define JUNK_CYCLE_PERIOD_FRAMES 30
#define JUNK_CYCLE_LATCH_COUNT 64
#define BOMBCHU_CAPACITY 50

// Declared in soh/OTRGlobals.h, a C++ header a C mod cannot include.
GetItemEntry Randomizer_GetItemFromKnownCheck(RandomizerCheck randomizerCheck, GetItemID ogId);
bool Randomizer_IsCheckShuffled(RandomizerCheck check);

typedef struct {
    RandomizerCheck check;
    RandomizerGet item;
    uint32_t period;
} JunkLatch;

static const RandomizerGet sJunkPlacements[] = {
    RG_GREEN_RUPEE, RG_BLUE_RUPEE, RG_RED_RUPEE, RG_RECOVERY_HEART, RG_ARROWS_5,     RG_ARROWS_10,    RG_ARROWS_30,
    RG_BOMBS_5,     RG_BOMBS_10,   RG_BOMBS_20,  RG_DEKU_NUTS_5,    RG_DEKU_NUTS_10, RG_DEKU_STICK_1, RG_DEKU_SEEDS_30,
};

static const RandomizerGet sJunkRotation[] = {
    RG_GREEN_RUPEE, RG_BLUE_RUPEE,  RG_RED_RUPEE,    RG_PURPLE_RUPEE,  RG_ARROWS_10,      RG_BOMBS_5,
    RG_BOMBCHU_5,   RG_DEKU_NUTS_5, RG_DEKU_STICK_1, RG_DEKU_SEEDS_30, RG_RECOVERY_HEART,
};

static const char* const sModes[] = { "Cycle", "Static" };
static const SOHModRandoOption sOption = {
    sizeof(SOHModRandoOption),
    JUNK_CYCLE_OPTION,
    "Junk Item Cycling",
    "Junk checks show a random junk item you can use right now, and give the item shown.\n"
    "Cycle: the item changes every 1.5 seconds.\n"
    "Static: each check keeps one item for the whole file.",
    sModes,
    ARRAY_COUNT(sModes),
    0,
};

static const char* const sRequiredHooks[] = { "OnRandoResolveCheckItem", "OnActorUpdate", "OnSceneInit" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static JunkLatch sLatches[JUNK_CYCLE_LATCH_COUNT];
static uint32_t sNextLatch;

static bool IsStatic(void) {
    return CVarGetInteger(JUNK_CYCLE_CVAR, 0) == JUNK_CYCLE_STATIC;
}

static uint32_t CurrentPeriod(void) {
    return gPlayState != NULL ? gPlayState->gameplayFrames / JUNK_CYCLE_PERIOD_FRAMES : 0;
}

static bool IsJunkPlacement(RandomizerGet item) {
    for (uint32_t index = 0; index < ARRAY_COUNT(sJunkPlacements); index++) {
        if (sJunkPlacements[index] == item) {
            return true;
        }
    }
    return false;
}

static bool HasAmmoRoom(uint8_t item, uint8_t upgrade) {
    return INV_CONTENT(item) == item && AMMO(item) < CUR_CAPACITY(upgrade);
}

static bool CanUseNow(RandomizerGet item) {
    switch (item) {
        case RG_ARROWS_10:
            return HasAmmoRoom(ITEM_BOW, UPG_QUIVER);
        case RG_BOMBS_5:
            return HasAmmoRoom(ITEM_BOMB, UPG_BOMB_BAG);
        case RG_DEKU_NUTS_5:
            return HasAmmoRoom(ITEM_NUT, UPG_NUTS);
        case RG_DEKU_STICK_1:
            return HasAmmoRoom(ITEM_STICK, UPG_STICKS);
        case RG_DEKU_SEEDS_30:
            return HasAmmoRoom(ITEM_SLINGSHOT, UPG_BULLET_BAG);
        case RG_BOMBCHU_5:
            return INV_CONTENT(ITEM_BOMBCHU) == ITEM_BOMBCHU && AMMO(ITEM_BOMBCHU) < BOMBCHU_CAPACITY;
        default:
            return true;
    }
}

static uint32_t Mix(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    value ^= value >> 16;
    return value;
}

static RandomizerGet RollJunk(RandomizerCheck check, uint32_t period) {
    RandomizerGet usable[ARRAY_COUNT(sJunkRotation)];
    uint32_t count = 0;
    for (uint32_t index = 0; index < ARRAY_COUNT(sJunkRotation); index++) {
        if (CanUseNow(sJunkRotation[index])) {
            usable[count] = sJunkRotation[index];
            count++;
        }
    }
    const uint32_t salt = (uint32_t)gSaveContext.ship.stats.fileCreatedAt;
    return usable[Mix(salt ^ Mix((uint32_t)check ^ Mix(period))) % count];
}

static JunkLatch* FindLatch(RandomizerCheck check) {
    for (uint32_t index = 0; index < JUNK_CYCLE_LATCH_COUNT; index++) {
        if (sLatches[index].check == check) {
            return &sLatches[index];
        }
    }
    return NULL;
}

static void LatchJunk(RandomizerCheck check, RandomizerGet item, uint32_t period) {
    sLatches[sNextLatch] = (JunkLatch){ check, item, period };
    sNextLatch = (sNextLatch + 1) % JUNK_CYCLE_LATCH_COUNT;
}

static void ForgetLatches(int16_t sceneNum) {
    memset(sLatches, 0, sizeof(sLatches));
    sNextLatch = 0;
}

static void ResolveJunk(RandomizerCheck check, RandomizerGet* item) {
    if (!IsJunkPlacement(*item) || !Randomizer_IsCheckShuffled(check)) {
        return;
    }
    if (IsStatic()) {
        *item = RollJunk(check, 0);
        return;
    }
    const JunkLatch* latch = FindLatch(check);
    if (latch != NULL) {
        *item = latch->item;
        return;
    }
    const uint32_t period = CurrentPeriod();
    *item = RollJunk(check, period);
    LatchJunk(check, *item, period);
}

static void CycleFreestandingJunk(void* instance) {
    EnItem00* item00 = instance;
    if (item00->actor.params != ITEM00_SOH_DUMMY || item00->randoCheck == RC_UNKNOWN_CHECK || IsStatic()) {
        return;
    }
    JunkLatch* latch = FindLatch(item00->randoCheck);
    if (latch == NULL || latch->period == CurrentPeriod()) {
        return;
    }
    latch->check = RC_UNKNOWN_CHECK;
    item00->itemEntry = Randomizer_GetItemFromKnownCheck(item00->randoCheck, GI_NONE);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterRandoOption) || !sApi->HasHook("OnRandoResolveCheckItem")) {
        return;
    }
    sApi->RegisterRandoOption(&sOption);
    sApi->RegisterHookByName("OnRandoResolveCheckItem", (void*)ResolveJunk);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_EN_ITEM00, CycleFreestandingJunk);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetLatches);
}
