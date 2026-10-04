#!/usr/bin/env python3
"""Run production chest sizing/update logic without a ROM, then compile the actor."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

from run_time_pedestal_tests import functions

ROOT = Path(__file__).resolve().parents[2]


def main():
    source_path = ROOT / "soh/src/overlays/actors/ovl_En_Box/z_en_box.c"
    source = source_path.read_text()
    chest = functions(source)
    actor = functions((ROOT / "soh/src/code/z_actor.c").read_text())
    category = functions((ROOT / "soh/soh/Enhancements/randomizer/item_category_adj.cpp").read_text())
    selected = [actor[name] for name in ("Actor_SetScale", "Actor_SetFocus")]
    selected += [category["Randomizer_AdjustItemCategory"]]
    selected += [body for name, body in chest.items() if name in {
        "EnBox_IsTimeGateChest", "EnBox_SetupAction", "EnBox_WaitOpen", "EnBox_SpawnIceSmoke",
        "EnBox_LoadChestDL", "EnBox_UpdateSizePosition", "EnBox_UpdateTexture", "EnBox_Update"}]
    prototypes = "\n".join(body[:body.index("{")] + ";" for body in selected)
    preamble = "\n".join(re.findall(r"^#define ENBOX_MOVE_.*$", source, re.M))
    preamble += "\n" + re.search(r"static AnimationHeader\* sAnimations\[4\] = \{.*?\};", source, re.S)[0]
    inventory = (ROOT / "soh/src/code/z_inventory.c").read_text()
    for name in ("gItemSlots", "gBitFlags"):
        preamble += "\n" + re.search(r"u\d+ " + name + r"\[\] = \{.*?\};", inventory, re.S)[0]
    flags = ["-std=gnu11", "-O1", "-g", "-DF3DEX_GBI_2", "-DLOG_LEVEL_GAME_PRINTS=0",
             "-Werror=implicit-function-declaration", "-Wno-incompatible-pointer-types",
             "-Wno-discarded-array-qualifiers", "-Wno-int-conversion",
             "-Ilibultraship/include", "-Isoh/include", "-Isoh/src", "-Isoh/assets", "-Isoh"]
    for path in ("CMake/soh-cvars.cmake", "CMake/lus-cvars.cmake"):
        for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT / path).read_text()):
            flags.append(f'-D{key}="{value}"')
    with tempfile.TemporaryDirectory(prefix="chest-size-") as directory:
        folder = Path(directory)
        (folder / "chest_size_production.inc").write_text(
            preamble + "\n" + prototypes + "\n" + "\n".join(selected))
        binary = folder / "chest_size_test"
        compiler = os.environ.get("CC", "cc")
        subprocess.run([compiler, *flags, "-I" + directory, "-fsanitize=undefined",
                        "-fno-sanitize-recover=undefined", "soh/tests/chest_size_test.c",
                        "-lm", "-o", str(binary)], cwd=ROOT, check=True)
        subprocess.run([str(binary)], cwd=ROOT, check=True)
        subprocess.run([compiler, *flags, "-fsyntax-only", str(source_path)], cwd=ROOT, check=True)
        print("PASS: complete chest actor compiles against integration headers")


if __name__ == "__main__":
    main()
