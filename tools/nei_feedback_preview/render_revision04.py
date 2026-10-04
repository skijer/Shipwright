"""Exact quantized production meshes/materials, standard alpha; no added bloom."""
import sys, struct, subprocess
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'nei_used_fx'))
import render as r
W,H=r.W,r.H
raw=Path(sys.argv[1]).read_bytes()
total,panels=struct.unpack_from('<II',raw);at=8
motion=panels==2
dtype=np.dtype([('p','<f4',3),('rgba','u1',4),('uv','<f4',2)])
frames=[]
for frame in range(total):
 row=[]
 for panel in range(panels):
  n=struct.unpack_from('<I',raw,at)[0];at+=4;layers=[]
  for _ in range(n):
   mat,count=struct.unpack_from('<II',raw,at);at+=8
   layers.append((mat,np.frombuffer(raw,dtype=dtype,count=count,offset=at)));at+=count*24
  row.append(layers)
 frames.append(row)
assert at==len(raw)
titles=['FIRE / Light charge with fire','ICE / charge preserved','LIGHT / recovered charge',
        'FIRE / release candidate','ICE / release candidate','LIGHT / restored release']
sub=['Same motion; original fire artwork','Existing frost and sparks','Earlier rays, spacing and material',
     'Tall crests + low outward sweep','Frost front + low outward sweep','Earlier separated rays and rings']
def draw(frame,still=False):
 r.Clear(0x4000|0x100)
 for panel in range(panels):
  left,top,cw,ch=(panel*750,110,750,800) if motion else ((panel%3)*500,100+(panel//3)*432,500,432)
  sample=(18 if motion else 49 if panel<3 else 63) if still else frame
  r.Viewport(left,H-top-ch,cw,ch);r.MatrixMode(0x1701);r.LoadIdentity()
  if motion:low,high,center=-300,300,190
  elif panel<3:low,high,center=-20,132,0
  else:low,high,center=-380,540,0
  half=(high-low)*cw/ch/2;r.Ortho(center-half,center+half,low,high,-1000,1000)
  r.MatrixMode(0x1700);r.LoadIdentity();r.Rotate(24,1,0,0);r.Rotate(-18,0,1,0)
  r.Disable(0x0B71);r.Disable(0x0DE1);r.DepthMask(0)
  r.Color(.12,.16,.20,1);r.Begin(1)
  extent,step=(650,100) if motion or panel>=3 else (110,20)
  for n in range(-extent,extent+1,step):
   r.Vertex(-extent,-2,n);r.Vertex(extent,-2,n);r.Vertex(n,-2,-extent);r.Vertex(n,-2,extent)
  r.End()
  r.Color(.34,.39,.44,1);r.Begin(1)
  r.Vertex(0,0,0);r.Vertex(0,56,0);r.Vertex(-10,56,0);r.Vertex(10,56,0)
  if not motion and panel<3:r.Vertex(0,38,0);r.Vertex(16,48,0)
  r.End()
  for mat,data in frames[sample][panel]:r.drawmesh(data,mat)
 pixels=np.zeros((H,W,4),np.uint8);r.ReadPixels(0,0,W,H,0x1908,0x1401,pixels.ctypes.data)
 im=Image.fromarray(pixels[::-1].copy()).convert('RGB');d=ImageDraw.Draw(im)
 d.rectangle((0,0,W,91),fill='#101821')
 title='PROJECTILE MOTION / FINAL CHARGE' if motion else 'ROD CHARGE + RELEASE / FINAL CHARGE'
 d.text((22,13),title,font=r.font(27),fill='#f1f4f8')
 d.text((22,53),'Production geometry + materials • offline preview • not captured gameplay • not pushed',font=r.font(17),fill='#a9b9ca')
 for panel in range(panels):
  x,y=(panel*750+20,116) if motion else ((panel%3)*500+17,108+(panel//3)*432)
  label=('FIRE / three-shot volleys' if panel==0 else 'ICE / three-shot volleys + wakes') if motion else titles[panel]
  detail='Full travel and fade; second volley stops on impact' if motion else sub[panel]
  d.text((x,y),label,font=r.font(19),fill='#e8f0f8');d.text((x,y+28),detail,font=r.font(13),fill='#a8bed2')
  if panel % (2 if motion else 3):d.line((x-17,y-6,x-17,y+(790 if motion else 425)),fill='#283645')
 if not motion:d.line((0,530,W,530),fill='#283645')
 d.rectangle((0,H-31,W,H),fill='#101821')
 if still:foot='Still: full charge and late release sampled separately. Gray marker: 56 units. Review appearance in the full-speed video.'
 elif motion:foot='20 updates/s • overlapping volleys, impact fade and slot reuse • interpolation tested separately with the production renderer.'
 else:
  tick=frame%90
  timing=f'CHARGE  {(tick+1)*.05:.2f}s' if tick<52 else f'RELEASE  {(tick-51)*.05:.2f} / 0.80s' if tick<68 else 'NEXT CAST'
  foot=f'20 updates/s • native 16-update release window • no slow motion • {timing}  • gray marker: 56 units'
 d.text((19,H-25),foot,font=r.font(14),fill='#adbed0')
 return im
out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
name='rod_motion_04' if motion else 'rod_charge_release_final'
draw(63,True).save(out/(name+'.png'))
if '--still' not in sys.argv:
 proc=subprocess.Popen(['ffmpeg','-y','-v','error','-f','rawvideo','-pix_fmt','rgb24','-s',f'{W}x{H}',
                        '-r','20','-i','-','-an','-c:v','libx264','-crf','18','-pix_fmt','yuv420p','-movflags','+faststart',str(out/(name+'.mp4'))],stdin=subprocess.PIPE)
 for frame in range(total):proc.stdin.write(draw(frame).tobytes())
 proc.stdin.close();assert proc.wait()==0
print(out/(name+'.mp4'))
