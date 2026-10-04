"""Exercise real Ice Arrow GBI/lifecycle and native elemental impact sound paths."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    flags = ["-std=gnu2x", "-O1", "-g", "-DNDEBUG", "-DF3DEX_GBI_2", "-DLOG_LEVEL_GAME_PRINTS=0",
             "-Werror=implicit-function-declaration", "-Wno-incompatible-pointer-types"]
    for path in ("soh/include", "soh/src", "soh/assets", "soh", "libultraship/include"):
        flags.append("-I" + str(ROOT / path))
    for path in ("CMake/soh-cvars.cmake", "CMake/lus-cvars.cmake"):
        for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT / path).read_text()):
            flags.append(f'-D{key}="{value}"')
    with tempfile.TemporaryDirectory(prefix="ice-arrow-snowflake-") as temp:
        binary = Path(temp) / "test"
        subprocess.run([os.environ.get("CC", "cc"), *flags,
                        "soh/tests/ice_arrow_snowflake_test.c",
                        "soh/src/overlays/actors/ovl_Arrow_Ice/z_arrow_ice.c",
                        "soh/src/overlays/actors/ovl_Arrow_Fire/z_arrow_fire.c",
                        "soh/src/overlays/actors/ovl_Arrow_Light/z_arrow_light.c",
                        "-lm", "-o", str(binary)], cwd=ROOT, check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
