import os
from pathlib import Path
import re
import subprocess
import tempfile
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts/diagnostics'))
from run_time_pedestal_tests import functions

def flags():
    result = ["-DF3DEX_GBI_2", "-DLOG_LEVEL_GAME_PRINTS=0"]
    result += ["-I" + str(ROOT / p) for p in
               ("soh", "soh/include", "soh/src", "soh/assets", "soh/mods", "libultraship/include")]
    for config in ("CMake/soh-cvars.cmake", "CMake/lus-cvars.cmake"):
        for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT / config).read_text()):
            result.append(f'-D{key}="{value}"')
    return result

if __name__ == "__main__":
    subprocess.run([sys.executable,str(ROOT/"tests/nei_used_fx/run_interpolation_test.py")],check=True)
    subprocess.run([sys.executable,str(ROOT/'tests/nei_used_fx/source_contract_test.py')],check=True)
    subprocess.run([sys.executable,str(ROOT/'tests/nei_used_fx/material_test.py')],check=True)
    subprocess.run([sys.executable,str(ROOT/'tests/nei_used_fx/attack_material_test.py')],check=True)
    subprocess.run([sys.executable,str(ROOT/'tests/nei_used_fx/flight_particle_test.py')],check=True)
    subprocess.run([sys.executable,str(ROOT/'tests/nei_used_fx/preserve_approved_test.py')],check=True)
    subprocess.run([sys.executable,str(ROOT/'tests/nei_used_fx/preserve_accepted_fire_test.py')],check=True)
    with tempfile.TemporaryDirectory(prefix="nei-used-fx-") as tmp:
        # The budget test uses the established real-engine graphics fixture.
        # Its unrelated GI-dispatch regressions are stripped at link time.
        (Path(tmp)/'nei_gi_dispatch.inc').write_text(
            'void Player_DrawGetItemImpl(PlayState*, Player*, Vec3f*, s32);\n'
            'void EnGirlA_Draw(Actor*, PlayState*);\n')
        (Path(tmp)/'nei_gi_bounds.inc').write_text('')
        for name in ("policy", "feedback_policy", "presentation", "budget"):
            binary = str(Path(tmp) / name)
            subprocess.run([os.environ.get("CXX", "c++"), "-std=c++20", "-O2", *flags(), '-I'+tmp,
                            '-ffunction-sections','-fdata-sections',
                            str(ROOT / "tests/nei_used_fx" / (name + "_test.cpp")), '-Wl,--gc-sections',
                            "-o", binary], check=True)
            subprocess.run([binary], check=True)
        # Compile the public interface as C, just as the unity item sources use it.
        source = Path(tmp) / "abi.c"
        source.write_text('#include "soh/Enhancements/randomizer/NeiUsedMagicPresentation.h"\n')
        subprocess.run([os.environ.get("CC", "cc"), "-std=gnu2x", *flags(), "-fsyntax-only", str(source)], check=True)
