#pragma once

#include <nlohmann/json.hpp>

// Reads the prepared seed, so it runs after the seed dump of the same attempt.
nlohmann::json FleetHints_DumpHintData();
