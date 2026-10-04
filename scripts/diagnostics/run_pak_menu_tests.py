"""Run focused PAK regressions without the game or its platform dependencies.

Pass the path to libultraship's ImGui v1.91.9b-docking source checkout. The
production template is extracted unchanged, following the audio runtime tests.
"""
import os
import pathlib
import re
import shlex
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]


def main(imgui):
    imgui = pathlib.Path(imgui).resolve()
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    with tempfile.TemporaryDirectory(prefix="soh-pak-tests-") as temporary:
        build = pathlib.Path(temporary)
        selection_test = build / "pak_selection_test"
        subprocess.run([
            *compiler, "-std=c++20", "-g", "-DNDEBUG", "-Wall", "-Wextra", "-Werror",
            *shlex.split(os.environ.get("PAK_TEST_CFLAGS", "")),
            "-I" + str(ROOT / "soh"), str(ROOT / "soh/tests/pak_selection_test.cpp"),
            "-o", str(selection_test)], check=True)
        subprocess.run([str(selection_test)], check=True)
        source = ROOT / "soh/mods/pak_loader/pak_loader.cpp"
        text = source.read_text()
        functions = []
        for name in (
            "PakLoader_ModelHasAdult", "PakLoader_ModelHasChild", "PakLoader_ModelHasAnyEquipment",
            "PakLoader_PakProvidesSlot", "PakLoader_SaveSelection", "RestoreSelection", "EnsureSlotMixLoaded",
            "RestoreSelections", "PakLoader_SelectAdultModel", "PakLoader_SelectChildModel",
            "PakLoader_SelectEquipment", "PakLoader_SetSlotMix",
        ):
            match = re.search(
                r'^(?:template <[^\n]+>\s*)?(?:(?:extern "C"|static)\s+)*(?:void|u8|s32|bool)\s+' + name +
                r'\([^;]*?\)\s*\{.*?^}', text, re.M | re.S)
            if match is None:
                raise RuntimeError(f"Cannot locate production function {name}")
            functions.append(f'#line {text.count(chr(10), 0, match.start()) + 1} "{source}"\n{match[0]}\n')
        (build / "pak_selection_runtime.inc").write_text("\n".join(functions))
        runtime_test = build / "pak_selection_runtime_test"
        subprocess.run([
            *compiler, "-std=c++20", "-g", "-DNDEBUG", "-Wall", "-Wextra", "-Werror",
            *shlex.split(os.environ.get("PAK_TEST_CFLAGS", "")),
            "-I" + str(ROOT / "soh"), "-I" + str(build),
            str(ROOT / "soh/tests/pak_selection_runtime_test.cpp"), "-o", str(runtime_test)], check=True)
        subprocess.run([str(runtime_test)], check=True)
        source = ROOT / "soh/soh/SohGui/UIWidgets.hpp"
        text = source.read_text()
        match = re.search(
            r"template <typename T>\nbool Combobox\(std::string label, T\* value, "
            r"const std::map<T, const char\*>& comboMap,.*?^}", text, re.M | re.S)
        if match is None:
            raise RuntimeError("Cannot locate production map Combobox template")
        (build / "pak_menu_combobox.inc").write_text(
            f'#line {text.count(chr(10), 0, match.start()) + 1} "{source}"\n{match[0]}\n')
        executable = build / "pak_menu_combobox_test"
        subprocess.run([
            *compiler, "-std=c++20", "-g", "-DNDEBUG", "-Wall", "-Wextra",
            *shlex.split(os.environ.get("PAK_TEST_CFLAGS", "")),
            "-I" + str(imgui), "-I" + str(build),
            str(ROOT / "soh/tests/pak_menu_combobox_test.cpp"),
            *[str(imgui / name) for name in
              ("imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp")],
            "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main(*sys.argv[1:])
