"""Export editable GLBs and render the actual candidate triangles offline."""
import io,json,math,struct
import numpy as np
from PIL import Image,ImageDraw,ImageFont
from meshkit import ROOT,Q,unit,rotation

def glb(m,path):
    blob=bytearray();views=[];accessors=[];images=[];textures=[];materials=[]
    def view(data,target=None):
        blob.extend(b'\0'*((-len(blob))%4));v=dict(buffer=0,byteOffset=len(blob),byteLength=len(data))
        if target:v['target']=target
        views.append(v);blob.extend(data);return len(views)-1
    def acc(a,typ,ct):
        i=view(a.tobytes(),34963 if typ=='SCALAR' else 34962)
        v=dict(bufferView=i,componentType=ct,count=len(a),type=typ)
        if typ=='VEC3':v.update(min=a.min(axis=0).tolist(),max=a.max(axis=0).tolist())
        accessors.append(v);return len(accessors)-1
    names=list(m.materials)
    for name,mat in m.materials.items():
        pbr=dict(baseColorFactor=mat['color']+[mat['alpha']],metallicFactor=mat['metal'],roughnessFactor=mat['rough'])
        if mat['tex'] is not None:
            buf=io.BytesIO();Image.fromarray(mat['tex']).save(buf,format='PNG')
            images.append(dict(bufferView=view(buf.getvalue()),mimeType='image/png'))
            textures.append(dict(source=len(images)-1,sampler=0));pbr['baseColorTexture']=dict(index=len(textures)-1)
        item=dict(name=name,pbrMetallicRoughness=pbr,doubleSided=False)
        if mat['alpha']<1:item['alphaMode']='BLEND'
        if mat['emission']:
            item['emissiveFactor']=[mat['emission']]*3
            if 'baseColorTexture' in pbr:item['emissiveTexture']=pbr['baseColorTexture']
        materials.append(item)
    meshes=[];nodes=[]
    for p in m.parts:
        primitive=dict(attributes=dict(POSITION=acc((p['p']*Q).astype('<f4'),'VEC3',5126),NORMAL=acc(p['n'].astype('<f4'),'VEC3',5126),TEXCOORD_0=acc(p['uv'].astype('<f4'),'VEC2',5126)),indices=acc(p['tri'].ravel().astype('<u4'),'SCALAR',5125),material=names.index(p['mat']))
        meshes.append(dict(name=p['name'],primitives=[primitive]));nodes.append(dict(name=p['name'],mesh=len(meshes)-1))
    root=dict(name=m.name,children=list(range(len(nodes))),scale=[m.native_scale]*3)
    nodes.append(root)
    doc=dict(asset=dict(version='2.0',generator='NEI unified GI upgrade / exact exported geometry'),buffers=[dict(byteLength=len(blob))],bufferViews=views,accessors=accessors,materials=materials,images=images,textures=textures,samplers=[dict(magFilter=9729,minFilter=9729,wrapS=10497,wrapT=10497)],meshes=meshes,nodes=nodes,scenes=[dict(nodes=[len(nodes)-1])],scene=0)
    js=json.dumps(doc,separators=(',',':')).encode();js+=b' '*((-len(js))%4);blob+=b'\0'*((-len(blob))%4)
    body=struct.pack('<I4s',len(js),b'JSON')+js+struct.pack('<I4s',len(blob),b'BIN\0')+blob
    path.write_bytes(struct.pack('<4sII',b'glTF',2,len(body)+12)+body)

def render(m,angle=25,size=480,elevation=12):
    rot=rotation('x',elevation)@rotation('y',angle)
    allp=np.concatenate([p['p'] for p in m.parts]);center=(allp.min(axis=0)+allp.max(axis=0))/2
    # Fixed per-model fit for all turntable angles.
    radius=max(np.linalg.norm((allp-center)[:,[0,2]],axis=1).max(),np.ptp(allp[:,1])/2)
    scale=size/(radius*2.75)
    pix=np.zeros((size,size,3),float);yy=np.linspace(0,1,size)[:,None,None]
    pix[:]=np.array([22,27,34])+(1-yy)*np.array([9,10,12])
    zb=np.full((size,size),-1e12);light=unit([-.45,.70,.62])
    parts=sorted(m.parts,key=lambda p:m.materials[p['mat']]['alpha']<1)
    for part in parts:
        pos=(part['p']-center)@rot.T;norm=part['n']@rot.T
        screen=np.c_[size*.5+pos[:,0]*scale,size*.52-pos[:,1]*scale]
        mat=m.materials[part['mat']];tex=mat['tex']
        for face in part['tri']:
            if np.cross(pos[face[1]]-pos[face[0]],pos[face[2]]-pos[face[0]])[2]<=0:continue
            v=screen[face];z=pos[face,2];nn=norm[face];x0,y0=v[0];x1,y1=v[1];x2,y2=v[2]
            area=(x1-x0)*(y2-y0)-(x2-x0)*(y1-y0)
            if abs(area)<1e-8:continue
            xmin=max(0,int(np.floor(v[:,0].min())));xmax=min(size-1,int(np.ceil(v[:,0].max())))
            ymin=max(0,int(np.floor(v[:,1].min())));ymax=min(size-1,int(np.ceil(v[:,1].max())))
            if xmax<xmin or ymax<ymin:continue
            xx,yy=np.meshgrid(np.arange(xmin,xmax+1)+.5,np.arange(ymin,ymax+1)+.5)
            w0=((x1-xx)*(y2-yy)-(x2-xx)*(y1-yy))/area;w1=((x2-xx)*(y0-yy)-(x0-xx)*(y2-yy))/area;w2=1-w0-w1
            depth=w0*z[0]+w1*z[1]+w2*z[2];tz=zb[ymin:ymax+1,xmin:xmax+1]
            mask=(w0>=0)&(w1>=0)&(w2>=0)&(depth>tz)
            if not mask.any():continue
            weights=np.stack([w0[mask],w1[mask],w2[mask]],axis=1);normals=unit(weights@nn)
            diff=np.maximum(normals@light,0);shade=.32+.68*diff
            if tex is None:base=np.broadcast_to(np.array(mat['color'])*255,(len(weights),3)).copy()
            else:
                uv=weights@part['uv'][face];h,w=tex.shape[:2]
                tx=np.mod(uv[:,0]*w-.5,w);ty=np.mod(uv[:,1]*h-.5,h);ix=np.floor(tx).astype(int);iy=np.floor(ty).astype(int);fx=(tx-ix)[:,None];fy=(ty-iy)[:,None]
                base=(tex[iy,ix,:3]*(1-fx)+tex[iy,(ix+1)%w,:3]*fx)*(1-fy)+(tex[(iy+1)%h,ix,:3]*(1-fx)+tex[(iy+1)%h,(ix+1)%w,:3]*fx)*fy
            if mat['emission']:shade=shade*(1-mat['emission'])+mat['emission']
            half=unit(light+[0,0,1]);spec=np.maximum(normals@half,0)**(8+mat['rough']*40)*(32 if mat['metal'] else 9)
            colors=base*shade[:,None]+spec[:,None];alpha=mat['alpha'];target=pix[ymin:ymax+1,xmin:xmax+1]
            target[mask]=colors*alpha+target[mask]*(1-alpha)
            if alpha>=1:tz[mask]=depth[mask]
    return Image.fromarray(np.uint8(np.clip(pix,0,255)))

def checkpoint(m,stats):
    d=ROOT/'CHECKPOINTS'/m.slug;d.mkdir(parents=True,exist_ok=True)
    glb(m,d/(m.slug+'.glb'))
    positions=np.concatenate([p['p'] for p in m.parts]);stats.update(slug=m.slug,name=m.name,entry=m.entry,draw_scale=m.draw_scale,matrix_scale=m.native_scale,triangles=sum(len(p['tri']) for p in m.parts),bounds_author=[positions.min(axis=0).tolist(),positions.max(axis=0).tolist()],markers=m.markers,notes=m.notes,runtime_tested=False)
    (d/'checkpoint.json').write_text(json.dumps(stats,indent=2)+'\n')
    render(m).save(d/'front.png');render(m,150).save(d/'back.png')
    print(json.dumps(dict(checkpoint=m.slug,triangles=stats['triangles'],loads=stats['vertex_loads'])),flush=True)
