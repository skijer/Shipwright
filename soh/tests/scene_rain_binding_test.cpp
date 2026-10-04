// Archive IO and logger boundaries are fixtures; exercise the production loader
// and parser against physical normal/Alt scene identity and owning-archive rules.
#include "test_require.h"
#include "z64.h"
#include "soh/resource/type/Scene.h"
#include <ship/resource/archive/ArchiveManager.h>
#include "soh/Enhancements/audio/SceneRainPolicy.h"
#include "soh/Enhancements/audio/GlobalOutdoorRainBridge.h"
static int sDensity = -1, sThunder = -1, sDiagnostics = -1, sBegins = 0;
extern "C" void GlobalOutdoorRain_BeginScene(PlayState*, int density, int thunder, int diagnostics) {
    sDensity = density;
    sThunder = thunder;
    sDiagnostics = diagnostics;
    ++sBegins;
}
int main() {
    Ship::ArchiveManager archives;
    auto base = std::make_shared<Ship::Archive>("oot.o2r");
    auto custom = std::make_shared<Ship::Archive>("Lost_Woods_Rain_POC.o2r");
    auto unrelated = std::make_shared<Ship::Archive>("unrelated.o2r");
    const char* json =
        R"({"version":1,"scene":"spot10_scene","mode":"continuous","density":25,"thunder":true,"diagnostics":true})";
    custom->files[kSceneRainPolicyPath] = json;
    unrelated->files[kSceneRainPolicyPath] = json;
    archives.owners[kSceneRainPolicyPath] = unrelated;
    PlayState play = {};
    play.sceneNum = 0x5B;
    SOH::Scene scene;
    play.sceneSegment = &scene;
    for (const char* physical :
         { "scenes/shared/spot10_scene/spot10_scene", "alt/scenes/shared/spot10_scene/spot10_scene" }) {
        scene.data->Path = physical;
        archives.owners[physical] = custom;
        for (int setup = 0; setup < 4; ++setup) {
            InitSceneRainPolicy(&play, &archives, setup, setup < 2 ? 1 : 0);
            REQUIRE(sDensity == 25 && sThunder && sDiagnostics);
            REQUIRE(archives.lastLookup == physical);
        }
        archives.owners[physical] = base;
        InitSceneRainPolicy(&play, &archives, 0, 1);
        REQUIRE(sDensity == 0); // globally mounted policy cannot opt in another scene archive
        scene.data->Parent = custom;
        InitSceneRainPolicy(&play, &archives, 2, 0);
        REQUIRE(sDensity == 25); // explicit resource ownership takes precedence
        scene.data->Parent.reset();
    }
    REQUIRE(unrelated->reads == 0);
    scene.data->Parent = custom;
    InitSceneRainPolicy(&play, &archives, 4, 0);
    REQUIRE(sDensity == 0);
    custom->files[kSceneRainPolicyPath] = "broken";
    InitSceneRainPolicy(&play, &archives, 0, 1);
    REQUIRE(sDensity == 0);
    custom->files[kSceneRainPolicyPath] = json;
    play.sceneNum = 0x51;
    InitSceneRainPolicy(&play, &archives, 0, 1);
    REQUIRE(sDensity == 0);
    play.sceneNum = 0x5B;
    play.sceneSegment = nullptr;
    InitSceneRainPolicy(&play, &archives, 0, 1);
    REQUIRE(sDensity == 0);
    REQUIRE(sBegins == 16); // every initialization resets the preceding lifetime
}
