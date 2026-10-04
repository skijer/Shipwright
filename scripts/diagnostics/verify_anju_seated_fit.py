"""Read exported R9 resources, check preservation and both native poses."""

from pathlib import Path
import argparse
import functools
import hashlib
import json
import zipfile

import numpy as np

import render_anju_preview as preview
from build_anju_hd import SourceModel
from fit_anju_seated import P, boundary_loops, posed
from verify_anju_pose_selection import SKIRT, check_selector, select_standing


def topology(part):
    rows, joints = part["vertices"], part["joints"]
    _, ids = np.unique(np.c_[joints, rows[:, :3]], axis=0, return_inverse=True)
    triangles = ids[part["tri"]]
    edges = np.sort(
        np.concatenate(
            [triangles[:, [0, 1]], triangles[:, [1, 2]], triangles[:, [2, 0]]]
        ),
        axis=1,
    )
    edges, counts = np.unique(edges, axis=0, return_counts=True)
    assert counts.max() == 2, "nonmanifold edge"
    assert len(np.unique(triangles)) - len(edges) + len(triangles) == 0
    neighbors = {}
    for a, b in edges[counts == 1]:
        neighbors.setdefault(a, []).append(b)
        neighbors.setdefault(b, []).append(a)
    assert all(len(n) == 2 for n in neighbors.values())
    seen, loops = set(), 0
    for start in neighbors:
        if start in seen:
            continue
        loops += 1
        pending = [start]
        while pending:
            node = pending.pop()
            if node not in seen:
                seen.add(node)
                pending.extend(neighbors[node])
    assert loops == 2, "expected only top and bottom boundary loops"
    return dict(
        vertices=len(np.unique(triangles)),
        triangles=len(triangles),
        boundary_loops=loops,
    )


def check(native, source, output):
    with zipfile.ZipFile(source) as z:
        old = {name: z.read(name) for name in z.namelist()}
    with zipfile.ZipFile(output) as z:
        assert z.testzip() is None
        new = {name: z.read(name) for name in z.namelist()}
    baseline = SourceModel(native, source)
    changed = {
        P + baseline.model.dlists[i].split("/")[-1]
        for i in (2, 3, 9, 10, 11, 13, 14, 16, 17, 19)
    }
    assert all(
        new[name] == data
        for name, data in old.items()
        if name not in changed | {"Anju_HD_README.json"}
    )
    for limb, name in SKIRT.items():
        assert new[P + name] == old[P + baseline.model.dlists[limb - 1].split("/")[-1]]
    check_selector(new.keys())
    # Exact old payloads and copied roots prove preservation for every frame.
    # Independently decode three frames to catch selector/graph mistakes.
    before, after = [preview.Anju(native, True, path) for path in (source, output)]
    select_standing(before)
    select_standing(after)
    for frame in (0, 16, 31):
        a, b = before.pose(before.animation, frame), after.pose(after.animation, frame)
        assert len(a) == len(b)
        for x, y in zip(a, b):
            assert x["path"] == y["path"]
            for field in ("pos", "normal", "uv", "tri"):
                assert np.array_equal(x[field], y[field]), (frame, x["path"], field)
    print(
        "PASS exact R8 standing payloads and sampled posed geometry",
        output.name,
        flush=True,
    )
    exported = SourceModel(native, output)
    fitted = [
        part
        for part in exported.parts
        if "SeatedFittedSkirt" in part["name"] or "SeatedForearm" in part["name"]
    ]
    assert len(fitted) == 2
    metrics = {p["name"]: topology(p) for p in fitted}
    model = preview.Anju(native, False, output)
    world = model.world(model.clips[model.animation][0])
    old_arm = [
        p for p in baseline.parts if p["name"].endswith(("batch7DL", "batch8DL"))
    ]
    new_arm = [p for p in fitted if "Forearm" in p["name"]]
    original_boundaries = boundary_loops(old_arm, world)
    new_boundaries = boundary_loops(new_arm, world)
    for (rows_a, owners_a, _), (rows_b, owners_b, _) in zip(
        original_boundaries, new_boundaries
    ):
        a = {tuple([owner, *row[:6]]) for row, owner in zip(rows_a, owners_a)}
        b = {tuple([owner, *row[:6]]) for row, owner in zip(rows_b, owners_b)}
        assert a == b, "sleeve/wrist boundary moved or UV changed"
    minimum_area = float("inf")
    maximum_edge = 0.0
    for frame in range(43):
        parts = model.pose(model.animation, frame)
        for part in parts:
            assert np.isfinite(part["pos"]).all() and np.isfinite(part["normal"]).all()
        # The rasterizer groups by texture, not mesh path. Measure the exact
        # exported vertex records separately so this gate cannot skip a mesh.
        world = model.world(model.clips[model.animation][frame])
        for part in fitted:
            p = posed(part["vertices"], part["joints"], world)[part["tri"]]
            area = (
                np.linalg.norm(np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]), axis=1)
                / 2
            )
            minimum_area = min(minimum_area, float(area.min()))
            maximum_edge = max(
                maximum_edge, float(np.linalg.norm(p[:, 1] - p[:, 0], axis=1).max())
            )
            assert area.min() > 0.1, (frame, part["name"], "collapsed triangle")
        if frame % 10 == 0:
            print("Decoded seated frame", frame, output.name, flush=True)
    assert np.isfinite(minimum_area) and maximum_edge > 0
    print(
        "PASS 43 seated frames, manifold skirt/arm, exact sleeve and wrist boundaries",
        flush=True,
    )
    return dict(
        file=output.name,
        sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
        size_bytes=output.stat().st_size,
        resources=len(new),
        seated_frames=43,
        standing_sample_frames=[0, 16, 31],
        standing_payloads_exact=True,
        sleeve_wrist_boundaries_exact=True,
        minimum_triangle_area=minimum_area,
        maximum_triangle_edge=maximum_edge,
        topology=metrics,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("native", type=Path)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("report", type=Path)
    args = parser.parse_args()
    preview.read_texture = functools.lru_cache(maxsize=32)(preview.read_texture)
    report = check(args.native, args.source, args.output)
    args.report.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n")


if __name__ == "__main__":
    main()
