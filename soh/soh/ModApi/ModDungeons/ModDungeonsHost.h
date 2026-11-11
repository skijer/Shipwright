#ifndef SOH_MOD_API_MOD_DUNGEONS_HOST_H
#define SOH_MOD_API_MOD_DUNGEONS_HOST_H

#include <string>

#include <nlohmann/json.hpp>

void BossRegistry_Init();
void BossRegistry_SetCurrentMod(const std::string& name);
// Draws the seed's custom bosses with the seeded RNG; call after Random_Init and before Fill.
void BossRegistry_AssignForSeed();
// A new randomizer file keeps the seed's bosses for the whole run.
void BossRegistry_InitSaveFile();
void BossRegistry_WriteSpoiler(nlohmann::ordered_json& spoiler);
void BossRegistry_ParseSpoiler(const nlohmann::json& spoiler);

#endif
