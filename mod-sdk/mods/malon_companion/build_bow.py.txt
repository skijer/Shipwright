"""Convert Luv4os' CC0 Romani bow into native XML display lists and PNG textures."""
from pathlib import Path
import re
import struct
import xml.etree.ElementTree as ET
from PIL import Image

HERE = Path(__file__).resolve().parent
OUT = HERE/'assets/objects/object_nei_malon'
PREFIX = 'objects/object_nei_malon/'

def save(tree, name):
    ET.indent(tree, space='    ')
    ET.ElementTree(tree).write(OUT/name, encoding='unicode')

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    text = (HERE/'reference/romanibow_model.c.txt').read_text()
    arrays = {}
    for name,body in re.findall(r'u64\s+(\w+)\[\]\s*=\s*\{(.*?)\};',text,re.S):
        arrays[name] = b''.join(int(v,16).to_bytes(8,'big') for v in re.findall(r'0x[0-9a-fA-F]+',body))
    textures = [('limb',8),('handle',16)]
    for label,width in textures:
        name = f'gRomaniBowDL_mesh_romani_bow_{label}_ci8_png_001'
        indices, palette = arrays[name+'_ci8'], arrays[name+'_pal_rgba16']
        colors = struct.unpack('>'+str(len(palette)//2)+'H',palette)
        rgba = bytes(channel for index in indices for channel in
                     (((colors[index]>>11)&31)*255//31,((colors[index]>>6)&31)*255//31,
                      ((colors[index]>>1)&31)*255//31,255 if colors[index]&1 else 0))
        # Pre-encode the texture; packaging also works with the system Python without Pillow.
        header = bytearray(64); header[4:8] = b'XETO'
        struct.pack_into('<Q',header,12,0xDEADBEEFDEADBEEF)
        (OUT/f'bow_{label}').write_bytes(header+struct.pack('<IIII',1,width,width,len(rgba))+rgba)
        old = OUT/f'bow_{label}.rgba32.png'
        if old.exists(): old.unlink()
    for name, body in re.findall(r'Vtx\s+(\w+)\[\d+\]\s*=\s*\{(.*?)\};',text,re.S):
        if name.endswith('cull'): continue
        root = ET.Element('Vertex',Version='0')
        for line in body.splitlines():
            nums = [int(v) for v in re.findall(r'-?\d+',line)]
            if not nums: continue
            assert len(nums) == 10
            x,y,z,flag,s,t,r,g,b,a = nums
            ET.SubElement(root,'Vtx',**dict(zip(('X','Y','Z','S','T','R','G','B','A'),map(str,(x,y,z,s,t,r,g,b,255)))))
        save(root,name)
    dl = ET.Element('DisplayList',Version='0')
    ET.SubElement(dl,'ClearGeometryMode',**{k:'1' for k in ['G_SHADE','G_LIGHTING','G_SHADING_SMOOTH','G_ZBUFFER','G_TEXTURE_GEN','G_TEXTURE_GEN_LINEAR','G_CULL_BACK','G_CULL_FRONT','G_FOG']})
    ET.SubElement(dl,'SetGeometryMode',G_SHADE='1',G_LIGHTING='1',G_SHADING_SMOOTH='1',G_ZBUFFER='1',G_FOG='1')
    ET.SubElement(dl,'SetOtherMode',Cmd='G_SETOTHERMODE_H',Sft='4',Length='20',G_TF_BILERP='1',G_TC_FILT='1',G_CD_MAGICSQ='1',G_TT_NONE='1',G_TL_TILE='1',G_TP_PERSP='1',G_TD_CLAMP='1',G_CYC_2CYCLE='1',G_PM_NPRIMITIVE='1',G_CK_NONE='1',G_AD_NOISE='1')
    ET.SubElement(dl,'SetOtherMode',Cmd='G_SETOTHERMODE_L',Sft='0',Length='32',G_AC_NONE='1',G_RM_AA_ZB_OPA_SURF2='1',G_ZS_PIXEL='1',G_RM_FOG_SHADE_A='1')
    for part,(label,width) in enumerate(textures+[('string',0)]):
        ET.SubElement(dl,'PipeSync')
        ET.SubElement(dl,'SetPrimColor',M='0',L='0',R='255',G='255',B='255',A='255')
        fields = dict(A0='G_CCMUX_TEXEL0' if width else 'G_CCMUX_0',B0='G_CCMUX_0',C0='G_CCMUX_SHADE',D0='G_CCMUX_0' if width else 'G_CCMUX_SHADE',Aa0='G_ACMUX_0',Ab0='G_ACMUX_0',Ac0='G_ACMUX_0',Ad0='G_ACMUX_1',A1='G_CCMUX_COMBINED',B1='G_CCMUX_0',C1='G_CCMUX_PRIMITIVE',D1='G_CCMUX_0',Aa1='G_ACMUX_0',Ab1='G_ACMUX_0',Ac1='G_ACMUX_0',Ad1='G_ACMUX_COMBINED')
        ET.SubElement(dl,'SetCombineLERP',**fields)
        ET.SubElement(dl,'Texture',S='65535',T='65535',Level='0',Tile='0',On='1' if width else '0')
        if width:
            ET.SubElement(dl,'LoadTextureBlock',Path=PREFIX+'bow_'+label,Format='0',Size='3',Width=str(width),Height=str(width),MaskS=str(width.bit_length()-1),MaskT=str(width.bit_length()-1),ShiftS='0',ShiftT='0',CMS_TXNoMirror='1',CMS_TXWrap='1',CMT_TXNoMirror='1',CMT_TXWrap='1')
        body = re.search(r'Gfx\s+\w+_tri_'+str(part)+r'\[\]\s*=\s*\{(.*?)\};',text,re.S)[1]
        for line in body.splitlines():
            match = re.search(r'gsSPVertex\((\w+)\s*\+\s*(\d+),\s*(\d+),\s*(\d+)\)',line)
            if match:
                name,offset,count,index = match.groups()
                ET.SubElement(dl,'LoadVertices',Path=PREFIX+name,VertexOffset=offset,Count=count,VertexBufferIndex=index)
            match = re.search(r'gsSP(?:1Triangle|2Triangles)\(([^)]+)\)',line)
            if match:
                nums = [v.strip() for v in match[1].split(',')]
                for i in range(0,len(nums),4):
                    ET.SubElement(dl,'Triangle1',V00=nums[i],V01=nums[i+1],V02=nums[i+2],Flag0=nums[i+3])
    ET.SubElement(dl,'EndDisplayList')
    save(dl,'bow_dl')
    print('Bow converted:',len(dl.findall('Triangle1')),'triangles; two original textures')

if __name__ == '__main__': main()
