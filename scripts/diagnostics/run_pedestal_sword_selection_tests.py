"""Exercise production sword source selection and the child's native grip basis."""
import pathlib
import subprocess
import tempfile
from run_time_pedestal_tests import functions

ROOT = pathlib.Path(__file__).resolve().parents[2]


def main():
    source = (ROOT / "soh/mods/pak_loader/pak_loader.cpp").read_text().replace('extern "C" ', '')
    names = ("IsOtrPathString", "MakeMiniDL", "MergeTimePedestalSwordPieces", "PakLoader_GetTimePedestalSwordDL")
    selected = functions(source, set(names))
    with tempfile.TemporaryDirectory(prefix="pedestal-sword-") as directory:
        directory = pathlib.Path(directory)
        (directory / "pedestal_sword.inc").write_text("\n".join(selected[name] for name in names if name in selected))
        executable = directory / "test"
        subprocess.run(["c++", "-std=c++20", "-Wall", "-Wextra", "-I" + str(directory),
                        str(ROOT / "soh/tests/pedestal_sword_selection_test.cpp"), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)

        custom = functions((ROOT / "soh/soh/Enhancements/customequipment.cpp").read_text().replace('extern "C" ', ''))
        render = functions((ROOT / "soh/src/code/z_player_lib.c").read_text())
        names = ("LoadGfxByName", "LoadCustomGfx", "BuildHandItemDL", "BuildTimePedestalHandItemDL",
                 "CustomEquipment_GetTimePedestalSwordDL", "CustomEquipment_OverrideMasterSwordHand")
        body = "\n".join(custom[name] for name in names if name in custom)
        body += "\n" + "\n".join(render[name] for name in
                                  ("Player_ReverseTimePedestalEquipmentSword", "Player_ApplyTimePedestalSword")
                                  if name in render)
        (directory / "pedestal_child_grip.inc").write_text(body)
        matrix = functions((ROOT / "soh/soh/gu_pc.c").read_text())["guMtxF2L"]
        (directory / "pedestal_matrix.inc").write_text(matrix)
        executable = directory / "child_grip_test"
        subprocess.run(["c++", "-std=c++20", "-Wall", "-Wextra", "-DF3DEX_GBI_2",
                        "-I" + str(directory), "-I" + str(ROOT / "libultraship/include"),
                        str(ROOT / "soh/tests/pedestal_child_grip_test.cpp"), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
