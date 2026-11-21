#pragma once

// A mod with a boss-breaking attack publishes SOHBossSuperAttack under this name; the reworked bosses ask it whether
// a hit paralyzes or damages them. With no provider every query is false and the bosses stay vanilla.
#define SOH_BOSS_SUPER_ATTACK_SERVICE "boss.super_attack"

#include <stdbool.h>
#include <stdint.h>

#include "z64.h"

typedef struct {
    bool (*isStriking)(PlayState* play);
    bool (*isFormActive)(PlayState* play);
    bool (*reaches)(PlayState* play, Vec3f* targetPos, f32 range);
    f32 (*getReach)(PlayState* play);
    u8 (*getDamage)(PlayState* play);
} SOHBossSuperAttack;

#ifdef __cplusplus
extern "C" {
#endif

u8 BossSuperDamage_IsActive(PlayState* play);
u8 BossSuperDamage_IsFormActive(PlayState* play);
u8 BossSuperDamage_FormAttackReaches(PlayState* play, Vec3f* targetPos, f32 range);
f32 BossSuperDamage_FormAttackRange(PlayState* play);
u8 BossSuperDamage_FormDamage(PlayState* play);

void BossSuperDamage_StartElectricSparks(Actor* boss, s16 durationFrames);
void BossSuperDamage_DrawElectricSparks(Actor* boss, PlayState* play, Vec3f* limbsPos, s32 limbCount, f32 scale);
void BossSuperDamage_DrawGlowFromSpheres(Actor* boss, PlayState* play, ColliderJntSph* collider, s32 sphereCount,
                                         f32 scale);

#ifdef __cplusplus
}
#endif
