#pragma once

#include <nlohmann/json.hpp>

// Everything the cross-game hint composer needs from Ship: hint options, stones, areas and hint texts.
// Reads the prepared seed, so it runs after the seed dump of the same attempt.
nlohmann::json FleetHints_DumpHintData();
