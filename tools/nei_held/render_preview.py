"""Inspect real held geometry in its attachment frame (not a game capture)."""
import argparse
import json
import math
from pathlib import Path
import subprocess
import sys
import numpy as np
from PIL import ImageDraw

ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT.parent/'nei_gi/runtime_preview'))
import render as r
r.ROOT=ROOT

PANELS=[
 ('Fire rod',['fire_rod'],'Grip + approved fire energy'),
 ('Ice rod',['ice_rod'],'Grip + moving frost; no fixed shards'),
 ('Light rod',['light_rod'],'Grip + preserved light corona'),
 ('Switch hook',['switch_hook'],'Complete docked assembly'),
 ('Whip',['whip'],'Equipped coil; separate parts in use'),
 ('Beetle',['beetle_body','beetle_wings'],'Body and animated wings'),
 ('Ball and chain',['ball'],'Active spiked ball; physics chain separate'),
 ('Shovel',['shovel'],'Blade fitted to two-hand shaft axis'),
 ('Gust jar',['gust_jar','gust_jar_band'],'Body + charge-sensitive wind band'),
 ('Deku leaf',['deku_leaf'],'Canopy; existing size states retained'),
 ('Mogma mitts',['mitt_left','mitt_right'],'One glove per hand'),
 ('Time gate',['time_gate'],'Held engraving; portal unchanged'),
]


def bounds(slugs):
    result=[]
    for slug in slugs:
        m=json.loads((ROOT/'CHECKPOINTS'/slug/'checkpoint.json').read_text())
        result.append(np.array(m['bounds_author'])*16*m['matrix_scale']*m['draw_scale'])
    return np.min([p[0] for p in result],axis=0),np.max([p[1] for p in result],axis=0)


def pivot():
    r.Disable(0x0B44);r.Disable(0x0DE1)
    r.Color(1,.75,.25,1)
    r.Begin(0x0001)
    for a in range(3):
        p=np.zeros(3);p[a]=.9;r.Vertex(*(-p));r.Vertex(*p)
    r.End()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('effects',type=Path)
    p.add_argument('output',type=Path)
    p.add_argument('--frames',type=int,default=180)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    fx=r.effects(a.effects);orb=r.orbs(a.effects)
    loaded={slug:r.model(slug, (24/255,156/255,233/255) if slug=='gust_jar_band' else (1,1,1))
            for _,slugs,_ in PANELS for slug in slugs}
    hot_band=r.model('gust_jar_band',(225/255,16/255,16/255))
    out=a.output/'NEI_Held_POC1_models.mp4'
    proc=subprocess.Popen(['ffmpeg','-loglevel','error','-y','-f','rawvideo','-vcodec','rawvideo','-pix_fmt','rgb24','-s',f'{r.W}x{r.H}','-r','20','-i','-','-an','-c:v','libx264','-preset','fast','-crf','18','-pix_fmt','yuv420p','-movflags','+faststart',str(out)],stdin=subprocess.PIPE)
    for frame in range(a.frames):
        r.DepthMask(1);r.Clear(0x4000|0x0100)
        for i,(name,slugs,note) in enumerate(PANELS):
            lo,hi=bounds(slugs)
            center=(lo+hi)/2
            radius=np.linalg.norm(hi-lo)*.58
            if i==10:radius*=1.25
            x=(i%3)*400;y=50+(3-i//3)*218
            r.pose(x,y,400,170,-radius,radius)
            r.Translate(*(-center))
            for slug in slugs:
                r.Push()
                if slug.startswith('mitt_'):r.Translate(-3.5 if slug=='mitt_left' else 3.5,0,0)
                if slug=='beetle_wings':r.Scale(1,1 if frame%2==0 else .3,1)
                passes=hot_band if slug=='gust_jar_band' and frame>=90 else loaded[slug]
                r.DepthMask(1);r.CallList(passes[0])
                r.DepthMask(0);r.CallList(passes[1]);r.Pop()
            if i<3:
                meta=json.loads((ROOT/'CHECKPOINTS'/slugs[0]/'checkpoint.json').read_text())
                r.Push();r.Translate(*(np.array(meta['markers']['tip_native'])*.05));r.Rotate(32,0,0,1)
                r.Scale(.2/.78,.2/.78,.2/.78)
                r.draworb(orb[frame][i]);r.drawfx(fx[frame][i]);r.Pop()
            r.DepthMask(0);pivot()
        im=r.pixels();d=ImageDraw.Draw(im)
        d.rectangle((0,0,r.W,70),fill='#111923')
        d.text((25,13),'Held models — attachment and component review',font=r.font(27),fill='#f0f5ff')
        d.text((25,49),'Actual exported geometry • each panel fitted independently • gold cross = attachment origin',font=r.font(15),fill='#aabacc')
        for i,(name,slugs,note) in enumerate(PANELS):
            x=(i%3)*400+20;y=78+(i//3)*218
            d.text((x,y),name,font=r.font(22),fill='#eef3fd')
            d.text((x,y+29),note,font=r.font(12),fill='#aabacc')
        d.text((25,969),'Offline review • hand poses, animation clearance, camera and lighting still need an in-game pass.',font=r.font(16),fill='#92a4ba')
        if frame==36 or a.frames==1:im.save(a.output/'NEI_Held_POC1_models.png')
        proc.stdin.write(im.tobytes())
    proc.stdin.close();assert proc.wait()==0
    print(out)


if __name__=='__main__':main()
