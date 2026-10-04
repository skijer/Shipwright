#include "EponaCosmeticsDL.h"

#include <cassert>
#include <cstdio>

using namespace EponaCosmetics;

static Gfx Command(uintptr_t a, uintptr_t b) {
    Gfx result{};
    result.words.w0 = a;
    result.words.w1 = b;
    return result;
}

static bool Equal(const Gfx& a, const Gfx& b) {
    return a.words.w0 == b.words.w0 && a.words.w1 == b.words.w1;
}

int main() {
    static uint8_t coatMask[4] = { 0, 0, 1, 1 };
    static uint8_t hairMask[4] = { 1, 1, 0, 0 };
    static const char name[] = "__OTR__objects/object_horse/test";
    TextureMaterial mixed{ false, { { name, { coatMask, hairMask, nullptr } } } };
    const auto resolve = [&](uint64_t hash, bool eye) {
        if (hash == 42) {
            return TextureMaterial{ true, {} }; // The palette must not replace the eye material.
        }
        return mixed;
    };
    const std::vector<Gfx> original = {
        Command(0x33000000, 0xBEEFBEEF), Command(0x06000000, 7), // Marker payload resembles triangles.
        Command(0x20100000, 0),          Command(0, 1),
        Command(0xF5100000, 0x070D0040), Command(0xE6000000, 0),
        Command(0xF3000000, 0x07003200), Command(0xF5100800, 0),
        Command(0xF2000000, 0x4004),     Command(0xFC127E03, 0xFFFFFDF8),
        Command(0xE200001C, 0xC8112078), Command(0xFA000000, 0xAA3A02FF),
        Command(0x01003006, 0x08000001), Command(0x05000204, 0),
        Command(0x05000204, 0),          Command(0xDF000000, 0),
    };
    auto unchanged = BuildNativeDisplayList(original, 0, resolve);
    assert(unchanged.size() == original.size());
    for (size_t i = 0; i < original.size(); ++i) {
        assert(Equal(unchanged[i], original[i]));
    }
    const auto changed = BuildNativeDisplayList(original, 3, resolve);
    assert(changed.size() > original.size());
    size_t originalIndex = 0, vertices = 0, triangles = 0, registrations = 0, resets = 0;
    bool coat = false, hair = false, eyes = false;
    for (size_t i = 0; i < changed.size(); ++i) {
        const auto& g = changed[i];
        if (originalIndex < original.size() && Equal(g, original[originalIndex])) {
            ++originalIndex;
        }
        const unsigned opcode = g.words.w0 >> 24;
        if (opcode == 0x01)
            ++vertices;
        if (opcode == 0x05)
            ++triangles;
        if (opcode == 0x3F) {
            ++registrations;
            ++i;
        } else if (opcode == 0x33 || opcode == 0x20 || opcode == 0x32) {
            if (originalIndex < original.size() && Equal(changed[i + 1], original[originalIndex])) {
                ++originalIndex;
            }
            ++i;
        } else if (opcode == 0xDE) {
            coat |= g.words.w1 == 0x09000001;
            hair |= g.words.w1 == 0x0A000001;
            eyes |= g.words.w1 == 0x0B000001;
            resets += g.words.w1 == 0x0C000001;
        }
    }
    assert(originalIndex == original.size());
    assert(vertices == 1); // All additional passes share the already transformed vertex cache.
    assert(triangles == 6 && registrations == 4 && resets == 2);
    assert(coat && hair && !eyes);
    assert(Equal(changed.back(), original.back()));

    auto invalid = original;
    invalid.insert(invalid.begin() + 2, Command(0xDE000000, 0x12345678));
    assert(BuildNativeDisplayList(invalid, 1, resolve).empty()); // Unsupported nested/custom geometry.
    invalid = { Command(0x20100000, 0) };
    assert(BuildNativeDisplayList(invalid, 1, resolve).empty());

    auto blinking = original;
    blinking[2] = Command(0xFD500000, 0x08000001);
    blinking.erase(blinking.begin() + 3);
    blinking.insert(blinking.begin() + 8, { Command(0x20100000, 0), Command(0, 42) });
    assert(!BuildNativeDisplayList(blinking, 1, resolve).empty());

    const auto missingBlinkMask = [&](uint64_t, bool) {
        return TextureMaterial{ false, { { name, { coatMask, nullptr, nullptr } }, { name, {} } } };
    };
    assert(BuildNativeDisplayList(original, 1, missingBlinkMask).empty());

    auto intensity = original;
    intensity[2].words.w0 = 0x20900000; // Native adult coat uses I8, whose alpha is also intensity.
    const auto pureCoat = [&](uint64_t, bool) {
        return TextureMaterial{ false, { { name, { coatMask, nullptr, nullptr } } } };
    };
    auto opaque = BuildNativeDisplayList(intensity, 1, pureCoat);
    triangles = registrations = 0;
    for (size_t i = 0; i < opaque.size(); ++i) {
        const auto opcode = opaque[i].words.w0 >> 24;
        triangles += opcode == 0x05;
        registrations += opcode == 0x3F;
        if (opcode == 0x20 || opcode == 0x32 || opcode == 0x33 || opcode == 0x3F)
            ++i;
    }
    assert(triangles == 2 && registrations == 0); // Preserve opaque I8 shading; no translucent second draw.
    puts("PASS: exact default, independent parts, vertex-cache safety, palette handling, and unsupported fallback");
}
