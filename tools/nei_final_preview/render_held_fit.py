"""Compact rod and shovel fit comparisons from actual player source poses."""
import argparse
import json
import math
from pathlib import Path

import numpy as np
from PIL import ImageDraw, ImageFont

from player_animation import (ROOT, PlayerArchive, NativeAnimations, verify_somaria_source,
                              read_player_animation, glb_meshes, translation, rotate, scale, sha256)
from player_render import Renderer

F = lambda size: ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", size)


def item(slug):
    path = ROOT/f"tools/nei_held/CHECKPOINTS/{slug}/{slug}.glb"
    return glb_meshes(path), path


def shovel_matrix(world, draw_scale):
    left, right = world[15, :3, 3], world[18, :3, 3]
    midpoint = (left+right)*.5
    dx, dy, dz = left-right
    yaw = math.degrees(math.atan2(dx, dz))
    pitch = math.degrees(math.atan2(dy, math.hypot(dx, dz)))
    return translation(*midpoint) @ rotate("y", yaw) @ rotate("x", -pitch) @ rotate("y", 90) @ scale(draw_scale)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--player", type=Path, required=True); p.add_argument("--native", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True); a = p.parse_args(); a.output.mkdir(parents=True, exist_ok=True)
    young = PlayerArchive(a.player); native = NativeAnimations(a.native)
    ready = native.load("link_fighter_wait_long")
    idle = native.load("link_normal_wait_free")
    _, mask = verify_somaria_source()
    dig_path = ROOT/"soh/assets/custom/misc/link_animetion/gPlayerAnim_nei_dampe_dig"
    dig, _ = read_player_animation(dig_path)
    dig_frame = 37
    joints = idle[dig_frame % len(idle)].copy(); joints[mask] = dig[dig_frame, mask]
    rod_world, dig_world = young.world(ready[0]), young.world(joints)
    closed, opened = young.geometry(hands="closed", include_sheath=False), young.geometry(hands="open", include_sheath=False)
    r = Renderer(1440, 1250); r.clear()
    rod_body = r.prepare(closed, rod_world); dig_body = r.prepare(opened, dig_world)
    items, transforms = {}, {}
    for col, slug in enumerate(("fire_rod", "ice_rod", "light_rod")):
        mesh, path = item(slug)
        m = rod_world[15] @ scale(100) @ translation(0, 2.1622, .045) @ rotate("z", -122) @ scale(.05)
        r.view(col*480, 645, 480, 500, yaw=25, pitch=8, center=(0, 21, 0), span=56)
        r.grid(); r.draw(rod_body + r.prepare(mesh, m[None]))
        items[slug] = {"glb_sha256": sha256(path), "draw_scale": .05}
        transforms[slug] = m.tolist()
    mesh, path = item("shovel")
    for col, draw_scale in enumerate((.06, .048, .048)):
        m = shovel_matrix(dig_world, draw_scale)
        if col == 2:
            midpoint = (dig_world[15, :3, 3]+dig_world[18, :3, 3])*.5
            r.view(col*480, 125, 480, 470, yaw=110, pitch=8, center=midpoint, span=31)
        else:
            r.view(col*480, 125, 480, 470, yaw=25, pitch=8, center=(10, 25, 6), span=74)
        r.grid(); r.draw(dig_body+r.prepare(mesh, m[None]))
        transforms["shovel_"+str(draw_scale)] = m.tolist()
    image = r.image(); d = ImageDraw.Draw(image)
    d.rectangle((0, 0, 1440, 92), fill="#111923")
    d.text((25, 14), "NEI  •  Rod grip and child shovel scale", font=F(30), fill="#f0f5ff")
    d.text((25, 56), "Young Din POC4  •  native ready pose + source dig animation  •  candidate draw matrices", font=F(18), fill="#aebed1")
    for col, label in enumerate(("Fire Rod", "Ice Rod", "Light Rod")):
        d.text((col*480+25, 108), label, font=F(24), fill="#efc977")
    for col, label in enumerate(("Shovel • previous scale", "Shovel • child −20%", "Two-hand attachment detail")):
        d.text((col*480+25, 650), label, font=F(22), fill="#efc977")
    d.rectangle((0, 1150, 1440, 1250), fill="#111923")
    d.text((25, 1166), "Rods: native two-handed idle frame 0; exact left wrist + native palm socket. Shovel: source dig frame 37/49.", font=F(18), fill="#dfe8f4")
    d.text((25, 1198), "Shovel keeps the existing two-hand midpoint/orientation; default open hands shown. Adult scale is −10% (not pictured).", font=F(17), fill="#a8b9ce")
    d.text((25, 1225), "Offline studio light/camera. Projectiles, particles, scene collision, first-person transitions and runtime acceptance remain untested.", font=F(15), fill="#91a5be")
    image.save(a.output/"Rods_and_Shovel_Actual_Pose_Fit.png")
    metadata = {"player_sha256": sha256(a.player), **native.manifest(),
                "dig_resource_sha256": sha256(dig_path), "dig_source_frame": dig_frame,
                "shovel_lower_body": "native link_normal_wait_free phase37, actual upper-body copy map",
                "rod_source_frame": 0, "items": items, "shovel_glb_sha256": sha256(path),
                "shovel_draw_scales": {"previous": .06, "child_candidate": .048, "adult_candidate": .054},
                "item_world_matrices": transforms, "offline_only": True,
                "script_sha256": sha256(__file__)}
    (a.output/"Held_fit_input_manifest.json").write_text(json.dumps(metadata, indent=2)+"\n")

    # Source-geometric compatibility proof of the universal native palm socket.
    r.clear()
    proof = [(PlayerArchive(a.native), "Native child", (0, 2.1622, .045)),
             (young, "Young Din POC4", (0, 2.1622, .045)),
             (PlayerArchive(a.native, age="adult"), "Native adult", (0, 3.28, -.77))]
    rod, _ = item("fire_rod")
    for row, (archive, label, socket) in enumerate(proof):
        hand = archive.geometry(only_limb=15)
        ph = r.prepare(hand, np.tile(scale(.01), (21, 1, 1)))
        pr = r.prepare(rod, (translation(*socket)@rotate("z", -122)@scale(.05))[None])
        for col, (yaw, pitch) in enumerate(((0, 0), (90, 0), (30, 25))):
            r.view(col*480, 70+(2-row)*375, 480, 375, yaw, pitch, center=(0, 2.2, 0), span=9)
            r.draw(ph+pr); r.axes(2)
    image = r.image(); d = ImageDraw.Draw(image)
    for row, (_, label, _) in enumerate(proof):
        for col, view in enumerate(("XY", "ZY", "Three-quarter")):
            d.text((col*480+18, 62+row*375), label+" / "+view, font=F(21), fill="#f0f5ff")
    d.text((20, 16), "Native palm socket compatibility  •  actual hand geometry, approximate material/light", font=F(21), fill="#efc977")
    d.text((20, 1200), "Child socket [0,216.22,+4.5]; adult [0,328,−77] native units. Shaft rotates −122° to hand +X.", font=F(20), fill="#c7d4e7")
    image.save(a.output/"Native_and_YoungDin_Grip_Compatibility.png")
    print(a.output/"Rods_and_Shovel_Actual_Pose_Fit.png")
    print(a.output/"Native_and_YoungDin_Grip_Compatibility.png")


if __name__ == "__main__": main()
