"""NEI batch mesh authoring. Python 3 / numpy / Pillow; no game checkout writes.

Model coordinates are quantized to 1/16 author unit. A pushed fixed-point
resource matrix converts them to each existing GI renderer's native units.
Meshes, preview GLBs, and O2R resources all derive from the same triangles.
"""
from pathlib import Path
import io, json, math, struct, hashlib
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[1]
Q=16
TAU=math.tau
def unit(a):
    a=np.asarray(a,float)
    return a/np.maximum(np.linalg.norm(a,axis=-1,keepdims=True),1e-12)
def rotation(axis,deg):
    a=math.radians(deg);c,s=math.cos(a),math.sin(a)
    if axis=='x':return np.array([[1,0,0],[0,c,-s],[0,s,c]])
    if axis=='y':return np.array([[c,0,s],[0,1,0],[-s,0,c]])
    return np.array([[c,-s,0],[s,c,0],[0,0,1]])
def bezier(a,b,c,d,count=16):
    t=np.linspace(0,1,count)[:,None]
    return (1-t)**3*np.array(a)+3*(1-t)**2*t*np.array(b)+3*(1-t)*t*t*np.array(c)+t**3*np.array(d)

class Model:
    def __init__(self,slug,name,entry,draw_scale,effective_scale=.75):
        self.slug=slug;self.name=name;self.entry=entry;self.draw_scale=draw_scale
        self.effective_scale=effective_scale
        self.native_scale=round(effective_scale/draw_scale/Q*65536)/65536
        self.parts=[];self.materials={};self.notes=[];self.markers={}
        self.prefix='objects/nei_gi_redesign/'+slug+'/'
    def material(self,name,color,kind=None,metal=0,rough=.6,alpha=1,emission=0,tex=None):
        if tex is None and kind:tex=surface(kind,color)
        self.materials[name]={'color':list(color) if tex is None else [1.,1.,1.],
            'tex':tex,'metal':metal,'rough':rough,'alpha':alpha,'emission':emission}
        return name
    def add(self,name,mat,p,n,uv,tri,flat=False):
        p=np.rint(np.asarray(p,float)*Q)/Q;n=unit(n);uv=np.asarray(uv,float)
        tri=np.asarray(tri,int).reshape(-1,3)
        areas=np.linalg.norm(np.cross(p[tri[:,1]]-p[tri[:,0]],p[tri[:,2]]-p[tri[:,0]]),axis=1)
        tri=tri[areas>1e-7]
        if not len(tri):return
        fn=np.cross(p[tri[:,1]]-p[tri[:,0]],p[tri[:,2]]-p[tri[:,0]])
        flip=np.einsum('ij,ij->i',fn,n[tri].sum(axis=1))<0
        tri[flip]=tri[flip][:,[0,2,1]]
        if flat:
            n=np.repeat(unit(np.cross(p[tri[:,1]]-p[tri[:,0]],p[tri[:,2]]-p[tri[:,0]])),3,axis=0)
            uv=uv[tri].reshape(-1,2);p=p[tri].reshape(-1,3);tri=np.arange(len(p)).reshape(-1,3)
        self.parts.append(dict(name=name,mat=mat,p=p,n=n,uv=uv,tri=tri))
    def transform(self,rot=None,offset=(0,0,0),scale=(1,1,1),start=0):
        rot=np.eye(3) if rot is None else rot
        for p in self.parts[start:]:
            p['p']=np.rint((p['p']*scale@rot.T+offset)*Q)/Q
            p['n']=unit(p['n']/scale@rot.T)
            # A second quantization can collapse the smallest feather ridges.
            f=p['tri'];points=p['p']
            area=np.linalg.norm(np.cross(points[f[:,1]]-points[f[:,0]],points[f[:,2]]-points[f[:,0]]),axis=1)
            p['tri']=f[area>1e-7]
        self.parts=[p for p in self.parts if len(p['tri'])]
    def tube(self,name,mat,path,radius,sides=10,pitch=25,cap=True):
        c=np.asarray(path,float);tang=unit(np.gradient(c,axis=0))
        ref=[0,1,0] if abs(tang[0,1])<.9 else [1,0,0]
        prev=unit(np.cross(tang[0],ref));bases=[]
        for t in tang:
            a=unit(prev-t*np.dot(prev,t));b=unit(np.cross(t,a));bases.append((a,b));prev=a
        rad=np.broadcast_to(radius,(len(c),));dist=np.r_[0,np.cumsum(np.linalg.norm(np.diff(c,axis=0),axis=1))]
        p=[];n=[];uv=[];tri=[];w=sides+1
        for i,center in enumerate(c):
            a,b=bases[i]
            for j in range(w):
                normal=a*math.cos(TAU*j/sides)+b*math.sin(TAU*j/sides)
                p.append(center+normal*rad[i]);n.append(normal);uv.append([j/sides,dist[i]/pitch])
        for i in range(len(c)-1):
            for j in range(sides):
                a=i*w+j;b=a+w;tri.extend([[a,b,a+1],[a+1,b,b+1]])
        if cap:
            for i,sign in [(0,-1),(len(c)-1,1)]:
                k=len(p);p.append(c[i]);n.append(tang[i]*sign);uv.append([.5,.5])
                for j in range(sides):tri.append([k,i*w+j,i*w+j+1])
        self.add(name,mat,p,n,uv,tri)
    def lathe(self,name,mat,rings,sides=24,cap=True):
        rings=np.asarray(rings,float);p=[];n=[];uv=[];tri=[];w=sides+1
        dy=np.gradient(rings[:,0]);dr=np.gradient(rings[:,1]);dist=np.r_[0,np.cumsum(np.linalg.norm(np.diff(rings,axis=0),axis=1))]
        for i,(y,r) in enumerate(rings):
            for j in range(w):
                a=TAU*j/sides;p.append([r*math.cos(a),y,r*math.sin(a)])
                n.append(unit([dy[i]*math.cos(a),-dr[i],dy[i]*math.sin(a)]));uv.append([j/sides,dist[i]/max(dist[-1],1)])
        for i in range(len(rings)-1):
            for j in range(sides):
                a=i*w+j;b=a+w;tri.extend([[a,b,a+1],[a+1,b,b+1]])
        if cap:
            for i,sign in [(0,-1),(len(rings)-1,1)]:
                k=len(p);p.append([0,rings[i,0],0]);n.append([0,sign,0]);uv.append([.5,.5])
                for j in range(sides):tri.append([k,i*w+j,i*w+j+1])
        self.add(name,mat,p,n,uv,tri)
    def sphere(self,name,mat,center,scale,rows=10,sides=20,flat=False):
        p=[];n=[];uv=[];tri=[];w=sides+1
        for i in range(rows+1):
            t=math.pi*i/rows
            for j in range(w):
                a=TAU*j/sides;q=np.array([math.sin(t)*math.cos(a),math.cos(t),math.sin(t)*math.sin(a)])
                p.append(np.array(center)+q*scale);n.append(unit(q/scale));uv.append([j/sides,i/rows])
        for i in range(rows):
            for j in range(sides):
                a=i*w+j;b=a+w;tri.extend([[a,b,a+1],[a+1,b,b+1]])
        self.add(name,mat,p,n,uv,tri,flat)
    def ring(self,name,mat,center,radius,thickness,axis='z',segments=32,sides=8):
        t=np.linspace(0,TAU,segments+1)
        r=np.broadcast_to(radius,(2,));path=np.c_[np.cos(t)*r[0],np.sin(t)*r[1],np.zeros(len(t))]
        if axis=='y':path=path[:,[0,2,1]]
        if axis=='x':path=path[:,[2,0,1]]
        self.tube(name,mat,path+center,thickness,sides,cap=False)
    def polygon(self,name,mat,xy,depth=2,z=0,bevel=.8):
        """Bevelled extrusion with flat normals; ear-clips concave face outlines."""
        xy=np.asarray(xy,float);center=xy.mean(axis=0);ln=len(xy)
        shrunk=center+(xy-center)*(1-bevel/max(np.ptp(xy,axis=0).max(),1)*2)
        p=np.vstack([np.c_[shrunk,np.full(ln,z+depth/2)],np.c_[xy,np.full(ln,z+depth/2-bevel/2)],np.c_[xy,np.full(ln,z-depth/2+bevel/2)],np.c_[shrunk,np.full(ln,z-depth/2)]])
        tri=[];n=[]
        faces=earclip(xy)
        tri.extend(faces);tri.extend([[i+3*ln for i in f[::-1]] for f in faces])
        for k in range(3):
            for j in range(ln):
                a=k*ln+j;b=k*ln+(j+1)%ln;tri.extend([[a,b,b+ln],[a,b+ln,a+ln]])
        # Exact per-face normal, retaining the specified outline order.
        tr=np.array(tri);pp=p[tr].reshape(-1,3)
        nn=np.repeat(unit(np.cross(p[tr[:,1]]-p[tr[:,0]],p[tr[:,2]]-p[tr[:,0]])),3,axis=0)
        uv=(pp[:,:2]-xy.min(axis=0))/np.maximum(np.ptp(xy,axis=0),1)
        self.add(name,mat,pp,nn,uv,np.arange(len(pp)).reshape(-1,3))
    def crystal(self,name,mat,center,radius,height,sides=6):
        p=[[0,-height*.5,0]]+[[radius*math.cos(TAU*j/sides),-height*.12,radius*math.sin(TAU*j/sides)] for j in range(sides)]+[[radius*.8*math.cos(TAU*j/sides),height*.24,radius*.8*math.sin(TAU*j/sides)] for j in range(sides)]+[[0,height*.5,0]]
        p=np.array(p)+center;tri=[]
        for j in range(sides):
            a=1+j;b=1+(j+1)%sides;c=a+sides;d=b+sides
            tri.extend([[0,b,a],[a,b,d],[a,d,c],[c,d,len(p)-1]])
        self.add(name,mat,p,unit(p-center),np.c_[(p[:,0]-center[0])/radius/2+.5,(p[:,1]-center[1])/height+.5],tri,True)

def earclip(xy):
    p=np.asarray(xy);inds=list(range(len(p)));out=[]
    cross=lambda a,b:float(a[0]*b[1]-a[1]*b[0])
    if sum(cross(p[i],p[(i+1)%len(p)]) for i in range(len(p)))<0:inds.reverse()
    while len(inds)>3:
        for ii,i in enumerate(inds):
            a=inds[ii-1];b=i;c=inds[(ii+1)%len(inds)]
            if cross(p[b]-p[a],p[c]-p[b])<=1e-9:continue
            if any(all(cross(p[v]-p[u],p[k]-p[u])>=-1e-8 for u,v in [(a,b),(b,c),(c,a)]) for k in inds if k not in (a,b,c)):continue
            out.append([a,b,c]);inds.pop(ii);break
        else:raise ValueError('Invalid/self-intersecting polygon')
    return out+[inds]

def surface(kind,color,size=256):
    """Repeatable procedural material swatches, authored as part of the mesh."""
    y,x=np.mgrid[0:size,0:size]/size;rng=np.random.default_rng(42)
    grain=rng.normal(0,.015,(size,size));val=np.ones_like(x)
    if kind in ('leather','braid'):
        a=np.mod((x+y)*4,1);b=np.mod((x-y)*4,1)
        seam=np.minimum(np.minimum(a,1-a),np.minimum(b,1-b))
        val=.5+.5*np.clip(seam/.10,0,1)**.5
    elif kind=='wood':val=.78+.18*np.sin(x*TAU*9+np.sin(y*TAU*2)*.7)+.05*np.sin(x*TAU*43)
    elif kind=='metal':val=.87+.1*np.sin(y*TAU)+.035*np.sin(x*TAU*37)
    elif kind=='stone':val=.84+.1*np.sin(x*TAU*5+np.sin(y*TAU*3))+.04*np.sin(y*TAU*20)
    elif kind=='energy':
        val=.77+.10*np.sin(x*TAU*4+np.sin(y*TAU*3)*2)+.10*np.cos(y*TAU*7+np.sin(x*TAU*6))
        val+=np.exp(-((np.sin(x*TAU*5+np.sin(y*TAU*3)*2))/.1)**2)*.35
    elif kind=='feather':val=.82+.12*np.cos((y-x*.3)*TAU*90)+.14*np.sin(x*math.pi)
    elif kind=='leaf':val=.72+.22*np.sin(x*math.pi)+.10*np.cos((y-np.abs(x-.5)*.5)*TAU*21)
    elif kind=='ceramic':val=.96+grain
    rgb=np.clip(np.array(color)[None,None,:]*255*(val+grain)[:,:,None],0,255).astype(np.uint8)
    return np.concatenate([rgb,np.full((size,size,1),255,np.uint8)],axis=2)

def header(kind,version=0):
    h=bytearray(64);struct.pack_into('<IIIQQ',h,0,0,kind,version,0xDEADBEEFDEADBEEF,1<<32);return bytes(h)
def texture_resource(tex):
    h,w=tex.shape[:2]
    return header(0x4F544558,1)+struct.pack('<IIIIffI',1,w,h,1,w/32,h/32,w*h*4)+tex.tobytes()
def matrix_resource(scale):
    fixed=np.rint(np.eye(4)*np.array([scale,scale,scale,1])[:,None]*65536).astype(np.int64).ravel()
    ints=[];fracs=[]
    for a,b in fixed.reshape(-1,2):
        ints.append((((int(a)>>16)&65535)<<16)|((int(b)>>16)&65535))
        fracs.append(((int(a)&65535)<<16)|(int(b)&65535))
    return header(0x4F4D5458)+struct.pack('<16I',*(ints+fracs))

def combine(tex,unlit=False):
    color='G_CCMUX_TEXEL0' if tex else 'G_CCMUX_PRIMITIVE'
    fields=[]
    for i in (0,1):
        if i==1:
            fields += [('A1','G_CCMUX_0'),('B1','G_CCMUX_0'),('C1','G_CCMUX_0'),('D1','G_CCMUX_COMBINED'),('Aa1','G_ACMUX_0'),('Ab1','G_ACMUX_0'),('Ac1','G_ACMUX_0'),('Ad1','G_ACMUX_COMBINED')]
            continue
        fields += [(f'A{i}','G_CCMUX_0' if unlit else color),(f'B{i}','G_CCMUX_0'),(f'C{i}','G_CCMUX_0' if unlit else 'G_CCMUX_SHADE'),(f'D{i}',color if unlit else 'G_CCMUX_0'),
            (f'Aa{i}','G_ACMUX_TEXEL0' if tex else 'G_ACMUX_0'),(f'Ab{i}','G_ACMUX_0'),(f'Ac{i}','G_ACMUX_PRIMITIVE' if tex else 'G_ACMUX_0'),(f'Ad{i}','G_ACMUX_0' if tex else 'G_ACMUX_PRIMITIVE')]
    return '<SetCombineLERP '+' '.join(k+'="'+v+'"' for k,v in fields)+'/>'

def export_resources(m,pass_tag='opa'):
    r={};r[m.prefix+'scale_mtx']=matrix_resource(m.native_scale)
    dl=['<DisplayList Version="0">','<PipeSync/>',f'<Matrix Path="{m.prefix}scale_mtx" Param="G_MTX_PUSH"/>',
        '<SetCycleType G_CYC_2CYCLE="1"/>','<SetAlphaCompare Mode="0"/>','<SetTextureLUT Mode="G_TT_NONE"/>',
        '<ClearGeometryMode G_CULL_FRONT="1" G_TEXTURE_GEN="1" G_TEXTURE_GEN_LINEAR="1" G_FOG="1"/>',
        '<SetGeometryMode G_ZBUFFER="1" G_SHADE="1" G_SHADING_SMOOTH="1" G_LIGHTING="1" G_CULL_BACK="1"/>']
    vertices=[];loads=0;last=None
    # All solids first, single-sided convex crystal shells last.
    parts=[p for p in m.parts if (m.materials[p['mat']]['alpha']<1)==(pass_tag=='xlu')]
    for part in parts:
        mat=m.materials[part['mat']]
        if part['mat']!=last:
            alpha=mat['alpha'];dl+=['<PipeSync/>',f'<SetRenderMode Mode1="G_RM_PASS" Mode2="G_RM_AA_ZB_{"XLU" if alpha<1 else "OPA"}_SURF2"/>',combine(mat['tex'] is not None,mat['emission']>0)]
            rgb=np.rint(np.array(mat['color'])*255).astype(int)
            dl.append(f'<SetPrimColor M="0" L="0" R="{rgb[0]}" G="{rgb[1]}" B="{rgb[2]}" A="{round(alpha*255)}"/>')
            if mat['tex'] is None:dl.append('<Texture S="0" T="0" Level="0" Tile="0" On="0"/>')
            else:
                path=m.prefix+part['mat']+'_tex';r[path]=texture_resource(mat['tex'])
                dl+=['<Texture S="65535" T="65535" Level="0" Tile="0" On="1"/>',f'<LoadTextureBlock Path="{path}" Format="0" Size="3" Width="32" Height="32" MaskS="5" MaskT="5" ShiftS="0" ShiftT="0" CMS_TXNoMirror="1" CMS_TXWrap="1" CMT_TXNoMirror="1" CMT_TXWrap="1"/>']
            last=part['mat']
        cache={};batch=[]
        def flush():
            nonlocal loads,cache,batch
            if not batch:return
            offset=len(vertices)
            for vi in cache:
                pos=np.rint(part['p'][vi]*Q).astype(int);uv=np.rint(part['uv'][vi]*1024).astype(int);n=np.rint(part['n'][vi]*127).astype(int)&255
                assert max(abs(uv))<32768 and max(abs(pos))<32768
                vertices.append(f'<Vtx X="{pos[0]}" Y="{pos[1]}" Z="{pos[2]}" S="{uv[0]}" T="{uv[1]}" R="{n[0]}" G="{n[1]}" B="{n[2]}" A="255"/>')
            dl.append(f'<LoadVertices Path="{m.prefix}mesh_{pass_tag}_vtx" VertexBufferIndex="0" VertexOffset="{offset}" Count="{len(cache)}"/>');loads+=1
            for i in range(0,len(batch),2):
                a=batch[i]
                if i+1<len(batch):
                    b=batch[i+1];dl.append(f'<Triangles2 V00="{a[0]}" V01="{a[1]}" V02="{a[2]}" Flag0="0" V10="{b[0]}" V11="{b[1]}" V12="{b[2]}" Flag1="0"/>')
                else:dl.append(f'<Triangle1 V00="{a[0]}" V01="{a[1]}" V02="{a[2]}" Flag0="0"/>')
            cache={};batch=[]
        for face in part['tri']:
            if len(set(map(int,face))-cache.keys())+len(cache)>32:flush()
            for vi in face:cache.setdefault(int(vi),len(cache))
            batch.append([cache[int(vi)] for vi in face])
        flush()
    dl+=['<PipeSync/>','<SetRenderMode Mode1="G_RM_PASS" Mode2="G_RM_AA_ZB_OPA_SURF2"/>','<Texture S="0" T="0" Level="0" Tile="0" On="0"/>','<SetPrimColor M="0" L="0" R="255" G="255" B="255" A="255"/>','<PopMatrix Param="G_MTX_MODELVIEW"/>','<EndDisplayList/>','</DisplayList>']
    r[m.prefix+'mesh_'+pass_tag+'_vtx']=('<Vertex Version="0">\n'+'\n'.join(vertices)+'\n</Vertex>\n').encode()
    r[m.entry if pass_tag=='opa' else m.prefix+'gi_xlu_dl']=('\n'.join(dl)+'\n').encode()
    for path,data in r.items():
        dest=ROOT/'RESOURCES'/path;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
    stats=dict(resource_count=len(r),vertex_records=len(vertices),vertex_loads=loads)
    if pass_tag=='opa' and any(m.materials[p['mat']]['alpha']<1 for p in m.parts):
        xlu=export_resources(m,'xlu')
        for k in stats:stats[k]+=xlu[k]
        stats['resource_count']-=1
        stats['xlu_entry']=m.prefix+'gi_xlu_dl'
    return stats
