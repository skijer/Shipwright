"""Offline software preview of the delivered archive, not captured gameplay.

Reads actual triangles, UVs, vertex alpha and I8 pixels. Replays the hook's
two-cycle color/alpha expression and scrolling offsets. Camera is illustrative;
attachment, game render ordering and interpolation still need runtime proof.
"""
import argparse
import math
from pathlib import Path
import struct
import zipfile

import numpy as np
from PIL import Image, ImageDraw, ImageFont


def read_archive(path):
    with zipfile.ZipFile(path) as z:
        root = "objects/din_fire_shield/poc1/"
        tex = [np.frombuffer(z.read(root + name)[80:], np.uint8).reshape(32, 64).astype(float) / 255
               for name in ("FlowTex", "FlameTex")]
        layers = []
        for name in ("Surface", "Rim"):
            data = z.read(root + name + "Vertices")
            count = struct.unpack_from("<I", data, 68)[0]
            vertices = np.array([struct.unpack_from("<hhhHhhBBBB", data, 72 + i * 16) for i in range(count)], float)
            layers.append(vertices.reshape(-1, 3, 10))
    return tex, layers


def texture_sample(texture, s, t):
    # GBI ST is 1/32 texel; tile origin offsets are 1/4 texel.
    x = s - .5; y = t - .5
    ix = np.floor(x).astype(int); iy = np.floor(y).astype(int)
    fx = x - ix; fy = y - iy
    return ((1-fx)*(1-fy)*texture[iy % 32, ix % 64] + fx*(1-fy)*texture[iy % 32, (ix+1) % 64]
            + (1-fx)*fy*texture[(iy+1) % 32, ix % 64] + fx*fy*texture[(iy+1) % 32, (ix+1) % 64])


def render(texture, layers, phase, opacity, yaw=25):
    width, height = 400, 470
    image = np.full((height, width, 3), [18, 22, 31], dtype=float) / 255
    angle = math.radians(yaw)
    bloom = .25 + .75 * opacity
    pulse = .96 + .04 * math.sin(phase * .71)
    alpha = int(opacity * pulse * 255) / 255
    for layer, mesh in enumerate(layers):
        prim = np.array([255, 225, 122]) / 255
        env = np.array([255, 43, 3]) / 255
        scroll = ((phase * (3 if layer == 0 else 7)) & 127) / 4
        for triangle in mesh:
            p = triangle[:, :3] * np.array([2.4*bloom, 2.7*bloom, 1])
            xs = (p[:, 0]*math.cos(angle) + p[:, 2]*math.sin(angle)) * .060 + width/2
            ys = -p[:, 1]*.060 + 255
            left = max(0, int(np.floor(xs.min()))); right = min(width-1, int(np.ceil(xs.max())))
            top = max(0, int(np.floor(ys.min()))); bottom = min(height-1, int(np.ceil(ys.max())))
            xx, yy = np.meshgrid(np.arange(left, right+1)+.5, np.arange(top, bottom+1)+.5)
            denom = (ys[1]-ys[2])*(xs[0]-xs[2]) + (xs[2]-xs[1])*(ys[0]-ys[2])
            if abs(denom) < 1e-9:
                continue
            a = ((ys[1]-ys[2])*(xx-xs[2]) + (xs[2]-xs[1])*(yy-ys[2])) / denom
            b = ((ys[2]-ys[0])*(xx-xs[2]) + (xs[0]-xs[2])*(yy-ys[2])) / denom
            c = 1-a-b
            inside = (a >= 0) & (b >= 0) & (c >= 0)
            weights = np.stack([a, b, c], axis=-1)
            st = weights @ (triangle[:, 4:6]/32)
            texel = texture_sample(texture[layer], st[:, :, 0], st[:, :, 1]-scroll)
            shade_alpha = (weights @ (triangle[:, 9]/255)).clip(0, 1)
            blend = (texel*shade_alpha*alpha*inside)[:, :, None]
            color = env + texel[:, :, None]*(prim-env)
            region = image[top:bottom+1, left:right+1]
            region[:] = region*(1-blend) + color*blend
    result = Image.fromarray((image.clip(0,1)*255).astype(np.uint8))
    draw = ImageDraw.Draw(result)
    font_path = Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf')
    font = ImageFont.truetype(str(font_path), 16) if font_path.exists() else ImageFont.load_default()
    small = ImageFont.truetype(str(font_path), 12) if font_path.exists() else ImageFont.load_default()
    draw.text((22,20), 'DIN / FIRE SHIELD POC1', fill=(242,222,190), font=font)
    draw.text((22,46), 'Archive geometry + texture · offline preview', fill=(158,169,185), font=small)
    draw.text((22,438), 'In-game attachment and appearance untested', fill=(158,169,185), font=small)
    return result


def render_get_item(archive, textures, flame_layers):
    """Actual GI vertices and shared flame material, with an illustrative camera."""
    with zipfile.ZipFile(archive) as z:
        data = z.read("objects/din_fire_shield/poc1/GIBracerVertices")
        count = struct.unpack_from("<I", data, 68)[0]
        bracer = np.array([struct.unpack_from("<hhhHhhBBBB", data, 72+i*16) for i in range(count)], float)
    meshes = [bracer.reshape(-1, 3, 10)] + [x.copy() for x in flame_layers]
    meshes[0][:, :, :3] *= .65
    for mesh in meshes[1:]:
        mesh[:, :, :3] = (mesh[:, :, :3] * [.035, .043, .015] + [0, 18, -45]) * .65
    width = height = 480
    rgb = np.full((height, width, 3), [18, 22, 31], dtype=float) / 255
    depth = np.full((height, width), -np.inf)
    yaw = math.radians(-28); pitch = math.radians(-12)
    phase = 21
    for layer, mesh in enumerate(meshes):
        for triangle in mesh:
            p = triangle[:, :3]
            x = p[:, 0]*math.cos(yaw) + p[:, 2]*math.sin(yaw)
            z = -p[:, 0]*math.sin(yaw) + p[:, 2]*math.cos(yaw)
            y = p[:, 1]*math.cos(pitch) - z*math.sin(pitch)
            z = p[:, 1]*math.sin(pitch) + z*math.cos(pitch)
            xs = x*5 + width/2; ys = -y*5 + 270
            left = max(0, int(np.floor(xs.min()))); right = min(width-1, int(np.ceil(xs.max())))
            top = max(0, int(np.floor(ys.min()))); bottom = min(height-1, int(np.ceil(ys.max())))
            if right < left or bottom < top: continue
            xx, yy = np.meshgrid(np.arange(left, right+1)+.5, np.arange(top, bottom+1)+.5)
            denom = (ys[1]-ys[2])*(xs[0]-xs[2]) + (xs[2]-xs[1])*(ys[0]-ys[2])
            if abs(denom) < 1e-9: continue
            a = ((ys[1]-ys[2])*(xx-xs[2]) + (xs[2]-xs[1])*(yy-ys[2])) / denom
            b = ((ys[2]-ys[0])*(xx-xs[2]) + (xs[0]-xs[2])*(yy-ys[2])) / denom
            c = 1-a-b
            weights = np.stack([a,b,c], axis=-1)
            pixel_z = weights @ z
            depth_region = depth[top:bottom+1, left:right+1]
            inside = (a >= 0) & (b >= 0) & (c >= 0) & (pixel_z >= depth_region)
            region = rgb[top:bottom+1, left:right+1]
            if layer == 0:
                region[inside] = (weights @ (triangle[:, 6:9]/255))[inside]
                depth_region[inside] = pixel_z[inside]
            else:
                st = weights @ (triangle[:, 4:6]/32)
                scroll = ((phase * (3 if layer == 1 else 7)) & 127)/4
                texel = texture_sample(textures[layer-1], st[:, :, 0], st[:, :, 1]-scroll)
                alpha = int((.96 + .04*math.sin(phase*.71))*255)/255
                blend = (texel*(weights @ (triangle[:, 9]/255))*alpha*inside)[:, :, None]
                env = np.array([255,43,3])/255; prim = np.array([255,225,122])/255
                color = env + texel[:, :, None]*(prim-env)
                region[:] = region*(1-blend) + color*blend
    result = Image.fromarray((rgb.clip(0,1)*255).astype(np.uint8))
    draw = ImageDraw.Draw(result)
    font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 15)
    draw.text((20,18), 'DIN / GET-ITEM MODEL · OFFLINE', font=font, fill=(242,222,190))
    draw.text((20,447), 'Actual archive geometry; game view untested', font=font, fill=(158,169,185))
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('archive', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    texture, layers = read_archive(args.archive)
    args.output.mkdir(parents=True, exist_ok=True)
    frames = []
    opacity = 0
    for tick in range(1, 54):
        guarding = 4 <= tick < 45
        opacity = min(1, max(0, opacity + (.25 if guarding else -.18)))
        frames.append(render(texture, layers, tick, opacity))
    frames[20].save(args.output/'Din_Fire_Shield_POC1_Offline.png')
    frames[0].save(args.output/'Din_Fire_Shield_POC1_Offline.gif', save_all=True,
                   append_images=frames[1:], duration=50, loop=0, disposal=2)
    render_get_item(args.archive, texture, layers).save(args.output/'Din_Fire_Shield_GI_Offline.png')
    with zipfile.ZipFile(args.archive) as z:
        data = z.read('objects/din_fire_shield/poc1/IconTex')
        Image.frombytes('RGBA', (32,32), data[80:]).save(args.output/'Din_Fire_Shield_Icon_32.png')
    print('Rendered 53 frames from the delivered archive; no runtime claim.')


if __name__ == '__main__':
    main()
