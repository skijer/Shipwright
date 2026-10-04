"""Bounded ground-leaf activation harness; does not reproduce GPU or resource-manager state."""
import re
import struct
import os
import sys
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests/nei_item_stow'))
from run_ballchain_tests import flags
sys.path.insert(0, str(ROOT / "scripts/diagnostics"))
from run_time_pedestal_tests import functions
# Validate the packed payload used by the resource loader against the fixture.
source = (ROOT / 'soh/mods/items/anim/deku_leaf/dekuleaf_anim_data.c').read_text()
values = [int(v, 16) for v in re.findall(r'0x[0-9a-fA-F]+', source)]
packed = (ROOT / 'soh/assets/custom/misc/link_animetion/gPlayerAnim_nei_dekuleaf_blow').read_bytes()
assert len(values) == 39 * 67
assert len(packed) == 68 + len(values) * 2
assert packed[68:] == struct.pack('<' + 'H' * len(values), *values)
with tempfile.TemporaryDirectory(prefix='nei-leaf-') as td:
    native = functions((ROOT / 'soh/src/code/z_skelanime.c').read_text(), {'LinkAnimation_Once'})
    (Path(td) / 'leaf_native_once.inc').write_text(native['LinkAnimation_Once'])
    binary = str(Path(td) / 'leaf')
    cc, cxx = os.environ.get('CC', 'cc'), os.environ.get('CXX', 'c++')
    common = ['-g', '-fsanitize=address,undefined', '-ffunction-sections', '-fdata-sections']
    cxx_flags = ['-std=c++20', '-I' + str(ROOT / 'tests/nei_leaf/stubs')]
    cxx_flags += [flag for flag in flags() if not flag.startswith('-std=')]
    engine = str(Path(td) / 'mm_sfx.o')
    activation = str(Path(td) / 'activation.o')
    subprocess.run([cxx, *cxx_flags, *common, '-c', str(ROOT / 'tests/nei_leaf/mm_sfx_engine_fixture.cpp'),
                    '-o', engine], check=True)
    subprocess.run([cc, *flags(), '-I'+td, *common, '-c', str(ROOT / 'tests/nei_leaf/activation_test.c'),
                    '-o', activation], check=True)
    subprocess.run([cxx, *common, activation, engine, '-Wl,--gc-sections', '-lm', '-o', binary], check=True)
    bank_test = str(Path(td) / 'mm_sfx_stop')
    subprocess.run([cxx, *cxx_flags, *common, str(ROOT / 'tests/nei_leaf/mm_sfx_stop_test.cpp'),
                    str(ROOT / 'soh/mods/sound_translator/mm_audio_sfx_params.cpp'), engine,
                    '-Wl,--gc-sections', '-lm', '-o', bank_test], check=True)
    # LeakSanitizer cannot enumerate threads under this sandbox; retain ASan/UBSan.
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
    # Separate processes preserve real cold-start storage. Bound hangs so this
    # regression fails clearly instead of leaving local runs or CI stuck.
    for command in ([bank_test, 'initialized'], [binary], [bank_test]):
        try:
            subprocess.run(command, check=True, env=env, timeout=5)
        except subprocess.TimeoutExpired:
            raise SystemExit('FAIL: MM sound stop hung; Leaf completion must return before first MM playback')
