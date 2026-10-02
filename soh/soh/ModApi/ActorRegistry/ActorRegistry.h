#ifndef SOH_MOD_API_ACTOR_REGISTRY_H
#define SOH_MOD_API_ACTOR_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Actor;
struct PlayState;

typedef void (*SOHActorFunc)(struct Actor* actor, struct PlayState* play);

#define SOH_ACTOR_ENEMY (1 << 0)
#define SOH_ACTOR_ENEMY_NOT_IN_CLEAR_ROOMS (1 << 1)
#define SOH_ACTOR_ENEMY_NOT_IN_TIMED_ROOMS (1 << 2)

typedef struct {
    uint32_t structSize;
    const char* key;
    const char* description;
    uint8_t category;
    uint32_t actorFlags;
    int16_t objectId;
    uint32_t instanceSize;
    SOHActorFunc init;
    SOHActorFunc destroy;
    SOHActorFunc update;
    SOHActorFunc draw;
    uint32_t enemyFlags;
    int16_t enemyParams;
    int16_t enemySpawnHeight;
    const char* naviHint;
} SOHActorDefinition;

#define SOH_ACTOR_DEFINITION_MIN_SIZE (offsetof(SOHActorDefinition, draw) + sizeof(((SOHActorDefinition*)0)->draw))
#define SOH_ACTOR_DEFINITION_ENEMY_SIZE \
    (offsetof(SOHActorDefinition, enemySpawnHeight) + sizeof(((SOHActorDefinition*)0)->enemySpawnHeight))
#define SOH_ACTOR_DEFINITION_HINT_SIZE \
    (offsetof(SOHActorDefinition, naviHint) + sizeof(((SOHActorDefinition*)0)->naviHint))

typedef struct {
    const char* key;
    const char* name;
    int16_t actorId;
    int16_t params;
    uint32_t flags;
    int16_t spawnHeight;
} SOHCustomEnemy;

int16_t ActorRegistry_Register(const SOHActorDefinition* definition);
int16_t ActorRegistry_GetId(const char* key);
int16_t ActorRegistry_FindCustomId(const char* key);
uint32_t ActorRegistry_GetEnemyCount(void);
const SOHCustomEnemy* ActorRegistry_GetEnemyAt(uint32_t index);
const SOHCustomEnemy* ActorRegistry_FindEnemyById(int16_t actorId);

#ifdef __cplusplus
}
#endif

#endif
