#!/usr/bin/env python3
"""Test the production GPU Epona overlay builder; optionally exercise real native archives."""

import argparse
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile
from zipfile import ZipFile

from run_child_ruto_face_test import function

ROOT = Path(__file__).resolve().parents[2]


def path_hash(path):
    value = 0xFFFFFFFFFFFFFFFF
    for byte in path.encode():
        value ^= byte << 56
        for _ in range(8):
            value = ((value << 1) ^ (0x42F0E1EBA9EA3693 if value >> 63 else 0)) & 0xFFFFFFFFFFFFFFFF
    return value


def native_lists(archive, game):
    with ZipFile(archive) as source:
        names = source.namelist()
        hashes = {path_hash(name): name for name in names}
        display_lists = []
        used_names = set()
        for name in names:
            if game == "oot":
                include = (name.startswith("objects/object_horse/gEpona") or
                           name.startswith("objects/object_horse_link_child/gChildEponaSkelLimbsLimb_")) and "DL_" in name
            else:
                include = name.startswith("objects/object_horse_link_child/") and "DL_" in name
            if not include:
                continue
            data = source.read(name)
            if struct.unpack_from("<I", data, 4)[0] != 0x4F444C54:
                continue
            commands = list(struct.iter_unpack("<II", data[72:]))
            i = 0
            while i < len(commands):
                opcode = commands[i][0] >> 24
                if opcode in (0x20, 0x31, 0x32, 0x33):
                    hashed = (commands[i + 1][0] << 32) | commands[i + 1][1]
                    if opcode == 0x20:
                        used_names.add(hashes[hashed])
                    i += 2
                else:
                    i += 1
            young = "object_horse_link_child" in name
            display_lists.append((name, young, commands))
        assert display_lists, f"No native horse display lists in {archive}"
    return hashes, display_lists, used_names


def fixture(archive, game):
    hashes, display_lists, used_names = native_lists(archive, game)
    text = '#include "EponaCosmeticsDL.h"\n#include "EponaCosmeticMasks.h"\n#include "EponaCosmeticsNativeTemplates.h"\n'
    text += '#include <cassert>\n#include <cstdio>\n#include <map>\n#include <string>\n'
    text += 'using namespace EponaCosmetics;\n'
    text += 'struct Fixture { const char* name; bool young; std::vector<Gfx> list; };\n'
    text += 'static const std::map<uint64_t,std::string> names = {\n'
    for name in sorted(used_names):
        text += f'{{0x{path_hash(name):016x}ULL, {json.dumps(name)}}},\n'
    text += '};\nstatic const Fixture fixtures[] = {\n'
    for name, young, commands in display_lists:
        text += '{' + json.dumps(name) + ',' + str(young).lower() + ',{\n'
        for a, b in commands:
            text += f'{{0x{a:08x},0x{b:08x}}},\n'
        text += '}},\n'
    text += '};\n'
    text += f'constexpr bool isMM = {str(game == "mm").lower()};\n'
    text += r'''
struct Masks { std::string path; std::array<std::vector<uint8_t>,3> bits; };
static std::map<std::string,Masks> masks;
static MaskTarget target(const std::string& name, bool eye) {
    auto [it, added] = masks.try_emplace(name);
    if (added) {
        it->second.path = "__OTR__" + name;
        for (unsigned part=0; part<3; ++part) {
            it->second.bits[part] = EponaMasks::BuildInverseMask(name, static_cast<EponaMasks::Part>(part));
            if (eye && part==2 && it->second.bits[part].empty()) it->second.bits[part].assign(512,1);
        }
    }
    MaskTarget out{it->second.path.c_str(),{}};
    for (unsigned part=0;part<3;++part) {
        auto& bits=it->second.bits[part]; out.inverseMasks[part]=bits.empty()?nullptr:bits.data();
    }
    return out;
}
static std::vector<Gfx> vertices(const std::vector<Gfx>& list) {
    std::vector<Gfx> out;
    for(size_t i=0;i<list.size();++i) {
        auto op=list[i].words.w0>>24;
        if(op==0x01 || op==0x32) out.push_back(list[i]);
        if(op==0x20 || op==0x32 || op==0x33 || op==0x3f) {
            if(op==0x32) out.push_back(list[i+1]); ++i;
        }
    }
    return out;
}
int main() {
    unsigned covered[2]={}; size_t variants=0;
    for(const auto& fixture:fixtures) {
        assert(NativeTemplateMatches(fixture.name, fixture.list, isMM));
        auto warmed=fixture.list;
        for(size_t i=0;i<warmed.size();++i) {
            auto opcode=warmed[i].words.w0>>24;
            if(opcode==0x20 || opcode==0x32)
                warmed[i].words.w1=(sizeof(uintptr_t)>4?0x123456789000ULL:0x12345678ULL)+i*64;
            if(opcode==0x20 || opcode==0x31 || opcode==0x32 || opcode==0x33) ++i;
        }
        assert(NativeTemplateMatches(fixture.name, warmed, isMM));
        auto modified=fixture.list;
        for(auto& command:modified) {
            if((command.words.w0>>24)==0xf3) { command.words.w1^=0x100000; break; }
        }
        assert(!NativeTemplateMatches(fixture.name, modified, isMM));
        modified=fixture.list;
        for(size_t i=0;i<modified.size();++i) {
            auto opcode=modified[i].words.w0>>24;
            if(opcode==0x32) { modified[i].words.w1+=16; break; }
            if(opcode==0x20 || opcode==0x31 || opcode==0x33) ++i;
        }
        if(vertices(fixture.list).size()>0 && (vertices(fixture.list)[0].words.w0>>24)==0x32)
            assert(!NativeTemplateMatches(fixture.name, modified, isMM));
        auto resolver=[&](uint64_t hash,bool eye) {
            TextureMaterial result;
            if(eye) {
                const std::string folder=fixture.young?"objects/object_horse_link_child/":"objects/object_horse/";
                const std::string prefix=fixture.young?"gChildEponaEye":"gEponaEye";
                result.targets.push_back(target(folder+prefix+"OpenTex",true));
                result.targets.push_back(target(folder+prefix+"HalfTex",true));
                result.targets.push_back(target(folder+prefix+(fixture.young?"CloseTex":"ClosedTex"),true));
            } else {
                const auto& name=names.at(hash); result.palette=name.find("TLUT")!=std::string::npos;
                if(!result.palette) result.targets.push_back(target(name,false));
            }
            return result;
        };
        for(unsigned changed=0;changed<8;++changed) {
            const auto& input=(changed&1)?warmed:fixture.list;
            auto built=BuildNativeDisplayList(input,changed,resolver);
            if(built.empty()) { fprintf(stderr,"unsupported native list: %s mask %u\n",fixture.name,changed); return 1; }
            assert(built.back().words.w0==0xdf000000);
            auto before=vertices(input), after=vertices(built);
            assert(before.size()==after.size());
            for(size_t i=0;i<before.size();++i) assert(before[i].words.w0==after[i].words.w0 && before[i].words.w1==after[i].words.w1);
            if(!changed) {
                assert(built.size()==fixture.list.size());
                for(size_t i=0;i<built.size();++i) assert(built[i].words.w0==fixture.list[i].words.w0 && built[i].words.w1==fixture.list[i].words.w1);
            }
            for(size_t i=0;i<built.size();++i) {
                auto op=built[i].words.w0>>24;
                if(op==0x20 || op==0x32 || op==0x33 || op==0x3f) { ++i; continue; }
                if(op==0xde && built[i].words.w1>=0x09000001 && built[i].words.w1<=0x0b000001) {
                    unsigned bit=1<<((built[i].words.w1>>24)-9);
                    assert(changed&bit); covered[fixture.young]|=bit;
                }
            }
            ++variants;
        }
    }
    assert(covered[1]==7);
    // MM contains only young Epona; OoT must exercise all three parts for both ages.
    if (std::size(fixtures)==21) assert(covered[0]==7);
    printf("PASS: %zu real native display lists / %zu variants; exact reset, coat/hair/eyes, blink palette, identical vertex loads\n",std::size(fixtures),variants);
}
'''
    return text


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--oot", type=Path, help="Read-only native OoT archive acceptance coverage")
    parser.add_argument("--mm", type=Path, help="Read-only native MM archive acceptance coverage")
    args = parser.parse_args()
    flags = ["-std=c++20", "-DF3DEX_GBI_2", "-Ilibultraship/include", "-Isoh/soh/Enhancements/cosmetics",
             "-fsanitize=undefined", "-fno-sanitize-recover=all"]
    with tempfile.TemporaryDirectory(prefix="epona-cosmetics-") as folder:
        binary = Path(folder) / "test"
        subprocess.run([os.environ.get("CXX", "c++"), *flags,
                        "soh/tests/epona_cosmetics_dl_test.cpp", "-o", str(binary)], cwd=ROOT, check=True)
        subprocess.run([str(binary)], check=True)
        renderer = (ROOT / "libultraship/src/fast/interpreter.cpp").read_text()
        renderer = re.sub(r"/\*.*?\*/", "", renderer, flags=re.S)
        renderer_header = (ROOT / "libultraship/include/fast/interpreter.h").read_text()
        types = renderer_header[renderer_header.index("enum {\n    SHADER_0"):
                                renderer_header.index("#define SHADER_MAX_TEXTURES")]
        types += re.search(r"struct ColorCombiner \{.*?\n};", renderer_header, re.S).group()
        (Path(folder) / "epona_combiner_types.inc").write_text(types)
        (Path(folder) / "epona_combiner_production.inc").write_text("\n".join(
            function(renderer, name) for name in (
                "Interpreter::GenerateCC", "Interpreter::GfxDpSetCombineMode", "color_comb", "alpha_comb",
                "Interpreter::GfxDpSetPrimColor", "gfx_set_combine_handler_rdp", "gfx_set_prim_color_handler_rdp")))
        subprocess.run([os.environ.get("CXX", "c++"), *flags, "-I" + folder,
                        "soh/tests/epona_cosmetics_alpha_test.cpp", "-o", str(binary)], cwd=ROOT, check=True)
        subprocess.run([str(binary)], check=True)
        production = (ROOT / "soh/soh/Enhancements/cosmetics/EponaCosmetics.cpp").read_text()
        draw = (ROOT / "soh/tests/epona_cosmetics_draw_test.cpp").read_text()
        constants = production[production.index("constexpr const char* kColorCVars"):production.index("struct CachedMasks")]
        swap = re.search(r"struct LimbSwap \{.*?\n};", production, re.S).group()
        methods = "\n".join(function(production, name) for name in (
            "NativePath", "DrawLimbs", "BindColors", "EponaCosmetics_BeginDraw", "EponaCosmetics_EndDraw"))
        draw = draw.replace("/* PRODUCTION_CONSTANTS */", constants).replace("/* PRODUCTION_SWAP */", swap)
        draw = draw.replace("/* PRODUCTION_DRAW */", methods)
        draw_source = Path(folder) / "draw.cpp"
        draw_source.write_text(draw)
        draw_flags = [*flags, "-DNDEBUG", "-DLOG_LEVEL_GAME_PRINTS=6", '-DCVAR_PREFIX_COSMETIC="gCosmetics"',
                      "-Isoh/include", "-Isoh", "-Isoh/src", "-Isoh/assets", "-Isoh/mods"]
        subprocess.run([os.environ.get("CXX", "c++"), *draw_flags, str(draw_source), "-o", str(binary)], cwd=ROOT, check=True)
        subprocess.run([str(binary)], check=True)
        for game, archive in (("oot", args.oot), ("mm", args.mm)):
            if archive is None:
                continue
            source = Path(folder) / f"{game}.cpp"
            source.write_text(fixture(archive, game))
            subprocess.run([os.environ.get("CXX", "c++"), *flags, str(source), "-o", str(binary)], cwd=ROOT, check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
