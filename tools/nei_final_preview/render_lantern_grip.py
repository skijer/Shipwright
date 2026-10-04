"""Actual-source open/closed left-hand closeups at the unchanged lantern placement."""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import ImageDraw, ImageFont
from player_animation import ROOT, NativeAnimations, glb_meshes, translation, scale, sha256
from player_render import Renderer
from render_whip_aim import PreviewPlayer

FONT='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--native',type=Path,required=True); p.add_argument('--din',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True); a=p.parse_args(); a.output.mkdir(parents=True,exist_ok=True)
    native=NativeAnimations(a.native); idle=native.load('link_normal_wait_free')
    archives=[(PreviewPlayer(a.native),'Native child'),(PreviewPlayer(a.din),'Young Din POC4'),
              (PreviewPlayer(a.native,age='adult'),'Native adult')]
    lantern_path=ROOT/'tools/nei_gi/CHECKPOINTS/lantern/lantern.glb'
    lantern=glb_meshes(lantern_path)
    r=Renderer(1440,1190);r.clear();matrices=[]
    for row,(archive,label) in enumerate(archives):
        world=archive.world(idle[0]); hand=world[15,:3,3]
        # Exact NeiLantern_DrawHeld matrix: actor yaw0, HeldScale=7/52,
        # -GripY52, and ModelScale .025/.5 applied to native GI GLB vertices.
        matrix=translation(*hand)@scale(7/52)@translation(0,-52,0)@scale(.05)
        matrices.append({'player':label,'world_matrix':matrix.tolist(),'wrist':hand.tolist()})
        for col,closed in enumerate((False,True)):
            meshes=archive.geometry(hands='closed' if closed else 'open',include_sheath=False,only_limb=15)
            meshes+=archive.geometry(include_sheath=False,only_limb=14)
            r.view(col*720,80+(2-row)*335,720,325,yaw=-25,pitch=8,
                   center=(hand[0],hand[1]-3,hand[2]),span=21)
            r.draw(r.prepare(meshes,world)+r.prepare(lantern,matrix[None]))
    image=r.image();d=ImageDraw.Draw(image)
    d.rectangle((0,0,1440,90),fill='#111923')
    d.text((22,12),'Lantern grip • same placement, selected closed left fist',font=ImageFont.truetype(FONT,28),fill='#ecf3fe')
    d.text((22,54),'Before: open palm',font=ImageFont.truetype(FONT,22),fill='#e8c374')
    d.text((742,54),'Candidate: clasped hand',font=ImageFont.truetype(FONT,22),fill='#a8e8d2')
    for row,(_,label) in enumerate(archives):
        for col in range(2): d.text((col*720+22,100+row*335),label,font=ImageFont.truetype(FONT,22),fill='#d7e5f5')
    d.rectangle((0,1110,1440,1190),fill='#111923')
    d.text((22,1122),'Actual idle pose, forearm/hand meshes and lantern GLB. The wrist, handle, scale and hanging position are identical.',font=ImageFont.truetype(FONT,18),fill='#d5e1ef')
    d.text((22,1153),'Offline closeup; native/Alt/PAK selection is fixture-tested. Live PAK appearance and runtime acceptance remain untested.',font=ImageFont.truetype(FONT,17),fill='#a2b5cd')
    path=a.output/'Lantern_Closed_Grip_Source_Preview.png';image.save(path)
    (a.output/'Lantern_Closed_Grip_Manifest.json').write_text(json.dumps({**native.manifest(),
        'din_archive_sha256':sha256(a.din),'lantern_glb_sha256':sha256(lantern_path),'script_sha256':sha256(__file__),
        'unchanged_matrices':matrices,'offline_only':True,'scope':'Hand display-list selection only; position unchanged',
        'geometry':'Native child, Young Din POC4, native adult; isolated actual left forearm and hand'},indent=2)+'\n')
    print(path)

if __name__=='__main__':main()
