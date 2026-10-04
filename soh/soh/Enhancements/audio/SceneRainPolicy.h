#pragma once

#include <string>

struct SceneRainPolicy {
    int density = 0;
    bool thunder = false;
    bool diagnostics = false;
    bool valid = false;
};

inline constexpr const char* kSceneRainPolicyPath = "custom/prelude/spot10_scene/weather.json";
SceneRainPolicy ParseSceneRainPolicy(const std::string& bytes, int scene, int setup);

struct PlayState;
namespace Ship {
class ArchiveManager;
}
void InitSceneRainPolicy(PlayState* play, Ship::ArchiveManager* archives, int setup, int age);
