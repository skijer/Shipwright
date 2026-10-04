// Reuse the real-header GI graphics boundary; exercise held rendering itself.
#define main NeiGiRegressionMain
#include "../nei_gi/presentation_test.cpp"
#undef main
#include "soh/Enhancements/randomizer/NeiHeldPresentation.cpp"
extern "C" void Matrix_RotateX(float, uint8_t) {}

int main() {
  using namespace Fixture;
  const char *opa = NEI_HELD_PATH("test");
  const char *glass = "__OTR__objects/nei_held_redesign/test/gi_xlu_dl";
  Reset();
  assert(!NeiHeld_DrawModel(&play, opa, glass));
  files.insert(opa);
  assert(!NeiHeld_DrawModel(&play, opa, glass));
  assert(Drawn().empty() && allocations == 0);
  files.insert(glass);
  matrix = 3;
  matrixY = 7;
  assert(NeiHeld_DrawModel(&play, opa, glass));
  assert(Drawn() == std::vector<std::string>({opa, glass}));
  assert(matrix == 3 && matrixY == 7 && stack.empty() && loads == 0);
  Reset();
  files.insert(std::string("alt/") + opa);
  assert(!NeiHeld_DrawModel(&play, opa, nullptr));
  alt = 1;
  assert(NeiHeld_DrawModel(&play, opa, nullptr));
  assert(loads == 0 && Drawn() == std::vector<std::string>({opa}));
  // Energy follows the rod even with the pickup-shimmer checkbox disabled.
  for (int i = 0; i < 3; ++i) {
    Reset();
    assert(!NeiHeld_DrawRod(&play, i) && arena.empty());
    const char *rods[] = {NEI_HELD_PATH("fire_rod"), NEI_HELD_PATH("ice_rod"),
                          NEI_HELD_PATH("light_rod")};
    files.insert(rods[i]);
    matrix = .05f;
    matrixY = 13;
    assert(NeiHeld_DrawRod(&play, i) && !arena.empty());
    // All three grip origins use the caller's wrist pose. The legacy light
    // rod's five-unit correction belongs only to its native fallback drawer.
    assert(submitted.front().first == .05f && submitted.front().second == 13);
    assert(matrix == .05f && matrixY == 13 && stack.empty());
    assert(Drawn() == std::vector<std::string>({rods[i]}));
  }
  Reset();
  Player p{};
  p.bodyPartsPos[PLAYER_BODYPART_L_HAND] = {2, 20, 2};
  p.bodyPartsPos[PLAYER_BODYPART_R_HAND] = {-2, 20, 2};
  files.insert(NEI_HELD_PATH("mitt_left"));
  assert(!NeiHeld_DrawMitts(&p, &play) && Drawn().empty());
  files.insert(NEI_HELD_PATH("mitt_right"));
  assert(NeiHeld_DrawMitts(&p, &play) && Drawn().size() == 2);
  assert(matrix == 1 && stack.empty());
  Reset();
  files.insert(NEI_HELD_PATH("gust_jar"));
  assert(!NeiHeld_DrawGustJar(&p, &play, 1, 1) && Drawn().empty());
  files.insert(NEI_HELD_PATH("gust_jar_band"));
  assert(NeiHeld_DrawGustJar(&p, &play, 1, 1) && Drawn().size() == 2);
  assert(matrix == 1 && stack.empty());
  // Preserve the original SUCK/BLOW endpoint colors, including heat clamp.
  struct JarColor {
    int direction;
    float heat;
    uint32_t rgba;
  };
  for (const auto test :
       {JarColor{0, -1, 0x3C78C8FF}, JarColor{0, 2, 0x00B4FFFF},
        JarColor{1, -1, 0xB42828FF}, JarColor{1, 2, 0xFF0000FF}}) {
    Reset();
    files.insert(NEI_HELD_PATH("gust_jar"));
    files.insert(NEI_HELD_PATH("gust_jar_band"));
    assert(NeiHeld_DrawGustJar(&p, &play, test.direction, test.heat));
    uint32_t color = 0;
    for (Gfx *command = Fixture::opa; command < gfx.polyOpa.p; ++command)
      if ((command->words.w0 >> 24) == G_SETPRIMCOLOR)
        color = command->words.w1;
    assert(color == test.rgba);
  }
  std::cout
      << "PASS: held complete-resource fallback, Alt-only archive, deferred "
         "paths, rod energy, paired mitts and jar matrix balance\n";
}
