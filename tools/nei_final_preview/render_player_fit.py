"""Render exact-input Spinner and Somaria player animation fitting previews.

Example:
  python tools/nei_final_preview/render_player_fit.py --player /path/YoungDin.o2r \
    --native /path/oot.o2r --output /path/previews

The input archives remain external. No native mesh or animation resources are
copied into the repository. This is source-sampled offline evidence, not gameplay.
"""
import argparse
import json
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from player_animation import (ROOT, ANIM_PATH, SOURCE_PATH, PlayerArchive, NativeAnimations,
                              verify_somaria_source, glb_meshes, transformed, translation,
                              scale, rotate, sha256)
from player_render import Renderer

FONT_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
FONT = lambda size: ImageFont.truetype(FONT_PATH, size)
WIDTH, HEIGHT = 1440, 900


def foot_measurements(archive, meshes, clip):
    result = {}
    for name, bone in [("right", 5), ("left", 8)]:
        all_points, sole_points, minimum = [], [], []
        for frame in clip:
            m = archive.world(frame)
            p = np.concatenate([transformed(q, m)[0][q.bones == bone] for q in meshes])
            all_points.append(p); sole_points.append(p[p[:, 1] < p[:, 1].min()+.8]); minimum.append(p[:, 1].min())
        points, sole = np.concatenate(all_points), np.concatenate(sole_points)
        result[name] = {"lowest_y_range_relative_actor": [min(minimum), max(minimum)],
                        "bounds_relative_actor": [points.min(0).tolist(), points.max(0).tolist()],
                        "sole_radial_bounds": [float(np.linalg.norm(sole[:, [0, 2]], axis=1).min()),
                                                float(np.linalg.norm(sole[:, [0, 2]], axis=1).max())]}
    return result


def annotate(image, mode, frame, source_frame, count, slow=False):
    d = ImageDraw.Draw(image)
    d.rectangle((0, 0, WIDTH, 106), fill="#111923")
    title = "Spinner | Native riding animation" if mode == "spinner" else "Cane of Somaria | Cast and grip"
    d.text((28, 17), "NEI  •  " + title, font=FONT(30), fill="#f0f5ff")
    d.text((30, 60), "Young Din POC4 geometry  •  actual animation data + candidate item  •  offline preview", font=FONT(19), fill="#aebed1")
    d.text((30, 118), "Front three-quarter", font=FONT(20), fill="#efc977")
    d.text((750, 118), "Side view", font=FONT(20), fill="#efc977")
    d.rectangle((0, 793, WIDTH, HEIGHT), fill="#111923")
    if mode == "spinner":
        line = f"Native stance frame {source_frame:02d}/34  •  1/32-turn platform step  •  actor height: 17 world units"
    else:
        line = f"Somaria source frame {source_frame:02d}/59  •  right wrist grip  •  " + ("1/3-speed scan" if slow else "source playSpeed 2, update rate 3")
    d.text((30, 808), line, font=FONT(19), fill="#e2eaf5")
    d.text((30, 840), "Fixed studio camera/light; runtime movement, collision, interpolation and effects still need in-game review.", font=FONT(18), fill="#9fb0c6")
    if mode == "cane":
        d.text((30, 870), "Actual upper-body copy mask over native two-handed idle, starting at idle phase 0. No invented pose.", font=FONT(16), fill="#93a6be")
    else:
        d.text((30, 870), "Near display lists and dynamic flex matrices; native stance held in place for contact and scale review.", font=FONT(16), fill="#93a6be")
    return image


def encoder(path):
    return subprocess.Popen(["ffmpeg", "-loglevel", "error", "-y", "-f", "rawvideo", "-vcodec", "rawvideo",
                             "-pix_fmt", "rgb24", "-s", f"{WIDTH}x{HEIGHT}", "-r", "20", "-i", "-", "-an",
                             "-c:v", "libx264", "-preset", "fast", "-crf", "18", "-pix_fmt", "yuv420p",
                             "-movflags", "+faststart", str(path)], stdin=subprocess.PIPE)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--player", type=Path, required=True); p.add_argument("--native", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--mode", choices=("spinner", "cane", "all"), default="all")
    p.add_argument("--still", action="store_true", help="One source frame, for narrow visual QA")
    p.add_argument("--cane-frame", type=int, default=0)
    p.add_argument("--cane-speed", choices=("scan", "nominal"), default="scan")
    args = p.parse_args(); args.output.mkdir(parents=True, exist_ok=True)
    archive, native = PlayerArchive(args.player), NativeAnimations(args.native)
    source, mask = verify_somaria_source()
    spinner = native.load("link_fighter_Lpower_kiru_wait")
    idle = native.load("link_fighter_wait_long")
    body = archive.geometry(hands="closed", include_sheath=False)
    renderer = Renderer(WIDTH, HEIGHT)
    metadata = archive.manifest() | native.manifest()
    old_manifest = args.output/"Player_animation_input_manifest.json"
    if old_manifest.exists():
        previous = json.loads(old_manifest.read_text())
        for key in ("spinner", "cane_of_somaria", "output_runs"):
            if key in previous: metadata[key] = previous[key]
    metadata.update({"somaria_resource_sha256": sha256(ANIM_PATH), "somaria_c_source_sha256": sha256(SOURCE_PATH),
                     "somaria_binary_matches_C": True, "source_frames": len(source),
                     "upper_body_mask_player_enum": list(range(10, 22)), "actor_scale": .01,
                     "child_root_translation_scale": .64, "movement_flags": 0,
                     "R_UPDATE_RATE": 3, "somaria_playSpeed": 2, "source_frames_per_gameplay_update": 3,
                     "body_triangles": sum(len(m.pos)//3 for m in body),
                     "model_configuration": "Young Din POC4, near LOD, both closed hands, no back equipment",
                     "limitations": ["Offline source sampler, not a running game or runtime camera",
                                     "Static light and approximate RDP material/reflection evaluation",
                                     "Flat-floor control; native inverse kinematics and head/torso tracking not simulated",
                                     "Actor shape yaw 0, native stance phase 0, no transition morph from preceding action",
                                     "Cane lower body uses real two-handed idle starting at phase 0; runtime phase can differ",
                                     "No scene collision, gameplay effects, camera, first-person state or hitbox verification"]})
    metadata["spinner_foot_measurements"] = foot_measurements(archive, body, spinner)
    production = ["soh/mods/items/objects/object_spinner.c", "soh/mods/items/objects/object_cane_of_somaria.c",
                  "soh/mods/items/helpers/equip_helper.c", "soh/mods/items/logic/item_spinner.c",
                  "soh/mods/items/logic/item_cane_of_somaria.c", "soh/src/code/z_skelanime.c",
                  "soh/src/code/z_player_lib.c", "soh/src/overlays/actors/ovl_player_actor/z_player.c"]
    metadata["production_source_sha256"] = {p: sha256(ROOT/p) for p in production}
    modes = ("spinner", "cane") if args.mode == "all" else (args.mode,)
    for mode in modes:
        slug = "spinner" if mode == "spinner" else "cane_of_somaria"
        path = ROOT/f"tools/nei_held/CHECKPOINTS/{slug}/{slug}.glb"
        item = glb_meshes(path); checkpoint = path.with_name("checkpoint.json")
        meta = json.loads(checkpoint.read_text()); draw_scale = meta["draw_scale"]
        metadata[slug] = {"glb": str(path.relative_to(ROOT)), "glb_sha256": sha256(path),
                          "checkpoint_sha256": sha256(checkpoint), "checkpoint": meta}
        slow = mode == "cane" and args.cane_speed == "scan"
        count = 1 if args.still else (96 if mode == "spinner" else (60 if slow else 84))
        stem = "Spinner_Actual_Player_Animation" if mode == "spinner" else ("Somaria_Actual_Cast_Slow_Scan" if slow else "Somaria_Actual_Cast_Game_Timing")
        output = args.output/(stem+".mp4")
        proc = encoder(output) if not args.still else None
        matrices, source_frames, samples, item_matrices = [], [], [], []
        for frame in range(count):
            renderer.clear()
            if mode == "spinner":
                source_frame = int(frame*1.5) % len(spinner)
                joints = spinner[source_frame]
                world = archive.world(joints, actor_position=(0, 17, 0))
                im = translation(0, 17, 0) @ rotate("y", frame*11.25) @ scale(draw_scale)
                center, span = (0, 26, 0), 65
            else:
                source_frame = args.cane_frame if args.still else (frame if slow else min((frame % 21)*3, 59))
                joints = idle[int(source_frame*.5) % len(idle)].copy()
                joints[mask] = source[source_frame, mask]
                world = archive.world(joints)
                im = world[18] @ scale(100) @ translation(0, 2.1622, -.045) @ rotate("z", -90) @ scale(draw_scale)
                center, span = (0, 24, 0), 72
            matrices.append(world); item_matrices.append(im); source_frames.append(source_frame)
            prepared = renderer.prepare(body, world) + renderer.prepare(item, im[None])
            for col, yaw in enumerate((25, 110)):
                renderer.view(col*720, 110, 720, 630, yaw=yaw, pitch=8, center=center, span=span)
                renderer.grid(); renderer.draw(prepared)
            image = annotate(renderer.image(), mode, frame, source_frame, count, slow)
            capture = (0, 8, 16, 24, 32, count-1) if mode == "spinner" else ((0, 6, 12, 30, 48, 59) if slow else (0, 2, 4, 10, 16, 20))
            if frame in capture:
                samples.append((source_frame, image.copy()))
            if frame == (0 if args.still else (16 if mode == "spinner" else 30)):
                image.save(args.output/(stem+".png"))
            if proc: proc.stdin.write(image.tobytes())
        if proc:
            proc.stdin.close()
            if proc.wait() != 0: raise RuntimeError("ffmpeg did not finish")
        # World matrices are fitting evidence; do not export native packed joint data.
        np.savez_compressed(args.output/(stem+"_world_matrices.npz"),
                            world_matrices=np.array(matrices), item_world_matrices=np.array(item_matrices),
                            source_frames=np.array(source_frames), player_limb_enum=np.arange(1, 22))
        metadata[slug]["matrix_chain"] = ("T(playerWorld) * RY(gameplayFrame*0x800 binang) * S(.2) * GLBscene" if mode == "spinner" else
                                            "rightWrist * S(1/.01) * T(0,2.1622,-.045) * RZ(-90deg) * S(drawScale) * GLBscene")
        if not args.still:
            sheet = Image.new("RGB", (1440, ((len(samples)+1)//2)*450), "#111923")
            for i, (frame, image) in enumerate(samples): sheet.paste(image.resize((720, 450)), ((i%2)*720, (i//2)*450))
            sheet.save(args.output/(stem+"_Frames.png"))
        metadata.setdefault("output_runs", {})[stem] = {
            "rendered_frames": count, "fps": 20, "source_frames": source_frames,
            "matrix_npz_sha256": sha256(args.output/(stem+"_world_matrices.npz")),
            "video_sha256": sha256(output) if proc else None,
            "preview_script_sha256": sha256(__file__),
            "sampler_script_sha256": sha256(Path(__file__).with_name("player_animation.py")),
            "renderer_script_sha256": sha256(Path(__file__).with_name("player_render.py")),
        }
        print(output if proc else args.output/(stem+".png"), flush=True)
    (args.output/"Player_animation_input_manifest.json").write_text(json.dumps(metadata, indent=2)+"\n")
    print(args.output/"Player_animation_input_manifest.json", flush=True)


if __name__ == "__main__": main()
