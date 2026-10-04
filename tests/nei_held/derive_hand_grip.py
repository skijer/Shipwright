"""Reproduce palm sockets from user-supplied native equipment, without exporting assets.

The hilt section at wrist-local X=0 gives the palm center and confirms the
weapon's +X axis. The optional YoungDin measurement records the compatibility
residual; runtime uses the native child convention for every player model.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import zipfile

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/diagnostics"))
from derive_pedestal_child_grip import read_mesh
from render_anju_preview import crc64, read_vertex_array


def section(points, triangles, first, end):
    crossings = []
    for indices in triangles:
        if min(indices) < first or max(indices) >= end:
            continue
        triangle = points[list(indices)]
        for a, b in zip(triangle, np.roll(triangle, -1, axis=0)):
            if a[0] * b[0] < 0:
                crossings.append(a + (b - a) * (-a[0] / (b[0] - a[0])))
    crossings = np.unique(np.round(crossings, 6), axis=0)
    assert len(crossings) >= 4
    return (crossings.min(axis=0) + crossings.max(axis=0)) / 2


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--young-din", type=Path)
    args = parser.parse_args()
    report = {"native": {}, "runtime_units": "model coordinates before actor scale"}
    with zipfile.ZipFile(args.archive) as archive:
        names = {crc64(name): name for name in archive.namelist()}
        for age, path, first, end, runtime in (
            ("child", "objects/object_link_child/gLinkChildLeftFistAndKokiriSwordNearDL", 19, 50,
             [0, 216.22, 4.5]),
            ("adult", "objects/object_link_boy/gLinkAdultLeftHandHoldingMasterSwordNearDL", 54, 73,
             [0, 328, -77]),
        ):
            points, triangles, vertex_path, sha256 = read_mesh(archive, names, path)
            center = section(points, triangles, first, end)
            assert np.max(np.abs(center - runtime)) < 1
            report["native"][age] = {
                "resource": vertex_path, "sha256": sha256,
                "hilt_plane_x0_center": center.tolist(), "runtime_socket": runtime,
            }
        fists = []
        for path in ("objects/object_link_child/gLinkChildLeftFistNearDL",
                     "objects/object_link_child/gLinkChildRightHandClosedNearDL"):
            points, triangles, _, _ = read_mesh(archive, names, path)
            fists.append(np.unique(points[np.unique(triangles)], axis=0))
        mirrored = fists[0] * [1, 1, -1]
        errors = np.linalg.norm(mirrored[:, None] - fists[1][None], axis=2)
        mirror_error = max(errors.min(axis=0).max(), errors.min(axis=1).max())
        assert mirror_error == 0
        report["child_right_fist_exact_z_mirror"] = True

    if args.young_din:
        grip_path = "objects/object_link_child/DinSleekEquipmentPOC1_OOT_Child/Vertices/Sword_3"
        with zipfile.ZipFile(args.young_din) as archive:
            raw = archive.read(grip_path)
        points = np.unique(np.array(read_vertex_array(raw), dtype=float)[:, :3], axis=0)
        rings = [points[points[:, 0] == x] for x in np.unique(points[:, 0])]
        assert len(rings) == 3 and all(len(ring) == 8 for ring in rings)
        center = np.mean([ring.mean(axis=0) for ring in rings], axis=0)
        center[0] = 0  # any point along the hilt's +X centerline
        residual = float(np.linalg.norm(center - report["native"]["child"]["runtime_socket"]) * .01)
        assert residual < .2
        report["young_din"] = {
            "resource": grip_path, "sha256": hashlib.sha256(raw).hexdigest(),
            "grip_centerline": center.tolist(), "residual_world_at_scale_0_01": residual,
            "limitation": "Mesh compatibility was reviewed separately; these measurements are not gameplay acceptance.",
        }
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
