from pathlib import Path
import math
import struct
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw, ImageFont

HERE=Path(__file__).resolve().parent
OUT=HERE/'assets/objects/object_nei_ray_gun';OUT.mkdir(parents=True,exist_ok=True)
def save(root,name):
    ET.indent(root,space='    ');ET.ElementTree(root).write(OUT/name,encoding='unicode')
def texture(path,image):
    path.parent.mkdir(parents=True,exist_ok=True);raw=image.convert('RGBA').tobytes();h=bytearray(64);h[4:8]=b'XETO'
    struct.pack_into('<Q',h,12,0xDEADBEEFDEADBEEF);path.write_bytes(h+struct.pack('<IIII',1,image.width,image.height,len(raw))+raw)
def setup():
    dl=ET.Element('DisplayList',Version='0')
    ET.SubElement(dl,'Grayscale',Enabled='false')
    ET.SubElement(dl,'ClearGeometryMode',**{k:'1' for k in ['G_SHADE','G_LIGHTING','G_SHADING_SMOOTH','G_ZBUFFER','G_TEXTURE_GEN','G_TEXTURE_GEN_LINEAR','G_CULL_BACK','G_CULL_FRONT','G_FOG','G_CLIPPING']})
    ET.SubElement(dl,'SetGeometryMode',G_LIGHTING='1',G_SHADE='1',G_SHADING_SMOOTH='1',G_ZBUFFER='1',G_CULL_BACK='1',G_FOG='1')
    ET.SubElement(dl,'PipeSync')
    ET.SubElement(dl,'SetOtherMode',Cmd='G_SETOTHERMODE_H',Sft='4',Length='20',G_TF_BILERP='1',G_TC_FILT='1',G_CD_MAGICSQ='1',G_TT_NONE='1',G_TL_TILE='1',G_TP_PERSP='1',G_TD_CLAMP='1',G_CYC_2CYCLE='1',G_PM_NPRIMITIVE='1',G_CK_NONE='1',G_AD_NOISE='1')
    ET.SubElement(dl,'SetOtherMode',Cmd='G_SETOTHERMODE_L',Sft='0',Length='32',G_AC_NONE='1',G_RM_AA_ZB_OPA_SURF2='1',G_ZS_PIXEL='1',G_RM_FOG_SHADE_A='1')
    ET.SubElement(dl,'Texture',S='0',T='0',Level='0',Tile='0',On='0')
    ET.SubElement(dl,'SetCombineLERP',A0='G_CCMUX_0',B0='G_CCMUX_0',C0='G_CCMUX_0',D0='G_CCMUX_SHADE',Aa0='G_ACMUX_0',Ab0='G_ACMUX_0',Ac0='G_ACMUX_0',Ad0='G_ACMUX_1',A1='G_CCMUX_COMBINED',B1='G_CCMUX_0',C1='G_CCMUX_PRIMITIVE',D1='G_CCMUX_0',Aa1='G_ACMUX_0',Ab1='G_ACMUX_0',Ac1='G_ACMUX_0',Ad1='G_ACMUX_COMBINED')
    return dl
def mesh(dl,name,triangles,color):
    vertices=ET.Element('Vertex',Version='0')
    for triangle in triangles:
        a,b,c=triangle
        u=[b[i]-a[i] for i in range(3)];v=[c[i]-a[i] for i in range(3)]
        n=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]
        length=math.sqrt(sum(x*x for x in n)) or 1;n=[round(x/length*127)&255 for x in n]
        for p in triangle:ET.SubElement(vertices,'Vtx',X=str(round(p[0])),Y=str(round(p[1])),Z=str(round(p[2])),S='0',T='0',R=str(n[0]),G=str(n[1]),B=str(n[2]),A='255')
    save(vertices,name+'_vtx');ET.SubElement(dl,'PipeSync')
    ET.SubElement(dl,'SetPrimColor',M='0',L='0',R=str(color[0]),G=str(color[1]),B=str(color[2]),A='255')
    for offset in range(0,len(triangles)*3,30):
        count=min(30,len(triangles)*3-offset)
        ET.SubElement(dl,'LoadVertices',Path='objects/object_nei_ray_gun/'+name+'_vtx',VertexOffset=str(offset),Count=str(count),VertexBufferIndex='0')
        for i in range(0,count,3):ET.SubElement(dl,'Triangle1',V00=str(i),V01=str(i+1),V02=str(i+2),Flag0='0')
def box(x0,y0,z0,x1,y1,z1):
    p=[(x,y,z) for x in (x0,x1) for y in(y0,y1) for z in(z0,z1)]
    faces=[(0,1,3,2),(4,6,7,5),(0,4,5,1),(2,3,7,6),(0,2,6,4),(1,5,7,3)]
    return [(p[a],p[b],p[c]) for a,b,c,d in faces]+[(p[a],p[c],p[d]) for a,b,c,d in faces]
def tube(radius,z0,z1,count=12):
    triangles=[]
    for i in range(count):
        a,b=2*math.pi*i/count,2*math.pi*(i+1)/count
        p=(radius*math.cos(a),radius*math.sin(a),z0);q=(radius*math.cos(b),radius*math.sin(b),z0)
        r=(q[0],q[1],z1);s=(p[0],p[1],z1)
        triangles.extend([(p,q,r),(p,r,s),((0,0,z1),s,r),((0,0,z0),q,p)])
    return triangles
dl=setup();mesh(dl,'body',box(-250,-200,-300,250,200,800),(65,160,130))
mesh(dl,'grip',box(-180,-950,-220,180,-150,160),(95,55,35))
mesh(dl,'barrel',tube(150,650,1450),(220,175,55));mesh(dl,'muzzle',tube(210,1300,1470),(245,210,90))
mesh(dl,'sight',box(-70,200,200,70,340,850),(65,70,85));mesh(dl,'cell',tube(110,-450,-250),(65,235,235))
ET.SubElement(dl,'EndDisplayList');save(dl,'gun_dl')
beam=setup();ET.SubElement(beam,'ClearGeometryMode',G_LIGHTING='1',G_CULL_BACK='1')
combine=beam.find('SetCombineLERP');combine.set('D0','G_CCMUX_PRIMITIVE');combine.set('A1','G_CCMUX_0');combine.set('C1','G_CCMUX_0');combine.set('D1','G_CCMUX_COMBINED')
mesh(beam,'bolt',tube(100,-220,220,6),(80,255,235));ET.SubElement(beam,'EndDisplayList');save(beam,'bolt_dl')
name=Image.new('RGBA',(128,16));draw=ImageDraw.Draw(name);font=ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf',14)
draw.text((35,-2),'Ray Gun',font=font,fill=(255,255,255,255));texture(HERE/'assets/textures/item_name_custom/gRayGunNameTex',name)
icon=Image.new('RGBA',(32,32));d=ImageDraw.Draw(icon)
d.polygon([(3,10),(22,6),(27,10),(26,15),(10,19),(8,28),(3,28),(5,17)],fill='#3d9c88',outline='#202e31')
d.polygon([(19,7),(29,4),(31,7),(30,12),(24,15)],fill='#dcb045',outline='#72552c')
d.polygon([(5,17),(10,18),(8,28),(3,28)],fill='#74452f',outline='#202e31')
d.line([(6,10),(21,7)],fill='#9de9cf',width=2)
texture(HERE/'assets/textures/icon_item_custom/gRayGunIconTex',icon)
print('Ray Gun model, bolt, icon and name baked')
