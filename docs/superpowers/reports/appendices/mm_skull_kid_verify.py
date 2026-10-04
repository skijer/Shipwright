#!/usr/bin/env python3
"""Compile exact production graph functions and exercise controlled binary O2R packs.
Usage: python mm_skull_kid_verify.py BUILD_DIR [MM_ARCHIVE SKULL_KID_3DS_ARCHIVE [ANJU_HD_ARCHIVE]]
All archives and object files are temporary; this never writes into the build tree.
"""
from pathlib import Path
import json
import os
import re
import shlex
import struct
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[4]
BUILD = Path(sys.argv[1]).resolve()
WORK = Path(tempfile.mkdtemp(prefix="mm-skull-kid-"))
print("Fixture output:", WORK, flush=True)

def function(source, name):
    match = re.search(r'^[^\n;{}]*\b' + re.escape(name) + r'\([^;{}]*\)\s*\{', source, re.M)
    if not match:
        raise RuntimeError("Missing production definition: " + name)
    start, depth, i = match.start(), 1, match.end()
    while depth:
        if source[i] == '{': depth += 1
        elif source[i] == '}': depth -= 1
        i += 1
    return source[start:i] + '\n'

ninja = (BUILD / "build.ninja").read_text()
def flags(suffix):
    match = re.search(r'^build soh/CMakeFiles/soh.dir/' + re.escape(suffix) + r':[^\n]*\n((?:  [^\n]*\n)+)', ninja, re.M)
    if not match: raise RuntimeError(suffix)
    return sum((shlex.split(v) for k, v in re.findall(r'^  (DEFINES|FLAGS|INCLUDES) = (.*)$', match[1], re.M)), [])

CFLAGS = flags('src/overlays/actors/ovl_En_Viewer/z_en_viewer.c.o') + ['-ffunction-sections', '-fdata-sections', '-DNDEBUG']
CPPFLAGS = flags('mods/transformation_masks/assets/mm_asset_loader.cpp.o') + ['-ffunction-sections', '-fdata-sections', '-DNDEBUG']
def run(args):
    subprocess.run([str(a) for a in args], cwd=ROOT, check=True)
def compile(path, cpp=False):
    out = WORK / (path.name + '.o')
    run(['c++' if cpp else 'cc', *(CPPFLAGS if cpp else CFLAGS), '-c', path, '-o', out])
    return out

NATIVE = 'objects/object_stk/'
OPTIONAL = 'objects/object_stk_3ds/v1/'
ROOTS = [
    'gSkullKidPelvisDL', 'gSkullKidRightThighDL', 'gSkullKidRightShinDL', 'gSkullKidRightFootDL',
    'gSkullKidLeftThighDL', 'gSkullKidLeftShinDL', 'gSkullKidLeftFootDL', 'gSkullKidTorsoDL',
    'gSkullKidLeftUpperArmDL', 'gSkullKidLeftForearmDL', 'gSkullKidLeftHandAndFluteDL',
    'gSkullKidRightUpperArmDL', 'gSkullKidRightForearmDL', 'gSkullKidRightHandDL', 'gSkullKidNeckDL',
    'gSkullKidHatBrimDL', 'gSkullKidHatRingsDL', 'gSkullKidHatNarrowSectionDL', 'gSkullKidHatTopDL',
    'gSkullKidNormalHeadDL', 'gSkullKidNormalEyesDL', 'gSkullKidMajorasMask1DL',
]

def crc64(path):
    # OTR's unreflected ECMA polynomial, all-one seed, no final xor.
    result = 0xffffffffffffffff
    for byte in path.encode():
        result ^= byte << 56
        for _ in range(8):
            result = ((result << 1) ^ (0x42f0e1eba9ea3693 if result >> 63 else 0)) & 0xffffffffffffffff
    return result

def header(kind, version=0):
    return struct.pack('<BBHIIQ', 0, 0, 0, kind, version, 0) + bytes(44)
def dl(commands):
    return header(0x4f444c54) + bytes([4]) + bytes(7) + b''.join(struct.pack('<II', *c) for c in commands)
def hashed(opcode, path, w1=0, low=0):
    value = crc64(path)
    return [(opcode << 24 | low, w1), (value >> 32, value & 0xffffffff)]
def pack(prefix, value):
    entries = {prefix + name: dl(hashed(0x31, prefix + 'sharedDL') + [(0xdf000000, 0)]) for name in ROOTS}
    entries[prefix + 'sharedDL'] = dl(hashed(0x20, prefix + 'bodyTex', low=0x100001) +
        hashed(0x32, prefix + 'bodyVtx', 16, 0x3006) +
        [(0xda380003, 0x0d0004c1), (0x05000204, 0), (0xdf000000, 0)])
    entries[prefix + 'bodyTex'] = header(0x4f544558, 1) + struct.pack('<IIIIffI', 1, 8, 8, 1, 8.0, 4.0, 256) + bytes([value]) * 256
    entries[prefix + 'bodyVtx'] = header(0x4f415252) + struct.pack('<II', 25, 4) + b''.join(
        struct.pack('<hhhHhhBBBB', value + i, 0, 0, 0, 0, 0, 0, 0, 127, 255) for i in range(4))
    return entries

def write_pack(name, entries):
    output = WORK / (name + '.o2r')
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as archive:
        for path, data in entries.items(): archive.writestr(path, data)
    return output

archives = [write_pack('native', pack(NATIVE, 41)), write_pack('complete_a', pack(OPTIONAL, 81)),
            write_pack('complete_b', pack(OPTIONAL, 121))]
for name, missing in [('missing_root', ROOTS[-2]), ('missing_vertex', 'bodyVtx'), ('missing_texture', 'bodyTex')]:
    entries = pack(OPTIONAL, 161)
    del entries[OPTIONAL + missing]
    archives.append(write_pack(name, entries))
for name, commands in [
    ('foreign_vertex', hashed(0x32, 'foreign/bodyVtx', 0, 0x3006)),
    ('vertex_range', hashed(0x32, OPTIONAL + 'bodyVtx', 1024, 0x3006)),
    ('cycle', hashed(0x31, OPTIONAL + ROOTS[0])),
    ('matrix_range', [(0xda380003, 0x0d000500)]),
    ('matrix_even_address', [(0xda380003, 0x0d000000)]),
    ('triangle_range', [(0x05fefefe, 0)]),
    ('triangle_odd_index', [(0x05010305, 0)]),
    ('triangle2_range', [(0x06000204, 0x00fefefe)]),
    ('noop_filename_pointer', [(0x00070001, 0x06000000)]),
    ('othermode_l_shift', [(0xe200ff01, 0)]),
    ('othermode_h_length', [(0xe3000020, 0)]),
    ('raw_vertex', [(0x01003006, 0x06000000)]),
    ('raw_display_list', [(0xde000000, 0x06000000)]),
    ('vertex_cache_range', hashed(0x32, OPTIONAL + 'bodyVtx', 0, 0x3004)),
    ('vertex_odd_index', hashed(0x32, OPTIONAL + 'bodyVtx', 0, 0x3007)),
]:
    entries = pack(OPTIONAL, 161)
    entries['foreign/bodyVtx'] = entries[OPTIONAL + 'bodyVtx']
    entries[OPTIONAL + 'sharedDL'] = dl(commands + [(0xdf000000, 0)])
    archives.append(write_pack(name, entries))
entries = pack(OPTIONAL, 161)
entries[OPTIONAL + 'bodyTex'] = entries[OPTIONAL + 'bodyTex'][:-1]
archives.append(write_pack('truncated_texture', entries))
entries = pack(OPTIONAL, 161)
entries[OPTIONAL + 'bodyVtx'] = entries[OPTIONAL + 'bodyTex']
archives.append(write_pack('wrong_vertex_type', entries))
poison = {'poisonDL': dl([(0xdf000000, 0)]), OPTIONAL + 'sharedDL.meta':
          json.dumps({'format': 'BINARY', 'type': 'DisplayList', 'version': 0, 'path': 'poisonDL'}).encode()}
archives.append(write_pack('global_metadata_poison', poison))

loader = (ROOT / 'soh/mods/transformation_masks/assets/mm_asset_loader.cpp').read_text()
source = (ROOT / 'soh/tests/mm_skull_kid_model_test.cpp').read_text()
begin = loader.index('namespace {', loader.index('static void* MmAssets_LoadFromMmArchive'))
end = loader.index('/**', loader.index('void MmAssets_EnsureStrictTextureBindings', begin))
production = (function(loader, 'MmAssets_LoadResourceObjectFromMmArchive') +
              function(loader, 'MmAssets_StripOtrPrefix') + loader[begin:end])
source = source.replace('/* PRODUCTION_GRAPH_FUNCTIONS */', production)
fixture = WORK / 'graph_fixture.cpp'
fixture.write_text(source)
objects = [compile(fixture, True)]
for path in ['soh/soh/resource/type/Array.cpp', 'soh/soh/resource/importer/ArrayFactory.cpp',
             'soh/mods/transformation_masks/assets/mm_display_list_patch.cpp',
             'soh/mods/transformation_masks/assets/mm_strict_texture_binding.cpp']:
    objects.append(compile(ROOT / path, True))
objects.append(compile(ROOT / 'soh/src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.c'))
libs = ['libultraship/src/libultraship.a', '_deps/stormlib-build/libstorm.a', 'libultraship/libImGui.a',
        'libultraship/libstb.a', 'libultraship/liblibgfxd.a', '_deps/prism-build/libprism.a', 'libultraship/libmonocypher.a']
sysroot = Path(os.environ.get('MM_VERIFY_SYSROOT', ROOT.parent / 'linux-sysroot/usr/lib/x86_64-linux-gnu'))
run(['c++', *objects, '-Wl,--gc-sections', '-Wl,--start-group', *[BUILD / lib for lib in libs], '-Wl,--end-group',
     '-L' + str(sysroot), '-Wl,-rpath,' + str(sysroot), '-lSDL2', '-lOpenGL', '-lzip', '-ltinyxml2', '-lspdlog',
     '-lfmt', '-lpng', '-lz', '-ldl', '-pthread', '-o', WORK / 'graph_fixture'])
run([WORK / 'graph_fixture', *archives])
if len(sys.argv) > 2:
    if len(sys.argv) not in (4, 5): raise SystemExit(__doc__)
    real_native, real_pack = map(lambda path: Path(path).resolve(), sys.argv[2:4])
    with zipfile.ZipFile(real_pack) as archive:
        actual_entries = {name: archive.read(name) for name in archive.namelist()}
    missing = dict(actual_entries)
    del missing[OPTIONAL + 'gSkullKidNormalEyesDL']
    truncated = dict(actual_entries)
    texture_path = next(path for path, data in truncated.items() if data[4:8] == b'XETO')
    truncated[texture_path] = truncated[texture_path][:-1]
    run([WORK / 'graph_fixture', '--actual', real_native, real_pack,
         write_pack('actual_missing_root', missing), write_pack('actual_truncated_texture', truncated)])
    if len(sys.argv) == 5:
        anju = Path(sys.argv[4]).resolve()
        prefix = 'objects/object_anju_hd/v1/'
        with zipfile.ZipFile(anju) as archive:
            entries = {name: archive.read(name) for name in archive.namelist()}
        broken_packs = []
        for leaf in ['gAnju1BlinkClosedHeadDL', 'gAnju2UmbrellaDL', 'tex5']:
            broken = dict(entries)
            del broken[prefix + leaf]
            broken_packs.append(write_pack('anju_missing_' + leaf, broken))
        broken = dict(entries)
        broken[prefix + 'tex2'] = broken[prefix + 'tex2'][:-1]
        broken_packs.append(write_pack('anju_truncated_texture', broken))
        broken = dict(entries)
        leaf = next(path for path, data in broken.items() if path.endswith('DL') and
                    struct.pack('<I', 0xDA380003) in data[72:])
        data = bytearray(broken[leaf])
        offset = data.index(struct.pack('<I', 0xDA380003), 72)
        struct.pack_into('<I', data, offset + 4, 0x0D0004C1)  # Slot 19 is outside Anju's 19-slot allocation.
        broken[leaf] = data
        broken_packs.append(write_pack('anju_matrix_slot_19', broken))
        run([WORK / 'graph_fixture', '--anju', real_native, anju, *broken_packs])
