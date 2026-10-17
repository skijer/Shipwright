#ifndef MM_COLLECTABLES_REMAINS_H
#define MM_COLLECTABLES_REMAINS_H

// The boss remains worn on Link's face (remains.c) and the friendly allies they summon (remains_allies.c),
// ported from NEI's boss_remains for Ship.
#include "soh/ModApi/ModApi.h"

typedef enum {
    REMAINS_NONE = -1,
    REMAINS_ODOLWA,
    REMAINS_GOHT,
    REMAINS_GYORG,
    REMAINS_TWINMOLD,
    REMAINS_COUNT,
} RemainsId;

s32 Remains_GetWorn(void);
void Remains_Init(const SOHModApi* api);
void Remains_Toggle(PlayState* play, Player* player, s32 remains);
void Remains_Forget(s32 remains);
void Remains_ResetFile(void);
bool Remains_IsOdolwaFlying(void);

void Remains_PlayMmSfx(u16 sfxId, Vec3f* pos);
void Remains_StopMmSfx(u16 sfxId);

bool RemainsAllies_Register(const SOHModApi* api);
Actor* RemainsAlly_FindNearestEnemy(PlayState* play, Vec3f* from, f32 maxDist);
Actor* RemainsAllyBug_Spawn(PlayState* play, Vec3f* pos, s16 rotY);
Actor* RemainsAllyBug_SpawnCloud(PlayState* play, Vec3f* pos, s16 rotY);
Actor* RemainsAllyBug_SpawnProjectile(PlayState* play, Vec3f* pos, s16 rotY);
Actor* RemainsAllyBug_SpawnThunderCharged(PlayState* play, Vec3f* pos, s16 rotY, s16 ttl, s16 damage);
Actor* RemainsAllyChu_Spawn(PlayState* play, Vec3f* pos, s16 rotY);
bool RemainsAllyChu_IsAlive(void);
void RemainsAllyChu_DetonateActive(PlayState* play);
Actor* RemainsAllyFish_Spawn(PlayState* play, Vec3f* pos, s16 rotY, s16 index);

#endif
