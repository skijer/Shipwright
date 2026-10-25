from pathlib import Path
import math
import struct
import xml.etree.ElementTree as ET
from PIL import Image,ImageDraw,ImageFont
HERE=Path(__file__).resolve().parent
OUT=HERE/'assets/objects/object_nei_possession';OUT.mkdir(parents=True,exist_ok=True)
vertices=ET.Element('Vertex',Version='0');triangles=[]
def triangle(a,b,c,color):
    offset=len(vertices)
    for p in (a,b,c):ET.SubElement(vertices,'Vtx',X=str(round(p[0])),Y=str(round(p[1])),Z=str(round(p[2])),S='0',T='0',R=str(color[0]),G=str(color[1]),B=str(color[2]),A='255')
    triangles.append(offset)
for i in range(16):
    a,b=math.tau*i/16,math.tau*(i+1)/16
    p=(600*math.cos(a),0,520*math.sin(a));q=(600*math.cos(b),0,520*math.sin(b))
    r=(390*math.cos(a),300,340*math.sin(a));s=(390*math.cos(b),300,340*math.sin(b))
    triangle(p,q,s,(180,25,40));triangle(p,s,r,(220,35,50));triangle(r,s,(0,410,0),(240,55,65))
triangle((-500,0,250),(500,0,250),(650,-30,1000),(160,15,30));triangle((-500,0,250),(650,-30,1000),(-650,-30,1000),(200,25,40))
# Eyes on the front of the crown: a simple distinct possession cap, not a borrowed game asset.
for x in(-150,150):
    triangle((x-100,100,455),(x+100,100,455),(x+100,310,390),(255,255,245))
    triangle((x-100,100,455),(x+100,310,390),(x-100,310,390),(255,255,245))
    triangle((x-40,130,460),(x+40,130,460),(x+40,265,420),(25,35,45))
    triangle((x-40,130,460),(x+40,265,420),(x-40,265,420),(25,35,45))
ET.indent(vertices,space='    ');ET.ElementTree(vertices).write(OUT/'cap_vtx',encoding='unicode')
dl=ET.Element('DisplayList',Version='0')
ET.SubElement(dl,'ClearGeometryMode',G_LIGHTING='1',G_CULL_BACK='1',G_CULL_FRONT='1',G_TEXTURE_GEN='1',G_TEXTURE_GEN_LINEAR='1')
ET.SubElement(dl,'SetGeometryMode',G_SHADE='1',G_SHADING_SMOOTH='1',G_ZBUFFER='1')
ET.SubElement(dl,'Texture',S='0',T='0',Level='0',Tile='0',On='0')
ET.SubElement(dl,'SetCombineLERP',A0='G_CCMUX_0',B0='G_CCMUX_0',C0='G_CCMUX_0',D0='G_CCMUX_SHADE',Aa0='G_ACMUX_0',Ab0='G_ACMUX_0',Ac0='G_ACMUX_0',Ad0='G_ACMUX_1',A1='G_CCMUX_0',B1='G_CCMUX_0',C1='G_CCMUX_0',D1='G_CCMUX_COMBINED',Aa1='G_ACMUX_0',Ab1='G_ACMUX_0',Ac1='G_ACMUX_0',Ad1='G_ACMUX_COMBINED')
for offset in range(0,len(vertices),30):
    count=min(30,len(vertices)-offset);ET.SubElement(dl,'LoadVertices',Path='objects/object_nei_possession/cap_vtx',VertexOffset=str(offset),Count=str(count),VertexBufferIndex='0')
    for i in range(0,count,3):ET.SubElement(dl,'Triangle1',V00=str(i),V01=str(i+1),V02=str(i+2),Flag0='0')
ET.SubElement(dl,'EndDisplayList');ET.indent(dl,space='    ');ET.ElementTree(dl).write(OUT/'cap_dl',encoding='unicode')
def texture(path,image):
    path.parent.mkdir(parents=True,exist_ok=True);h=bytearray(64);h[4:8]=b'XETO';struct.pack_into('<Q',h,12,0xDEADBEEFDEADBEEF)
    raw=image.convert('RGBA').tobytes();path.write_bytes(h+struct.pack('<IIII',1,image.width,image.height,len(raw))+raw)
image=Image.new('RGBA',(32,32));d=ImageDraw.Draw(image)
d.ellipse((4,3,29,25),fill='#d5283b',outline='#66182a');d.polygon([(6,17),(24,17),(31,26),(4,29),(0,24)],fill='#a81b31')
d.ellipse((12,8,19,19),fill='white');d.ellipse((20,8,27,19),fill='white');d.ellipse((15,11,18,17),fill='#14222f');d.ellipse((23,11,26,17),fill='#14222f')
texture(HERE/'assets/textures/icon_item_custom/gPossessionCapIconTex',image)
image=Image.new('RGBA',(128,16));d=ImageDraw.Draw(image);font=ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf',13)
d.text((12,-1),'Possession Cap',font=font,fill='white');texture(HERE/'assets/textures/item_name_custom/gPossessionCapNameTex',image)
print('Possession cap model, icon and name baked')
