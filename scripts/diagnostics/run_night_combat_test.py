"""Exercise the real night updater, sequence queue and sequence-mode body.

Only the sequence-mode body's unrelated translation-unit dependencies are
fixtures; generated source is confined to the temporary build directory.
"""
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]


def main(build, cc='cc', cxx='c++'):
    build = pathlib.Path(build)
    source = (ROOT / 'soh/src/code/code_800EC960.c').read_text()
    body = re.search(r'^void Audio_SetSequenceMode\([^;]*?\)\s*\{.*?^}', source, re.M | re.S)
    if body is None:
        raise RuntimeError('Missing production Audio_SetSequenceMode')
    (build / 'night_sequence_mode.inc').write_text(body[0])
    includes = ['-I' + str(ROOT / p) for p in
                ['soh/tests/night_runtime_stubs', 'soh/tests/audio_sequence_stubs', 'soh/include', 'soh']]
    obj = str(build / 'night_seq.o')
    subprocess.run([cc, '-std=c11', *includes, '-c', str(ROOT / 'soh/src/code/code_800F9280.c'), '-o', obj], check=True)
    exe = str(build / 'night_combat_test')
    subprocess.run([cxx, '-std=c++20', '-DNDEBUG', '-DNIGHT_COMBAT_RUNTIME_TEST',
                    '-DCVAR_PREFIX_AUDIO="gAudioEditor"', '-I' + str(build), *includes,
                    str(ROOT / 'soh/tests/hyrule_field_night_runtime_test.cpp'),
                    str(ROOT / 'soh/soh/Enhancements/audio/HyruleFieldNightMusic.cpp'), obj, '-o', exe], check=True)
    subprocess.run([exe], check=True)
    print('PASS night combat bridge: handoff, restoration, identity, ownership cleanup and vanilla eligibility')


if __name__ == '__main__':
    main(*sys.argv[1:])
