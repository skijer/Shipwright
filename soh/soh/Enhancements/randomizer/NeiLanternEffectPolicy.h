#pragma once
#include "NeiGiEffectPolicy.h"

namespace NeiLantern {
// Author-space dimensions shared with SOURCE/lantern.py. The physical chamber
// surrounds these effects; neither sampler writes gameplay state or spawns actors.
constexpr float GripY = 52.f;
constexpr float HeldScale = 7.f / GripY;
constexpr float GiScale = .025f;
constexpr float EffectiveScale = .5f;
constexpr float ModelScale = GiScale / EffectiveScale;

inline NeiGi::Kind CoreTexture(uint8_t fire) {
    return fire == 1 ? NeiGi::Kind::Fire : (fire == 2 ? NeiGi::Kind::Ice : NeiGi::Kind::Neutral);
}

inline NeiGi::Mesh SampleCore(uint8_t fire, uint32_t frame, const NeiGi::Basis& camera = {}) {
    using namespace NeiGi;
    Mesh mesh;
    const float t = Time(frame);
    if (fire == 1 || fire == 2) {
        // Reuse the immutable fire/frost texels on a narrow upright flame body,
        // rather than introducing another texture generator or a world actor.
        mesh = SampleOrb(CoreTexture(fire), camera);
        for (size_t i = 0; i < mesh.count; ++i) {
            auto& p = mesh.vertices[i].p;
            const float height = p.y / 15.f;
            if (fire == 1) {
                const float taper = .58f * (1.f - std::fmax(0.f, height) * .73f);
                p.x = p.x * taper + 1.25f * std::sin(t * 3 + height * 4) * (height + 1.f) * .5f;
                p.z *= taper;
                p.y = p.y * 1.30f - 4.f;
            } else {
                p.x = p.x * .32f + .65f * std::sin(t + height * 3);
                p.z *= .32f;
                p.y = p.y * 1.15f - 5.f;
            }
        }
    } else if (fire == 3) {
        const Point center{ std::sin(t) * 1.8f, -2.f + std::sin(t * 2), std::cos(t) * 1.8f };
        Glow(mesh, center, 8.f + .7f * std::sin(t * 2), 0xA950FF, 165, camera);
        Glow(mesh, center, 3.5f, 0xEDD0FF, 215, camera);
    } else if (fire == 4) {
        // A quiet warm center; the ascending paired curls carry the motion.
        Glow(mesh, { 0, -7, 0 }, 8.f + .45f * std::sin(t * 2), 0x70E969, 130, camera);
        Glow(mesh, { 0, -7, 0 }, 3.1f, 0xDCFFB0, 210, camera);
    }
    return mesh;
}

inline NeiGi::Mesh SampleAccents(uint8_t fire, uint32_t frame, const NeiGi::Basis& camera = {}) {
    using namespace NeiGi;
    Mesh mesh;
    const float t = Time(frame), clock = (frame % 180u) / 180.f;
    if (fire == 1) {
        // Hot tongues flick upward, while small embers rise and die below the lid.
        for (int tongue = 0; tongue < 2; ++tongue) {
            auto p = [&](int j) {
                const float f = j / 7.f, phase = t * (tongue ? 3 : 2) + tongue * 2.5f;
                return Point{ std::sin(phase + f * 5) * 3.2f * f, -24.f + f * 31.f,
                              std::cos(phase + f * 3) * 2.5f * f };
            };
            for (int j = 0; j < 7; ++j)
                Band(mesh, p(j), p(j + 1), 1.4f * (1.f - j / 8.f), 0xFF8C18, 0xFFF1AB, camera, 195);
        }
        for (int i = 0; i < 4; ++i) {
            const float f = std::fmod(clock * 3 + i * .25f, 1.f);
            const Point p{ std::sin(t * 2 + i * 2.3f) * 7.f, -4.f + f * 25.f, std::cos(t * 2 + i * 2.3f) * 7.f };
            Glow(mesh, p, .65f + .9f * (1.f - f), 0xFFC46A, uint8_t(200 * (1 - f)), camera);
        }
    } else if (fire == 2) {
        // Falling faceted frost is deliberately different from electrical arcs
        // and from the regular fire's upward embers.
        for (int i = 0; i < 6; ++i) {
            const float f = std::fmod(clock * 2 + i / 6.f, 1.f);
            const float a = i * 2.4f + .2f * std::sin(t);
            const Point p{ 10.f * std::cos(a), 19.f - f * 40.f, 10.f * std::sin(a) };
            const float spin = t * (i % 2 ? -1 : 1) + i;
            const uint8_t alpha = uint8_t(220 * std::pow(std::sin(f * Tau * .5f), 2.f));
            IceCrystal(mesh, p, { std::sin(spin), std::cos(spin), .3f }, 3.5f, .85f, spin, alpha, camera);
        }
    } else if (fire == 3) {
        // Three disembodied wisps follow tilted, counter-moving elliptical paths.
        for (int wisp = 0; wisp < 3; ++wisp) {
            auto p = [&](int j) {
                const float a = t * (wisp == 1 ? -1 : 1) + wisp * Tau / 3 - (6 - j) * .14f;
                return Point{ 10.f * std::cos(a), 12.f * std::sin(a + wisp * .65f), 8.f * std::sin(a) };
            };
            for (int j = 0; j < 6; ++j)
                Band(mesh, p(j), p(j + 1), .45f + .18f * j, 0x9746EF, 0xEECFFF, camera, uint8_t(45 + j * 30));
            Glow(mesh, p(6), 2.2f, 0xD4A3FF, 200, camera);
        }
    } else if (fire == 4) {
        // Gentle paired growth curls and buoyant seed motes stay inside the glass.
        for (int curl = 0; curl < 2; ++curl) {
            auto p = [&](int j) {
                const float f = j / 12.f, a = t + f * Tau * 1.25f + curl * Tau * .5f;
                const float radius = 6.5f * std::sin(f * Tau * .5f);
                return Point{ radius * std::cos(a), -24.f + 44.f * f, radius * std::sin(a) };
            };
            for (int j = 0; j < 12; ++j)
                Band(mesh, p(j), p(j + 1), .75f, 0x4EC74D, 0xC8FF9B, camera,
                     uint8_t(70 + 80 * std::sin((j + .5f) / 12.f * Tau * .5f)));
        }
        for (int i = 0; i < 4; ++i) {
            const float f = std::fmod(clock + i * .25f, 1.f), a = t + i * 2.3f;
            const Point p{ 10.f * std::cos(a), -19.f + f * 36.f, 10.f * std::sin(a) };
            Glow(mesh, p, 1.1f, 0xBDFF98, uint8_t(160 * std::sin(f * Tau * .5f)), camera);
        }
    }
    return mesh;
}
} // namespace NeiLantern
