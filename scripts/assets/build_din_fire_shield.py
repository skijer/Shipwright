"""Build the isolated SoH Din fire-shield POC from the user's Fire POC7 pack.

The source pack is read-only. The output never overrides a shared texture,
player mesh, shield display list, or cosmetics manifest.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import zipfile

from PIL import Image

ROOT = "objects/din_fire_shield/poc1/"
SOURCE_TEXTURE = "alt/objects/gameplay_keep/gEffUnknown1Tex"
SOURCE_FLOW = "alt/objects/gameplay_keep/spin_reimagined/gSpinFireLevel2FlowTex"


def header(kind: int) -> bytes:
    data = bytearray(64)
    data[1] = 1
    struct.pack_into("<IIQ", data, 4, kind, 0, 0xDEADBEEFDEADBEEF)
    return bytes(data)


def crc64(path: str) -> int:
    value = 0xFFFFFFFFFFFFFFFF
    for byte in path.encode("ascii"):
        value ^= byte << 56
        for _ in range(8):
            value = ((value << 1) ^ (0x42F0E1EBA9EA3693 if value >> 63 else 0)) & 0xFFFFFFFFFFFFFFFF
    return value


def flame_texture(source: Path, resource: str) -> tuple[bytes, str]:
    with zipfile.ZipFile(source) as archive:
        raw = archive.read(resource)
    kind, width, height, flags = struct.unpack_from("<IIII", raw, 64)
    assert struct.unpack_from("<I", raw, 8)[0] == 1
    assert (kind, width, height, flags) == (6, 1024, 512, 3)
    assert len(raw) == 92 + width * height * 4
    # This source is grayscale stored as RGBA. Native I8 needs intensity bytes;
    # format conversion/downsampling here does not alter the source resource.
    image = Image.frombytes("RGBA", (width, height), raw[92:]).getchannel("R")
    intensity = image.resize((64, 32), Image.Resampling.LANCZOS).tobytes()
    return header(0x4F544558) + struct.pack("<IIII", 6, 64, 32, len(intensity)) + intensity, hashlib.sha256(raw).hexdigest()


def point(radius: float, angle: float, alpha: int, rim: bool) -> tuple[int, ...]:
    # The positive-Y edge has tapered tongues. Vertex alpha feathers their tips;
    # two scrolling samples create motion without spawning particle actors.
    extension = 1.0
    if rim and radius > 1.0:
        extension += 0.09 * max(0.0, math.sin(angle)) * (0.5 + 0.5 * math.sin(angle * 9.0))
    x = radius * math.cos(angle)
    y = radius * math.sin(angle) * extension
    z = -170.0 * max(0.0, 1.0 - min(radius, 1.0) ** 2) - (12.0 if rim else 0.0)
    return (round(x * 1000), round(y * 1000), round(z), 0,
            round((x + 1.0) * 32 * 32), round((1.0 - y) * 16 * 32),
            255, 255, 255, alpha)


def mesh(rim: bool) -> list[tuple[int, ...]]:
    segments = 32
    rings = [(0.72, 0), (0.93, 240), (1.10, 0)] if rim else [(0.0, 115), (0.58, 105), (0.90, 180), (1.01, 0)]
    output = []
    for ring in range(len(rings) - 1):
        for i in range(segments):
            a = math.tau * i / segments
            b = math.tau * (i + 1) / segments
            p = point(rings[ring][0], a, rings[ring][1], rim)
            q = point(rings[ring + 1][0], a, rings[ring + 1][1], rim)
            r = point(rings[ring + 1][0], b, rings[ring + 1][1], rim)
            s = point(rings[ring][0], b, rings[ring][1], rim)
            output.extend((p, q, r))
            if rings[ring][0] > 0:
                output.extend((p, r, s))
    return output


def resources(name: str, vertices: list[tuple[int, ...]]) -> dict[str, bytes]:
    vertex_path = ROOT + name + "Vertices"
    array = header(0x4F415252) + struct.pack("<II", 0x19, len(vertices))
    array += b"".join(struct.pack("<hhhHhhBBBB", *v) for v in vertices)
    commands = bytearray(header(0x4F444C54) + b"\x04" + b"\xff" * 7)
    hashed = crc64(vertex_path)
    for start in range(0, len(vertices), 30):
        count = min(30, len(vertices) - start)
        commands += struct.pack("<IIII", 0x32000000 | count << 12 | count << 1,
                                start * 16, hashed >> 32, hashed & 0xFFFFFFFF)
        for i in range(0, count, 3):
            commands += struct.pack("<II", 0x05000000 | (i * 2) << 16 | ((i + 1) * 2) << 8 | (i + 2) * 2, 0)
    commands += struct.pack("<II", 0xDF000000, 0)
    return {vertex_path: array, ROOT + name + "DL": bytes(commands)}


def get_item_bracer(source: Path) -> tuple[list[tuple[int, ...]], dict]:
    """Reframe actual child-bracer vertices as a freestanding collectible.

    The source cuff/gem/flame plate are preserved, without source display lists,
    player body parts, or the sheen overlay. Bake light into vertex RGB so this
    small GI resource needs no material or scene-light dependencies.
    """
    vertices = []
    with zipfile.ZipFile(source) as archive:
        info = json.loads(archive.read("DinEquipmentInfo.json"))
        for part in info["geometry"]:
            if part["group"] != "Bracer" or part["coat"]:
                continue
            data = archive.read(part["vertices"])
            count = struct.unpack_from("<I", data, 68)[0]
            assert len(data) == 72 + 16 * count
            color = info["controls"][part["role"]]
            for i in range(count):
                x, y, z, flag, s, t, nx, ny, nz, alpha = struct.unpack_from("<hhhHhhbbbB", data, 72 + i * 16)
                # Right-handed reorientation: original cuff axis X becomes -Y,
                # outward face -Y becomes +Z, and original Z becomes X.
                pos = (round((z - 38) * .19), round((427 - x) * .19 - 25), round(-(y + 32) * .19))
                normal = (nz / 127, -nx / 127, -ny / 127)
                diffuse = max(0, sum(a*b for a, b in zip(normal, (-.35, .5, .79))))
                rgb = tuple(min(255, round(c * (.56 + .44 * diffuse))) for c in color)
                vertices.append((*pos, 0, 0, 0, *rgb, 255))
    assert vertices and len(vertices) % 3 == 0
    return vertices, {"archive": source.name, "sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                      "note": "Existing bracer geometry; GI-only reorientation and baked default colors"}


def icon_texture(source: Path) -> bytes:
    icon = Image.open(source).convert("RGBA").resize((32, 32), Image.Resampling.LANCZOS)
    pixels = icon.tobytes()
    low, high = icon.getchannel("A").getextrema()
    assert low == 0 and high > 240
    return header(0x4F544558) + struct.pack("<IIII", 1, 32, 32, len(pixels)) + pixels


def build(source: Path, destination: Path, bracer: Path, icon: Path) -> dict:
    texture, texture_hash = flame_texture(source, SOURCE_TEXTURE)
    flow, flow_hash = flame_texture(source, SOURCE_FLOW)
    files = {ROOT + "FlameTex": texture, ROOT + "FlowTex": flow}
    files.update(resources("Surface", mesh(False)))
    files.update(resources("Rim", mesh(True)))
    gi_vertices, gi_source = get_item_bracer(bracer)
    files.update(resources("GIBracer", gi_vertices))
    files[ROOT + "IconTex"] = icon_texture(icon)
    manifest = {
        "candidate": "Din Fire Shield SoH POC1", "runtime_tested": False,
        "code_baseline": "e1c78f8603d5bdb0e8f90516d86a5d095de3c15b",
        "source_archive": source.name, "source_archive_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "source_texture": SOURCE_TEXTURE, "source_texture_sha256": texture_hash,
        "source_flow": SOURCE_FLOW, "source_flow_sha256": flow_hash,
        "texture_contract": {"format": "I8", "width": 64, "height": 32, "logical_uv_units_per_texel": 32},
        "geometry": {"surface_triangles": len(mesh(False)) // 3, "rim_triangles": len(mesh(True)) // 3,
                     "gibracer_triangles": len(gi_vertices) // 3},
        "get_item_source": gi_source,
        "icon_source_sha256": hashlib.sha256(icon.read_bytes()).hexdigest(),
        "icon_contract": {"format": "RGBA32", "width": 32, "height": 32, "colors": "static default Din palette"},
        "cosmetic_editor": {"group": "Effects / Magic Effects",
                            "core": "gCosmetics.Custom.DinFireShieldCore.Value",
                            "outer": "gCosmetics.Custom.DinFireShieldOuter.Value",
                            "scope": "Held shield and animated get-item flame"},
        "resources": {path: hashlib.sha256(data).hexdigest() for path, data in files.items()},
        "requirements": ["POC1 code hook", "gEnhancements.DinFireShield=1", "Alternate Assets enabled",
                         "Existing matching Din bracer pack", "Child Deku shield or adult Hylian shield equipped"],
        "overrides": [], "normal_blocking": True, "added_damage": False,
    }
    files["DinFireShieldPOC1.json"] = json.dumps(manifest, indent=2).encode()
    destination.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, content in sorted(files.items()):
            info = zipfile.ZipInfo(name, date_time=(2026, 9, 25, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, content)
    return manifest


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--bracer", type=Path, required=True)
    parser.add_argument("--icon", type=Path, required=True)
    args = parser.parse_args()
    result = build(args.source, args.output, args.bracer, args.icon)
    print(json.dumps({"archive": str(args.output), "geometry": result["geometry"], "resources": len(result["resources"])}))


if __name__ == "__main__":
    main()
