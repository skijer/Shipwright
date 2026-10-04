#include "test_require.h"
#include "../soh/Enhancements/Graphics/NativeMaterialProfile.h"
#include <iostream>

using namespace Prelude;
using nlohmann::json;

static void CheckSagePlatformCommandSafety() {
    // Main platform prefix from kenjyanoma_room_0DL_001020, with the scene's
    // environment color made explicit and its 0A scroll call omitted.
    const std::vector<NativeMaterialCommand> material = {
        { 0xe7000000, 0 },          { 0xe3001001, 0 },          { 0xd7000002, 0xffffffff }, { 0x20100000, 0 },
        { 0xa3be9376, 0xabcb60ef }, { 0xf5100000, 0x07017c5e }, { 0xe6000000, 0 },          { 0xf3000000, 0x073ff100 },
        { 0xe7000000, 0 },          { 0xf5101000, 0x00017c5e }, { 0xf2000000, 0x0007c07c }, { 0xe8000000, 0 },
        { 0xf5101000, 0x0101785f }, { 0xf2000000, 0x0107c07c }, { 0xfc267e04, 0x1ffcfdf8 }, { 0xe200001c, 0xc8112078 },
        { 0xd9f1fbff, 0 },          { 0xd9ffffff, 0x00010000 }, { 0xfa000000, 0xffffffff }, { 0xfb000000, 0x80808080 },
        { 0xdf000000, 0 },
    };
    const auto profile = NativeMaterialProfile::ChamberOfSagesPlatform;
    REQUIRE(FindNativeScrollInsertion(material, profile) == 20);
    REQUIRE(!FindNativeScrollInsertion(material, NativeMaterialProfile::LakeHylia));
    REQUIRE(!FindNativeScrollInsertion(material, NativeMaterialProfile::WaterTempleCaustics));
    auto draw = material;
    draw.insert(draw.end() - 1, { { 0x32004008, 0 }, { 0xde000000, 0x0a000001 }, { 0x06000204, 0x00040600 } });
    REQUIRE(FindNativeScrollInsertion(draw, profile) == 22); // DE is vertex hash data.
    auto authoredShifts = material;
    authoredShifts[9].w1 = 0x00014050;
    authoredShifts[12].w1 = 0x01014050;
    REQUIRE(FindNativeScrollInsertion(authoredShifts, profile) == 20);
    auto replacedTexture = material;
    replacedTexture[4] = { 0xdf000000, 0x0a000001 }; // Replacement hashes are not opcodes.
    REQUIRE(FindNativeScrollInsertion(replacedTexture, profile) == 20);

    for (size_t i : { 9u, 10u, 12u, 13u }) {
        auto invalid = material;
        invalid.erase(invalid.begin() + i);
        REQUIRE(!FindNativeScrollInsertion(invalid, profile));
        invalid = material;
        invalid.insert(invalid.begin() + i, invalid[i]);
        REQUIRE(!FindNativeScrollInsertion(invalid, profile));
    }
    for (size_t i : { 9u, 12u }) {
        for (uintptr_t bits : { 1u << 18, 1u << 19, 1u << 8, 1u << 9, 1u << 14, 1u << 4 }) {
            auto invalid = material;
            invalid[i].w1 ^= bits; // Clamp, mirror, or the wrong logical wrapping period.
            REQUIRE(!FindNativeScrollInsertion(invalid, profile));
        }
        for (uintptr_t descriptor : { 0xf5681000u, 0xf5102000u, 0xf5101100u }) {
            auto invalid = material;
            invalid[i].w0 = descriptor; // Native layers share RGBA16, line 8, TMEM 0.
            REQUIRE(!FindNativeScrollInsertion(invalid, profile));
        }
    }
    for (size_t i : { 10u, 13u }) {
        auto invalid = material;
        invalid[i].w1 ^= 4;
        REQUIRE(!FindNativeScrollInsertion(invalid, profile));
        invalid = material;
        invalid[i].w0 |= 4;
        REQUIRE(!FindNativeScrollInsertion(invalid, profile));
        invalid = material;
        invalid[i].w1 = (invalid[i].w1 & 0x01000000) | 0x000fc0fc;
        REQUIRE(!FindNativeScrollInsertion(invalid, profile));
    }
    for (const auto command :
         { NativeMaterialCommand{ 0xde000000, 0x0a000001 }, NativeMaterialCommand{ 0xde000000, 0x08000001 },
           NativeMaterialCommand{ 0xde000000, 0x09000001 }, NativeMaterialCommand{ 0xda000000, 0 },
           NativeMaterialCommand{ 0xdb000000, 0 }, NativeMaterialCommand{ 0x4a000000, 0 } }) {
        auto invalid = material;
        invalid.insert(invalid.end() - 1, command);
        REQUIRE(!FindNativeScrollInsertion(invalid, profile));
    }
    auto invalid = draw;
    invalid.insert(invalid.end() - 1, { 0xe7000000, 0 });
    REQUIRE(!FindNativeScrollInsertion(invalid, profile));
    invalid = draw;
    invalid.erase(invalid.end() - 2); // A vertex load without its draw is not a material-only list.
    REQUIRE(!FindNativeScrollInsertion(invalid, profile));
    invalid = material;
    invalid.back() = { 0x20100000, 0 };
    REQUIRE(!FindNativeScrollInsertion(invalid, profile));
    invalid = material;
    invalid.back().w1 = 1;
    REQUIRE(!FindNativeScrollInsertion(invalid, profile));
    invalid = material;
    invalid.push_back({ 0xe7000000, 0 });
    REQUIRE(!FindNativeScrollInsertion(invalid, profile));
}

int main() {
    REQUIRE(static_cast<int>(NativeMaterialProfile::FountainCentral64) == 9);
    REQUIRE(static_cast<int>(NativeMaterialProfile::WaterTempleCaustics) == 10);
    REQUIRE(static_cast<int>(NativeMaterialProfile::ZorasDomainCaustics) == 11);
    auto lake = json::parse(
        R"({"chain":[{"path":"objects/object_spot06_objects/gLakeHyliaHighWaterDL","upto":248}],"placed":{"recipe":"water"}})");
    REQUIRE(ResolveNativeMaterial(lake, true) == NativeMaterialProfile::LakeHylia);
    auto pool = lake;
    pool["chain"][0]["path"] = "objects/object_spot01_objects/gKakarikoWellWaterDL";
    REQUIRE(ResolveNativeMaterial(pool, true) == NativeMaterialProfile::Pool);
    // Current art, placement UUIDs and destination paths are not behavior identity.
    lake["texture"] = "fire-water-4096";
    lake["newDlPath"] = "custom/prelude/arbitrary_room/paste900";
    lake["placed"]["recipe"] = "a-new-recipe";
    REQUIRE(ResolveNativeMaterial(lake, true) == NativeMaterialProfile::LakeHylia);
    auto light = json::parse(
        R"({"chain":[],"stored":{"textures":[{"label":"spot10_room_1Tex_008030"},{"label":"spot10_room_1Tex_008030"}],"texPaths":["custom/new-art","custom/second"]},"placed":{"recipe":"user:arbitrary"}})");
    REQUIRE(ResolveNativeMaterial(light, false) == NativeMaterialProfile::LostWoodsLightSheet);
    light["stored"]["textures"][0]["w"] = 4096;
    light["stored"]["textures"][0]["rgba"] = { { "$blob", 999 } };
    light["stored"]["texPaths"][0] = "a-different-replacement";
    REQUIRE(ResolveNativeMaterial(light, false) == NativeMaterialProfile::LostWoodsLightSheet);
    light["stored"]["textures"][1]["label"] = "unrelated";
    REQUIRE(ResolveNativeMaterial(light, false) == NativeMaterialProfile::None);
    lake["chain"].push_back(pool["chain"][0]);
    REQUIRE(ResolveNativeMaterial(lake, true) == NativeMaterialProfile::None);
    REQUIRE(ResolveNativeMaterial(json::object(), false) == NativeMaterialProfile::None);

    std::vector<NativeMaterialCommand> dl = { { 0x33000000, 0 },          { 0xde000000, 0x08000000 },
                                              { 0x20100000, 0 },          { 0xdeadbeef, 42 },
                                              { 0xf2000000, 0x007cc07c }, { 0xf2000000, 0x017cc07c },
                                              { 0x32001002, 0 },          { 0xdf000000, 55 },
                                              { 0x06000204, 0x00040600 }, { 0xdf000000, 0 } };
    REQUIRE(FindNativeScrollInsertion(dl) == 8); // DE and DF above are hash payloads.
    auto native = dl;
    native.insert(native.begin() + 8, { 0xde000000, 0x08000000 });
    REQUIRE(!FindNativeScrollInsertion(native));
    auto multi = dl;
    multi.insert(multi.end() - 1, { 0xf2000000, 0x007cc07c });
    REQUIRE(!FindNativeScrollInsertion(multi));
    auto empty = dl;
    empty.erase(empty.begin() + 8);
    REQUIRE(!FindNativeScrollInsertion(empty));
    auto truncated = dl;
    truncated.pop_back();
    REQUIRE(!FindNativeScrollInsertion(truncated));

    for (uint32_t f : { 0u, 1u, 127u, 128u, 2047u, 2048u, 0xffffffffu }) {
        auto a = NativeScrollParameters(NativeMaterialProfile::LakeHylia, f, 77);
        auto b = NativeScrollParameters(NativeMaterialProfile::Pool, f, 77);
        auto c = NativeScrollParameters(NativeMaterialProfile::LostWoodsLightSheet, 77, f);
        REQUIRE(a.x1 == 0u - f && a.y1 == f && a.x2 == f && a.y2 == f);
        REQUIRE(b.x1 == 127 - f % 128 && b.y1 == (f & 127) && b.x2 == f % 128);
        REQUIRE(a.width == 32 && a.height == 32 && a.dx1 == -1 && a.dy1 == 1);
        REQUIRE(b.width == 32 && b.height == 32 && b.dx1 == -1 && b.dy1 == 1);
        REQUIRE(c.x1 == f % 128 && c.x2 == f % 128 && c.y1 == 0 && c.y2 == 0);
        REQUIRE(c.width == 32 && c.height == 16 && c.dx1 == 1 && c.dy1 == 0);
    }
    auto declared = pool;
    declared["nativeAnimation"] = { { "version", 1 },
                                    { "source", "mm.bg_keikoku_spr.lower_a" },
                                    { "binding", "material-motion" },
                                    { "logicalWidth", 32 },
                                    { "logicalHeight", 32 } };
    REQUIRE(ResolveNativeMaterial(declared, true) == NativeMaterialProfile::FountainLowerA32);
    for (auto bad : { json(nullptr), json(true), json(1), json("x") }) {
        auto copy = declared;
        copy["nativeAnimation"] = bad;
        REQUIRE(ResolveNativeMaterial(copy, true) == NativeMaterialProfile::None);
    }
    for (const char* field : { "version", "source", "binding", "logicalWidth", "logicalHeight" }) {
        auto copy = declared;
        copy["nativeAnimation"].erase(field);
        REQUIRE(ResolveNativeMaterial(copy, true) == NativeMaterialProfile::None);
        for (auto bad : { json(nullptr), json(true), json(1.0), json("bad") }) {
            copy = declared;
            copy["nativeAnimation"][field] = bad;
            REQUIRE(ResolveNativeMaterial(copy, true) == NativeMaterialProfile::None);
        }
    }
    auto caustics = json::parse(
        R"({"nativeAnimation":{"version":1,"binding":"material-motion","source":"oot.water_temple.caustics","logicalWidth":32,"logicalHeight":32},"chain":[{"path":"arbitrary"}],"stored":{"textures":[{"label":"arbitrary"}]},"geometry":"arbitrary","texture":"arbitrary"})");
    REQUIRE(ResolveNativeMaterial(caustics, true) == NativeMaterialProfile::WaterTempleCaustics);
    REQUIRE(ResolveNativeMaterial(caustics, false) == NativeMaterialProfile::WaterTempleCaustics);
    auto domainCaustics = caustics;
    domainCaustics["nativeAnimation"]["source"] = "oot.zoras_domain.caustics";
    REQUIRE(static_cast<int>(ResolveNativeMaterial(domainCaustics, true)) == 11);
    REQUIRE(static_cast<int>(ResolveNativeMaterial(domainCaustics, false)) == 11);
    auto sagePlatform = caustics;
    sagePlatform["nativeAnimation"]["source"] = "oot.chamber_of_sages.platform";
    REQUIRE(ResolveNativeMaterial(sagePlatform, true) == NativeMaterialProfile::ChamberOfSagesPlatform);
    REQUIRE(ResolveNativeMaterial(sagePlatform, false) == NativeMaterialProfile::ChamberOfSagesPlatform);
    auto undeclaredPlatform = sagePlatform;
    undeclaredPlatform.erase("nativeAnimation");
    undeclaredPlatform["chain"][0]["path"] = "scenes/shared/kenjyanoma_scene/kenjyanoma_room_0DL_001020";
    undeclaredPlatform["stored"]["textures"] =
        json::array({ { { "label", "kenjyanoma_room_0Tex_00D618" } }, { { "label", "kenjyanoma_room_0Tex_00D618" } } });
    REQUIRE(ResolveNativeMaterial(undeclaredPlatform, true) == NativeMaterialProfile::None);
    REQUIRE(ResolveNativeMaterial(undeclaredPlatform, false) == NativeMaterialProfile::None);
    auto undeclaredCaustics = caustics;
    undeclaredCaustics.erase("nativeAnimation");
    REQUIRE(ResolveNativeMaterial(undeclaredCaustics, true) == NativeMaterialProfile::None);
    REQUIRE(ResolveNativeMaterial(undeclaredCaustics, false) == NativeMaterialProfile::None);
    for (const auto& source : { caustics, domainCaustics, sagePlatform }) {
        for (auto bad : { json(nullptr), json(true), json(2), json(1.0), json("1") }) {
            auto copy = source;
            copy["nativeAnimation"]["version"] = bad;
            REQUIRE(ResolveNativeMaterial(copy, true) == NativeMaterialProfile::None);
        }
        for (int dimension : { 16, 64 }) {
            auto copy = source;
            copy["nativeAnimation"]["logicalWidth"] = dimension;
            copy["nativeAnimation"]["logicalHeight"] = dimension;
            REQUIRE(ResolveNativeMaterial(copy, true) == NativeMaterialProfile::None);
        }
        auto extraCaustics = source;
        extraCaustics["nativeAnimation"]["extra"] = true;
        REQUIRE(ResolveNativeMaterial(extraCaustics, true) == NativeMaterialProfile::None);
    }
    for (int role = 0; role < 3; ++role) {
        auto direct = pool;
        direct["chain"][0]["path"] =
            "objects/object_keikoku_obj/object_keikoku_obj_DL_000" + std::to_string(1 + role * 2) + "00";
        REQUIRE(static_cast<int>(ResolveNativeMaterial(direct, true)) == 7 + role);
        direct["chain"].push_back(direct["chain"][0]);
        REQUIRE(ResolveNativeMaterial(direct, true) == NativeMaterialProfile::None);
        for (uint32_t f : { 0u, 1u, 31u, 32u, 63u, 64u, 127u, 128u, 2047u, 2048u, 0xffffffffu }) {
            auto a = NativeScrollParameters(static_cast<NativeMaterialProfile>(4 + role), 999, f);
            auto b = NativeScrollParameters(static_cast<NativeMaterialProfile>(7 + role), 999, f);
            REQUIRE((a.y2 % 128) * 2 == b.y2 % 256);
            REQUIRE(a.dy2 * 2 == b.dy2 && a.x1 == 0 && a.y1 == 0 && a.x2 == 0);
        }
    }
    std::vector<NativeMaterialCommand> fountain = { { 0xf5101000, 0x00014050 }, { 0xf5101100, 0x01014451 },
                                                    { 0xf2000000, 0x0007c07c }, { 0xf2000000, 0x0107c07c },
                                                    { 0x32004008, 0 },          { 0xdf000000, 55 },
                                                    { 0x06000204, 0x00000406 }, { 0xdf000000, 0 } };
    const auto fp = NativeMaterialProfile::FountainLowerA32;
    REQUIRE(FindNativeScrollInsertion(fountain, fp) == 6);
    fountain[1].w1 = 0x01017c5e; // Actual raised authored shifts, not zero-shift stand-ins.
    REQUIRE(FindNativeScrollInsertion(fountain, fp) == 6);
    auto large = fountain;
    for (size_t i = 0; i < 2; ++i)
        large[i].w1 ^= (3u << 14) | (3u << 4); // mask5 -> mask6, preserve shifts
    large[2].w1 = 0x000fc0fc;
    large[3].w1 = 0x010fc0fc;
    REQUIRE(FindNativeScrollInsertion(large, NativeMaterialProfile::FountainCentral64) == 6);
    REQUIRE(!FindNativeScrollInsertion(large, fp));
    for (size_t i = 0; i < 4; ++i) {
        auto bad = fountain;
        bad.erase(bad.begin() + i);
        REQUIRE(!FindNativeScrollInsertion(bad, fp));
        bad = fountain;
        bad.insert(bad.begin() + i, bad[i]);
        REQUIRE(!FindNativeScrollInsertion(bad, fp));
    }
    for (uintptr_t bits : { 1u << 18, 1u << 19, 1u << 8, 1u << 9, 1u << 14, 1u << 4 }) {
        auto bad = fountain;
        bad[0].w1 ^= bits;
        REQUIRE(!FindNativeScrollInsertion(bad, fp));
    }
    auto bad = fountain;
    bad[2].w1 ^= 4;
    REQUIRE(!FindNativeScrollInsertion(bad, fp));
    bad = fountain;
    bad.insert(bad.begin() + 6, { 0xde000000, 0x08000000 });
    REQUIRE(!FindNativeScrollInsertion(bad, fp));
    bad = fountain;
    bad.insert(bad.end() - 1, { 0xf5101000, 0x00014050 });
    REQUIRE(!FindNativeScrollInsertion(bad, fp));
    bad = fountain;
    bad.back().w1 = 1;
    REQUIRE(!FindNativeScrollInsertion(bad, fp));

    // Tile 0 remains asset-owned. Tile 1 is the fixed 32x32 caustic layer.
    std::vector<NativeMaterialCommand> causticMaterial = {
        { 0xf5101000, 0x00018067 }, { 0xf5101000, 0x07000000 }, { 0xf5101100, 0x01014053 }, { 0xf200800c, 0x00104108 },
        { 0xf2000000, 0x0107c07c }, { 0xf5101000, 0x07000000 }, { 0xdf000000, 0 },
    };
    for (const auto cp : { NativeMaterialProfile::WaterTempleCaustics, NativeMaterialProfile::ZorasDomainCaustics }) {
        REQUIRE(FindNativeScrollInsertion(causticMaterial, cp) == 6);
        REQUIRE(!FindNativeScrollInsertion(causticMaterial, NativeMaterialProfile::LakeHylia));
        REQUIRE(!FindNativeScrollInsertion(causticMaterial, NativeMaterialProfile::FountainLowerA32));
        auto authoredSetup = causticMaterial;
        authoredSetup[0] = { 0xf5684000, 0x0001c4af }; // IA8, line 32, authored masks and shifts.
        authoredSetup[2].w1 = 0x01015c5e;              // Authored tile 1 shifts remain valid.
        REQUIRE(FindNativeScrollInsertion(authoredSetup, cp) == 6);
        auto causticDraw = causticMaterial;
        causticDraw.insert(causticDraw.end() - 1, { 0x06000204, 0x00000406 });
        REQUIRE(FindNativeScrollInsertion(causticDraw, cp) == 6);
        for (const auto& vertexCommands : {
                 std::vector<NativeMaterialCommand>{ { 0x01000000, 0 } },
                 std::vector<NativeMaterialCommand>{ { 0x48000000, 0 } },
                 std::vector<NativeMaterialCommand>{ { 0x32000000, 0 }, { 0xde000000, 0x08000000 } },
             }) {
            auto truncatedDraw = causticMaterial;
            truncatedDraw.insert(truncatedDraw.end() - 1, vertexCommands.begin(), vertexCommands.end());
            REQUIRE(!FindNativeScrollInsertion(truncatedDraw, cp));
            truncatedDraw.insert(truncatedDraw.end() - 1, { 0x06000204, 0x00000406 });
            REQUIRE(FindNativeScrollInsertion(truncatedDraw, cp) == 6 + vertexCommands.size());
        }

        auto base256 = causticMaterial;
        base256[3] = { 0xf2000000, 0x003fc3fc };
        REQUIRE(FindNativeScrollInsertion(base256, cp) == 6);
        for (size_t missing : { 2u, 4u }) {
            auto invalid = causticMaterial;
            invalid.erase(invalid.begin() + missing);
            REQUIRE(!FindNativeScrollInsertion(invalid, cp));
        }
        for (size_t duplicate : { 2u, 4u }) {
            auto invalid = causticMaterial;
            invalid.insert(invalid.begin() + duplicate, invalid[duplicate]);
            REQUIRE(!FindNativeScrollInsertion(invalid, cp));
        }
        for (uintptr_t bits : { 1u << 18, 1u << 8, 1u << 14, 1u << 4 }) {
            auto invalid = causticMaterial;
            invalid[2].w1 ^= bits;
            REQUIRE(!FindNativeScrollInsertion(invalid, cp));
        }
        for (NativeMaterialCommand descriptor :
             { NativeMaterialCommand{ 0xf5001100, 0x01014053 }, NativeMaterialCommand{ 0xf5081100, 0x01014053 },
               NativeMaterialCommand{ 0xf5100100, 0x01014053 }, NativeMaterialCommand{ 0xf5101000, 0x01014053 } }) {
            auto invalid = causticMaterial;
            invalid[2] = descriptor;
            REQUIRE(!FindNativeScrollInsertion(invalid, cp));
        }
        auto invalidCaustic = causticMaterial;
        invalidCaustic[4].w1 ^= 4;
        REQUIRE(!FindNativeScrollInsertion(invalidCaustic, cp));
        invalidCaustic = causticMaterial;
        invalidCaustic[3].w1 |= 0x02000000;
        REQUIRE(!FindNativeScrollInsertion(invalidCaustic, cp));
        invalidCaustic = causticMaterial;
        invalidCaustic.insert(invalidCaustic.end() - 1, { 0xf5101000, 0x00018067 });
        REQUIRE(!FindNativeScrollInsertion(invalidCaustic, cp));
        invalidCaustic = causticDraw;
        invalidCaustic.insert(invalidCaustic.end() - 1, { 0xe7000000, 0 });
        REQUIRE(!FindNativeScrollInsertion(invalidCaustic, cp));
        for (uintptr_t opcode : { 0xde000000u, 0xda000000u, 0xdb000000u, 0x4a000000u }) {
            invalidCaustic = causticMaterial;
            invalidCaustic.insert(invalidCaustic.end() - 1, { opcode, 0 });
            REQUIRE(!FindNativeScrollInsertion(invalidCaustic, cp));
        }
        invalidCaustic = causticMaterial;
        invalidCaustic.insert(invalidCaustic.begin(), { 0x33000000, 0 });
        invalidCaustic.insert(invalidCaustic.begin() + 1, { 0xf5101100, 0x01014053 });
        REQUIRE(FindNativeScrollInsertion(invalidCaustic, cp) == 8);
        invalidCaustic = causticMaterial;
        invalidCaustic.back() = { 0x33000000, 0 };
        REQUIRE(!FindNativeScrollInsertion(invalidCaustic, cp));
        invalidCaustic = causticMaterial;
        invalidCaustic.push_back({ 0xe7000000, 0 });
        REQUIRE(!FindNativeScrollInsertion(invalidCaustic, cp));
    }

    for (uint32_t f : { 0u, 1u, 127u, 128u, 2047u, 2048u, 0xffffffffu }) {
        auto p = NativeScrollParameters(NativeMaterialProfile::WaterTempleCaustics, f + 31u, f);
        REQUIRE(p.x1 == 0 && p.y1 == 0 && p.x2 == f && p.y2 == 0);
        REQUIRE(p.width == 32 && p.height == 32);
        REQUIRE(p.dx1 == 0 && p.dy1 == 0 && p.dx2 == 1 && p.dy2 == 0);
    }
    for (uint32_t f : { 0u, 1u, 126u, 127u, 128u, 129u, 2047u, 2048u, 0xffffffffu }) {
        auto p = NativeScrollParameters(NativeMaterialProfile::ZorasDomainCaustics, f ^ 0xa5a5a5a5u, f);
        REQUIRE(p.x1 == 0 && p.y1 == 0 && p.x2 == 0 && p.y2 == 127u - f % 128u);
        REQUIRE(p.width == 32 && p.height == 32);
        REQUIRE(p.dx1 == 0 && p.dy1 == 0 && p.dx2 == 0 && p.dy2 == -1);
    }
    CheckSagePlatformCommandSafety();
    struct PlatformPhase {
        uint32_t frame, left, right;
    };
    for (const auto& test :
         { PlatformPhase{ 0, 127, 0 }, PlatformPhase{ 1, 126, 1 }, PlatformPhase{ 126, 1, 126 },
           PlatformPhase{ 127, 0, 127 }, PlatformPhase{ 128, 127, 0 }, PlatformPhase{ 129, 126, 1 },
           PlatformPhase{ 2047, 0, 127 }, PlatformPhase{ 2048, 127, 0 }, PlatformPhase{ 0xffffffff, 0, 127 } }) {
        const auto p =
            NativeScrollParameters(NativeMaterialProfile::ChamberOfSagesPlatform, test.frame ^ 0xa5a5a5a5u, test.frame);
        REQUIRE(p.x1 == test.left && p.y1 == test.right && p.x2 == test.right && p.y2 == test.right);
        REQUIRE(p.width == 32 && p.height == 32);
        REQUIRE(p.dx1 == -1 && p.dy1 == 1 && p.dx2 == 1 && p.dy2 == 1);
    }
    std::cout << "PASS native material identity, command safety, and native scroll parameters\n";
}
