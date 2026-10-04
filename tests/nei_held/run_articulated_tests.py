"""Focused real-header dispatch and geometry verification for articulated items."""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def flags():
    result = ["-std=c++20", "-DF3DEX_GBI_2", "-DLOG_LEVEL_GAME_PRINTS=0"]
    result += ["-I" + str(ROOT / path) for path in
               ("soh", "soh/include", "soh/src", "soh/assets", "soh/mods", "libultraship/include")]
    for config in ("CMake/soh-cvars.cmake", "CMake/lus-cvars.cmake"):
        for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT / config).read_text()):
            result.append(f'-D{key}="{value}"')
    return result


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="nei-articulated-tests-") as temporary:
        binary = str(Path(temporary) / "runtime")
        pose = str(Path(temporary) / "pose.o")
        whip = str(Path(temporary) / "whip.o")
        subprocess.run([os.environ.get("CC", "cc"), "-std=gnu2x", *flags()[1:],
                        "-ffunction-sections", "-fdata-sections", "-c",
                        str(ROOT / "soh/mods/items/helpers/equip_helper.c"), "-o", pose], check=True)
        subprocess.run([os.environ.get("CC", "cc"), "-std=gnu2x", *flags()[1:],
                        "-ffunction-sections", "-fdata-sections", "-c",
                        str(ROOT / "soh/mods/items/objects/object_whip.c"), "-o", whip], check=True)
        subprocess.run([os.environ.get("CXX", "c++"), *flags(), "-ffunction-sections", "-fdata-sections",
                        str(ROOT / "tests/nei_held/articulated_runtime_test.cpp"), pose, whip,
                        "-Wl,--gc-sections", "-o", binary], check=True)
        subprocess.run([binary, *sys.argv[1:]], check=True)
    subprocess.run([sys.executable, str(ROOT / "tests/nei_held/articulated_geometry_test.py")], check=True)
