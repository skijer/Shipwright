#include "soh/Enhancements/randomizer/NeiGiEffectPolicy.h"
#include "soh/Enhancements/randomizer/NeiGiEnergyTexture.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <set>

int main() {
  using namespace NeiGi;
  const Kind kinds[] = {Kind::Fire,  Kind::Ice,   Kind::Light,
                        Kind::Hylia, Kind::Zonai, Kind::Demise};
  for (Kind kind : kinds) {
    for (uint32_t tick : {0u, 1u, 45u, 89u, 179u, 180u, 65535u,
                          std::numeric_limits<uint32_t>::max()}) {
      const auto mesh = SampleEnergy(kind, tick);
      const auto again = SampleEnergy(kind, tick);
      assert(mesh.count > 30 && mesh.count <= mesh.vertices.size() &&
             mesh.count % 3 == 0);
      size_t bright = 0;
      for (size_t i = 0; i < mesh.count; ++i) {
        const auto &v = mesh.vertices[i];
        assert(std::isfinite(v.p.x) && std::isfinite(v.p.y) &&
               std::isfinite(v.p.z));
        assert(std::abs(v.p.x) < 45 && std::abs(v.p.y) < 45 &&
               std::abs(v.p.z) < 45);
        assert(v.p.x == again.vertices[i].p.x &&
               v.alpha == again.vertices[i].alpha);
        bright += v.alpha >= 200;
        if (kind == Kind::Hylia || kind == Kind::Zonai ||
            kind == Kind::Demise) {
          // Crystal's actual octahedral interior (30/50 author units * .74).
          assert((std::abs(v.p.x) + std::abs(v.p.z)) / 22.2f +
                     std::abs(v.p.y) / 37.f <=
                 1.001f);
        }
      }
      assert(bright >= 6); // Energy must never fade out entirely.
    }
    const auto a = SampleEnergy(kind, 0), b = SampleEnergy(kind, 19);
    bool moves = false;
    for (size_t i = 0; i < a.count && i < b.count; ++i) {
      moves |= std::abs(a.vertices[i].p.x - b.vertices[i].p.x) > .25f;
    }
    assert(moves);
  }
  assert(SampleEnergy(Kind::Neutral, 0).count == 0);
  assert(SampleShimmer(0, false).count == 0);
  const auto glints = SampleShimmer(45, true);
  assert(glints.count > 0 && glints.count <= glints.vertices.size());
  assert(ColorHex(Kind::Demise) == 0x000000);
  // Native orb texels must be stable for deferred GPU interpretation, animated,
  // bounded to one TMEM load, and empty at their border (no square billboard).
  for (Kind kind :
       {Kind::Fire, Kind::Ice, Kind::Hylia, Kind::Zonai, Kind::Demise}) {
    const auto &a = OrbTexture(kind, 0);
    assert(a.size() == 4096 && &a == &OrbTexture(kind, OrbFrameCount));
    assert(&a != &OrbTexture(kind, 1));
    size_t opaque = 0, moving = 0;
    for (size_t i = 0; i < a.size(); ++i) {
      opaque += a[i] > 170;
      moving += std::abs(int(a[i]) - int(OrbTexture(kind, 9)[i])) > 20;
      if (i < 64 || i >= 4032 || i % 64 == 0 || i % 64 == 63)
        assert(a[i] == 0);
    }
    assert(opaque > 40 && moving > 100);
    if (kind == Kind::Demise)
      assert(a[32 * 64 + 32] == 0); // Approved black center stays visible.
    for (const Basis camera : {Basis{}, Basis{{.7071f, 0, .7071f},
                                              {.4082f, .8165f, -.4082f},
                                              {-.5774f, .5774f, .5774f}}}) {
      auto orb = SampleOrb(kind, camera);
      assert(orb.count > 30 && orb.count <= orb.vertices.size());
      for (size_t i = 0; i < orb.count; ++i) {
        const auto &v = orb.vertices[i];
        assert(v.u >= 0 && v.u <= 1 && v.v >= 0 && v.v <= 1);
        if (IsSpell(kind)) {
          const float length =
              std::sqrt(v.p.x * v.p.x + v.p.y * v.p.y + v.p.z * v.p.z);
          assert(length > 14.06f); // Do not disappear behind the opaque core.
          assert((std::abs(v.p.x) + std::abs(v.p.z)) / 22.2f +
                     std::abs(v.p.y) / 37.f <
                 1.f);
        }
      }
    }
  }
  assert(SampleOrb(Kind::Light).count == 0);
  assert(SampleOrb(Kind::Leaf).count == 0);
  // Ice crystals have hard, differently colored planar faces, not soft star
  // fans. Cull their rear faces before submission because effects do not write
  // depth; rear faces must never paint over the visible crystal surface.
  for (const Basis camera :
       {Basis{}, Basis{{0, 0, 1}, {0, 1, 0}, {-1, 0, 0}}}) {
    Mesh crystal;
    IceCrystal(crystal, {}, {0.3f, 1, 0.2f}, 9, 2, .4f, 230, camera);
    assert(crystal.count > 12 && crystal.count <= 72 && crystal.count % 3 == 0);
    std::set<uint32_t> faceColors;
    for (size_t i = 0; i < crystal.count; i += 3) {
      const auto &a = crystal.vertices[i], &b = crystal.vertices[i + 1],
                 &c = crystal.vertices[i + 2];
      const auto n = Cross(b.p - a.p, c.p - a.p);
      assert(n.x * camera.forward.x + n.y * camera.forward.y +
                 n.z * camera.forward.z >
             0);
      assert(a.rgb == b.rgb && a.rgb == c.rgb);
      assert(a.alpha == 230 && b.alpha == 230 && c.alpha == 230);
      assert((a.rgb & 255) > (a.rgb >> 16));
      faceColors.insert(a.rgb);
    }
    assert(faceColors.size() >= 3);
  }
  std::cout << "NEI energy: continuous, bounded, animated, contained; "
               "independent optional shimmer passed\n";
}
