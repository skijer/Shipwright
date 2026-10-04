"""Exercise production custom stow, player entry guards, and item input suppression.

Player function bodies are extracted with the existing diagnostic helper. For
Player_UseItem only the unrelated native item dispatch after accepted stow is
omitted; all preceding interceptors and acceptance guards execute unchanged.
"""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import os
import sys

from run_ballchain_tests import ROOT, flags

spec = importlib.util.spec_from_file_location(
    "pedestal", ROOT / "scripts/diagnostics/run_time_pedestal_tests.py")
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)


def player_functions(negative=False):
    path = "soh/src/overlays/actors/ovl_player_actor/z_player.c"
    raw = (subprocess.check_output(
        ["git", "show", "bea6f438fc80e8fc92522bbba70e5938b034a617:" + path], cwd=ROOT, text=True)
        if negative else (ROOT / path).read_text())
    source = extract.functions(raw)
    use = source["Player_UseItem"]
    end = use.index("            if ((play->bombchuBowlingStatus == 0)")
    body = use[:end] + "\n}\n}\n}\n"
    body += "\n".join(source[name] for name in
                      ("Player_PutAwayHeldItem", "Player_ActionHandler_Roll", "Player_InitItemAction"))
    hud = source["Player_UpdateInterface"]
    start = hud.index("else if (((this->heldItemAction >= PLAYER_IA_SWORD_MASTER)") + len("else ")
    candidate = extract.block_from(hud, start)
    start = hud.index("if (doAction != DO_ACTION_PUTAWAY)")
    end = hud.index("Interface_SetDoAction(play, doAction);", start)
    body += "\nstatic s32 Fixture_HudPutAway(PlayState* play, Player* this) {\n"
    body += "s32 doAction = DO_ACTION_NONE;\n" + candidate + "\n" + hud[start:end]
    body += "return doAction;\n}\n"
    return body


if __name__ == "__main__":
    negative = "--baseline-negative" in sys.argv
    with tempfile.TemporaryDirectory(prefix="nei-stow-tests-") as temporary:
        folder = Path(temporary)
        (folder / "stow_player.inc").write_text(player_functions(negative))
        binary = str(folder / "stow")
        subprocess.run([os.environ.get("CC", "cc"), *flags(), "-I" + str(folder),
                        "-Werror=implicit-function-declaration", "-ffunction-sections", "-fdata-sections",
                        str(ROOT / "tests/nei_item_stow/stow_contract_test.c"),
                        "-Wl,--gc-sections", "-o", binary], check=True)
        result = subprocess.run([binary], capture_output=negative, text=True)
        if negative:
            if result.returncode == 0 or "Assertion" not in result.stderr:
                raise AssertionError("Original player stow behavior was not rejected: " + result.stderr)
            print("PASS: player stow assertions reject the original published baseline")
        else:
            result.check_returncode()
