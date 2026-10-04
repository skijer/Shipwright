#!/usr/bin/env python3
"""Run the real house collectible with native offer and NEI accessor code.

Actor allocation, hook dispatch, graphics and item-table lookup are boundaries.
The entire new actor body is exercised. No scene archive is changed or required.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile

from run_time_pedestal_tests import block_from, functions, without_includes

ROOT = Path(__file__).resolve().parents[2]


def main():
    source = ROOT / "soh/mods/house_rocs_feather.cpp"
    actor = functions((ROOT / "soh/src/code/z_actor.c").read_text())
    player = functions((ROOT / "soh/src/code/z_player_lib.c").read_text())
    nei = functions((ROOT / "soh/mods/nei_save.cpp").read_text().replace('extern "C" ', ""))
    draw = functions((ROOT / "soh/src/code/z_draw.c").read_text())
    native = [actor[name] for name in ("Actor_SetScale", "Actor_Kill", "Actor_HasParent", "GiveItemEntryFromActor")]
    native += [re.sub(r"\bthis\b", "player", player[name]) for name in
               ("Player_InBlockingCsMode", "Player_InCsMode")]
    native += [nei[name] for name in ("Nei_GetOwnedItem", "Nei_SetOwnedItem")]
    native += [draw["GetItemEntry_Draw"]]
    # Exercise the existing progressive receipt independently of the extra
    # placement. Omit other randomizer rewards, network counters and stats;
    # keep the actual item selection/write and real NEI storage intact.
    rando = functions((ROOT / "soh/soh/Enhancements/randomizer/randomizer.cpp").read_text()
                      .replace('extern "C" ', ""))["Randomizer_Item_Give"]
    roc_case = rando.index("case RG_PROGRESSIVE_ROCS:")
    roc_switch = block_from(rando, rando.index("switch", roc_case))
    native += ["void Fixture_ReceiveRandomizedCheck(GetItemEntry entry) {\n"
               "    REQUIRE(entry.modIndex == MOD_RANDOMIZER);\n"
               "    switch (static_cast<RandomizerGet>(entry.getItemId)) {\n"
               "        case RG_PROGRESSIVE_ROCS:\n" + roc_switch + "\nbreak;\n"
               "        default: REQUIRE(false);\n    }\n}"]
    flags = ["-std=c++20", "-O1", "-g", "-DNDEBUG", "-DF3DEX_GBI_2", "-DLOG_LEVEL_GAME_PRINTS=0",
             "-Ilibultraship/include", "-Isoh/include", "-Isoh/src", "-Isoh/assets", "-Isoh"]
    for path in ("CMake/soh-cvars.cmake", "CMake/lus-cvars.cmake"):
        for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT / path).read_text()):
            flags.append(f'-D{key}="{value}"')
    with tempfile.TemporaryDirectory(prefix="house-feather-") as directory:
        out = Path(directory)
        (out / "house_rocs_feather_native.inc").write_text("\n".join(native))
        (out / "house_rocs_feather_production.inc").write_text(
            without_includes(source.read_text()) if source.exists() else "// Feature absent: no registered hook.\n")
        binary = out / "house_rocs_feather_test"
        compiler = os.environ.get("CXX", "c++")
        subprocess.run([compiler, *flags, "-I" + directory, "-fsanitize=undefined",
                        "-fno-sanitize-recover=undefined", "soh/tests/house_rocs_feather_test.cpp",
                        "-o", str(binary)], cwd=ROOT, check=True)
        subprocess.run([str(binary)], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
