"""Run Demise's real item scheduling, native lightning, and effect-pool code."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

from run_time_pedestal_tests import functions

ROOT = Path(__file__).resolve().parents[2]


def main():
    flags = ["-std=gnu2x", "-O1", "-g", "-DNDEBUG", "-DF3DEX_GBI_2", "-DLOG_LEVEL_GAME_PRINTS=0",
             "-ffunction-sections", "-fdata-sections", "-Werror=implicit-function-declaration",
             "-Wno-incompatible-pointer-types", "-Wno-discarded-qualifiers", "-Wno-int-conversion",
             "-include", "global.h"]
    for path in ("soh/include", "soh/src", "soh/assets", "soh", "libultraship/include"):
        flags.append("-I" + str(ROOT / path))
    for path in ("CMake/soh-cvars.cmake", "CMake/lus-cvars.cmake"):
        for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT / path).read_text()):
            flags.append(f'-D{key}="{value}"')
    with tempfile.TemporaryDirectory(prefix="demise-lightning-") as temp:
        temp = Path(temp)
        # Keep the real spawn argument packaging; unrelated particle allocators
        # are external boundaries recorded by the fixture, not replacement item logic.
        spawn = functions((ROOT / "soh/src/code/z_effect_soft_sprite_old_init.c").read_text())[
            "EffectSsLightning_Spawn"]
        wrapper = temp / "lightning_spawn.c"
        wrapper.write_text('#include "overlays/effects/ovl_Effect_Ss_Lightning/z_eff_ss_lightning.h"\n' + spawn)
        binary = temp / "test"
        subprocess.run([os.environ.get("CC", "cc"), *flags,
                        "soh/tests/demise_lightning_test.c", "soh/mods/items/helpers/fx_helper.c",
                        "soh/src/overlays/effects/ovl_Effect_Ss_Lightning/z_eff_ss_lightning.c",
                        "soh/src/code/z_effect_soft_sprite.c", "soh/src/code/z_skin_matrix.c", str(wrapper),
                        "-Wl,--gc-sections", "-lm", "-o", str(binary)], cwd=ROOT, check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
