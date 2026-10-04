"""Checkpoint each completed model; package only once with --package."""
import argparse,json,zipfile,shutil,struct,io
import numpy as np
from PIL import Image
from pathlib import Path
from meshkit import ROOT,export_resources
from preview import checkpoint
import models
import lantern

def whip():
    """Import the previously validated Whip A checkpoint, preserving its triangles."""
    b=(ROOT/'REFERENCES'/'Whip_A.glb').read_bytes();n=struct.unpack_from('<I',b,12)[0]
    g=json.loads(b[20:20+n]);binary=b[28+n:]
    def acc(i):
        a=g['accessors'][i];v=g['bufferViews'][a['bufferView']];cols={'VEC3':3,'VEC2':2,'SCALAR':1}[a['type']]
        return np.frombuffer(binary,dtype={5126:'<f4',5125:'<u4'}[a['componentType']],count=a['count']*cols,offset=v.get('byteOffset',0)).reshape(a['count'],cols)
    from meshkit import Model
    m=Model('whip','Whip A','objects/nei_gi_redesign/whip/gi_dl',.5,.5)
    for mat in g['materials']:
        pbr=mat['pbrMetallicRoughness'];tex=None
        if 'baseColorTexture' in pbr:
            t=g['textures'][pbr['baseColorTexture']['index']];im=g['images'][t['source']];v=g['bufferViews'][im['bufferView']]
            tex=np.array(Image.open(io.BytesIO(binary[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']])).convert('RGBA'))
        m.material(mat['name'],pbr['baseColorFactor'][:3],metal=pbr['metallicFactor'],rough=pbr['roughnessFactor'],tex=tex)
    for i,p in enumerate(g['meshes'][0]['primitives']):
        a=p['attributes'];m.add('Approved coiled whip part '+str(i),g['materials'][p['material']]['name'],acc(a['POSITION']),acc(a['NORMAL']),acc(a['TEXCOORD_0']),acc(p['indices']).reshape(-1,3))
    m.notes=['Whip A checkpoint, based on skeijer unreleased screenshot; independently reconstructed, not his source mesh.']
    return m

BUILDERS={
 'lantern':lantern.build,
 'whip':whip,
 'fire_rod':lambda:models.rod('fire_rod'), 'ice_rod':lambda:models.rod('ice_rod'), 'light_rod':lambda:models.rod('light_rod'),
 'hylia_grace':lambda:models.spell('hylia_grace'), 'zonai_permafrost':lambda:models.spell('zonai_permafrost'), 'demise_destruction':lambda:models.spell('demise_destruction'),
 'rocs_feather':models.feather,'time_gate':models.gate,'gust_jar':models.jar,'shovel':models.shovel,
 'ball_and_chain':models.ball_chain,'deku_leaf':models.leaf,'mogma_mitts':models.mitts,'switch_hook':models.hook,'beetle':models.beetle,
}
def main():
    p=argparse.ArgumentParser();p.add_argument('items',nargs='*');p.add_argument('--package',action='store_true');args=p.parse_args()
    for slug in args.items or BUILDERS:
        m=BUILDERS[slug]();stats=export_resources(m);checkpoint(m,stats)
    if args.package:
        out=ROOT/'NEI_GI_Upgrade.o2r'
        with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
            for path in sorted((ROOT/'RESOURCES').rglob('*')):
                if not path.is_file():continue
                rel=path.relative_to(ROOT/'RESOURCES').as_posix();data=path.read_bytes()
                z.writestr(rel,data);z.writestr('alt/'+rel,data)
        print('Combined archive: '+str(out))
if __name__=='__main__':main()
