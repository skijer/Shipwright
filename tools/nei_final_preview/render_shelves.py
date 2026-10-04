"""Render source GI meshes with shop matrices captured by the production test.

The counter is a scale reference, not a captured or reconstructed Market scene.
Model rotation is animated separately because the fixture normalizes it to zero.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys

import numpy as np
from PIL import ImageDraw

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "nei_gi/runtime_preview"))
import render as r

NAMES = {
    "hylia_grace": "Hylia’s Grace",
    "zonai_permafrost": "Zonai Permafrost",
    "demise_destruction": "Demise Destruction",
    "time_gate": "Time Gate",
    "switch_hook": "Switch Hook",
    "rocs_feather": "Roc’s Feather · both routes",
    "spinner": "Spinner",
    "cane_of_somaria": "Cane of Somaria",
    "minish_cap": "Minish Cap",
    "rocs_cape": "Roc’s Cape",
    "fire_rod": "Fire Rod · retained fit",
    "ball_and_chain": "Ball and Chain · retained fit",
}
ORDER = list(NAMES)
NEW_ITEMS = ("spinner", "cane_of_somaria", "minish_cap", "rocs_cape")


def video(path):
    return subprocess.Popen([
        "ffmpeg", "-loglevel", "error", "-y", "-f", "rawvideo", "-vcodec", "rawvideo",
        "-pix_fmt", "rgb24", "-s", f"{r.W}x{r.H}", "-r", "20", "-i", "-", "-an",
        "-c:v", "libx264", "-preset", "fast", "-crf", "18", "-pix_fmt", "yuv420p",
        "-movflags", "+faststart", str(path),
    ], stdin=subprocess.PIPE)


def counter():
    r.Disable(0x0DE1)
    r.Disable(0x0B44)
    r.Begin(0x0007)
    for color, points in [
        ((.28, .19, .115), [(-16, 0, -10), (-16, 0, 10), (16, 0, 10), (16, 0, -10)]),
        ((.16, .105, .066), [(-16, 0, 10), (-16, -4, 10), (16, -4, 10), (16, 0, 10)]),
    ]:
        r.Color(*color, 1)
        for p in points:
            r.Vertex(*p)
    r.End()
    r.Color(.58, .40, .22, 1)
    r.Begin(0x0001)
    for x in (-12, -6, 0, 6, 12):
        r.Vertex(x, .02, -10)
        r.Vertex(x, .02, 10)
    r.End()


def draw_model(passes):
    r.DepthMask(1)
    r.CallList(passes[0])
    r.DepthMask(0)
    r.CallList(passes[1])


def shelf_transform(item):
    matrix = np.asarray(item["shop_matrix"], dtype=float)
    assert matrix.shape == (4, 4) and np.isfinite(matrix).all()
    linear = matrix[:3, :3]
    assert np.allclose(linear, np.eye(3) * linear[0, 0]), "Fixture must normalize spin"
    # r.model already folds the caller scale into its resource-matrix/GLB data.
    scale = linear[0, 0] / item["draw_scale"]
    return matrix[:3, 3], scale


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("matrices", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--frames", type=int, default=160)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    data = json.loads(args.matrices.read_text())
    by_slug = {item["slug"]: item for item in data["items"]}
    items = [by_slug[s] for s in ORDER if s in by_slug]
    assert all(s in by_slug for s in NEW_ITEMS), "All four new GIs need shelf evidence"
    loaded = {s: r.model(s) for s in by_slug if s in ORDER}
    shelf_path = args.output / "NEI_Final_POC1_shop_fitting.mp4"
    proc = video(shelf_path)
    for frame in range(args.frames):
        r.DepthMask(1)
        r.Clear(0x4000 | 0x0100)
        for i, item in enumerate(items):
            row, col = divmod(i, 3)
            # All panels use the same world-unit camera and counter dimensions.
            r.pose(col * 400, 70 + (3 - row) * 220, 400, 180, -6, 32)
            counter()
            r.Push()
            translation, scale = shelf_transform(item)
            r.Translate(*translation)
            r.Scale(scale, scale, scale)
            r.Rotate(frame * 360 / args.frames, 0, 1, 0)
            draw_model(loaded[item["slug"]])
            r.Pop()
        im = r.pixels()
        d = ImageDraw.Draw(im)
        d.rectangle((0, 0, r.W, 72), fill="#111923")
        d.text((24, 13), "Shop fitting · final pass POC1", font=r.font(27), fill="#f0f5ff")
        d.text((24, 48), "Actual GI meshes + captured shop matrices · one scale throughout · schematic counters",
               font=r.font(15), fill="#aabacc")
        for i, item in enumerate(items):
            row, col = divmod(i, 3)
            d.text((col * 400 + 18, 78 + row * 220), NAMES[item["slug"]],
                   font=r.font(20), fill="#eef3fd")
        d.text((24, 965), "Geometry review: effects omitted for clearance · offline lighting · in-game acceptance pending",
               font=r.font(15), fill="#aabacc")
        if frame == 0:
            im.save(args.output / "NEI_Final_POC1_shop_fitting.png")
        proc.stdin.write(im.tobytes())
    proc.stdin.close()
    assert proc.wait() == 0
    print(shelf_path, flush=True)

    new_path = args.output / "NEI_Final_POC1_new_GI_models.mp4"
    proc = video(new_path)
    bounds = {}
    for slug in NEW_ITEMS:
        meta = json.loads((r.ROOT / "CHECKPOINTS" / slug / "checkpoint.json").read_text())
        bounds[slug] = np.asarray(meta["bounds_author"]) * 16 * meta["matrix_scale"] * meta["draw_scale"]
    for frame in range(args.frames):
        r.DepthMask(1)
        r.Clear(0x4000 | 0x0100)
        for i, slug in enumerate(NEW_ITEMS):
            low, high = bounds[slug]
            center = (low + high) / 2
            radius = np.linalg.norm(high - low) * .58
            r.pose((i % 2) * 600, 70 + (1 - i // 2) * 440, 600, 375, -radius, radius)
            r.Translate(*(-center))
            r.Rotate(frame * 360 / args.frames, 0, 1, 0)
            draw_model(loaded[slug])
        im = r.pixels()
        d = ImageDraw.Draw(im)
        d.rectangle((0, 0, r.W, 72), fill="#111923")
        d.text((24, 13), "Four final additions · GI model review", font=r.font(27), fill="#f0f5ff")
        d.text((24, 48), "Exact exported meshes · views fitted individually for detail · supplied icons as references",
               font=r.font(15), fill="#aabacc")
        for i, slug in enumerate(NEW_ITEMS):
            d.text(((i % 2) * 600 + 24, 85 + (i // 2) * 440), NAMES[slug],
                   font=r.font(25), fill="#eef3fd")
        d.text((24, 965), "Model turntables · shelf proportions and used-item animation are separate previews",
               font=r.font(16), fill="#aabacc")
        if frame == 0:
            im.save(args.output / "NEI_Final_POC1_new_GI_models.png")
        proc.stdin.write(im.tobytes())
    proc.stdin.close()
    assert proc.wait() == 0
    print(new_path, flush=True)


if __name__ == "__main__":
    main()
