// Real shared batcher, at the full existing 5 x 3 projectile-set capacity.
#define main NeiGiRegressionMain
#include "../nei_gi/presentation_test.cpp"
#undef main
#include "soh/Enhancements/randomizer/NeiUsedMagicPolicy.h"
int main() {
  using namespace Fixture;
  using namespace NeiUsedMagic;
  const NeiGi::TextureMaterial ice{"__OTR__objects/nei_used_magic/ice_fracture",
                                   true, true};
  const NeiGi::TextureMaterial rays{"__OTR__objects/nei_used_magic/light_rays",
                                    true, false};
  const NeiGi::TextureMaterial attacks[] = {
      {"__OTR__objects/nei_rod_attack/fire_surge", false, false},
      {"__OTR__objects/nei_rod_attack/frost_surge", false, false},
      {"__OTR__objects/nei_rod_attack/light_surge", false, false}};
  const NeiGi::TextureMaterial releases[] = {
      {"__OTR__objects/nei_rod_attack/fire_release_crest", true, false},
      {"__OTR__objects/nei_rod_attack/ice_release_crest", true, false},
      {"__OTR__objects/nei_rod_attack/light_surge", false, false}};
  const NeiGi::TextureMaterial iceWake{"__OTR__objects/nei_rod_attack/ice_release_flow", true, true};
  const Basis cameras[] = {{},
                           {{0, 0, 1}, {0, 1, 0}, {-1, 0, 0}},
                           {{.7071f, 0, .7071f},
                            {.4082f, .8165f, -.4082f},
                            {-.5774f, .5774f, .5774f}}};
  for (Kind kind : {Kind::Fire, Kind::Ice, Kind::Light}) {
    const auto &attack = attacks[kind == Kind::Fire  ? 0
                                 : kind == Kind::Ice ? 1
                                                     : 2];
    const auto &release = releases[kind == Kind::Fire  ? 0
                                   : kind == Kind::Ice ? 1
                                                       : 2];
    size_t maxBytes = 0, maxCommands = 0;
    for (const auto &camera : cameras)
      for (uint32_t frame = 0; frame < 180; ++frame) {
        Reset();
        play.gameplayFrames = frame;
        files.insert(ice.path);
        files.insert(iceWake.path);
        files.insert(rays.path);
        files.insert(attack.path);
        files.insert(release.path);
        auto draw = [&](const Mesh &mesh) {
          NeiGi_DrawMesh(&play, kind == Kind::Ice ? IceAtmosphere(mesh) : mesh);
          if (kind == Kind::Ice)
            NeiGi_DrawTexturedMesh(&play, IceSurface(mesh), ice);
        };
        for (int set = 0; set < 5; ++set) {
          const Point trail[] = {{0, 0, 0},   {-15, 0, 0}, {-30, 0, 0},
                                 {-45, 0, 0}, {-60, 0, 0}, {-75, 0, 0}};
          if (kind != Kind::Ice)
            NeiGi_DrawMesh(&play,
                           SampleTrail(kind, frame, trail, 6, 2, camera));
          for (int p = 0; p < 3; ++p) {
            const float a = (p == 0   ? 0.f
                             : p == 1 ? -1.f
                                      : 1.f) *
                            (30 * (0x10000 / 360)) * Tau / 65536.f;
            const Point direction{std::cos(a), 0, -std::sin(a)};
            Point sideTrail[6];
            for (int i = 0; i < 6; ++i)
              sideTrail[i] = direction * (-15.f * i);
            if (kind == Kind::Ice)
              NeiGi_DrawTexturedMesh(
                  &play, SampleTrail(kind, frame, sideTrail, 6, 2, camera),
                  iceWake);
            NeiGi_DrawTexturedMesh(
                &play,
                SampleProjectileSurface(kind, frame + set * 19 + p * 7, 2,
                                        direction, camera),
                attack);
            NeiGi_DrawMesh(&play,
                           SampleProjectile(kind, frame + set * 19 + p * 7, 2,
                                            direction, camera));
          }
        }
        draw(SampleCharge(kind, frame, 1, camera));
        if (kind != Kind::Ice)
          NeiGi_DrawTexturedMesh(
              &play, SampleChargeSurface(kind, frame, 1, camera), kind == Kind::Fire ? release : rays);
        draw(SampleChargeSparks(kind, frame, 1, camera));
        NeiGi_DrawMesh(&play, SampleSpin(kind, frame, 150, true, camera));
        NeiGi_DrawTexturedMesh(&play, SampleSpinSurface(kind, frame, 150, true),
                               release);
        NeiGi_DrawTexturedMesh(&play, SampleSpinFlow(kind, frame, 150, true, 1),
                               release);
        NeiGi_DrawMesh(&play, NeiGi::SampleOrb(kind, camera), kind);
        NeiGi_DrawMesh(&play, NeiGi::SampleEnergy(kind, frame, camera));
        size_t bytes = 0;
        for (const auto &a : arena)
          bytes += a.size() * sizeof(Vtx);
        maxBytes = std::max(maxBytes, bytes);
        maxCommands = std::max(maxCommands, size_t(gfx.polyXlu.p - xlu));
        assert(gfx.polyXlu.p < xlu + 4096 && allocations < 128 &&
               stack.empty() && interpolation == 0 && loads == 0);
      }
    assert(maxBytes < 90000 &&
           maxCommands < 2400); // Reserve ordinary scene/actor space.
    std::cout << (kind == Kind::Fire  ? "Fire"
                  : kind == Kind::Ice ? "Ice"
                                      : "Light")
              << " maximum 15 heads + " << (kind == Kind::Ice ? 15 : 5)
              << " wakes + charge + released spin: " << maxBytes
              << " vertex bytes, " << maxCommands << " XLU commands\n";
  }
}
