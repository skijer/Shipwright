// Exercise the production held lantern using the existing real-header graphics boundary.
#define main NeiGiLanternRegressionMain
#include "presentation_test.cpp"
#undef main
#include "soh/Enhancements/randomizer/NeiHeldPresentation.cpp"
#include "soh/Enhancements/randomizer/NeiLanternPresentation.cpp"
extern "C" void Matrix_RotateX(float, uint8_t) {}

int main() {
    using namespace Fixture;
    Player player{};
    player.bodyPartsPos[PLAYER_BODYPART_L_HAND] = { 2, 20, 3 };
    const Player before = player;
    const char* opaque = "__OTR__objects/nei_gi_redesign/lantern/gi_dl";
    const char* glass = "__OTR__objects/nei_gi_redesign/lantern/gi_xlu_dl";
    Reset();
    assert(!NeiLantern_DrawHeld(&player, &play, 1));
    files.insert(opaque);
    assert(!NeiLantern_DrawHeld(&player, &play, 1));
    assert(Drawn().empty() && arena.empty() && submitted.empty());

    for (int useAlt = 0; useAlt <= 1; ++useAlt) {
        for (uint8_t fire = 0; fire <= 5; ++fire) {
            Reset();
            const std::string prefix = useAlt ? "alt/" : "";
            files.insert(prefix + opaque);
            files.insert(prefix + glass);
            if (useAlt) {
                assert(!NeiLantern_DrawHeld(&player, &play, fire));
                assert(Drawn().empty());
                alt = 1;
            }
            matrix = 3;
            matrixY = 17;
            assert(NeiLantern_DrawHeld(&player, &play, fire));
            assert(Drawn() == std::vector<std::string>({ opaque, glass }));
            assert(matrix == 3 && matrixY == 17 && stack.empty());
            assert(loads == 0 && interpolation == 0);
            // The GI resource's internal matrix is 1.25 and vertices use Q=16.
            assert(std::abs(submitted.front().first * 1.25f * 16 - NeiLantern::HeldScale) < .00001f);
            assert(std::abs(submitted.front().second - 13.f) < .00001f); // hand 20 minus seven
            assert(std::abs(submitted.back().second - 13.f) < .00001f);
            assert(arena.empty() == (fire == 0 || fire >= 5));
            assert(std::memcmp(&before, &player, sizeof(player)) == 0);
        }
    }
    std::cout << "PASS: lantern partial/base/Alt fallback, grip alignment, lit/unlit passes and read-only player state\n";
}
