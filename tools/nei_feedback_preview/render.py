"""Production effect geometry/materials in offline OpenGL; no synthetic bloom."""
import sys,struct,subprocess
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'nei_used_fx'))
import render as r
W,H=r.W,r.H
TITLE=['FIRE / accepted projectile','ICE / three shots, three wakes','ICE / following wake detail',
       'FIRE / released charge','ICE / released charge']
SUB=['Appearance preserved','Native spread and speed; camera follows','Mature scale; continuous inspection fixture',
     'Curling heat • two scrolling layers','Rolling frost • two scrolling layers']
raw=Path(sys.argv[1]).read_bytes();at=0;frames=[]
dtype=np.dtype([('p','<f4',3),('rgba','u1',4),('uv','<f4',2)])
for f in range(180):
 panels=[]
 for panel in range(5):
  n=struct.unpack_from('<I',raw,at)[0];at+=4;layers=[]
  for _ in range(n):
   mat,count=struct.unpack_from('<II',raw,at);at+=8
   layers.append((mat,np.frombuffer(raw,dtype=dtype,count=count,offset=at)));at+=count*24
  panels.append(layers)
 frames.append(panels)
assert at==len(raw)
def panelrect(panel):
 return (panel*500,95,500,375) if panel<3 else ((panel-3)*750,485,750,480)
def draw(frame,poster=False):
 r.Clear(0x4000|0x100)
 for panel,layers in enumerate(frames[frame]):
  # The still compares a mature volley and a near-full wall, labelled below.
  if poster and panel<3:layers=frames[9][panel]
  left,top,cw,ch=panelrect(panel)
  r.Viewport(left,H-top-ch,cw,ch);r.MatrixMode(0x1701);r.LoadIdentity()
  if panel<2:low,high=-75,145;center=-5;step=20
  elif panel==2:low,high=-48,94;center=-10;step=20
  else:low,high=-280,430;center=0;step=100
  half=(high-low)*cw/ch/2;r.Ortho(center-half,center+half,low,high,-800,800)
  r.MatrixMode(0x1700);r.LoadIdentity();r.Rotate(24,1,0,0);r.Rotate(-18,0,1,0)
  r.Disable(0x0B71);r.Disable(0x0DE1);r.DepthMask(0);r.Color(.11,.14,.18,1);r.Begin(0x0001)
  extent=600 if panel>=3 else 160
  # Move the projectile grid at native velocity under the following camera.
  shift=-((frame%45)*15%20) if panel<3 else 0
  for n in range(-extent,extent+1,step):
   r.Vertex(-extent+shift,-2,n);r.Vertex(extent+shift,-2,n)
   r.Vertex(n+shift,-2,-extent);r.Vertex(n+shift,-2,extent)
  r.End()
  r.Color(.40,.44,.50,1);r.Begin(0x0001)
  if panel>=3:r.Vertex(0,0,0);r.Vertex(0,56,0);r.Vertex(-10,56,0);r.Vertex(10,56,0)
  r.End()
  for mat,data in layers:r.drawmesh(data,mat)
 pix=np.zeros((H,W,4),np.uint8);r.ReadPixels(0,0,W,H,0x1908,0x1401,pix.ctypes.data)
 im=Image.fromarray(pix[::-1].copy()).convert('RGB');d=ImageDraw.Draw(im)
 d.rectangle((0,0,W,89),fill='#101821');d.text((24,15),'NEI  /  ROD REVISION • REVIEW 03',font=r.font(27),fill='#f1f4f8')
 d.text((24,53),'Production meshes + materials • offline preview • standard alpha • not captured gameplay • not pushed',font=r.font(17),fill='#a9b9ca')
 for panel in range(5):
  left,top,cw,ch=panelrect(panel);x=left+18;y=top+4
  d.text((x,y),TITLE[panel],font=r.font(20),fill='#e6eff8');d.text((x,y+29),SUB[panel],font=r.font(13),fill='#a4b5c8')
  if panel in (1,2,4):d.line((left,top,left,top+ch),fill='#283645')
  if panel>=3:
   tick=int(frame%45)-10
   timing=f'REAL TIME  ·  {(tick+1)*.05:.2f} / 0.80 s' if 0<=tick<16 else 'REAL TIME  ·  next release'
   d.text((x,top+ch-29),timing,font=r.font(16),fill='#b6cddd')
 d.line((0,475,W,475),fill='#283645')
 d.rectangle((0,H-29,W,H),fill='#101821')
 footer='Still: mature volley and near-full release shown together.' if poster else '20 updates/s • release ends after 16 active updates • no slow motion. Upper-right wake is held for inspection.'
 d.text((20,H-24),footer+'  Grids: 20 / 100 units.',font=r.font(13),fill='#a4b5c8')
 return im
out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
draw(23,True).save(out/'rod_revision_03.png')
if '--still' not in sys.argv:
 proc=subprocess.Popen(['ffmpeg','-y','-v','error','-f','rawvideo','-pix_fmt','rgb24','-s',f'{W}x{H}','-r','20','-i','-',
  '-an','-c:v','libx264','-crf','18','-pix_fmt','yuv420p','-movflags','+faststart',str(out/'rod_revision_03.mp4')],stdin=subprocess.PIPE)
 for f in range(180):proc.stdin.write(draw(f).tobytes())
 proc.stdin.close();assert proc.wait()==0
print(out)
