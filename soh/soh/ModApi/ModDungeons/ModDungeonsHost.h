#ifndef SOH_MOD_API_MOD_DUNGEONS_HOST_H
#define SOH_MOD_API_MOD_DUNGEONS_HOST_H

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

void BossRegistry_Init();
void BossRegistry_SetCurrentMod(const std::string& name);
// Call after Random_Init and before Fill.
void BossRegistry_AssignForSeed();
void BossRegistry_InitSaveFile();
// The souls of the custom bosses holding a slot in the seed being generated.
std::vector<std::string> BossRegistry_GetSeedSoulKeys();
void BossRegistry_WriteSpoiler(nlohmann::ordered_json& spoiler);
void BossRegistry_ParseSpoiler(const nlohmann::json& spoiler);

#endif
