"""Compare actual native whip release poses on child, Young Din, and adult geometry.

Offline source-frame evidence only: no camera, gameplay or GPU integration proof.
"""
import argparse
import json
import math
from pathlib import Path
import subprocess

import numpy as np
from PIL import ImageDraw, ImageFont

from player_animation import (ROOT, PlayerArchive, NativeAnimations, glb_meshes, rotate, scale, sha256,
                              verify_somaria_source, crc64, read_texture)
from player_render import Renderer

FONT = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'

class PreviewPlayer(PlayerArchive):
    def texture(self, path, palette):
        kind, width, height, raw = read_texture(self.get(path))
        if kind == 9:
            key = (path, palette)
            if key not in self.texture_cache:
                self.used.add(path)
                ia = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 2)
                self.texture_cache[key] = np.c_[ia[:,0], ia[:,0], ia[:,0], ia[:,1]].reshape(height,width,4).copy()
            return self.texture_cache[key]
        return super().texture(path, palette)

    def display_commands(self, path, active=()):
        for op, w0, w1, hashed in super().display_commands(path, active):
            if op == 0xFD:
                # Native player eyes/mouth are runtime-bound segments. Select
                # actual open-eye / neutral-mouth textures for this control.
                suffix = {0x08000001: 'EyesOpenTex', 0x09000001: 'Mouth1Tex'}.get(w1)
                if suffix is None:
                    raise ValueError(f'Unknown segmented face texture {w1:#x}')
                resource = self.prefix + self.name + suffix
                self.get(resource)
                yield 0x20, w0, 0, crc64(resource)
            else:
                yield op, w0, w1, hashed


def pitched_world(archive, frame, pitch):
    world = archive.world(frame)
    # Player_OverrideLimbDrawGameplayDefault applies upperLimbRot before the
    # UPPER limb's local TranslateRotateZYX, in its parent's coordinate frame.
    upper = 9
    parent = archive.parents[upper]
    pivot = world[parent]
    adjustment = pivot @ rotate('x', pitch) @ np.linalg.inv(pivot)
    for limb in archive.order:
        ancestor = limb
        while ancestor >= 0 and ancestor != upper:
            ancestor = archive.parents[ancestor]
        if ancestor == upper:
            world[limb] = adjustment @ world[limb]
    return world


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native', type=Path, required=True)
    parser.add_argument('--din', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--frames', type=int, default=90)
    args = parser.parse_args(); args.output.mkdir(parents=True, exist_ok=True)
    native = NativeAnimations(args.native)
    throw = native.load('link_boom_throwR')
    reach = native.load('link_hook_wait')
    idle = native.load('link_normal_wait_free')
    _, mask = verify_somaria_source()
    players = [(PreviewPlayer(args.native), 'Native child'),
               (PreviewPlayer(args.din), 'Young Din POC4'),
               (PreviewPlayer(args.native, age='adult'), 'Native adult')]
    geometry = [p.geometry(hands='closed', include_sheath=False) for p, _ in players]
    handle_path = ROOT/'tools/nei_held/CHECKPOINTS/whip_handle/whip_handle.glb'
    handle = glb_meshes(handle_path)
    r = Renderer(1440, 1180)
    video = args.output/'Whip_Aimed_Release_Source_Preview.mp4'
    ffmpeg = subprocess.Popen(['ffmpeg', '-loglevel', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'rgb24',
                              '-s', '1440x1180', '-r', '20', '-i', '-', '-an', '-c:v', 'libx264',
                              '-preset', 'fast', '-crf', '18', '-pix_fmt', 'yuv420p', '-movflags', '+faststart',
                              str(video)], stdin=subprocess.PIPE)
    metrics = []
    for tick in range(args.frames):
        r.clear()
        pitch = 0 if tick < args.frames//2 else -25
        for col, ((archive, label), meshes) in enumerate(zip(players, geometry)):
            for row, candidate in enumerate((False, True)):
                source = reach if candidate else throw
                index = int(tick*1.5) % len(source) if candidate else min(int(tick*1.5), len(source)-1)
                frame = idle[0].copy(); frame[mask] = source[index, mask]
                world = pitched_world(archive, frame, pitch if candidate else 0)
                item_world = world[18] @ scale(100)
                center = (0, 34, 7) if archive.age=='adult' else (0, 23, 7)
                span = 90 if archive.age=='adult' else 65
                r.view(col*480, 95+(1-row)*490, 480, 455, yaw=58, pitch=8, center=center, span=span)
                r.grid(); r.draw(r.prepare(meshes,world) + r.prepare(handle,item_world[None]))
                # The short guide shows the requested launch direction only;
                # no synthetic tether/collision path is shown as runtime evidence.
                origin = world[18,:3,3]
                direction = np.array([0, -math.sin(math.radians(pitch)), math.cos(math.radians(pitch))])
                r.Disable(0x0DE1); r.Disable(0x0B44); r.LineWidth(2); r.Color4f(.25,.85,.95,1)
                r.Begin(0x0001); r.Vertex3f(*origin); r.Vertex3f(*(origin+direction*19)); r.End()
                if tick==args.frames-1:
                    metrics.append({'player':label,'candidate':candidate,'clip_frame':index,'aim_pitch_degrees':pitch,
                                    'hand':world[18,:3,3].tolist(),'shoulder':world[16,:3,3].tolist(),
                                    'handle_forward':(item_world[:3,1]).tolist()})
        image = r.image(); d = ImageDraw.Draw(image)
        d.rectangle((0,0,1440,98),fill='#111923')
        d.text((24,12),'Whip release • actual source poses and player geometry',font=ImageFont.truetype(FONT,28),fill='#edf4ff')
        d.text((24,52),'Upper: existing arm-down follow-through   /   Lower: candidate raised reach + captured aim',font=ImageFont.truetype(FONT,20),fill='#c1d3e8')
        for col,(_,label) in enumerate(players):
            d.text((col*480+22,106),label,font=ImageFont.truetype(FONT,23),fill='#e8c374')
            d.text((col*480+22,585),'Candidate • '+('level aim' if pitch==0 else '25° upward aim'),font=ImageFont.truetype(FONT,21),fill='#a8e8d2')
        d.rectangle((0,1080,1440,1180),fill='#111923')
        d.text((24,1093),'Native boom_throwR versus hook_wait • same idle lower body • actual approved whip handle attachment',font=ImageFont.truetype(FONT,18),fill='#d5e1ef')
        d.text((24,1123),'Cyan line = requested aim. Offline studio camera/light; first-person transition, collisions and runtime acceptance untested.',font=ImageFont.truetype(FONT,17),fill='#a2b5cd')
        d.text((24,1152),'Native child / Young Din POC4 / native adult shown. Adult Din is not present in the supplied reference archive.',font=ImageFont.truetype(FONT,17),fill='#a2b5cd')
        if tick==args.frames-1: image.save(args.output/'Whip_Aimed_Release_Source_Preview.png')
        ffmpeg.stdin.write(image.tobytes())
    ffmpeg.stdin.close(); assert ffmpeg.wait()==0
    manifest = {**native.manifest(), 'din_archive_sha256':sha256(args.din), 'handle_sha256':sha256(handle_path),
                'script_sha256':sha256(__file__), 'offline_only':True, 'metrics':metrics,
                'candidate_source':'native link_hook_wait; captured pitch applied through native upper-limb pre-rotation',
                'lower_body':'native idle frame0, actual upper-body copy map',
                'limitations':['No first-person camera state','No gameplay/hit/tether simulation','No adult Din reference']}
    (args.output/'Whip_Aimed_Release_Manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(video)

if __name__=='__main__': main()
