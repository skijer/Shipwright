"""Local mesh fitting for the supplied HD Anju; no runtime or texture changes.

Coordinates are in the source mesh's N64 model units. The native umbrella shaft
runs diagonally across the palm; its measured axis determines the grip basis.
"""
import copy

import numpy as np


def smooth(t):
    t = np.clip(t, 0, 1)
    return t * t * (3 - 2 * t)


def close_thumb(v):
    """Curl the distal thumb toward the palm without detaching its nail."""
    pivot = np.array([300., -175., 125.])
    tangent = np.array([150., -120., 125.])
    tangent /= np.linalg.norm(tangent)
    inward = np.array([0., 1., -1.])
    inward -= tangent * np.dot(inward, tangent)
    inward /= np.linalg.norm(inward)
    across = np.cross(tangent, inward)
    offset = v - pivot
    distance, radial, width = offset @ tangent, offset @ inward, offset @ across
    angle = np.maximum(distance, 0) / 90
    bent = (pivot + ((90 - radial) * np.sin(angle))[:, None] * tangent +
            (90 - (90 - radial) * np.cos(angle))[:, None] * inward + width[:, None] * across)
    weight = smooth((v[:, 2] - 20) / 60) * smooth((-v[:, 1] - 30) / 75)
    weight *= distance > 0
    return v * (1 - weight[:, None]) + bent * weight[:, None]


def umbrella_grip(v):
    v = np.asarray(v, dtype=float)
    thumb = smooth((v[:, 2] - 20) / 60)
    v = close_thumb(v)
    x, y, z = v.T
    # Curl the four fingers from the palm into a wrap around the shaft.
    amount = np.maximum(x - 330, 0) / 125
    depth = z + 50
    bent = np.c_[330 + (125 - depth) * np.sin(amount), y, 75 - (125 - depth) * np.cos(amount)]
    curled = np.where((x > 330)[:, None], bent, v)
    out = curled * (1 - thumb[:, None]) + v * thumb[:, None]
    # A monotone angular closure keeps the web deformation gradual.
    oy, oz = out[:, 1] + 110, out[:, 2] - 30
    phi = np.mod(np.arctan2(oz, oy), 2 * np.pi)
    turn = smooth(phi / 2.6) * (1 - smooth((phi - 2.6) / 1.9))
    angle = -np.deg2rad(65) * turn * smooth((x - 140) / 180)
    out[:, 1] = oy * np.cos(angle) - oz * np.sin(angle) - 110
    out[:, 2] = oy * np.sin(angle) + oz * np.cos(angle) + 30
    # Orient the gripping palm to the diagonal native umbrella shaft.
    q = np.array([-1648, 2785, 1602.])
    q /= np.linalg.norm(q)
    p = np.array([q[1], -q[0], 0.])
    p /= np.linalg.norm(p)
    n = np.cross(p, q)
    angle = np.deg2rad(30)
    basis = np.stack([p * np.cos(angle) + n * np.sin(angle), q,
                      -p * np.sin(angle) + n * np.cos(angle)], axis=1)
    oriented = out @ basis.T
    w = smooth((x - 65) / 175)
    return v * (1 - w[:, None]) + oriented * w[:, None]


def deform_vertices(part, mask, mapping):
    if not np.any(mask):
        return part
    out = copy.deepcopy(part)
    v = out['vertices']
    before = v[mask, :3].astype(float)
    after = mapping(before)
    # The inverse-transpose Jacobian retains source smoothing and applies the
    # same map to nails and skin, independent of draw-list boundaries.
    eps = .1
    jac = np.stack([(mapping(before + axis * eps) - mapping(before - axis * eps)) / (2 * eps)
                    for axis in np.eye(3)], axis=2)
    assert np.all(np.linalg.det(jac) > .01), 'Sculpt folded a local volume inside out'
    normals = v[mask, 6:9].astype(float)
    normals[normals > 127] -= 256
    norms = np.linalg.solve(jac.transpose(0, 2, 1), normals[..., None])[..., 0]
    norms /= np.maximum(np.linalg.norm(norms, axis=1, keepdims=True), 1e-9)
    v[mask, :3] = np.round(after).astype(int)
    v[mask, 6:9] = np.round(norms * 127).astype(int) & 255
    return out


def fitted_skirt(v):
    x, y, z = v.T
    w = smooth((x - 250) / 2300)
    return np.c_[x, y * (1 - .30 * w), z * (1 - .28 * w)]


def trim_hidden_calves(part, world, body):
    """Remove upper calf surfaces concealed inside the closed skirt.

    Clip in the common fitting space. Existing lower vertices remain byte exact;
    new cut-edge vertices retain a neighboring native joint and lie inside the
    skirt. No exposed ankle or foot geometry is shortened or rescaled.
    """
    p = copy.deepcopy(part)
    v, joints = p['vertices'], p['joints']
    matrices = body @ world[joints]
    pos = np.einsum('nij,nj->ni', matrices[:, :3, :3], v[:, :3]) + matrices[:, :3, 3]
    normal = v[:, 6:9].astype(float)
    normal[normal > 127] -= 256
    normal = np.einsum('nij,nj->ni', matrices[:, :3, :3], normal)
    verts, owners, triangles, edges = v.tolist(), joints.tolist(), [], {}
    for face in p['tri']:
        polygon = []
        for a, b in zip(face, np.roll(face, -1)):
            da, db = pos[a, 0] - 2860, pos[b, 0] - 2860
            if da >= 0:
                polygon.append(int(a))
            if (da >= 0) != (db >= 0):
                key = tuple(sorted([int(a), int(b)]))
                if key not in edges:
                    amount = da / (da - db)
                    joint = int(joints[a] if amount < .5 else joints[b])
                    inverse = np.linalg.inv(body @ world[joint])
                    row = v[a].astype(float) * (1 - amount) + v[b] * amount
                    point = pos[a] * (1 - amount) + pos[b] * amount
                    row[:3] = inverse[:3, :3] @ point + inverse[:3, 3]
                    nn = inverse[:3, :3] @ (normal[a] * (1 - amount) + normal[b] * amount)
                    row[6:9] = nn / max(np.linalg.norm(nn), 1e-8) * 127
                    row = np.round(row).astype(int)
                    row[6:9] &= 255
                    edges[key] = len(verts)
                    verts.append(row.tolist())
                    owners.append(joint)
                polygon.append(edges[key])
        if len(polygon) >= 3:
            triangles.extend((polygon[0], polygon[i], polygon[i + 1]) for i in range(1, len(polygon) - 1))
    p['vertices'] = np.array(verts)
    p['joints'] = np.array(owners)
    p['tri'] = np.array(triangles)
    return p


def fit_umbrella_and_skirt(source, grip=True):
    """Fit only the gripping hand, lower garment and concealed upper calves.

    Use the source standing frame as a common space so vertices shared by
    different display lists receive the same sculpt. Bone offsets, ownership,
    animation, the opposite hand and exposed ankle/foot seams remain intact.
    """
    rest = source.model.world(source.model.clips[source.model.animation][0])
    body = np.linalg.inv(rest[9])
    result = []
    for part in source.parts:
        if grip and part['limb'] == 7:
            mask = (part['joints'] == 7) & (part['vertices'][:, 0] > 65)
            part = deform_vertices(part, mask, umbrella_grip)
        if part['name'].split('/')[-1] in ('batch24DL', 'batch31DL'):
            part = trim_hidden_calves(part, rest, body)
        if part['limb'] >= 9 and part['texture'].endswith('tex8'):
            for joint in np.unique(part['joints']):
                matrix = body @ rest[joint]
                inverse = np.linalg.inv(matrix)

                def mapping(v):
                    v = v @ matrix[:3, :3].T + matrix[:3, 3]
                    v = fitted_skirt(v)
                    return v @ inverse[:3, :3].T + inverse[:3, 3]

                part = deform_vertices(part, part['joints'] == joint, mapping)
        result.append(part)
    source.parts = result


def smooth_heels(v, limb):
    """Open the arch through the entire shoe section, preserving the ankle.

    The earlier sole-only lift could pass through vertices higher on the vamp.
    This continuous map compresses that whole section together. Toe and heel
    ground contacts, and the shin/ankle weld above local Y=560, stay fixed.
    """
    x, y, z = v.T.copy()
    arch = 145 * smooth((x - 150) / 215) * (1 - smooth((x - 365) / 335))
    y -= arch * smooth((v[:, 1] - 680) / 300)
    taper = smooth((v[:, 1] - 800) / 160) * (1 - smooth((v[:, 0] - 170) / 70))
    x = 75 + (x - 75) * (1 - .45 * taper)
    center = -76 if limb == 12 else 76
    z = center + (z - center) * (1 - .48 * taper)
    lift = 75 * smooth((v[:, 1] - 560) / 120) * (1 - smooth((v[:, 1] - 850) / 80))
    lift *= 1 - smooth((v[:, 0] - 200) / 350)
    y -= lift
    return np.c_[x, y, z]


def reverse_bind_axes(vertices, mask):
    """Convert between torso and pelvis bind coordinates (a half-turn in Z)."""
    vertices[mask, :2] *= -1
    normals = vertices[mask, 6:9].astype(int)
    normals[normals > 127] -= 256
    normals[:, :2] *= -1
    vertices[mask, 6:9] = normals & 255


def tuck_vest(v):
    # Extend only the last hem ring into the belt, narrowing it underneath.
    # The visible upper vest and the separate buttons keep their source shape.
    w = 1 - smooth((v[:, 0] + 20) / 100)
    return np.c_[v[:, 0] - 150 * w, v[:, 1] * (1 - .08 * w), v[:, 2] * (1 - .08 * w)]


def fit_vest_and_heels(source):
    """R3 source-space correction, applied before the existing R2 fitting.

    The supplied vest and lowest button straddled torso/pelvis/skirt bones.
    Rebind each button completely to the torso, and the complete belt ring to
    the pelvis. Tuck the vest's lowest ring inside that belt and attach it to
    the same bone, retaining the bend between it and the upper torso. Include
    duplicate vertices at UV seams to avoid opening cracks.
    """
    vest_parts = ('batch4DL', 'batch5DL', 'batch20DL', 'batch36DL')
    belt_keys = set()
    for part in source.parts:
        if part['name'].split('/')[-1] not in ('batch3DL', 'batch19DL', 'batch35DL'):
            continue
        for vertex, joint in zip(part['vertices'], part['joints']):
            if vertex[4] > 715 and vertex[5] <= 56:
                belt_keys.add((int(joint), *map(int, vertex[:3])))
    result = []
    for original in source.parts:
        part = copy.deepcopy(original)
        name = part['name'].split('/')[-1]
        vertices, joints = part['vertices'], part['joints']
        if part['limb'] in (12, 15):
            limb = part['limb']
            positions = vertices[:, :3].astype(float)
            changed = np.any(np.abs(smooth_heels(positions, limb) - positions) > 1e-8, axis=1)
            part = deform_vertices(part, (joints == limb) & changed, lambda v: smooth_heels(v, limb))
        elif name in vest_parts:
            assert set(joints) <= {1, 9, 16}
            reverse_bind_axes(vertices, joints != 1)
            joints[:] = 1
            if name in ('batch4DL', 'batch20DL'):
                hem = vertices[:, 0] <= 0
                part = deform_vertices(part, vertices[:, 0] < 80, tuck_vest)
                reverse_bind_axes(part['vertices'], hem)
                part['joints'][hem] = 9
        else:
            mask = np.array([(int(j), *map(int, v[:3])) in belt_keys for v, j in zip(vertices, joints)])
            if np.any(mask):
                assert set(joints[mask]) <= {1, 9, 16}
                reverse_bind_axes(vertices, mask & (joints == 1))
                joints[mask] = 9
        result.append(part)
    source.parts = result


def relaxed_left_fingers(v):
    """Small finger curl for a separate standing-only candidate hand."""
    x, y, z = v.T
    radius = 850
    angle = np.maximum(x - 360, 0) / radius
    depth = z - 30
    bent = np.c_[360 + (radius + depth) * np.sin(angle), y,
                 30 - radius + (radius + depth) * np.cos(angle)]
    # The thumb and palm remain open; every nail follows its finger's map.
    weight = smooth((z + 80) / 70) * (x > 360)
    return v * (1 - weight[:, None]) + bent * weight[:, None]


def standing_left_hand(parts):
    result = []
    for part in parts:
        if part['limb'] == 4:
            mask = (part['joints'] == 4) & (part['vertices'][:, 0] > 360)
            result.append(deform_vertices(part, mask, relaxed_left_fingers))
    return result
