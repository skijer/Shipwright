"""Approved GI parts fitted to existing gameplay frames, without GI pose/spin."""
import copy
import math
import sys
from pathlib import Path
import numpy as np

GI_SOURCE = Path(__file__).resolve().parents[2] / 'nei_gi' / 'SOURCE'
sys.path.insert(0, str(GI_SOURCE))
import models
from meshkit import Model, rotation


def component(source, slug, select=lambda p: True, factor=1., draw_scale=1.):
    m = copy.deepcopy(source)
    m.slug = slug
    m.name = source.name + ' / gameplay ' + slug
    m.prefix = 'objects/nei_held_redesign/' + slug + '/'
    m.entry = m.prefix + 'gi_dl'
    m.parts = [p for p in m.parts if select(p)]
    used = {p['mat'] for p in m.parts}
    m.materials = {k:v for k,v in m.materials.items() if k in used}
    m.native_scale = round(factor / 16 * 65536) / 65536
    m.draw_scale = draw_scale
    m.effective_scale = m.native_scale * 16 * draw_scale
    m.markers = {}
    return m


def rod(slug):
    m = component(models.rod(slug), slug, factor=4, draw_scale=.05)
    aim = rotation('z', 32)
    # The existing rod frames have a diagonal shaft. Grip center, not GI
    # center, is attached to the hand; all three share a 22-unit envelope.
    m.transform(aim @ rotation('z', 18), offset=aim @ [0,25,0])
    tip = aim @ [0,68 if slug == 'ice_rod' else 66,0]
    m.markers = {'grip_native':[0,0,0], 'tip_native':(tip*4).tolist()}
    m.notes = ['Approved rod with GI lean removed; grip author Y=-25 at hand; shaft native 32 degrees; factor 4. No baked ice splinters.']
    return m


def shovel():
    m = component(models.shovel(), 'shovel', factor=10, draw_scale=.06)
    aim = rotation('z', -80) @ rotation('y', 90)
    m.transform(aim @ rotation('z', 24), offset=aim @ [0,-20,0])
    m.markers = {'grip_native':[0,0,0]}
    m.notes = ['Shaft lies along existing hand-to-hand frame. Blade plane follows native spade; midpoint grip at author Y=20.']
    return m


def ball():
    m = component(models.ball_chain(), 'ball',
                  lambda p:not p['name'].startswith('Forged rectangular chain link'), 4, .1)
    m.transform(offset=[0,5,3])
    m.markers = {'physics_center_native':[0,0,0]}
    m.notes = ['Six approved pointed spikes and anchor retained; display coil removed; center at authoritative ball position. Existing held .06 / thrown .1 scales retained.']
    return m


def leaf():
    m = component(models.leaf(), 'deku_leaf', factor=1.85, draw_scale=.16)
    m.notes = ['Matches the original 160-unit canopy width; existing hold/attack/glide scaling and canopy offsets retained.']
    return m


def beetle(wings=False):
    wing_names = {'Rigid engraved wing blade','Wing scroll inlay','Wing tip inset'}
    m = component(models.beetle(), 'beetle_wings' if wings else 'beetle_body',
                  lambda p:(p['name'] in wing_names) == wings, 2.6, .06)
    # Approved GI tips +Z; gameplay's existing extra Y+90 expects -X.
    m.transform(rotation('y', -90) @ rotation('x', -50))
    m.notes = ['Undo GI display tilt; mechanical pincers point along native -X. Wing blades/inlays kept separate from stationary hinge axles. Existing flight/aim and wing animation retained.']
    return m


def mitt(left):
    source = models.mitts()
    count = len(source.parts)//2
    source.parts = source.parts[:count] if left else source.parts[count:]
    sign = -1 if left else 1
    m = component(source, 'mitt_left' if left else 'mitt_right', factor=1, draw_scale=.13)
    # Undo the display pair's spacing and splay before putting each glove on
    # its own hand. The palm is the hand pivot; cuffs extend toward forearms.
    m.transform(offset=[-sign*25,0,0])
    m.transform(rotation('z', sign*12))
    m.markers = {'palm_author':[0,0,0]}
    m.notes = ['Single approved glove, palm at hand origin; +Y points along fingers. Paired drawing follows each forearm separately.']
    return m


def gate():
    m = component(models.gate(), 'time_gate', factor=16, draw_scale=.008)
    m.notes = ['Approved engraved gear; 12.8-unit nominal diameter during existing casting pose. Portal is unchanged.']
    return m


def jar(band=False):
    m = component(models.jar(), 'gust_jar_band' if band else 'gust_jar',
                  lambda p:(p['name']=='Blue belly band') == band, 1, .22)
    if band:
        mat=m.materials['wind_band']
        tex=mat['tex'].copy()
        # Neutral porcelain relief receives the original SUCK/BLOW color.
        grey=np.max(tex[:,:,:3],axis=2)
        tex[:,:,:3]=grey[:,:,None]
        mat['tex']=tex
    m.notes = ['Author Y is the open mouth; native 90-degree aim rotation retained. Midpoint of paired loop handles at hand center. Separate wind band preserves direction/charge feedback.']
    return m


BUILDERS = {
    'fire_rod':lambda:rod('fire_rod'), 'ice_rod':lambda:rod('ice_rod'),
    'light_rod':lambda:rod('light_rod'), 'shovel':shovel, 'ball':ball,
    'deku_leaf':leaf, 'beetle_body':lambda:beetle(False),
    'beetle_wings':lambda:beetle(True), 'mitt_left':lambda:mitt(True),
    'mitt_right':lambda:mitt(False), 'time_gate':gate,
    'gust_jar':lambda:jar(False), 'gust_jar_band':lambda:jar(True),
}
