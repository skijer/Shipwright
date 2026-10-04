#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace NeiGi {
enum class Kind { Neutral, Fire, Ice, Light, Hylia, Zonai, Demise, Leaf };
constexpr float Tau = 6.28318530718f;
constexpr uint32_t ColorHex(Kind kind) {
    switch (kind) {
        case Kind::Fire:
            return 0xFA8B20;
        case Kind::Ice:
            return 0x357CFF;
        case Kind::Light:
            return 0xFDFF7B;
        case Kind::Hylia:
            return 0xFF96FF;
        case Kind::Zonai:
            return 0x64FFE6;
        case Kind::Demise:
            return 0x000000;
        case Kind::Leaf:
            return 0x5AC85A;
        default:
            return 0xE6E6EB;
    }
}
inline bool IsRod(Kind k) {
    return k == Kind::Fire || k == Kind::Ice || k == Kind::Light;
}
inline bool IsSpell(Kind k) {
    return k == Kind::Hylia || k == Kind::Zonai || k == Kind::Demise;
}
struct Point {
    float x = 0, y = 0, z = 0;
    Point operator+(Point b) const {
        return { x + b.x, y + b.y, z + b.z };
    }
    Point operator-(Point b) const {
        return { x - b.x, y - b.y, z - b.z };
    }
    Point operator*(float s) const {
        return { x * s, y * s, z * s };
    }
};
inline Point Cross(Point a, Point b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
inline Point Unit(Point p) {
    float n = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
    return n > .0001f ? p * (1.f / n) : Point{ 1, 0, 0 };
}
// Camera axes expressed in the rotating GI's coordinates, with scale removed.
struct Basis {
    Point right{ 1, 0, 0 }, up{ 0, 1, 0 }, forward{ 0, 0, 1 };
};
struct EffectVertex {
    Point p;
    uint32_t rgb;
    uint8_t alpha;
    float u = 0, v = 0;
};
struct Mesh {
    // At most 512 triangles / 24 KiB of arena vertices, independent of frame count.
    std::array<EffectVertex, 1536> vertices{};
    size_t count = 0;
    void Tri(EffectVertex a, EffectVertex b, EffectVertex c) {
        if (count + 3 > vertices.size())
            return;
        vertices[count++] = a;
        vertices[count++] = b;
        vertices[count++] = c;
    }
};
inline float Time(uint32_t frame) {
    return (frame % 180u) * (Tau / 180.f);
}
inline Point Plane(const Basis& b, float x, float y, float z = 0) {
    return b.right * x + b.up * y + b.forward * z;
}
inline void Glow(Mesh& m, Point center, float radius, uint32_t color, uint8_t alpha, const Basis& b) {
    for (int j = 0; j < 16; ++j) {
        float a = Tau * j / 16, c = Tau * (j + 1) / 16;
        m.Tri({ center, color, alpha }, { center + Plane(b, std::cos(a) * radius, std::sin(a) * radius), color, 0 },
              { center + Plane(b, std::cos(c) * radius, std::sin(c) * radius), color, 0 });
    }
}
inline void Star(Mesh& m, Point p, float size, float spin, uint32_t color, const Basis& b, uint32_t core = 0xFFFFFF) {
    for (int j = 0; j < 8; ++j) {
        float a = spin + Tau * j / 8, c = spin + Tau * (j + 1) / 8;
        float r = j % 2 ? size * .19f : size, s = (j + 1) % 2 ? size * .19f : size;
        m.Tri({ p, core, 255 }, { p + Plane(b, std::cos(a) * r, std::sin(a) * r), color, 45 },
              { p + Plane(b, std::cos(c) * s, std::sin(c) * s), color, 45 });
    }
}
// Soft colored skirt and bright narrow center. All vertices are per-frame arena data.
inline void Band(Mesh& m, Point a, Point b, float width, uint32_t color, uint32_t core, const Basis& camera,
                 uint8_t alpha = 240) {
    Point side = Unit(Cross(b - a, camera.forward)) * width;
    m.Tri({ a, color, alpha }, { a - side, color, 0 }, { b - side, color, 0 });
    m.Tri({ a, color, alpha }, { b - side, color, 0 }, { b, color, alpha });
    m.Tri({ a, color, alpha }, { b, color, alpha }, { b + side, color, 0 });
    m.Tri({ a, color, alpha }, { b + side, color, 0 }, { a + side, color, 0 });
    side = side * .18f;
    m.Tri({ a - side, core, alpha }, { b - side, core, alpha }, { b + side, core, alpha });
    m.Tri({ a - side, core, alpha }, { b + side, core, alpha }, { a + side, core, alpha });
}
// A pointed hexagonal ice prism with hard planar faces. The effect pass does
// not write depth, so submit only faces directed toward the camera; otherwise
// a rear face could paint over the front and flatten the crystal's shape.
inline void IceCrystal(Mesh& m, Point center, Point direction, float length, float width, float roll, uint8_t alpha,
                       const Basis& camera) {
    const Point axis = Unit(direction);
    const Point side = Unit(Cross(axis, std::abs(axis.y) < .9f ? Point{ 0, 1, 0 } : Point{ 1, 0, 0 }));
    const Point across = Cross(axis, side);
    const Point light = Unit(camera.up * .6f - camera.right * .4f + camera.forward * .7f);
    auto dot = [](Point a, Point b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
    auto face = [&](Point a, Point b, Point c) {
        const Point normal = Unit(Cross(b - a, c - a));
        if (dot(normal, camera.forward) <= .0001f)
            return;
        const float shade = .35f + .65f * std::fmax(0.f, dot(normal, light));
        const uint32_t color =
            (uint32_t(48 + 180 * shade) << 16) | (uint32_t(140 + 105 * shade) << 8) | uint32_t(240 + 15 * shade);
        m.Tri({ a, color, alpha }, { b, color, alpha }, { c, color, alpha });
    };
    auto ring = [&](int j, bool upper) {
        const float a = roll + Tau * (j % 6) / 6;
        return center + axis * (length * (upper ? .20f : -.28f)) +
               (side * std::cos(a) + across * std::sin(a)) * (width * (upper ? .85f : 1.f));
    };
    const Point tip = center + axis * (length * .62f), base = center - axis * (length * .58f);
    for (int j = 0; j < 6; ++j) {
        const Point lo = ring(j, false), nextLo = ring(j + 1, false);
        const Point hi = ring(j, true), nextHi = ring(j + 1, true);
        face(lo, nextLo, nextHi);
        face(lo, nextHi, hi);
        face(hi, nextHi, tip);
        face(nextLo, lo, base);
    }
}
inline Mesh SampleShimmer(uint32_t frame, bool enabled, const Basis& camera = {}, Kind kind = Kind::Neutral) {
    Mesh m;
    if (!enabled)
        return m;
    float t = Time(frame);
    for (int i = 0; i < 5; ++i) {
        float phase = t + i * Tau / 5;
        Point p = { 24 * std::cos(i * 2.4f + t), 24 * std::sin(phase), 24 * std::sin(i * 2.4f + t) };
        float pulse = .25f + .75f * std::pow(.5f + .5f * std::sin(t * 4 + i * 2.f), 2.f);
        // Wonder-item silhouette; Deku Leaf keeps green halos AND green glints.
        const bool leaf = kind == Kind::Leaf;
        Glow(m, p, 7 * pulse, leaf ? ColorHex(Kind::Leaf) : 0xA8E9FF, 140, camera);
        Star(m, p, 6 * pulse, t * .5f, leaf ? 0x5AC85A : 0xE6F8FF, camera, leaf ? 0xB0FFB0 : 0xFFFFFF);
    }
    return m;
}
// Curved textured skin: a flat billboard would be hidden by the opaque core.
// The optional shimmer and the light rod deliberately do not use this pass.
inline Mesh SampleOrb(Kind kind, const Basis& camera = {}) {
    Mesh m;
    if (kind != Kind::Fire && kind != Kind::Ice && !IsSpell(kind))
        return m;
    constexpr int rings = 5, segments = 20;
    const float radius = IsSpell(kind) ? 16.3f : 15.f;
    auto vertex = [&](int ring, int j) {
        const float a = Tau * (j % segments) / segments;
        const float latitude = (Tau * .25f) * ring / rings;
        const float x = std::sin(latitude) * std::cos(a), y = std::sin(latitude) * std::sin(a);
        Point p = Plane(camera, radius * x, radius * y, radius * std::cos(latitude));
        if (IsSpell(kind)) {
            const float extent = (std::abs(p.x) + std::abs(p.z)) / 22.2f + std::abs(p.y) / 37.f;
            if (extent > .99f)
                p = p * (.99f / extent);
        }
        return EffectVertex{ p, 0xFFFFFF, 255, .5f + .5f * x, .5f - .5f * y };
    };
    for (int ring = 0; ring < rings; ++ring)
        for (int j = 0; j < segments; ++j) {
            m.Tri(vertex(ring, j), vertex(ring + 1, j), vertex(ring + 1, j + 1));
            if (ring)
                m.Tri(vertex(ring, j), vertex(ring + 1, j + 1), vertex(ring, j + 1));
        }
    return m;
}
inline Mesh SampleEnergy(Kind kind, uint32_t frame, const Basis& camera = {}) {
    Mesh m;
    const float t = Time(frame);
    const uint32_t color = ColorHex(kind);
    if (kind == Kind::Fire) {
        // The turbulent textured body supplies the flame; embers rise from it.
        for (int i = 0; i < 4; ++i) {
            float f = std::fmod((frame % 180u) / 45.f + i * .25f, 1.f);
            Star(m, { std::sin(t * 3 + i * 2.f) * 7, 10 + f * 21, std::cos(t * 3 + i * 2.f) * 7 }, 2.2f * (1 - f) + .4f,
                 t, 0xFFD274, camera);
        }
    } else if (kind == Kind::Ice) {
        // Small, tumbling ice fragments fall away and fade at both ends of
        // their lifetime. These are faceted solids, not four-point shimmers.
        for (int i = 0; i < 8; ++i) {
            const float f = std::fmod((frame % 180u) / 90.f + i / 8.f, 1.f);
            const float fade = std::sin(f * Tau * .5f);
            const float x = 16 * std::cos(i * 2.4f) + 2 * std::sin(t + i + f * 3);
            const float y = 20 - 43 * f;
            const float spin = t * (i % 2 ? -1 : 1) + i;
            IceCrystal(m, Plane(camera, x, y, 11 + 2 * std::sin(i * 1.4f)),
                       Plane(camera, std::sin(spin), std::cos(spin), .3f), 2.3f + .7f * std::sin(i * 2.f), .65f, spin,
                       static_cast<uint8_t>(225 * fade * fade), camera);
        }
    } else if (kind == Kind::Light) {
        Glow(m, {}, 15, color, 190, camera);
        Glow(m, {}, 8, 0xFFFFE0, 235, camera);
        for (int ring = 0; ring < 2; ++ring) {
            auto p = [&](int j) {
                float a = Tau * j / 16 + t * (ring ? -3 : 2), r = 12;
                return ring ? Point{ r * std::cos(a), r * .65f * std::sin(a), r * .75f * std::sin(a) }
                            : Point{ r * std::cos(a), r * std::sin(a), std::sin(a * 2 + t) * 2 };
            };
            for (int j = 0; j < 16; ++j)
                Band(m, p(j), p(j + 1), 3.5f, color, 0xFFFFE9, camera);
        }
        Star(m, camera.forward * 10, 16 + 3 * std::sin(t * 4), t * .5f, 0xFFF5A3, camera);
    } else if (IsSpell(kind)) {
        const bool dark = kind == Kind::Demise, jagged = kind == Kind::Zonai || dark;
        const uint32_t edge = dark ? 0xAEBBCB : color, hot = dark ? 0xE6E6EB : 0xFFF0FF;
        for (int ring = 0; ring < 3; ++ring) {
            auto p = [&](int j) {
                float a = Tau * .68f * j / 16 + t * (ring % 2 ? -2 : 3) + ring * 2.1f;
                float r = 15.1f + (jagged ? .7f * std::sin(j * 2.8f + t * 9) : .25f * std::sin(a * 3 + t));
                if (ring == 0)
                    return Point{ r * std::cos(a), r * .4f * std::sin(a), r * .92f * std::sin(a) };
                if (ring == 1)
                    return Point{ r * .45f * std::sin(a), r * std::cos(a), r * .9f * std::sin(a) };
                return Point{ r * .7f * std::cos(a), r * .7f * std::cos(a), r * std::sin(a) };
            };
            for (int j = 0; j < 16; ++j) {
                float trail = .3f + .7f * j / 15;
                Band(m, p(j), p(j + 1), (dark ? 2.f : 2.7f) * trail, edge, hot, camera,
                     static_cast<uint8_t>(90 + 150 * trail));
            }
            Star(m, p(16), 1.8f, t * 2, edge, camera);
        }
        // A wandering discharge reaches into the tips without crossing the shell.
        auto surge = [&](int j) {
            float y = (j / 8.f - .5f) * 54;
            return Point{ 2 * std::sin(t * 7 + j * (jagged ? 2.6f : .7f)), y, 10 * (1 - std::abs(y) / 30) };
        };
        for (int j = 0; j < 8; ++j)
            Band(m, surge(j), surge(j + 1), 1.7f, edge, hot, camera,
                 static_cast<uint8_t>(100 + 100 * (.5f + .5f * std::sin(t * 6))));
        // Project all energy strictly inside the existing octahedral shell.
        // This applies after band width and camera-facing glow construction.
        for (size_t i = 0; i < m.count; ++i) {
            auto& p = m.vertices[i].p;
            float extent = (std::abs(p.x) + std::abs(p.z)) / 22.2f + std::abs(p.y) / 37.f;
            if (extent > .97f)
                p = p * (.97f / extent);
        }
    }
    return m;
}
} // namespace NeiGi
