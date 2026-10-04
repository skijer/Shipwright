"""Reproduce/verify the child ceremonial sword basis from a supplied native OOT o2r.

Reads the archive without modifying it. Uses the existing native resource reader
(NumPy, SciPy and Pillow). Emits measurements only; no game assets are exported.
"""
import argparse
import hashlib
import json
import pathlib
import re
import zipfile

import numpy as np

from render_anju_preview import commands, crc64, read_vertex_array

ROOT = pathlib.Path(__file__).resolve().parents[2]
ADULT = "objects/object_link_boy/gLinkAdultLeftHandHoldingMasterSwordNearDL"
CHILD = "objects/object_link_child/gLinkChildLeftHandHoldingMasterSwordDL"
FIST = "objects/object_link_child/gLinkChildLeftFistNearDL"


def read_mesh(archive, names, path):
    vertex_paths = {names[hashed] for op, _, _, hashed in commands(archive.read(path)) if op == 0x32}
    assert len(vertex_paths) == 1, (path, vertex_paths)
    vertex_path = vertex_paths.pop()
    raw = archive.read(vertex_path)
    vertices = np.array(read_vertex_array(raw), dtype=float)[:, :3]
    cache, triangles = {}, []
    for op, w0, w1, hashed in commands(archive.read(path)):
        if op == 0x32:
            count = (w0 >> 12) & 0xFF
            end = (w0 >> 1) & 0x7F
            assert names[hashed] == vertex_path and w1 % 16 == 0
            cache.update({end - count + i: w1 // 16 + i for i in range(count)})
        elif op in (0x05, 0x06):
            for word in (w0, w1) if op == 0x06 else (w0,):
                triangles.append(tuple(cache[((word >> shift) & 0xFF) // 2] for shift in (16, 8, 0)))
    return vertices, triangles, vertex_path, hashlib.sha256(raw).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=pathlib.Path, required=True)
    args = parser.parse_args()
    with zipfile.ZipFile(args.archive) as archive:
        names = {crc64(name): name for name in archive.namelist()}
        adult, adult_tri, adult_path, adult_sha = read_mesh(archive, names, ADULT)
        child, child_tri, child_path, child_sha = read_mesh(archive, names, CHILD)
        fist, _, _, _ = read_mesh(archive, names, FIST)
    assert adult.shape == (186, 3) and child.shape == (163, 3)
    adult_tri = [triangle for triangle in adult_tri if max(triangle) < 136]
    child_tri = [tuple(index - 27 for index in triangle) for triangle in child_tri if min(triangle) >= 27]
    assert adult_tri == child_tri, "Native sword topology does not match; do not fit unrelated models"
    ordinary = adult[:136]
    ceremonial = child[27:]
    a = ordinary - ordinary.mean(axis=0)
    b = ceremonial - ceremonial.mean(axis=0)
    u, _, vh = np.linalg.svd(a.T @ b)
    rotation = u @ vh
    assert abs(np.linalg.det(rotation) - 1) < 1e-10
    translation = ceremonial.mean(axis=0) - ordinary.mean(axis=0) @ rotation
    fitted = ordinary @ rotation + translation

    source = (ROOT / "soh/soh/Enhancements/customequipment.cpp").read_text()
    initializer = re.search(r"MtxF swordBasis = (.*?);", source, re.S)[1]
    matrix = np.array([float(value) for value in re.findall(r"(-?\d+\.\d+)f", initializer)]).reshape(4, 4)
    actual = np.c_[ordinary, np.ones(len(ordinary))] @ matrix[:, :3]
    max_error = float(np.abs(actual - ceremonial).max())
    assert max_error < 1.1, max_error
    old = ordinary * [-1, -1, 1]
    # Hilt axis center in the ordinary adult mesh, mapped into child hand space.
    # This lies inside both fist and ceremonial-hand bounds, while the old
    # half-turn displaces the entire ordinary fist to negative local Y.
    grip = np.array([0.0, 328.0, -77.0, 1.0]) @ matrix[:, :3]
    bounds = lambda points: [points.min(axis=0).tolist(), points.max(axis=0).tolist()]
    assert np.all(grip >= fist.min(axis=0)) and np.all(grip <= fist.max(axis=0))
    assert np.all(grip >= child[:27].min(axis=0)) and np.all(grip <= child[:27].max(axis=0))
    print(json.dumps({
        "native_resources": {adult_path: adult_sha, child_path: child_sha},
        "matched_sword_vertices": 136,
        "matched_sword_triangles": len(adult_tri),
        "fitted_matrix_rows": np.c_[np.vstack([rotation, translation]), [0, 0, 0, 1]].tolist(),
        "fit_rms_model_units": float(np.sqrt(np.mean((fitted - ceremonial) ** 2))),
        "production_max_component_error_model_units": max_error,
        "old_half_turn_max_component_error_model_units": float(np.abs(old - ceremonial).max()),
        "ordinary_child_fist_bounds": bounds(fist),
        "native_ceremonial_child_hand_bounds": bounds(child[:27]),
        "ordinary_fist_after_old_limb_half_turn_bounds": bounds(fist * [-1, -1, 1]),
        "mapped_hilt_center_in_child_hand": grip.tolist(),
        "limitation": "Bounds confirm gross grip placement, not finger contact or artist-pack/runtime appearance."
    }, indent=2))


if __name__ == "__main__":
    main()
