#include "soh/Enhancements/randomizer/NeiLanternEffectPolicy.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

static uint64_t Shape(const NeiGi::Mesh& mesh) {
    uint64_t hash = 1469598103934665603ull;
    for (size_t i = 0; i < mesh.count; ++i) {
        const auto& p = mesh.vertices[i].p;
        for (float value : { p.x, p.y, p.z }) {
            hash ^= uint32_t(std::lround(value * 16));
            hash *= 1099511628211ull;
        }
    }
    return hash;
}

static void Contained(const NeiGi::Mesh& mesh) {
    assert(mesh.count <= 1536 && mesh.count % 3 == 0);
    for (size_t i = 0; i < mesh.count; ++i) {
        const auto p = mesh.vertices[i].p;
        assert(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
        // Serialized positions must remain inside the physical glass chamber,
        // including the half-quantum error of the native Vtx upload.
        const float x = std::round(p.x * 16) / 16;
        const float y = std::round(p.y * 16) / 16;
        const float z = std::round(p.z * 16) / 16;
        assert(x * x + z * z < 18.6f * 18.6f);
        assert(y > -27 && y < 25);
    }
}

int main() {
    assert(NeiLantern::SampleCore(0, 0).count == 0);
    assert(NeiLantern::SampleAccents(0, 0).count == 0);
    assert(NeiLantern::SampleCore(5, 0).count == 0);
    assert(NeiLantern::SampleAccents(255, 0).count == 0);
    uint64_t shapes[4]{};
    for (uint8_t fire = 1; fire <= 4; ++fire) {
        assert(NeiLantern::SampleCore(fire, 0).count > 0);
        assert(NeiLantern::SampleAccents(fire, 0).count > 0);
        shapes[fire - 1] = Shape(NeiLantern::SampleAccents(fire, 0));
        assert(shapes[fire - 1] != Shape(NeiLantern::SampleAccents(fire, 47)));
        assert(shapes[fire - 1] == Shape(NeiLantern::SampleAccents(fire, 180)));
        for (uint32_t frame = 0; frame < 180; ++frame) {
            for (float angle : { 0.f, .5f, 1.5f, 3.f, 5.f }) {
                const NeiGi::Basis camera{ { std::cos(angle), 0, std::sin(angle) },
                                          { 0, 1, 0 },
                                          { -std::sin(angle), 0, std::cos(angle) } };
                Contained(NeiLantern::SampleCore(fire, frame, camera));
                Contained(NeiLantern::SampleAccents(fire, frame, camera));
            }
        }
    }
    for (int i = 0; i < 4; ++i)
        for (int j = i + 1; j < 4; ++j)
            assert(shapes[i] != shapes[j]); // Motion/geometry differ, independently of RGB.
    std::cout << "PASS: lantern variants, animation, invalid/unlit states and chamber containment\n";
}
