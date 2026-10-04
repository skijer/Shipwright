#!/usr/bin/env python3
"""Run exact production-function resource/lifecycle CPU fixtures, with normal build headers.
Usage: source build-env.sh; python mm_texture_verify.py BUILD_DIR MM_ARCHIVE [--compile-production]
No archive writes. Temporary objects live outside the build tree; no concurrent Ninja writer.
"""
from pathlib import Path
import os, re, shlex, subprocess, sys, tempfile
ROOT = Path(__file__).resolve().parents[4]
BUILD = Path(sys.argv[1]).resolve()
ARCHIVE = Path(sys.argv[2]).resolve()
WORK = Path(tempfile.mkdtemp(prefix='mm-texture-'))
print('Fixture output:', WORK, flush=True)

def function(source, name):
    # Extract a definition, preserving its actual body; no implementation is mirrored in the fixture.
    match = re.search(r'^[^\n;{}]*\b' + re.escape(name) + r'\([^;{}]*\)\s*\{', source, re.M)
    if not match: raise RuntimeError('Missing production definition: ' + name)
    start, depth, i = match.start(), 1, match.end()
    while depth:
        if source[i] == '{': depth += 1
        elif source[i] == '}': depth -= 1
        i += 1
    return source[start:i] + '\n'

ninja = (BUILD/'build.ninja').read_text()
def flags(object_suffix):
    match = re.search(r'^build soh/CMakeFiles/soh.dir/' + re.escape(object_suffix) + r':[^\n]*\n((?:  [^\n]*\n)+)', ninja, re.M)
    if not match: raise RuntimeError(object_suffix)
    return sum((shlex.split(v) for k,v in re.findall(r'^  (DEFINES|FLAGS|INCLUDES) = (.*)$', match[1], re.M)), [])
CFLAGS = flags('src/overlays/actors/ovl_En_Viewer/z_en_viewer.c.o') + ['-ffunction-sections', '-fdata-sections', '-DNDEBUG']
CPPFLAGS = flags('mods/transformation_masks/assets/mm_asset_loader.cpp.o') + ['-ffunction-sections', '-fdata-sections', '-DNDEBUG']
def run(args): subprocess.run([str(a) for a in args], cwd=ROOT, check=True)
def compile(path, cpp=False):
    out=WORK/(path.name+'.o')
    run(['c++' if cpp else 'cc', *(CPPFLAGS if cpp else CFLAGS), '-I'+str(WORK), '-c', path, '-o', out])
    return out

objects=[compile(ROOT/'soh/tests/mm_strict_texture_binding_test.cpp', True), compile(ROOT/'soh/mods/transformation_masks/assets/mm_strict_texture_binding.cpp', True)]
libs = ['libultraship/src/libultraship.a','_deps/stormlib-build/libstorm.a','libultraship/libImGui.a', 'libultraship/libstb.a','libultraship/liblibgfxd.a','_deps/prism-build/libprism.a','libultraship/libmonocypher.a']
sysroot=ROOT.parent/'linux-sysroot/usr/lib/x86_64-linux-gnu'
run(['c++',*objects,'-Wl,--gc-sections','-Wl,--start-group',*[BUILD/l for l in libs],'-Wl,--end-group','-L'+str(sysroot),'-Wl,-rpath,'+str(sysroot),'-lSDL2','-lOpenGL','-lzip','-ltinyxml2','-lspdlog','-lfmt','-lpng','-lz','-ldl','-pthread','-o',WORK/'texture_fixture'])
run([WORK/'texture_fixture',ARCHIVE])

viewer=(ROOT/'soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c').read_text()
source=(ROOT/'soh/tests/static_story_tatl_matrix_test.c').read_text()
matrix=(ROOT/'soh/src/code/sys_matrix.c').read_text()
skin=(ROOT/'soh/src/code/z_skin_matrix.c').read_text()
source=source.replace('/* PRODUCTION_MATRIX_FUNCTIONS */', '\n'.join(function(skin,n) for n in ['SkinMatrix_SetTranslate','SkinMatrix_SetScale']) + '\n'.join(function(matrix,n) for n in ['Matrix_Translate','Matrix_Scale','Matrix_MultVec3f']))
source=source.replace('/* PRODUCTION_TATL_CALLBACK */',function(viewer,'EnViewer_StaticTatlOverrideLimbDraw'))
path=WORK/'tatl_fixture.c';path.write_text(source)
objects=[compile(path),compile(ROOT/'soh/src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.c')]
run(['cc',*objects,'-Wl,--gc-sections','-lm','-o',WORK/'tatl_fixture'])
run([WORK/'tatl_fixture'])

if '--compile-production' in sys.argv:
    for name, cpp in [('soh/mods/transformation_masks/assets/mm_asset_loader.cpp', True),
                      ('soh/mods/transformation_masks/assets/mm_strict_texture_binding.cpp', True),
                      ('soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c', False)]:
        compile(ROOT/name, cpp)
        print('PASS production compile:', name, flush=True)
