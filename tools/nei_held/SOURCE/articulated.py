"""Held switch hook and articulated whip, derived from approved GI triangles.

No GI files are modified. Stored positions are 16 times world units so the
shared 1/16 quantizer retains thin trim; each resource matrix cancels that and
the existing gameplay draw scale. The grip/tip partitions keep source UVs,
normals, materials and triangle indices.
"""
import argparse
from copy import deepcopy
from pathlib import Path
import shutil
import sys

import numpy as np

HELD_ROOT = Path(__file__).resolve().parents[1]
GI_SOURCE = HELD_ROOT.parent / "nei_gi" / "SOURCE"
sys.path.insert(0, str(GI_SOURCE))
import meshkit
import models as gi_models
import build_batch as gi_batch
import preview

STORAGE_SCALE = 16.0
WHIP_WORLD_SCALE = 0.30
WHIP_GRIP = np.array([0.0, -26.0, 0.0])
WHIP_HEAD_SOCKET = np.array([24.0, 49.0, -10.0])
HOOK_GRIP = np.array([0.0, -33.0, 0.0])
HOOK_HUB = np.array([-9.0, 25.0, 1.0])
HOOK_DOCK = np.array([1.0, 16.4, 0.0])
# z_player_lib.c: Matrix_RotateZYX(0, -0x4000, -0x4000).
ACTOR_TO_HAND = np.array([[0., 1., 0.], [0., 0., 1.], [1., 0., 0.]])


def model(slug, draw_scale):
    m = meshkit.Model(slug, slug.replace("_", " ").title(),
                      "objects/nei_held_redesign/" + slug + "/gi_dl",
                      draw_scale, 1.0 / STORAGE_SCALE)
    m.prefix = "objects/nei_held_redesign/" + slug + "/"
    return m


def copy_parts(slug, source, selected, draw_scale, transform, normals):
    m = model(slug, draw_scale)
    m.materials = deepcopy(source.materials)
    for index in selected:
        p = deepcopy(source.parts[index])
        p["p"] = np.rint(transform(p["p"]) * STORAGE_SCALE * meshkit.Q) / meshkit.Q
        p["n"] = meshkit.unit(normals(p["n"]))
        m.parts.append(p)
    return m


def align(a, b):
    a, b = meshkit.unit(a), meshkit.unit(b)
    v = np.cross(a, b)
    cross = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
    return np.eye(3) + cross + cross @ cross / (1.0 + np.dot(a, b))


def switch_hook():
    source = gi_models.hook()
    unlean = meshkit.rotation("z", -18)
    aim = align(HOOK_HUB - HOOK_GRIP, HOOK_DOCK)
    scale = np.linalg.norm(HOOK_DOCK) / np.linalg.norm(HOOK_HUB - HOOK_GRIP)

    def hand(p):
        return (p @ unlean.T - HOOK_GRIP) @ aim.T * scale

    def normal(n):
        return n @ unlean.T @ aim.T

    complete = copy_parts("switch_hook", source, range(16), .01, hand, normal)
    body = copy_parts("switch_hook_body", source, range(10), .01, hand, normal)
    tip = copy_parts("switch_hook_tip", source, range(10, 16), .01,
                     lambda p: (hand(p) - HOOK_DOCK) @ ACTOR_TO_HAND,
                     lambda n: normal(n) @ ACTOR_TO_HAND)
    for m in (complete, body):
        m.markers.update(grip_world=[0, 0, 0], dock_world=HOOK_DOCK.tolist(),
                         chain_socket_world=[1, 15, 0])
        m.notes = ["Approved GI design, rigidly fitted between the native grip and hook-actor socket.",
                   "Complete mesh only while docked; grip/neck stay in hand during flight."]
    tip.markers["actor_origin_world"] = [0, 0, 0]
    tip.notes = ["Approved hub, bezel and crescent jaws; actor +Z is forward.",
                 "Original hook actor, collision probes, chain endpoints and aim path are unchanged."]
    return (complete, body, tip)


def whip():
    source = gi_batch.whip()
    hand = lambda p: (p - WHIP_GRIP) * WHIP_WORLD_SCALE
    normal = lambda n: n
    complete = copy_parts("whip", source, range(11), 1., hand, normal)
    handle = copy_parts("whip_handle", source, range(3), 1., hand, normal)
    rotate = meshkit.rotation("y", 90)  # Approved head points -X; gameplay expects +Z.
    tip = copy_parts("whip_tip", source, range(4, 11), .022,
                     lambda p: (p - WHIP_HEAD_SOCKET) @ rotate.T * WHIP_WORLD_SCALE,
                     lambda n: n @ rotate.T)
    for m in (complete, handle):
        m.markers.update(grip_world=[0, 0, 0], rope_socket_world=[0, 7.2, 0])
        m.notes = ["Approved Whip A grip and ferrules; grip midpoint is the hand origin.",
                   "Whole coil is equipped-only; all source parts retain one consistent 0.30 world scale."]
    tip.markers["rope_socket_world"] = [0, 0, 0]
    tip.notes = ["Approved snake head, eyes and details, rebased at the coil's terminal socket.",
                 "Head faces +Z and remains the same size as the equipped coil's head."]

    segment = model("whip_segment", .015)
    segment.materials = deepcopy(source.materials)
    segment.tube("Straight braided whip segment", "orange_braid",
                 [[0, 0, -7.5 * STORAGE_SCALE], [0, 0, 7.5 * STORAGE_SCALE]],
                 1.2 * STORAGE_SCALE, sides=10, pitch=25 * WHIP_WORLD_SCALE * STORAGE_SCALE,
                 cap=False)
    segment.markers.update(start_world=[0, 0, -7.5], end_world=[0, 0, 7.5])
    segment.notes = ["Approved orange-braid material on a straight segment; runtime follows the existing whip path.",
                     "15-world-unit length, 1.2-unit radius; no coiled geometry is used during attacks."]
    return (complete, handle, tip, segment)


def build():
    return {m.slug: m for m in (*switch_hook(), *whip())}


def export_models(built, output_root=HELD_ROOT, checkpoints=True, install=False):
    output_root = Path(output_root)
    original_mesh_root, original_preview_root = meshkit.ROOT, preview.ROOT
    try:
        meshkit.ROOT = preview.ROOT = output_root
        for m in built.values():
            stats = meshkit.export_resources(m)
            if checkpoints:
                preview.checkpoint(m, stats)
            if install:
                src = output_root / "RESOURCES" / m.prefix
                destination = HELD_ROOT.parents[1] / "soh/assets/custom" / m.prefix
                shutil.copytree(src, destination, dirs_exist_ok=True)
    finally:
        meshkit.ROOT, preview.ROOT = original_mesh_root, original_preview_root


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--install", action="store_true", help="copy generated resources into the game assets")
    args = parser.parse_args()
    export_models(build(), install=args.install)
