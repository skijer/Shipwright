#!/usr/bin/env python3
"""Exercise production stat callbacks and pickup effects without a ROM.

Only the seed-option store, hook dispatcher, RNG, and graphics/audio boundaries
are fixtures. The stat calculations and item receipt code are copied verbatim.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

from run_time_pedestal_tests import functions

ROOT = Path(__file__).resolve().parents[2]
RANDO = ROOT / "soh/soh/Enhancements/randomizer"


def main():
    header = (RANDO / "randostatupgrade.h").read_text()
    helpers = header[header.index("static constexpr"):header.index('extern "C" {')]
    source = functions((RANDO / "randostatupgrade.cpp").read_text().replace('extern "C" ', ''))
    selected = [source["RegisterRandoStatUpgradeHooks"]]
    selected += [source[name] for name in (
        "IsCrawlStatActive", "IsClimbStatActive", "IsPushStatActive",
        "GetCrawlStatValue", "GetClimbStatValue", "GetPushStatValue")]
    give = functions((RANDO / "randomizer.cpp").read_text().replace('extern "C" ', ''))["Randomizer_Item_Give"]
    stat_give = give[give.index("        case RG_QUARTER_HEART:"):]
    stat_give = stat_give[:stat_give.index("        default:")]
    receipt = "void GiveStat(PlayState* play, RandomizerGet item) { switch(item) {\n"
    receipt += stat_give + "default: std::abort(); } }\n"
    logic_source = (RANDO / "logic.cpp").read_text()
    magic = logic_source[logic_source.index("        case RG_MAGIC_SINGLE: {"):]
    magic = magic[:magic.index("            // Custom Item")]
    receipt += "SaveContext* GetSaveContext() { return &gSaveContext; }\n"
    receipt += "bool HasLogicalMagic() { switch(RG_MAGIC_SINGLE) {\n" + magic + "default: return false; } }\n"
    new_game = functions((RANDO / "savefile.cpp").read_text().replace('extern "C" ', ''))["Randomizer_InitSaveFile"]
    reset = new_game[new_game.index("    // Starts pending ice traps"):new_game.index("    SetStartingItems();")]
    receipt += "void ResetNewGameStats() {\n" + reset + "}\n"
    logic = logic_source.split("void Logic::ApplyItemEffect", 1)[1]
    health = logic[logic.index("                case RG_HEART_CONTAINER:"):logic.index("                case RG_BOOMERANG:")]
    receipt += "void ApplyHealthStat(SaveContext* mSaveContext, RandomizerGet item, bool state) { switch(item) {\n"
    receipt += health + "default: break; } }\n"
    tracker = functions((RANDO / "randomizer_item_tracker.cpp").read_text())
    slots = re.search(r"u8 gItemSlots\[\] = \{.*?\};", (ROOT / "soh/src/code/z_inventory.c").read_text(), re.S)[0]
    receipt += slots + "\n"
    for name in ("DrawItem", "DrawItemCount"):
        declaration = re.search(r"uint32_t actualItemId =\s*.*?;", tracker[name], re.S)[0]
        receipt += f"uint32_t {name}Inventory(ItemTrackerItem item) {{ {declaration} return actualItemId; }}\n"
    push_cases = [
        ("ovl_Bg_Spot15_Rrbox/z_bg_spot15_rrbox.c", "func_808B4194", "unk_174", "float", "CrateAcceleration"),
        ("ovl_Bg_Spot15_Rrbox/z_bg_spot15_rrbox.c", "func_808B4194", "unk_168", "short", "CrateDelay"),
        ("ovl_Obj_Oshihiki/z_obj_oshihiki.c", "ObjOshihiki_Push", "timer", "short", "BlockDelay"),
        ("ovl_Bg_Haka_Gate/z_bg_haka_gate.c", "BgHakaGate_StatueTurn", "vTimer", "short", "StatueDelay"),
        ("ovl_Bg_Hidan_Rock/z_bg_hidan_rock.c", "func_8088B268", "timer", "short", "RockDelay"),
        ("ovl_Bg_Po_Event/z_bg_po_event.c", "BgPoEvent_BlockPush", "direction", "short", "PuzzleDelay"),
    ]
    for path, function, field, type_, name in push_cases:
        body = functions((ROOT / "soh/src/overlays/actors" / path).read_text())[function]
        statement = next(s for s in re.findall(rf"this->{field} =\s*.*?;", body, re.S) if "FasterBlockPush" in s)
        receipt += f"{type_} {name}() {{ struct {{ {type_} {field}; }} actor{{}}; "
        receipt += statement.replace("this->", "actor.") + f" return actor.{field}; }}\n"
    collision = functions((ROOT / "soh/src/code/z_collision_check.c").read_text())
    receipt += collision["CollisionCheck_ApplyDamage"] + "\n"
    flags = ["-std=c++20", "-O1", "-g", "-DF3DEX_GBI_2", "-DLOG_LEVEL_GAME_PRINTS=0",
             "-Ilibultraship/include", "-Isoh/include", "-Isoh/src", "-Isoh/assets", "-Isoh"]
    for path in ("CMake/soh-cvars.cmake", "CMake/lus-cvars.cmake"):
        for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT / path).read_text()):
            flags.append(f'-D{key}="{value}"')
    with tempfile.TemporaryDirectory(prefix="stat-upgrades-") as directory:
        folder = Path(directory)
        (folder / "stat_upgrade_production.inc").write_text(
            helpers + "\n" + "\n".join(selected) + "\n" + receipt)
        binary = folder / "stat_upgrade_test"
        subprocess.run([os.environ.get("CXX", "c++"), *flags, "-I" + directory,
                        "-fsanitize=undefined", "-fno-sanitize-recover=undefined",
                        "soh/tests/stat_upgrade_test.cpp", "-o", str(binary)], cwd=ROOT, check=True)
        subprocess.run([str(binary)], check=True)
        conversions = (ROOT / "soh/soh/Network/Anchor/JsonConversions.hpp").read_text()
        start = conversions.index("inline void to_json(json& j, const ShipRandomizerSaveContextData&")
        end = conversions.index("inline void to_json(json& j, const ShipQuestSpecificSaveContextData&", start)
        (folder / "stat_upgrade_json_production.inc").write_text(conversions[start:end])
        json_binary = folder / "stat_upgrade_json_test"
        subprocess.run([os.environ.get("CXX", "c++"), *flags, "-I" + directory,
                        "soh/tests/stat_upgrade_json_test.cpp", "-o", str(json_binary)], cwd=ROOT, check=True)
        subprocess.run([str(json_binary)], check=True)


if __name__ == "__main__":
    main()
