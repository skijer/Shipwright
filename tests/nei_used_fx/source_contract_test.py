"""Constrain this visual revision to the approved source diff, including RNG cadence."""
from pathlib import Path
import re, subprocess, sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts/diagnostics'))
from run_time_pedestal_tests import functions
BASE='cec63fce86b1f582f6e61ad6a98eca6c3cca784b'
def baseline(p):return subprocess.check_output(['git','show',f'{BASE}:{p}'],cwd=ROOT,text=True)
def tokens(s):return re.sub(r'\s+','',re.sub(r'//[^\n]*|/\*.*?\*/','',s,flags=re.S))
def gameplay(s):
 s=re.sub(r'    set->drawEpoch = \+\+s(?:Fire|Ice)DrawEpoch;\n', '', s)
 s=s.replace('EffectSsEnIce_Spawn(play, &sparkPos, 0.0f,', 'EffectSsEnIce_Spawn(play, &sparkPos, scale * 0.3f,')
 s=s.replace('&primColor, &envColor, 0, 10);', '&primColor, &envColor, 1000, 10);')
 s=re.sub(r'\s*(?:FX_DrawChargeAura|FX_DrawSpinFireCylinder|NeiUsedMagic_DrawCharge|NeiUsedMagic_DrawSpin)\([^;]*;', '',s)
 s=s.replace('RodCommon_PreserveChargeSparkCadence','FX_SpawnRodSwingParticles')
 s=re.sub(r'    // Use bright yellow.*?    if \(\(play->gameplayFrames % 3\)',
          '    if ((play->gameplayFrames % 3)',s,flags=re.S)
 s=re.sub(r'        // Sample the existing wave/beam state;[^\n]*\n        for \(s32 i = 0; i < .*?\n        }\n','',s,flags=re.S)
 return tokens(s)
for element in ('fire','ice','light'):
 path=f'soh/mods/items/logic/item_rod_{element}.c'
 old=functions(baseline(path))
 # Separately tested put-away audio fix is now part of this combined revision.
 # Normalize only the exact per-rod ownership wrappers, retaining gameplay checks.
 current=(ROOT/path).read_text()
 for kind in ('Equip','Unequip'):
  current=current.replace(f'ItemEquip_Play{kind}SFXForAction(play, p, PLAYER_IA_ROD_{element.upper()})',
                          f'ItemEquip_Play{kind}SFX(play, p)')
 new=functions(current)
 assert old.keys()==new.keys(),(element,'unexpected function additions/deletions')
 for name in old:
  assert gameplay(old[name])==gameplay(new[name]),(element,name,'nonvisual logic changed')
# The invisible original particle still occupies the same effect slot for the
# same life, running the real unmodified GSpk update (two RNG draws per tick).
old=functions((ROOT/'soh/mods/items/helpers/fx_helper.c').read_text())['FX_SpawnRodSwingParticles']
new=functions((ROOT/'soh/mods/items/logic/item_rod_common.c').read_text())['RodCommon_PreserveChargeSparkCadence']
old=old.replace('FX_SpawnRodSwingParticles','RodCommon_PreserveChargeSparkCadence').replace('&env, 100, 10','&env, 0, 0')
assert tokens(old)==tokens(new)
for path in ('soh/src/overlays/effects/ovl_Effect_Ss_G_Spk/z_eff_ss_g_spk.c',
             'soh/mods/items/logic/item_time_gate.c',
             'soh/src/overlays/effects/ovl_Effect_Ss_En_Ice/z_eff_ss_en_ice.c',
             'soh/src/overlays/effects/ovl_Effect_Ss_KiraKira/z_eff_ss_kirakira.c'):
 assert (ROOT/path).read_text()==baseline(path),path
# Accepted Fire dispatch retains the GI17 center-history path. Ice now has
# independently tested per-head reconstructed trails, without gameplay edits.
fire=(ROOT/'soh/mods/items/objects/object_firerod.c').read_text()
ice=subprocess.check_output(['git','show','c77c18587a976f6d6cb5c8f91f27593286469218:soh/mods/items/objects/object_icerod.c'],cwd=ROOT,text=True)
marker='    // Item-local USED meshes.'
expected=ice.split(marker,1)[1].replace('IceRod','FireRod').replace('iceRod','fireRod')
expected=expected.replace('DrawTrail(play, 1,','DrawTrail(play, 0,').replace('DrawProjectile(play, 1,','DrawProjectile(play, 0,')
visual=fire.split(marker,1)[1]
visual=re.sub(r'\s*FrameInterpolation_Record(?:Open|Close)Child\([^;]*;', '', visual)
visual=re.sub(r'\s*Vec3f\s+direction\s*=\s*RodVisual_Heading\([^;]*;', '', visual)
visual=visual.replace('&set->pos[p], &direction,', '&set->pos[p], &set->vel[p],')
assert tokens(visual)==tokens(expected)
for element in ('fire','ice'):
 current=(ROOT/f'soh/mods/items/logic/item_rod_{element}.c').read_text()
 for suffix in ('SingleProjectile','TripleProjectile'):
  init=functions(current)[f'{element.capitalize()}Rod_Init{suffix}']
  assert init.count(f'set->drawEpoch = ++s{element.capitalize()}DrawEpoch;')==1
path='soh/mods/items/objects/object_lightrod.c'
assert (ROOT/path).read_text().count('Rand_ZeroOne()')==baseline(path).count('Rand_ZeroOne()')==1
path='soh/soh/Enhancements/randomizer/NeiGiPresentation.cpp'
old=functions(baseline(path))['NeiGi_DrawMesh']
new=functions((ROOT/path).read_text())['NeiGi_DrawMeshMaterial']
new=re.sub(r'static void NeiGi_DrawMeshMaterial\(.*?\) \{',
           'void NeiGi_DrawMesh(PlayState* play, const NeiGi::Mesh& mesh, Kind orb) {',new,count=1,flags=re.S)
new=new.replace('(material ? 32 : 63)','63')
a=new.index('    if (material) {');b=new.index('    } else if (orb != Kind::Neutral) {',a)
new=new[:a]+'    if (orb != Kind::Neutral) {'+new[b+len('    } else if (orb != Kind::Neutral) {'):]
assert tokens(old)==tokens(new),'Existing GI batcher command path changed'
print('USED VFX source contract: rod gameplay, Time Gate state, local/remote shot dispatch, flight particle size alone suppressed; impact particles, gameplay and RNG cadence preserved')

assert 'gEffFire1DL' not in fire and 'objects/gameplay_keep' not in fire
print('PASS Fire projectile draw is independent of shared gameplay_keep fire material')
