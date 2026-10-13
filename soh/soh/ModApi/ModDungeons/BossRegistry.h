#ifndef SOH_MOD_API_BOSS_REGISTRY_H
#define SOH_MOD_API_BOSS_REGISTRY_H

#include <stdbool.h>
#include <stdint.h>

// A custom boss takes over one vanilla boss slot: the slot's scene id loads the boss's scene, so the
// blue warp, heart container, boss soul, Boss Rush and both entrance shuffles keep treating it as the slot.
typedef enum {
    SOH_BOSS_SLOT_GOHMA,
    SOH_BOSS_SLOT_KING_DODONGO,
    SOH_BOSS_SLOT_BARINADE,
    SOH_BOSS_SLOT_PHANTOM_GANON,
    SOH_BOSS_SLOT_VOLVAGIA,
    SOH_BOSS_SLOT_MORPHA,
    SOH_BOSS_SLOT_BONGO_BONGO,
    SOH_BOSS_SLOT_TWINROVA,
    SOH_BOSS_SLOT_GANONDORF,
    SOH_BOSS_SLOT_GANON,
    SOH_BOSS_SLOT_MAX,
} SOHBossSlot;

#define SOH_BOSS_SLOT_BIT(slot) (1u << (slot))
#define SOH_BOSS_SLOTS_CHILD                                                       \
    (SOH_BOSS_SLOT_BIT(SOH_BOSS_SLOT_GOHMA) | SOH_BOSS_SLOT_BIT(SOH_BOSS_SLOT_KING_DODONGO) | \
     SOH_BOSS_SLOT_BIT(SOH_BOSS_SLOT_BARINADE))
#define SOH_BOSS_SLOTS_ADULT                                                                         \
    (SOH_BOSS_SLOT_BIT(SOH_BOSS_SLOT_PHANTOM_GANON) | SOH_BOSS_SLOT_BIT(SOH_BOSS_SLOT_VOLVAGIA) |     \
     SOH_BOSS_SLOT_BIT(SOH_BOSS_SLOT_MORPHA) | SOH_BOSS_SLOT_BIT(SOH_BOSS_SLOT_BONGO_BONGO) |         \
     SOH_BOSS_SLOT_BIT(SOH_BOSS_SLOT_TWINROVA))
#define SOH_BOSS_SLOTS_DUNGEON (SOH_BOSS_SLOTS_CHILD | SOH_BOSS_SLOTS_ADULT)
#define SOH_BOSS_SLOTS_FINAL (SOH_BOSS_SLOT_BIT(SOH_BOSS_SLOT_GANONDORF) | SOH_BOSS_SLOT_BIT(SOH_BOSS_SLOT_GANON))

typedef struct {
    uint32_t structSize;
    const char* key;
    const char* displayName;
    // SceneDB name of the arena; NULL fights in the slot's own scene.
    const char* scene;
    uint32_t allowedSlots;
    // Honoured outside the randomizer; a seed draws the slot from allowedSlots.
    int32_t preferredSlot;
    // Randomizer condition to win the fight (LogicExpr); NULL keeps the replaced boss's.
    const char* logic;
    // Optional CVar: 0 leaves every slot to its vanilla boss.
    const char* enabledCvar;
} SOHBossDefinition;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Actor Actor;

bool BossRegistry_Register(const SOHBossDefinition* definition);
int32_t BossRegistry_GetSlot(const char* key);
const char* BossRegistry_GetSceneBoss(int16_t sceneNum);
int16_t BossRegistry_GetSlotScene(int32_t slot);
void BossRegistry_ReportDefeated(Actor* boss);
// The scene whose resource loads for `sceneNum`: the arena of the boss that holds its slot, or itself.
int32_t BossRegistry_ResolveSceneSource(int32_t sceneNum);

#ifdef __cplusplus
}
#endif

#endif
