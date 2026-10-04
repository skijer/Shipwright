"""Run the real sword, collision and drop code against production actor tables."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

from run_time_pedestal_tests import functions
from din_fire_sword_reaction_fixture import build_reactions

ROOT = Path(__file__).resolve().parents[2]


def main():
    flags = ["-std=gnu2x", "-O1", "-g", "-DNDEBUG", "-DF3DEX_GBI_2", "-DLOG_LEVEL_GAME_PRINTS=0",
             "-ffunction-sections", "-fdata-sections", "-w", "-Werror=implicit-function-declaration"]
    for path in ("soh/include", "soh/src", "soh/assets", "soh", "libultraship/include"):
        flags.append("-I" + str(ROOT / path))
    for path in ("CMake/soh-cvars.cmake", "CMake/lus-cvars.cmake"):
        for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT / path).read_text()):
            flags.append(f'-D{key}="{value}"')
    tables = {
        "DekuBaba": ("En_Dekubaba", "sDekuBabaDamageTable"),
        "ReDead": ("En_Rd", "sDamageTable"),
        "Anubis": ("En_Anubice", "sDamageTable[0]"),
        "Wolfos": ("En_Wf", "sDamageTable"),
        "Freezard": ("En_Fz", "sDamageTable"),
        "Lizalfos": ("En_Zf", "sDamageTable"),
        "Armos": ("En_Am", "sDamageTable"),
        "Firefly": ("En_Firefly", "sDamageTable"),
        "Tailpasaran": ("En_Tp", "sDamageTable"),
        "Wallmaster": ("En_Wallmas", "sDamageTable"),
        "Floormaster": ("En_Floormas", "sDamageTable"),
        "Crow": ("En_Crow", "sDamageTable"),
        "DekuNuts": ("En_Dekunuts", "sDamageTable"),
        "Peahat": ("En_Peehat", "sDamageTable"),
    }
    with tempfile.TemporaryDirectory(prefix="din-fire-sword-damage-") as temp:
        temp = Path(temp)
        wrappers = []
        # Compile unchanged actor translation units and expose only their actual
        # static table/initializer. Section GC discards unneeded runtime code.
        for name, (actor, table) in tables.items():
            wrapper = temp / f"{name}.c"
            source = f"overlays/actors/ovl_{actor}/z_{actor.lower()}.c"
            wrapper.write_text(f'#include "{source}"\nDamageTable* Test_{name}Table(void) {{ return &{table}; }}\n')
            wrappers.append(str(wrapper))
        bush = temp / "Bush.c"
        bush.write_text('#include "overlays/actors/ovl_En_Kusa/z_en_kusa.c"\n'
                        'u32 Test_BushMask(void) { return sCylinderInit.info.bumper.dmgFlags; }\n')
        wrappers.append(str(bush))
        withered = temp / "Withered.c"
        withered_setup = functions((ROOT / "soh/src/overlays/actors/ovl_En_Karebaba/z_en_karebaba.c").read_text())[
            "EnKarebaba_SetupUpright"]
        # Run the verbatim production state setup. Subsequent animation actions
        # are irrelevant to the collision mask and are inert fixture boundaries.
        withered.write_text('#include "overlays/actors/ovl_En_Karebaba/z_en_karebaba.h"\n'
                            'void EnKarebaba_Spin(EnKarebaba* baba, PlayState* play) {}\n'
                            'void EnKarebaba_Upright(EnKarebaba* baba, PlayState* play) {}\n' + withered_setup + '\n'
                            'u32 Test_WitheredMask(void) { EnKarebaba baba = { 0 }; '
                            'EnKarebaba_SetupUpright(&baba); return baba.bodyCollider.info.bumper.dmgFlags; }\n')
        wrappers.append(str(withered))
        wrappers.extend(build_reactions(temp, flags))
        binary = temp / "test"
        subprocess.run([os.environ.get("CC", "cc"), *flags,
                        "soh/tests/din_fire_sword_damage_test.c", "soh/src/code/din_fire_sword.c",
                        "soh/src/code/z_collision_check.c", "soh/src/code/z_actor.c", *wrappers,
                        "-Wl,--gc-sections", "-lm", "-o", str(binary)], cwd=ROOT, check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
