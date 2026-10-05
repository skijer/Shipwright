// Stat pickups. Based on marsh6487's poc/soh-stat-items-20260923 (0574f79c), itself the stat-item work of
// HarbourMasters/Shipwright #6760. The original threads seed settings and tracker entries through the
// randomizer; here the pickups are ordinary custom items that the randomizer shuffles by itself.
//
//   Power       +8% sword damage per copy
//   Defense     -6% damage taken per copy
//   Speed       +4% walking and running speed per copy
//   Climb       +20% ladder and ledge climbing speed per copy
//   Crawl       +12% crawlspace speed per copy
//   Push        faster block pushing and pulling per copy
//   Quarter Heart  a quarter of a heart container, up to the vanilla maximum
//
// The stats stack up to STAT_MAX copies. The counts are kept per save file in the mod's storage. Climb and Push
// borrow the game's own ClimbSpeed / FasterBlockPush options (the pushable actors and the climbing action already
// read them every frame) for exactly as long as the movement lasts, then hand the player's own setting back.

#include <stdio.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include <libultraship/bridge/consolevariablebridge.h>

#define STAT_MAX 10
#define STAT_MOD "stat_upgrades"
#define POWER_PER_COPY 0.08f
#define DEFENSE_PER_COPY 0.06f
#define SPEED_PER_COPY 0.04f
#define CRAWL_PER_COPY 0.12f
#define CLIMB_CVAR "gEnhancements.ClimbSpeed" // the game's own climb-speed option, which adds to the animation speed
#define CLIMB_PER_COPY 2
#define PUSH_CVAR "gEnhancements.FasterBlockPush"
#define PUSH_PER_COPY 1
#define QUARTER_HEART_UNITS 4
#define MAX_HEALTH_CAPACITY 0x140
#define POOL_COPIES 8

typedef enum {
    STAT_POWER,
    STAT_DEFENSE,
    STAT_SPEED,
    STAT_CLIMB,
    STAT_CRAWL,
    STAT_PUSH,
    STAT_COUNT,
} Stat;

typedef struct {
    const char* key;
    const char* icon;
    const char* name;
    const char* model;
    const char* getText;
    const char* pauseText;
} StatItem;

#define TEX "__OTR__textures/stat_upgrades/"
#define OBJ "__OTR__objects/object_stat_upgrade/"
static const ALIGN_ASSET(2) char sPowerIcon[] = TEX "gStatPowerTex";
static const ALIGN_ASSET(2) char sPowerName[] = TEX "gStatPowerNameTex";
static const ALIGN_ASSET(2) char sPowerModel[] = OBJ "gStatPowerDL";
static const ALIGN_ASSET(2) char sDefenseIcon[] = TEX "gStatDefenseTex";
static const ALIGN_ASSET(2) char sDefenseName[] = TEX "gStatDefenseNameTex";
static const ALIGN_ASSET(2) char sDefenseModel[] = OBJ "gStatDefenseDL";
static const ALIGN_ASSET(2) char sSpeedIcon[] = TEX "gStatSpeedTex";
static const ALIGN_ASSET(2) char sSpeedName[] = TEX "gStatSpeedNameTex";
static const ALIGN_ASSET(2) char sSpeedModel[] = OBJ "gStatSpeedDL";
static const ALIGN_ASSET(2) char sClimbIcon[] = TEX "gClimbSpeedTex";
static const ALIGN_ASSET(2) char sClimbName[] = TEX "gStatClimbNameTex";
static const ALIGN_ASSET(2) char sClimbModel[] = OBJ "gStatClimbSpeedDL";
static const ALIGN_ASSET(2) char sCrawlIcon[] = TEX "gCrawlSpeedTex";
static const ALIGN_ASSET(2) char sCrawlName[] = TEX "gStatCrawlNameTex";
static const ALIGN_ASSET(2) char sCrawlModel[] = OBJ "gStatCrawlSpeedDL";
static const ALIGN_ASSET(2) char sPushIcon[] = TEX "gPushSpeedTex";
static const ALIGN_ASSET(2) char sPushName[] = TEX "gStatPushNameTex";
static const ALIGN_ASSET(2) char sPushModel[] = OBJ "gStatPushSpeedDL";
static const ALIGN_ASSET(2) char sHeartIcon[] = "__OTR__textures/icon_item_24_static/gQuestIconHeartPieceTex";
static const ALIGN_ASSET(2) char sHeartName[] = TEX "gStatHeartNameTex";
static const ALIGN_ASSET(2) char sHeartModel[] = "__OTR__objects/object_gi_hearts/gGiHeartPieceDL";

static const StatItem sStats[STAT_COUNT] = {
    { "marsh6487.stat_power", sPowerIcon, sPowerName, sPowerModel,
      "You got a %rPower Up%w!&Your sword hits %r8%w percent harder.",
      "%rPower Up&%wEvery copy makes your sword hit harder." },
    { "marsh6487.stat_defense", sDefenseIcon, sDefenseName, sDefenseModel,
      "You got a %bDefense Up%w!&You take %b6%w percent less damage.",
      "%bDefense Up&%wEvery copy cuts the damage you take." },
    { "marsh6487.stat_speed", sSpeedIcon, sSpeedName, sSpeedModel,
      "You got a %gSpeed Up%w!&You move %g4%w percent faster.", "%gSpeed Up&%wEvery copy makes Link faster." },
    { "marsh6487.stat_climb", sClimbIcon, sClimbName, sClimbModel,
      "You got a %yClimb Up%w!&You climb ladders and ledges faster.", "%yClimb Up&%wEvery copy speeds up climbing." },
    { "marsh6487.stat_crawl", sCrawlIcon, sCrawlName, sCrawlModel,
      "You got a %yCrawl Up%w!&You crawl through tunnels faster.", "%yCrawl Up&%wEvery copy speeds up crawling." },
    { "marsh6487.stat_push", sPushIcon, sPushName, sPushModel,
      "You got a %yPush Up%w!&You push and pull blocks faster.", "%yPush Up&%wEvery copy speeds up pushing blocks." },
};
#define QUARTER_HEART_KEY "marsh6487.quarter_heart"

static const char* const sRequiredHooks[] = { "OnResolveSwordDamage", "OnCollisionResolveDamage",
                                              "OnPlayerResolveMotionScale", "OnLoadGame", "OnPlayerFilterInput",
                                              "OnPlayerUpdate", "OnExitGame", "OnGameFrameUpdate" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static u8 sCount[STAT_COUNT];
static bool sClimbBoosted;
static s32 sClimbOriginal;
static bool sPushBoosted;
static s32 sPushOriginal;

static void StorageKey(char* out, size_t size, int32_t fileNum, Stat stat) {
    snprintf(out, size, "file%d.stat%d", (int)fileNum, (int)stat);
}

static void LoadCounts(int32_t fileNum) {
    for (s32 stat = 0; stat < STAT_COUNT; stat++) {
        char key[32];
        u8 value = 0;

        StorageKey(key, sizeof(key), fileNum, stat);
        sApi->StorageGet(STAT_MOD, key, &value, sizeof(value));
        sCount[stat] = MIN(value, STAT_MAX);
    }
}

static void SaveCount(Stat stat) {
    char key[32];

    StorageKey(key, sizeof(key), gSaveContext.fileNum, stat);
    sApi->StorageSet(STAT_MOD, key, &sCount[stat], sizeof(sCount[stat]));
}

static void Receive(Stat stat) {
    if (sCount[stat] < STAT_MAX) {
        sCount[stat]++;
        SaveCount(stat);
    }
}

static void ReceivePower(const char* key) {
    Receive(STAT_POWER);
}

static void ReceiveDefense(const char* key) {
    Receive(STAT_DEFENSE);
}

static void ReceiveSpeed(const char* key) {
    Receive(STAT_SPEED);
}

static void ReceiveClimb(const char* key) {
    Receive(STAT_CLIMB);
}

static void ReceiveCrawl(const char* key) {
    Receive(STAT_CRAWL);
}

static void ReceivePush(const char* key) {
    Receive(STAT_PUSH);
}

static void ReceiveQuarterHeart(const char* key) {
    gSaveContext.healthCapacity = MIN(gSaveContext.healthCapacity + QUARTER_HEART_UNITS, MAX_HEALTH_CAPACITY);
    gSaveContext.health = MIN(gSaveContext.health + QUARTER_HEART_UNITS, gSaveContext.healthCapacity);
}

static void ForgetFile(int32_t fileNum) {
    LoadCounts(fileNum);
}

// -- effects ---------------------------------------------------------------------------------------------------

static void BoostSword(PlayState* play, int32_t dmgFlags, uint8_t* damage) {
    if (sCount[STAT_POWER] > 0) {
        *damage = (uint8_t)MIN(255.0f, *damage * (1.0f + POWER_PER_COPY * sCount[STAT_POWER]) + 0.5f);
    }
}

static void SoftenDamage(Actor* victim, ColliderInfo* attack, float* damage) {
    if (sCount[STAT_DEFENSE] > 0) {
        *damage = MAX(1.0f, *damage / (1.0f + DEFENSE_PER_COPY * sCount[STAT_DEFENSE]));
    }
}

static void QuickenLink(Player* player, int32_t kind, float* scale) {
    if (sCount[STAT_SPEED] > 0) {
        *scale *= 1.0f + SPEED_PER_COPY * sCount[STAT_SPEED];
    }
}

// Climbing: the game's own ClimbSpeed option is read every frame by the climbing action, so the stat borrows it for
// as long as Link climbs and gives the player's own setting back the moment he lets go.
static void RestoreClimb(void) {
    if (sClimbBoosted) {
        CVarSetInteger(CLIMB_CVAR, sClimbOriginal);
        sClimbBoosted = false;
    }
}

static void BoostClimb(Player* player, Input* input) {
    bool climbing = (player->stateFlags1 & (PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_CLIMBING_LEDGE)) != 0;

    if (climbing && sCount[STAT_CLIMB] > 0) {
        if (!sClimbBoosted) {
            sClimbOriginal = CVarGetInteger(CLIMB_CVAR, 0);
            sClimbBoosted = true;
        }
        CVarSetInteger(CLIMB_CVAR, sClimbOriginal + CLIMB_PER_COPY * sCount[STAT_CLIMB]);
    } else {
        RestoreClimb();
    }
}

static void RestorePush(void) {
    if (sPushBoosted) {
        CVarSetInteger(PUSH_CVAR, sPushOriginal);
        sPushBoosted = false;
    }
}

// Runs before the actors update, so it reads what Link was doing last frame.
static void BoostPush(void) {
    PlayState* play = gPlayState;
    bool pushing = false;

    if (play != NULL && sCount[STAT_PUSH] > 0) {
        pushing = (GET_PLAYER(play)->stateFlags2 & (PLAYER_STATE2_MOVING_DYNAPOLY | PLAYER_STATE2_GRABBING_DYNAPOLY)) != 0;
    }
    if (pushing) {
        if (!sPushBoosted) {
            sPushOriginal = CVarGetInteger(PUSH_CVAR, 0);
            sPushBoosted = true;
        }
        CVarSetInteger(PUSH_CVAR, sPushOriginal + PUSH_PER_COPY * sCount[STAT_PUSH]);
    } else {
        RestorePush();
    }
}

static void RestoreAll(int32_t fileNum) {
    RestoreClimb();
    RestorePush();
}

static void BoostCrawl(void) {
    PlayState* play = gPlayState;

    if (play != NULL && sCount[STAT_CRAWL] > 0) {
        Player* player = GET_PLAYER(play);

        if (player->stateFlags2 & PLAYER_STATE2_CRAWLING) {
            player->linearVelocity *= 1.0f + CRAWL_PER_COPY * sCount[STAT_CRAWL];
        }
    }
}

// -- registration ----------------------------------------------------------------------------------------------

static const SOHCustomItemRandomizer sStatRando = { sizeof(SOHCustomItemRandomizer),
                                                    0,
                                                    SOH_CUSTOM_ITEM_TYPE_ITEM,
                                                    0,
                                                    POOL_COPIES,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    NULL };
static const SOHCustomItemRandomizer sHeartRando = { sizeof(SOHCustomItemRandomizer),
                                                     0,
                                                     SOH_CUSTOM_ITEM_TYPE_ITEM,
                                                     0,
                                                     POOL_COPIES,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     NULL };

static bool RegisterPickup(const char* key, const char* icon, const char* name, const char* model, const char* getText,
                           const char* pauseText, SOHCustomItemStateFunc onReceive,
                           const SOHCustomItemRandomizer* rando) {
    SOHCustomItemDefinition definition = Z64Items_Define(key, icon, name);

    Z64Items_SetTextbox(&definition, getText);
    Z64Items_SetPauseText(&definition, pauseText);
    Z64Items_SetGetItemModel(&definition, model);
    Z64Items_SetLogic(&definition, rando);
    definition.onReceive = onReceive;
    definition.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
    return Z64Items_Register(sApi, &definition);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    static const SOHCustomItemStateFunc sReceivers[STAT_COUNT] = { ReceivePower, ReceiveDefense, ReceiveSpeed,
                                                                    ReceiveClimb, ReceiveCrawl, ReceivePush };

    if (!SOH_MOD_API_HAS(sApi, RegisterCustomItem)) {
        return;
    }
    for (s32 stat = 0; stat < STAT_COUNT; stat++) {
        const StatItem* item = &sStats[stat];

        RegisterPickup(item->key, item->icon, item->name, item->model, item->getText, item->pauseText,
                       sReceivers[stat], &sStatRando);
    }
    RegisterPickup(QUARTER_HEART_KEY, sHeartIcon, sHeartName, sHeartModel,
                   "You got a %rQuarter Heart%w!&Your life grows by a quarter heart.",
                   "%rQuarter Heart&%wA quarter of a heart container.", ReceiveQuarterHeart, &sHeartRando);

    SOH_REGISTER_HOOK(sApi, OnResolveSwordDamage, BoostSword);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnCollisionResolveDamage, ACTOR_PLAYER, SoftenDamage);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveMotionScale, QuickenLink);
    SOH_REGISTER_HOOK(sApi, OnLoadGame, ForgetFile);
    SOH_REGISTER_HOOK(sApi, OnPlayerFilterInput, BoostClimb);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, BoostCrawl);
    SOH_REGISTER_HOOK(sApi, OnExitGame, RestoreAll);
    SOH_REGISTER_HOOK(sApi, OnGameFrameUpdate, BoostPush);
}
