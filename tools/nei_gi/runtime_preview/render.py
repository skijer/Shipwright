"""Preview exact GLB checkpoint geometry and exported C++ effect triangles.

Headless Mesa renders standard alpha blending, native I8 orb texels, vertex-color energy,
opaque depth, then the translucent crystal skin. No bloom or added particles.
Lighting is an offline approximation; this is not captured gameplay.
"""
import argparse, ctypes as C, io, json, math, struct, subprocess
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw,ImageFont
from gl_context import context,gl,ptr,integer,uint
F=C.c_float;D=C.c_double
ROOT=Path(__file__).resolve().parents[1]
SLUGS=['fire_rod','ice_rod','light_rod','hylia_grace','zonai_permafrost','demise_destruction']
NAMES=['Fire Rod','Ice Rod','Light Rod',"Hylia’s Grace",'Zonai Permafrost','Demise Destruction']
TIPS=[(9.883,30.415,0),(10.365,31.899,0),(9.883,30.415,0),(0,0,0),(0,0,0),(0,0,0)]
W,H=1200,1000
context(W,H)
ClearColor=gl('glClearColor',None,F,F,F,F);Clear=gl('glClear',None,uint)
Enable=gl('glEnable',None,uint);Disable=gl('glDisable',None,uint)
Viewport=gl('glViewport',None,integer,integer,integer,integer)
MatrixMode=gl('glMatrixMode',None,uint);LoadIdentity=gl('glLoadIdentity',None)
Ortho=gl('glOrtho',None,D,D,D,D,D,D);Rotate=gl('glRotatef',None,F,F,F,F)
Translate=gl('glTranslatef',None,F,F,F);Scale=gl('glScalef',None,F,F,F)
Push=gl('glPushMatrix',None);Pop=gl('glPopMatrix',None)
Begin=gl('glBegin',None,uint);End=gl('glEnd',None)
Vertex=gl('glVertex3f',None,F,F,F);Color=gl('glColor4f',None,F,F,F,F);TexCoord=gl('glTexCoord2f',None,F,F)
GenLists=gl('glGenLists',uint,integer);NewList=gl('glNewList',None,uint,uint);EndList=gl('glEndList',None);CallList=gl('glCallList',None,uint)
GenTextures=gl('glGenTextures',None,integer,C.POINTER(uint));BindTexture=gl('glBindTexture',None,uint,uint)
TexParameteri=gl('glTexParameteri',None,uint,uint,integer);TexImage=gl('glTexImage2D',None,uint,integer,integer,integer,integer,integer,uint,uint,ptr)
BlendFunc=gl('glBlendFunc',None,uint,uint);DepthMask=gl('glDepthMask',None,C.c_ubyte)
ReadPixels=gl('glReadPixels',None,integer,integer,integer,integer,uint,uint,ptr)
EnableClient=gl('glEnableClientState',None,uint);DisableClient=gl('glDisableClientState',None,uint)
VertexPointer=gl('glVertexPointer',None,integer,uint,integer,ptr);ColorPointer=gl('glColorPointer',None,integer,uint,integer,ptr)
TexCoordPointer=gl('glTexCoordPointer',None,integer,uint,integer,ptr)
DrawArrays=gl('glDrawArrays',None,uint,integer,integer)
Enable(0x0B71);Enable(0x0BE2);BlendFunc(0x0302,0x0303)
ClearColor(.068,.086,.115,1)
fontpath='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
font=lambda n:ImageFont.truetype(fontpath,n)

x,y=np.deg2rad([12,25]);sx,cx,sy,cy=np.sin(x),np.cos(x),np.sin(y),np.cos(y)
R=np.array([[cy,0,sy],[sx*sy,cx,-sx*cy],[-cx*sy,sx,cx*cy]])
light=np.array([-.4,.75,.6]);light/=np.linalg.norm(light)

def model(slug, color_tint=(1,1,1)):
 data=(ROOT/'CHECKPOINTS'/slug/(slug+'.glb')).read_bytes();n=struct.unpack_from('<I',data,12)[0]
 doc=json.loads(data[20:20+n]);binary=data[28+n:]
 meta=json.loads((ROOT/'CHECKPOINTS'/slug/'checkpoint.json').read_text())
 factor=doc['nodes'][-1]['scale'][0]*meta['draw_scale']
 def acc(i):
  a=doc['accessors'][i];v=doc['bufferViews'][a['bufferView']];cols={'VEC3':3,'VEC2':2,'SCALAR':1}[a['type']]
  return np.frombuffer(binary,dtype={5126:'<f4',5125:'<u4'}[a['componentType']],count=a['count']*cols,offset=v.get('byteOffset',0)+a.get('byteOffset',0)).reshape(-1,cols)
 mats=[]
 for mat in doc['materials']:
  pbr=mat['pbrMetallicRoughness'];tex=0
  if 'baseColorTexture' in pbr:
   image=doc['images'][doc['textures'][pbr['baseColorTexture']['index']]['source']];v=doc['bufferViews'][image['bufferView']]
   im=Image.open(io.BytesIO(binary[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']])).convert('RGBA')
   pixels=np.ascontiguousarray(im);tid=uint();GenTextures(1,C.byref(tid));tex=tid.value
   BindTexture(0x0DE1,tex);TexParameteri(0x0DE1,0x2801,0x2601);TexParameteri(0x0DE1,0x2800,0x2601)
   TexParameteri(0x0DE1,0x2802,0x2901);TexParameteri(0x0DE1,0x2803,0x2901)
   TexImage(0x0DE1,0,0x1908,im.width,im.height,0,0x1908,0x1401,pixels.ctypes.data)
  mats.append((tex,pbr['baseColorFactor'],bool(mat.get('emissiveFactor',[0])[0]),mat.get('alphaMode')=='BLEND'))
 passes=[]
 for transparent in (False,True):
  dl=GenLists(1);NewList(dl,0x1300)
  Enable(0x0B44)
  for mesh in doc['meshes']:
   for primitive in mesh['primitives']:
    tex,base,emissive,trans=mats[primitive['material']]
    if trans!=transparent:continue
    a=primitive['attributes'];pos=acc(a['POSITION'])*factor;norm=acc(a['NORMAL'])@R.T;uv=acc(a['TEXCOORD_0']);indices=acc(primitive['indices']).ravel()
    shade=np.ones(len(pos)) if emissive else .42+.58*np.maximum(norm@light,0)
    colors=shade[:,None]*np.array(base[:3])*np.array(color_tint)
    if tex:Enable(0x0DE1);BindTexture(0x0DE1,tex)
    else:Disable(0x0DE1)
    Begin(0x0004)
    for i in indices:
     Color(*colors[i],base[3]);TexCoord(*uv[i]);Vertex(*pos[i])
    End()
  Disable(0x0DE1);EndList();passes.append(dl)
 return passes

def effects(path):
 raw=path.read_bytes();at=0;frames=[]
 dtype=np.dtype([('p','<f4',3),('rgba','u1',4)])
 for _ in range(180):
  row=[]
  for _ in range(8):
   n=struct.unpack_from('<I',raw,at)[0];at+=4
   row.append(np.frombuffer(raw,dtype=dtype,count=n,offset=at));at+=n*16
  frames.append(row)
 assert at==len(raw)
 return frames

def orbs(path):
 raw=Path(str(path)+'.orbs').read_bytes();at=0;frames=[];textures={}
 dtype=np.dtype([('p','<f4',3),('rgba','u1',4),('uv','<f4',2)])
 for _ in range(180):
  row=[]
  for _ in range(6):
   n=struct.unpack_from('<I',raw,at)[0];at+=4
   if not n:row.append(None);continue
   hot,edge=struct.unpack_from('<II',raw,at);at+=8
   key=(hot,edge,raw[at:at+4096])
   if key not in textures:
    intensity=np.frombuffer(raw,dtype=np.uint8,count=4096,offset=at).reshape(64,64)
    rgb=lambda c:np.array([(c>>16)&255,(c>>8)&255,c&255])
    rgba=np.zeros((64,64,4),np.uint8)
    rgba[:,:,:3]=np.rint(rgb(edge)+(rgb(hot)-rgb(edge))*intensity[:,:,None]/255)
    rgba[:,:,3]=intensity
    tid=uint();GenTextures(1,C.byref(tid));textures[key]=tid.value
    BindTexture(0x0DE1,tid.value)
    TexParameteri(0x0DE1,0x2801,0x2601);TexParameteri(0x0DE1,0x2800,0x2601)
    TexParameteri(0x0DE1,0x2802,0x812F);TexParameteri(0x0DE1,0x2803,0x812F)
    TexImage(0x0DE1,0,0x1908,64,64,0,0x1908,0x1401,rgba.ctypes.data)
   at+=4096
   mesh=np.frombuffer(raw,dtype=dtype,count=n,offset=at);at+=n*24
   row.append((textures[key],mesh))
  frames.append(row)
 assert at==len(raw)
 return frames

def draworb(orb):
 if orb is None:return
 texture,data=orb
 Disable(0x0B44);Enable(0x0DE1);BindTexture(0x0DE1,texture)
 EnableClient(0x8074);EnableClient(0x8076);EnableClient(0x8078)
 VertexPointer(3,0x1406,24,data.ctypes.data);ColorPointer(4,0x1401,24,data.ctypes.data+12)
 TexCoordPointer(2,0x1406,24,data.ctypes.data+16)
 DrawArrays(0x0004,0,len(data))
 DisableClient(0x8074);DisableClient(0x8076);DisableClient(0x8078);Disable(0x0DE1)

def drawfx(data):
 Disable(0x0B44);Disable(0x0DE1)
 EnableClient(0x8074);EnableClient(0x8076)
 VertexPointer(3,0x1406,16,data.ctypes.data);ColorPointer(4,0x1401,16,data.ctypes.data+12)
 DrawArrays(0x0004,0,len(data));DisableClient(0x8074);DisableClient(0x8076)

def pose(x,y,w,h,low=-49,high=68):
 Viewport(x,y,w,h);MatrixMode(0x1701);LoadIdentity()
 half=(high-low)*w/h/2;Ortho(-half,half,low,high,-300,300)
 MatrixMode(0x1700);LoadIdentity();Rotate(12,1,0,0);Rotate(25,0,1,0)

def pixels():
 data=np.zeros((H,W,4),np.uint8);ReadPixels(0,0,W,H,0x1908,0x1401,data.ctypes.data)
 return Image.fromarray(data[::-1].copy()).convert('RGB')

def annotate(im,mode,close_up=False):
 d=ImageDraw.Draw(im)
 d.rectangle((0,0,W,69),fill='#111923')
 title='ice rod • crystalline energy' if mode=='ice' else ('energy close-ups' if close_up else ('elemental energy' if mode=='energy' else 'wonder-style shimmer'))
 d.text((30,15),'NEI GI • '+title+' — POC7',font=font(28),fill='#f0f5ff')
 d.text((30,51),'Production effect geometry + native orb texels · accepted models · no added bloom',font=font(14),fill='#aabacc')
 if mode=='ice':
  d.text((35,95),'Full rod',font=font(25),fill='#eef3fd')
  d.text((635,95),'Ice detail',font=font(25),fill='#eef3fd')
  d.text((35,134),'Central ice focus + surrounding energy',font=font(17),fill='#aabacc')
  d.text((635,134),'Collapsing fragments • fixed crystals removed',font=font(17),fill='#aabacc')
 elif mode=='energy':
  notes=['Roiling fire body + rising hot embers','Orb energy + collapsing ice fragments','Existing corona and glints preserved',
         'Turbulent pink core + crossing motes','Contained turquoise energy + motes','Black center + turbulent pale energy']
  for i,name in enumerate(NAMES):
   xx=(i%3)*400;yy=82+(i//3)*445
   d.text((xx+22,yy),name,font=font(24),fill='#eef3fd')
   d.text((xx+22,yy+32),notes[i],font=font(13),fill='#aabacc')
 else:
  for row,name in enumerate(['Roc’s Feather', 'Deku Leaf • GREEN particles']):
   yy=90+row*425
   d.text((40,yy),name+' — OFF',font=font(22),fill='#bac7d8')
   d.text((640,yy),name+' — ON',font=font(22),fill='#eef3fd')
 d.text((30,970),'Preview for approval • In-game camera, lighting, and interpolation still need runtime review.',font=font(17),fill='#92a4ba')
 return im

def main():
 p=argparse.ArgumentParser();p.add_argument('effects',type=Path);p.add_argument('output',type=Path);p.add_argument('--frames',type=int,default=180);p.add_argument('--mode',choices=['all','energy','shimmer','ice'],default='all');p.add_argument('--close-up',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 fx=effects(a.effects);orb=orbs(a.effects);models=[model(s) for s in SLUGS];feather=model('rocs_feather');leaf=model('deku_leaf')
 for mode in (('energy','shimmer') if a.mode=='all' else (a.mode,)):
  stem='NEI_GI_POC7_'+mode+('_closeups' if a.close_up and mode=='energy' else '')
  out=a.output/(stem+'.mp4')
  proc=subprocess.Popen(['ffmpeg','-loglevel','error','-y','-f','rawvideo','-vcodec','rawvideo','-pix_fmt','rgb24','-s',f'{W}x{H}','-r','20','-i','-','-an','-c:v','libx264','-preset','fast','-crf','18','-pix_fmt','yuv420p','-movflags','+faststart',str(out)],stdin=subprocess.PIPE)
  for frame in range(a.frames):
   DepthMask(1);Clear(0x4000|0x0100)
   if mode=='ice':
    for col in range(2):
     pose(col*600,70,600,750,-28 if col else -49,28 if col else 68)
     if col:Translate(*(-np.array(TIPS[1])))
     DepthMask(1);CallList(models[1][0]);DepthMask(0)
     Push();Translate(*TIPS[1]);draworb(orb[frame][1]);drawfx(fx[frame][1]);Pop();CallList(models[1][1])
   elif mode=='energy':
    for i,(opa,xlu) in enumerate(models):
     if a.close_up:
      pose((i%3)*400,80+(1-i//3)*445,400,350,-27,27)
      Translate(*(-np.array(TIPS[i])))
     else:pose((i%3)*400,80+(1-i//3)*445,400,350)
     DepthMask(1);CallList(opa);DepthMask(0)
     Push();Translate(*TIPS[i]);draworb(orb[frame][i]);drawfx(fx[frame][i]);Pop();CallList(xlu)
   else:
    for row,item in enumerate([feather,leaf]):
     for i in range(2):
      pose(i*600,65+(1-row)*425,600,370,-49,68);DepthMask(1);CallList(item[0]);DepthMask(0)
      if i:drawfx(fx[frame][6+row])
   im=annotate(pixels(),mode,a.close_up)
   if frame==36 or a.frames==1:im.save(a.output/(stem+'.png'))
   proc.stdin.write(im.tobytes())
  proc.stdin.close();assert proc.wait()==0;print(out,flush=True)
if __name__=='__main__':main()
