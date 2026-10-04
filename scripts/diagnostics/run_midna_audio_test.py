#!/usr/bin/env python3
"""Exercise Midna's real PCM mixer and Navi event routing without a ROM."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
from run_child_ruto_face_test import function

ROOT = Path(__file__).resolve().parents[2]
actor = ROOT / 'soh/src/overlays/actors/ovl_En_Elf/z_en_elf.c'
hud = ROOT / 'soh/src/code/z_parameter.c'
production = function(hud.read_text(), 'Interface_SetNaviCall')
production += function(actor.read_text(), 'EnElf_UpdateMidnaIdleAudio')
fixture = (ROOT / 'soh/tests/midna_audio_routing_test.c').read_text()
if 'static void EnElf_PlayNaviSound(' in actor.read_text():
    production += function(actor.read_text(), 'EnElf_PlayNaviSound')
    fixture = fixture.replace('/* ACTOR_ROUTING_CHECKS */', '''
    EnElf fairy = { 0 };
    clipPresent = 1;
    for (int type = FAIRY_NAVI; type <= FAIRY_HEAL_BIG; ++type) {
        fairy.actor.params = type;
        nativeCalls = customCalls = 0;
        EnElf_PlayNaviSound(&fairy, MIDNA_AUDIO_DASH, NA_SE_EV_FAIRY_DASH);
        REQUIRE(customCalls == (type == FAIRY_NAVI));
        REQUIRE(nativeCalls == (type != FAIRY_NAVI));
        if (nativeCalls) REQUIRE(nativeId == NA_SE_EV_FAIRY_DASH);
    }
    fairy.actor.params = FAIRY_NAVI;
    clipPresent = 0;
    nativeCalls = customCalls = 0;
    EnElf_PlayNaviSound(&fairy, MIDNA_AUDIO_VANISH, NA_SE_EV_NAVY_VANISH);
    REQUIRE(customCalls == 1 && customEvent == MIDNA_AUDIO_VANISH);
    REQUIRE(nativeCalls == 1 && nativeId == NA_SE_EV_NAVY_VANISH);
''')
fixture = fixture.replace('/* PRODUCTION_MIDNA_AUDIO_ROUTING */', production)
flags = ['-std=gnu2x', '-DF3DEX_GBI_2', '-DLOG_LEVEL_GAME_PRINTS=0', '-DNDEBUG',
         '-Werror=implicit-function-declaration', '-Wno-int-conversion',
         '-Wno-incompatible-pointer-types', '-Wno-discarded-qualifiers',
         '-Ilibultraship/include', '-Isoh/include', '-Isoh/src', '-Isoh/assets', '-Isoh', '-Isoh/mods']
for path in ('CMake/soh-cvars.cmake', 'CMake/lus-cvars.cmake'):
    for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT/path).read_text()):
        flags.append(f'-D{key}="{value}"')
with tempfile.TemporaryDirectory(prefix='midna-audio-test-') as folder:
    folder = Path(folder)
    source, binary = folder/'routing.c', folder/'routing'
    source.write_text(fixture)
    subprocess.run([os.environ.get('CC', 'cc'), *flags, str(source), '-o', str(binary)], cwd=ROOT, check=True)
    subprocess.run([str(binary)], cwd=ROOT, check=True)
    subprocess.run([os.environ.get('CC', 'cc'), *flags, '-fsyntax-only', str(actor), str(hud)], cwd=ROOT, check=True)
    binary = folder/'mixer'
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-pthread', '-Isoh',
                    'soh/tests/midna_audio_test.cpp', 'soh/soh/Enhancements/audio/MidnaAudio.cpp',
                    '-o', str(binary)], cwd=ROOT, check=True)
    result = subprocess.run([str(binary)], cwd=ROOT, capture_output=True, text=True)
    if result.returncode:
        print(result.stderr)
        result.check_returncode()
    print(result.stdout.strip())
    resources = (ROOT / 'soh/soh/Enhancements/audio/MidnaAudioResources.cpp').read_text()
    adapter = ''.join(function(resources, name) for name in
                      ('GetOwnResourceManager', 'ListClips', 'ReadAssignment', 'WriteAssignment',
                       'Enabled', 'HasModel', 'ReadClip', 'Gain'))
    source = folder/'resources.cpp'
    source.write_text((ROOT / 'soh/tests/midna_audio_resources_test.cpp').read_text().replace(
        '/* PRODUCTION_MIDNA_AUDIO_RESOURCES */', adapter))
    binary = folder/'resources'
    for mode in ([], ['-DCOMBO_BUILD']):
        subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-Isoh', '-Isoh/tests', *mode,
                        str(source), '-o', str(binary)], cwd=ROOT, check=True)
        subprocess.run([str(binary)], cwd=ROOT, check=True)
    print('PASS: complete modified actor and HUD translation units compile')
