"""Compile the real fire-sword renderer against actual game headers and GBI."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    flags = ["-std=gnu2x", "-O1", "-g", "-DNDEBUG", "-DF3DEX_GBI_2", "-DLOG_LEVEL_GAME_PRINTS=0",
             "-Werror=implicit-function-declaration"]
    for path in ("soh/include", "soh/src", "soh/assets", "soh", "libultraship/include"):
        flags.append("-I" + str(ROOT / path))
    for path in ("CMake/soh-cvars.cmake", "CMake/lus-cvars.cmake"):
        for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT / path).read_text()):
            flags.append(f'-D{key}="{value}"')
    with tempfile.TemporaryDirectory(prefix="din-fire-sword-") as temp:
        binary = Path(temp) / "test"
        subprocess.run([os.environ.get("CC", "cc"), *flags,
                        "soh/tests/din_fire_sword_test.c", "soh/src/code/din_fire_sword.c",
                        "-lm", "-o", str(binary)], cwd=ROOT, check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
