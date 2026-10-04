"""Apply supplied goth artwork and material colors to the approved R5 archive.

Geometry is decoded only to separate shoe and ankle materials. Every triangle,
bone owner, normal, UV and vertex position must match the R5 input exactly.
"""
from pathlib import Path
import argparse
import copy
import hashlib
import json
import zipfile

import numpy as np
from PIL import Image

from build_anju_hd import PREFIX, SourceModel, dl_bytes, export_mesh, hashed, texture_bytes
from render_anju_preview import commands


def geometry(parts):
    """Canonical triangles, including winding and every original attribute."""
    result = []
    for part in parts:
        for face in part['tri']:
            corners = [tuple(map(int, part['vertices'][i])) + (int(part['joints'][i]),) for i in face]
            cycle = min(tuple(corners[i:] + corners[:i]) for i in range(3))
            result.append((part['limb'], cycle))
    return sorted(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('native', 'r5', 'artwork', 'output'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.r5) as archive:
        assert archive.testzip() is None
        files = {name: archive.read(name) for name in archive.namelist()}
    original = dict(files)
    meta = json.loads(files['Anju_HD_README.json'])
    assert meta['revision'] == 'R5'
    artwork = {
        'tex5': 'hair_black.png',
        'tex8': 'clothes_charcoal.png',
        'shirtTex': 'shirt_and_shoes_black.png',
    }
    for texture, filename in artwork.items():
        with Image.open(args.artwork / filename) as image:
            files[PREFIX + texture] = texture_bytes(image)

    model = SourceModel(args.native, args.r5)
    # Sleeve and torso shoulder-cap faces sit just beyond the old shirt UV
    # classifier. They are cloth, not skin; only change their material binding.
    shoulder_faces = {'batch0DL': 20, 'batch6DL': 7, 'batch11DL': 7}
    for part in model.parts:
        stem = part['name'].split('/')[-1]
        if stem not in shoulder_faces:
            continue
        assert len(part['tri']) == shoulder_faces[stem] and part['texture'] == PREFIX + 'tex2'
        words = []
        for op, a, b, h in commands(files[part['name']]):
            words.append((a, b))
            if op == 0x20:
                words[-1:] = hashed(op, PREFIX + 'shirtTex', b, a & 0xFFFFFF)
            elif h is not None:
                words.append((h >> 32, h & 0xFFFFFFFF))
        files[part['name']] = dl_bytes(words)
    shoe_counts = {}
    for part in model.parts:
        if part['limb'] not in (12, 15):
            continue
        assert part['name'].endswith(('batch27DL', 'batch34DL'))
        face_v = part['vertices'][part['tri'], 5]
        shoe = np.all(face_v >= 700, axis=1)
        skin = np.all(face_v < 600, axis=1)
        assert np.all(shoe ^ skin), 'Unexpected shoe/ankle UV boundary'
        root = model.model.dlists[part['limb']]
        words = hashed(0x33, root, 0xBEEFBEEF)
        for label, selected, texture in (('GothShoe', shoe, 'shirtTex'), ('Skin', skin, 'tex2')):
            child = copy.deepcopy(part)
            child['tri'] = part['tri'][selected]
            child['texture'] = PREFIX + texture
            stem = part['name'].split('/')[-1].removesuffix('DL') + label + 'DL'
            words += hashed(0x31, export_mesh(child, stem, files))
        files[root] = dl_bytes(words + [(0xDF000000, 0)])
        shoe_counts[str(part['limb'])] = {'shoe_triangles': int(shoe.sum()), 'skin_triangles': int(skin.sum())}

    # The native canopy uses a monochrome pattern with material colors. Keep
    # that artwork and every vertex; change only its brown/gold prim/env colors.
    colors = {0x3C1400FF: 0x15171BFF, 0xAFA064FF: 0xAAAEB5FF,
              0xC38228FF: 0x555A63FF, 0x281400FF: 0x15171BFF}
    for name in ('gAnju2UmbrellaDL', 'gAnju2UmbrellaStandingDL'):
        words = []
        changed = 0
        for op, a, b, h in commands(files[PREFIX + name]):
            if op in (0xFA, 0xFB) and b in colors:
                b = colors[b]
                changed += 1
            words.append((a, b))
            if h is not None:
                words.append((h >> 32, h & 0xFFFFFFFF))
        assert changed == 4
        files[PREFIX + name] = dl_bytes(words)

    meta.update(name='Anju HD Goth Umbrella R5', palette='Goth', geometry_revision='R5')
    meta['notes'] += [
        'Black hair and brows; charcoal clothes and heels; silver-gray trim and umbrella.',
        'Original skin, face, blue eyes and blink geometry retained exactly.',
        'Shoe triangles use the supplied black artwork; ankle triangles retain the original skin atlas.',
        'All R5 geometry, UVs, normals, bone ownership and placement anchors are unchanged.',
        'Choose either this archive or the auburn R5 archive; both use the same private namespace.',
    ]
    files['Anju_HD_README.json'] = json.dumps(meta, indent=2).encode()
    out = args.output / 'Anju_HD_Goth_Umbrella_R5.o2r'
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, data in files.items():
            archive.writestr(name, data)
    with zipfile.ZipFile(out) as archive:
        assert archive.testzip() is None
    for standing in (False, True):
        base = SourceModel(args.native, args.r5, standing_preview=standing)
        edited = SourceModel(args.native, out, standing_preview=standing)
        assert geometry(base.parts) == geometry(edited.parts)
    changed = sorted(name for name in original if original[name] != files[name])
    expected = {'Anju_HD_README.json'} | {PREFIX + name for name in (
        'tex5', 'tex8', 'shirtTex', 'batch0DL', 'batch6DL', 'batch11DL',
        'gAnju2UmbrellaDL', 'gAnju2UmbrellaStandingDL')} | {
        model.model.dlists[limb] for limb in (12, 15)}
    assert set(changed) == expected
    report = dict(geometry_exact_r5=True, skin_face_eyes_exact_r5=True,
                  standing_and_seated_checked=True, shoe_materials=shoe_counts,
                  changed_existing_resources=changed,
                  added_resources=sorted(set(files) - set(original)),
                  resources=len(files), size_bytes=out.stat().st_size,
                  sha256=hashlib.sha256(out.read_bytes()).hexdigest())
    (args.output / 'palette_validation.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
