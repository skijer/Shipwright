#pragma once

#include <cstdint>
#include <optional>
#include <string>

// The OoT entrance a passage arrives at, beside its hole; nullopt for an unknown passage.
std::optional<int32_t> FleetPassages_Entrance(const std::string& key);
