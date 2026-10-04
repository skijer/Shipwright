#!/usr/bin/env python3
"""Exercise the production static-actor initialization and camera culling functions."""
import re
import subprocess
import tempfile
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


def main():
    viewer = (ROOT / 'soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c').read_text()
    engine = (ROOT / 'soh/src/code/z_actor.c').read_text()
    fixture = (ROOT / 'soh/tests/static_story_culling_test.c').read_text()
    production = (function(viewer, 'EnViewer_SetupAction') + function(viewer, 'EnViewerStatic_Init') +
                  function(engine, 'Actor_CullingVolumeTest') + function(engine, 'Ship_CalcShouldDrawAndUpdate'))
    fixture = fixture.replace('/* PRODUCTION_CULLING_FUNCTIONS */', production)
    flags = ['-std=gnu11', '-DF3DEX_GBI_2', '-DLOG_LEVEL_GAME_PRINTS=6', '-DNDEBUG',
             '-DCVAR_PREFIX_ENHANCEMENT="gEnhancements"',
             '-Ilibultraship/include', '-Isoh/include', '-Isoh/src', '-Isoh/assets', '-Isoh', '-Isoh/mods']
    with tempfile.TemporaryDirectory(prefix='static-story-culling-') as directory:
        source = Path(directory) / 'culling.c'
        output = Path(directory) / 'culling'
        source.write_text(fixture)
        subprocess.run(['cc', *flags, str(source),
                        'soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c', '-lm', '-o', str(output)],
                       cwd=ROOT, check=True)
        failures = []
        for case in ['default', 'settings', 'legacy']:
            result = subprocess.run([str(output), case], cwd=ROOT)
            if result.returncode:
                failures.append(case)
        if failures:
            raise SystemExit('Failed culling cases: ' + ', '.join(failures))


if __name__ == '__main__':
    main()
