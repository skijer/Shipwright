#include "mods/items/helpers/ice_trail.h"
#include "soh/Enhancements/randomizer/NeiUsedMagicPolicy.h"
#include <fstream>
#include <vector>
using namespace NeiUsedMagic;

// Native normal-rate large-spin window:16 draws, radius80..500. Blank frames
// separate repeats. Camera, placement and detail lifetime are preview fixtures.
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  std::ofstream out(argv[1], std::ios::binary);
  const float x = 24 * Tau / 360, y = -18 * Tau / 360;
  const Basis camera{
      {std::cos(y), 0, std::sin(y)},
      {std::sin(x) * std::sin(y), std::cos(x), -std::sin(x) * std::cos(y)},
      {-std::cos(x) * std::sin(y), std::sin(x), std::cos(x) * std::cos(y)}};
  for (unsigned frame = 0; frame < 180; ++frame)
    for (int panel = 0; panel < 5; ++panel) {
      struct Layer {
        Mesh mesh;
        uint32_t material;
      };
      std::vector<Layer> layers;
      auto append = [&](Mesh m, Point off = {}, uint32_t material = 0) {
        if (!m.count)
          return;
        for (size_t i = 0; i < m.count; ++i)
          m.vertices[i].p = m.vertices[i].p + off;
        layers.push_back({m, material});
      };
      if (panel < 3) {
        const Kind kind = panel == 0 ? Kind::Fire : Kind::Ice;
        // Native15units/update and30-degree spread. Follow the center shot.
        // Detail panel holds one head at mature scale for inspection.
        const unsigned age = frame % 45u;
        const float travel = 15.f * (age + 1);
        const float scale =
            panel == 2 ? 2.f : 2.f * (1 - std::pow(.8f, float(age + 1)));
        const Point offset{30 - travel, 20, 0};
        NeiIceTrailPoint center[6];
        for (int j = 0; j < 6; ++j)
          center[j] = {std::max(0.f, travel - 15 * j), 0, 0};
        Point pos[3], dir[3];
        for (int p = 0; p < 3; ++p) {
          const float a = (p == 1   ? -1.f
                           : p == 2 ? 1.f
                                    : 0.f) *
                          (30 * (0x10000 / 360)) * Tau / 65536.f;
          dir[p] = {std::cos(a), 0, -std::sin(a)};
          pos[p] = dir[p] * travel;
        }
        // The volley leaves this close camera after12ticks.
        if (panel == 2 || age < 12) {
          if (kind == Kind::Fire) {
            Point wake[6];
            for (int j = 0; j < 6; ++j)
              wake[j] = {center[j].x, center[j].y, center[j].z};
            append(SampleTrail(kind, frame, wake, 6, scale, camera), offset);
          } else
            for (int p = 0; p < (panel == 2 ? 1 : 3); ++p) {
              NeiIceTrailPoint reconstructed[6];
              Point wake[6];
              NeiIceTrail_Reconstruct(center, 6, {pos[p].x, pos[p].y, pos[p].z},
                                      p, reconstructed);
              for (int j = 0; j < 6; ++j)
                wake[j] = {reconstructed[j].x, reconstructed[j].y,
                           reconstructed[j].z};
              append(SampleTrail(kind, frame, wake, 6, scale, camera), offset,
                     8);
            }
          for (int p = 0; p < (panel == 2 ? 1 : 3); ++p) {
            append(SampleProjectileSurface(kind, frame + p * 7, scale, dir[p],
                                           camera),
                   pos[p] + offset, panel == 0 ? 4 : 5);
            append(SampleProjectile(kind, frame + p * 7, scale, dir[p], camera),
                   pos[p] + offset);
          }
        }
      } else {
        const Kind kind = panel == 3 ? Kind::Fire : Kind::Ice;
        const int tick = int(frame % 45u) - 10;
        if (tick >= 0 && tick < 16) {
          const float radius = std::min(500.f, 80.f + tick * 30.f);
          append(SampleSpin(kind, frame, radius, true, camera));
          append(SampleSpinSurface(kind, frame, radius, true), {},
                 panel == 3 ? 7 : 8);
          append(SampleSpinFlow(kind, frame, radius, true, 1), {},
                 panel == 3 ? 7 : 8);
        }
      }
      uint32_t count = layers.size();
      out.write((char *)&count, 4);
      for (auto &layer : layers) {
        out.write((char *)&layer.material, 4);
        count = layer.mesh.count;
        out.write((char *)&count, 4);
        for (size_t i = 0; i < layer.mesh.count; ++i) {
          const auto &v = layer.mesh.vertices[i];
          const float p[] = {std::round(v.p.x * 16) / 16,
                             std::round(v.p.y * 16) / 16,
                             std::round(v.p.z * 16) / 16};
          const uint8_t c[] = {uint8_t(v.rgb >> 16), uint8_t(v.rgb >> 8),
                               uint8_t(v.rgb), v.alpha};
          const float uv[] = {std::round(v.u * 1024) / 1024,
                              std::round(v.v * 1024) / 1024};
          out.write((char *)p, 12);
          out.write((char *)c, 4);
          out.write((char *)uv, 8);
        }
      }
    }
  return !out;
}
