#include "test_require.h"
#include "soh/Enhancements/audio/SceneRainPolicy.h"
#include <nlohmann/json.hpp>
int main() {
    nlohmann::json policy = { { "version", 1 },  { "scene", "spot10_scene" }, { "mode", "continuous" },
                              { "density", 25 }, { "thunder", true },         { "diagnostics", true } };
    for (int setup = 0; setup < 4; ++setup) {
        const auto result = ParseSceneRainPolicy(policy.dump(), 0x5B, setup);
        REQUIRE(result.valid && result.density == 25 && result.thunder && result.diagnostics);
    }
    for (int scene : { 0, 0x51, 0x5A, 0x5C })
        REQUIRE(!ParseSceneRainPolicy(policy.dump(), scene, 0).valid);
    for (int setup : { -1, 4, 5, 255 })
        REQUIRE(!ParseSceneRainPolicy(policy.dump(), 0x5B, setup).valid);
    for (const auto& bytes : { "", "{", "[]", "null", "25" })
        REQUIRE(!ParseSceneRainPolicy(bytes, 0x5B, 0).valid);
    for (auto key : { "version", "scene", "mode", "density", "thunder", "diagnostics" }) {
        auto invalid = policy;
        invalid.erase(key);
        REQUIRE(!ParseSceneRainPolicy(invalid.dump(), 0x5B, 0).valid);
        invalid = policy;
        invalid[key] = nullptr;
        REQUIRE(!ParseSceneRainPolicy(invalid.dump(), 0x5B, 0).valid);
    }
    for (auto density : { -1, 0, 65, 100000 }) {
        auto invalid = policy;
        invalid["density"] = density;
        REQUIRE(!ParseSceneRainPolicy(invalid.dump(), 0x5B, 0).valid);
    }
    auto invalid = policy;
    invalid["density"] = 25.5;
    REQUIRE(!ParseSceneRainPolicy(invalid.dump(), 0x5B, 0).valid);
    invalid = policy;
    invalid["version"] = 1.0;
    REQUIRE(!ParseSceneRainPolicy(invalid.dump(), 0x5B, 0).valid);
    invalid = policy;
    invalid["mode"] = "intermittent";
    REQUIRE(!ParseSceneRainPolicy(invalid.dump(), 0x5B, 0).valid);
    REQUIRE(!ParseSceneRainPolicy(std::string(8193, ' ') + policy.dump(), 0x5B, 0).valid);
}
