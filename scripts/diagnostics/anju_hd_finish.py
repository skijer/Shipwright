"""R4 pose-fitted grips, continuous elbow skin, and a bounded ankle taper.

These are archive geometry corrections. Native joints, animation samples,
placement offsets, and the runtime resource ownership contract are unchanged.
"""
import copy

import numpy as np

from anju_hd_fit import deform_vertices, smooth
from render_anju_preview import Anju, read_vertex_array


SHAFT_AXIS = np.array([-1648., 2785., 1602.])
SHAFT_AXIS /= np.linalg.norm(SHAFT_AXIS)
SHAFT_BASE = np.array([274., 48., 213.])
GRIP_CENTER = np.array([330., 0., 105.])
UMBRELLA_VERTICES = 'objects/object_an2/object_an2Vtx_000000'
HOOK_POINTS = {
    (3, 586, 426), (66, 398, 180), (209, 488, -48), (210, 243, -15),
    (230, 52, 161), (239, 451, 266), (260, 8, 267), (295, 515, -5),
    (332, 84, 211), (383, 296, 71),
}


def grip_basis(world):
    forward = np.linalg.inv(world[7])[:3, :3] @ world[6, :3, 0]
    forward -= SHAFT_AXIS * np.dot(forward, SHAFT_AXIS)
    forward /= np.linalg.norm(forward)
    return np.stack([forward, SHAFT_AXIS, np.cross(forward, SHAFT_AXIS)], axis=1)


def gripping_hand(vertices):
    """Oppose the thumb and curl the fingers through one continuous map.

    The monotone polar turn keeps the thumb web from folding through the palm.
    Apply this same map to nail vertices and both copies of the wrist seam.
    """
    x, y, z = vertices.astype(float).T.copy()
    oy, oz = y + 80, z - 20
    phi = np.mod(np.arctan2(oz, oy), 2 * np.pi)
    angle = np.deg2rad(55) * smooth(phi / 2.2) * (1 - smooth((phi - 2.2) / 2.3))
    angle *= smooth((x - 140) / 180)
    y = oy * np.cos(angle) - oz * np.sin(angle) - 80
    z = oy * np.sin(angle) + oz * np.cos(angle) + 20
    y += .45 * np.maximum(-y - 180, 0)
    before = np.c_[x, y, z]
    angle = np.maximum(x - 330, 0) / 155
    depth = z + 50
    curled = np.c_[330 + (155 - depth) * np.sin(angle), y, 105 - (155 - depth) * np.cos(angle)]
    result = np.where((x > 330)[:, None], curled, before)
    # Keep the inner skin outside the triangular shaft's 68-unit circumradius.
    # This field has a positive radial derivative and fades out at radius 150.
    radial = result[:, [0, 2]] - GRIP_CENTER[[0, 2]]
    radius = np.linalg.norm(radial, axis=1)
    fitted = radius + (82 - .75 * radius) * (1 - smooth((radius - 85) / 65))
    result[:, [0, 2]] = GRIP_CENTER[[0, 2]] + radial * (fitted / np.maximum(radius, 1e-6))[:, None]
    return result


def ankle_taper(vertices):
    x, y, z = vertices.T
    weight = smooth((y - 160) / 240) * (1 - smooth((y - 520) / 220))
    return np.c_[100 + (x - 100) * (1 - .16 * weight), y, 70 + (z - 70) * (1 - .16 * weight)]


def fitted_umbrella(rows, basis):
    result = rows.copy()
    hook = np.array([tuple(row[:3]) in HOOK_POINTS for row in rows])
    translation = basis @ GRIP_CENTER - SHAFT_BASE
    # Move the accessory into the palm instead of sharply twisting the wrist.
    # Extend its straight handle so the wide hook starts below the pinkie.
    result[:, :3] = np.round(rows[:, :3] + translation + hook[:, None] * SHAFT_AXIS * 280).astype(int)
    return result


def elbow_patch(parts, world):
    """Replace the intersecting inner elbow with a joined, rounded skin patch.

    Preserve both boundary loops exactly. The patch follows a rounded bend;
    its inner radius narrows at the crease. Both existing bone owners remain.
    Fit each native pose independently because their elbow angles differ.
    """
    parts = copy.deepcopy(parts)
    keys, rows, joints, faces, part_indices = {}, [], [], [], []
    for part in parts:
        indices = []
        for row, joint in zip(part['vertices'], part['joints']):
            key = (int(joint), *row[:3])
            if key not in keys:
                keys[key] = len(rows)
                rows.append(row.copy())
                joints.append(joint)
            indices.append(keys[key])
        indices = np.array(indices)
        part_indices.append(indices)
        faces.extend(indices[part['tri']])
    vertices, joints, faces = np.array(rows), np.array(joints), np.array(faces)
    free = ((joints == 2) & (vertices[:, 0] > 833)) | ((joints == 3) & (vertices[:, 0] < 440))
    removed = faces[np.any(free[faces], axis=1)]
    edges = np.sort(np.concatenate([removed[:, [0, 1]], removed[:, [1, 2]], removed[:, [2, 0]]]), axis=1)
    unique, counts = np.unique(edges, axis=0, return_counts=True)
    boundary = unique[counts == 1]
    boundary_keys = {tuple(edge) for edge in boundary}
    edge_rows = {}
    for part, indices in zip(parts, part_indices):
        for face in part['tri']:
            ids = indices[face]
            if not np.any(free[ids]):
                continue
            for a, b in zip(range(3), (1, 2, 0)):
                if tuple(sorted((ids[a], ids[b]))) in boundary_keys:
                    edge_rows[(ids[a], ids[b])] = (part['vertices'][face[a]].copy(), part['vertices'][face[b]].copy())

    def boundary_rows(a, b):
        if (a, b) in edge_rows:
            return edge_rows[(a, b)]
        return edge_rows[(b, a)][::-1]
    ends = [np.unique(boundary[np.all(joints[boundary] == joint, axis=1)]) for joint in (2, 3)]
    assert list(map(len, ends)) == [11, 15], 'Unexpected source elbow boundary'
    matrices = world[joints]
    positions = np.einsum('nij,nj->ni', matrices[:, :3, :3], vertices[:, :3]) + matrices[:, :3, 3]
    start, end = [positions[indices].mean(axis=0) for indices in ends]
    first_tangent, last_tangent = world[2, :3, 0], world[3, :3, 0]
    axis = np.cross(first_tangent, last_tangent)
    axis /= np.linalg.norm(axis)
    theta = np.arccos(np.clip(first_tangent @ last_tangent, -1, 1))
    first_normal = np.cross(axis, first_tangent)
    first_length, last_length = np.linalg.lstsq(
        np.stack([first_tangent, last_tangent], axis=1), end - start, rcond=None)[0]
    corner = (start + first_length * first_tangent + end - last_length * last_tangent) / 2
    distance = min(first_length, last_length) * .8
    radius = distance / np.tan(theta / 2)
    circle_center = corner - first_tangent * distance + first_normal * radius

    def rotate(vector, angle):
        return (vector * np.cos(angle) + np.cross(axis, vector) * np.sin(angle) +
                axis * (axis @ vector) * (1 - np.cos(angle)))

    angles, profiles, endpoint_rows, closing_rows, seam_angles = [], [], [], [], []
    for index, (indices, center, tangent) in enumerate(zip(ends, [start, end], [first_tangent, last_tangent])):
        normal = np.cross(axis, tangent)
        delta = positions[indices] - center
        angle = np.mod(np.arctan2(delta @ axis, delta @ normal), 2 * np.pi)
        order = np.argsort(angle)
        ordered = indices[order]
        seam = []
        for at, vertex in enumerate(ordered):
            incoming = boundary_rows(ordered[at - 1], vertex)[1]
            outgoing = boundary_rows(vertex, ordered[(at + 1) % len(ordered)])[0]
            if not np.array_equal(incoming[4:6], outgoing[4:6]):
                seam.append(at)
        assert len(seam) == 1, 'Expected one UV wrap seam per elbow boundary'
        seam_angle = angle[order[seam[0]]]
        relative = np.mod(angle - seam_angle, 2 * np.pi)
        order = np.argsort(relative)
        angles.append(relative[order])
        ends[index] = indices[order]
        seam_angles.append(seam_angle)
        endpoint_rows.append(np.array([
            boundary_rows(a, b)[0] for a, b in zip(ends[index], np.roll(ends[index], -1))]))
        closing_rows.append(boundary_rows(ends[index][-1], ends[index][0])[1])
        profiles.append([np.ptp(delta @ normal) / 2, np.ptp(delta @ axis) / 2])
    seam_angles = np.unwrap(seam_angles)
    profiles = np.array(profiles)
    new_rows, new_joints, new_positions, rings, parameters, triangles = [], [], [], [], [], []
    seam_copies = {}

    def add_ring(points, attributes, owner, ring_angles, closing, unchanged=False):
        indices = []
        inverse = np.linalg.inv(world[owner])
        for point, attribute in zip(points, attributes):
            row = attribute.copy()
            if not unchanged:
                row[:3] = np.round(inverse[:3, :3] @ point + inverse[:3, 3]).astype(int)
            indices.append(len(new_rows))
            new_rows.append(row)
            new_joints.append(owner)
            new_positions.append(point)
        rings.append(np.array(indices))
        parameters.append(ring_angles)
        seam_copies[indices[0]] = closing.copy()

    add_ring(positions[ends[0]], endpoint_rows[0], 2, angles[0], closing_rows[0], unchanged=True)
    turns, circumference = np.linspace(0, theta, 11), np.linspace(0, 2 * np.pi, 32, endpoint=False)
    for index, turn in enumerate(turns):
        fraction = (index + 1) / (len(turns) + 1)
        normal = rotate(first_normal, turn)
        # The outward offset leaves room between the two arms of the deep bend.
        center = circle_center - normal * radius * 1.9
        size = profiles[0] * (1 - fraction) + profiles[1] * fraction
        physical_angle = circumference + seam_angles[0] * (1 - fraction) + seam_angles[1] * fraction
        cosine, sine = np.cos(physical_angle), np.sin(physical_angle)
        radial = np.where(cosine > 0, min(size[0], radius * .6), size[0])
        points = center + np.outer(cosine * radial, normal) + np.outer(sine * size[1], axis)
        attributes = []
        for angle in circumference:
            samples = []
            for endpoint, endpoint_angles, closing in zip(endpoint_rows, angles, closing_rows):
                extended = np.r_[endpoint_angles, 2 * np.pi]
                values = np.vstack([endpoint, closing]).astype(float)
                values[:, 6:9] = np.where(values[:, 6:9] > 127, values[:, 6:9] - 256, values[:, 6:9])
                at = max(0, np.searchsorted(extended, angle, side='right') - 1)
                weight = (angle - extended[at]) / (extended[at + 1] - extended[at])
                samples.append(values[at] * (1 - weight) + values[at + 1] * weight)
            row = np.round(samples[0] * (1 - fraction) + samples[1] * fraction).astype(int)
            row[6:9] &= 255
            attributes.append(row)
        closing = np.round(closing_rows[0] * (1 - fraction) + closing_rows[1] * fraction).astype(int)
        add_ring(points, attributes, 2 if turn < theta * .5 else 3, circumference, closing)
    add_ring(positions[ends[1]], endpoint_rows[1], 3, angles[1], closing_rows[1], unchanged=True)

    # Join loops with unequal vertex counts without adding T-junctions to the
    # preserved source boundaries. Each original boundary edge appears once.
    for ring_a, ring_b, angle_a, angle_b in zip(rings, rings[1:], parameters, parameters[1:]):
        a = b = 0
        count_a, count_b = len(ring_a), len(ring_b)
        while a < count_a or b < count_b:
            next_a = (angle_a[(a + 1) % count_a] + (2 * np.pi if a + 1 >= count_a else 0)
                      if a < count_a else np.inf)
            next_b = (angle_b[(b + 1) % count_b] + (2 * np.pi if b + 1 >= count_b else 0)
                      if b < count_b else np.inf)
            if next_a < next_b:
                triangles.append([ring_a[a % count_a], ring_a[(a + 1) % count_a], ring_b[b % count_b]])
                a += 1
            else:
                triangles.append([ring_a[a % count_a], ring_b[(b + 1) % count_b], ring_b[b % count_b]])
                b += 1
    new_rows, new_joints = np.array(new_rows), np.array(new_joints)
    new_positions, triangles = np.array(new_positions), np.array(triangles)
    face_normals = np.cross(new_positions[triangles[:, 1]] - new_positions[triangles[:, 0]],
                            new_positions[triangles[:, 2]] - new_positions[triangles[:, 0]])
    normals = np.zeros_like(new_positions)
    for corner_index in range(3):
        np.add.at(normals, triangles[:, corner_index], face_normals)
    normals /= np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-9)
    endpoints = set(rings[0]) | set(rings[-1])
    for index in range(len(new_rows)):
        if index not in endpoints:
            new_rows[index, 6:9] = np.round(world[new_joints[index], :3, :3].T @ normals[index] * 127).astype(int) & 255
    # Duplicate UV corners at the wrap, keeping their positions and smoothing
    # welded. This retains both original UV values at each boundary seam.
    theta_by_vertex = np.zeros(len(new_rows))
    for ring, values in zip(rings, parameters):
        theta_by_vertex[ring] = values
    duplicates = {}
    additional_rows, additional_joints = [], []
    for face in triangles:
        if np.ptp(theta_by_vertex[face]) <= np.pi:
            continue
        for at, index in enumerate(face):
            if index not in seam_copies:
                continue
            if index not in duplicates:
                row = new_rows[index].copy()
                row[4:6] = seam_copies[index][4:6]
                duplicates[index] = len(new_rows) + len(additional_rows)
                additional_rows.append(row)
                additional_joints.append(new_joints[index])
            face[at] = duplicates[index]
    new_rows = np.vstack([new_rows, additional_rows])
    new_joints = np.r_[new_joints, additional_joints]
    for part, indices in zip(parts, part_indices):
        part['tri'] = part['tri'][~np.any(free[indices[part['tri']]], axis=1)]
    patch = copy.deepcopy(parts[0])
    patch.update(name=parts[0]['name'].removesuffix('batch7DL') + 'leftElbowFitDL',
                 vertices=new_rows, joints=new_joints, tri=triangles, limb=2)
    return parts + [patch]


def fit_pose(parts, world):
    basis = grip_basis(world)
    result = []
    for part in parts:
        if part['limb'] in (6, 7):
            part = deform_vertices(part, part['joints'] == 7, lambda v: gripping_hand(v) @ basis.T)
        if part['name'].endswith(('batch31DL', 'batch34DL')):
            mask = (part['joints'] == 15) & (part['vertices'][:, 1] > 160) & (part['vertices'][:, 1] < 740)
            part = deform_vertices(part, mask, ankle_taper)
        result.append(part)
    elbow = [part for part in result if part['name'].endswith(('batch7DL', 'batch8DL'))]
    result = [part for part in result if not part['name'].endswith(('batch7DL', 'batch8DL'))]
    return result + elbow_patch(elbow, world), basis


def fit_final_details(source, native_path, source_path):
    original = copy.deepcopy(source.parts)
    rows = np.array(read_vertex_array(source.model.resources[UMBRELLA_VERTICES]), dtype=int)
    for standing in (False, True):
        model = Anju(native_path, standing, source_path)
        world = model.world(model.clips[model.animation][0])
        parts, basis = fit_pose(original, world)
        umbrella = fitted_umbrella(rows, basis)
        if standing:
            source.standing_parts = parts
            source.standing_umbrella = umbrella
        else:
            source.parts = parts
            source.seated_umbrella = umbrella
