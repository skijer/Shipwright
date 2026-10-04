"""Independent readback checks for the R4 archive and its pose candidates."""
from pathlib import Path
import argparse, hashlib, json, zipfile
import numpy as np
from build_anju_hd import SourceModel
from render_anju_preview import Anju, read_vertex_array
from render_anju_hd_fit_review import select_standing_hand
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('native', type=Path)
parser.add_argument('r3', type=Path)
parser.add_argument('r4', type=Path)
parser.add_argument('r3_reproduction', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
NATIVE, R3, R4 = (args.native, args.r3, args.r4)
P = 'objects/object_anju_hd/v1/'

def posed(part, world):
    m = world[part['joints']]
    return np.einsum('nij,nj->ni', m[:, :3, :3], part['vertices'][:, :3]) + m[:, :3, 3]

def crossings(parts, world):
    positions = []
    indices = []
    keys = []
    keymap = {}
    for p in parts:
        x = posed(p, world)
        position_keys = []
        for row, j in zip(p['vertices'], p['joints']):
            key = (int(j), *map(int, row[:3]))
            keymap.setdefault(key, len(keymap))
            position_keys.append(keymap[key])
        positions.extend(x[p['tri']])
        keys.extend(np.array(position_keys)[p['tri']])
    a = np.array(positions)
    keys = np.array(keys)
    bounds = np.all(a.min(1)[:, None] <= a.max(1)[None] + 1e-05, 2) & np.all(a.max(1)[:, None] >= a.min(1)[None] - 1e-05, 2)
    ii, jj = np.where(np.triu(bounds, 1))
    adjacent = np.any(keys[ii, :, None] == keys[jj, None, :], axis=(1, 2))
    ii, jj = (ii[~adjacent], jj[~adjacent])
    hit = np.zeros(len(ii), bool)
    for x, y in ((a[ii], a[jj]), (a[jj], a[ii])):
        e1, e2 = (y[:, 1] - y[:, 0], y[:, 2] - y[:, 0])
        for edge in range(3):
            orig = x[:, edge]
            direction = x[:, (edge + 1) % 3] - orig
            h = np.cross(direction, e2)
            det = np.einsum('ij,ij->i', e1, h)
            valid = np.abs(det) > 1e-07
            den = np.where(valid, det, 1)
            offset = orig - y[:, 0]
            u = np.einsum('ij,ij->i', offset, h) / den
            q = np.cross(offset, e1)
            v = np.einsum('ij,ij->i', direction, q) / den
            t = np.einsum('ij,ij->i', e2, q) / den
            hit |= valid & (u > 1e-06) & (v > 1e-06) & (u + v < 1 - 1e-06) & (t > 1e-06) & (t < 1 - 1e-06)
    return int(hit.sum())

def shaft_hits(parts, world, ring):
    axis = np.array([-1648.0, 2785.0, 1602.0])
    axis /= np.linalg.norm(axis)
    px = np.cross(axis, [1, 0, 0])
    px /= np.linalg.norm(px)
    py = np.cross(axis, px)
    basis = np.stack([px, py], axis=1)
    r = ring @ basis
    d1, d2 = (r[1] - r[0], r[2] - r[0])
    sign = np.sign(d1[0] * d2[1] - d1[1] * d2[0])
    local = np.linalg.inv(world[7]) @ world
    hits = 0
    for p in parts:
        if p['limb'] not in (6, 7):
            continue
        pos = posed(p, local)
        for face in p['tri']:
            poly = list(pos[face])
            for a, b in zip(r, np.roll(r, -1, axis=0)):
                edge = b - a
                n = np.array([-edge[1], edge[0]]) * sign
                n /= np.linalg.norm(n)
                nxt = []
                for x, y in zip(poly, poly[1:] + poly[:1]):
                    da = (x @ basis - a) @ n - 1
                    db = (y @ basis - a) @ n - 1
                    if da >= 0:
                        nxt.append(x)
                    if (da >= 0) != (db >= 0):
                        nxt.append(x + (y - x) * da / (da - db))
                poly = nxt
                if not poly:
                    break
            hits += bool(poly)
    return hits
report = {}
with zipfile.ZipFile(R3) as a, zipfile.ZipFile(R4) as b:
    assert b.testzip() is None
    an, bn = (set(a.namelist()), set(b.namelist()))
    textures = [n for n in an if n.startswith(P) and int.from_bytes(a.read(n)[4:8], 'little') == 1330922840]
    assert len(textures) == 7 and all((a.read(n) == b.read(n) for n in textures))
    report['unchanged_textures'] = textures
    report['changed_existing_resources'] = [n for n in sorted(an & bn) if a.read(n) != b.read(n)]
    report['new_resources'] = sorted(bn - an)
    report['removed_resources'] = sorted(an - bn)
    allowed = {'Anju_HD_README.json', P + 'gAnju1LeftUpperArmDL', P + 'gAnju1LeftForearmDL', P + 'object_an2Vtx_000000', P + 'batch8Vtx', P + 'batch8DL', P + 'batch13Vtx', P + 'batch14Vtx', P + 'batch15Vtx', P + 'batch34Vtx'}
    unexpected = set(report['changed_existing_resources']) - allowed
    print('changed paths', report['changed_existing_resources'], 'unexpected', unexpected, flush=True)
    assert not unexpected
    assert all((n.startswith(P) or n == 'Anju_HD_README.json' for n in bn))
    report['sha256'] = hashlib.sha256(R4.read_bytes()).hexdigest()
    report['resources'] = len(bn)
    repro = args.r3_reproduction
    with zipfile.ZipFile(repro) as c:
        assert an == set(c.namelist()) and all((a.read(n) == c.read(n) for n in an))
    report['r3_reproduction_exact_resources'] = len(an)
    with zipfile.ZipFile(NATIVE) as native_archive:
        native_rows = np.array(read_vertex_array(native_archive.read('objects/object_an2/object_an2Vtx_000000')))
    ring_indices = [next((i for i, row in enumerate(native_rows) if tuple(row[:3]) == point)) for point in ((230, 52, 161), (260, 8, 267), (332, 84, 211))]
    for standing in (True, False):
        label = 'standing' if standing else 'seated'
        source = SourceModel(NATIVE, R4, standing_preview=standing)
        model = Anju(NATIVE, standing, R4)
        if standing:
            select_standing_hand(model)
        elbow = [p for p in source.parts if p['name'].split('/')[-1].startswith(('batch7', 'batch8', 'leftElbowFit'))]
        path = P + 'object_an2Vtx_000000' + ('Standing' if standing else '')
        ring = np.array(read_vertex_array(b.read(path)))[ring_indices, :3]
        clip = model.clips[model.animation]
        counts = []
        shaft = []
        for frame in range(len(clip)):
            world = model.world(clip[frame])
            counts.append(crossings(elbow, world))
            shaft.append(shaft_hits(source.parts, world, ring))
            decoded = model.pose(model.animation, frame)
            assert all((np.isfinite(p['pos']).all() and np.isfinite(p['normal']).all() for p in decoded))
        report[label] = {'frames': len(clip), 'elbow_crossings': counts, 'shaft_penetrations': shaft}
        assert max(counts) == 0, (label, counts)
        assert max(shaft) == 0, (label, shaft)
        print(label, 'PASS all frames', len(clip), flush=True)
    old_elbow = SourceModel(NATIVE, R3)
    original_rows = {(int(j), *map(int, v)) for p in old_elbow.parts if p['name'].endswith(('batch7DL', 'batch8DL')) for v, j in zip(p['vertices'], p['joints'])}
    original_keys = {(row[0], *row[1:4]) for row in original_rows}
    for standing in (True, False):
        decoded = SourceModel(NATIVE, R4, standing_preview=standing)
        patch = next((p for p in decoded.parts if 'leftElbowFit' in p['name']))
        rows = {(int(j), *map(int, v)) for v, j in zip(patch['vertices'], patch['joints'])}
        keys = {(r[0], *r[1:4]) for r in rows}
        shared = keys & original_keys
        expected = {r for r in original_rows if (r[0], *r[1:4]) in shared}
        assert len(shared) == 26, (standing, len(shared))
        assert expected <= rows, (standing, expected - rows)
        print('boundary attributes', standing, len(shared), 'positions', len(expected), 'rows PASS', flush=True)
    report['elbow_boundary_positions'] = 26
    report['elbow_boundary_uv_seams_preserved'] = True
    old = SourceModel(NATIVE, R3)
    new = SourceModel(NATIVE, R4)
    oldparts = {p['name']: p for p in old.parts}
    newparts = {p['name']: p for p in new.parts}
    for name in ('batch31DL', 'batch34DL'):
        p, q = (oldparts[P + name], newparts[P + name])
        assert np.array_equal(p['joints'], q['joints']) and np.array_equal(p['tri'], q['tri'])
        v = p['vertices']
        fixed = (p['joints'] != 15) | (v[:, 1] <= 160) | (v[:, 1] >= 740)
        assert np.array_equal(v[fixed], q['vertices'][fixed])
        assert np.array_equal(v[:, 1], q['vertices'][:, 1])
    report['ankle_fixed_rows_exact'] = True
out = args.output
out.write_text(json.dumps(report, indent=2))
print('PASS', out, flush=True)
