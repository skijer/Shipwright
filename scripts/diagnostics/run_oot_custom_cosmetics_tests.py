"""Run the OoT custom cosmetic scanner against XML and display-list fixtures.

The scanner, grouping, randomizer and GBI encoding are production code. Only
archive/resource services, CVar persistence and UI output are replaced. This
checks command/state changes, not game rendering or real mod compatibility.
"""
import os
import pathlib
import re
import shlex
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]


def function(source, name):
    match = re.search(r'^(?:extern "C" )?[\w:* ]+\b' + re.escape(name) +
                      r'\([^;]*?\)\s*\{.*?^}', source, re.M | re.S)
    if match is None:
        raise RuntimeError(f"Cannot find production function {name}")
    return match[0]


def main():
    header = (ROOT / "soh/soh/Enhancements/cosmetics/CosmeticsEditor.h").read_text()
    editor = (ROOT / "soh/soh/Enhancements/cosmetics/CosmeticsEditor.cpp").read_text()
    dynamic = (ROOT / "soh/soh/Enhancements/cosmetics/DynamicCosmeticsEditor.cpp").read_text()
    utility = (ROOT / "soh/soh/ShipUtils.cpp").read_text()
    widgets = (ROOT / "soh/soh/SohGui/UIWidgets.cpp").read_text()

    # Compile all dynamic logic; the fixture supplies only its service/UI boundary.
    dynamic = dynamic[dynamic.index("static constexpr const char* CUSTOM_COSMETIC_GROUP"):]
    declarations = header[header.index("typedef enum {"):header.index("#ifdef __cplusplus")]
    option_start = header.index("typedef struct {\n    const char* cvar;")
    declarations += header[option_start:header.index("typedef struct {\n    const std::string Name;", option_start)]
    declarations += "\nvoid RandomizeColor(CosmeticOption&, bool manual = true);\n"
    random = "\n".join(function(utility, name) for name in ["ShipUtils::RandInit", "ShipUtils::next32", "ShipUtils::RandomDouble"])
    random += "\n" + function(widgets, "GetRandomValue")
    random += "\n" + "\n".join(function(editor, name) for name in ["CopyMultipliedColor", "ApplySideEffects", "RandomizeColor"])
    boundary = "\n".join(function(editor, name) for name in [
        "CosmeticsEditor_AutoRandomizeAll", "CosmeticsEditor_RandomizeAll", "CosmeticsEditor_ResetAll",
    ])
    with tempfile.TemporaryDirectory(prefix="oot-custom-cosmetics-") as temporary:
        build = pathlib.Path(temporary)
        (build / "custom_cosmetic_declarations.inc").write_text(declarations)
        (build / "custom_cosmetic_randomizer.inc").write_text(random)
        (build / "custom_cosmetic_production.inc").write_text(dynamic)
        (build / "custom_cosmetic_boundary.inc").write_text(boundary)
        executable = build / "custom_cosmetic_test"
        subprocess.run([
            *shlex.split(os.environ.get("CXX", "c++")), "-std=c++20", "-O1", "-g",
            "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", "-Wno-missing-field-initializers",
            "-DF3DEX_GBI_2", "-I" + str(build), "-I" + str(ROOT / "libultraship/include"),
            "-I" + str(ROOT / "ZAPDTR/lib/tinyxml2"),
            str(ROOT / "soh/tests/custom_cosmetics_test.cpp"),
            str(ROOT / "ZAPDTR/lib/tinyxml2/tinyxml2.cpp"), "-o", str(executable),
        ], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
