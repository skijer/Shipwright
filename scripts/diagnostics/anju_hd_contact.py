"""R5: fitted prop contact, a folded thumb, and the original left arm.

R4's broad radial clearance field left the grip hovering and its elbow loft
changed the arm silhouette. Preserve the R3 arm, separate the thumb and finger
curls, and trim only skin buried inside the permanently held umbrella shaft.
"""
import copy

import numpy as np

from anju_hd_fit import close_thumb, deform_vertices, smooth
from anju_hd_finish import HOOK_POINTS, SHAFT_AXIS, UMBRELLA_VERTICES, ankle_taper, grip_basis
from render_anju_preview import Anju, read_vertex_array


def original_grip_basis():
    p = np.array([SHAFT_AXIS[1], -SHAFT_AXIS[0], 0.])
    p /= np.linalg.norm(p)
    normal = np.cross(p, SHAFT_AXIS)
    angle = np.pi / 6
    return np.stack([p * np.cos(angle) + normal * np.sin(angle), SHAFT_AXIS,
                     -p * np.sin(angle) + normal * np.cos(angle)], axis=1)


def folded_thumb(vertices):
    """Retain the compact R3 thumb fold in its unrotated palm coordinates."""
    thumb = smooth((vertices[:, 2] - 20) / 60)
    vertices = close_thumb(vertices)
    x, y, z = vertices.T.copy()
    angle, depth = np.maximum(x - 330, 0) / 125, z + 50
    bent = np.c_[330 + (125 - depth) * np.sin(angle), y, 75 - (125 - depth) * np.cos(angle)]
    result = np.where((x > 330)[:, None], bent, vertices) * (1 - thumb[:, None]) + vertices * thumb[:, None]
    oy, oz = result[:, 1] + 110, result[:, 2] - 30
    phi = np.mod(np.arctan2(oz, oy), 2 * np.pi)
    angle = -np.deg2rad(65) * smooth(phi / 2.6) * (1 - smooth((phi - 2.6) / 1.9))
    angle *= smooth((x - 140) / 180)
    result[:, 1] = oy * np.cos(angle) - oz * np.sin(angle) - 110
    result[:, 2] = oy * np.sin(angle) + oz * np.cos(angle) + 30
    return result


def contact_hand(vertices):
    # Curl the fingers independently, so the thumb's opposing turn does not
    # drag the other fingertips sideways into thin, stretched shapes.
    x, y, z = vertices.T.copy()
    angle, depth = np.maximum(x - 330, 0) / 135, z + 50
    bent = np.c_[330 + (135 - depth) * np.sin(angle), y, 85 - (135 - depth) * np.cos(angle)]
    fingers = np.where((x > 330)[:, None], bent, vertices)
    thumb = smooth((z - 20) / 60)
    return fingers * (1 - thumb[:, None]) + folded_thumb(vertices) * thumb[:, None]


def trim_shaft_contact(part, ring, margin=.75):
    """Subtract the shaft prism from buried contact skin, retaining its outline.

    The hand and shaft share joint 7 permanently. Only all-joint-7 triangles
    inside its contact surface are cut; the forearm/wrist seam remains exact.
    The cut boundary hugs the prop instead of pushing the palm away from it.
    This is a prop-specific contact mesh, not a hand for use without a shaft.
    """
    first = ring[1] - ring[0]
    first -= SHAFT_AXIS * (first @ SHAFT_AXIS)
    first /= np.linalg.norm(first)
    basis = np.stack([first, np.cross(SHAFT_AXIS, first)], axis=1)
    section = ring @ basis
    a, b = section[1] - section[0], section[2] - section[0]
    winding = np.sign(a[0] * b[1] - a[1] * b[0])
    planes = []
    for a, b in zip(section, np.roll(section, -1, axis=0)):
        edge = b - a
        normal = np.array([-edge[1], edge[0]]) * winding
        normal /= np.linalg.norm(normal)
        planes.append((basis @ normal, a @ normal - margin))
    result = copy.deepcopy(part)
    vertices, owners = part['vertices'], part['joints']
    rows, joints, triangles = vertices.tolist(), owners.tolist(), []

    def clip(polygon, normal, offset, inside):
        output = []
        for a, b in zip(polygon, polygon[1:] + polygon[:1]):
            da, db = a[:3] @ normal - offset, b[:3] @ normal - offset
            keep_a, keep_b = (da >= 0, db >= 0) if inside else (da <= 0, db <= 0)
            if keep_a:
                output.append(a)
            if keep_a != keep_b:
                output.append(a + (b - a) * da / (da - db))
        return output

    for face in part['tri']:
        if not np.all(owners[face] == 7):
            triangles.append(face.tolist())
            continue
        original = vertices[face].astype(float)
        original[:, 6:9] = np.where(original[:, 6:9] > 127, original[:, 6:9] - 256, original[:, 6:9])
        polygon = list(original)
        for normal, offset in planes:
            polygon = clip(polygon, normal, offset, True)
            if len(polygon) < 3:
                break
        if len(polygon) < 3:
            triangles.append(face.tolist())
            continue
        polygon, kept = list(original), []
        for normal, offset in planes:
            outside = clip(polygon, normal, offset, False)
            if len(outside) >= 3:
                kept.append(outside)
            polygon = clip(polygon, normal, offset, True)
            if len(polygon) < 3:
                break
        for polygon in kept:
            indices = []
            for source_row in polygon:
                row = source_row.copy()
                row[6:9] *= 127 / max(np.linalg.norm(row[6:9]), 1e-9)
                row = np.round(row).astype(int)
                row[6:9] &= 255
                indices.append(len(rows))
                rows.append(row.tolist())
                joints.append(7)
            for at in range(1, len(indices) - 1):
                face = [indices[0], indices[at], indices[at + 1]]
                points = np.array([rows[index][:3] for index in face])
                area = np.linalg.norm(np.cross(points[1] - points[0], points[2] - points[0]))
                if area > .5:
                    triangles.append(face)
    result.update(vertices=np.array(rows), joints=np.array(joints), tri=np.array(triangles))
    return result


def fit_contact_details(source, native_path, source_path):
    base = copy.deepcopy(source.parts)
    original = np.array(read_vertex_array(source.model.resources[UMBRELLA_VERTICES]), dtype=int)
    ring_points = ((230, 52, 161), (260, 8, 267), (332, 84, 211))
    ring_indices = [next(i for i, row in enumerate(original) if tuple(row[:3]) == point) for point in ring_points]
    hook = np.array([tuple(row[:3]) in HOOK_POINTS for row in original])
    for standing in (False, True):
        model = Anju(native_path, standing, source_path)
        world = model.world(model.clips[model.animation][0])
        basis = grip_basis(world)
        parts = []
        for part in base:
            if part['limb'] in (6, 7):
                part = deform_vertices(part, part['joints'] == 7, lambda v: contact_hand(v) @ basis.T)
            if part['name'].endswith(('batch31DL', 'batch34DL')):
                mask = (part['joints'] == 15) & (part['vertices'][:, 1] > 160) & (part['vertices'][:, 1] < 740)
                part = deform_vertices(part, mask, ankle_taper)
            parts.append(part)
        rotation = basis @ original_grip_basis().T
        umbrella = original.copy()
        umbrella[:, :3] = np.round(original[:, :3] @ rotation.T + hook[:, None] * SHAFT_AXIS * 280).astype(int)
        normals = original[:, 6:9].astype(float)
        normals[normals > 127] -= 256
        umbrella[:, 6:9] = np.round(normals @ rotation.T).astype(int) & 255
        ring = umbrella[ring_indices, :3].astype(float)
        parts = [trim_shaft_contact(part, ring) if part['limb'] == 7 else part for part in parts]
        if standing:
            source.standing_parts, source.standing_umbrella = parts, umbrella
        else:
            source.parts, source.seated_umbrella = parts, umbrella
