"""New attack artwork must remain independent from all existing spell assets."""
from pathlib import Path
import hashlib,json,struct
from PIL import Image
ROOT=Path(__file__).resolve().parents[2]
manifest=json.loads((ROOT/'tools/nei_feedback_preview/attack_material_manifest.json').read_text())
assets=ROOT/'soh/assets/custom/objects/nei_rod_attack'
# Explicitly accepted in review02; a release-art rebuild must not reauthor it.
assert hashlib.sha256((assets/'fire_surge').read_bytes()).hexdigest()=='824c9501c9702710636e056bb463e476d81f2680cef710e3936cd01e60335463'
assert {p.name for p in assets.iterdir()}==set(manifest)=={'fire_surge','frost_surge','light_surge',
                                                        'fire_release_flow','ice_release_flow','fire_release_crest','ice_release_crest'}
for name,m in manifest.items():
 source=ROOT/m['source'];raw=(assets/name).read_bytes()
 assert hashlib.sha256(source.read_bytes()).hexdigest()==m['source_sha256']
 assert hashlib.sha256(raw).hexdigest()==m['resource_sha256']
 assert struct.unpack_from('<III',raw)==(0,0x4F544558,1)
 assert struct.unpack_from('<IIIIffI',raw,64)==(1,512,512,1,16.,16.,512*512*4)
 expected=Image.open(source).convert('RGBA').resize((512,512),Image.Resampling.LANCZOS).tobytes()
 assert raw[92:]==expected # source alpha/color survives packaging
 assert min(raw[95::4])==0 and max(raw[95::4])==255
print('PASS original attack artwork hashes, native texture layout and authored RGBA; no medallion artwork input')
