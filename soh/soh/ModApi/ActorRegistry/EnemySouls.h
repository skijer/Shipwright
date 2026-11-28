#ifndef SOH_MOD_API_ENEMY_SOULS_H
#define SOH_MOD_API_ENEMY_SOULS_H

#include <string>

#include <nlohmann/json.hpp>

#include "ActorRegistry.h"

// Fills the enemy's soul category and drop check, creating the soul item and drop check the definition asks for.
void EnemySouls_OnEnemyRegistered(const SOHActorDefinition* definition, SOHCustomEnemy& enemy);
bool EnemySouls_HasBosses();
std::string EnemySouls_DescribeNativeSoul(const char* soulKey, const std::string& name);
std::string EnemySouls_DescribeCustomSoul(const char* soulKey, const std::string& fallbackName);
void EnemySouls_WriteSpoiler(nlohmann::ordered_json& spoiler);

#endif
