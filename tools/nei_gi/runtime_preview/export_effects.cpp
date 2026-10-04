// Emit the production policy's triangles for offline preview. No alternate
// effect implementation.
#include "soh/Enhancements/randomizer/NeiGiEffectPolicy.h"
#include "soh/Enhancements/randomizer/NeiGiEnergyTexture.h"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
int main(int argc, char **argv) {
  if (argc != 2 && argc != 3)
    return 2;
  std::ofstream out(argv[1], std::ios::binary);
  std::ofstream orbs(std::string(argv[1]) + ".orbs", std::ios::binary);
  constexpr float x = 12 * NeiGi::Tau / 360, y = 25 * NeiGi::Tau / 360;
  const float sx = std::sin(x), cx = std::cos(x), sy = std::sin(y),
              cy = std::cos(y);
  NeiGi::Basis camera{
      {cy, 0, sy}, {sx * sy, cx, -sx * cy}, {-cx * sy, sx, cx * cy}};
  // Optional held-rod frame: the model's native shaft is tilted about Z.
  // Transform the camera into that frame exactly as the runtime does.
  if (argc == 3) {
    const float a = -std::strtof(argv[2], nullptr) * NeiGi::Tau / 360;
    auto rotate = [a](NeiGi::Point p) {
      return NeiGi::Point{p.x * std::cos(a) - p.y * std::sin(a),
                          p.x * std::sin(a) + p.y * std::cos(a), p.z};
    };
    camera = {rotate(camera.right), rotate(camera.up), rotate(camera.forward)};
  }
  const NeiGi::Kind kinds[] = {NeiGi::Kind::Fire,  NeiGi::Kind::Ice,
                               NeiGi::Kind::Light, NeiGi::Kind::Hylia,
                               NeiGi::Kind::Zonai, NeiGi::Kind::Demise};
  for (uint32_t frame = 0; frame < 180; ++frame) {
    for (int k = 0; k < 8; ++k) {
      auto m = k >= 6 ? NeiGi::SampleShimmer(frame, true, camera,
                                             k == 7 ? NeiGi::Kind::Leaf
                                                    : NeiGi::Kind::Neutral)
                      : NeiGi::SampleEnergy(kinds[k], frame, camera);
      uint32_t count = m.count;
      out.write(reinterpret_cast<const char *>(&count), sizeof(count));
      for (size_t i = 0; i < m.count; ++i) {
        auto v = m.vertices[i];
        // Same 1/16-unit quantization as the runtime Vtx upload.
        v.p.x = std::round(v.p.x * 16) / 16;
        v.p.y = std::round(v.p.y * 16) / 16;
        v.p.z = std::round(v.p.z * 16) / 16;
        const float p[] = {v.p.x, v.p.y, v.p.z};
        const uint8_t rgba[] = {uint8_t(v.rgb >> 16), uint8_t(v.rgb >> 8),
                                uint8_t(v.rgb), v.alpha};
        out.write(reinterpret_cast<const char *>(p), sizeof(p));
        out.write(reinterpret_cast<const char *>(rgba), sizeof(rgba));
      }
    }
    for (auto kind : kinds) {
      const auto mesh = NeiGi::SampleOrb(kind, camera);
      const uint32_t count = mesh.count;
      orbs.write(reinterpret_cast<const char *>(&count), sizeof(count));
      if (!count)
        continue;
      const auto color = NeiGi::OrbPalette(kind);
      orbs.write(reinterpret_cast<const char *>(&color.hot), 4);
      orbs.write(reinterpret_cast<const char *>(&color.edge), 4);
      const auto &texels = NeiGi::OrbTexture(kind, frame);
      orbs.write(reinterpret_cast<const char *>(texels.data()), texels.size());
      for (size_t i = 0; i < mesh.count; ++i) {
        const auto &v = mesh.vertices[i];
        const float p[] = {std::round(v.p.x * 16) / 16,
                           std::round(v.p.y * 16) / 16,
                           std::round(v.p.z * 16) / 16};
        const uint8_t rgba[] = {255, 255, 255, v.alpha};
        // Match s10.5 packing, the RSP's 0xFFFF scale, and texel centers.
        auto nativeUv = [](float uv) {
          const int packed = std::lround(uv * 63 * 32);
          return (((packed * 65535) >> 16) / 32.f + .5f) / 64;
        };
        const float uv[] = {nativeUv(v.u), nativeUv(v.v)};
        orbs.write(reinterpret_cast<const char *>(p), sizeof(p));
        orbs.write(reinterpret_cast<const char *>(rgba), sizeof(rgba));
        orbs.write(reinterpret_cast<const char *>(uv), sizeof(uv));
      }
    }
  }
  if (!out || !orbs)
    return 1;
  std::cout << "Exported 180 frames of production energy, shimmer, and native "
               "orb texels\n";
}
