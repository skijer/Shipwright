// Frozen policy at user acceptance of Fire projectile, 2026-09-28.
// Fixture only: compare sampled body, surface, and trail; do not update to bless regressions.
#pragma once

#include "soh/Enhancements/randomizer/NeiGiEffectPolicy.h"
#include <algorithm>

// Item-local USED effects. Pure sampling: no actor state, RNG, texture cache,
// global override, or accumulated particle lifetime. Preview uses these meshes.
namespace AcceptedFire {
using NeiGi::Basis;
using NeiGi::Kind;
using NeiGi::Mesh;
using NeiGi::Point;
using NeiGi::Tau;

inline float Clamp(float v, float low, float high) {
    return std::isfinite(v) ? std::clamp(v, low, high) : low;
}
inline uint8_t Alpha(float v) {
    return static_cast<uint8_t>(Clamp(v, 0, 255));
}
inline Point Radial(float a, float r, float y = 0) {
    return { std::cos(a) * r, y, std::sin(a) * r };
}
inline void Fade(Mesh& m, float scale, float alpha) {
    for (size_t i = 0; i < m.count; ++i) {
        m.vertices[i].p = m.vertices[i].p * scale;
        m.vertices[i].alpha = Alpha(m.vertices[i].alpha * alpha);
    }
}
inline void Ribbon(Mesh& m, Point a, Point b, Point sideA, Point sideB, uint32_t color, uint8_t alphaA,
                   uint8_t alphaB) {
    m.Tri({ a, color, alphaA }, { a - sideA, color, 0 }, { b - sideB, color, 0 });
    m.Tri({ a, color, alphaA }, { b - sideB, color, 0 }, { b, color, alphaB });
    m.Tri({ a, color, alphaA }, { b, color, alphaB }, { b + sideB, color, 0 });
    m.Tri({ a, color, alphaA }, { b + sideB, color, 0 }, { a + sideA, color, 0 });
}
inline void Ring(Mesh& m, float radius, float height, float width, float phase, float arc, uint32_t color,
                 uint8_t alpha, int segments = 24) {
    for (int j = 0; j < segments; ++j) {
        const float a = phase + arc * j / segments, b = phase + arc * (j + 1) / segments;
        Ribbon(m, Radial(a, radius, height), Radial(b, radius, height), Radial(a, width), Radial(b, width), color,
               alpha, alpha);
    }
}
inline void Ray(Mesh& m, Point p, Point direction, float length, float width, uint32_t color, uint8_t alpha,
                const Basis& camera) {
    const Point axis = NeiGi::Unit(direction);
    const Point side = NeiGi::Unit(NeiGi::Cross(axis, camera.forward)) * width;
    const Point tip = p + axis * length;
    m.Tri({ p, 0xFFFFEF, alpha }, { p - side, color, 0 }, { tip, color, 0 });
    m.Tri({ p, 0xFFFFEF, alpha }, { tip, color, 0 }, { p + side, color, 0 });
    m.Tri({ p, 0xFFFFEF, alpha }, { p - side * .12f, color, alpha }, { tip, color, 0 });
    m.Tri({ p, 0xFFFFEF, alpha }, { tip, color, 0 }, { p + side * .12f, color, alpha });
}
inline void Frost(Mesh& m, Point p, float size, float angle, uint8_t alpha, const Basis& camera) {
    // Six angular arms, with real hard edges. No electrical zigzags or star fan.
    for (int j = 0; j < 6; ++j) {
        const float a = angle + j * Tau / 6;
        const Point tip = p + NeiGi::Plane(camera, std::cos(a) * size, std::sin(a) * size);
        const Point side = NeiGi::Plane(camera, -std::sin(a), std::cos(a)) * (size * .08f);
        m.Tri({ p - side, 0xBDEBFF, alpha }, { tip, 0xF4FFFF, alpha }, { p + side, 0xBDEBFF, alpha });
    }
}
// One tapered flame silhouette, with a warm rim and a narrow luminous core.
// Camera-facing ribbons avoid opaque crossed billboards and keep the tip readable.
inline void Flame(Mesh& m, Point origin, Point direction, float length, float width, float phase,
                  const Basis& camera, float opacity = 1, int segments = 6) {
    const Point axis = NeiGi::Unit(direction);
    Point side = NeiGi::Cross(axis, camera.forward);
    if (side.x * side.x + side.y * side.y + side.z * side.z < .001f)
        side = camera.right;
    side = NeiGi::Unit(side);
    for (int layer = 0; layer < 2; ++layer) {
        auto center = [&](float f) {
            return origin + axis * (length * f) + side * (std::sin(phase + f * 5) * width * f * .45f);
        };
        for (int j = 0; j < segments; ++j) {
            const float a = float(j) / segments, b = float(j + 1) / segments;
            const float w = width * (layer ? .38f : 1.f);
            const float wa = w * std::sin((.12f + .88f * a) * Tau * .5f) * (1 - a * .7f);
            const float wb = w * std::sin((.12f + .88f * b) * Tau * .5f) * (1 - b * .7f);
            const uint32_t color = layer ? (a < .5f ? 0xFFF2AD : 0xFFD35B) : (a < .5f ? 0xFFAF36 : 0xF45D19);
            Ribbon(m, center(a), center(b), side * wa, side * wb, color,
                   Alpha((layer ? 235 : 185) * (1 - a * a) * opacity),
                   Alpha((layer ? 235 : 185) * (1 - b * b) * opacity));
        }
    }
}
inline Mesh SampleProjectile(Kind kind, uint32_t frame, float scale, Point direction, const Basis& camera = {}) {
    Mesh m;
    scale = Clamp(scale, 0, 4);
    if (scale <= .001f || !NeiGi::IsRod(kind))
        return m;
    const float t = NeiGi::Time(frame);
    const Point axis = NeiGi::Unit(direction);
    if (kind == Kind::Ice) {
        // A luminous frost bolt, with crystalline detail inside its energy body.
        NeiGi::Glow(m, {}, 23, 0x6DCCFF, 155, camera);
        NeiGi::Glow(m, axis * 3.f, 10, 0xF1FFFF, 250, camera);
        NeiGi::IceCrystal(m, axis * 4.f, axis, 29, 4, .25f, 230, camera);
        Ray(m, axis * 2.f, axis, 32, 6, 0xD9FFFF, 255, camera);
        NeiGi::Band(m, axis * -65.f, axis * 6.f, 5, 0x60BEF2, 0xFFFFFF, camera, 200);
        for (int j = 0; j < 4; ++j) {
            const float a = j * Tau / 4 + t;
            const Point p = axis * (-13.f - j * 12.f) + NeiGi::Plane(camera, std::cos(a)*10, std::sin(a)*10);
            NeiGi::Star(m, p, 3.5f, a, 0xC9F8FF, camera);
        }
    } else if (kind == Kind::Fire) {
        NeiGi::Glow(m, {}, 24, 0xFF6B18, 160, camera);
        NeiGi::Glow(m, axis * 3.f, 11, 0xFFF4CE, 250, camera);
        Ray(m, axis * 3.f, axis, 22, 6, 0xFFDF8D, 255, camera);
        Flame(m, axis * 7.f, axis * -1.f, 78, 19, t * 4, camera, 1, 3);
    } else {
        NeiGi::Glow(m, {}, 19, 0xF4D25B, 112, camera);
        NeiGi::Glow(m, {}, 9, 0xFFFFE8, 245, camera);
        for (int i = 0; i < 12; ++i) {
            const float a = t * .12f + i * Tau / 12;
            const float pulse = 1 + .09f * std::sin(t * 3 + i);
            Ray(m, {}, NeiGi::Plane(camera, std::cos(a), std::sin(a)), (i % 3 ? 15 : 26) * pulse, i % 3 ? 2.1f : 3.6f,
                0xFFE88A, 225, camera);
        }
        // A concentrated forward streak and flowing shoulders give direction.
        Ray(m, {}, axis, 24, 4.2f, 0xFFF3B8, 255, camera);
        for (int j = 0; j < 6; ++j) {
            const float a = t * 2 + j * Tau / 6;
            const Point p = NeiGi::Plane(camera, std::cos(a) * 10, std::sin(a) * 10);
            NeiGi::Star(m, p, 1.7f, a, 0xFFF2B8, camera);
        }
    }
    Fade(m, scale * .5f, Clamp(scale * 2, 0, 1));
    return m;
}
inline Mesh SampleTrail(Kind kind, uint32_t frame, const Point* points, size_t count, float scale,
                        const Basis& camera = {}) {
    Mesh m;
    scale = Clamp(scale, 0, 4);
    if (!points || count < 2 || scale <= .001f || !NeiGi::IsRod(kind))
        return m;
    count = std::min(count, size_t(6));
    const float t = NeiGi::Time(frame);
    for (size_t i = 1; i < count; ++i) {
        const float f = 1.f - float(i) / count;
        if (kind == Kind::Light) {
            const Point side = NeiGi::Unit(NeiGi::Cross(points[i] - points[i - 1], camera.forward));
            Ribbon(m, points[i - 1], points[i], side * ((f + .18f) * 5 * scale), side * (f * 5 * scale), 0xFFE797,
                   Alpha(230 * (f + .1f)), Alpha(230 * f));
            NeiGi::Band(m, points[i - 1], points[i], f * scale, 0xFFF4C2, 0xFFFFEE, camera, Alpha(230 * f));
        } else {
            const Point side = NeiGi::Unit(NeiGi::Cross(points[i] - points[i - 1], camera.forward));
            const uint32_t color = kind == Kind::Fire ? 0xFF902D : 0x8BDDFF;
            Ribbon(m, points[i - 1], points[i], side * ((f + .15f) * 9 * scale), side * (f * 9 * scale),
                   color, Alpha(210 * (f + .1f)), Alpha(210 * f));
            NeiGi::Band(m, points[i - 1], points[i], 2 * f * scale,
                        kind == Kind::Fire ? 0xFFD28D : 0xD7FAFF, 0xFFFFFF, camera, Alpha(230*f));

        }
    }
    return m;
}
inline Mesh SampleChargeSparks(Kind kind, uint32_t frame, float charge, const Basis& camera = {}) {
    Mesh m;
    charge = Clamp(charge, 0, 1);
    if (charge <= 0 || !NeiGi::IsRod(kind))
        return m;
    if (kind == Kind::Fire) {
        for (int i = 0; i < 3; ++i) {
            const float f = std::fmod((frame % 180u) / 55.f + i / 3.f, 1.f);
            const Point p{std::sin(i * 2.4f + f * 2) * 4, 7 + 27 * f, std::cos(i * 2.4f) * 3};
            NeiGi::Glow(m, p, .5f + .4f * charge, 0xFFC46D, Alpha(190 * std::sin(f * Tau * .5f)), camera);
        }
        return m;
    }
    const float t = NeiGi::Time(frame), r = 5 + 9 * charge;
    for (int i = 0; i < 8; ++i) {
        const float f = std::fmod((frame % 180u) / 45.f + i / 8.f, 1.f);
        const float a = i * 2.39996f + t;
        Point p = Radial(a, r * (1 - .55f * f), (kind == Kind::Ice ? 13 - 24 * f : -6 + 27 * f));
        const uint8_t alpha = Alpha(240 * std::sin(f * Tau * .5f));
        if (kind == Kind::Ice) {
            NeiGi::IceCrystal(m, p, { std::sin(a), 1, std::cos(a) }, 3.5f + charge, .95f, a + t, alpha, camera);
        } else if (kind == Kind::Fire) {
            Ray(m, p, camera.up + camera.right * (std::sin(a) * .25f), 3 + 5 * charge, 1.4f, 0xFF9C35, alpha, camera);
        } else {
            NeiGi::Glow(m, p, 3, 0xFFE070, alpha / 2, camera);
            Ray(m, p, camera.up, 4 + 3 * charge, .8f, 0xFFF1A6, alpha, camera);
            Ray(m, p, camera.up * -1.f, 2, .5f, 0xFFF1A6, alpha, camera);
        }
    }
    Fade(m, .6f + .4f * charge, .45f + .55f * charge);
    return m;
}
inline Mesh SampleCharge(Kind kind, uint32_t frame, float charge, const Basis& camera = {}) {
    Mesh m;
    charge = Clamp(charge, 0, 1);
    if (charge <= 0 || !NeiGi::IsRod(kind))
        return m;
    const float t = NeiGi::Time(frame), radius = 17 + 18 * charge;
    if (kind == Kind::Ice) {
        // Frost forms from the floor upward: radial ice teeth and snow crystals.
        Ring(m, radius, 1, 5, 0, Tau, 0xB9EBFF, 170, 20);
        for (int i = 0; i < 12; ++i) {
            const float f = std::fmod((frame % 180u) / 60.f + i / 12.f, 1.f);
            const float a = i * Tau / 12 + t * .12f, pulse = std::sin(f * Tau * .5f);
            const float length = (14 + 20 * charge) * pulse;
            const Point p = Radial(a, radius, 8 + 25 * (1 - f));
            NeiGi::IceCrystal(m, p, Radial(a + f, .3f, 1), length, 2.7f * pulse, a + f, Alpha(230 * pulse), camera);
        }
        for (int i = 0; i < 6; ++i) {
            const float f = std::fmod((frame % 180u) / 90.f + i / 6.f, 1.f);
            Frost(m, Radial(i * 2.4f + t * .3f, radius * .85f, 48 * (1 - f)), 2 + charge, t * .4f,
                  Alpha(200 * std::sin(f * Tau * .5f)), camera);
        }
    } else if (kind == Kind::Fire) {
        Flame(m, {0, -2, 0}, {0, 1, 0}, 12 + 19 * charge, 3 + 3 * charge, t * 3, camera);
    } else {
        Ring(m, radius, 2, 5, t * .2f, Tau, 0xFFF0A8, 230, 24);
        Ring(m, radius * .73f, 6, 2.5f, -t * .3f, Tau, 0xFFF9C9, 180, 20);
        for (int i = 0; i < 14; ++i) {
            const float a = i * Tau / 14 + t * .14f;
            const float pulse = .75f + .25f * std::sin(t * 3 + i * 2.4f);
            Ray(m, Radial(a, radius, 3), { 0, 1, 0 }, (34 + 45 * charge) * pulse, 3.2f, 0xFFE47A, 238, camera);
        }
        NeiGi::Glow(m, { 0, 20, 0 }, 27 + 12 * charge, 0xFFF0AD, 65, camera);
    }
    Fade(m, 1, .4f + .6f * charge);
    return m;
}
// Native gEffFireCircleDL: radius ~849, height 2400; caller scales XZ by
// radius/1000 and Y by .08. Preserve that full 192-unit wall silhouette.
inline Mesh SampleSpin(Kind kind, uint32_t frame, float radius, bool big, const Basis& camera = {}) {
    Mesh m;
    radius = Clamp(radius, 0, 500);
    if (radius <= 0 || !NeiGi::IsRod(kind)) return m;
    const float edge = radius * .849f * (1 + .05f * std::sin((frame % 16u) * Tau / 16));
    Ring(m, edge, 3, std::min(edge*.3f, big ? 25.f : 18.f), 0, Tau,
         kind == Kind::Fire ? 0xFFCA7A : kind == Kind::Ice ? 0xBEF4FF : 0xFFF4BE, 225, 32);
    return m;
}
inline Mesh SampleSpinSurface(Kind kind, uint32_t frame, float radius, bool big) {
    Mesh m;
    radius = Clamp(radius, 0, 500);
    if (radius <= 0 || !NeiGi::IsRod(kind)) return m;
    const float edge = radius * .849f * (1 + .05f * std::sin((frame % 16u) * Tau / 16));
    constexpr int segments=48, rows=3;
    const float scroll = (frame % 720u) / 720.f;
    // Continuous native-height cylinder; the artwork supplies the flame/frost
    // silhouette, not sparse spikes or a top-edge opacity fade.
    auto v = [&](int j, int row) {
        const float f = row / float(rows), a = j * Tau / segments;
        // Mirrored neighboring panels share exact edge texels. Rotate geometry
        // slowly to animate the artwork without an authored wrap seam.
        const float tile = j / 8.f;
        const float u = 1 - std::abs(std::fmod(tile,2.f)-1);
        return NeiGi::EffectVertex{Radial(a+scroll*Tau,edge*(1+.025f*std::sin(a*5+scroll*Tau*24)*std::sin(f*Tau*.5f)),192*f),0xFFFFFF,uint8_t(big?245:225),u,1-f};
    };
    for(int j=0;j<segments;++j) for(int row=0;row<rows;++row) {
        m.Tri(v(j,row),v(j+1,row),v(j+1,row+1));
        m.Tri(v(j,row),v(j+1,row+1),v(j,row+1));
    }
    return m;
}
// A substantial textured wake belongs to each moving head. The original set
// still owns one history trail; no projectile/gameplay state is added.
inline Mesh SampleProjectileSurface(Kind kind, uint32_t frame, float scale, Point direction, const Basis& camera={}) {
    Mesh m;
    if ((kind!=Kind::Fire && kind!=Kind::Ice) || !std::isfinite(scale) || scale<=0) return m;
    scale = Clamp(scale,0,4)*.5f;
    const Point axis = NeiGi::Unit(direction);
    Point side = NeiGi::Cross(axis,camera.forward);
    if(side.x*side.x+side.y*side.y+side.z*side.z<.001f) side=camera.right;
    side=NeiGi::Unit(side);
    constexpr int segments=8;
    const float t=NeiGi::Time(frame);
    auto v=[&](int j, int edge) {
        const float f=j/float(segments);
        const float width=(2+26*std::sin(f*Tau*.5f))*(1-f*.35f);
        const Point p=axis*(9-102*f)+side*(std::sin(t*3+f*8)*4*f+edge*width);
        return NeiGi::EffectVertex{p*scale,0xFFFFFF,Alpha(245*(1-f*.55f)),(edge+1)*.5f,1-f};
    };
    for(int j=0;j<segments;++j){m.Tri(v(j,-1),v(j,1),v(j+1,1));m.Tri(v(j,-1),v(j+1,1),v(j+1,-1));}
    return m;
}
inline Mesh SampleBurst(Kind kind, uint32_t frame, float life, const Basis& camera = {}) {
    Mesh m;
    life = Clamp(life, 0, 1);
    if (life <= 0 || (kind != Kind::Ice && kind != Kind::Light))
        return m;
    const float t = NeiGi::Time(frame);
    if (kind == Kind::Ice) {
        NeiGi::Glow(m, { 0, 5, 0 }, 24, 0x90D7EA, 60, camera);
        for (int i = 0; i < 7; ++i) {
            const float a = Tau * i / 7, length = i ? 25 + 6 * std::sin(i) : 46;
            NeiGi::IceCrystal(m, Radial(a, i ? 9 : 0, length * .33f), Radial(a, i ? .25f : 0, 1), length, i ? 3.8f : 5,
                              a, 232, camera);
        }
    } else {
        NeiGi::Glow(m, { 0, 12, 0 }, 28, 0xFFE279, 80, camera);
        NeiGi::Glow(m, { 0, 10, 0 }, 10, 0xFFFFEB, 240, camera);
        for (int i = 0; i < 9; ++i) {
            const float a = Tau * i / 9;
            Ray(m, Radial(a, i ? 9 : 0, 0), { 0, 1, 0 }, 50 + 23 * std::sin(i * 2.4f + t * .5f), 3.4f, 0xFFE8A0, 240,
                camera);
        }
    }
    Fade(m, .6f + .4f * std::min(1.f, life * 4), std::min(1.f, life * 4));
    return m;
}
inline Mesh SamplePortal(uint32_t frame, float scale, float alpha, const Basis& camera = {}) {
    Mesh m;
    scale = Clamp(scale, 0, 1.25f);
    alpha = Clamp(alpha, 0, 255);
    if (scale <= 0 || alpha <= 0)
        return m;
    const float t = NeiGi::Time(frame);
    const Basis floor{ { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } };
    NeiGi::Glow(m, { 0, 1, 0 }, 65, 0x6777E9, 150, floor);
    Ring(m, 70, 2, 5, t * .25f, Tau, 0x99EFFF, 240, 24);
    Ring(m, 55, 3, 2.6f, -t * .6f, Tau * .84f, 0xDBCEFF, 240, 24);
    Ring(m, 37, 4, 4, t * 1.1f, Tau * .88f, 0x7FFFF0, 215, 20);
    // Twelve hour marks form a recognizable temporal dial, with longer
    // cardinal marks. These rotate independently from the outer energy rim.
    for (int i = 0; i < 12; ++i) {
        const float a = i * Tau / 12 - t * .16f;
        const Point p = Radial(a, 62, 3), side = Radial(a + Tau / 4, 1.3f);
        const Point end = Radial(a, i % 3 ? 65 : 68, 3);
        m.Tri({ p - side, 0xD3FAFF, 230 }, { end - side, 0xD3FAFF, 230 }, { end + side, 0xD3FAFF, 230 });
        m.Tri({ p - side, 0xD3FAFF, 230 }, { end + side, 0xD3FAFF, 230 }, { p + side, 0xD3FAFF, 230 });
    }
    // Wisps open upward around the player rather than placing an opaque disc
    // over them. Their alpha softens to zero at both ends.
    for (int i = 0; i < 4; ++i) {
        auto p = [&](int j) {
            const float f = j / 9.f, a = i * Tau / 4 + t * (i % 2 ? -.8f : .8f) + f * 1.25f;
            return Radial(a, 48 + 14 * f, f * 83);
        };
        for (int j = 0; j < 9; ++j) {
            const float f = j / 9.f, g = (j + 1) / 9.f;
            const Point side = camera.right * (2.8f * (1 - f) + .4f);
            Ribbon(m, p(j), p(j + 1), side, side * .85f, i % 2 ? 0xBEA4FF : 0xA1FFFF,
                   Alpha(210 * std::sin(f * Tau * .5f)), Alpha(210 * std::sin(g * Tau * .5f)));
        }
    }
    Fade(m, scale, alpha / 255);
    return m;
}

// Surface layers are sampled independently so the common renderer can retain
// its existing geometry-only fallback whenever a private resource is absent.
inline bool IsIceFace(const Mesh& source, size_t i) {
    const auto& a = source.vertices[i];
    const auto& b = source.vertices[i + 1];
    const auto& c = source.vertices[i + 2];
    return a.alpha && a.alpha == b.alpha && b.alpha == c.alpha && a.rgb == b.rgb && b.rgb == c.rgb;
}
inline Mesh IceSurface(const Mesh& source) {
    Mesh m;
    for (size_t i = 0; i < source.count; i += 3) {
        auto a = source.vertices[i], b = source.vertices[i + 1], c = source.vertices[i + 2];
        if (!IsIceFace(source, i))
            continue;
        auto uv = [](NeiGi::EffectVertex& v) {
            v.u = v.p.x * .035f + v.p.z * .015f;
            v.v = v.p.y * .05f + v.p.z * .025f;
        };
        uv(a);
        uv(b);
        uv(c);
        m.Tri(a, b, c);
    }
    return m;
}
inline Mesh IceAtmosphere(const Mesh& source) {
    Mesh m;
    for (size_t i = 0; i < source.count; i += 3)
        if (!IsIceFace(source, i))
            m.Tri(source.vertices[i], source.vertices[i + 1], source.vertices[i + 2]);
    return m;
}
inline Mesh CylinderSurface(uint32_t frame, float radius, float height, uint32_t bottom, uint32_t top, float alpha,
                            int repeats = 3) {
    Mesh m;
    const float t = NeiGi::Time(frame);
    constexpr int segments = 28, rows = 4;
    auto vertex = [&](int j, int row) {
        const float f = row / float(rows), a = Tau * j / segments;
        const float r = radius * (1 + .12f * f * std::sin(a * 3 + t * 2));
        const Point p = Radial(a + t * .25f + f * .3f, r, height * f);
        const float blend = f;
        uint32_t rgb = 0;
        for (int shift : { 16, 8, 0 }) {
            const float low = (bottom >> shift) & 255, high = (top >> shift) & 255;
            rgb |= uint32_t(low + (high - low) * blend) << shift;
        }
        return NeiGi::EffectVertex{ p, rgb, Alpha(alpha * (1 - f * f)),
                                    j * repeats / float(segments) + (frame % 180u) / 180.f, 1 - f };
    };
    for (int row = 0; row < rows; ++row)
        for (int j = 0; j < segments; ++j) {
            m.Tri(vertex(j, row), vertex(j + 1, row), vertex(j + 1, row + 1));
            m.Tri(vertex(j, row), vertex(j + 1, row + 1), vertex(j, row + 1));
        }
    return m;
}
inline Mesh SampleChargeSurface(Kind kind, uint32_t frame, float charge, const Basis& camera = {}) {
    charge = Clamp(charge, 0, 1);
    if (charge <= 0)
        return {};
    if (kind == Kind::Fire) return {};
    if (kind == Kind::Ice)
        return IceSurface(SampleCharge(kind, frame, charge, camera));
    if (kind != Kind::Fire && kind != Kind::Light)
        return {};
    return CylinderSurface(frame, 17 + 18 * charge, 30 + 47 * charge, kind == Kind::Fire ? 0xFFF1B8 : 0xFFFFDD,
                           kind == Kind::Fire ? 0xFF861C : 0xFFE486, 130 + 100 * charge, kind == Kind::Fire ? 3 : 2);
}
inline Mesh SamplePortalSurface(uint32_t frame, float scale, float alpha) {
    scale = Clamp(scale, 0, 1.25f);
    alpha = Clamp(alpha, 0, 255);
    if (scale <= 0 || alpha <= 0)
        return {};
    auto m = CylinderSurface(frame, 55, 96, 0x68EFFF, 0xBC9EFF, 150, 3);
    Fade(m, scale, alpha / 255);
    return m;
}
} // namespace NeiUsedMagic
