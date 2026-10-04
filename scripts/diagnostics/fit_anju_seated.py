"""Seated-only Anju repair from a preserved R8 archive.

Rebuild the skirt as one continuous fitted surface, preserve the original belt,
and smooth the connected crying forearm between its original boundaries.
Standing roots retain the original resource graph. No texture, skeleton,
animation, placement, hand or umbrella changes.
"""

from pathlib import Path
import argparse
import copy
import functools
import io
import json
import zipfile

import numpy as np
from scipy.interpolate import PchipInterpolator

import render_anju_preview as preview
from build_anju_hd import SourceModel, dl_bytes, export_mesh, hashed
from verify_anju_pose_selection import SKIRT

P = "objects/object_anju_hd/v1/"


def posed(vertices, joints, world):
    return (
        np.einsum("nij,nj->ni", world[joints, :3, :3], vertices[:, :3])
        + world[joints, :3, 3]
    )


def mesh_part(name, limb, texture, positions, joints, uv, triangles, world):
    positions, joints, uv, triangles = map(
        np.asarray, (positions, joints, uv, triangles)
    )
    normals = np.zeros_like(positions, dtype=float)
    faces = np.cross(
        positions[triangles[:, 1]] - positions[triangles[:, 0]],
        positions[triangles[:, 2]] - positions[triangles[:, 0]],
    )
    for i in range(3):
        np.add.at(normals, triangles[:, i], faces)
    # Texture seams share geometry and must also share smooth normals.
    _, ids = np.unique(np.round(positions, 4), axis=0, return_inverse=True)
    shared = np.zeros((ids.max() + 1, 3))
    np.add.at(shared, ids, normals)
    normals = shared[ids]
    normals /= np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-9)
    rotation = world[joints, :3, :3].transpose(0, 2, 1)
    local = np.einsum("nij,nj->ni", rotation, positions - world[joints, :3, 3])
    local_normal = np.einsum("nij,nj->ni", rotation, normals)
    rows = np.zeros((len(positions), 10), dtype=int)
    rows[:, :3] = np.round(local).astype(int)
    rows[:, 4:6] = np.round(uv).astype(int)
    rows[:, 6:9] = np.round(local_normal * 127).astype(int) & 255
    rows[:, 9] = 255
    assert np.abs(rows[:, :3]).max() < 32767
    return dict(
        name=P + name,
        limb=limb,
        texture=texture,
        vertices=rows,
        joints=joints.astype(int),
        tri=triangles.astype(int),
    )


def skirt_part(parts, world):
    # Lower edge of the unchanged sash supplies the exact waist contour.
    rows, owners = [], []
    for part in parts:
        if not part["texture"].endswith("tex8"):
            continue
        mask = (part["vertices"][:, 4] >= 715) & (part["vertices"][:, 5] == 56)
        rows.extend(part["vertices"][mask])
        owners.extend(part["joints"][mask])
    rows, owners = np.asarray(rows), np.asarray(owners)
    waist = posed(rows, owners, world)
    waist = np.unique(np.round(waist, 2), axis=0)
    center = waist.mean(axis=0)
    angles = np.arctan2(waist[:, 0] - center[0], waist[:, 2] - center[2])
    order = np.argsort(angles)
    angles, waist = angles[order], waist[order]
    count, rings = 48, 33
    theta = np.linspace(-np.pi, np.pi, count + 1)
    wrapped = np.r_[angles - 2 * np.pi, angles, angles + 2 * np.pi]
    contour = np.vstack([waist, waist, waist])
    top = np.column_stack([np.interp(theta, wrapped, contour[:, k]) for k in range(3)])
    # Centerline follows the seated body: waist, hips, lap, knee and hem.
    # Radii are close to the legs and taper gradually into the existing waist.
    t = np.array([0, 0.16, 0.32, 0.51, 0.66, 0.78, 1.0])
    centers = np.array(
        [
            [center[0], center[1], center[2]],
            [38, 2500, -1370],
            [25, 2350, -1090],
            [10, 2330, -600],
            [0, 2130, -100],
            [0, 1770, -15],
            [0, 1050, -5],
        ]
    )
    widths = np.array([390, 485, 510, 525, 540, 555, 570])
    depths = np.array([295, 340, 360, 365, 355, 360, 360])
    curve = PchipInterpolator(t, centers, axis=0)
    width, depth = PchipInterpolator(t, widths), PchipInterpolator(t, depths)
    positions, joints, uv, triangles = [], [], [], []
    for i, u in enumerate(np.linspace(0, 1, rings)):
        c = curve(u)
        tangent = curve.derivative()(u)
        normal = np.array([0.0, tangent[2], -tangent[1]])
        normal /= np.linalg.norm(normal)
        ring = c + np.sin(theta)[:, None] * np.array([float(width(u)), 0, 0])
        ring += np.cos(theta)[:, None] * normal * float(depth(u))
        if i == 0:
            ring = top.copy()
        # A small inward tuck joins beneath the sash; its original mesh stays.
        if i == 1:
            ring = 0.65 * top + 0.35 * ring
        # Entire shared rings use one native joint: no gaps between panels.
        owner = 9 if u < 0.24 else 17 if u < 0.62 else 18
        positions.extend(ring)
        joints.extend([owner] * (count + 1))
        uv.extend(
            np.column_stack(
                [np.linspace(3, 707, count + 1), np.full(count + 1, 3 + 548 * u)]
            )
        )
        if i:
            a = (i - 1) * (count + 1)
            b = i * (count + 1)
            for j in range(count):
                triangles.extend(
                    [(a + j, b + j, a + j + 1), (a + j + 1, b + j, b + j + 1)]
                )
    return mesh_part(
        "SeatedFittedSkirtR9DL", 9, P + "tex8", positions, joints, uv, triangles, world
    )


def boundary_loops(parts, world):
    rows, joints, faces = [], [], []
    for part in parts:
        faces.extend(part["tri"] + len(rows))
        rows.extend(part["vertices"])
        joints.extend(part["joints"])
    rows, joints, faces = map(np.asarray, (rows, joints, faces))
    _, first, ids = np.unique(
        np.c_[joints, rows[:, :3]], axis=0, return_index=True, return_inverse=True
    )
    faces = ids[faces]
    edges = np.sort(
        np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]]), axis=1
    )
    edges, counts = np.unique(edges, axis=0, return_counts=True)
    assert not np.any(counts > 2)
    adjacent = {}
    for a, b in edges[counts == 1]:
        adjacent.setdefault(a, []).append(b)
        adjacent.setdefault(b, []).append(a)
    assert all(len(n) == 2 for n in adjacent.values())
    seen, loops = set(), []
    for start in adjacent:
        if start in seen:
            continue
        loop = []
        previous = -1
        current = start
        while current not in seen:
            seen.add(current)
            loop.append(current)
            nxt = next(n for n in adjacent[current] if n != previous)
            previous, current = current, nxt
        index = first[loop]
        loops.append(
            (rows[index], joints[index], posed(rows[index], joints[index], world))
        )
    assert len(loops) == 2
    return sorted(loops, key=lambda item: len(item[0]))


def forearm_part(parts, world):
    # Retain the original connected mesh and its UVs. In particular, the wrist
    # boundary is staggered between two bones and must not be angle-sorted.
    arm = [p for p in parts if p["name"].endswith(("batch7DL", "batch8DL"))]
    rows, joints, triangles = [], [], []
    for part in arm:
        triangles.extend(part["tri"] + len(rows))
        rows.extend(part["vertices"])
        joints.extend(part["joints"])
    rows, joints, triangles = map(np.asarray, (rows, joints, triangles))
    positions = posed(rows, joints, world)
    n = rows[:, 6:9].astype(float)
    n[n > 127] -= 256
    source_normals = np.einsum("nij,nj->ni", world[joints, :3, :3], n / 127)
    _, ids = np.unique(np.c_[joints, rows[:, :3]], axis=0, return_inverse=True)
    faces = ids[triangles]
    edges = np.sort(
        np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]]), axis=1
    )
    edges, counts = np.unique(edges, axis=0, return_counts=True)
    boundary = {tuple(edge) for edge in edges[counts == 1]}
    # Split interior edges once for a rounder silhouette. Sleeve and staggered
    # hand boundaries keep every original edge and record.
    pos = list(positions)
    owner = list(joints)
    tex = list(rows[:, 4:6])
    norms = list(source_normals)
    midpoints = {}
    refined = []

    def midpoint(a, b):
        if tuple(sorted((ids[a], ids[b]))) in boundary:
            return None
        key = tuple(sorted((a, b)))
        if key not in midpoints:
            midpoints[key] = len(pos)
            pos.append((positions[a] + positions[b]) / 2)
            owner.append(joints[a] if joints[a] == joints[b] else 3)
            tex.append((rows[a, 4:6] + rows[b, 4:6]) / 2)
            norms.append((source_normals[a] + source_normals[b]) / 2)
        return midpoints[key]

    for a, b, c in triangles:
        ab, bc, ca = midpoint(a, b), midpoint(b, c), midpoint(c, a)
        if ab is not None and bc is not None and ca is not None:
            refined.extend([(a, ab, ca), (ab, b, bc), (ca, bc, c), (ab, bc, ca)])
        elif ab is None and bc is not None and ca is not None:
            refined.extend([(a, b, bc), (a, bc, ca), (ca, bc, c)])
        elif bc is None and ca is not None and ab is not None:
            refined.extend([(b, c, ca), (b, ca, ab), (ab, ca, a)])
        elif ca is None and ab is not None and bc is not None:
            refined.extend([(c, a, ab), (c, ab, bc), (bc, ab, b)])
        elif ab is not None:
            refined.extend([(a, ab, c), (ab, b, c)])
        elif bc is not None:
            refined.extend([(b, bc, a), (bc, c, a)])
        elif ca is not None:
            refined.extend([(c, ca, b), (ca, a, b)])
        else:
            refined.append((a, b, c))
    positions, joints, uv, source_normals, triangles = map(
        np.asarray, (pos, owner, tex, norms, refined)
    )
    _, first, ids = np.unique(
        np.c_[joints, np.round(positions, 5)],
        axis=0,
        return_index=True,
        return_inverse=True,
    )
    points = positions[first].copy()
    faces = ids[triangles]
    edges = np.sort(
        np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]]), axis=1
    )
    edges, counts = np.unique(edges, axis=0, return_counts=True)
    fixed = np.unique(edges[counts == 1])
    neighbors = [set() for _ in points]
    for a, b in edges:
        neighbors[a].add(b)
        neighbors[b].add(a)
    elbow = world[3, :3, 3]
    wrist = world[4, :3, 3]
    axis = wrist - elbow
    length = np.linalg.norm(axis)
    axis /= length
    t = (points - elbow) @ axis
    influence = np.clip((0.70 * length - t) / (0.4 * length), 0, 1)
    original = points.copy()
    for _ in range(12):
        for step in (0.4, -0.42):
            mean = np.array([points[list(ns)].mean(0) for ns in neighbors])
            points += step * (mean - points) * influence[:, None]
            points[fixed] = original[fixed]
    center = elbow + ((points - elbow) @ axis)[:, None] * axis
    points = center + (points - center) * (1 - 0.05 * influence[:, None])
    points[:, 1] += 25 * np.exp(-np.sum((points - elbow) ** 2, axis=1) / (170**2))
    points[fixed] = original[fixed]
    result = mesh_part(
        "SeatedForearmR9DL", 3, P + "tex2", points[ids], joints, uv, triangles, world
    )
    # Blend into the untouched wrist shading as well as its exact geometry.
    newn = result["vertices"][:, 6:9].astype(float)
    newn[newn > 127] -= 256
    oldn = np.einsum(
        "nij,nj->ni", world[joints, :3, :3].transpose(0, 2, 1), source_normals
    )
    mix = influence[ids, None]
    n = mix * newn / 127 + (1 - mix) * oldn
    n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-9)
    result["vertices"][:, 6:9] = np.round(n * 127).astype(int) & 255
    original_endpoints = np.isin(ids[: len(rows)], fixed)
    result["vertices"][: len(rows)][original_endpoints, :6] = rows[
        original_endpoints, :6
    ]
    # The original wrist normals provide continuity with the unchanged hand.
    wrist_end = original_endpoints & (influence[ids[: len(rows)]] < 0.01)
    result["vertices"][: len(rows)][wrist_end, 6:9] = rows[wrist_end, 6:9]

    return result


def build(native, source, destination):
    model = SourceModel(native, source)
    seated = preview.Anju(native, False, source)
    world = seated.world(seated.clips[seated.animation][0])
    with zipfile.ZipFile(source) as z:
        files = {n: z.read(n) for n in z.namelist()}
    original = dict(files)
    # Preserve every lower-body standing root before changing any seated root.
    for limb, name in SKIRT.items():
        root = P + model.model.dlists[limb - 1].split("/")[-1]
        files[P + name] = files[root]
    changed_limbs = {2, 3, 9, 10, 11, 13, 14, 16, 17, 19}
    parts = []
    for source_part in model.parts:
        part = copy.deepcopy(source_part)
        if part["name"].endswith(("batch7DL", "batch8DL")):
            continue
        if (
            part["limb"] >= 9
            and part["texture"].endswith("tex8")
            and not part["name"].endswith(("batch20DL", "batch36DL"))
        ):
            belt = (part["vertices"][:, 4] >= 715) & (part["vertices"][:, 5] <= 60)
            part["tri"] = part["tri"][belt[part["tri"]].all(axis=1)]
            if not len(part["tri"]):
                continue
        parts.append(part)
    parts += [skirt_part(model.parts, world), forearm_part(model.parts, world)]
    for limb in sorted(changed_limbs):
        root = P + model.model.dlists[limb].split("/")[-1]
        words = hashed(0x33, root, 0xBEEFBEEF)
        for part in parts:
            if part["limb"] != limb or not len(part["tri"]):
                continue
            name = part["name"].split("/")[-1].removesuffix("DL") + "SeatedR9DL"
            words += hashed(0x31, export_mesh(part, name, files))
        files[root] = dl_bytes(words + [(0xDF000000, 0)])
    meta = json.loads(files["Anju_HD_README.json"])
    meta.update(
        revision="R9",
        geometry_revision="R9 seated / R8 standing",
        actor_params={"seated_crying": "0x7E0D", "standing_umbrella": "0x7E1D"},
    )
    palette = meta.get("palette", "Auburn")
    meta["name"] = f"Anju HD {palette} Umbrella R9"
    meta["palette"] = palette
    meta["requires_adapter"] = "Anju HD v1 pose selector, fork commit a89adb09 or newer"
    meta["animations"] = {"seated_crying": 43, "standing_umbrella": 32}
    meta.pop("animation", None)
    meta["source_revision"] = "Recovered R8; seated repair rebuilt as R9"
    meta["notes"] = [
        "Continuous seated skirt with a fitted waist, smooth center and room below the knees.",
        "Seated elbow rounded on the connected source mesh; original sleeve and wrist boundaries retained.",
        "Standing R8 geometry, UVs, normals, hand, grip and umbrella preserved through separate roots.",
        "Original textures, both hands, face, eyes, blinking, shoes and umbrella retained.",
        "No scene, path, collision, skeleton, animation or placement overrides.",
        "Choose one palette archive at a time; both use the same private namespace.",
        "Requires the standing-pose fork update and alternate assets enabled.",
        "Offline archive readback is not an in-game visual test.",
    ]
    for key in list(meta):
        if key.startswith("standing_preview_"):
            meta[key.replace("standing_preview_", "standing_")] = meta.pop(key)
    meta["standing_skirt_roots"] = {
        str(limb - 1): P + name for limb, name in SKIRT.items()
    }
    files["Anju_HD_README.json"] = (json.dumps(meta, indent=2) + "\n").encode()
    # All existing textures, hands, umbrella and standing arm payloads stay exact.
    allowed = {P + model.model.dlists[i].split("/")[-1] for i in changed_limbs} | {
        "Anju_HD_README.json"
    }
    assert all(files[n] == b for n, b in original.items() if n not in allowed)
    destination.parent.mkdir(parents=True, exist_ok=True)
    payload = io.BytesIO()
    with zipfile.ZipFile(payload, "w", zipfile.ZIP_DEFLATED) as z:
        for name, data in files.items():
            z.writestr(name, data)
    destination.write_bytes(payload.getvalue())
    with zipfile.ZipFile(destination) as z:
        assert z.testzip() is None
    print(destination, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("native", type=Path)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    preview.read_texture = functools.lru_cache(maxsize=32)(preview.read_texture)
    build(args.native, args.source, args.output)


if __name__ == "__main__":
    main()
