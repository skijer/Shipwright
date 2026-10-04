"""Convert supplied medallion reference RGBA to private, native RGBA32 materials.

Only alpha and monochrome contrast are converted. No global paths are written.
32x32 logical tiles use native resource width/height scale (same as GI materials).
"""
import argparse, hashlib, json, struct, zipfile
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
DEST = ROOT / 'soh/assets/custom/objects/nei_used_magic'
INPUTS = {
 'ice_fracture':'alt/custom/medallion_magic/spells/water/sTex',
 'fire_wisp':'alt/custom/medallion_magic/arrows/spirit/s1Tex',
 'light_rays':'alt/overlays/ovl_Arrow_Light/s1Tex',
}
def header():
 h=bytearray(64);struct.pack_into('<IIIQQ',h,0,0,0x4F544558,1,0xDEADBEEFDEADBEEF,1<<32);return bytes(h)
def build(source):
 DEST.mkdir(parents=True,exist_ok=True);manifest={}
 with zipfile.ZipFile(source) as z:
  for slug,key in INPUTS.items():
   raw=z.read(key);kind,w,h,flags=struct.unpack_from('<IIII',raw,64)
   assert flags==3 and len(raw)==92+w*h*4
   rgba=np.frombuffer(raw[92:],np.uint8).reshape(h,w,4)
   intensity=np.max(rgba[:,:,:3],axis=2)/255.
   tex=np.full((h,w,4),255,np.uint8)
   if slug=='ice_fracture':
    tex[:,:,:3]=np.rint((.45+.55*intensity[:,:,None])*255)
   else:
    # White texels receive element tint from production mesh. Premultiplying
    # here would multiply intensity twice under standard alpha blending.
    tex[:,:,3]=np.rint(np.clip(intensity**1.15,0,1)*255)
   resource=header()+struct.pack('<IIIIffI',1,w,h,1,w/32,h/32,w*h*4)+tex.tobytes()
   (DEST/slug).write_bytes(resource)
   manifest[slug]={'source_entry':key,'source_sha256':hashlib.sha256(raw).hexdigest(),
                   'resource_sha256':hashlib.sha256(resource).hexdigest(),'pixels':[w,h],
                   'logical_tile':[32,32],'resource_kind':1,'resource_flags':1}
 (Path(__file__).with_name('material_manifest.json')).write_text(json.dumps(manifest,indent=2)+'\n')
 print('Built',len(manifest),'private material resources')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('reference_pack',type=Path);a=p.parse_args();build(a.reference_pack)
