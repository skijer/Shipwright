"""Execute real item draw routines and inspect emitted display-list state.

Graphics allocation/setup, resource classification and CVar storage are boundaries;
the item routines and GBI macros are production code. No game/render claim is made.
"""
from pathlib import Path
import subprocess
import tempfile

from run_time_pedestal_tests import functions

ROOT = Path(__file__).resolve().parents[2]


def main():
    selected = []
    for path, names in [
        ("soh/src/code/z_draw.c", ["GetItem_DrawDListWithCosmetics", "GetItem_DrawOpa0", "GetItem_DrawXlu01"]),
        ("soh/src/code/z_en_item00.c", ["EnItem00_DrawHeartContainer", "EnItem00_DrawHeartPiece"]),
        ("soh/src/overlays/actors/ovl_Item_B_Heart/z_item_b_heart.c", ["ItemBHeart_Draw"]),
        ("soh/soh/Enhancements/randomizer/draw.cpp", ["Randomizer_DrawDoubleDefense"]),
    ]:
        # The selected C++ function has a C-compatible body; omit only linkage
        # syntax in this C fixture. Its original C++ form is checked below.
        found = functions((ROOT / path).read_text().replace('extern "C" ', ''))
        for name in names:
            if name == "GetItem_DrawDListWithCosmetics" and name not in found:
                continue  # The pre-fix run must fail on absent tint, not compilation.
            selected.append(found[name])
    with tempfile.TemporaryDirectory(prefix="custom-item-color-") as directory:
        out = Path(directory)
        (out / "custom_item_color_production.inc").write_text("\n".join(selected))
        binary = out / "custom_item_color_test"
        cmd = ["cc", "-std=gnu11", "-O1", "-g", "-DNDEBUG", "-DF3DEX_GBI_2", "-DLOG_LEVEL_GAME_PRINTS=0",
               '-DCVAR_PREFIX_COSMETIC="gCosmetics"', "-Isoh/include", "-Isoh/src",
               "-Isoh/assets", "-Isoh", "-Ilibultraship/include", "-I" + str(out),
               "-Wno-incompatible-pointer-types", "soh/tests/custom_item_color_test.c",
               "-o", str(binary)]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(binary)], check=True)
        syntax = cmd[:cmd.index("soh/tests/custom_item_color_test.c")] + [
            "-fsyntax-only", "-Werror=implicit-function-declaration", "-Wno-int-conversion",
            "-Wno-discarded-qualifiers", '-DCVAR_PREFIX_ENHANCEMENT="gEnhancements"',
            '-DCVAR_PREFIX_SETTING="gSettings"', '-DCVAR_PREFIX_REMOTE="gRemote"',
            '-DCVAR_PREFIX_RANDOMIZER_SETTING="gRandomizerSettings"',
        ]
        for source in ["soh/src/code/z_draw.c", "soh/src/code/z_en_item00.c",
                       "soh/src/overlays/actors/ovl_Item_B_Heart/z_item_b_heart.c"]:
            subprocess.run(syntax + [source], cwd=ROOT, check=True)
        print("PASS complete item draw translation units and resource bridge declaration")
        cpp_source = (ROOT / "soh/soh/Enhancements/randomizer/draw.cpp").read_text()
        cpp_body = functions(cpp_source.replace('extern "C" ', ''))["Randomizer_DrawDoubleDefense"]
        cpp_check = out / "double_defense.cpp"
        cpp_check.write_text('#include "z64.h"\nextern "C" {\n#include "macros.h"\n'
                             '#include "functions.h"\n#include "variables.h"\n'
                             '#include "objects/object_gi_hearts/object_gi_hearts.h"\n}\n'
                             'extern "C" ' + cpp_body + '\n')
        cpp_cmd = ["c++", "-std=gnu++20"] + cmd[2:cmd.index("-Wno-incompatible-pointer-types")]
        subprocess.run(cpp_cmd + ["-fsyntax-only", str(cpp_check)], cwd=ROOT, check=True)
        print("PASS Double Defense C++ body and shared C linkage")


if __name__ == "__main__":
    main()
