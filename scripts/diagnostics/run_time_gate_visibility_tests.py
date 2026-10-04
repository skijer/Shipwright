"""Compile the production Time Gate draw entry points against real game headers."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[2]

def main():
    flags = ['-std=gnu2x', '-O1', '-g', '-DF3DEX_GBI_2', '-DLOG_LEVEL_GAME_PRINTS=0',
             '-Werror=implicit-function-declaration']
    flags += ['-I' + str(ROOT / p) for p in ('soh/include', 'soh/src', 'soh/assets', 'soh', 'libultraship/include')]
    for path in ('CMake/soh-cvars.cmake', 'CMake/lus-cvars.cmake'):
        for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT / path).read_text()):
            flags += [f'-D{key}="{value}"']
    with tempfile.TemporaryDirectory(prefix='time-gate-visibility-') as temp:
        binary = Path(temp) / 'test'
        subprocess.run([os.environ.get('CC', 'cc'), *flags,
                        str(ROOT / 'soh/tests/time_gate_visibility_test.c'),
                        str(ROOT / 'soh/mods/items/objects/object_timegate.c'),
                        '-lm', '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)

if __name__ == '__main__':
    main()
