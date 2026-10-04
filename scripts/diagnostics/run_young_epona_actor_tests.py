#!/usr/bin/env python3
"""Exercise the production horse actor against real integration headers."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FLAGS = ["-std=gnu2x", "-DF3DEX_GBI_2", "-DLOG_LEVEL_GAME_PRINTS=0", "-DNDEBUG",
         "-Werror=implicit-function-declaration", "-Wno-int-conversion",
         "-Wno-incompatible-pointer-types", "-Wno-discarded-qualifiers",
         "-ffunction-sections", "-fdata-sections",
         "-Ilibultraship/include", "-Isoh/include", "-Isoh/src", "-Isoh/assets", "-Isoh", "-Isoh/mods"]
for path in ("CMake/soh-cvars.cmake", "CMake/lus-cvars.cmake"):
    for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT / path).read_text()):
        FLAGS.append(f'-D{key}="{value}"')

with tempfile.TemporaryDirectory(prefix="young-epona-actor-") as folder:
    binary = str(Path(folder) / "actor")
    subprocess.run([os.environ.get("CC", "cc"), *FLAGS,
                    "soh/tests/young_epona_actor_test.c", "soh/src/code/z_skin_matrix.c",
                    "-Wl,--gc-sections", "-lm", "-o", binary], cwd=ROOT, check=True)
    subprocess.run([binary], cwd=ROOT, check=True)
    print("PASS: entire z_en_horse.c compiled against real integration headers", flush=True)
