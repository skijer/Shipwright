"""Narrow source-mesh surface-intersection check for the raised Somaria cast.

This does not test collision volumes or runtime IK. It checks actual body/item
triangles from already exported source matrices; fist contact is excluded.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from scipy.spatial import cKDTree

from player_animation import ROOT, PlayerArchive, glb_meshes, transformed, sha256


def segment_triangle(origins, ends, triangles):
    direction = ends-origins
    edge1 = triangles[:, 1]-triangles[:, 0]
    edge2 = triangles[:, 2]-triangles[:, 0]
    p = np.cross(direction, edge2)
    det = np.einsum("ij,ij->i", edge1, p)
    valid = np.abs(det) > 1e-10
    inv = np.divide(1., det, out=np.zeros_like(det), where=valid)
    distance = origins-triangles[:, 0]
    u = np.einsum("ij,ij->i", distance, p)*inv
    q = np.cross(distance, edge1)
    v = np.einsum("ij,ij->i", direction, q)*inv
    t = np.einsum("ij,ij->i", edge2, q)*inv
    hit = valid & (u >= -1e-8) & (v >= -1e-8) & (u+v <= 1+1e-8) & (t >= -1e-8) & (t <= 1+1e-8)
    return hit, origins+t[:, None]*direction


def intersections(a, b):
    centers = b.mean(1); radii = np.linalg.norm(b-centers[:, None], axis=2).max(1)
    tree = cKDTree(centers)
    amin, amax, bmin, bmax = a.min(1), a.max(1), b.min(1), b.max(1)
    output = []
    for i, triangle in enumerate(a):
        center = triangle.mean(0); radius = np.linalg.norm(triangle-center, axis=1).max()
        near = np.array(tree.query_ball_point(center, radius+radii.max()), dtype=int)
        if not len(near): continue
        near = near[np.all(amax[i] >= bmin[near], axis=1) & np.all(bmax[near] >= amin[i], axis=1)]
        if not len(near): continue
        bt = b[near]
        found = np.zeros(len(near), dtype=bool); points = np.zeros((len(near), 3))
        for k in range(3):
            hit, p = segment_triangle(np.tile(triangle[k], (len(near), 1)),
                                      np.tile(triangle[(k+1)%3], (len(near), 1)), bt)
            found |= hit; points[hit] = p[hit]
            hit, p = segment_triangle(bt[:, k], bt[:, (k+1)%3], np.tile(triangle, (len(near), 1, 1)))
            found |= hit; points[hit] = p[hit]
        output.extend((int(i), int(j), point.tolist()) for j, point in zip(near[found], points[found]))
    return output


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--player", type=Path, required=True); p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    archive = PlayerArchive(a.player); body = archive.geometry(hands="closed", include_sheath=False)
    item_path = ROOT/"tools/nei_held/CHECKPOINTS/cane_of_somaria/cane_of_somaria.glb"
    cane = glb_meshes(item_path)
    matrix_path = a.output/"Somaria_Actual_Cast_Slow_Scan_world_matrices.npz"
    matrices = np.load(matrix_path)
    category = np.concatenate([m.bones.reshape(-1, 3) for m in body])
    masks = {"head_and_hair": np.isin(category, [10, 11]).any(1),
             "left_forearm": np.isin(category, [14]).any(1),
             "right_forearm": np.isin(category, [17]).any(1),
             "torso_and_shoulders": np.isin(category, [9, 12, 13, 16, 20]).any(1)}
    report = {"player_sha256": sha256(a.player), "cane_glb_sha256": sha256(item_path),
              "matrix_evidence_sha256": sha256(matrix_path), "source_frames": list(range(12, 49)),
              "method": "Two-sided triangle-edge/surface intersection; coplanar contacts not tested; fist surfaces excluded",
              "limitations": "Surface intersection evidence for exact offline source poses, not runtime collision or enclosed-volume proof",
              "frames": {}}
    for frame in range(12, 49):
        bp = np.concatenate([transformed(m, matrices["world_matrices"][frame])[0] for m in body]).reshape(-1, 3, 3)
        ip = np.concatenate([transformed(m, matrices["item_world_matrices"][frame][None])[0] for m in cane]).reshape(-1, 3, 3)
        per_frame = {}
        for label, mask in masks.items():
            hit = intersections(ip, bp[mask])
            per_frame[label] = {"triangle_pairs": len(hit), "example_contacts_world": [v[2] for v in hit[:8]]}
        report["frames"][str(frame)] = per_frame
        if frame % 6 == 0: print(frame, {k: v["triangle_pairs"] for k, v in per_frame.items()}, flush=True)
    report["summary"] = {label: {"frames_with_contacts": [int(f) for f, v in report["frames"].items() if v[label]["triangle_pairs"]],
                                  "max_triangle_pairs": max(v[label]["triangle_pairs"] for v in report["frames"].values())}
                         for label in masks}
    (a.output/"Somaria_Raised_Cast_Surface_Check.json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(report["summary"], indent=2), flush=True)


if __name__ == "__main__": main()
