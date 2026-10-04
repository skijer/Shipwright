"""Independent R5 readback: prop contact, folded thumbnail, and retained artwork.

Checks actual exported triangles through both native animation loops. Numerical
checks complement the multi-angle previews; they do not certify visual anatomy.
The original R3 left elbow is retained, not replaced by the rejected R4 loft.
"""
from pathlib import Path
import argparse
import hashlib
import json
import zipfile
import numpy as np
from build_anju_hd import SourceModel
from render_anju_preview import Anju, read_vertex_array
from render_anju_hd_fit_review import select_standing_hand
from anju_hd_finish import SHAFT_AXIS, HOOK_POINTS, UMBRELLA_VERTICES
parser = argparse.ArgumentParser(description=__doc__)
for name in ('native', 'source', 'r3', 'r4', 'r5', 'output'):
    parser.add_argument(name, type=Path)
args = parser.parse_args()
NATIVE, SOURCE, R3, R4, R5 = (args.native, args.source, args.r3, args.r4, args.r5)
P = 'objects/object_anju_hd/v1/'

def posed(part, world):
    m = world[part['joints']]
    return np.einsum('nij,nj->ni', m[:, :3, :3], part['vertices'][:, :3]) + m[:, :3, 3]

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
with zipfile.ZipFile(R3) as a, zipfile.ZipFile(R4) as b, zipfile.ZipFile(R5) as c, zipfile.ZipFile(NATIVE) as native:
    assert c.testzip() is None
    assert all((n.startswith(P) or n == 'Anju_HD_README.json' for n in c.namelist()))
    changed = [n for n in a.namelist() if c.read(n) != a.read(n)]
    allowed = ['Anju_HD_README.json'] + [P + n for n in ('batch13Vtx', 'batch14Vtx', 'batch14DL', 'batch15Vtx', 'batch34Vtx', 'object_an2Vtx_000000')]
    assert set(changed) == set(allowed), (set(changed) - set(allowed), set(allowed) - set(changed))
    report['changed_from_r3'] = changed
    textures = [n for n in a.namelist() if n.startswith(P) and int.from_bytes(a.read(n)[4:8], 'little') == 1330922840]
    assert len(textures) == 7 and all((c.read(n) == a.read(n) for n in textures))
    assert c.read(P + 'batch34Vtx') == b.read(P + 'batch34Vtx')
    assert c.read(P + 'batch13Vtx') == b.read(P + 'batch13Vtx')
    report.update(left_arm_and_sleeve_exact_r3=True, all_seven_textures_exact=True, ankle_and_wrist_exact_r4=True)
    original = np.array(read_vertex_array(native.read(UMBRELLA_VERTICES)))
    indices = [next((i for i, row in enumerate(original) if tuple(row[:3]) == p)) for p in ((230, 52, 161), (260, 8, 267), (332, 84, 211))]
    hookmask = np.array([tuple(v[:3]) in HOOK_POINTS for v in original])
    raw = SourceModel(NATIVE, SOURCE)
    raw_nails = next((p for p in raw.parts if p['name'].endswith('batch15DL')))
    source_tri = raw_nails['vertices'][raw_nails['tri'], :3]
    thumb_faces = np.all(source_tri[:, :, 2] > 150, axis=1)
    assert thumb_faces.sum() == 27
    for standing in (True, False):
        s = SourceModel(NATIVE, R5, standing_preview=standing)
        m = Anju(NATIVE, standing, R5)
        if standing:
            select_standing_hand(m)
        key = P + UMBRELLA_VERTICES.split('/')[-1] + ('Standing' if standing else '')
        v = np.array(read_vertex_array(c.read(key)))
        ring = v[indices, :3].astype(float)
        counts = []
        gaps = []
        hookgaps = []
        clip = m.clips[m.animation]
        first = ring[1] - ring[0]
        first -= SHAFT_AXIS * (first @ SHAFT_AXIS)
        first /= np.linalg.norm(first)
        basis = np.stack([first, np.cross(SHAFT_AXIS, first)], axis=1)
        r = ring @ basis
        d1, d2 = (r[1] - r[0], r[2] - r[0])
        sign = np.sign(d1[0] * d2[1] - d1[1] * d2[0])
        planes = []
        for aa, bb in zip(r, np.roll(r, -1, axis=0)):
            e = bb - aa
            nn = np.array([-e[1], e[0]]) * sign
            nn /= np.linalg.norm(nn)
            planes.append((basis @ nn, aa @ nn))
        hand = next((p for p in s.parts if p['name'].endswith(('batch14DL', 'batch14StandingDL'))))
        for i, frame in enumerate(clip):
            w = m.world(frame)
            counts.append(shaft_hits(s.parts, w, ring))
            local = np.linalg.inv(w[7]) @ w
            points = posed(hand, local)
            inside = np.array([points @ nn - off for nn, off in planes]).min(axis=0)
            gaps.append(float(-inside.max()))
            hookgaps.append(float((v[hookmask, :3] @ SHAFT_AXIS).min() - (points @ SHAFT_AXIS).max()))
            for p in m.pose(m.animation, i):
                assert np.isfinite(p['pos']).all() and np.isfinite(p['normal']).all()
        assert max(counts) == 0 and min(gaps) >= -1 and (max(gaps) < 1.5), (standing, counts, gaps)
        assert min(hookgaps) > 40
        nails = next((p for p in s.parts if p['name'].endswith(('batch15DL', 'batch15StandingDL'))))
        assert len(nails['tri']) == len(thumb_faces)
        center = nails['vertices'][nails['tri'][thumb_faces], :3].reshape(-1, 3).mean(0)
        thumb_y = float(center @ SHAFT_AXIS)
        assert -120 < thumb_y < 0, thumb_y
        report['standing' if standing else 'seated'] = {'frames': len(clip), 'shaft_penetrations': counts, 'closest_contact_gap_model_units': [min(gaps), max(gaps)], 'hook_gap_min_model_units': min(hookgaps), 'thumbnail_axis_center': thumb_y}
        print('PASS', standing, len(clip), 'frames; contact', min(gaps), max(gaps), 'folded thumbnail', thumb_y, flush=True)
    report['resources'] = len(c.namelist())
    report['size_bytes'] = R5.stat().st_size
    report['sha256'] = hashlib.sha256(R5.read_bytes()).hexdigest()
args.output.write_text(json.dumps(report, indent=2))
print('PASS complete', flush=True)
