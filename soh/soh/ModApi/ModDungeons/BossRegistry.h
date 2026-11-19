#ifndef SOH_MOD_API_BOSS_REGISTRY_H
#define SOH_MOD_API_BOSS_REGISTRY_H

#include <stdbool.h>
#include <stdint.h>

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
#define SOH_BOSS_SLOTS_CHILD                                                                  \
    (SOH_BOSS_SLOT_BIT(SOH_BOSS_SLOT_GOHMA) | SOH_BOSS_SLOT_BIT(SOH_BOSS_SLOT_KING_DODONGO) | \
     SOH_BOSS_SLOT_BIT(SOH_BOSS_SLOT_BARINADE))
#define SOH_BOSS_SLOTS_ADULT                                                                      \
    (SOH_BOSS_SLOT_BIT(SOH_BOSS_SLOT_PHANTOM_GANON) | SOH_BOSS_SLOT_BIT(SOH_BOSS_SLOT_VOLVAGIA) | \
     SOH_BOSS_SLOT_BIT(SOH_BOSS_SLOT_MORPHA) | SOH_BOSS_SLOT_BIT(SOH_BOSS_SLOT_BONGO_BONGO) |     \
     SOH_BOSS_SLOT_BIT(SOH_BOSS_SLOT_TWINROVA))
#define SOH_BOSS_SLOTS_DUNGEON (SOH_BOSS_SLOTS_CHILD | SOH_BOSS_SLOTS_ADULT)
#define SOH_BOSS_SLOTS_FINAL (SOH_BOSS_SLOT_BIT(SOH_BOSS_SLOT_GANONDORF) | SOH_BOSS_SLOT_BIT(SOH_BOSS_SLOT_GANON))

typedef struct {
    uint32_t structSize;
    const char* key;
    const char* displayName;
    const char* scene;
    uint32_t allowedSlots;
    int32_t preferredSlot;
    const char* logic;
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
int32_t BossRegistry_ResolveSceneSource(int32_t sceneNum);

#ifdef __cplusplus
}
#endif

#endif
