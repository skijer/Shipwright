"""Exercise real SW97 arrow lifecycles, textures, cosmetics and sound timing."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def check_editor_controls():
    # Both groups share one ImGui tab and derive widget IDs from their labels.
    source = (ROOT / "soh/soh/Enhancements/cosmetics/CosmeticsEditor.cpp").read_text()
    rows = re.findall(r'COSMETIC_OPTION\("([^"]+)",\s*"([^"]+)",\s*'
                      r'(COSMETICS_GROUP_\w+),\s*ColorRGBA8\([^)]*\),\s*'
                      r'(true|false),\s*(true|false),\s*(true|false)\)', source)
    effects = {"COSMETICS_GROUP_MAGIC", "COSMETICS_GROUP_ARROWS",
               "COSMETICS_GROUP_SPIN_ATTACK", "COSMETICS_GROUP_TRAILS"}
    for prefix, elements, group in (
        ("Arrows", ("Fire", "Water", "Forest", "Shadow", "Light", "Spirit"), "COSMETICS_GROUP_ARROWS"),
        ("Magic", ("Fire", "Water", "Forest"), "COSMETICS_GROUP_MAGIC"),
    ):
        for element in elements:
            for channel in ("Primary", "Secondary"):
                key = f"{prefix}.Medallion{element}{channel}"
                selected = [row for row in rows if row[0] == key]
                assert len(selected) == 1, key
                _, label, actual_group, alpha, rainbow, advanced = selected[0]
                assert (actual_group, alpha, rainbow, advanced) == (group, "false", "true", "false"), key
                assert sum(row[1] == label and row[2] in effects for row in rows) == 1, label
    print("PASS all 18 medallion controls visible in Effects with unique widget labels and rainbow support")


def main():
    check_editor_controls()
    flags = ["-std=gnu2x", "-O1", "-g", "-DNDEBUG", "-DF3DEX_GBI_2", "-DLOG_LEVEL_GAME_PRINTS=0",
             "-Werror=implicit-function-declaration", "-Wno-incompatible-pointer-types",
             "-ffunction-sections", "-fdata-sections"]
    for path in ("soh/include", "soh/src", "soh/assets", "soh", "libultraship/include"):
        flags.append("-I" + str(ROOT / path))
    for path in ("CMake/soh-cvars.cmake", "CMake/lus-cvars.cmake"):
        for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT / path).read_text()):
            flags.append(f'-D{key}="{value}"')
    with tempfile.TemporaryDirectory(prefix="medallion-arrow-textures-") as temp:
        binary = Path(temp) / "test"
        subprocess.run([os.environ.get("CC", "cc"), *flags,
                        "soh/tests/medallion_arrow_textures_test.c",
                        "-Wl,--gc-sections", "-lm", "-o", str(binary)], cwd=ROOT, check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
