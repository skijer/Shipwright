#include "SceneRainPolicy.h"
#include <nlohmann/json.hpp>

SceneRainPolicy ParseSceneRainPolicy(const std::string& bytes, int scene, int setup) {
    // This probe deliberately supports just the authored Lost Woods and its
    // ordinary child/adult setups. No accidental cutscene or vanilla opt-in.
    if (scene != 0x5B || setup < 0 || setup > 3 || bytes.size() > 8192)
        return {};
    const auto json = nlohmann::json::parse(bytes, nullptr, false);
    if (!json.is_object() || !json.contains("version") || !json["version"].is_number_integer() ||
        json["version"] != 1 || !json.contains("scene") || json["scene"] != "spot10_scene" || !json.contains("mode") ||
        json["mode"] != "continuous" || !json.contains("density") || !json["density"].is_number_integer() ||
        json["density"] < 1 || json["density"] > 64 || !json.contains("thunder") || !json["thunder"].is_boolean() ||
        !json.contains("diagnostics") || !json["diagnostics"].is_boolean())
        return {};
    return { json["density"].get<int>(), json["thunder"].get<bool>(), json["diagnostics"].get<bool>(), true };
}

#ifndef SCENE_RAIN_POLICY_TEST
#include "GlobalOutdoorRainBridge.h"
#include "soh/resource/type/Scene.h"
#include <ship/resource/archive/ArchiveManager.h>
#include <ship/resource/archive/Archive.h>
#include <spdlog/spdlog.h>
#include "z64.h"

void InitSceneRainPolicy(PlayState* play, Ship::ArchiveManager* archives, int setup, int age) {
    SceneRainPolicy policy;
    std::string archivePath;
    std::string resourcePath;
    if (play->sceneNum == SCENE_LOST_WOODS && play->sceneSegment != nullptr) {
        const auto initData = static_cast<SOH::Scene*>(play->sceneSegment)->GetInitData();
        if (initData) {
            resourcePath = initData->Path;
            // Like Prelude material recipes, bind to the exact loaded resource
            // (including its alt/ prefix), never to a global weather.json lookup.
            const auto archive = initData->Parent ? initData->Parent : archives->GetArchiveFromFile(resourcePath);
            if (archive) {
                archivePath = archive->GetPath();
                if (archive->HasFile(kSceneRainPolicyPath)) {
                    const auto file = archive->LoadFile(kSceneRainPolicyPath);
                    if (file && file->Buffer && file->Buffer->size() <= 8192) {
                        policy = ParseSceneRainPolicy(std::string(file->Buffer->begin(), file->Buffer->end()),
                                                      play->sceneNum, setup);
                    }
                    if (!policy.valid)
                        SPDLOG_WARN("[rain-probe] rejected policy archive={} setup={}", archivePath, setup);
                }
            }
        }
        SPDLOG_INFO("[rain-probe] bind resource={} archive={} setup={} age={} density={} thunder={} diagnostics={}",
                    resourcePath, archivePath, setup, age, policy.density, policy.thunder, policy.diagnostics);
    }
    GlobalOutdoorRain_BeginScene(play, policy.density, policy.thunder, policy.diagnostics);
}
#endif
