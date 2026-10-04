"""Render the native seated Anju loop. Requires NumPy, SciPy and Pillow; reads the supplied MM archive only."""
from pathlib import Path
from typing import Tuple
import argparse
import struct
import sys
import zipfile

import numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy.spatial.transform import Rotation

_POLY = 0x42F0E1EBA9EA3693


def _crc_table():
    t = []
    for n in range(256):
        c = n << 56
        for _ in range(8):
            c = ((c << 1) ^ _POLY) & 0xFFFFFFFFFFFFFFFF if (c >> 63) & 1 else (c << 1) & 0xFFFFFFFFFFFFFFFF
        t.append(c)
    return t


_CRC_TBL = _crc_table()


def crc64(path: str) -> int:
    """CRC-64/ECMA-182, init all-ones, NO final xor (libultraship StrHash64)."""
    crc = 0xFFFFFFFFFFFFFFFF
    for ch in path.encode("ascii"):
        crc = (_CRC_TBL[((crc >> 56) ^ ch) & 0xFF] ^ (crc << 8)) & 0xFFFFFFFFFFFFFFFF
    return crc



def _read_header(data: bytes):
    bo = data[0]
    endt = "<" if bo == 0 else ">"
    rtype = struct.unpack_from(endt + "I", data, 0x04)[0]
    ver = struct.unpack_from(endt + "I", data, 0x08)[0]
    return bo, endt, rtype, ver


def read_texture(data: bytes) -> Tuple[int, int, int, bytes]:
    bo, e, rtype, ver = _read_header(data)
    ttype, w, h = struct.unpack_from(e + "III", data, 0x40)
    if ver == 0:
        size = struct.unpack_from(e + "I", data, 0x4C)[0]
        body = data[0x50:0x50 + size]
    else:
        size = struct.unpack_from(e + "I", data, 0x58)[0]
        body = data[0x5C:0x5C + size]
    return ttype, w, h, body



def read_vertex_array(data: bytes):
    bo, e, rtype, ver = _read_header(data)
    atype, count = struct.unpack_from(e + "II", data, 0x40)
    verts = []
    off = 0x48
    for _ in range(count):
        rec = struct.unpack_from(e + "hhhHhhBBBB", data, off)
        verts.append(rec)
        off += 16
    return verts

def commands(data):
    offset = 72
    while offset < len(data):
        w0, w1 = struct.unpack_from('<II', data, offset)
        offset += 8
        op, hashed = w0 >> 24, None
        if op in (0x20, 0x31, 0x32, 0x33, 0x35, 0x36, 0x37):
            hi, lo = struct.unpack_from('<II', data, offset)
            offset += 8
            hashed = hi << 32 | lo
        yield op, w0, w1, hashed
        if op == 0xDF:
            return
    raise ValueError('Display list has no end command')


class Native:
    def __init__(self, archive=None):
        with zipfile.ZipFile(archive or UPLOAD / 'mm(1).o2r') as z:
            self.resources = {n: z.read(n) for n in z.namelist()
                              if n.startswith(('objects/object_stk/', 'objects/object_stk2/'))}
        self.hashes = {crc64(n): n for n in self.resources}
        b = self.resources[NATIVE + 'gSkullKidSkel']
        assert tuple(b[64:66]) == (1, 1)
        assert struct.unpack_from('<II', b, 66) == (21, 20)
        offset, self.names = 79, []
        while offset < len(b):
            n, = struct.unpack_from('<I', b, offset)
            offset += 4
            self.names.append(b[offset:offset+n].decode())
            offset += n
        self.offsets, self.children, self.siblings, self.dlists = [], [], [], []
        for name in self.names:
            b = self.resources[name]
            x, y, z, c, s = struct.unpack('<hhhBB', b[-8:])
            self.offsets.append((x, y, z)); self.children.append(c); self.siblings.append(s)
            n, = struct.unpack_from('<I', b, 106)
            self.dlists.append(b[110:110+n].decode())
        self.offsets = np.array(self.offsets, dtype=float)
        self.parents = np.full(21, -1, dtype=int)
        def walk(i, parent):
            self.parents[i] = parent
            if self.children[i] != 255: walk(self.children[i], i)
            if self.siblings[i] != 255: walk(self.siblings[i], parent)
        walk(0, -1)
        self.animations = {}
        for name, b in self.resources.items():
            if not name.endswith('Anim') or 'TexAnim' in name:
                continue
            kind, count, num = struct.unpack_from('<IhI', b, 64)
            if kind != 0:
                continue
            data = np.frombuffer(b, dtype='<i2', count=num, offset=74)
            offset = 74 + num*2
            joints, = struct.unpack_from('<I', b, offset); offset += 4
            if joints != 22:
                continue
            indices = np.frombuffer(b, dtype='<u2', count=joints*3, offset=offset).reshape(joints, 3)
            offset += joints*6
            static, = struct.unpack_from('<H', b, offset)
            assert offset + 2 == len(b)
            indices = indices[None] + np.where(indices[None] >= static, np.arange(count)[:, None, None], 0)
            self.animations[name.split('/')[-1]] = data[indices].copy()
        self.bind_frame = self.animations['gSkullKidTPoseAnim'][0]
        self.bind = self.world(self.bind_frame)

    def world(self, frame):
        matrices = np.tile(np.eye(4), (21, 1, 1))
        matrices[:, :3, :3] = Rotation.from_euler('ZYX', frame[1:, ::-1]*np.pi/32768).as_matrix()
        matrices[:, :3, 3] = self.offsets
        matrices[0, :3, 3] = frame[0]
        for i, parent in enumerate(self.parents):
            if parent >= 0:
                matrices[i] = matrices[parent] @ matrices[i]
        return matrices

PREFIX = 'objects/object_an1/'
ANIM = 'objects/object_an2/gAnju2UmbrellaCryAnim'
UMBRELLA = 'objects/object_an2/gAnju2UmbrellaDL'
POSES = [('0x7E0D', 'Seated umbrella crying', ANIM)]


def rgba16(raw):
    q = np.frombuffer(raw, dtype='>u2').astype(np.uint32)
    rgb = np.stack([(q >> 11) & 31, (q >> 6) & 31, (q >> 1) & 31], axis=1)
    rgb = (rgb << 3) | (rgb >> 2)
    return np.c_[rgb, (q & 1) * 255].astype(np.uint8)


class Anju:
    def __init__(self, archive, standing=False, overlay=None, native_offsets=False):
        self.animation = 'objects/object_an2/gAnju2UmbrellaIdleAnim' if standing else ANIM
        self.eye = 'gAnju1EyeOpenTex' if standing else 'gAnju1EyeSadTex'
        self.umbrella = UMBRELLA
        with zipfile.ZipFile(archive) as z:
            self.resources = {n: z.read(n) for n in z.namelist() if n.startswith((PREFIX, 'objects/object_an2/'))}
        native_limbs = {n: b for n, b in self.resources.items() if n.endswith('Limb')}
        if overlay:
            with zipfile.ZipFile(overlay) as z:
                self.resources.update({n.removeprefix('alt/'): z.read(n) for n in z.namelist()
                                       if n.startswith(('alt/objects/', 'alt/custom/', 'objects/object_anju_hd/v1/')) and not n.endswith('/')})
            if native_offsets:
                for name, data in native_limbs.items():
                    if name in self.resources:
                        self.resources[name] = self.resources[name][:-8] + data[-8:]
        self.hashes = {crc64(n): n for n in self.resources}
        b = self.resources[PREFIX + 'gAnju1Skel']
        assert struct.unpack_from('<bbII', b, 64) == (1, 1, 20, 19)
        p, names = 79, []
        while p < len(b):
            length, = struct.unpack_from('<I', b, p)
            p += 4
            names.append(b[p:p + length].decode())
            p += length
        self.offsets, self.children, self.siblings, self.dlists = [], [], [], []
        for name in names:
            b = self.resources[name]
            x, y, z, child, sibling = struct.unpack('<hhhBB', b[-8:])
            length, = struct.unpack_from('<I', b, 106)
            self.dlists.append(b[110:110 + length].decode())
            self.offsets.append([x, y, z])
            self.children.append(child)
            self.siblings.append(sibling)
        self.parents = np.full(len(names), -1)
        self.order = []

        def walk(i, parent):
            self.parents[i] = parent
            self.order.append(i)
            if self.children[i] != 255:
                walk(self.children[i], i)
            if self.siblings[i] != 255:
                walk(self.siblings[i], parent)

        walk(0, -1)
        assert len(set(self.order)) == 20
        self.slots = [i for i in self.order if self.dlists[i]]
        assert len(self.slots) == 19
        private_prefix = 'objects/object_anju_hd/v1/'
        if private_prefix + 'gAnju1HeadDL' in self.resources:
            self.dlists = [private_prefix + n.split('/')[-1] if n else n for n in self.dlists]
            for i in (1, 9, 16):
                self.offsets[i][1] += 476
            self.umbrella = private_prefix + 'gAnju2UmbrellaDL'
        self.clips = {}
        for name in [self.animation]:
            b = self.resources[name]
            kind, frames, count = struct.unpack_from('<IhI', b, 64)
            assert kind == 0 and frames == (32 if standing else 43)
            data = np.frombuffer(b, dtype='<i2', count=count, offset=74)
            p = 74 + count * 2
            joints, = struct.unpack_from('<I', b, p)
            p += 4
            assert joints == 21
            indices = np.frombuffer(b, dtype='<u2', count=joints * 3, offset=p).reshape(joints, 3)
            p += joints * 6
            static, = struct.unpack_from('<H', b, p)
            assert p + 2 == len(b)
            indices = indices[None] + np.where(indices[None] >= static, np.arange(frames)[:, None, None], 0)
            assert indices.max() < count
            self.clips[name] = data[indices].copy()
        self.texture_cache = {}
        self.vertex_cache = {}

    def texture(self, path, palette):
        key = (path, palette)
        if key not in self.texture_cache:
            kind, width, height, raw = read_texture(self.resources[path])
            if kind == 1:
                rgba = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 4)
            elif kind == 2:
                rgba = rgba16(raw)
            elif kind == 4:
                assert palette
                colors = rgba16(read_texture(self.resources[palette])[3])
                rgba = colors[np.frombuffer(raw, dtype=np.uint8)]
            elif kind == 6:
                gray = np.frombuffer(raw, dtype=np.uint8)
                rgba = np.c_[gray, gray, gray, np.full_like(gray, 255)]
            else:
                raise ValueError((path, kind))
            self.texture_cache[key] = rgba.reshape(height, width, 4)
        return self.texture_cache[key]

    def display_commands(self, path, active=()):
        if path in active or len(active) > 16:
            raise ValueError('Cyclic or excessive display-list recursion: ' + path)
        for op, w0, w1, hashed in commands(self.resources[path]):
            if op == 0x31:
                yield from self.display_commands(self.hashes[hashed], active + (path,))
                if (w0 >> 16) & 1:
                    return
            elif op != 0xDF:
                yield op, w0, w1, hashed

    def world(self, frame):
        world = np.tile(np.eye(4), (20, 1, 1))
        world[:, :3, :3] = Rotation.from_euler('ZYX', frame[1:, ::-1] * np.pi / 32768).as_matrix()
        world[:, :3, 3] = self.offsets
        world[0, :3, 3] = frame[0]
        for i in self.order:
            if self.parents[i] >= 0:
                world[i] = world[self.parents[i]] @ world[i]
        return world

    def pose(self, name, frame=0):
        matrices = self.world(self.clips[name][frame])
        groups, cache = {}, {}
        geom, texture, palette, tile, origin = 0x230405, None, None, 0, (0, 0)
        prim, env = (255, 255, 255), (0, 0, 0)
        combiner = None
        draw_order = [(i, self.dlists[i]) for i in self.order]
        hand = next(i for i, item in enumerate(draw_order) if item[0] == 7)
        draw_order.insert(hand + 1, (7, self.umbrella))
        for owner, path in draw_order:
            if not path:
                continue
            joint = owner
            for op, w0, w1, hashed in self.display_commands(path):
                if op == 0xDA:
                    assert w1 >> 24 == 0x0D
                    joint = self.slots[(w1 & 0xFFFFFF) // 64]
                elif op == 0xD9:
                    geom = (geom & (w0 & 0xFFFFFF)) | w1
                elif op == 0x20:
                    texture = self.hashes[hashed]
                elif op == 0xFD:
                    segment = w1 >> 24
                    assert segment in (8, 9)
                    texture = PREFIX + (self.eye if segment == 8 else 'gAnju1MouthClosedTex')
                elif op == 0xFA:
                    prim = tuple((w1 >> shift) & 255 for shift in (24, 16, 8))
                elif op == 0xFB:
                    env = tuple((w1 >> shift) & 255 for shift in (24, 16, 8))
                elif op == 0xFC:
                    combiner = (w0, w1)
                elif op == 0xF0:
                    palette = texture
                elif op == 0xF5 and (w1 >> 24) & 7 == 0:
                    tile = w1
                elif op == 0xF2 and (w1 >> 24) & 7 == 0:
                    origin = (((w0 >> 12) & 4095) / 4, (w0 & 4095) / 4)
                elif op == 0x32:
                    vpath = self.hashes[hashed]
                    if vpath not in self.vertex_cache:
                        self.vertex_cache[vpath] = read_vertex_array(self.resources[vpath])
                    verts = self.vertex_cache[vpath]
                    count = (w0 >> 12) & 255
                    start = ((w0 >> 1) & 127) - count
                    assert w1 % 16 == 0 and w1 // 16 + count <= len(verts)
                    matrix = matrices[joint]
                    for k, vertex in enumerate(verts[w1 // 16:w1 // 16 + count]):
                        normal = np.array(vertex[6:9], dtype=float)
                        normal[normal > 127] -= 256
                        cache[start + k] = (matrix[:3, :3] @ vertex[:3] + matrix[:3, 3],
                                            matrix[:3, :3] @ (normal / 127), np.array(vertex[4:6]) / 32)
                elif op in (5, 6):
                    intensity = read_texture(self.resources[texture])[0] == 6 or combiner == (0xFC30FE80, 0x5FFEF3F8)
                    key = (texture, palette, tile, origin, bool(geom & 0x400), prim if intensity else None, env if intensity else None)
                    raw_header = self.resources[texture]
                    texture_scale = struct.unpack_from('<f', raw_header, 84)[0] if _read_header(raw_header)[3] else 1.0
                    group = groups.setdefault(key, {'pos': [], 'normal': [], 'uv': [], 'tri': [],
                                                     'path': texture, 'texture_scale': texture_scale,
                                                     'texture': self.texture(texture, palette),
                                                     'prim': prim if intensity else None, 'env': env if intensity else None,
                                                     'tile': tile, 'origin': origin, 'cull_back': bool(geom & 0x400)})
                    for word in (w0, w1) if op == 6 else (w0,):
                        ids = [((word >> 16) & 255) // 2, ((word >> 8) & 255) // 2, (word & 255) // 2]
                        start = len(group['pos'])
                        for i in ids:
                            position, normal, uv = cache[i]
                            group['pos'].append(position)
                            group['normal'].append(normal)
                            group['uv'].append(uv)
                        group['tri'].append([start, start + 1, start + 2])
                else:
                    assert op in (0x33, 0xD7, 0xE7, 0xFC, 0xFA, 0xE2, 0x3D, 0xE3, 0xE8, 0xE6, 0xF3, 0xF5, 0xF2, 0xDF), hex(op)
        for group in groups.values():
            for key in ('pos', 'normal', 'uv', 'tri'):
                group[key] = np.array(group[key])
            assert np.isfinite(group['pos']).all()
        return list(groups.values())


def wrap(coordinate, size, mode):
    if mode & 2:
        return np.clip(coordinate, 0, size - 1)
    if mode & 1:
        coordinate = coordinate % (2 * size)
        return np.where(coordinate >= size, 2 * size - 1 - coordinate, coordinate)
    return coordinate % size


def render(parts, size=(500, 620), yaw=12, scale=.067, center_y=4100):
    width, height = size
    canvas = np.full((height, width, 3), [22, 29, 42], dtype=np.uint8)
    depth = np.full((height, width), -np.inf)
    a = np.deg2rad(yaw)
    rotation = np.array([[np.cos(a), 0, -np.sin(a)], [0, 1, 0], [np.sin(a), 0, np.cos(a)]])
    for part in parts:
        pos = part['pos'] @ rotation.T
        normals = part['normal'] @ rotation.T
        xy = np.c_[width / 2 + pos[:, 0] * scale, height / 2 - (pos[:, 1] - center_y) * scale]
        tex = part['texture'].astype(float)
        if part['prim'] is not None:
            # Native umbrella's (PRIMITIVE - ENVIRONMENT) * TEXEL0 + ENVIRONMENT.
            t = tex[:, :, :1] / 255.0
            tex[:, :, :3] = np.array(part['env']) + (np.array(part['prim']) - np.array(part['env'])) * t
        th, tw = tex.shape[:2]
        tile = part['tile']
        cms, cmt = (tile >> 8) & 3, (tile >> 18) & 3
        shifts, shiftt = tile & 15, (tile >> 10) & 15
        uv = part['uv'].copy()
        for axis, shift in enumerate((shifts, shiftt)):
            uv[:, axis] *= 2. ** (-shift if shift <= 10 else 16 - shift)
            uv[:, axis] -= part['origin'][axis]
            # libultraship's linear sampler adds half a virtual texel before
            # normalization, including for HD textures backed by 32px tiles.
            uv[:, axis] += .5
        uv *= part.get('texture_scale', 1.0)
        for ids in part['tri']:
            pts, z = xy[ids], pos[ids, 2]
            lo = np.maximum(np.floor(pts.min(0)).astype(int), [0, 0])
            hi = np.minimum(np.ceil(pts.max(0)).astype(int), [width - 1, height - 1])
            if np.any(lo > hi):
                continue
            x0, y0 = pts[0]
            x1, y1 = pts[1]
            x2, y2 = pts[2]
            denominator = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
            if abs(denominator) < 1e-8 or (part['cull_back'] and denominator >= 0):
                continue
            xmin, ymin = lo
            xmax, ymax = hi
            gy, gx = np.mgrid[ymin:ymax + 1, xmin:xmax + 1]
            gx, gy = gx + .5, gy + .5
            wa = ((y1 - y2) * (gx - x2) + (x2 - x1) * (gy - y2)) / denominator
            wb = ((y2 - y0) * (gx - x2) + (x0 - x2) * (gy - y2)) / denominator
            wc = 1 - wa - wb
            zz = wa * z[0] + wb * z[1] + wc * z[2]
            region = depth[ymin:ymax + 1, xmin:xmax + 1]
            mask = (wa >= 0) & (wb >= 0) & (wc >= 0) & (zz >= region)
            if not mask.any():
                continue
            bary = np.stack([wa[mask], wb[mask], wc[mask]], axis=1)
            coords = bary @ uv[ids] - .5
            low = np.floor(coords).astype(int)
            fraction = coords - low
            lx, hx = wrap(low[:, 0], tw, cms), wrap(low[:, 0] + 1, tw, cms)
            ly, hy = wrap(low[:, 1], th, cmt), wrap(low[:, 1] + 1, th, cmt)
            fx, fy = fraction[:, :1], fraction[:, 1:]
            rgba = (tex[ly, lx] * (1 - fx) + tex[ly, hx] * fx) * (1 - fy) + (tex[hy, lx] * (1 - fx) + tex[hy, hx] * fx) * fy
            covered = rgba[:, 3] >= 128
            nn = bary @ normals[ids]
            nn /= np.maximum(np.linalg.norm(nn, axis=1, keepdims=True), 1e-5)
            shade = .72 + .28 * np.maximum(nn @ np.array([.25, .4, .88]), 0)
            target_y, target_x = np.where(mask)
            target_y, target_x = target_y[covered], target_x[covered]
            canvas[ymin:ymax + 1, xmin:xmax + 1][target_y, target_x] = np.clip(rgba[covered, :3] * shade[covered, None], 0, 255).astype(np.uint8)
            region[target_y, target_x] = zz[target_y, target_x]
    return Image.fromarray(canvas)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--animate', action='store_true')
    parser.add_argument('--standing', action='store_true', help='Preview the native standing idle candidate')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    model = Anju(args.archive, args.standing)
    clip = model.clips[model.animation]
    print('Native loop:', model.animation, len(clip), 'frames; root bounds', clip[:, 0].min(0), clip[:, 0].max(0), flush=True)
    parts = model.pose(model.animation)
    points = np.concatenate([p['pos'] for p in parts])
    print('Body + umbrella bounds:', points.min(0), points.max(0), flush=True)
    print('Triangle count:', sum(len(p['tri']) for p in parts), flush=True)
    frames = []
    font = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
    title = ImageFont.truetype(font, 24)
    note = ImageFont.truetype(font, 17)
    count = len(clip) if args.animate else 1
    for f in range(count):
        parts = model.pose(model.animation, f)
        sheet = Image.new('RGB', (1080, 730), (22, 29, 42))
        draw = ImageDraw.Draw(sheet)
        heading = 'ANJU  |  Standing umbrella idle  |  Candidate pose' if args.standing else 'ANJU  |  0x7E0D  |  Seated umbrella crying'
        draw.text((24, 16), heading, font=title, fill=(238, 242, 250))
        draw.text((24, 53), 'Native MM model and animation  /  Offline preview', font=note, fill=(174, 188, 208))
        for index, (yaw, label) in enumerate([(30, 'Front three-quarter'), (110, 'Side')]):
            scale, center = (.052, 4300) if args.standing else (.064, 2500)
            panel = render(parts, (540, 550), yaw=yaw, scale=scale, center_y=center)
            pdraw = ImageDraw.Draw(panel)
            origin_y = round(550 / 2 + center * scale)
            pdraw.line((18, origin_y, 522, origin_y), fill=(115, 137, 160), width=1)
            pdraw.ellipse((266, origin_y-4, 274, origin_y+4), fill=(230, 196, 121))
            pdraw.text((20, origin_y+9), 'Placed actor Y (origin)', font=note, fill=(190, 203, 220))
            sheet.paste(panel, (index*540, 115))
            draw.text((index*540+24, 89), label, font=note, fill=(230, 196, 121))
        detail = 'Native 32-frame standing idle with her umbrella attached to the right hand.' if args.standing else 'Align the seated body with your furniture; placed Y stays fixed through the animation.'
        footer = 'Additional pose preview for review; the seated crying entry stays separate.' if args.standing else 'No floor snap or automatic foot lift. Furniture is not included.'
        draw.text((24, 677), detail, font=note, fill=(210, 220, 234))
        draw.text((24, 704), footer, font=note, fill=(174, 188, 208))
        stem = 'Anju_Standing_Umbrella_Preview' if args.standing else 'Anju_Seated_Umbrella_Preview'
        if f == 0:
            sheet.save(args.output/(stem+'.png'))
        frames.append(sheet)
        if f % 10 == 0: print('Rendered frame', f, flush=True)
    if args.animate:
        frames[0].save(args.output/(stem+'.gif'), save_all=True, append_images=frames[1:], duration=50, loop=0, disposal=2)
    print(args.output, flush=True)

if __name__ == '__main__':
    main()
