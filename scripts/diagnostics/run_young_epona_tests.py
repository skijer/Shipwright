#!/usr/bin/env python3
"""Run the production child-horse entry/summon code against engine boundaries."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
from run_time_pedestal_tests import functions

ROOT = Path(__file__).resolve().parents[2]
source = functions((ROOT / 'soh/src/code/z_horse.c').read_text())
names = ('Horse_CanSpawn', 'Horse_CanSpawnYoung', 'Horse_YoungEponaAssetsAvailable', 'Horse_CanUseYoungEpona',
         'Horse_FindYoungEpona', 'Horse_GetActorSaveData', 'Horse_SaveYoungEpona',
         'Horse_SpawnYoungEpona', 'Horse_TrySummonYoungEpona', 'Horse_InitPlayerHorse')
fixture = (ROOT / 'soh/tests/young_epona_test.c').read_text().replace(
    '/* PRODUCTION_HORSE */', '\n'.join(source[n] for n in names if n in source))
npc_source = functions((ROOT / 'soh/src/overlays/actors/ovl_En_Horse_Link_Child/z_en_horse_link_child.c').read_text())
fixture = fixture.replace('/* PRODUCTION_RANCH_UPDATE */', npc_source['EnHorseLinkChild_Update'])
flags = ['-std=gnu2x', '-DF3DEX_GBI_2', '-DLOG_LEVEL_GAME_PRINTS=0', '-DNDEBUG',
         '-Werror=implicit-function-declaration', '-Wno-int-conversion',
         '-Wno-incompatible-pointer-types', '-Wno-discarded-qualifiers',
         '-Wno-discarded-array-qualifiers',
         '-Ilibultraship/include', '-Isoh/include', '-Isoh/src', '-Isoh/assets', '-Isoh', '-Isoh/mods',
         '-fsanitize=undefined', '-fno-sanitize-recover=all']
for path in ('CMake/soh-cvars.cmake', 'CMake/lus-cvars.cmake'):
    for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT/path).read_text()):
        flags.append(f'-D{key}="{value}"')
with tempfile.TemporaryDirectory(prefix='young-epona-test-') as folder:
    cfile, binary = Path(folder)/'test.c', Path(folder)/'test'
    cfile.write_text(fixture)
    subprocess.run([os.environ.get('CC', 'cc'), *flags, str(cfile), '-lm', '-o', str(binary)], cwd=ROOT, check=True)
    subprocess.run([str(binary)], cwd=ROOT, check=True)
    if 'Horse_CanUseYoungEpona' in source:
        for path in ('soh/src/code/z_horse.c', 'soh/src/code/z_message_PAL.c',
                     'soh/src/overlays/actors/ovl_En_Horse/z_en_horse.c',
                     'soh/src/overlays/actors/ovl_En_Horse_Link_Child/z_en_horse_link_child.c',
                     'soh/src/overlays/actors/ovl_player_actor/z_player.c'):
            subprocess.run([os.environ.get('CC', 'cc'), *flags, '-fsyntax-only', path], cwd=ROOT, check=True)
        print('PASS: all five affected C translation units compile')
