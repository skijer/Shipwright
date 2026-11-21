#pragma once

#ifdef __cplusplus
#include <string>
#include <nlohmann/json.hpp>

std::string FleetForeignItems_Key(const nlohmann::json& foreign);
std::string FleetForeignItems_KeyForCheck(const nlohmann::json& seed, const std::string& check);
void FleetForeignItems_Register(const nlohmann::json& seed);

extern "C" {
#endif

const char* FleetForeignItems_FindIconOwner(const char* iconPath);

#ifdef __cplusplus
}
#endif
