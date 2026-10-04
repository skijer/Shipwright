"""Render actual POC archive data; the held-pose camera is illustrative.

Hand-local +X is drawn upward to inspect the intended guarding orientation.
This does not reproduce skeletal attachment or establish in-game correctness.
"""
import argparse
import math
from pathlib import Path
import struct
import zipfile

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = "objects/din_fire_shield/poc1/"


def read_archive(path):
    with zipfile.ZipFile(path) as archive:
        textures = []
        for name in ("FlowTex", "FlameTex"):
            data = archive.read(ROOT + name)
            version = struct.unpack_from('<I', data, 8)[0]
            kind, width, height = struct.unpack_from('<III', data, 64)
            if version == 0:
                pixels = np.frombuffer(data[80:], np.uint8).reshape(height, width)
            else:
                assert struct.unpack_from('<Iff', data, 76) == (3, 1., 1.)
                pixels = np.frombuffer(data[92:], np.uint8).reshape(height, width, 4)[:, :, 0]
            textures.append(pixels.astype(float) / 255)
        layers = []
        for name in ("Surface", "Rim"):
            data = archive.read(ROOT + name + "Vertices")
            count = struct.unpack_from("<I", data, 68)[0]
            vertices = np.array([struct.unpack_from("<hhhHhhBBBB", data, 72 + i * 16)
                                 for i in range(count)], float)
            layers.append(vertices.reshape(-1, 3, 10))
    return textures, layers


def sample(texture, s, t):
    height, width = texture.shape
    # Match libultraship's non-rectangle bilinear half-texel adjustment,
    # then sample the physical whole-image upload using native logical UVs.
    x, y = (s + .5) * width / 64 - .5, (t + .5) * height / 32 - .5
    ix, iy = np.floor(x).astype(int), np.floor(y).astype(int)
    fx, fy = x - ix, y - iy
    return ((1-fx)*(1-fy)*texture[iy % height, ix % width]
            + fx*(1-fy)*texture[iy % height, (ix+1) % width]
            + (1-fx)*fy*texture[(iy+1) % height, ix % width]
            + fx*fy*texture[(iy+1) % height, (ix+1) % width])


def font(size):
    return ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', size)


def render(data, phase, title, background=(18, 22, 31)):
    textures, layers = data
    width, height = 420, 440
    rgb = np.full((height, width, 3), background, dtype=float) / 255
    angle = math.radians(15)
    alpha = int((.96 + .04 * math.sin(phase * .71)) * 255) / 255
    prim, env = np.array([255, 225, 122]) / 255, np.array([255, 43, 3]) / 255
    for layer, mesh in enumerate(layers):
        scroll = ((phase * (3 if layer == 0 else 7)) & 127) / 4
        for triangle in mesh:
            p = triangle[:, :3] * np.array([2.4, 2.7, 1])
            xs = (p[:, 1] * math.cos(angle) + p[:, 2] * math.sin(angle)) * .060 + width / 2
            ys = -p[:, 0] * .060 + 242
            left, right = max(0, int(np.floor(xs.min()))), min(width-1, int(np.ceil(xs.max())))
            top, bottom = max(0, int(np.floor(ys.min()))), min(height-1, int(np.ceil(ys.max())))
            if right < left or bottom < top:
                continue
            xx, yy = np.meshgrid(np.arange(left, right+1)+.5, np.arange(top, bottom+1)+.5)
            denom = (ys[1]-ys[2])*(xs[0]-xs[2]) + (xs[2]-xs[1])*(ys[0]-ys[2])
            if abs(denom) < 1e-9:
                continue
            a = ((ys[1]-ys[2])*(xx-xs[2]) + (xs[2]-xs[1])*(yy-ys[2])) / denom
            b = ((ys[2]-ys[0])*(xx-xs[2]) + (xs[0]-xs[2])*(yy-ys[2])) / denom
            c = 1-a-b
            inside = (a >= 0) & (b >= 0) & (c >= 0)
            weights = np.stack([a, b, c], axis=-1)
            st = weights @ (triangle[:, 4:6] / 32)
            texel = sample(textures[layer], st[:, :, 0], st[:, :, 1]-scroll)
            vertex_alpha = (weights @ (triangle[:, 9] / 255)).clip(0, 1)
            blend = (texel * vertex_alpha * alpha * inside)[:, :, None]
            color = env + texel[:, :, None] * (prim-env)
            region = rgb[top:bottom+1, left:right+1]
            region[:] = region * (1-blend) + color * blend
    result = Image.fromarray((rgb.clip(0, 1)*255).astype(np.uint8))
    draw = ImageDraw.Draw(result)
    draw.text((20, 18), title, font=font(19), fill=(255, 234, 207))
    draw.text((20, 46), "Actual archive pixels / UVs / alpha", font=font(13), fill=(199, 209, 224))
    tex_height, tex_width = textures[0].shape
    draw.text((20, 411), f"{tex_width} x {tex_height} physical texture pixels", font=font(13), fill=(199, 209, 224))
    return result


def comparison(old, new, phase, background=(18, 22, 31)):
    result = Image.new('RGB', (840, 484), (12, 15, 22))
    result.paste(render(old, phase, "POC1 / Original core", background), (0, 0))
    result.paste(render(new, phase, "POC2 / HD fire + rotated UV", background), (420, 0))
    draw = ImageDraw.Draw(result)
    draw.text((20, 452), "OFFLINE RENDER | Illustrative held-pose orientation; POC2 needs in-game verification.",
              font=font(14), fill=(199, 209, 224))
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('baseline', type=Path)
    parser.add_argument('candidate', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    old, new = read_archive(args.baseline), read_archive(args.candidate)
    frames = [comparison(old, new, phase) for phase in range(64)]
    frames[21].save(args.output / 'Din_Fire_Shield_POC2_Offline.png')
    comparison(old, new, 21, (103, 109, 116)).save(args.output / 'Din_Fire_Shield_POC2_Light_Background.png')
    frames[0].save(args.output / 'Din_Fire_Shield_POC2_Offline.gif', save_all=True,
                   append_images=frames[1:], duration=50, loop=0, disposal=2)
    print('Rendered 64 frames from both archives; POC2 has no runtime claim.')


if __name__ == '__main__':
    main()
