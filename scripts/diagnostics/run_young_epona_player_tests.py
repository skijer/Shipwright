#!/usr/bin/env python3
"""Exercise production player horse actions with real engine headers, then compile the full player."""
import argparse
import os
from pathlib import Path
import re
import resource
import subprocess
import tempfile

from run_child_ruto_face_test import function

ROOT = Path(__file__).resolve().parents[2]
PLAYER = ROOT / "soh/src/overlays/actors/ovl_player_actor/z_player.c"


def declaration(source, name):
    match = re.search(r"^static [^;\n]*\b" + re.escape(name) + r"\[.*?^};", source, re.M | re.S)
    if match is None:
        raise RuntimeError(f"Missing production declaration: {name}")
    return match.group() + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", choices=("mount", "gate", "camera", "save", "dismount", "age", "all"), default="all")
    parser.add_argument("--skip-syntax", action="store_true", help="Use only for an intentional failing baseline run")
    args = parser.parse_args()
    source = PLAYER.read_text()
    actor = (ROOT / "soh/src/code/z_actor.c").read_text()
    horse = (ROOT / "soh/src/code/z_horse.c").read_text()
    types = source[source.index("typedef enum AnimSfxType"):source.index("typedef struct struct_808551A4")]
    types += re.search(r"typedef struct struct_80854578 \{.*?} struct_80854578;", source, re.S).group()
    tables = "".join(declaration(source, name) for name in (
        "D_80854578", "D_808548FC", "D_80854914", "D_8085492C", "D_80854944", "D_80854968",
        "D_8085498C", "D_80854998", "D_808549A4", "D_808549C4"))
    tables += re.search(r"^static Vec3s D_8085499C = .*?;", source, re.M).group() + "\n"
    if "static struct_80854578 sYoungEponaMountInfo" in source:
        tables += declaration(source, "sYoungEponaMountInfo")
    production = function(actor, "Actor_MountHorse")
    production += function(horse, "Horse_CanSpawnYoung") + function(horse, "Horse_SaveYoungEpona")
    production += "".join(function(source, name) for name in (
        "Player_SetupActionPreserveAnimMovement", "Player_SetupWaitForPutAway", "Player_Action_WaitForPutAway",
        "func_8083A360", "func_8083C0E8", "Player_ActionHandler_3",
        "func_8084C89C", "func_8084C9BC", "func_8084CBF4", "Player_Action_8084CC98", "Player_Action_8084D3E4"))
    if "static void Player_DetachYoungEponaOnAgeChange" in source:
        production += function(source, "Player_DetachYoungEponaOnAgeChange")
        production += "#define HAVE_YOUNG_EPONA_AGE_DETACH 1\n"
    fixture = (ROOT / "soh/tests/young_epona_player_test.c").read_text()
    fixture = fixture.replace("/* PRODUCTION_TYPES */", types).replace("/* PRODUCTION_TABLES */", tables)
    fixture = fixture.replace("/* PRODUCTION_PLAYER */", production)
    flags = ["-std=gnu2x", "-DF3DEX_GBI_2", "-DLOG_LEVEL_GAME_PRINTS=0", "-DNDEBUG",
             "-Werror=implicit-function-declaration", "-Wno-int-conversion", "-Wno-incompatible-pointer-types",
             "-Wno-discarded-qualifiers", "-Ilibultraship/include", "-Isoh/include", "-Isoh/src",
             "-Isoh/assets", "-Isoh", "-Isoh/mods", "-fsanitize=undefined", "-fno-sanitize-recover=all"]
    for path in ("CMake/soh-cvars.cmake", "CMake/lus-cvars.cmake"):
        for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT / path).read_text()):
            flags.append(f'-D{key}="{value}"')
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    with tempfile.TemporaryDirectory(prefix="young-epona-player-") as folder:
        cfile, binary = Path(folder) / "test.c", Path(folder) / "test"
        cfile.write_text(fixture)
        subprocess.run([os.environ.get("CC", "cc"), *flags, str(cfile), "-lm", "-o", str(binary)], cwd=ROOT, check=True)
        subprocess.run([str(binary), args.case], cwd=ROOT, check=True)
        if not args.skip_syntax:
            result = subprocess.run([os.environ.get("CC", "cc"), *flags, "-fsyntax-only", str(PLAYER)],
                                    cwd=ROOT, capture_output=True, text=True)
            if result.returncode:
                raise RuntimeError(result.stderr)
            print("PASS: whole z_player.c compiles against production headers")


if __name__ == "__main__":
    main()
