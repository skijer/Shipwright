#pragma once

#include <string>
#include <nlohmann/json.hpp>

// The custom item that stands in a Ship check for a reward of the other game: one per distinct reward of the seed.
std::string FleetForeignItems_Key(const nlohmann::json& foreign);
// Empty when the seed places no foreign reward in `check`.
std::string FleetForeignItems_KeyForCheck(const nlohmann::json& seed, const std::string& check);
// Registers every reward the seed places in Ship's checks; repeated calls add nothing.
void FleetForeignItems_Register(const nlohmann::json& seed);
