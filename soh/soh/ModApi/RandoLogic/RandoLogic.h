#ifndef SOH_MOD_API_RANDO_LOGIC_H
#define SOH_MOD_API_RANDO_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#define RANDO_LOGIC_CAPABILITIES(X) \
    X(BlastOrSmash)                 \
    X(HasExplosives)                \
    X(CanCutShrubs)                 \
    X(CanBonkTrees)                 \
    X(CanBreakPots)                 \
    X(CanBreakCrates)               \
    X(CanBreakSmallCrates)          \
    X(CanBreakMudWalls)             \
    X(CanBreakUpperBeehives)        \
    X(CanBreakLowerBeehives)        \
    X(CanOpenBombGrotto)            \
    X(CanOpenStormsGrotto)          \
    X(HasFireSource)                \
    X(HasFireSourceWithTorch)       \
    X(BlueFire)                     \
    X(CanClimbLadder)               \
    X(CanClimbHighLadder)           \
    X(CanHitSwitch)                 \
    X(CanHitEyeTargets)             \
    X(CanDetonateBombFlowers)       \
    X(CanStunDeku)                  \
    X(CanReflectNuts)               \
    X(CanJumpslash)                 \
    X(CanUseSword)                  \
    X(CanAttack)                    \
    X(CanDamage)                    \
    X(CanShield)                    \
    X(CanStandingShield)            \
    X(CanUseProjectile)             \
    X(CanClearStalagmite)           \
    X(HookshotOrBoomerang)          \
    X(CallGossipFairy)              \
    X(SummonEpona)                  \
    X(CanGetDekuBabaSticks)         \
    X(CanGetDekuBabaNuts)           \
    X(CanGetNightTimeGS)            \
    X(CanOpenUnderwaterChest)       \
    X(ScarecrowsSong)

typedef enum {
#define RANDO_LOGIC_CAPABILITY_ID(name) RLC_##name,
    RANDO_LOGIC_CAPABILITIES(RANDO_LOGIC_CAPABILITY_ID)
#undef RANDO_LOGIC_CAPABILITY_ID
        RLC_MAX,
} RandoLogicCapability;

typedef enum {
    SOH_LOGIC_ALLOW,
    SOH_LOGIC_REQUIRE,
    SOH_LOGIC_DENY,
    SOH_LOGIC_REPLACE,
} SOHLogicEffect;

#define SOH_LOGIC_TARGET_LOCATIONS (1 << 0)
#define SOH_LOGIC_TARGET_ENTRANCES (1 << 1)
#define SOH_LOGIC_TARGET_EVENTS (1 << 2)
#define SOH_LOGIC_TARGET_ALL 0
#define SOH_LOGIC_ANY (-1)

typedef struct {
    uint32_t structSize;
    uint32_t targets;
    const char* capability;
    const char* conditionText;
    int32_t checkType;
    int32_t region;
    int32_t scene;
    int32_t entranceTo;
    int32_t logicEvent;
    const int32_t* checks;
    uint32_t checkCount;
} SOHLogicSelector;

typedef bool (*SOHLogicCondition)(void* userData);

typedef struct {
    uint32_t structSize;
    const char* name;
    SOHLogicEffect effect;
    SOHLogicCondition condition;
    void* userData;
    const SOHLogicSelector* include;
    uint32_t includeCount;
    const SOHLogicSelector* exclude;
    uint32_t excludeCount;
} SOHLogicRule;

typedef enum {
    SOH_LOGIC_GRANT_CAPABILITY,
    SOH_LOGIC_GRANT_ITEM,
    SOH_LOGIC_GRANT_ENEMY_KILL,
} SOHLogicGrantKind;

// While condition holds, a capability, a vanilla item (value = RandomizerGet, for HasItem and CanUse) or the kill
// of an enemy (value = RandomizerEnemy) counts as had. include/exclude scope it like a rule's selectors; no
// include means everywhere. A check's scope knows its scene and type, not its region.
typedef struct {
    uint32_t structSize;
    const char* owner;
    SOHLogicGrantKind kind;
    const char* capability;
    int32_t value;
    SOHLogicCondition condition;
    void* userData;
    const SOHLogicSelector* include;
    uint32_t includeCount;
    const SOHLogicSelector* exclude;
    uint32_t excludeCount;
} SOHLogicGrant;

#ifdef __cplusplus
extern "C" {
#endif

bool RandoLogic_GrantCapability(const char* owner, const char* capability, SOHLogicCondition condition, void* userData);
bool RandoLogic_Grant(const SOHLogicGrant* grant);
bool RandoLogic_RegisterRule(const SOHLogicRule* rule);
void RandoLogic_RemoveOwner(const char* name);

bool RandoLogic_CanUseItem(int32_t randoGet);
bool RandoLogic_HasLogicItem(int32_t randoGet);
bool RandoLogic_IsChild(void);
bool RandoLogic_IsAdult(void);
bool RandoLogic_IsAtDay(void);
bool RandoLogic_IsAtNight(void);
bool RandoLogic_HasCapability(const char* capability);

extern bool gRandoLogicHasCapabilityGrants;
extern bool gRandoLogicHasItemGrants;
extern bool gRandoLogicHasEnemyGrants;
bool RandoLogic_IsCapabilityGranted(RandoLogicCapability capability);
bool RandoLogic_IsItemGranted(int32_t randoGet);
bool RandoLogic_IsEnemyKillGranted(int32_t enemy);

#ifdef __cplusplus
}

#include <functional>
#include <string>

void RandoLogic_IndexWorld();
// Each evaluates the vanilla condition with that check, entrance or event as the scope of the grants, then
// applies the rules registered for it.
bool RandoLogic_EvaluateLocation(int32_t check, const std::string& condition, const std::function<bool()>& evaluate);
bool RandoLogic_EvaluateEntrance(int32_t fromRegion, int32_t toRegion, const std::string& condition,
                                 const std::function<bool()>& evaluate);
bool RandoLogic_EvaluateEvent(int32_t logicEvent, const std::string& condition, const std::function<bool()>& evaluate);
#endif

#define LOGIC_MOD_GRANTS(name)                                                          \
    if (gRandoLogicHasCapabilityGrants && RandoLogic_IsCapabilityGranted(RLC_##name)) { \
        return true;                                                                    \
    }

#define LOGIC_MOD_ITEM_GRANTS(item)                                                         \
    if (gRandoLogicHasItemGrants && RandoLogic_IsItemGranted(static_cast<int32_t>(item))) { \
        return true;                                                                        \
    }

#define LOGIC_MOD_ENEMY_GRANTS(enemy)                                                              \
    if (gRandoLogicHasEnemyGrants && RandoLogic_IsEnemyKillGranted(static_cast<int32_t>(enemy))) { \
        return true;                                                                               \
    }

#endif
