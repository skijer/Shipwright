"""Check real pedestal/player headers, including declarations hidden by fixtures."""
import os
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
args = [os.environ.get("CC", "cc"), "-std=gnu2x", "-fsyntax-only",
        "-Werror=implicit-function-declaration", "-Wno-incompatible-pointer-types", "-Wno-int-conversion",
        "-DLOG_LEVEL_GAME_PRINTS=0", "-DF3DEX_GBI_2"]
# SoH intentionally passes its OTR resource-name arrays through legacy typed
# asset pointers. Keep that established convention while rejecting missing APIs.
args += ["-I" + str(ROOT / path) for path in
         ("soh/include", "soh/src", "soh/assets", "soh", "soh/mods", "libultraship/include")]
for path in ("CMake/soh-cvars.cmake", "CMake/lus-cvars.cmake"):
    for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT / path).read_text()):
        args.append(f'-D{key}="{value}"')
for path in ("soh/src/overlays/actors/ovl_Bg_Toki_Swd/z_bg_toki_swd.c",
             "soh/src/code/z_player_lib.c",
             "soh/src/overlays/actors/ovl_player_actor/z_player.c",
             "soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c",
             "soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c"):
    result = subprocess.run([*args, str(ROOT / path)], capture_output=True, text=True)
    if result.returncode:
        sys.stderr.write(result.stderr)
        raise SystemExit(result.returncode)
    print("PASS real-header C23 syntax:", path)
