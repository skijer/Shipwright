"""Used Spinner and hand-held Somaria derived from the missing GI source art.

The accepted held builders are untouched. World dimensions are explicit here;
the wrapper supplies the player/hand frame, without GI lean or turntable spin.
"""
import importlib.util
from pathlib import Path
import sys
import numpy as np

GI_SOURCE=Path(__file__).resolve().parents[2]/'nei_gi/SOURCE'
sys.path.insert(0,str(GI_SOURCE))
from meshkit import rotation, unit, Q
_spec=importlib.util.spec_from_file_location('nei_gi_completion',GI_SOURCE/'completion.py')
source=importlib.util.module_from_spec(_spec);_spec.loader.exec_module(source)


def _used(m,world_scale,draw_scale):
    m.prefix='objects/nei_held_redesign/'+m.slug+'/'
    m.entry=m.prefix+'gi_dl'
    m.native_scale=round(world_scale/draw_scale/16*65536)/65536
    m.draw_scale=draw_scale
    m.effective_scale=m.native_scale*16*draw_scale
    return m


def spinner():
    m=_used(source.spinner(),.5,.2)
    m.transform(offset=[0,-13.125,0])
    # Exact full-clip boot minima are -.5272 and -.218 world relative to root.
    # Put the flat deck just below the lower sole, and shorten only the underside
    # to leave three world units of floor clearance at the unchanged hover17.
    pivot=-15.125
    ratio=(-28-pivot)/(-32.125-pivot)
    for p in m.parts:
        mask=p['p'][:,1]<pivot
        p['p'][mask,1]=np.rint((pivot+(p['p'][mask,1]-pivot)*ratio)*Q)/Q
        p['n'][mask,1]/=ratio
        p['n']=unit(p['n'])
    m.markers=dict(deck_author=[0,-1.125,0],deck_world_y=-.5625,world_scale=.5,
                   deck_radius_world=19.65,player_hover_world=17,lowest_world_y=-14,
                   radius_world=24.4,rotation_binang_per_frame=0x800)
    m.notes=['Same supplied-icon Spinner as GI; lower hull compressed only for floor clearance.',
             'Wrapper T(player) RY(BINANG_TO_RAD(frames*0x800)) S(.2); resource matrix .15625.',
             'Usable flat deck radius19.65 covers exact native-stance foot radius18.8805.',
             'Decky=-.5625 is below rightsole -.5272..-.4704; leftsole -.218..-.1561 stays slightly raised by native stance.',
             'Radius24.4; underside y=-14 at normal player floor+17 leaves three world units clearance.',
             'Hover, attack, homing, damage and collision behavior are unchanged.']
    return m


def cane_of_somaria():
    m=_used(source.cane_of_somaria(False),.35,1)
    m.transform(offset=[0,22.5,0])
    m.markers=dict(grip_author=[0,0,0],tip_author=[-11.5,59.2,0],
                   canonical_shaft_axis=[0,1,0],world_scale=m.effective_scale,
                   hand='right',pose_rotations_degrees=[0,0,-90],
                   grip_native_child=[0,216.22,-4.5],grip_native_adult=[0,328,77])
    m.notes=['Canonical upright source with grip-origin zero, fitted by the captured right palm pose; no GI tilt.',
             'Captured hand matrix is divided by player scale; resource conversion supplies .35 world/author.',
             'Only CANE_TYPE_SOMARIA is replaced. Other cane modes retain legacy geometry, tints and VFX.',
             'Rotate canonical +Y onto right-wrist +X; native child/adult closed-fist grip offsets are [0,216.22,-4.5]/[0,328,77].',
             'Universal child socket verified against native child and YoungDin meshes; custom mesh grip differs by only .187 world units.']
    return m


BUILDERS={'spinner':spinner,'cane_of_somaria':cane_of_somaria}
