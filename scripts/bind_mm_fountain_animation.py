#!/usr/bin/env python3
"""Bind native MM fountain motion to verified existing Prelude water, without changing art.

Only complete, ordered fountain families are recognized. Integer export coordinates
permit at most one world unit of residual after a common upright similarity fit.
Unknown/remeshed/ambiguous families fail closed. This does not repair missing draws.
"""
import argparse
import copy
import hashlib
import itertools
import json
import math
from pathlib import Path
import struct
import zipfile

EDITS = 'prelude/project/edits.json'
ROLES = {'flat': {'xyz': [[50, 0, 50], [50, 0, -50], [-50, 0, -50], [-50, 0, 50]],
          'uv': [[-512, 1536], [2048, 1536], [2048, -1024], [-512, -1024]],
          'material': '80ff803c506c240c913e1f1ebbfdf7ecc4f13aa398fb6382b668ab38763086cc',
          'primitive': [[100663812, 1030]]},
 'stone1': {'xyz': [[60, -40, -60],
                    [60, 0, -60],
                    [60, 0, 60],
                    [60, -40, 60],
                    [-60, -40, -60],
                    [-60, 0, -60],
                    [60, 0, -60],
                    [60, -40, -60],
                    [-60, -40, 60],
                    [-60, 0, 60],
                    [-60, 0, -60],
                    [-60, -40, -60],
                    [60, -40, 60],
                    [60, 0, 60],
                    [-60, 0, 60],
                    [-60, -40, 60]],
            'uv': [[3072, 1024],
                   [3072, 0],
                   [0, 0],
                   [0, 1024],
                   [0, 1024],
                   [0, 0],
                   [3072, 0],
                   [3072, 1024],
                   [0, 1024],
                   [0, 0],
                   [3072, 0],
                   [3072, 1024],
                   [3072, 1024],
                   [3072, 0],
                   [0, 0],
                   [0, 1024]],
            'material': '114f563cd29889880f91ca34f189dc4c1223018b0d20c4e794403a56c07cffda',
            'primitive': [[100663812, 1030], [101190156, 527374], [101716500, 1053718], [102242844, 1580062]]},
 'stone2': {'xyz': [[60, 0, 60],
                    [50, 0, 50],
                    [-50, 0, 50],
                    [-60, 0, 60],
                    [60, 0, -60],
                    [-60, 0, -60],
                    [-50, 0, -50],
                    [50, 0, -50]],
            'uv': [[-1934, 512],
                   [-1572, 512],
                   [238, -1298],
                   [238, -1660],
                   [238, 2684],
                   [2410, 512],
                   [2048, 512],
                   [238, 2322]],
            'material': 'dffc829eea39189091e778e1675a6e6215a771c2f54ceaee63378d24fe73fc67',
            'primitive': [[100663812, 1030], [101190156, 527374]]},
 'stone3': {'xyz': [[50, 0, -50], [1, -26, 0], [50, 0, 50], [-50, 0, -50], [-50, 0, 50]],
            'uv': [[-1280, -1792], [0, -512], [1280, -1792], [-1280, 768], [1280, 768]],
            'material': 'd85ab659627e0af58a37cd521dbddfba1a7caaef74f7bbc8a564ce8d846bc7d3',
            'primitive': [[100663812, 132616]]},
 'stone4': {'xyz': [[60, 0, 60],
                    [60, 0, -60],
                    [50, 0, -50],
                    [50, 0, 50],
                    [-50, 0, -50],
                    [-60, 0, -60],
                    [-60, 0, 60],
                    [-50, 0, 50]],
            'uv': [[-1934, 512],
                   [238, 2684],
                   [238, 2322],
                   [-1572, 512],
                   [2048, 512],
                   [2410, 512],
                   [238, -1660],
                   [238, -1298]],
            'material': 'dffc829eea39189091e778e1675a6e6215a771c2f54ceaee63378d24fe73fc67',
            'primitive': [[100663812, 1030], [101190156, 527374]]},
 'stone5': {'xyz': [[50, 0, 50], [1, -26, 0], [-50, 0, 50], [-50, 0, -50], [50, 0, -50]],
            'uv': [[1280, -1792], [0, -512], [1280, 768], [-1280, 768], [-1280, -1792]],
            'material': 'd85ab659627e0af58a37cd521dbddfba1a7caaef74f7bbc8a564ce8d846bc7d3',
            'primitive': [[100663812, 393736]]},
 'stone6': {'xyz': [[0, 39, 29],
                    [20, 39, 21],
                    [0, 178, 1],
                    [-20, 39, 21],
                    [-28, 39, 1],
                    [-20, 39, -19],
                    [0, 39, -27],
                    [20, 39, -19],
                    [28, 39, 1]],
            'uv': [[512, 146],
                   [146, 146],
                   [512, 512],
                   [878, 146],
                   [878, 512],
                   [878, 878],
                   [512, 878],
                   [146, 878],
                   [146, 512]],
            'material': '0b64863ec1e90ea5fb4cca89d8f2b121edaf29489ae46e552eebaa1ee5a9edc7',
            'primitive': [[100663296, 0], [100663296, 0], [100663296, 0], [100663296, 0]]},
 'raised': {'xyz': [[-25, 141, -24],
                    [0, 202, 1],
                    [25, 141, -24],
                    [50, 0, -50],
                    [-50, 0, -50],
                    [0, 202, 1],
                    [25, 141, 25],
                    [50, 0, 50],
                    [0, 202, 1],
                    [-25, 141, 25],
                    [-50, 0, 50],
                    [-25, 141, 25],
                    [0, 202, 1],
                    [-50, 0, 50]],
            'uv': [[896, 307],
                   [768, 0],
                   [640, 307],
                   [640, 1024],
                   [896, 1024],
                   [512, 0],
                   [384, 307],
                   [384, 1024],
                   [256, 0],
                   [128, 307],
                   [128, 1024],
                   [1152, 307],
                   [1024, 0],
                   [1152, 1024]],
            'material': '05ac51dc019b1691b2c375b2ee6ba5a443fcf52ccd2033fb05192bf9729f86f1',
            'primitive': [[100663812, 395264],
                          [101056516, 264716],
                          [101582340, 918540],
                          [101453842, 1314316],
                          [101977106, 1447936],
                          [101194262, 529920]]}}


def digest(value):
    return hashlib.sha256(json.dumps(value, separators=(',', ':')).encode()).hexdigest()


def crc64(name):
    crc = 0xffffffffffffffff
    for byte in name.encode():
        crc ^= byte << 56
        for _ in range(8):
            crc = ((crc << 1) ^ (0x42f0e1eba9ea3693 if crc & (1 << 63) else 0)) & 0xffffffffffffffff
    return crc


def declaration(role):
    return dict(version=1, source='mm.bg_keikoku_spr.' + ('lower_a' if role == 'flat' else 'central'),
                binding='material-motion', logicalWidth=32, logicalHeight=32)


def commands(data):
    if len(data) < 80 or data[4:8] != b'TLDO' or data[64] != 4 or (len(data)-72) % 8:
        raise ValueError('Unsupported display-list resource')
    words = list(struct.iter_unpack('<II', data[72:]))
    result = []
    i = 0
    while i < len(words):
        a, b = words[i]
        op = a >> 24
        payload = None
        if op in (0x20, 0x32, 0x33):
            i += 1
            if i >= len(words): raise ValueError('Truncated hash command')
            payload = words[i][0] << 32 | words[i][1]
        result.append((a, b, payload))
        i += 1
    if result[-1][:2] != (0xdf000000, 0) or any(a >> 24 == 0xdf for a, _, _ in result[:-1]):
        raise ValueError('Missing or nonterminal ENDDL')
    return result


def material_signature(cmds):
    return digest([[a,b] for a,b,_ in cmds if a >> 24 not in (0x32,0x33,0x05,0x06,0x07,0x49,0xdf)])


def vertices(data):
    if len(data) < 72 or data[4:8] != b'RRAO' or struct.unpack_from('<I',data,64)[0] != 25:
        raise ValueError('Unsupported vertex resource')
    count = struct.unpack_from('<I',data,68)[0]
    if count > 100000 or len(data) != 72 + count*16: raise ValueError('Invalid vertex extent')
    return [list(v[:3])+list(v[4:6]) for v in struct.iter_unpack('<hhhHhhBBBB',data[72:])]


def fit(p, q):
    """Least-squares yaw, positive uniform scale and translation; no per-role fits."""
    pc = [sum(v[i] for v in p)/len(p) for i in range(3)]
    qc = [sum(v[i] for v in q)/len(q) for i in range(3)]
    pp = [[v[i]-pc[i] for i in range(3)] for v in p]
    qq = [[v[i]-qc[i] for i in range(3)] for v in q]
    a = sum(v[0]*w[0]+v[2]*w[2] for v,w in zip(pp,qq))
    b = sum(v[2]*w[0]-v[0]*w[2] for v,w in zip(pp,qq))
    angle = math.atan2(b,a)
    co, si = math.cos(angle), math.sin(angle)
    rotate = lambda v: [co*v[0]+si*v[2],v[1],-si*v[0]+co*v[2]]
    denom = sum(sum(x*x for x in v) for v in pp)
    if denom <= 0: raise ValueError('Collapsed footprint')
    scale = sum(sum(x*y for x,y in zip(rotate(v),w)) for v,w in zip(pp,qq))/denom
    if not math.isfinite(scale) or scale < .1: raise ValueError('Unstable or reflected footprint')
    origin = [qc[i]-scale*rotate(pc)[i] for i in range(3)]
    return lambda v: [origin[i]+scale*rotate(v)[i] for i in range(3)]


def matches(transform, canonical, observed, tolerance):
    return all(abs(a-b) <= tolerance for p,q in zip(canonical, observed) for a,b in zip(transform(p),q[:3]))


def associate(candidates):
    """Every candidate component must belong to exactly one complete group."""
    used = set()
    groups = []
    for flat in [c for c in candidates if c['role']=='flat']:
        transform = fit(ROLES['flat']['xyz'],flat['v'])
        if not matches(transform,ROLES['flat']['xyz'],flat['v'],1): raise ValueError('Invalid flat footprint')
        alternatives = []
        roles = list(ROLES)
        for role in roles:
            options = []
            for candidate in candidates:
                if candidate['role'] != role or candidate['scene'] != flat['scene']: continue
                for y in ((-15,39,53) if role=='stone6' else (39,)):
                    canonical = [[x,yy-39+y,z] for x,yy,z in ROLES[role]['xyz']] if role=='stone6' else ROLES[role]['xyz']
                    # Footprint coordinates each have +/-1 quantization: origin
                    # uncertainty <=1, each fitted axis <=4/100. Propagate that
                    # finite bound to the largest canonical role coordinate.
                    margin = 2 + .08 * max(sum(abs(x) for x in p) for p in canonical)
                    if matches(transform,canonical,candidate['v'],margin): options.append((candidate,canonical))
            if not options: raise ValueError('Partial fountain family: missing '+role)
            alternatives.append(options)
        if math.prod(map(len,alternatives)) > 4096: raise ValueError('Ambiguous candidate enumeration')
        solutions = []
        for group in itertools.product(*alternatives):
            p = [v for _,xyz in group for v in xyz]
            q = [v[:3] for c,_ in group for v in c['v']]
            common = fit(p,q)
            if matches(common,p,q,1): solutions.append(group)
        if len(solutions) != 1: raise ValueError('Ambiguous or inconsistent complete fountain group')
        group = solutions[0]
        ids = {c['key'] for c,_ in group}
        if used & ids: raise ValueError('Overlapping fountain groups')
        used |= ids
        groups.append([c for c,_ in group])
    if used != {c['key'] for c in candidates}: raise ValueError('Unmatched fountain component')
    if not groups: raise ValueError('No complete fountain families found')
    return groups


def analyze(archive):
    names = archive.namelist()
    if len(names) != len(set(names)): raise ValueError('Duplicate ZIP entries')
    project = json.loads(archive.read(EDITS))
    if type(project.get('codec')) is not int or project['codec'] != 2: raise ValueError('Unsupported project codec')
    # Validate all codec descriptors before touching metadata; no unchecked slices.
    blobs=project.get('blobs')
    if not isinstance(blobs,list): raise ValueError('Missing blob table')
    for desc in blobs:
        if not isinstance(desc,dict) or desc.get('t') not in ('u8','i16','f32') or any(type(desc.get(k)) is not int or desc[k]<0 for k in ('o','n')):
            raise ValueError('Malformed blob descriptor')
        if desc['n'] % {'u8':1,'i16':2,'f32':4}[desc['t']]: raise ValueError('Misaligned blob length')
        path = desc.get('p','prelude/project/blobs.bin')
        if not isinstance(path,str) or path not in names or desc['o']+desc['n'] > archive.getinfo(path).file_size:
            raise ValueError('Blob range outside resource')
    def validate_blob_refs(node):
        if isinstance(node,dict):
            if '$blob' in node and (len(node)!=1 or type(node['$blob']) is not int or not 0<=node['$blob']<len(blobs)):
                raise ValueError('Invalid blob table reference')
            for child in node.values(): validate_blob_refs(child)
        elif isinstance(node,list):
            for child in node: validate_blob_refs(child)
    validate_blob_refs(project['edits'])
    hashes = {}
    for name in names:
        h = crc64(name)
        if h in hashes: raise ValueError('Resource hash collision')
        hashes[h] = name
    candidates, rows, seen = [], {}, set()
    for scene, edits in project['edits'].items():
        for edit in edits:
            for kind in ('pastes','shapes'):
                for item in edit.get('data',{}).get(kind,[]):
                    path = item.get('newDlPath')
                    if not isinstance(path,str) or path not in names: continue
                    data = archive.read(path)
                    if len(data)<72 or data[4:8]!=b'TLDO': continue
                    cmds = commands(data)
                    signature = material_signature(cmds)
                    possible = [r for r,spec in ROLES.items() if signature==spec['material']]
                    if not possible: continue
                    if path in seen: raise ValueError('Duplicate candidate material resource')
                    seen.add(path)
                    primitive = [[a,b] for a,b,_ in cmds if a>>24 in (5,6,7,0x49)]
                    loads = [(a,b,h) for a,b,h in cmds if a>>24==0x32]
                    if len(loads)!=1 or loads[0][2] not in hashes: raise ValueError('Unsupported vertex reference')
                    a,b,h = loads[0]
                    vpath = hashes[h]
                    verts = vertices(archive.read(vpath))
                    n = (a>>12)&0xff
                    start = b//16
                    if b%16 or start+n>len(verts) or ((a>>1)&0x7f)!=n: raise ValueError('Invalid vertex reference range')
                    role = None
                    for r in possible:
                        spec=ROLES[r];count=len(spec['xyz'])
                        textures=item.get('stored',{}).get('textures',[])
                        if r.startswith('stone'):
                            expected='Z2_00KEIKOKUTex_031B98' if r=='stone1' else 'Z2_00KEIKOKUTex_02A850'
                            if len(textures)!=1 or textures[0].get('label')!=expected: continue
                        if primitive!=spec['primitive'] or n!=count or len(verts)%count: continue
                        if any([v[3:] for v in verts[i:i+count]]!=spec['uv'] for i in range(0,len(verts),count)): continue
                        if role: raise ValueError('Multiple possible roles')
                        role=r
                    if role is None: raise ValueError('Changed fountain label, topology or UV')
                    count=len(ROLES[role]['xyz'])
                    if len(verts)//count>256: raise ValueError('Too many stored components')
                    for i in range(0,len(verts),count):
                        candidates.append(dict(key=(path,i),scene=scene,role=role,v=verts[i:i+count]))
                    rows[path]=dict(item=item,role=role,storedComponents=len(verts)//count,
                                    referencedVertexRanges=[[start,start+n]],vertexResource=vpath)
    if len(candidates)>2048: raise ValueError('Too many candidate components')
    groups=associate(candidates)
    report=[]
    for path,row in rows.items():
        if row['role'] not in ('flat','raised'): continue
        expected=declaration(row['role']);item=row['item']
        if 'nativeAnimation' in item and (item['nativeAnimation']!=expected or any(type(item['nativeAnimation'].get(k)) is not int for k in ('version','logicalWidth','logicalHeight'))):
            raise ValueError('Conflicting explicit native animation')
        item['nativeAnimation']=expected
        report.append(dict(path=path,**{k:v for k,v in row.items() if k!='item'}))
    return project,dict(bindings=report,completeStoredGroups=len(groups),
                        note='Stored components are not a count of visible draws; existing references are preserved.')


def bind(source, destination, dry_run=False):
    source,destination=Path(source),Path(destination)
    if source.resolve()==destination.resolve(): raise ValueError('Input and output must differ')
    if destination.exists(): raise ValueError('Output already exists')
    with zipfile.ZipFile(source) as archive:
        project,report=analyze(archive)
        if not dry_run:
            # Exclusive creation only after full successful analysis. On failure
            # discard our partial copy, never the source or an existing output.
            created=False
            try:
                stream=destination.open('xb');created=True
                with stream, zipfile.ZipFile(stream,'w') as out:
                    for info in archive.infolist():
                        out.writestr(info,json.dumps(project,separators=(',',':')).encode() if info.filename==EDITS else archive.read(info))
            except BaseException:
                if created: destination.unlink(missing_ok=True)
                raise
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input',type=Path);parser.add_argument('output',type=Path)
    parser.add_argument('--dry-run',action='store_true');args=parser.parse_args()
    try: print(json.dumps(bind(args.input,args.output,args.dry_run),indent=2))
    except (ValueError,KeyError,TypeError,zipfile.BadZipFile) as error: parser.exit(1,str(error)+'\n')

if __name__=='__main__': main()
