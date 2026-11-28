#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/ModApi/ActorRegistry/ActorRegistry.h"

#define ENEMY_SPAWN_ROOM_CLEAR_TRIGGER (1 << 0)
#define ENEMY_SPAWN_ROOM_HAS_WATER (1 << 1)
#define ENEMY_SPAWN_ROOM_LOW_CEILING (1 << 2)
#define ENEMY_SPAWN_ROW_SUBMERGED (1 << 3)
#define ENEMY_SPAWN_ROW_SURFACE (1 << 4)
#define ENEMY_SPAWN_ROW_MQ (1 << 5)

// Row layout of StaticData/EnemySpawns.inc (contract section 6).
struct EnemySpawnRow {
    int32_t sceneId;
    int32_t setupIndex;
    int32_t roomIndex;
    int32_t actorIndex;
    int32_t actorId;
    int32_t params;
    int32_t posX;
    int32_t posY;
    int32_t posZ;
    int32_t rotY;
    uint32_t roomFlags;
};

constexpr int32_t kNoLogicEnemy = -1;

enum EnemyMinibossMode {
    ENEMY_MINIBOSSES_VANILLA,
    ENEMY_MINIBOSSES_AMONG_THEMSELVES,
    ENEMY_MINIBOSSES_ANYWHERE,
};

enum EnemySoulBehaviour {
    ENEMY_SOUL_INVULNERABLE,
    ENEMY_SOUL_NO_SPAWN,
};

// One enemy the randomizer knows, native or registered by a mod.
struct EnemyKind {
    const char* key;
    const char* name;
    int16_t actorId;
    int16_t params;
    int16_t spawnHeight;
    uint32_t flags;
    int32_t logicEnemy;
    const char* soulKey;
    uint8_t soulCategory;
    SOHLogicKillFunc logicKill;
    void* logicKillUserData;
    const char* enhancementCvar;
    bool spawnable;
    bool custom;
};

struct EnemySoul {
    const char* key;
    const char* name;
    RandomizerGet item;
    RandomizerInf flag;
};

struct EnemyPlacement {
    std::map<int32_t, std::map<std::string, std::string>> replacements;
    std::set<std::string> soulPoolExcluded;
};

struct PlannedReplacement {
    EnemyKind kind;
    bool onlySubmerged;
};

struct EnemyLogicPlan {
    std::vector<PlannedReplacement> replacements;
    bool keepsOriginal;
};

const std::vector<EnemyKind>& EnemyCatalog_GetNatives();
std::vector<EnemyKind> EnemyCatalog_GetCustoms();
std::vector<EnemyKind> EnemyCatalog_GetPlaceableCustoms();
std::optional<EnemyKind> EnemyCatalog_Find(const std::string& key);
std::optional<EnemyKind> EnemyCatalog_Classify(int16_t sceneNum, int16_t actorId, int16_t params);
const std::vector<EnemySoul>& EnemyCatalog_GetNativeSouls();
const EnemySoul* EnemyCatalog_FindNativeSoul(const std::string& soulKey);
const EnemySoul* EnemyCatalog_FindNativeSoulByFlag(RandomizerInf flag);

bool EnemyRando_IsClearRoom(bool mq, int16_t sceneNum, int16_t roomNum);
bool EnemyRando_IsTimedRoom(bool mq, int16_t sceneNum, int16_t roomNum);
bool EnemyRando_IsRandomizableSpawn(int16_t sceneNum, int16_t roomNum, int16_t actorId, int16_t params, float posX,
                                    bool mq, bool isChild);
bool EnemyRando_IsAllowedAt(int16_t sceneNum, int16_t roomNum, bool mq, const EnemyKind& enemy, int16_t posY,
                            bool fromBari);

void EnemyPlacement_AssignForSeed(uint64_t seed);
void EnemyPlacement_WriteSpoiler(nlohmann::ordered_json& spoiler);
void EnemyPlacement_ParseSpoiler(const nlohmann::json& spoiler);
void EnemyPlacement_InitSaveFile();
const EnemyPlacement& EnemyPlacement_GetSeed();
const EnemyPlacement& EnemyPlacement_GetSave();
std::optional<EnemyKind> EnemyPlacement_FindSaveReplacement(int16_t sceneNum, const std::string& originalKey);
bool EnemyPlacement_IsSoulExcluded(const EnemyPlacement& placement, const std::string& soulKey);
bool EnemyPlacement_IsSeedSoulItemExcluded(RandomizerGet soul);
std::vector<RandomizerGet> EnemyPlacement_GetSeedCustomSoulItems(bool bossSouls);
const EnemyLogicPlan* EnemyPlacement_FindLogicPlan(int32_t sceneNum, RandomizerEnemy original);
void EnemyPlacement_DrawPoolMenu();
