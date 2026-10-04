"""Exercise the production barrier draw and color commands without running the game."""
from pathlib import Path
import subprocess
import tempfile

from run_oot_custom_cosmetics_tests import function

ROOT = Path(__file__).resolve().parents[2]


def main():
    source = (ROOT / "soh/mods/transformation_masks/mm_player_form.cpp").read_text()
    patch = function(source, "MmForm_PatchZoraBarrierColors")
    properties_start = source.index("typedef struct {\n    const char* skelPath;")
    properties_end = source.index("\n};", source.index("static const MmFormProperties sFormProps")) + 3
    matrix_source = (ROOT / "soh/src/code/sys_matrix.c").read_text()
    skin_source = (ROOT / "soh/src/code/z_skin_matrix.c").read_text()
    matrix = "\n".join(function(skin_source, name) for name in [
        "SkinMatrix_SetTranslate", "SkinMatrix_SetScale", "SkinMatrix_SetRotateZYX",
    ])
    matrix += "\n" + "\n".join(function(matrix_source, name) for name in [
        "Matrix_MtxFCopy", "Matrix_Push", "Matrix_Pop", "Matrix_Translate",
        "Matrix_RotateX", "Matrix_RotateY", "Matrix_RotateZ", "Matrix_RotateZYX",
        "Matrix_SetTranslateRotateYXZ", "Matrix_Scale",
    ])
    with tempfile.TemporaryDirectory(prefix="zora-barrier-") as directory:
        out = Path(directory)
        (out / "zora_barrier_properties.inc").write_text(source[properties_start:properties_end])
        (out / "zora_barrier_production.inc").write_text(patch + "\n" + function(source, "MmForm_DrawZoraBarrier"))
        (out / "zora_barrier_matrix.inc").write_text(matrix)
        binary = out / "test"
        subprocess.run([
            "c++", "-std=gnu++20", "-O1", "-g", "-DNDEBUG", "-DF3DEX_GBI_2", "-DLOG_LEVEL_GAME_PRINTS=0",
            '-DCVAR_PREFIX_COSMETIC="gCosmetics"', "-Isoh/include", "-Isoh/src", "-Isoh/assets", "-Isoh",
            "-Ilibultraship/include", "-I" + str(out), "soh/tests/zora_barrier_cosmetics_test.cpp",
            "-o", str(binary),
        ], cwd=ROOT, check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
