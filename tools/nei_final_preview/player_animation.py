"""Source-frame player fitting sampler, deliberately separate from runtime proof.

Reads unmodified OOT player resources from the supplied archive and the shipped
67-s16 PlayerAnimation data. Bone and vertex transforms follow DrawFlexLod. The
renderer supplies studio light only; it does not run player state, IK, camera,
effects, or native animation selection. Missing native clips are never invented.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import re
import struct
import sys
import xml.etree.ElementTree as ET
import zipfile
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image
from scipy.spatial.transform import Rotation

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/diagnostics"))
from render_anju_preview import commands, crc64, read_texture, read_vertex_array, rgba16

LIMBS = ["ROOT", "WAIST", "LOWER", "R_THIGH", "R_SHIN", "R_FOOT",
         "L_THIGH", "L_SHIN", "L_FOOT", "UPPER", "HEAD", "HAT", "COLLAR",
         "L_SHOULDER", "L_FOREARM", "L_HAND", "R_SHOULDER", "R_FOREARM",
         "R_HAND", "SHEATH", "TORSO"]
ANIM_PATH = ROOT / "soh/assets/custom/misc/link_animetion/gPlayerAnim_nei_somaria"
SOURCE_PATH = ROOT / "soh/mods/items/anim/somaria_cane/somaria_anim_data.c"


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def translation(x=0, y=0, z=0):
    out = np.eye(4)
    out[:3, 3] = [x, y, z]
    return out


def scale(s):
    return np.diag([s, s, s, 1.])


def rotate(axis, degrees):
    out = np.eye(4)
    out[:3, :3] = Rotation.from_euler(axis, degrees, degrees=True).as_matrix()
    return out


def read_player_animation(path):
    data = Path(path).read_bytes()
    return parse_player_animation(data, str(path))


def parse_player_animation(data, label="resource"):
    if data[4:8] != b"MAPO":
        raise ValueError(f"Not a SOH_PlayerAnimation resource: {label}")
    count, = struct.unpack_from("<I", data, 64)
    if count % 67 or len(data) != 68 + count * 2:
        raise ValueError("PlayerAnimation must contain complete 67-s16 frames")
    raw = np.frombuffer(data, dtype="<i2", count=count, offset=68).reshape(-1, 67)
    return raw[:, :66].reshape(-1, 22, 3).copy(), raw[:, 66].copy()


class NativeAnimations:
    """External, user-provided base archive; only derived evidence is exported."""
    def __init__(self, path):
        self.path, self.used = Path(path), {}

    def load(self, name):
        path = "objects/gameplay_keep/gPlayerAnim_" + name
        with zipfile.ZipFile(self.path) as z:
            header = z.read(path)
            if header[4:8] != b"MNAO" or struct.unpack_from("<I", header, 64)[0] != 1:
                raise ValueError("Expected LinkAnimationHeader")
            frames, length = struct.unpack_from("<hI", header, 68)
            resource = header[74:74+length].decode().removeprefix("__OTR__")
            data = z.read(resource)
        clip, flags = parse_player_animation(data, resource)
        if len(clip) != frames: raise ValueError("Native header/payload frame count mismatch")
        self.used[name] = {"header": path, "header_sha256": hashlib.sha256(header).hexdigest(),
                           "resource": resource, "resource_sha256": hashlib.sha256(data).hexdigest(),
                           "frames": frames}
        return clip

    def manifest(self):
        return {"native_archive": str(self.path), "native_archive_sha256": sha256(self.path),
                "native_clips": self.used}


def verify_somaria_source(path=ANIM_PATH):
    clip, flags = read_player_animation(path)
    body = re.search(r"gSomariaAnimData\[\]\s*=\s*\{(.*?)\};", SOURCE_PATH.read_text(), re.S).group(1)
    source = np.array([int(n, 16) for n in re.findall(r"0x[0-9a-fA-F]+", body)], dtype=np.uint16).view(np.int16)
    packed = np.concatenate([clip.reshape(-1, 66), flags[:, None]], axis=1).ravel()
    if len(clip) != 60 or not np.array_equal(source, packed):
        raise ValueError("Shipped Somaria resource differs from its 60-frame C array")
    player = (ROOT / "soh/src/overlays/actors/ovl_player_actor/z_player.c").read_text()
    body = re.search(r"sUpperBodyLimbCopyMap\[PLAYER_LIMB_MAX\]\s*=\s*\{(.*?)\};", player, re.S).group(1)
    mask = np.array([b == "true" for b in re.findall(r"\b(true|false)\s*,?\s*//", body)])
    if len(mask) != 22 or not np.array_equal(mask, np.arange(22) >= 10):
        raise ValueError("Upper-body copy map changed; review sampler before rendering")
    registry = (ROOT / "soh/mods/items/anim/nei_anims.h").read_text()
    if '#define NEI_ANIM_SOMARIA NEI_ANIM_PATH("somaria")' not in registry:
        raise ValueError("Somaria animation registry changed")
    return clip, mask


def control_composite(clip, mask, base_frame=0):
    """Apply the actual copy mask to a SOURCE-derived, frozen lower-body control.

    This is not the game's missing native idle. Its provenance must be shown.
    """
    out = np.repeat(clip[base_frame:base_frame+1], len(clip), axis=0)
    out[:, mask] = clip[:, mask]
    return out


@dataclass
class Mesh:
    pos: np.ndarray
    normal: np.ndarray
    uv: np.ndarray
    rgba: np.ndarray
    bones: np.ndarray
    texture: np.ndarray | None
    cull: bool
    label: str
    transparent: bool = False
    texgen: bool = False


class PlayerArchive:
    def __init__(self, path, age="child"):
        self.path = Path(path)
        self.age = age
        self.prefix = "objects/object_link_child/" if age == "child" else "objects/object_link_boy/"
        self.name = "gLinkChild" if age == "child" else "gLinkAdult"
        with zipfile.ZipFile(path) as z:
            self.resources = {n: z.read(n) for n in z.namelist() if not n.endswith("/")}
        self.alias = {n: n for n in self.resources}
        self.alias.update({n[4:]: n for n in self.resources if n.startswith("alt/")})
        self.hashes = {crc64(n): self.alias[n] for n in self.alias}
        self.hashes.update({crc64(n): n for n in self.resources})
        skeleton_path = self.prefix+self.name+"Skel"
        b = self.get(skeleton_path)
        if b[4:8] != b"LKSO" or struct.unpack_from("<bbI", b, 64) != (1, 2, 21):
            raise ValueError("Expected a 21-limb OOT player flex LOD skeleton")
        p, self.limb_resources = 79, []
        while p < len(b):
            n, = struct.unpack_from("<I", b, p)
            p += 4
            self.limb_resources.append(b[p:p+n].decode()); p += n
        self.offsets, self.children, self.siblings, self.dlists = [], [], [], []
        for path in self.limb_resources:
            b = self.get(path)
            x, y, z, child, sibling = struct.unpack("<hhhBB", b[-8:])
            n, = struct.unpack_from("<I", b, 106)
            self.dlists.append(b[110:110+n].decode())
            self.offsets.append((x, y, z)); self.children.append(child); self.siblings.append(sibling)
        self.offsets = np.array(self.offsets, dtype=float)
        self.parents = np.full(21, -1, dtype=int)
        self.order = []
        def walk(i, parent):
            if i in self.order:
                raise ValueError("Cyclic skeleton")
            self.parents[i] = parent; self.order.append(i)
            if self.children[i] != 255: walk(self.children[i], i)
            if self.siblings[i] != 255: walk(self.siblings[i], parent)
        walk(0, -1)
        assert len(self.order) == 21
        self.slots = [i for i in self.order if self.dlists[i]]
        self.vertex_cache, self.texture_cache, self.command_cache = {}, {}, {}
        self.dynamic_segments = set()
        self.used = {self.alias[skeleton_path], *self.limb_resources}

    def get(self, path):
        return self.resources[self.alias[path]]

    def display_commands(self, path, active=()):
        path = self.alias[path]
        if path in active or len(active) > 32:
            raise ValueError("Cyclic display list: " + path)
        self.used.add(path)
        b = self.resources[path]
        if b.startswith(b"<"):
            for node in ET.fromstring(b):
                if node.tag in ("SetPrimColor", "SetEnvColor"):
                    rgba = [int(node.get(k, "255")) for k in "RGBA"]
                    yield (0xFA if node.tag == "SetPrimColor" else 0xFB), 0, sum(c << s for c, s in zip(rgba, (24, 16, 8, 0))), None
                elif node.tag != "EndDisplayList":
                    raise ValueError("Unsupported XML material op: " + node.tag)
            return
        if path not in self.command_cache:
            self.command_cache[path] = list(commands(b))
        for op, w0, w1, hashed in self.command_cache[path]:
            if op == 0x31:
                yield from self.display_commands(self.hashes[hashed], active + (path,))
                if (w0 >> 16) & 1: return
            elif op not in (0xDF, 0x33):
                yield op, w0, w1, hashed

    def texture(self, path, palette=None):
        key = (path, palette)
        if key not in self.texture_cache:
            self.used.add(path)
            kind, width, height, raw = read_texture(self.get(path))
            if kind == 1: rgba = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 4)
            elif kind == 2: rgba = rgba16(raw)
            elif kind in (3, 4):
                if palette is None: raise ValueError("CI texture without palette")
                colors = rgba16(read_texture(self.get(palette))[3])
                indices = np.frombuffer(raw, dtype=np.uint8)
                if kind == 3: indices = np.column_stack([indices >> 4, indices & 15]).ravel()
                rgba = colors[indices]
            elif kind in (5, 6):
                if kind == 5:
                    nibbles = np.frombuffer(raw, dtype=np.uint8)
                    gray = np.column_stack([nibbles >> 4, nibbles & 15]).ravel() * 17
                else: gray = np.frombuffer(raw, dtype=np.uint8)
                rgba = np.c_[gray, gray, gray, np.full_like(gray, 255)]
            else: raise ValueError((path, kind))
            self.texture_cache[key] = rgba.reshape(height, width, 4).copy()
        return self.texture_cache[key]

    def world(self, frame, actor_scale=.01, actor_position=(0, 0, 0), movement_flags=0):
        """OOT TranslateRotateZYX, joint[0] root, child root correction, flat-floor control."""
        if np.shape(frame) != (22, 3): raise ValueError("Expected 22 Vec3s joint entries")
        matrices = np.tile(np.eye(4), (21, 1, 1))
        matrices[:, :3, :3] = Rotation.from_euler("ZYX", frame[1:, ::-1] * (np.pi / 32768)).as_matrix()
        matrices[:, :3, 3] = self.offsets
        root = frame[0].astype(float).copy()
        if self.age == "child":
            if not (movement_flags & 4) or movement_flags & 1: root[[0, 2]] *= .64
            if not (movement_flags & 4) or movement_flags & 2: root[1] *= .64
        matrices[0, :3, 3] = root
        actor = translation(*actor_position) @ scale(actor_scale)
        for i in self.order:
            parent = self.parents[i]
            matrices[i] = (actor if parent < 0 else matrices[parent]) @ matrices[i]
        return matrices

    def geometry(self, hands="closed", include_sheath=True, only_limb=None):
        paths = list(self.dlists)
        if hands == "closed":
            paths[15] = self.prefix+self.name+("LeftFistNearDL" if self.age == "child" else "LeftHandClosedNearDL")
            paths[18] = self.prefix+self.name+"RightHandClosedNearDL"
        if not include_sheath: paths[19] = ""
        cache, groups = {}, {}
        geom, texture, image, palette, tile, origin = 0x230405, None, None, None, 0, (0, 0)
        prim, env, combiner = (255, 255, 255, 255), (255, 255, 255, 255), (0xFC127E03, 0xFFFFFDF8)
        for owner in self.order:
            if not paths[owner] or (only_limb is not None and owner != only_limb): continue
            bone = owner
            for op, w0, w1, hashed in self.display_commands(paths[owner]):
                if op == 0xDA:
                    if w1 >> 24 != 0x0D: raise ValueError("Matrix outside dynamic flex segment")
                    slot = (w1 & 0xFFFFFF) // 64
                    bone = self.slots[slot]
                elif op == 0xD9: geom = (geom & (w0 & 0xFFFFFF)) | w1
                elif op == 0x20: image = self.hashes[hashed]
                elif op in (0xFA, 0xFB):
                    rgba = tuple((w1 >> shift) & 255 for shift in (24, 16, 8, 0))
                    if op == 0xFA: prim = rgba
                    else: env = rgba
                elif op == 0xFC: combiner = (w0, w1)
                elif op == 0xF0: palette = image
                elif op in (0xF3, 0xF4): texture = image
                elif op == 0xF5 and (w1 >> 24) & 7 == 0: tile = w1
                elif op == 0xF2 and (w1 >> 24) & 7 == 0:
                    origin = (((w0 >> 12) & 4095) / 4, (w0 & 4095) / 4)
                elif op == 0x32:
                    path = self.hashes[hashed]; self.used.add(path)
                    if path not in self.vertex_cache: self.vertex_cache[path] = read_vertex_array(self.get(path))
                    verts = self.vertex_cache[path]
                    count, start = (w0 >> 12) & 255, ((w0 >> 1) & 127) - ((w0 >> 12) & 255)
                    if w1 % 16 or w1 // 16 + count > len(verts): raise ValueError("Invalid vertex load")
                    for k, v in enumerate(verts[w1 // 16:w1 // 16 + count]): cache[start + k] = (v, bone)
                elif op in (5, 6):
                    key = texture, palette, tile, origin, bool(geom & 0x400), prim, env, combiner, bool(geom & 0x40000)
                    group = groups.setdefault(key, [])
                    for word in ((w0, w1) if op == 6 else (w0,)):
                        for shift in (16, 8, 0): group.append(cache[((word >> shift) & 255) // 2])
                elif op == 0xDE:
                    # Archive's two hair material hooks call runtime segments 8/9.
                    # They contain no local geometry. Keep previous texture/material.
                    self.dynamic_segments.add(hex(w1))
                elif op not in (0xD7, 0xE7, 0xE2, 0x3D, 0xE3, 0xE8, 0xE6, 0xF3, 0xF4, 0xF5, 0xF2):
                    raise ValueError(f"Unsupported display command {op:#x} in {paths[owner]}")
        meshes = []
        for (texture, palette, tile, origin, cull, prim, env, combiner, texgen), vertices in groups.items():
            v = np.array([a for a, b in vertices], dtype=float)
            bones = np.array([b for a, b in vertices], dtype=int)
            normal = v[:, 6:9].copy(); normal[normal > 127] -= 256
            uv = v[:, 4:6] / 32
            tex = self.texture(texture, palette) if texture else np.ones((1, 1, 4), dtype=np.uint8) * 255
            for axis, shift in enumerate((tile & 15, (tile >> 10) & 15)):
                uv[:, axis] *= 2. ** (-shift if shift <= 10 else 16 - shift)
                uv[:, axis] -= origin[axis]
                uv[:, axis] += .5
            tex = material_texture(tex, prim, env, combiner)
            uv /= [tex.shape[1], tex.shape[0]]
            meshes.append(Mesh(v[:, :3], normal / 127, uv, np.ones((len(v), 4)), bones, tex, cull, texture or "solid", texgen=texgen))
        return meshes

    def manifest(self):
        return {"archive": str(self.path), "archive_sha256": sha256(self.path),
                "archive_version": "Young Din POC4 Equipment HairFix OOT; unmodified geometry",
                "limbs": [{"enum": i+1, "name": LIMBS[i], "parent_enum": int(self.parents[i])+1,
                           "translation": self.offsets[i].tolist()} for i in self.order],
                "flex_matrix_slot_to_player_limb": [i+1 for i in self.slots],
                "runtime_material_segments_approximated": sorted(self.dynamic_segments),
                "used_resource_sha256": {n: hashlib.sha256(self.resources[n]).hexdigest() for n in sorted(self.used)}}


def material_texture(texture, prim, env, combiner):
    """Evaluate color mux at shade=1; texture1/reflection is approximated by tex0.

    Geometry is exact. This intentionally is not a full RDP/cosmetic emulator.
    """
    t = texture.astype(float) / 255
    p, e = np.array(prim) / 255, np.array(env) / 255
    shape = t.shape[:2] + (3,)
    combined = np.zeros(shape)
    w0, w1 = combiner
    cycles = [(w0 >> 20 & 15, w1 >> 28 & 15, w0 >> 15 & 31, w1 >> 15 & 7),
              (w0 >> 5 & 15, w1 >> 24 & 15, w0 & 31, w1 >> 6 & 7)]
    for a, b, c, d in cycles:
        def rgb(n, slot):
            if n == 0: return combined
            if n in (1, 2): return t[:, :, :3]
            if n == 3: return p[:3]
            if n == 4: return 1.
            if n == 5: return e[:3]
            if slot == "c":
                if n in (8, 9): return t[:, :, 3:4]
                if n == 10: return p[3]
                if n == 11: return 1.
                if n == 12: return e[3]
                return 0.
            if n == 6: return 1.
            return 0.
        combined = np.clip((rgb(a, "a") - rgb(b, "b")) * rgb(c, "c") + rgb(d, "d"), 0, 1)
    out = np.empty_like(texture)
    out[:, :, :3] = np.rint(combined * 255)
    # The supplied body combine modes use opaque alpha, except eye overlays.
    out[:, :, 3] = texture[:, :, 3] if (w0, w1) == (0xFC12164A, 0xF0FFFE38) else 255
    return out


def transformed(mesh, matrices):
    m = matrices[mesh.bones]
    pos = np.einsum("nij,nj->ni", m[:, :3, :3], mesh.pos) + m[:, :3, 3]
    normals = np.einsum("nij,nj->ni", m[:, :3, :3], mesh.normal)
    normals /= np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-12)
    return pos, normals


def glb_meshes(path):
    """Load checkpoint author geometry under its real GLB scene node transforms."""
    data = Path(path).read_bytes(); n, = struct.unpack_from("<I", data, 12)
    doc, binary = json.loads(data[20:20+n]), data[28+n:]
    def acc(index):
        a = doc["accessors"][index]; v = doc["bufferViews"][a["bufferView"]]
        columns = {"VEC3": 3, "VEC2": 2, "VEC4": 4, "SCALAR": 1}[a["type"]]
        dtype = {5126: "<f4", 5125: "<u4", 5123: "<u2", 5121: "u1"}[a["componentType"]]
        return np.frombuffer(binary, dtype=dtype, count=a["count"]*columns,
                             offset=v.get("byteOffset", 0)+a.get("byteOffset", 0)).reshape(-1, columns)
    mats = []
    for mat in doc.get("materials", []):
        pbr = mat.get("pbrMetallicRoughness", {}); tex = None
        if "baseColorTexture" in pbr:
            image = doc["images"][doc["textures"][pbr["baseColorTexture"]["index"]]["source"]]
            v = doc["bufferViews"][image["bufferView"]]; start = v.get("byteOffset", 0)
            tex = np.array(Image.open(io.BytesIO(binary[start:start+v["byteLength"]])).convert("RGBA"))
        mats.append((pbr.get("baseColorFactor", [1, 1, 1, 1]), tex, mat.get("alphaMode") == "BLEND"))
    out = []
    def visit(index, parent):
        node = doc["nodes"][index]; m = parent.copy()
        if "matrix" in node: m = m @ np.array(node["matrix"]).reshape(4, 4).T
        else:
            m = m @ translation(*node.get("translation", [0, 0, 0]))
            if "rotation" in node:
                r = np.eye(4); r[:3, :3] = Rotation.from_quat(node["rotation"]).as_matrix(); m = m @ r
            m = m @ np.diag([*node.get("scale", [1, 1, 1]), 1])
        if "mesh" in node:
            for primitive in doc["meshes"][node["mesh"]]["primitives"]:
                a = primitive["attributes"]; indices = acc(primitive["indices"]).ravel().astype(int)
                pos = acc(a["POSITION"])[indices] @ m[:3, :3].T + m[:3, 3]
                normal = acc(a["NORMAL"])[indices] @ np.linalg.inv(m[:3, :3])
                uv = acc(a["TEXCOORD_0"])[indices] if "TEXCOORD_0" in a else np.zeros((len(pos), 2))
                base, tex, blend = mats[primitive.get("material", 0)]
                out.append(Mesh(pos, normal, uv, np.tile(base, (len(pos), 1)), np.zeros(len(pos), int), tex, True, str(path), blend))
        for child in node.get("children", []): visit(child, m)
    for index in doc["scenes"][doc.get("scene", 0)]["nodes"]: visit(index, np.eye(4))
    return out


def export_matrices(archive, clip, outpath, metadata):
    matrices = np.stack([archive.world(frame) for frame in clip])
    np.savez_compressed(outpath, world_matrices=matrices, joint_table=clip,
                        player_limb_enum=np.arange(1, 22), upper_body_mask=np.arange(22) >= 10,
                        metadata=json.dumps(metadata))
    return matrices


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("archive", type=Path); p.add_argument("output", type=Path)
    args = p.parse_args(); args.output.mkdir(parents=True, exist_ok=True)
    archive = PlayerArchive(args.archive); clip, mask = verify_somaria_source()
    meshes = archive.geometry(hands="closed", include_sheath=False)
    composite = control_composite(clip, mask)
    metadata = archive.manifest()
    metadata.update({"animation": str(ANIM_PATH.relative_to(ROOT)), "animation_sha256": sha256(ANIM_PATH),
                     "c_source_sha256": sha256(SOURCE_PATH), "source_frames": len(clip),
                     "native_spinner_animation": "Not loaded by this diagnostic; render_player_fit.py accepts --native",
                     "composite": "Source upper-body mask; frozen Somaria frame 0 lower-body CONTROL, not native idle",
                     "actor_scale": .01, "child_root_translation_scale": .64,
                     "movement_flags": 0, "head_tracking_and_ik": "not simulated",
                     "body_triangles": sum(len(m.pos)//3 for m in meshes),
                     "lighting": "offline approximation; not running game/camera"})
    export_matrices(archive, clip, args.output / "Somaria_full_source_limb_matrices.npz", metadata)
    export_matrices(archive, composite, args.output / "Somaria_upper_body_control_limb_matrices.npz", metadata)
    for name, index in [("left", 15), ("right", 18)]:
        hand = archive.geometry(hands="closed", only_limb=index)
        points = np.concatenate([m.pos for m in hand])
        metadata[name+"_fist_local_bounds_native"] = [points.min(0).tolist(), points.max(0).tolist()]
        metadata[name+"_fist_bone_enums"] = sorted({int(b)+1 for m in hand for b in m.bones})
    (args.output / "Sampler_source_inspection.json").write_text(json.dumps(metadata, indent=2)+"\n")
    print(json.dumps({k: v for k, v in metadata.items() if k not in ("limbs", "used_resource_sha256")}, indent=2))


if __name__ == "__main__": main()
