#!/usr/bin/env python3
"""Check the production Child Ruto face bindings without a game executable.

Optionally pass oot.o2r to verify the native head's texture-segment contract.
The archive is opened read-only and no copyrighted asset bytes are copied out.
"""
import argparse
import re
import struct
import subprocess
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def function(source, name):
    match = re.search(r'^[^\n;{}]*\b' + re.escape(name) + r'\([^;{}]*\)\s*\{', source, re.M)
    if match is None:
        raise RuntimeError(f'Missing production function: {name}')
    end, depth = match.end(), 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


def check_native_head(path):
    with zipfile.ZipFile(path) as archive:
        data = archive.read('objects/object_ru1/gRutoChildHeadDL')
    if data[4:8] != b'TLDO' or len(data) < 72 or (len(data) - 72) % 8:
        raise RuntimeError('Unsupported native Child Ruto display-list format')
    commands = list(struct.iter_unpack('<II', data[72:]))
    texture_segments = set()
    index = 0
    while index < len(commands):
        w0, w1 = commands[index]
        opcode = w0 >> 24
        if opcode == 0xDF:
            break
        if opcode == 0xFD and w1 >> 24 in range(1, 16):
            texture_segments.add(w1 >> 24)
        index += 2 if opcode in {0x20, 0x24, 0x25, 0x27, 0x31, 0x32, 0x33, 0x35, 0x36, 0x42} else 1
    if texture_segments != {0x08, 0x09}:
        raise RuntimeError(f'Unexpected native Child Ruto texture segments: {texture_segments}')
    print('PASS real native Child Ruto head reads eye/mouth textures from segments 0x08/0x09', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('oot_archive', type=Path, nargs='?')
    args = parser.parse_args()
    if args.oot_archive:
        check_native_head(args.oot_archive)

    viewer = ROOT / 'soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c'
    source = viewer.read_text()
    production = (function(source, 'EnViewer_StaticChildRutoOverrideLimbDraw') +
                  function(source, 'EnViewer_DrawStaticChildRuto'))
    fixture = (ROOT / 'soh/tests/static_story_child_ruto_face_test.c').read_text()
    fixture = fixture.replace('/* PRODUCTION_CHILD_RUTO_DRAW */', production)
    flags = ['-std=gnu11', '-DF3DEX_GBI_2', '-DLOG_LEVEL_GAME_PRINTS=6', '-DNDEBUG',
             '-Wno-int-conversion', '-Wno-incompatible-pointer-types', '-Wno-discarded-qualifiers',
             '-Ilibultraship/include', '-Isoh/include', '-Isoh/src', '-Isoh/assets', '-Isoh', '-Isoh/mods']
    with tempfile.TemporaryDirectory(prefix='child-ruto-face-') as directory:
        test_source = Path(directory) / 'child_ruto_face.c'
        binary = Path(directory) / 'child_ruto_face'
        test_source.write_text(fixture)
        subprocess.run(['cc', *flags, str(test_source),
                        'soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c', '-o', str(binary)],
                       cwd=ROOT, check=True)
        subprocess.run([str(binary)], cwd=ROOT, check=True)
        subprocess.run(['cc', *flags, '-fsyntax-only', str(viewer)], cwd=ROOT, check=True)


if __name__ == '__main__':
    main()
