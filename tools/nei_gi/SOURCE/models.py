"""Approved NEI GI designs, built as editable three-dimensional meshes."""
import math
import numpy as np
from PIL import Image,ImageDraw
from meshkit import Model,ROOT,TAU,unit,rotation,bezier,surface

ENTRIES={
 'fire_rod':('Fire Rod','fire_rod/Cylinder_001_opaque_dl',.2,.78),
 'ice_rod':('Ice Rod','ice_rod/ice_rod_opaque_dl',.2,.78),
 'light_rod':('Light Rod','light_rod/Cylinder_002_opaque_dl',.2,.78),
 'rocs_feather':("Roc's Feather",'rocs_feather/rocs_feather_dl',.5,.78),
 'time_gate':('Time Gate','time_gate/g_timegate_dl',.5,.97),
 'shovel':('Shovel','shovel/gShovelGiveDL_opaque_dl',.2,.78),
 'gust_jar':('Gust Jar','gust_jar/jar_model_dl',5.,.78),
 'hylia_grace':("Hylia's Grace",'magic_spell/gHyliaGraceGiveDL',1.,.74),
 'zonai_permafrost':('Zonai Permafrost','magic_spell/gZonaiPermafrostGiveDL',1.,.74),
 'demise_destruction':('Demise Destruction','magic_spell/gDemiseDestructionGiveDL',1.,.74),
 'ball_and_chain':('Ball and Chain','ball_and_chain/g_ball_and_chain_dl',.25,.87),
 'deku_leaf':('Deku Leaf','deku_leaf/g_dekuleaf_dl',.5,.65),
 'mogma_mitts':('Mogma Mitts','mogma_mitts/gMogmaMittsGiveDL',.5,.52),
 'switch_hook':('Switch Hook','switchhook/gSwitchHookGiveDL',.01,.86),
 'beetle':('Beetle','beetle/g_beetle_dl',.3,.58),
}
def model(slug):
    name,path,scale,effective=ENTRIES[slug]
    return Model(slug,name,'objects/nei_gi_redesign/'+slug+'/gi_dl',scale,effective)
def metals(m):
    m.material('gold',[.76,.48,.13],'metal',.8,.33)
    m.material('edge',[.97,.77,.37],'metal',.8,.27)
    m.material('steel',[.49,.59,.69],'metal',.85,.36)
    m.material('dark',[.13,.15,.20],'metal',.6,.55)
    m.material('ivory',[.86,.80,.64],'stone',0,.8)
def band(m,name,mat,y,r,w=1):
    m.lathe(name,mat,[(y-w,r*.95),(y-w*.6,r),(y+w*.6,r),(y+w,r*.95)],12)
def studs(m,y,r,count=6,mat='edge'):
    for j in range(count):
        a=TAU*j/count;m.sphere('Rivet',mat,[r*math.cos(a),y,r*math.sin(a)],[.75,.75,.75],4,6)

def rod(slug):
    m=model(slug);metals(m);ice=slug=='ice_rod';light=slug=='light_rod'
    trim='steel' if ice else 'gold';edge='steel' if ice else 'edge'
    grip=[.09,.23,.35] if ice else ([.70,.55,.29] if light else [.24,.09,.035])
    m.material('grip',grip,'braid',0,.78)
    core=np.array([53,124,255] if ice else ([253,255,123] if light else [250,139,32]))/255
    m.material('energy',core,'energy',.2,.12,emission=.65)
    m.material('crystal_highlight',[.65,.92,1.] if ice else [1.,.93,.59],None,.4,.15,emission=.25)
    m.lathe('Wrapped shaft','grip',[(-46,3.6),(-43,4.5),(-20,4.1),(0,4.0),(20,4.8),(25,5.5)],16)
    for y,r in [(-44,5.1),(-16,5),(19,5.8),(25,7.6)]:
        band(m,'Engraved collar',trim,y,r,1.5);band(m,'Collar edge',edge,y-1.5,r+.4,.35);band(m,'Collar edge',edge,y+1.5,r+.4,.35)
    studs(m,25,7.6);studs(m,-44,5.3)
    m.crystal('Pommel',trim if not ice else 'energy',[0,-49.5,0],5.4,9,6)
    # Metal inlay curves reinforce the shaft without hiding the wrap.
    t=np.linspace(0,1,28)
    for phase in [0,math.pi]:
        a=phase+t*TAU*1.2;r=4.5+.6*t
        m.tube('Winding metal inlay',trim,np.c_[r*np.cos(a),-12+35*t,r*np.sin(a)],.55,5)
    if ice:
        m.crystal('Faceted ice focus','energy',[0,43,0],10.5,34,6)
    else:
        m.sphere('Luminous focus','energy',[0,41,0],[12,12,12],8,16)
        # Fine arcs lie on the surface; particles are added by the GI renderer.
        for phase in [0,2.1,4.2]:
            t=np.linspace(-1.2,1.4,16)
            path=np.c_[12.1*np.cos(t),41+12.1*np.sin(t),np.zeros(len(t))]@rotation('y',math.degrees(phase)).T
            m.tube('Focus surface arc','crystal_highlight',path,.42,4)
    for j in range(4):
        a=TAU*j/4+.4;t=np.linspace(0,1,14)
        r=6+8*np.sin(t*math.pi*.88);ang=a+t*.85
        path=np.c_[r*np.cos(ang),26+27*t,r*np.sin(ang)]
        m.tube('Crown talon',trim,path,np.linspace(2.3,.15,len(t)),7)
    tilt=rotation('z',-18);m.transform(tilt)
    m.markers['tip_author']=(np.array([0,43 if ice else 41,0])@tilt.T).tolist()
    m.notes=['18 degree baked lean; tip marker consumed by the unified GI particle draw.']
    return m

def spell(slug):
    m=model(slug);dark=slug=='demise_destruction';pink=slug=='hylia_grace'
    col=[0,0,0] if dark else (np.array([255,150,255] if pink else [100,255,230])/255).tolist()
    m.material('core',col,None if dark else 'energy',.15,.25,emission=0 if dark else .5)
    shell=[.82,.84,.87] if dark else ([1,.70,.95] if pink else [.63,.96,1.])
    m.material('shell',shell,None,.1,.2,alpha=.29)
    m.material('facet_edges',[.86,.9,.95] if dark else shell,None,.1,.25,emission=.25)
    m.sphere('Black core #000000' if dark else 'Spell energy core','core',[0,0,0],[19,19,19],12,24)
    # Closed octahedron, black/pink/turquoise core drawn before its translucent skin.
    p=np.array([[0,50,0],[30,0,0],[0,0,30],[-30,0,0],[0,0,-30],[0,-50,0]])
    tri=[]
    for j in range(4):tri.extend([[0,1+j,1+(j+1)%4],[5,1+(j+1)%4,1+j]])
    m.add('Crystal shell','shell',p,unit(p),p[:,[0,1]]/[60,100]+.5,tri,True)
    for i,j in [(0,1),(0,2),(0,3),(0,4),(5,1),(5,2),(5,3),(5,4),(1,2),(2,3),(3,4),(4,1)]:
        m.tube('Polished crystal edge','facet_edges',[p[i],p[j]],.18,5)
    m.notes=['Core before shell; neutral pale crystal for Demise. Demise core primitive RGB is exactly 0,0,0.']
    return m

def feather():
    m=model('rocs_feather');metals(m)
    # Broad angled barb groups survive GI-size filtering; the previous dense
    # horizontal swatch dissolved into dark scanlines in the overview.
    ty,tx=np.mgrid[0:256,0:256]/256
    rachis=.475+.05*np.sin(ty*2)
    phase=(ty-np.abs(tx-rachis)*.17)*20
    f=phase%1
    val=.76+.20*np.exp(-((f-.28)/.26)**2)
    val-=.08*np.exp(-(np.minimum(f,1-f)/.08)**2)
    val+=.055*np.sin(phase*TAU*3+.2*np.sin(tx*TAU*3))
    val*=.88+.12*np.sin(ty*math.pi)**.5
    rgb=np.uint8(np.clip(np.array([.07,.61,.98])*255*val[:,:,None],0,255))
    feather_tex=np.dstack([rgb,np.full((256,256),255,np.uint8)])
    m.material('blue',[1,1,1],tex=feather_tex,rough=.82)
    m.material('cyan',[.12,.72,.96],'ceramic',0,.84)
    m.material('shaft',[.15,.72,.91],'ceramic',0,.75)
    # The clefts are real silhouette cuts, not surface lines over a solid backing.
    # Keep more than half the vane connected so the outline does not become a fern.
    from meshkit import earclip
    for sign in [-1,1]:
        def edge(y,factor=1):
            t=np.clip((y+27)/88,0,1)
            width=math.sin(math.pi*t)**.82*(18 if sign==1 else 15)
            return [2*math.sin(t*2)+sign*width*factor,y]
        notches=[-14,-2,11,23,35,46] if sign==1 else [-17,-6,7,19,31,43]
        outline=[edge(-27)]
        previous=-27
        for i,y in enumerate(notches):
            for yy in np.linspace(previous,y-.9,5)[1:]:outline.append(edge(yy))
            t=np.clip((y+27)/88,0,1);w=math.sin(math.pi*t)**.82*(18 if sign==1 else 15)
            shaft=2*math.sin(t*2)
            outline.append([shaft+sign*w*(.48 if i in (1,3) else .62),y-4.6])
            outline.append(edge(y+.9));previous=y+.9
        for yy in np.linspace(previous,61,8)[1:]:outline.append(edge(yy))
        # Return down the shaft; slight overlap is hidden by the physical quill.
        for y in np.linspace(57,-24,12):
            t=(y+27)/88;outline.append([2*math.sin(t*2)-sign*.22,y])
        xy=np.array(outline);t=np.clip((xy[:,1]+27)/88,0,1)
        width=np.maximum(np.sin(t*math.pi)**.82*(18 if sign==1 else 15),.1)
        f=np.clip(np.abs(xy[:,0]-2*np.sin(t*2))/width,0,1)
        z=2.5*np.sin(t*math.pi)+1.25*np.sin(f*math.pi)
        p=np.c_[xy,z];uv=np.c_[np.clip((xy[:,0]+19)/40,0,1),t]
        triangles=earclip(xy)
        m.add('Connected vane with six open tuft clefts','blue',p,[[0,.035,1]]*len(p),uv,triangles)
        m.add('Notched vane underside','blue',p-[0,0,.28],[[0,-.035,-1]]*len(p),uv,triangles)
        # Follow the actual triangulated surface so raised barbs remain smooth,
        # without dipping into the vane and reappearing as disconnected dots.
        def surface_z(point):
            for ids in triangles:
                a,b,c=xy[ids]
                ab=b-a;ac=c-a;ap=point-a
                det=ab[0]*ac[1]-ab[1]*ac[0]
                if abs(det)<1e-8:continue
                u=(ap[0]*ac[1]-ap[1]*ac[0])/det
                v=(ab[0]*ap[1]-ab[1]*ap[0])/det
                if u>=-1e-6 and v>=-1e-6 and u+v<=1+1e-6:
                    return p[ids[0],2]*(1-u-v)+p[ids[1],2]*u+p[ids[2],2]*v
            return None
        for y in np.arange(-20,49,5):
            t=(y+27)/88;w=math.sin(math.pi*t)**.82*(18 if sign==1 else 15)
            x=2*math.sin(t*2);z=2.5*math.sin(t*math.pi)
            path=bezier([x,y,z+.15],[x+sign*w*.25,y+2.0,z+1.15],
                        [x+sign*w*.53,y+4.2,z+1.2],[x+sign*w*.74,y+5.0,z+.85],10)
            visible=[]
            for point in path:
                z_at=surface_z(point[:2])
                if z_at is None:break
                visible.append([point[0],point[1],z_at+.08])
            if len(visible)>1:
                m.tube('Readable raised barb ridge','cyan',visible,np.linspace(.20,.09,len(visible)),5)
    t=np.linspace(0,1,36);path=np.c_[2*np.sin(t*2),-50+110*t,2.5*np.sin(np.clip((t-.22)/.78,0,1)*math.pi)]
    m.tube('Curved quill','shaft',path,1.35*(1-t)+.22,8)
    start=len(m.parts);band(m,'Quill gold binding','gold',-42,2.0,1.7);m.transform(offset=[.4,0,0],start=start)
    m.transform(rotation('z',-16))
    m.notes=['Visual POC3: accepted open tuft clefts and vane geometry preserved exactly; coarser angled barb texture, clearer quill, and thicker tapered relief improve detail at GI preview size.']
    return m

def ball_chain():
    from itertools import permutations,product
    m=model('ball_and_chain')
    def forged(color,seed):
        rng=np.random.default_rng(seed)
        cloud=np.asarray(Image.fromarray(rng.integers(85,170,(12,12),dtype=np.uint8)).resize((256,256),Image.Resampling.BICUBIC))/255
        value=.76+cloud*.38+rng.normal(0,.013,(256,256))
        rgb=np.uint8(np.clip(np.array(color)*255*value[:,:,None],0,255))
        return np.dstack([rgb,np.full((256,256),255,np.uint8)])
    for name,color,seed in [('iron',[.51,.57,.56],11),('edge',[.55,.60,.58],12),
                             ('seam',[.28,.32,.31],13),('patina',[.35,.36,.25],14),
                             ('steel',[.49,.55,.54],15)]:
        m.material(name,color,tex=forged(color,seed),metal=.85,rough=.57)
    center=np.array([0,-5,-3.])
    # Eight broad cast plates and six pointed iron spikes. Each plate has a
    # real stepped, recessed border; no thin coloured great-circle decoration.
    verts=np.unique(np.array([np.array(p)*s for p in permutations([0,1,2])
                              for s in product([-1,1],repeat=3)]),axis=0)
    faces=[]
    for signs in product([-1,1],repeat=3):
        normal=unit(signs);face=verts[np.isclose(verts@np.array(signs),3)]
        faces.append((normal,face,True))
    for axis in range(3):
        for sign in [-1,1]:
            normal=np.eye(3)[axis]*sign
            faces.append((normal,verts[np.isclose(verts@normal,2)],False))
    pose=rotation('y',-60)@rotation('x',35)
    for index,(normal,face,large) in enumerate(faces):
        fc=face.mean(axis=0);u=unit(face[0]-fc);v=np.cross(normal,u)
        angles=np.arctan2((face-fc)@v,(face-fc)@u)
        face=face[np.argsort(angles)]
        # Subdivide the panel's perimeter so its broad planes gently follow
        # the weight, while the bevels retain their physical depth.
        outline=np.array([a*(1-t)+b*t for a,b in zip(face,np.roll(face,-1,axis=0))
                          for t in np.linspace(0,1,4,endpoint=False)])
        rings=[(1.,30.4),(.965,30.5),(.92,31.15),(.87,31.45),(.76,31.45),
               (.72,30.82),(.675,30.82),(.635,31.25),(.05,31.3)] if large else [(1.,30.4),(.90,31.1),(.65,31.1),(.05,31.1)]
        mats=['seam','patina','edge','iron','iron','seam','edge','iron'] if large else ['seam','patina','iron']
        for k,((a,ra),(b,rb)) in enumerate(zip(rings,rings[1:])):
            outer=unit(fc+(outline-fc)*a)*ra
            inner=unit(fc+(outline-fc)*b)*rb
            p=np.vstack([outer,inner]);ln=len(outline)
            tri=[[j,(j+1)%ln,(j+1)%ln+ln] for j in range(ln)]
            tri += [[j,(j+1)%ln+ln,j+ln] for j in range(ln)]
            uv=np.c_[(p@u)/38+.5,(p@v)/38+.5]
            m.add('Cast plate %02d / recessed step %d'%(index,k),mats[k],p@pose.T+center,
                  unit(p)@pose.T,uv,tri)
        inner=unit(fc+(outline-fc)*rings[-1][0])*rings[-1][1]
        p=np.vstack([unit(fc)*31.3,inner]);ln=len(inner)
        m.add('Cast plate centre','iron',p@pose.T+center,unit(p)@pose.T,
              np.c_[(p@u)/38+.5,(p@v)/38+.5],[[0,j+1,(j+1)%ln+1] for j in range(ln)])
        if not large:
            start=len(m.parts)
            # The TP reference has sharp projecting spikes, not rounded bosses.
            # Flat triangular sides meet at one true apex above a square collar.
            corners=np.array([[-1,-1],[1,-1],[1,1],[-1,1]])
            spike=np.vstack([np.c_[corners[:,0]*5.8,np.zeros(4),corners[:,1]*5.8],
                             np.c_[corners[:,0]*5.1,np.full(4,1.1),corners[:,1]*5.1],
                             [[0,11.5,0]]])
            tri=[]
            for j in range(4):
                k=(j+1)%4
                tri.extend([[j,j+4,k+4],[j,k+4,k],[j+4,8,k+4]])
            tri.extend([[0,1,2],[0,2,3]])
            m.add('Sharp four-sided iron spike','steel',spike,unit(spike-[0,.5,0]),
                  spike[:,[0,2]]/12+.5,tri,True)
            # Local Y points radially out from the weight.
            basis=np.column_stack([u,normal,-v]);m.transform(pose@basis,offset=(normal*30.8)@pose.T+center,start=start)
    start=len(m.parts)
    m.lathe('Forged shackle base','iron',[(25,5.5),(28,5.5),(30,4.2)],8)
    m.transform(offset=[0,-5,-3],start=start)
    m.ring('Heavy anchor shackle','steel',[0,30,-3],[5.2,7.5],3.0,segments=20,sides=8)
    # Rectangular oval links with straight sides and thick forged cross-sections.
    link=[]
    for cx,cy,startangle in [(3.2,6,0),(-3.2,6,90),(-3.2,-6,180),(3.2,-6,270)]:
        for a in np.linspace(startangle,startangle+90,6,endpoint=False):
            link.append([cx+3*math.cos(math.radians(a)),cy+3*math.sin(math.radians(a)),0])
    link.append(link[0])
    centers=[(3,42,-3),(17,42,-4),(31,34,-3),(43,21,-4),(49,5,-3),
             (47,-12,-4),(38,-28,-3),(23,-40,-4),(5,-45,-3),(-13,-42,-4),
             (-29,-33,-3),(-40,-18,-4)]
    for j,c in enumerate(centers):
        start=len(m.parts);m.tube('Forged rectangular chain link %02d'%j,'steel',link,2.7,8,cap=False)
        tangent=np.array(centers[min(j+1,len(centers)-1)])-np.array(centers[max(j-1,0)])
        a=math.degrees(math.atan2(-tangent[0],tangent[1]))
        m.transform(rotation('z',a)@rotation('y',72 if j%2 else -8),offset=c,start=start)
    m.notes=['Visual POC3: six sharp four-sided iron spikes with true pointed tips replace the rounded bosses, following user correction of the TP reference. Forged iron plates and heavy rectangular links preserved.']
    return m

def leaf():
    m=model('deku_leaf');metals(m)
    m.material('leaf',[.20,.56,.055],'leaf',0,.77)
    m.material('vein',[.37,.65,.10],'wood',0,.65)
    # Pointed, asymmetrical lobes preserve Deku Leaf identity in silhouette.
    edge=[(0,53),(-5,39),(-16,45),(-15,31),(-30,36),(-25,22),(-40,23),(-33,9),(-43,4),(-32,-6),(-37,-18),(-22,-18),(-24,-31),(-10,-27),(0,-38),(10,-27),(25,-30),(23,-17),(38,-18),(32,-5),(43,5),(32,10),(40,25),(25,23),(29,37),(15,31),(17,46),(5,39)]
    # Triangulate radial sectors, bulging centre gives a curved physical leaf.
    p=[[0,4,5]]+[[x,y,1.5*math.sin(y/45*math.pi)-abs(x)*.015] for x,y in edge]
    uv=np.c_[(np.array(p)[:,0]+44)/88,(53-np.array(p)[:,1])/95]
    tri=[[0,j+1,(j+1)%len(edge)+1] for j in range(len(edge))]
    m.add('Cambered leaf','leaf',p,[[0,.08,1]]*len(p),uv,tri)
    m.add('Leaf underside','leaf',np.array(p)-[0,0,.5],[[0,-.08,-1]]*len(p),uv,tri)
    path=bezier([0,-50,1],[1,-15,7],[-2,22,6],[0,53,0],32)
    m.tube('Central stem','vein',path,np.linspace(1.5,.25,len(path)),7)
    for sign in [-1,1]:
        for y,x,endy in [(-24,24,-31),(-12,37,-18),(0,43,4),(11,40,23),(23,30,36),(34,16,45)]:
            path=bezier([0,y,5.3],[sign*x*.3,y+3,4.5],[sign*x*.8,endy-3,2],[sign*x,endy,1],12)
            m.tube('Leaf vein','vein',path,np.linspace(.65,.10,len(path)),5)
    m.notes=['Green shimmer/dust; supersedes the earlier blue suggestion. Physical camber and shaped lobes.']
    return m

def mitts():
    m=model('mogma_mitts');metals(m)
    m.material('leather',[.27,.15,.075],'leather',0,.9)
    m.material('red',[.66,.15,.065],'stone')
    m.material('green',[.23,.40,.08],'stone')
    im=Image.fromarray(surface('stone',[.86,.80,.64]));d=ImageDraw.Draw(im)
    for x in [64,192]:
        d.line([(x-23,58),(x,155),(x+24,58)],fill=(162,47,22,255),width=7)
        d.line([(x-23,165),(x-3,118),(x+22,165)],fill=(61,103,24,255),width=6)
    m.material('cuff_pattern',[1,1,1],tex=np.array(im),rough=.75)
    for sign in [-1,1]:
        start=len(m.parts)
        m.lathe('Hollow ivory cuff','cuff_pattern',[(-42,17),(-38,18),(-15,12),(-11,11)],16,False)
        m.lathe('Cuff lining','leather',[(-42,15.4),(-37,16),(-16,10)],16,False)
        band(m,'Cuff rim','gold',-41,17.8,1)
        m.sphere('Leather palm','leather',[0,1,0],[11.5,19,8],8,14)
        path=bezier([sign*8,-4,0],[sign*17,-4,0],[sign*19,7,0],[sign*17,11,1],14)
        m.tube('Curved thumb','leather',path,np.linspace(4.5,3,len(path)),8)
        for k in range(4):
            x=-8+k*5.3;height=28+(1-abs(k-1.5)/2)*8
            blade=[(x-2.2,6),(x+2.2,6),(x+3,height-8),(x+1,height+3),(x-1,height+7),(x-1.4,height-7)]
            m.polygon('Forged digging claw','steel',blade,3.2,z=6,bevel=.8)
            m.sphere('Claw anchor rivet','edge',[x,8,8],[1.4,1.4,.9],4,8)
        # Recognisable red / green geometric markings on both ivory cuffs.
        for face in [-1,1]:
            for x in [-12,12]:m.sphere('Cuff stud','steel',[x,-35,face*12],[1.6,1.6,1.1],4,8)
        m.transform(rotation('z',-sign*12),offset=[sign*25,0,0],start=start)
    m.notes=['Paired mitts with four metal digging claws each and red/green ivory cuff markings.']
    return m

def hook():
    m=model('switch_hook');metals(m)
    m.material('purple',[.28,.22,.70],'leather',0,.58)
    m.material('focus',[.38,.35,.96],'energy',.1,.23,emission=.2)
    m.lathe('Violet grip','purple',[(-48,5),(-43,7),(-22,7),(-18,5.5)],18)
    band(m,'Grip base','gold',-47,7.4,2);band(m,'Grip collar','gold',-20,7.8,2)
    path=bezier([0,-18,0],[4,-4,0],[-12,3,0],[-9,17,0],25)
    m.tube('Flexible steel neck','steel',path,3.3,10)
    for j in range(3,len(path)-1,4):m.ring('Neck seam','dark',path[j],3.4,.4,'y',16,5)
    m.sphere('Violet hub','focus',[-9,25,1],[9,9,6],8,16)
    m.ring('Hub bezel','gold',[-9,25,0],9.3,1.2,segments=24,sides=6)
    # Opposed crescent jaws, visibly open and separate from the hub.
    for phase in [0,math.pi]:
        t=np.linspace(-.5,1.8,17)+phase
        xy=np.c_[-9+np.cos(t)*23,25+np.sin(t)*23]
        inner=np.c_[-9+np.cos(t[::-1])*14,25+np.sin(t[::-1])*14]
        outline=np.vstack([xy,inner])
        m.polygon('Crescent switch jaw','steel',outline,3.8,bevel=1.1)
        m.tube('Crescent gold edge','gold',np.c_[xy,np.full(len(xy),1.3)],.65,5)
    m.transform(rotation('z',18));m.notes=['Open opposing crescent jaws, violet focus and grip, flexible segmented neck.']
    return m

def beetle():
    m=model('beetle');metals(m)
    m.material('gold',[.90,.71,.27],'ceramic',.8,.37)
    m.material('edge',[.99,.86,.45],'ceramic',.85,.33)
    m.material('teal',[.08,.61,.53],'ceramic',.55,.44)
    m.material('dark',[.12,.22,.20],'metal',.5,.57)
    m.material('motif',[.23,.36,.075],'metal',.25,.65)
    m.material('purple',[.43,.28,.39],'stone',.15,.68)
    stations=np.array([[-33,5,5],[-29,12,8],[-21,17,11],[-8,20,12.8],
                       [6,20,12.8],[18,17,11],[28,11,7.5],[31,4,3.5]],float)
    # Elongated, panelled chassis with a defined underside, rather than a spherical face.
    p=[];n=[];uv=[];tri=[];sides=24
    for i,(z,rx,ry) in enumerate(stations):
        for j in range(sides+1):
            a=TAU*j/sides
            p.append([rx*math.cos(a),ry*math.sin(a)-1,z]);n.append(unit([math.cos(a)/rx,math.sin(a)/ry,.003]))
            uv.append([j/sides,(z+33)/64])
    for i in range(len(stations)-1):
        for j in range(sides):
            a=i*(sides+1)+j;b=a+sides+1;tri.extend([[a,b,a+1],[a+1,b,b+1]])
    m.add('Elongated teal metal chassis','teal',p,n,uv,tri)
    for z,rx,ry in [stations[0],stations[-1]]:
        m.sphere('Recessed chassis end cap','dark' if z<0 else 'teal',[0,-1,z],[rx,ry,.6],4,12)
    # Cast brass frame follows the shell. The panel joins and fittings sit flush.
    for theta in [math.pi*1.5]:
        path=[[rx*math.cos(theta),ry*math.sin(theta)-1,z] for z,rx,ry in stations]
        m.tube('Longitudinal brass frame','gold',path,2.0,6)
    for z,rx,ry in [(-21,17,11),(16,17.7,11.5)]:
        m.ring('Structural shell collar','gold',[0,-1,z],[rx+.6,ry+.6],3.0 if z>0 else 1.9,'z',24,6)
    for sign in [-1,1]:
        path=[[sign*(rx+.5),-1,z] for z,rx,ry in stations]
        m.tube('Side chassis rail','gold',path,1.5,6)
        # Small six-sided recessed metal fasteners replace the bulbous eyes/pupils.
        start=len(m.parts)
        m.lathe('Flush hexagonal socket rim','gold',[(0,4.5),(.6,4.6),(1.1,3.7),(1.0,2.5),(.2,2.4)],6,False)
        m.lathe('Solid brass hex fastener','gold',[(.15,2.3),(.55,2.3)],6)
        m.transform(rotation('y',sign*23)@rotation('x',76),offset=[sign*8.5,6,28.2],start=start)
    def plate(name,mat,outline,depth,y=0,bevel=.7):
        start=len(m.parts)
        m.polygon(name,mat,[[x,-z] for x,z in outline],depth,bevel=bevel)
        m.transform(rotation('x',-90),offset=[0,y,0],start=start)
    for sign in [-1,1]:
        # Broad sharpened crescent plates, matching the mechanical claw silhouette.
        outer=bezier([7,20],[8,50],[30,57],[44,54],13)
        inner=bezier([44,54],[27,40],[26,26],[15,17],13)
        shape=np.vstack([outer,inner[1:]])*np.array([sign,1])
        plate('Bevelled crescent pincer','gold',shape,3.4,-1,1.3)
        rim=np.c_[outer[:,0]*sign,np.full(len(outer),.55),outer[:,1]]
        m.tube('Pincer polished cutting edge','edge',rim,.38,4)
        glyph=np.array([[15,30],[25,36],[29,45],[23,39],[20,39],[22,42],[18,39],[20,35],[15,33]])*np.array([sign,1])
        plate('Inset pincer sigil','purple',glyph,.12,.78,.01)
        # Squared, rigid blade wings on distinct axle blocks; no insect-like soft fins.
        start=len(m.parts)
        wing=[(-5,0),(6,0),(10,38),(7,45),(-8,45),(-7,12)]
        m.polygon('Rigid engraved wing blade','gold',wing,2.5,bevel=.9)
        path=bezier([-1,9,1.32],[11,16,1.32],[-13,22,1.32],[2,33,1.32],20)
        m.tube('Wing scroll inlay','motif',path,.95,5)
        m.polygon('Wing tip inset','motif',[(-5,37),(5,37),(5,41),(-5,41)],.15,z=1.25,bevel=.03)
        m.transform(rotation('z',-sign*37)@rotation('x',-30),offset=[sign*12,8,-15],start=start)
        start=len(m.parts)
        m.lathe('Wing hinge axle','gold',[(-4,3.5),(4,3.5)],8)
        m.transform(rotation('z',90),offset=[sign*13,8,-15],start=start)
        # Tucked angular brackets and small pins read as machinery, not cartoon feet.
        for z in [-15,1,14]:
            shape=np.array([[13,z-3],[22,z-3],[25,z+3],[20,z+5],[16,z+1]])*np.array([sign,1])
            plate('Underside jointed bracket','gold',shape,2.4,-10,.5)
            m.sphere('Bracket hinge pin','edge',[sign*19,-8.7,z-1],[1.3,.5,1.3],4,6)
    m.lathe('Rear coupling boss','gold',[(-2,4),(2,4)],8)
    # Show the top casing and broad pincers in the GI pose, preserving presentation scale.
    m.transform(rotation('x',50))
    m.notes=['Visual POC2: elongated mechanical chassis; flush hex sockets; rigid engraved wing blades; broad crescent pincers. Reference supplied by user.']
    return m

def gate():
    m=model('time_gate');metals(m)
    m.material('frame',[.25,.16,.40],'stone',.25,.65)
    ref=np.array(Image.open(ROOT/'REFERENCES'/'TimeGate.png').convert('RGBA'))
    # Use the supplied icon as the exact engraving reference, mapped onto the face.
    m.material('engraving',[1,1,1],tex=ref,emission=.25)
    m.material('rim',[.50,.39,.66],'metal',.55,.4)
    # Silhouette follows the reference's radial alpha boundary rather than a square card.
    h,w=ref.shape[:2];cy,cx=(h-1)/2,(w-1)/2;alpha=ref[:,:,3]
    outline=[]
    for a in np.linspace(0,TAU,144,endpoint=False):
        rr=np.arange(1,min(w,h)/2);xs=np.clip(np.rint(cx+np.cos(a)*rr).astype(int),0,w-1);ys=np.clip(np.rint(cy-np.sin(a)*rr).astype(int),0,h-1)
        hits=rr[alpha[ys,xs]>230];r=(hits[-1] if len(hits) else min(w,h)*.35)/min(w,h)*106
        outline.append([math.cos(a)*r,math.sin(a)*r])
    m.polygon('Carved gear body','frame',outline,depth=6,bevel=.6)
    for sign in [-1,1]:
        p=np.array([[0,0,sign*3.1]]+[[x,y,sign*3.1] for x,y in outline]);uv=np.c_[p[:,0]/106+.5,.5-p[:,1]/106]
        tri=[[0,j+1,(j+1)%len(outline)+1] for j in range(len(outline))]
        m.add('Icon-matched engraved face','engraving',p,[[0,0,sign]]*len(p),uv,tri)
        m.ring('Raised face rim','rim',[0,0,sign*3.2],40.6,.75,segments=64,sides=6)
    m.notes=['Upright. Both engraved faces use the supplied Time Gate icon; bevel and rim add depth.']
    return m

def jar():
    m=model('gust_jar');metals(m)
    m.material('ceramic',[.87,.90,.91],'ceramic',.05,.3)
    m.material('inside',[.21,.29,.38],'stone',0,.8)
    m.material('blue',[.018,.26,.64],'ceramic',.1,.3)
    # Repeating white curl motif on blue porcelain; UV wraps around the whole jar.
    im=Image.new('RGBA',(512,256),(8,69,165,255));d=ImageDraw.Draw(im)
    for j in range(8):
        t=np.linspace(0,math.pi*3.1,90);r=np.linspace(4,30,len(t));pts=list(zip(j*64+32+np.cos(t)*r,128+np.sin(t)*r*2.5))
        d.line(pts,fill=(223,237,246,255),width=8)
    m.material('wind_band',[1,1,1],tex=np.array(im),rough=.35)
    m.lathe('Porcelain body','ceramic',[(-42,18),(-38,23),(-31,29),(-20,33),(-5,34),(12,30),(23,21),(30,19)],32,False)
    m.lathe('Blue belly band','wind_band',[(-17,33.6),(-6,34.4),(6,32.5),(12,30.5)],32,False)
    m.lathe('Blue neck','blue',[(25,20),(29,20),(35,24)],32,False)
    m.lathe('Open flared lip','ceramic',[(33,23),(38,27),(41,28),(42,26),(39,23),(35,22),(29,18)],32,False)
    m.lathe('Recessed inner wall','inside',[(29,18),(24,19),(14,23),(-14,25),(-29,20)],32,False)
    m.lathe('Inside floor','inside',[(-31,.1),(-30,19)],24,True)
    m.lathe('Foot','blue',[(-45,18),(-43,22),(-40,22)],32)
    band(m,'Foot porcelain edge','ceramic',-43,22.7,1.1)
    for sign in [-1,1]:
        t=np.linspace(-1.4,1.45,30);path=np.c_[sign*(29+16*np.cos(t)),-3+22*np.sin(t),np.zeros(len(t))]
        m.tube('Loop handle','ceramic',path,4.2,10)
        m.sphere('Handle socket','blue',[sign*28,17,0],[5,5,5],5,10)
        # Three dimensional horn fins flare from the lip; understated white ornament.
        for k in range(3):
            path=bezier([sign*24,30,-1+k],[sign*34,29,0],[sign*(39+k*2),39+k*2,0],[sign*(33+k*3),47+k*1.5,0],13)
            m.tube('White lip fin','ceramic',path,np.linspace(2.5,.2,len(path)),6)
    m.notes=['Upright hollow vessel, twin loop handles, blue wind band, porcelain lip ornaments.']
    return m

def shovel():
    m=model('shovel');metals(m)
    m.material('wood',[.49,.15,.065],'wood',0,.67)
    worn=surface('metal',[.35,.46,.57]);rng=np.random.default_rng(92)
    im=Image.fromarray(worn);d=ImageDraw.Draw(im)
    for j in range(58):
        x,y=rng.integers(0,256,2);d.line([(int(x),int(y)),(int(x+4),int(y+13)),(int(x+1),int(y+24))],fill=(112,64,37,255),width=int(rng.integers(1,4)))
    m.material('worn_blade',[1,1,1],tex=np.array(im),metal=.8,rough=.43)
    m.lathe('Wood shaft','wood',[(-16,3),(28,3.5),(42,4),(49,6),(52,4)],16)
    m.crystal('Octagonal end cap','gold',[0,49,0],6.4,7,8)
    m.lathe('Steel neck','dark',[(-26,4.2),(-20,5),(-8,4.2)],16)
    for y in [-19,-8,32]:band(m,'Bronze collar','gold',y,4.8,1.25);studs(m,y,4.9,4)
    outline=[(-22,-18),(-14,-23),(-5,-22),(0,-20),(5,-22),(14,-23),(22,-18),(19,-36),(13,-47),(0,-57),(-13,-47),(-19,-36)]
    m.polygon('Original spade silhouette','worn_blade',outline,3.2,bevel=1.2)
    for sign in [-1,1]:
        m.tube('Bronze blade edge','gold',[[x,y,sign*1.4] for x,y in outline+[outline[0]]],.45,6)
        m.polygon('Raised central reinforcing rib','steel',[(-2,-19),(2,-19),(2,-38),(0,-56),(-2,-38)],2,z=sign*2,bevel=.7)
        for y in [-25,-31,-38]:m.sphere('Blade rivet','gold',[0,y,sign*3.2],[1.0,1.,.7],4,8)
    m.transform(rotation('z',-24));m.notes=['24 degree tilt, preserved spade outline, wood grain and worn steel with bronze fittings.']
    return m
