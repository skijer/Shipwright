#include "ActorRegistry.h"

#include <algorithm>
#include <cstring>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "soh/ActorDB.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ModApi/ModMessages/ModMessages.h"

extern "C" {
#include "z64.h"
#include "macros.h"
}

namespace {

constexpr uint8_t NoNaviEnemyId = 0xFF;

std::unordered_map<std::string, int16_t> sCustomIds;
std::deque<std::string> sNames;
std::vector<SOHCustomEnemy> sEnemies;

const char* StoreName(const char* name) {
    return sNames.emplace_back(name).c_str();
}

void AddEnemy(const SOHActorDefinition* definition, int16_t actorId) {
    SOHCustomEnemy enemy = {};

    enemy.key = StoreName(definition->key);
    enemy.name = definition->description != nullptr ? StoreName(definition->description) : enemy.key;
    enemy.actorId = actorId;
    enemy.params = definition->enemyParams;
    enemy.flags = definition->enemyFlags;
    enemy.spawnHeight = definition->enemySpawnHeight;

    auto position = std::lower_bound(
        sEnemies.begin(), sEnemies.end(), enemy,
        [](const SOHCustomEnemy& left, const SOHCustomEnemy& right) { return strcmp(left.key, right.key) < 0; });
    sEnemies.insert(position, enemy);
}

bool IsEnemyDefinition(const SOHActorDefinition* definition) {
    return definition->structSize >= SOH_ACTOR_DEFINITION_ENEMY_SIZE && (definition->enemyFlags & SOH_ACTOR_ENEMY);
}

void AttachNaviHint(const SOHActorDefinition* definition, int16_t actorId) {
    if (definition->structSize < SOH_ACTOR_DEFINITION_HINT_SIZE || definition->naviHint == nullptr) {
        return;
    }
    uint8_t naviEnemyId = ModMessages_RegisterNaviHint(definition->key, definition->naviHint);

    if (naviEnemyId == NoNaviEnemyId) {
        return;
    }
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnActorInit>(actorId, [naviEnemyId](void* actor) {
        Actor* spawned = (Actor*)actor;

        if (spawned->naviEnemyId == NoNaviEnemyId) {
            spawned->naviEnemyId = naviEnemyId;
        }
    });
}

} // namespace

int16_t ActorRegistry_GetId(const char* key) {
    if (key == nullptr || ActorDB::Instance == nullptr) {
        return -1;
    }
    return (int16_t)ActorDB::Instance->RetrieveId(key);
}

int16_t ActorRegistry_FindCustomId(const char* key) {
    if (key == nullptr) {
        return -1;
    }
    auto entry = sCustomIds.find(key);
    return entry == sCustomIds.end() ? -1 : entry->second;
}

int16_t ActorRegistry_Register(const SOHActorDefinition* definition) {
    if (definition == nullptr || definition->key == nullptr || ActorDB::Instance == nullptr) {
        return -1;
    }
    if (definition->structSize < SOH_ACTOR_DEFINITION_MIN_SIZE) {
        return -1;
    }
    int16_t alreadyRegisteredId = ActorRegistry_GetId(definition->key);

    if (alreadyRegisteredId >= 0) {
        return alreadyRegisteredId;
    }

    ActorDBInit init;

    init.name = definition->key;
    init.desc = definition->description != nullptr ? definition->description : definition->key;
    init.category = definition->category;
    init.flags = definition->actorFlags;
    init.objectId = definition->objectId;
    init.instanceSize = definition->instanceSize != 0 ? definition->instanceSize : sizeof(Actor);
    init.init = definition->init;
    init.destroy = definition->destroy;
    init.update = definition->update;
    init.draw = definition->draw;
    init.reset = nullptr;

    int16_t actorId = (int16_t)ActorDB::Instance->AddEntry(init).entry.id;

    sCustomIds.emplace(definition->key, actorId);
    if (IsEnemyDefinition(definition)) {
        AddEnemy(definition, actorId);
    }
    AttachNaviHint(definition, actorId);
    return actorId;
}

uint32_t ActorRegistry_GetEnemyCount(void) {
    return (uint32_t)sEnemies.size();
}

const SOHCustomEnemy* ActorRegistry_GetEnemyAt(uint32_t index) {
    return index < sEnemies.size() ? &sEnemies[index] : nullptr;
}

const SOHCustomEnemy* ActorRegistry_FindEnemyById(int16_t actorId) {
    for (const SOHCustomEnemy& enemy : sEnemies) {
        if (enemy.actorId == actorId) {
            return &enemy;
        }
    }
    return nullptr;
}
