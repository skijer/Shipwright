"""Prepare the supplied HD Anju on the native MM rig, without scene overrides.

Geometry and archive edits only; refined artwork is supplied as separate PNGs.
The renderer decodes the actual exported display lists for preview validation.
"""
from pathlib import Path
import argparse
import copy
import io
import json
import struct
import zipfile

import numpy as np
from PIL import Image

from render_anju_preview import Anju, commands, crc64, read_vertex_array, render
from anju_hd_fit import fit_umbrella_and_skirt, fit_vest_and_heels, standing_left_hand
from anju_hd_finish import UMBRELLA_VERTICES, fit_final_details
from anju_hd_contact import fit_contact_details

PREFIX = 'objects/object_anju_hd/v1/'
SOURCE = 'custom/prelude/objects/object_an1/gAnju1TorsoDL/'


def header(kind, version=0):
    return struct.pack('<BBHIIQ', 0, 1, 0, kind, version, 0) + bytes(44)


def dl_bytes(items):
    return header(0x4F444C54) + bytes([4]) + bytes(7) + b''.join(struct.pack('<II', a, b) for a, b in items)


def hashed(op, path, word1=0, low=0):
    h = crc64(path)
    return [(op << 24 | low, word1), (h >> 32, h & 0xFFFFFFFF)]


def texture_bytes(image, virtual_width=32, virtual_height=32, virtual_bytes=2):
    image = image.convert('RGBA')
    w, h = image.size
    raw = image.tobytes()
    return header(0x4F544558, 1) + struct.pack(
        '<IIIIffI', 1, w, h, 1, 4 * w / (virtual_width * virtual_bytes), h / virtual_height, len(raw)) + raw


def vertex_bytes(v):
    return header(0x4F415252) + struct.pack('<II', 25, len(v)) + b''.join(
        struct.pack('<hhhHhhBBBB', *map(int, row)) for row in v)


def smooth(t):
    t = np.clip(t, 0, 1)
    return t * t * (3 - 2 * t)


def refresh_normals(part, changed):
    v = part['vertices']
    old = v[:, 6:9].astype(float)
    old[old > 127] -= 256
    xyz, inverse = np.unique(np.c_[v[:, :3], part['joints']], axis=0, return_inverse=True)
    accumulated = np.zeros((len(xyz), 3))
    tri = part['tri']
    face = np.cross(v[tri[:, 1], :3] - v[tri[:, 0], :3], v[tri[:, 2], :3] - v[tri[:, 0], :3]).astype(float)
    face[np.sum(face * old[tri].mean(axis=1), axis=1) < 0] *= -1
    for k in range(3):
        np.add.at(accumulated, inverse[tri[:, k]], face)
    normals = accumulated[inverse]
    length = np.linalg.norm(normals, axis=1)
    use = changed & (length > 1e-8)
    v[use, 6:9] = np.round(normals[use] / length[use, None] * 127).astype(int) & 255


def fit_heels(part):
    """Reshape the supplied shoe, retaining its ankle seam and footprint Y."""
    result = copy.deepcopy(part)
    v = result['vertices']
    x, y, z = v[:, :3].astype(float).T
    foot = result['joints'] == result['limb']
    sole = foot & (y > 930)
    # Open the instep under the existing vamp; the toe and heel contact stay.
    arch = np.interp(x, [150, 210, 365, 458, 589, 700], [0, 60, 155, 125, 65, 0])
    y[sole] -= arch[sole]
    # A narrower heel shaft replaces the broad block at the rear.
    heel = foot & (x < 240)
    taper = smooth((v[:, 1] - 800) / 160) * (1 - smooth((x - 170) / 70))
    x[heel] = 75 + (x[heel] - 75) * (1 - .45 * taper[heel])
    center = -76 if result['limb'] == 12 else 76
    z[heel] = center + (z[heel] - center) * (1 - .48 * taper[heel])
    # Raise the rear upper gently while leaving the shin's weld ring untouched.
    lift = 75 * smooth((v[:, 1] - 560) / 120) * (1 - smooth((v[:, 1] - 850) / 80))
    lift *= (1 - smooth((v[:, 0] - 200) / 350))
    y[foot] -= lift[foot]
    v[:, :3] = np.round(np.c_[x, y, z]).astype(int)
    refresh_normals(result, np.any(v[:, :3] != part['vertices'][:, :3], axis=1))
    return result


def blink_parts(parts, amount):
    output = copy.deepcopy(parts)
    for p in output:
        if p['limb'] != 8:
            continue
        v = p['vertices']
        xyz = v[:, :3].astype(float)
        x, y, z = xyz.T.copy()
        if p['texture'].endswith('tex2'):
            w = smooth((x - 365) / 25) * smooth((575 - x) / 70)
            w *= smooth((np.abs(z) - 50) / 20) * smooth((305 - np.abs(z)) / 35)
            w *= smooth((y - 300) / 35) * (p['joints'] == 8)
            brow = ((v[:, 4:6] >= [112, 235]) & (v[:, 4:6] <= [144, 251])).all(axis=1)
            w[brow] = 0
            closed_line = 398 + .005 * (np.abs(z) - 155) ** 2
            xyz[:, 0] += amount * w * (closed_line - x)
            uv = v[:, 4:6].copy()
            v[:, :3] = np.round(xyz).astype(int)
            # The enlarged lid needs clean skin, not a stretched baked crease.
            # Reproject just that face island toward its own forehead texels;
            # retain the original atlas, eye artwork and separate lash strip.
            skin = smooth((x - 450) / 35) * smooth((580 - x) / 70)
            skin *= smooth((np.abs(z) - 60) / 25) * smooth((290 - np.abs(z)) / 40)
            skin *= smooth((y - 300) / 35) * (uv[:, 1] < 130)
            v[:, 5] -= np.round(28 * amount * skin).astype(int)
            refresh_normals(p, w > 0)
        elif p['texture'].endswith('tex0'):
            if amount >= 1:
                p['tri'] = p['tri'][:0]
                continue
            top = np.interp(np.abs(z), [40, 91, 98, 107, 118, 129, 144, 162, 180, 195, 207, 216, 224, 260],
                            [400, 408, 418, 433, 446, 457, 466, 470, 469, 466, 460, 457, 455, 415])
            closed_line = 398 + .005 * (np.abs(z) - 155) ** 2
            ceiling = top * (1 - amount) + closed_line * amount - 5
            # Clip, rather than scale, the iris: it must keep its original shape.
            verts, triangles = [], []
            signed = v.astype(float)
            signed[:, 6:9] = np.where(signed[:, 6:9] > 127, signed[:, 6:9] - 256, signed[:, 6:9])
            for tri in p['tri']:
                polygon = [(signed[i], x[i] - ceiling[i]) for i in tri]
                clipped = []
                for i, (a, da) in enumerate(polygon):
                    b, db = polygon[(i + 1) % len(polygon)]
                    if da <= 0:
                        clipped.append(a)
                    if (da <= 0) != (db <= 0):
                        t = da / (da - db)
                        clipped.append(a + (b - a) * t)
                if len(clipped) < 3:
                    continue
                start = len(verts)
                verts.extend(clipped)
                triangles.extend((start, start + i, start + i + 1) for i in range(1, len(clipped) - 1))
            p['vertices'] = np.round(verts).astype(int)
            p['vertices'][:, 6:9] &= 255
            p['tri'] = np.array(triangles)
            p['joints'] = np.full(len(verts), 8)
    return output


def separate_materials(parts, use_shirt):
    result = []
    for original in parts:
        p = copy.deepcopy(original)
        # These three small inner surfaces sit entirely above the skirt hem.
        # Give them matching lining so a bent knee cannot show tiny skin slivers
        # through the layered skirt. The exposed calves and feet stay original.
        if p['name'].split('/')[-1] in ('batch21DL', 'batch28DL', 'batch37DL'):
            assert p['texture'].endswith('tex2')
            p['texture'] = SOURCE + 'tex8'
            p['vertices'][:, 4] = 220 + p['vertices'][:, 4] // 8
            p['vertices'][:, 5] = 120 + p['vertices'][:, 5] // 8
        if p['texture'].endswith('tex2'):
            uv = p['vertices'][:, 4:6]
            if p['limb'] == 8:
                brow = ((uv >= [112, 235]) & (uv <= [144, 251])).all(axis=1)
                keep = brow[p['tri']].all(axis=1)
                if keep.any():
                    q = copy.deepcopy(p)
                    q['tri'] = p['tri'][keep]
                    q['name'] = SOURCE + 'browsDL'
                    q['texture'] = SOURCE + 'tex5'
                    q['vertices'][:, 4] = (uv[:, 0] - 112) * 32
                    q['vertices'][:, 5] = 780 + (uv[:, 1] - 235) * 10
                    result.append(q)
                    p['tri'] = p['tri'][~keep]
            elif use_shirt:
                shirt = ((uv[:, 0] < 667) & (uv[:, 1] >= 408) & (uv[:, 1] < 740)) | (
                    (uv[:, 0] < 826) & (uv[:, 1] >= 740))
                keep = shirt[p['tri']].all(axis=1)
                if keep.any():
                    q = copy.deepcopy(p)
                    q['tri'] = p['tri'][keep]
                    q['name'] = p['name'].removesuffix('DL') + 'ShirtDL'
                    q['texture'] = SOURCE + 'shirtTex'
                    result.append(q)
                    p['tri'] = p['tri'][~keep]
        if len(p['tri']):
            result.append(p)
    return result


def export_mesh(part, name, files):
    path = PREFIX + name
    vertex_path = path.removesuffix('DL') + 'Vtx'
    material = PREFIX + part['texture'].split('/')[-1]
    cull = part['texture'].endswith('tex5') and not part['name'].endswith('browsDL')
    words = hashed(0x33, path, 0xBEEFBEEF)
    words += [(0xE7000000, 0), (0xD7000002, 0xFFFFFFFF),
              (0xD9000000, 0x230405 if cull else 0x230005),
              (0xE3001001, 2), (0xFA000000, 0xFFFFFFFF),
              (0xFC127E03, 0xFFFFFDF8), (0xE200001C, 0xC8112078)]
    words += hashed(0x20, material, low=0x100000)
    words += [(0xF5100000, 0x07000000), (0xE6000000, 0), (0xF3000000, 0x073FF100),
              (0xE7000000, 0), (0xF5101000, 0x14050), (0xF2002002, 0x007E07E)]
    packed = []
    for start in range(0, len(part['tri']), 10):
        batch = part['tri'][start:start + 10]
        indices = sorted(set(map(int, batch.flat)), key=lambda i: (part['joints'][i], i))
        assert len(indices) <= 30
        slots = {index: slot for slot, index in enumerate(indices)}
        first = len(packed)
        packed.extend(part['vertices'][indices])
        cursor = 0
        while cursor < len(indices):
            joint = int(part['joints'][indices[cursor]])
            stop = cursor + 1
            while stop < len(indices) and part['joints'][indices[stop]] == joint:
                stop += 1
            # All 19 native flex slots are filled before the RSP executes the
            # emitted graph, including blended seams using a later limb.
            assert 1 <= joint <= 19, (name, joint, part['limb'])
            words += [(0xDA380003, 0x0D000001 + (joint - 1) * 64)]
            words += hashed(0x32, vertex_path, (first + cursor) * 16, ((stop - cursor) << 12) | (stop << 1))
            cursor = stop
        for i in range(0, len(batch), 2):
            def triangle(t):
                a, b, c = [slots[int(k)] * 2 for k in t]
                return a << 16 | b << 8 | c
            if i + 1 < len(batch):
                words.append((0x06000000 | triangle(batch[i]), triangle(batch[i + 1])))
            else:
                words.append((0x05000000 | triangle(batch[i]), 0))
    words.append((0xDF000000, 0))
    assert len(words) <= 4096, (name, len(words))
    files[path] = dl_bytes(words)
    if packed:
        files[vertex_path] = vertex_bytes(packed)
    return path


def export_model(source, output, umbrella_image=None, revision='R2'):
    files = {}
    for path in {p['texture'] for p in source.parts}:
        files[PREFIX + path.split('/')[-1]] = source.model.resources[path]
    for limb, native_root in enumerate(source.model.dlists):
        if not native_root:
            continue
        root = PREFIX + native_root.split('/')[-1]
        words = hashed(0x33, root, 0xBEEFBEEF)
        for part in source.parts:
            if part['limb'] == limb and len(part['tri']):
                child = export_mesh(part, part['name'].split('/')[-1], files)
                words += hashed(0x31, child)
        files[root] = dl_bytes(words + [(0xDF000000, 0)])
    for suffix, amount in [('Half', .5), ('Closed', 1)]:
        root = PREFIX + f'gAnju1Blink{suffix}HeadDL'
        words = hashed(0x33, root, 0xBEEFBEEF)
        for part in blink_parts(source.parts, amount):
            if part['limb'] != 8 or not len(part['tri']):
                continue
            stem = part['name'].split('/')[-1]
            if part['texture'].endswith(('tex0', 'tex2')):
                stem = stem.removesuffix('DL') + suffix + 'DL'
                export_mesh(part, stem, files)
            words += hashed(0x31, PREFIX + stem)
        files[root] = dl_bytes(words + [(0xDF000000, 0)])
    if revision in ('R3', 'R4', 'R5'):
        # A separate candidate keeps the approved crying hand byte-identical.
        # The seated adapter does not select this root; the standing preview does.
        root = PREFIX + 'gAnju1RelaxedLeftHandDL'
        words = hashed(0x33, root, 0xBEEFBEEF)
        for part in standing_left_hand(source.parts):
            stem = part['name'].split('/')[-1].removesuffix('DL') + 'RelaxedDL'
            words += hashed(0x31, export_mesh(part, stem, files))
        files[root] = dl_bytes(words + [(0xDF000000, 0)])
    standing_roots = {}
    if revision in ('R4', 'R5'):
        base_parts = {part['name']: part for part in source.parts}
        for limb in ((6, 7) if revision == 'R5' else (2, 3, 6, 7)):
            root = PREFIX + source.model.dlists[limb].split('/')[-1].removesuffix('DL') + 'StandingDL'
            words = hashed(0x33, root, 0xBEEFBEEF)
            for part in source.standing_parts:
                if part['limb'] != limb or not len(part['tri']):
                    continue
                base = base_parts.get(part['name'])
                unchanged = base is not None and all(
                    np.array_equal(base[key], part[key]) for key in ('vertices', 'joints', 'tri'))
                stem = part['name'].split('/')[-1]
                if not unchanged:
                    stem = stem.removesuffix('DL') + 'StandingDL'
                    export_mesh(part, stem, files)
                words += hashed(0x31, PREFIX + stem)
            files[root] = dl_bytes(words + [(0xDF000000, 0)])
            standing_roots[str(limb)] = root
    # Preserve native materials and canopy shape; R4 fits the accessory grip.
    native_root = 'objects/object_an2/gAnju2UmbrellaDL'
    words = []
    for op, a, b, h in commands(source.model.resources[native_root]):
        words.append((a, b))
        if h is None:
            continue
        path = source.model.hashes[h]
        target = PREFIX + path.split('/')[-1]
        new_hash = crc64(target)
        words.append((new_hash >> 32, new_hash & 0xFFFFFFFF))
        if op == 0x32:
            files[target] = (vertex_bytes(source.seated_umbrella) if revision in ('R4', 'R5') and path == UMBRELLA_VERTICES
                             else source.model.resources[path])
        elif op == 0x20:
            image = Image.fromarray(source.model.texture(path, None))
            w, height = image.size
            if umbrella_image and path.endswith('gAnju2UmbrellaDesignTex'):
                image = Image.open(umbrella_image)
            files[target] = texture_bytes(image, w, height, 1)
    files[PREFIX + 'gAnju2UmbrellaDL'] = dl_bytes(words)
    if revision in ('R4', 'R5'):
        alternate_umbrella = PREFIX + 'gAnju2UmbrellaStandingDL'
        alternate_vertices = PREFIX + UMBRELLA_VERTICES.split('/')[-1] + 'Standing'
        replacements = {crc64(PREFIX + 'gAnju2UmbrellaDL'): alternate_umbrella,
                        crc64(PREFIX + UMBRELLA_VERTICES.split('/')[-1]): alternate_vertices}
        words = []
        for op, a, b, h in commands(files[PREFIX + 'gAnju2UmbrellaDL']):
            words.append((a, b))
            if h is not None:
                value = crc64(replacements[h]) if h in replacements else h
                words.append((value >> 32, value & 0xFFFFFFFF))
        files[alternate_umbrella] = dl_bytes(words)
        files[alternate_vertices] = vertex_bytes(source.standing_umbrella)
    metadata = dict(name=f'Anju HD Auburn Umbrella {revision}', namespace=PREFIX, alternate_assets_required=True,
                    requires_adapter='Anju HD v1', actor_param='0x7E0D', limb_count=20, matrix_count=19,
                    local_branch_fit_y=476, blink='open / half / closed / closed / half at 20 Hz',
                    animation='Native MM seated umbrella cry, 43 frames',
                    notes=['Uses the supplied HD model and original blue eyes.',
                           'Auburn hair, retained sheen, culled overlapping hair undersides.',
                           'Reshaped original heels; ankle seam and sole contact height retained.',
                           'Native umbrella geometry, hand attachment and brown/gold material.',
                           'No scene, path, collision, skeleton or animation overrides.',
                           'Requires the prepared fork update; not a drop-in pack for an older build.'])
    if revision == 'R2':
        metadata['revision'] = 'R2'
        metadata['notes'] += [
            'Right fingers and thumb wrap around the unchanged native umbrella shaft.',
            'Fitted lower skirt; concealed upper calf surfaces trimmed to prevent poke-through.',
            'Original exposed calves, shoes, opposite hand and approved artwork retained.',
            'Standing umbrella idle is an offline candidate preview, without an assigned catalogue parameter.']
    elif revision == 'R3':
        metadata['revision'] = 'R3'
        metadata['standing_preview_hand'] = PREFIX + 'gAnju1RelaxedLeftHandDL'
        metadata['notes'] += [
            'Retains the R2 umbrella grip, fitted skirt, and concealed calf trim.',
            'Lowest button follows the torso; the tucked vest hem joins the complete pelvis belt.',
            'Continuous shoe fitting replaces the intersecting sole-only lift.',
            'Original crying left hand retained; separate relaxed left hand for the standing candidate.',
            'Standing umbrella idle remains an offline preview without an assigned catalogue parameter.',
            'All seven approved textures are unchanged.']
    elif revision == 'R4':
        metadata['revision'] = 'R4'
        metadata['standing_preview_hand'] = PREFIX + 'gAnju1RelaxedLeftHandDL'
        metadata['standing_preview_roots'] = standing_roots
        metadata['standing_preview_umbrella'] = alternate_umbrella
        metadata['notes'][3] = 'Native umbrella material and hand bone; accessory position fitted to the natural grip.'
        metadata['notes'] += [
            'Separate seated and standing wrist fits; fingers, thumb, and nails use one continuous deformation.',
            'Straight handle extended below the palm so the wide hook clears the hand.',
            'Continuous left elbow patch replaces intersecting skin, retaining both source boundary loops.',
            'Left ankle tapered at most 16 percent; upper seam, toe, heel, and sole contacts retained.',
            'R3 vest, shoes, skirt, left-hand gestures, blink heads, and all seven textures retained.',
            'The seated adapter uses the default roots. Standing corrective roots are preview candidates only.',
            'Standing umbrella idle has no catalogue parameter yet.']
    elif revision == 'R5':
        metadata['revision'] = 'R5'
        metadata['standing_preview_hand'] = PREFIX + 'gAnju1RelaxedLeftHandDL'
        metadata['standing_preview_roots'] = standing_roots
        metadata['standing_preview_umbrella'] = alternate_umbrella
        metadata['notes'][3] = 'Native umbrella fitted to the grip, including normals and hook clearance.'
        metadata['notes'] += [
            'R3 left arm and sleeve geometry restored exactly; no R4 elbow loft.',
            'Compact folded thumb and independently curled fingers, with the R4 wrist alignment retained.',
            'Contact skin buried inside the shaft is trimmed; this hand requires its attached umbrella.',
            'R4 ankle taper and all seven approved textures retained.',
            'The seated adapter uses the default roots. Standing corrective roots remain preview candidates.',
            'Standing umbrella idle has no catalogue parameter yet.']
    files['Anju_HD_README.json'] = json.dumps(metadata, indent=2).encode()
    payload = io.BytesIO()
    with zipfile.ZipFile(payload, 'w', zipfile.ZIP_DEFLATED) as z:
        for path, data in files.items():
            z.writestr(path, data)
    with zipfile.ZipFile(io.BytesIO(payload.getvalue())) as z:
        assert z.testzip() is None and len(z.namelist()) == len(files)
    output.write_bytes(payload.getvalue())
    print('Export:', output, 'resources', len(files), flush=True)


class SourceModel:
    def __init__(self, native, source, standing_preview=False):
        self.model = Anju(native, True, source)
        if standing_preview:
            from render_anju_hd_fit_review import select_standing_hand
            select_standing_hand(self.model)
        self.parts = []
        for limb, root in enumerate(self.model.dlists):
            if not root:
                continue
            for op, w0, w1, hash_value in commands(self.model.resources[root]):
                if op != 0x31:
                    continue
                path = self.model.hashes[hash_value]
                owner = limb
                refs, triangles, cache, texture, vertex_path = {}, [], {}, None, None
                for cmd, a, b, h in commands(self.model.resources[path]):
                    if cmd == 0xDA:
                        assert b & 1 and b >> 24 == 13
                        owner = self.model.slots[(b & 0xFFFFFE) // 64]
                    elif cmd == 0x20:
                        texture = self.model.hashes[h]
                    elif cmd == 0x32:
                        current = self.model.hashes[h]
                        assert vertex_path is None or vertex_path == current
                        vertex_path = current
                        count = (a >> 12) & 255
                        start = ((a >> 1) & 127) - count
                        assert b % 16 == 0
                        for i in range(count):
                            index = b // 16 + i
                            assert index not in refs or refs[index] == owner
                            refs[index] = owner
                            cache[start + i] = index
                    elif cmd in (5, 6):
                        for word in (a, b) if cmd == 6 else (a,):
                            triangles.append([cache[(word >> shift & 255) // 2] for shift in (16, 8, 0)])
                if vertex_path:
                    vertices = np.array(read_vertex_array(self.model.resources[vertex_path]), dtype=int)
                    assert len(refs) == len(vertices)
                    self.parts.append(dict(name=path, limb=limb, texture=texture,
                                           vertex_path=vertex_path, vertices=vertices,
                                           joints=np.array([refs[i] for i in range(len(vertices))]),
                                           tri=np.array(triangles)))

    def head(self, parts=None):
        output = []
        basis = np.array([[0, 0, -1], [1, 0, 0], [0, 1, 0]])
        for source in parts or self.parts:
            if source['limb'] != 8:
                continue
            v = source['vertices']
            normals = v[:, 6:9].astype(float)
            normals[normals > 127] -= 256
            triangles = source['tri'][np.all(source['joints'][source['tri']] == 8, axis=1)]
            output.append(dict(pos=v[:, :3] @ basis.T, normal=normals @ basis.T / 127,
                               uv=v[:, 4:6] / 32, tri=triangles,
                               texture=self.model.texture(source['texture'], None),
                               texture_scale=self.model.texture(source['texture'], None).shape[0] / 32,
                               tile=0x14050, origin=(.5, .5), prim=None, env=None,
                               cull_back=source['texture'].endswith('tex5') and not source['name'].endswith('browsDL')))
        return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('native', type=Path)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--hair', type=Path)
    parser.add_argument('--clothes', type=Path)
    parser.add_argument('--shirt', type=Path)
    parser.add_argument('--umbrella', type=Path)
    parser.add_argument('--export-only', action='store_true')
    parser.add_argument('--revision', choices=['R1', 'R2', 'R3', 'R4', 'R5'], default='R5')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    source = SourceModel(args.native, args.source)
    if args.hair:
        source.model.resources[SOURCE + 'tex5'] = texture_bytes(Image.open(args.hair))
        source.model.texture_cache.clear()
    if args.clothes:
        source.model.resources[SOURCE + 'tex8'] = texture_bytes(Image.open(args.clothes))
    if args.shirt:
        source.model.resources[SOURCE + 'shirtTex'] = texture_bytes(Image.open(args.shirt))
    if args.revision in ('R3', 'R4', 'R5'):
        fit_vest_and_heels(source)
    else:
        source.parts = [fit_heels(p) if p['limb'] in (12, 15) else p for p in source.parts]
    source.parts = separate_materials(source.parts, bool(args.shirt))
    if args.revision in ('R2', 'R3', 'R4', 'R5'):
        fit_umbrella_and_skirt(source, grip=args.revision not in ('R4', 'R5'))
    if args.revision == 'R4':
        fit_final_details(source, args.native, args.source)
    elif args.revision == 'R5':
        fit_contact_details(source, args.native, args.source)
    export_model(source, args.output / f'Anju_HD_Auburn_Umbrella_{args.revision}.o2r', args.umbrella, args.revision)
    if args.export_only:
        return
    render(source.head(), (900, 850), yaw=0, scale=.8, center_y=620).save(args.output / 'Head_Source.png')
    for amount in (.5, 1):
        render(source.head(blink_parts(source.parts, amount)), (900, 850), yaw=0, scale=.8, center_y=620).save(
            args.output / f'Head_Blink_{amount}.png')
    for p in source.parts:
        if p['limb'] in (8, 12, 15):
            v = p['vertices']
            print(p['name'].split('/')[-1], 'joints', np.unique(p['joints'], return_counts=True),
                  'xyz', v[:, :3].min(0), v[:, :3].max(0))
            if p['limb'] == 8 and p['texture'].endswith('tex2'):
                eye = v[(v[:, 0] > 300) & (v[:, 0] < 650) & (v[:, 1] > 200) & (np.abs(v[:, 2]) < 260)]
                np.savetxt(args.output / 'eye_vertices.txt', eye, fmt='%d')
    np.savez(args.output / 'source_geometry.npz', **{
        f'{p["name"].split("/")[-1]}_{key}': p[key] for p in source.parts for key in ('vertices', 'tri', 'joints')})


if __name__ == '__main__':
    main()
