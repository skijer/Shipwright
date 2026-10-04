"""Exercise production held-state functions with real game types and small engine fixtures."""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/diagnostics"))
from run_time_pedestal_tests import functions


def flags():
    result = ["-std=gnu2x", "-DF3DEX_GBI_2", "-DLOG_LEVEL_GAME_PRINTS=0",
              "-Werror=implicit-function-declaration"]
    result += ["-I" + str(ROOT / path) for path in
               ("soh", "soh/include", "soh/src", "soh/assets", "soh/mods", "libultraship/include")]
    for config in ("CMake/soh-cvars.cmake", "CMake/lus-cvars.cmake"):
        for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT / config).read_text()):
            result.append(f'-D{key}="{value}"')
    return result


def baseline_functions(path):
    return functions(subprocess.check_output(
        ["git", "show", "bea6f438fc80e8fc92522bbba70e5938b034a617:" + path], cwd=ROOT, text=True))


def write_rod_functions(directory, negative=False):
    parts = []
    helper = functions((ROOT / "soh/mods/items/helpers/equip_helper.c").read_text())
    raw_helper = (ROOT / "soh/mods/items/helpers/equip_helper.c").read_text()
    end = raw_helper.index("} ItemUnequipSoundState;") + len("} ItemUnequipSoundState;")
    start = raw_helper.rfind("typedef struct {", 0, end)
    parts.append(raw_helper[start:end])
    parts += [helper[name] for name in ("ItemEquip_UnequipSoundState", "ItemEquip_ResetUnequipSound", "ItemEquip_BeginItemChangeSound", "ItemEquip_ClaimUnequipSound", "ItemEquip_PlayEquipSFXForAction", "ItemEquip_PlayUnequipSFXForAction", "ItemEquip_Update")]
    player = functions((ROOT / "soh/src/overlays/actors/ovl_player_actor/z_player.c").read_text())
    parts.append(player["Player_FinishItemChange"])
    parts.append(functions((ROOT / "soh/mods/items/logic/item_mitts.c").read_text())["Mitts_OnUnequip"])
    for element, prefix, spin in (("fire", "FireRod", "Fire"), ("ice", "IceRod", "Ice"),
                                  ("light", "LightRod", "Light")):
        source = (ROOT / f"soh/mods/items/logic/item_rod_{element}.c").read_text()
        end = source.index("// Shared-core descriptor") if element == "fire" else source.index("extern int Player_IsZTargeting")
        parts.append(source[source.index("static ItemEquipState"):end])
        methods = functions(source)
        if negative:
            name = "Player_Init" + prefix + "IA"
            methods[name] = baseline_functions(f"soh/mods/items/logic/item_rod_{element}.c")[name]
        names = [prefix + "_ExitFirstPerson", prefix + "_StopSpin" + spin]
        if element == "light":
            names.append(prefix + "_DestroySetColliders")
        names += [prefix + "_OnUnequip", "Player_Init" + prefix + "IA"]
        if prefix + "_PutAway" in methods:
            names.append(prefix + "_PutAway")
        parts += [methods[name] for name in names]
    (directory / "rod_stow_functions.inc").write_text("\n\n".join(parts))


def write_lantern_functions(directory, negative=False):
    path = "soh/mods/items/logic/item_lantern.c"
    methods = functions((ROOT / path).read_text())
    if negative:
        methods["Handle_Lantern"] = baseline_functions(path)["Handle_Lantern"]
    names = ["Lantern_IsInHand", "Lantern_UpdatePassive", "Player_InitLanternIA",
             "Lantern_PutAway", "Handle_Lantern"]
    (directory / "lantern_stow_functions.inc").write_text("\n\n".join(methods[name] for name in names))


if __name__ == "__main__":
    negative = "--baseline-negative" in sys.argv
    with tempfile.TemporaryDirectory(prefix="nei-lantern-rod-tests-") as temporary:
        directory = Path(temporary)
        write_rod_functions(directory, negative)
        write_lantern_functions(directory, negative)
        for name in ("rod_stow", "lantern_stow"):
            binary = str(directory / name)
            subprocess.run([os.environ.get("CC", "cc"), *flags(), "-I" + temporary,
                            str(ROOT / "tests/nei_item_stow" / (name + "_test.c")), "-lm", "-o", binary], check=True)
            result = subprocess.run([binary], capture_output=negative, text=True)
            if negative:
                if result.returncode == 0:
                    raise AssertionError(f"{name}: baseline unexpectedly passed")
                if "Assertion" not in result.stderr:
                    raise AssertionError(result.stderr)
                print(f"PASS: {name} rejects the original baseline behavior")
            else:
                result.check_returncode()
