#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <string>
#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "soh/Enhancements/game-interactor/vanilla-behavior/GIVanillaBehavior.h"
extern "C" {
SaveContext gSaveContext = {};
}

static int failures;
#define CHECK(condition)                                                      \
    do {                                                                      \
        if (!(condition)) {                                                   \
            std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
            ++failures;                                                       \
        }                                                                     \
    } while (0)
struct TestOption {
    uint8_t value = 0;
    uint8_t Get() const {
        return value;
    }
    operator bool() const {
        return value != 0;
    }
};
namespace Rando {
struct Context {
    std::array<TestOption, RSK_MAX> options{};
    TestOption& GetOption(RandomizerSettingKey key) {
        return options[key];
    }
    static Context* GetInstance() {
        static Context context;
        return &context;
    }
};
} // namespace Rando
static bool isRando = true;
#undef IS_RANDO
#define IS_RANDO isRando
#define RAND_GET_OPTION(key) Rando::Context::GetInstance()->GetOption(key)
static std::map<int, std::function<void(bool*, va_list)>> hooks;
#define COND_VB_SHOULD(id, condition, ...)       \
    hooks[id] = [](bool* should, va_list args) { \
        if (condition) {                         \
            __VA_ARGS__                          \
        }                                        \
    }
static float randomRoll;
static int criticalEffects;
static int currentMenuAdjustable;
extern "C" f32 Rand_ZeroOne(void) {
    return randomRoll;
}
extern "C" void EffectSsBomb2_SpawnFade(PlayState*, Vec3f*, Vec3f*, Vec3f*) {
    ++criticalEffects;
}
extern "C" void Magic_Fill(PlayState*) {
}
extern "C" int32_t CVarGetInteger(const char*, int32_t) {
    return currentMenuAdjustable;
}
struct ItemTrackerItem {
    uint32_t id;
};
namespace GameInteractor {
bool IsSaveLoaded() {
    return true;
}
} // namespace GameInteractor

static bool Dispatch(int id, ...);
static bool NEI_PlayerDamageBoostActive() {
    return false;
}
// This stat-only fixture has no active fire sword; its collision integration is
// exercised separately against real code by din_fire_sword_damage_test.c.
static uint8_t DinFireSword_DamageEntry(PlayState*, Actor*, const ColliderInfo*, uint32_t, uint8_t vanillaEntry) {
    return vanillaEntry;
}
#define GameInteractor_Should(id, initial, ...) Dispatch(id, __VA_ARGS__)
#include "stat_upgrade_production.inc"

static bool Dispatch(int id, ...) {
    va_list args;
    va_start(args, id);
    bool should = true;
    hooks.at(id)(&should, args);
    va_end(args);
    return should;
}

int main() {
    // These IDs are already stored in integration saves and seed data.
    CHECK(RG_SEASON_WINTER == 553);
    CHECK(RSK_CROSSOVER_MARIO_MASK == 304);
    CHECK(RHT_GANONS_CASTLE_RED_ICE == 1638);
    RegisterRandoStatUpgradeHooks();
    auto& options = Rando::Context::GetInstance()->options;
    options[RSK_SPEED_UPGRADE].value = 1;
    options[RSK_DEFENSE_UPGRADE].value = 1;
    options[RSK_POWER_UPGRADE].value = 1;
    options[RSK_MAGIC_STAT_UPGRADE].value = 1;
    options[RSK_CRAWL_SPEED_UPGRADE].value = 1;
    options[RSK_CLIMB_SPEED_UPGRADE].value = 1;
    options[RSK_PUSH_SPEED_UPGRADE].value = 1;
    options[RSK_QUARTER_HEART].value = 1;
    auto& stats = gSaveContext.ship.quest.data.randomizer;
    CHECK(MagicStatLogicThreshold() == 2);
    CHECK(StatUpgradeRequired(5, RSK_SPEED_UPGRADE_ADJUSTABLE, RSK_SPEED_UPGRADE_TOTAL, RSK_SPEED_UPGRADE_REQUIRED) ==
          5);
    Player player{};
    player.actor.bgCheckFlags = BGCHECKFLAG_GROUND;
    f32 speed = 10;
    Dispatch(VB_PLAYER_SPEED_MULTIPLIER, &player, &speed);
    CHECK(speed == 10);
    for (int i = 0; i < 8; ++i) {
        GiveStat(nullptr, RG_SPEED_UPGRADE);
        GiveStat(nullptr, RG_DEFENSE_UPGRADE);
        GiveStat(nullptr, RG_POWER_UPGRADE);
        GiveStat(nullptr, RG_CRAWL_SPEED_UPGRADE);
        GiveStat(nullptr, RG_CLIMB_SPEED_UPGRADE);
        GiveStat(nullptr, RG_PUSH_SPEED_UPGRADE);
    }
    CHECK(stats.speedUpgrades == 5 && stats.defenseUpgrades == 5 && stats.powerUpgrades == 5);
    CHECK(stats.crawlSpeedUpgrades == 5 && stats.climbSpeedUpgrades == 5 && stats.pushSpeedUpgrades == 5);
    CHECK(GetCrawlStatValue() == 5 && GetClimbStatValue() == 5 && GetPushStatValue() == 5);
    Dispatch(VB_PLAYER_SPEED_MULTIPLIER, &player, &speed);
    CHECK(std::fabs(speed - 14) < .001f);
    player.stateFlags1 = PLAYER_STATE1_HOSTILE_LOCK_ON;
    speed = 10;
    Dispatch(VB_PLAYER_SPEED_MULTIPLIER, &player, &speed);
    CHECK(speed == 10);
    player.stateFlags1 = PLAYER_STATE1_IN_WATER;
    player.actor.bgCheckFlags = 0;
    Dispatch(VB_PLAYER_SPEED_MULTIPLIER, &player, &speed);
    CHECK(std::fabs(speed - 14) < .001f);
    player.stateFlags1 = 0;
    speed = 10;
    Dispatch(VB_PLAYER_SPEED_MULTIPLIER, &player, &speed);
    CHECK(speed == 10);
    s16 damage = -16;
    Dispatch(VB_PLAYER_INCOMING_DAMAGE, static_cast<PlayState*>(nullptr), &damage);
    CHECK(damage == -8);
    damage = 16;
    Dispatch(VB_PLAYER_INCOMING_DAMAGE, static_cast<PlayState*>(nullptr), &damage);
    CHECK(damage == 16);
    Actor enemy{};
    enemy.category = ACTORCAT_ENEMY;
    u8 attack = 200;
    Dispatch(VB_PLAYER_ATTACK_DAMAGE_MULTIPLIER, static_cast<PlayState*>(nullptr), &attack, &enemy);
    CHECK(attack == 255);
    enemy.category = ACTORCAT_PLAYER;
    attack = 4;
    Dispatch(VB_PLAYER_ATTACK_DAMAGE_MULTIPLIER, static_cast<PlayState*>(nullptr), &attack, &enemy);
    CHECK(attack == 4);
    enemy.category = ACTORCAT_ENEMY;
    stats.powerUpgrades = 1;
    randomRoll = .9f;
    Dispatch(VB_PLAYER_ATTACK_DAMAGE_MULTIPLIER, static_cast<PlayState*>(nullptr), &attack, &enemy);
    CHECK(attack == 4);
    randomRoll = .1f;
    Dispatch(VB_PLAYER_ATTACK_DAMAGE_MULTIPLIER, static_cast<PlayState*>(nullptr), &attack, &enemy);
    CHECK(attack == 8 && criticalEffects == 1);
    for (int i = 1; i <= 8; ++i) {
        GiveStat(nullptr, RG_MAGIC_STAT_UPGRADE);
        CHECK(gSaveContext.magicFillTarget == i * 12);
    }
    GiveStat(nullptr, RG_MAGIC_STAT_UPGRADE);
    CHECK(stats.magicStatUpgrades == 8 && gSaveContext.magicFillTarget == 96);
    // The loaded seed owns its magic settings, even when the next-seed menu differs.
    stats.magicStatUpgrades = 0;
    options[RSK_MAGIC_STAT_UPGRADE_ADJUSTABLE].value = 1;
    options[RSK_MAGIC_STAT_UPGRADE_TOTAL].value = 4;
    options[RSK_MAGIC_STAT_UPGRADE_REQUIRED].value = 39;
    currentMenuAdjustable = 0;
    GiveStat(nullptr, RG_MAGIC_STAT_UPGRADE);
    CHECK(gSaveContext.magicFillTarget == 50);
    s16 fill = 0;
    CHECK(!Dispatch(VB_MAGIC_FILL_TARGET, &fill));
    CHECK(fill == 50);
    CHECK(MagicStatLogicThreshold() == 1);
    gSaveContext.healthCapacity = 48;
    gSaveContext.health = 48;
    GiveStat(nullptr, RG_QUARTER_HEART);
    CHECK(gSaveContext.healthCapacity == 52 && gSaveContext.health == 52 && stats.quarterHearts == 1);
    s16 hearts = 3;
    Dispatch(VB_HEART_DISPLAY_TOTAL_COUNT, &hearts);
    CHECK(hearts == 4);
    SaveContext logicSave{};
    logicSave.healthCapacity = 48;
    ApplyHealthStat(&logicSave, RG_QUARTER_HEART, true);
    CHECK(logicSave.healthCapacity == 52);
    ApplyHealthStat(&logicSave, RG_QUARTER_HEART, false);
    CHECK(logicSave.healthCapacity == 48);
    for (uint32_t id = RG_QUARTER_HEART; id <= RG_PUSH_SPEED_UPGRADE; ++id) {
        CHECK(DrawItemInventory({ id }) == ITEM_NONE);
        CHECK(DrawItemCountInventory({ id }) == ITEM_NONE);
    }
    gSaveContext.inventory.items[SLOT_BOW] = ITEM_BOW;
    CHECK(DrawItemInventory({ ITEM_BOW }) == ITEM_BOW);
    CHECK(DrawItemCountInventory({ ITEM_BOW }) == ITEM_BOW);
    options[RSK_MAGIC_STAT_UPGRADE_ADJUSTABLE].value = 0;
    stats.magicStatUpgrades = 1;
    gSaveContext.magicLevel = 1;
    gSaveContext.isMagicAcquired = 1;
    CHECK(!HasLogicalMagic());
    stats.magicStatUpgrades = 2;
    CHECK(HasLogicalMagic());
    options[RSK_MAGIC_STAT_UPGRADE].value = 0;
    CHECK(HasLogicalMagic());
    gSaveContext.magicLevel = 0;
    gSaveContext.isMagicAcquired = 0;
    CHECK(!HasLogicalMagic());
    options[RSK_PUSH_SPEED_UPGRADE].value = 0;
    currentMenuAdjustable = 1;
    CHECK(CrateAcceleration() == .5f);
    CHECK(CrateDelay() == 9 && BlockDelay() == 9);
    CHECK(StatueDelay() == 5 && RockDelay() == 5 && PuzzleDelay() == 5);
    options[RSK_PUSH_SPEED_UPGRADE].value = 1;
    stats.pushSpeedUpgrades = 1;
    CHECK(CrateAcceleration() == .75f);
    CHECK(CrateDelay() == 8 && BlockDelay() == 8);
    CHECK(StatueDelay() == 4 && RockDelay() == 4 && PuzzleDelay() == 4);
    stats.powerUpgrades = 5;
    Actor target{};
    target.category = ACTORCAT_ENEMY;
    Collider collider{};
    collider.actor = &target;
    collider.acFlags = AC_HIT;
    ColliderInfo touch{}, hit{};
    touch.toucher.damage = 1;
    hit.bumperFlags = BUMP_HIT;
    hit.acHitInfo = &touch;
    CollisionCheck_ApplyDamage(nullptr, nullptr, &collider, &hit);
    CollisionCheck_ApplyDamage(nullptr, nullptr, &collider, &hit);
    CHECK(target.colChkInfo.damage == 4);
    collider.acFlags |= AC_HARD;
    CollisionCheck_ApplyDamage(nullptr, nullptr, &collider, &hit);
    CHECK(target.colChkInfo.damage == 4);
    ResetNewGameStats();
    CHECK(stats.quarterHearts == 0 && stats.defenseUpgrades == 0 && stats.speedUpgrades == 0);
    CHECK(stats.powerUpgrades == 0 && stats.magicStatUpgrades == 0 && stats.crawlSpeedUpgrades == 0);
    CHECK(stats.climbSpeedUpgrades == 0 && stats.pushSpeedUpgrades == 0);
    isRando = false;
    player.actor.bgCheckFlags = BGCHECKFLAG_GROUND;
    speed = 10;
    Dispatch(VB_PLAYER_SPEED_MULTIPLIER, &player, &speed);
    CHECK(speed == 10 && !IsCrawlStatActive() && !IsClimbStatActive() && !IsPushStatActive());
    if (failures)
        return 1;
    std::puts("PASS: stat pickups, caps, saved-seed settings, damage, movement guards, magic, quarter hearts, and "
              "existing IDs");
}
