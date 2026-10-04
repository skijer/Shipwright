"""Exercise actual final native/PAK hand override order for the held lantern."""
from pathlib import Path
import subprocess
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/nei_item_stow'))
from run_ballchain_tests import flags
sys.path.insert(0,str(ROOT/'scripts/diagnostics'))
from run_time_pedestal_tests import functions
f=functions((ROOT/'soh/src/code/z_player_lib.c').read_text())
tail=f['Player_OverrideLimbDrawGameplayDefault']
tail=tail[tail.index('    GameInteractor_Should(VB_PLAYER_OVERRIDE_LIMB_DRAW'):]
helpers=f.get('Player_ApplyLanternGrip','')
with tempfile.TemporaryDirectory(prefix='lantern-grip-') as td:
    Path(td,'grip_tail.inc').write_text(helpers+'\nstatic s32 Fixture_Apply(PlayState* play, Player* this, s32 limbIndex, Gfx** dList) {\nvoid* thisx=this; Vec3s r={0}; Vec3s* rot=&r;\n'+tail)
    binary=str(Path(td,'grip'))
    subprocess.run(['cc',*flags(),'-I'+td,str(ROOT/'tests/nei_lantern_grip/grip_test.c'),'-lm','-o',binary],check=True)
    subprocess.run([binary],check=True)
