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
#define SOH_ACTOR_ENEMY_MINIBOSS (1 << 3)
#define SOH_ACTOR_ENEMY_NEEDS_WATER (1 << 4)
#define SOH_ACTOR_ENEMY_NOT_IN_WATER (1 << 5)
#define SOH_ACTOR_ENEMY_NEEDS_CEILING (1 << 6)
#define SOH_ACTOR_ENEMY_UNDERWATER (1 << 7)

typedef enum {
    SOH_ENEMY_CATEGORY_REGULAR,
    SOH_ENEMY_CATEGORY_MINIBOSS,
    SOH_ENEMY_CATEGORY_BOSS,
} SOHEnemyCategory;

// The game whose vanilla roster has the family; AUTO reads the soul key's "oot." or "mm." prefix. Labels only.
typedef enum {
    SOH_ENEMY_ORIGIN_AUTO,
    SOH_ENEMY_ORIGIN_CUSTOM,
    SOH_ENEMY_ORIGIN_OOT,
    SOH_ENEMY_ORIGIN_MM,
} SOHEnemyOrigin;

// The enemy idling inside its soul flame. A null skeleton leaves the soul item to the mod.
typedef struct {
    const char* skeleton;
    const char* idleAnimation;
    uint8_t isFlex;
    uint8_t limbCount;
    float scale;
    float yOffset;
    uint8_t flameRed;
    uint8_t flameGreen;
    uint8_t flameBlue;
} SOHSoulModel;

typedef bool (*SOHLogicKillFunc)(void* userData);

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
    const char* soulKey;              /* NULL = this enemy has no soul */
    SOHLogicKillFunc logicKill;  /* NULL = always killable in logic */
    void* logicKillUserData;
    uint8_t enemyCategory;
    uint8_t enemyOrigin;
    uint8_t dropOnKill;
    SOHSoulModel soulModel;
    /* Non-zero: soulModel's skeleton and idleAnimation point at the mod's own C data, not resource paths */
    uint8_t isSoulModelInMemory;
} SOHActorDefinition;

#define SOH_ACTOR_DEFINITION_MIN_SIZE (offsetof(SOHActorDefinition, draw) + sizeof(((SOHActorDefinition*)0)->draw))
#define SOH_ACTOR_DEFINITION_ENEMY_SIZE \
    (offsetof(SOHActorDefinition, enemySpawnHeight) + sizeof(((SOHActorDefinition*)0)->enemySpawnHeight))
#define SOH_ACTOR_DEFINITION_HINT_SIZE \
    (offsetof(SOHActorDefinition, naviHint) + sizeof(((SOHActorDefinition*)0)->naviHint))
#define SOH_ACTOR_DEFINITION_SOUL_SIZE \
    (offsetof(SOHActorDefinition, logicKillUserData) + sizeof(((SOHActorDefinition*)0)->logicKillUserData))
#define SOH_ACTOR_DEFINITION_GENERIC_SOUL_SIZE     (offsetof(SOHActorDefinition, soulModel) + sizeof(((SOHActorDefinition*)0)->soulModel))
#define SOH_ACTOR_DEFINITION_SOUL_MEMORY_SIZE \
    (offsetof(SOHActorDefinition, isSoulModelInMemory) + sizeof(((SOHActorDefinition*)0)->isSoulModelInMemory))

typedef struct {
    const char* key;
    const char* name;
    int16_t actorId;
    int16_t params;
    uint32_t flags;
    int16_t spawnHeight;
    const char* soulKey;
    SOHLogicKillFunc logicKill;
    void* logicKillUserData;
    // The soul's category, set by the first definition naming the soul; it picks the soul shuffle option.
    uint8_t soulCategory;
    // The drop check registered for dropOnKill, or RC_UNKNOWN_CHECK.
    int32_t dropCheck;
} SOHCustomEnemy;

int16_t ActorRegistry_Register(const SOHActorDefinition* definition);
int16_t ActorRegistry_GetId(const char* key);
int16_t ActorRegistry_FindCustomId(const char* key);
uint32_t ActorRegistry_GetEnemyCount(void);
const SOHCustomEnemy* ActorRegistry_GetEnemyAt(uint32_t index);
const SOHCustomEnemy* ActorRegistry_FindEnemyById(int16_t actorId);
const SOHCustomEnemy* ActorRegistry_FindEnemyByKey(const char* key);

#ifdef __cplusplus
}

#include <string>
#include <vector>

struct SOHRegisteredActor {
    std::string key;
    std::string owner;
    int16_t actorId;
};

std::vector<SOHRegisteredActor> ActorRegistry_List();
#endif

#endif
