"""Exercise production SW97 cast limbs and isolated tornado dust textures."""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    flags = ["-std=gnu2x", "-O1", "-g", "-DNDEBUG", "-DF3DEX_GBI_2", "-DLOG_LEVEL_GAME_PRINTS=0",
             "-Werror=implicit-function-declaration", "-Wno-incompatible-pointer-types", "-Wno-int-conversion", "-Wno-discarded-qualifiers",
             "-ffunction-sections", "-fdata-sections"]
    for path in ("soh/include", "soh/src", "soh/assets", "soh", "libultraship/include"):
        flags.append("-I" + str(ROOT / path))
    for path in ("CMake/soh-cvars.cmake", "CMake/lus-cvars.cmake"):
        for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT / path).read_text()):
            flags.append(f'-D{key}="{value}"')
    with tempfile.TemporaryDirectory(prefix="medallion-cast-textures-") as temp:
        binary = Path(temp) / "test"
        subprocess.run([os.environ.get("CC", "cc"), *flags,
                        "soh/tests/medallion_cast_textures_test.c",
                        "soh/src/code/z_fcurve_data_skelanime.c",
                        "soh/src/code/z_effect_soft_sprite_old_init.c",
                        "-Wl,--gc-sections", "-lm", "-o", str(binary)], cwd=ROOT, check=True)
        subprocess.run([str(binary), *sys.argv[1:]], check=True)


if __name__ == "__main__":
    main()
