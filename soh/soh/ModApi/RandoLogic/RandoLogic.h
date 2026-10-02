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

#ifdef __cplusplus
extern "C" {
#endif

bool RandoLogic_GrantCapability(const char* owner, const char* capability, SOHLogicCondition condition, void* userData);
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
bool RandoLogic_IsCapabilityGranted(RandoLogicCapability capability);
bool RandoLogic_ResolveLocation(int32_t check, bool vanillaResult);
bool RandoLogic_ResolveEntrance(int32_t fromRegion, int32_t toRegion, bool vanillaResult);
bool RandoLogic_ResolveEvent(int32_t logicEvent, bool vanillaResult);

#ifdef __cplusplus
}

void RandoLogic_IndexWorld();
#endif

#define LOGIC_MOD_GRANTS(name)                                                          \
    if (gRandoLogicHasCapabilityGrants && RandoLogic_IsCapabilityGranted(RLC_##name)) { \
        return true;                                                                    \
    }

#endif
