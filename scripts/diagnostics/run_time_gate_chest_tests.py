"""Exercise production chest initialization, offers, collection and message routing.

Graphics, collision and item-table services are engine boundaries. Function bodies
under test are copied verbatim from production into a temporary translation unit.
No scene archive is edited or required by these focused regression tests.
"""
import pathlib
import re
import subprocess
import tempfile

from run_time_pedestal_tests import functions

ROOT = pathlib.Path(__file__).resolve().parents[2]


def first_call(source, name):
    start = source.index(name + "(")
    pos = start + len(name) + 1
    depth = 1
    while depth:
        depth += (source[pos] == "(") - (source[pos] == ")")
        pos += 1
    return source[start:pos] + ";"


def main():
    chest_source = (ROOT / "soh/src/overlays/actors/ovl_En_Box/z_en_box.c").read_text()
    chest = functions(chest_source)
    actor = functions((ROOT / "soh/src/code/z_actor.c").read_text())
    player = functions((ROOT / "soh/src/overlays/actors/ovl_player_actor/z_player.c").read_text())
    parameter_source = (ROOT / "soh/src/code/z_parameter.c").read_text()
    inventory_source = (ROOT / "soh/src/code/z_inventory.c").read_text()
    kaleido_source = (ROOT / "soh/src/overlays/misc/ovl_kaleido_scope/z_kaleido_scope_PAL.c").read_text()
    messages = functions((ROOT / "soh/soh/Enhancements/randomizer/Messages/ItemMessages.cpp").read_text())
    with tempfile.TemporaryDirectory(prefix="time-gate-chest-") as folder:
        build = pathlib.Path(folder)
        preamble = "\n".join(re.findall(r"^#define ENBOX_MOVE_.*$", chest_source, re.M))
        for pattern in (r"typedef enum \{[^}]*\} EnBoxStateUnk1FB;",
                        r"static AnimationHeader\* sAnimations\[4\] = \{.*?\};",
                        r"static InitChainEntry sInitChain\[\] = \{.*?\};"):
            preamble += "\n" + re.search(pattern, chest_source, re.S)[0]
        preamble += "\n" + re.search(r"static s16 sExtraItemBases\[\] = \{.*?\};", parameter_source, re.S)[0]
        for name in ("gItemSlots", "gBitFlags", "gEquipMasks", "gEquipShifts", "gUpgradeMasks", "gUpgradeShifts"):
            preamble += "\n" + re.search(r"u\d+ " + name + r"\[\] = \{.*?\};", inventory_source, re.S)[0]
        # The real receipt branch checks equipment age requirements through the
        # production Kaleido macro; keep its vanilla item table intact too.
        preamble += "\n" + re.search(r"u8 gItemAgeReqs\[ITEM_NONE\] = \{.*?\};", kaleido_source, re.S)[0]
        selected = [body for name, body in chest.items() if name in {
            "EnBox_IsTimeGateChest", "EnBox_SetupAction", "EnBox_Init", "EnBox_WaitOpen",
            "EnBox_AppearOnSwitchFlag", "EnBox_AppearInit", "EnBox_AppearAnimation"}]
        selected += [actor[name] for name in (
            "Flags_GetTreasure", "Flags_SetTreasure", "GiveItemEntryFromActor",
            "GiveItemEntryFromActorWithFixedRange", "Actor_OfferGetItem", "Actor_OfferGetItemNearby")]
        selected += [player["func_8083A434"]]
        selected += [functions(parameter_source)["Item_CheckObtainability"]]
        # Keep the real receipt branch and its one-shot guard. The remaining
        # function handles subsequent textbox/equip input, outside this probe.
        receive = player["func_8084DFF4"]
        receive = receive[:receive.index(" else if (equipNow")]
        selected += [receive.replace("func_8084DFF4", "Fixture_ReceiveItem", 1) + "\nreturn 0;\n}"]
        prototypes = "\n".join(body[:body.index("{")] + ";" for body in selected)
        (build / "time_gate_chest_production.inc").write_text(
            preamble + "\n" + prototypes + "\n" + "\n".join(selected))
        message_body = messages["BuildItemMessage"]
        registration = first_call(messages["RegisterItemMessages"], "COND_ID_HOOK")
        (build / "time_gate_message_production.inc").write_text(
            message_body + "\nvoid Fixture_RegisterItemMessage() {\n" + registration + "\n}\n")
        includes = ["-Isoh/include", "-Isoh/src", "-Isoh/assets", "-Isoh", "-Ilibultraship/include", "-I" + str(build)]
        common = ["-O1", "-g", "-ffunction-sections", "-fdata-sections", "-DLOG_LEVEL_GAME_PRINTS=0",
                  '-DCVAR_PREFIX_ENHANCEMENT="gEnhancements"', '-DCVAR_PREFIX_CHEAT="gCheats"',
                  *includes, "-Wl,--gc-sections"]
        failures = 0
        for compiler, standard, source in (("cc", "gnu11", "time_gate_chest_test.c"),
                                           ("c++", "c++20", "time_gate_message_test.cpp")):
            binary = build / source.split(".")[0]
            warnings = (["-Wno-incompatible-pointer-types", "-Wno-discarded-array-qualifiers",
                         "-fsanitize=bounds", "-fno-sanitize-recover=bounds"] if compiler == "cc" else [])
            command = [compiler, "-std=" + standard, *common, *warnings,
                       str(ROOT / "soh/tests" / source), "-lm", "-o", str(binary)]
            result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
            if result.returncode:
                print(result.stderr)
                raise SystemExit(result.returncode)
            failures += subprocess.run([str(binary)]).returncode != 0
        if failures:
            raise SystemExit(f"{failures} Time Gate test group(s) failed")


if __name__ == "__main__":
    main()
