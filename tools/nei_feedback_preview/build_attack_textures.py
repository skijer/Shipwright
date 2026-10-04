"""Package original RGBA spell artwork; preserve alpha, no Henriko/medallion inputs."""
import hashlib,json,struct
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[2]
DEST=ROOT/'soh/assets/custom/objects/nei_rod_attack'
DEST.mkdir(parents=True,exist_ok=True)
manifest={}
for name in ('fire_surge','frost_surge','light_surge','fire_release_flow','ice_release_flow','fire_release_crest','ice_release_crest'):
 source=Path(__file__).with_name('art')/(name+'.png')
 im=Image.open(source).convert('RGBA').resize((512,512),Image.Resampling.LANCZOS)
 header=bytearray(64)
 struct.pack_into('<IIIQQ',header,0,0,0x4F544558,1,0xDEADBEEFDEADBEEF,1<<32)
 raw=header+struct.pack('<IIIIffI',1,512,512,1,16,16,512*512*4)+im.tobytes()
 (DEST/name).write_bytes(raw)
 manifest[name]={'source':str(source.relative_to(ROOT)),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
 'resource_sha256':hashlib.sha256(raw).hexdigest(),'pixels':[512,512],'logical_tile':[32,32],
 'origin':'Original generated RGBA artwork; no medallion/Henriko texture source; authored alpha preserved'}
Path(__file__).with_name('attack_material_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
