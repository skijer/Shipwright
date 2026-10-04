"""Offline exact production USED-effect meshes. Standard alpha; no bloom or invented detail."""
import argparse, ctypes as C, struct, sys, subprocess
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'nei_gi/runtime_preview'))
from gl_context import context, gl, ptr, integer, uint

W,H=1500,1000
context(W,H)
F=C.c_float; D=C.c_double
ClearColor=gl('glClearColor',None,F,F,F,F);Clear=gl('glClear',None,uint)
Enable=gl('glEnable',None,uint);Disable=gl('glDisable',None,uint)
Viewport=gl('glViewport',None,integer,integer,integer,integer)
MatrixMode=gl('glMatrixMode',None,uint);LoadIdentity=gl('glLoadIdentity',None)
Ortho=gl('glOrtho',None,D,D,D,D,D,D);Rotate=gl('glRotatef',None,F,F,F,F)
Begin=gl('glBegin',None,uint);End=gl('glEnd',None)
Vertex=gl('glVertex3f',None,F,F,F);Color=gl('glColor4f',None,F,F,F,F)
BlendFunc=gl('glBlendFunc',None,uint,uint);DepthMask=gl('glDepthMask',None,C.c_ubyte)
ReadPixels=gl('glReadPixels',None,integer,integer,integer,integer,uint,uint,ptr)
EnableClient=gl('glEnableClientState',None,uint);DisableClient=gl('glDisableClientState',None,uint)
VertexPointer=gl('glVertexPointer',None,integer,uint,integer,ptr);ColorPointer=gl('glColorPointer',None,integer,uint,integer,ptr)
DrawArrays=gl('glDrawArrays',None,uint,integer,integer)
GenTextures=gl('glGenTextures',None,integer,C.POINTER(uint));BindTexture=gl('glBindTexture',None,uint,uint)
TexParameteri=gl('glTexParameteri',None,uint,uint,integer);TexImage=gl('glTexImage2D',None,uint,integer,integer,integer,integer,integer,uint,uint,ptr)
TexCoordPointer=gl('glTexCoordPointer',None,integer,uint,integer,ptr)
Enable(0x0BE2);BlendFunc(0x0302,0x0303);ClearColor(.035,.046,.065,1)
font=lambda n:ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',n)
NAMES=['LIGHT • released projectile','ICE • released crystalline lance','TIME GATE • growth / hold / fade',
       'FIRE • charge aura + embers','ICE • charge frost + falling shards','LIGHT • charge radiance + sparks',
       'ICE • jump wave','LIGHT • jump beam','ICE • expanding spin','LIGHT • expanding spin']

def load(path):
 raw=Path(path).read_bytes(); at=0; frames=[]
 dtype=np.dtype([('p','<f4',3),('rgba','u1',4),('uv','<f4',2)])
 for frame in range(180):
  row=[]
  for panel in range(10):
   layers=struct.unpack_from('<I',raw,at)[0];at+=4;layerdata=[]
   for _ in range(layers):
    material,n=struct.unpack_from('<II',raw,at);at+=8
    layerdata.append((material,np.frombuffer(raw,dtype=dtype,count=n,offset=at)));at+=n*24
   row.append(layerdata)
  frames.append(row)
 assert at==len(raw)
 return frames

textures={}
def texture(material):
 if material in textures:return textures[material]
 names={1:'ice_fracture',2:'fire_wisp',3:'light_rays',4:'fire_surge',5:'frost_surge',6:'light_surge',
        7:'fire_release_flow',8:'ice_release_flow',9:'fire_release_crest',10:'ice_release_crest'}
 folder='nei_rod_attack' if material>=4 else 'nei_used_magic'
 path=Path(__file__).resolve().parents[2]/'soh/assets/custom/objects'/folder/names[material]
 raw=path.read_bytes();w,h=struct.unpack_from('<II',raw,68)
 pixels=np.frombuffer(raw,np.uint8,offset=92).reshape(h,w,4)
 tid=uint();GenTextures(1,C.byref(tid));BindTexture(0x0DE1,tid.value)
 TexParameteri(0x0DE1,0x2801,0x2601);TexParameteri(0x0DE1,0x2800,0x2601)
 TexParameteri(0x0DE1,0x2802,0x812F if material in (4,5,6) else 0x2901);TexParameteri(0x0DE1,0x2803,0x2901 if material in (1,7,8) else 0x812F)
 TexImage(0x0DE1,0,0x1908,w,h,0,0x1908,0x1401,pixels.ctypes.data)
 textures[material]=tid.value;return tid.value
def drawmesh(data,material):
 Disable(0x0B44);Disable(0x0DE1);Disable(0x0B71);DepthMask(0)
 EnableClient(0x8074);EnableClient(0x8076)
 VertexPointer(3,0x1406,24,data.ctypes.data);ColorPointer(4,0x1401,24,data.ctypes.data+12)
 if material:
  Enable(0x0DE1);BindTexture(0x0DE1,texture(material));EnableClient(0x8078)
  TexCoordPointer(2,0x1406,24,data.ctypes.data+16)
 DrawArrays(0x0004,0,len(data));DisableClient(0x8074);DisableClient(0x8076)
 DisableClient(0x8078);Disable(0x0DE1)

def draw(data, extra=False):
 Clear(0x00004000|0x00000100)
 panels=list(range(6,10)) if extra else list(range(6))
 cols=2 if extra else 3; rows=2; cw=W//cols;ch=445
 for i,k in enumerate(panels):
  left=(i%cols)*cw;top=(i//cols)*ch+95
  Viewport(left,H-top-ch,cw,ch)
  MatrixMode(0x1701);LoadIdentity()
  if k < 2: low,high=-52,52
  elif k==2:low,high=-43,115
  elif k<6:low,high=-24,95
  elif k<8:low,high=-47,108
  else:low,high=-510,590
  half=(high-low)*cw/ch/2;Ortho(-half,half,low,high,-600,600)
  MatrixMode(0x1700);LoadIdentity();Rotate(24,1,0,0);Rotate(-18,0,1,0)
  if k >= 2:
   Disable(0x0B71);DepthMask(0);Color(.08,.10,.13,1);Begin(0x0001)
   extent=600 if k>=8 else 160 if k>=6 else 90
   for axis in range(-extent,extent+1,100 if k>=8 else 20):
    Vertex(-extent,-2,axis);Vertex(extent,-2,axis);Vertex(axis,-2,-extent);Vertex(axis,-2,extent)
   End()
  for material,layer in data[k]:drawmesh(layer,material)
 pixels=np.zeros((H,W,4),np.uint8);ReadPixels(0,0,W,H,0x1908,0x1401,pixels.ctypes.data)
 im=Image.fromarray(pixels[::-1].copy()).convert('RGB');d=ImageDraw.Draw(im)
 d.text((28,19),'NEI  /  USED MAGIC',font=font(29),fill='#f0f2f8')
 d.text((28,57),'Exact production triangles • standard alpha • no bloom • offline preview, not captured gameplay',font=font(17),fill='#98a5b9')
 for i,k in enumerate(panels):
  x=(i%cols)*cw+20;y=(i//cols)*ch+99
  d.text((x,y),NAMES[k],font=font(19),fill='#d4e3f0')
  if i%cols:d.line(((i%cols)*cw,y-5,(i%cols)*cw,y+ch-5),fill='#1d2839')
 return im

if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('samples');ap.add_argument('out');ap.add_argument('--still',action='store_true');ap.add_argument('--video',action='store_true');args=ap.parse_args()
 out=Path(args.out);out.mkdir(parents=True,exist_ok=True);frames=load(args.samples)
 draw(frames[72]).save(out/'nei_used_magic_contact.png')
 draw(frames[72],True).save(out/'nei_used_attacks_contact.png')
 for extra,name in ([] if args.still else [(False,'nei_used_magic'),(True,'nei_used_attacks')]):
  if args.video:
   proc=subprocess.Popen(['ffmpeg','-y','-loglevel','error','-f','rawvideo','-pix_fmt','rgb24',
                          '-s',f'{W}x{H}','-r','20','-i','-','-an','-c:v','libx264','-crf','20',
                          '-pix_fmt','yuv420p','-movflags','+faststart',str(out/(name+'.mp4'))],stdin=subprocess.PIPE)
   for i in range(180):proc.stdin.write(draw(frames[i],extra).tobytes())
   proc.stdin.close();assert proc.wait()==0
  else:
   ims=[draw(frames[i],extra) for i in range(0,180,2)]
   ims[0].save(out/(name+'.gif'),save_all=True,append_images=ims[1:],duration=100,loop=0,optimize=False)
 print(out)
