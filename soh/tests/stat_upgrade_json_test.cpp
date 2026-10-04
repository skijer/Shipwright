#include <cstdio>
#include <nlohmann/json.hpp>
#include "z64.h"
using json = nlohmann::json;
#include "stat_upgrade_json_production.inc"

int main() {
    const json legacy = { { "triforcePiecesCollected", 7 }, { "bombchuUpgradeLevel", 2 } };
    const json current = { { "triforcePiecesCollected", 7 }, { "bombchuUpgradeLevel", 2 }, { "quarterHearts", 1 },
                           { "defenseUpgrades", 2 },         { "speedUpgrades", 3 },       { "powerUpgrades", 4 },
                           { "magicStatUpgrades", 5 },       { "crawlSpeedUpgrades", 6 },  { "climbSpeedUpgrades", 7 },
                           { "pushSpeedUpgrades", 8 } };
    ShipRandomizerSaveContextData data{};
    try {
        from_json(current, data);
        json result;
        to_json(result, data);
        if (result != current) {
            std::puts("FAIL: current Anchor stat snapshot changed on round trip");
            return 1;
        }
        from_json(legacy, data);
        to_json(result, data);
        for (const auto& [key, value] : current.items()) {
            if (result.at(key) != (legacy.contains(key) ? legacy.at(key) : json(0))) {
                std::printf("FAIL: legacy Anchor snapshot has incorrect %s\n", key.c_str());
                return 1;
            }
        }
    } catch (const json::exception& error) {
        std::printf("FAIL: supported Anchor snapshot rejected: %s\n", error.what());
        return 1;
    }
    std::puts("PASS: Anchor stat snapshots round-trip and older snapshots reset missing counters");
    return 0;
}
