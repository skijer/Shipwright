from pathlib import Path
import hashlib,json,struct
ROOT=Path(__file__).resolve().parents[2]
manifest=json.loads((ROOT/'tools/nei_used_fx/material_manifest.json').read_text())
assets=ROOT/'soh/assets/custom/objects/nei_used_magic'
assert {p.name for p in assets.iterdir()}==set(manifest)
for name,m in manifest.items():
 b=(assets/name).read_bytes()
 assert hashlib.sha256(b).hexdigest()==m['resource_sha256']
 endian,kind,version=struct.unpack_from('<III',b)
 assert (endian,kind,version)==(0,0x4F544558,1)
 fmt,w,h,flags,hs,vs,size=struct.unpack_from('<IIIIffI',b,64)
 assert (fmt,flags)==(1,1) and size==w*h*4 and len(b)==92+size
 assert (hs,vs)==(w/32,h/32) and [w,h]==m['pixels']
 alpha=b[95::4]
 if name=='ice_fracture':assert min(alpha)==max(alpha)==255
 else:assert min(alpha)==0 and max(alpha)>240
print('USED private materials: native RGBA32, correct logical-tile scale, alpha and source/resource hashes passed')
