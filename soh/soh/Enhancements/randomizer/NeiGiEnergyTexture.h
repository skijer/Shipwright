#pragma once

#include "NeiGiEffectPolicy.h"
#include <algorithm>

namespace NeiGi {
constexpr size_t OrbTextureSize = 64, OrbFrameCount = 48;
using OrbTexels = std::array<uint8_t, OrbTextureSize * OrbTextureSize>;

namespace OrbDetail {
inline float Smooth(float lo, float hi, float x) {
    x = std::clamp((x - lo) / (hi - lo), 0.f, 1.f);
    return x * x * (3 - 2 * x);
}
inline float Lattice(int x, int y, int z) {
    uint32_t h = uint32_t(x) * 0x9E3779B9u ^ uint32_t(y) * 0x85EBCA6Bu ^ uint32_t(z) * 0xC2B2AE35u;
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    return (h & 0xFFFFu) / 65535.f;
}
inline float Noise(float x, float y, float z) {
    const int ix = int(std::floor(x)), iy = int(std::floor(y)), iz = int(std::floor(z));
    const float u = Smooth(0, 1, x - ix), v = Smooth(0, 1, y - iy), w = Smooth(0, 1, z - iz);
    auto mix = [](float a, float b, float t) { return a + (b - a) * t; };
    auto layer = [&](int k) {
        return mix(mix(Lattice(ix, iy, k), Lattice(ix + 1, iy, k), u),
                   mix(Lattice(ix, iy + 1, k), Lattice(ix + 1, iy + 1, k), u), v);
    };
    return mix(layer(iz), layer(iz + 1), w);
}
inline float Cloud(float x, float y, float z) {
    return .58f * Noise(x, y, z) + .28f * Noise(x * 2.13f, y * 2.13f, z * 2.13f) +
           .14f * Noise(x * 4.31f, y * 4.31f, z * 4.31f);
}
// Immutable, original procedural texels, generated once at program startup.
// Stable addresses let the renderer cache textures without invalidation or
// per-item regeneration. Each I8 frame fits one native 4 KiB TMEM load.
struct Textures {
    alignas(8) std::array<std::array<OrbTexels, OrbFrameCount>, 4> profiles{};
    Textures() {
        for (size_t frame = 0; frame < OrbFrameCount; ++frame) {
            const float t = Tau * frame / OrbFrameCount, ct = std::cos(t), st = std::sin(t);
            for (size_t v = 0; v < OrbTextureSize; ++v)
                for (size_t u = 0; u < OrbTextureSize; ++u) {
                    const float x = 2.f * u / (OrbTextureSize - 1) - 1, y = 1 - 2.f * v / (OrbTextureSize - 1);
                    const float r = std::sqrt(x * x + y * y), a = std::atan2(y, x);
                    const float warp = Noise(x * 3 + ct, y * 3 + st, 2 + st);
                    const float n = Cloud(x * 4 + ct + warp, y * 4 + st - warp, 7 + 1.3f * st);
                    const float fine = Noise(x * 12 + 2 * ct, y * 12 + 2 * st, 11 + std::sin(t * 2));
                    const float edge = Smooth(.98f, .64f + .2f * (n - .5f), r);
                    const float wisps = Smooth(.99f, .68f, r + .12f * std::sin(a * 7 + t * 2) * (warp - .25f));
                    const float hot = Smooth(.27f, .73f, n);
                    const float ridges = std::pow(1 - std::abs(2 * fine - 1), 5.f);
                    const float fire = wisps * (.3f + .6f * hot + .25f * ridges);
                    // Three angular families of frost veins over slowly moving
                    // cloudy density; sharp detail distinguishes ice from fire.
                    const float frostX = x * 8 + .8f * warp + .35f * ct;
                    const float frostY = y * 8 + .8f * n + .35f * st;
                    const float veinDistance =
                        std::min({ std::abs(std::sin(frostX)), std::abs(std::sin(frostX * .5f + frostY * .866f)),
                                   std::abs(std::sin(frostX * .5f - frostY * .866f)) });
                    const float veins = 1 - Smooth(.02f, .15f, veinDistance);
                    const float ice = edge * (.28f + .48f * hot + .42f * veins);
                    const float spell = edge * (.25f + .65f * hot + .25f * ridges);
                    const float dark = Smooth(.28f, .50f, r) * wisps * (.35f + .55f * hot + .4f * ridges);
                    const float values[] = { fire, ice, spell, dark };
                    for (size_t p = 0; p < profiles.size(); ++p)
                        profiles[p][frame][v * OrbTextureSize + u] =
                            uint8_t(std::lround(255 * Smooth(1.f, .92f, r) * std::clamp(values[p], 0.f, 1.f)));
                }
        }
    }
};
inline const Textures textures;
} // namespace OrbDetail

inline const OrbTexels& OrbTexture(Kind kind, uint32_t frame) {
    const size_t profile = kind == Kind::Fire ? 0 : kind == Kind::Ice ? 1 : kind == Kind::Demise ? 3 : 2;
    return OrbDetail::textures.profiles[profile][frame % OrbFrameCount];
}
struct OrbColors {
    uint32_t hot, edge;
};
inline OrbColors OrbPalette(Kind kind) {
    switch (kind) {
        case Kind::Fire:
            return { 0xFFF1A8, 0xD52C05 };
        case Kind::Ice:
            return { 0xE8FFFF, 0x006FCB };
        case Kind::Hylia:
            return { 0xFFE5FF, 0xB91A99 };
        case Kind::Zonai:
            return { 0xC8FFF3, 0x007B67 };
        default:
            return { 0xC5D6E8, 0x05070B };
    }
}
} // namespace NeiGi
