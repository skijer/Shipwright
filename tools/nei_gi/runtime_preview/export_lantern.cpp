// Emit actual lantern sampler geometry and the shared immutable native texels.
// The stream layout matches export_effects.cpp so existing preview loaders apply.
#include "soh/Enhancements/randomizer/NeiLanternEffectPolicy.h"
#include "soh/Enhancements/randomizer/NeiGiEnergyTexture.h"
#include <cmath>
#include <fstream>
#include <iostream>

static void Vertex(std::ofstream& out, const NeiGi::EffectVertex& v, bool textured) {
    const float p[] = { std::round(v.p.x * 16) / 16, std::round(v.p.y * 16) / 16,
                        std::round(v.p.z * 16) / 16 };
    const uint8_t rgba[] = { uint8_t(v.rgb >> 16), uint8_t(v.rgb >> 8), uint8_t(v.rgb), v.alpha };
    out.write(reinterpret_cast<const char*>(p), sizeof(p));
    out.write(reinterpret_cast<const char*>(rgba), sizeof(rgba));
    if (textured) {
        auto nativeUv = [](float uv) {
            const int packed = std::lround(uv * 63 * 32);
            return (((packed * 65535) >> 16) / 32.f + .5f) / 64;
        };
        const float uv[] = { nativeUv(v.u), nativeUv(v.v) };
        out.write(reinterpret_cast<const char*>(uv), sizeof(uv));
    }
}

int main(int argc, char** argv) {
    if (argc != 2)
        return 2;
    std::ofstream out(argv[1], std::ios::binary);
    std::ofstream orbs(std::string(argv[1]) + ".orbs", std::ios::binary);
    constexpr float x = 12 * NeiGi::Tau / 360, y = 25 * NeiGi::Tau / 360;
    const float sx = std::sin(x), cx = std::cos(x), sy = std::sin(y), cy = std::cos(y);
    const NeiGi::Basis camera{ { cy, 0, sy }, { sx * sy, cx, -sx * cy }, { -cx * sy, sx, cx * cy } };
    for (uint32_t frame = 0; frame < 180; ++frame) {
        for (int slot = 0; slot < 8; ++slot) {
            const uint8_t fire = slot < 4 ? slot + 1 : 0;
            const auto core = fire >= 3 ? NeiLantern::SampleCore(fire, frame, camera) : NeiGi::Mesh{};
            const auto accents = NeiLantern::SampleAccents(fire, frame, camera);
            const uint32_t count = core.count + accents.count;
            out.write(reinterpret_cast<const char*>(&count), sizeof(count));
            for (size_t i = 0; i < core.count; ++i)
                Vertex(out, core.vertices[i], false);
            for (size_t i = 0; i < accents.count; ++i)
                Vertex(out, accents.vertices[i], false);
        }
        for (int slot = 0; slot < 6; ++slot) {
            const uint8_t fire = slot < 2 ? slot + 1 : 0;
            const auto core = NeiLantern::SampleCore(fire, frame, camera);
            const uint32_t count = core.count;
            orbs.write(reinterpret_cast<const char*>(&count), sizeof(count));
            if (count == 0)
                continue;
            const auto kind = NeiLantern::CoreTexture(fire);
            const auto color = NeiGi::OrbPalette(kind);
            orbs.write(reinterpret_cast<const char*>(&color.hot), 4);
            orbs.write(reinterpret_cast<const char*>(&color.edge), 4);
            const auto& texels = NeiGi::OrbTexture(kind, frame);
            orbs.write(reinterpret_cast<const char*>(texels.data()), texels.size());
            for (size_t i = 0; i < core.count; ++i)
                Vertex(orbs, core.vertices[i], true);
        }
    }
    if (!out || !orbs)
        return 1;
    std::cout << "Exported 180 frames of four contained lantern variants\n";
}
