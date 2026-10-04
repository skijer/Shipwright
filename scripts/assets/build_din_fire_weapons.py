"""Add private child/adult fire-sword layers to the verified HD shield archive.

Original equipment and every shield resource remain byte-for-byte unchanged.
The hilt, grip, gems and stowed sword continue using the active equipment pack.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import zipfile

from build_din_fire_shield import header, crc64
from build_din_fire_shield_hd import texture

ROOT = "objects/din_fire_sword/poc1/"
SOURCES = {
    "child": ("objects/object_link_child/DinSleekEquipmentPOC1_OOT_Child/",
              "7d3f7619d49f9f2cd977bc3ba21f21d7d35115534e426ccf4a53756118ef76f6"),
    "adult": ("objects/object_link_boy/DinSleekEquipmentPOC1_OOT_Adult/",
              "ce1b8bfbfe9e1b2cf4a51355ea62c16826f60e8a65f07945d33a628b724cc781"),
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def mesh_resources(path, records):
    assert len(records) % 3 == 0
    vertices = path + "Vertices"
    array = header(0x4F415252) + struct.pack("<II", 0x19, len(records))
    array += b"".join(struct.pack("<hhhHhhBBBB", *v) for v in records)
    dl = bytearray(header(0x4F444C54) + b"\x04" + b"\xff" * 7)
    hashed = crc64(vertices)
    for start in range(0, len(records), 30):
        count = min(30, len(records) - start)
        dl += struct.pack("<IIII", 0x32000000 | count << 12 | count << 1,
                          start * 16, hashed >> 32, hashed & 0xFFFFFFFF)
        for i in range(0, count, 3):
            dl += struct.pack("<II", 0x05000000 | (i*2) << 16 | ((i+1)*2) << 8 | (i+2)*2, 0)
    dl += struct.pack("<II", 0xDF000000, 0)
    return {vertices: array, path + "DL": bytes(dl)}


def read_vertices(data):
    assert struct.unpack_from("<I", data, 4)[0] == 0x4F415252
    kind, count = struct.unpack_from("<II", data, 64)
    assert kind == 0x19 and len(data) == 72 + count * 16 and count % 3 == 0
    return [struct.unpack_from("<hhhHhhBBBB", data, 72 + i*16) for i in range(count)]


def sword_meshes(records):
    low = min(v[0] for v in records)
    high = max(v[0] for v in records)
    length = high - low
    min_y, max_y = min(v[1] for v in records), max(v[1] for v in records)
    # The baseline's sheen is displaced 1.5 local units. Move the private
    # opaque hot layer 4 units along authored normals to sit outside it.
    core = []
    for v in records:
        normal = [c if c < 128 else c - 256 for c in v[6:9]]
        norm = math.sqrt(sum(c*c for c in normal))
        assert norm > 0
        p = [round(v[i] + 4 * normal[i] / norm) for i in range(3)]
        core.append((*p, 0, round((v[1]-min_y)/(max_y-min_y)*2048),
                     round((v[0]-low)/length*1024), 255, 255, 255, 255))

    def cross_section(x):
        points = []
        for start in range(0, len(records), 3):
            tri = records[start:start+3]
            for a, b in ((tri[0], tri[1]), (tri[1], tri[2]), (tri[2], tri[0])):
                if a[0] == b[0]:
                    if abs(x-a[0]) < 1e-6:
                        points.extend((a[1:3], b[1:3]))
                elif min(a[0], b[0]) <= x <= max(a[0], b[0]):
                    t = (x-a[0])/(b[0]-a[0])
                    points.append((a[1]+t*(b[1]-a[1]), a[2]+t*(b[2]-a[2])))
        assert points, x
        ys, zs = zip(*points)
        return (min(ys)+max(ys))/2, (min(zs)+max(zs))/2, (max(ys)-min(ys))/2, (max(zs)-min(zs))/2

    rings = []
    sections, sides = 16, 12
    for i in range(sections+1):
        t = i/sections
        cy, cz, ry, rz = cross_section(low + length*t)
        envelope = max(0, math.sin(math.pi*t)) ** .35
        ry = ry*1.7 + length*.025*envelope
        rz = max(rz*2.0, ry*.20) + length*.015*envelope
        x = low + length*t + length*.07*t**3
        alpha = round(210 * min(1, t/.10) * min(1, (1-t)/.10))
        ring = []
        for j in range(sides+1):
            angle = 2*math.pi*j/sides
            ring.append((round(x), round(cy+ry*math.cos(angle)), round(cz+rz*math.sin(angle)),
                         0, round(j/sides*2048), round(t*1024), 255, 255, 255, alpha))
        rings.append(ring)
    flames = []
    for i in range(sections):
        for j in range(sides):
            a, b, c, d = rings[i][j], rings[i][j+1], rings[i+1][j], rings[i+1][j+1]
            flames.extend((a, b, c, b, d, c))
    return core, flames


def build(shield, child, adult, core_source, flame_source, output, size=1024):
    with zipfile.ZipFile(shield) as archive:
        assert archive.testzip() is None
        original = {n: archive.read(n) for n in archive.namelist()}
    assert all(n.startswith("objects/din_fire_shield/poc1/") or n == "DinFireShieldPOC1.json" for n in original)
    files = dict(original)
    mesh_info, sources = {}, {}
    for age, pack in (("child", child), ("adult", adult)):
        prefix, expected = SOURCES[age]
        assert sha(pack.read_bytes()) == expected, f"Unexpected {age} equipment baseline"
        with zipfile.ZipFile(pack) as archive:
            source = prefix + "Vertices/Sword_4"
            records = read_vertices(archive.read(source))
            core, flames = sword_meshes(records)
            files.update(mesh_resources(ROOT + age + "/Core", core))
            files.update(mesh_resources(ROOT + age + "/Flame", flames))
            mesh_info[age] = {"core_triangles": len(core)//3, "flame_triangles": len(flames)//3,
                              "source_vertices": source, "source_vertices_sha256": sha(archive.read(source)),
                              "core_normal_offset": 4, "tip_flame_extension_fraction": .07}
        sources[age] = {"archive": pack.name, "sha256": expected, "required_resource": prefix + "SwordDL"}
    # Explicit vanilla-sized fallback for long/broken swords. The known child
    # and adult silhouettes above are fitted directly to their packed vertices.
    adult_records = records
    for profile, low, high, width in (("bgs", 900, 5500, 700), ("broken", 900, 2500, 700)):
        x0, x1 = min(v[0] for v in adult_records), max(v[0] for v in adult_records)
        y0, y1 = min(v[1] for v in adult_records), max(v[1] for v in adult_records)
        fitted = [(*[round(low+(v[0]-x0)/(x1-x0)*(high-low)),
                     round((v[1]-(y0+y1)/2)/(y1-y0)*width+270), v[2]], *v[3:]) for v in adult_records]
        core, flames = sword_meshes(fitted)
        files.update(mesh_resources(ROOT + profile + "/Core", core))
        files.update(mesh_resources(ROOT + profile + "/Flame", flames))
        mesh_info[profile] = {"fit": "approximate vanilla proportions; no arbitrary runtime geometry detection",
                              "blade_x": [low, high], "width": width,
                              "core_triangles": len(core)//3, "flame_triangles": len(flames)//3}
    texture_header = original["objects/din_fire_shield/poc1/FlowTex"]
    for name, source in (("CoreTex", core_source), ("FlameTex", flame_source)):
        files[ROOT + name], _ = texture(texture_header, source, True, size)
    added = {n: data for n, data in files.items() if n not in original}
    assert len(added) == 18 and all(n.startswith(ROOT) for n in added)
    assert all(files[n] == data for n, data in original.items())
    manifest = {
        "candidate": "Din Fire Weapons POC1 — HD sword and shield",
        "runtime_tested": False, "new_build_required": True,
        "parent_code": "918c32539d0e5cf1edec7eaf9662db6516ae3762",
        "shield_archive": shield.name, "shield_archive_sha256": sha(shield.read_bytes()),
        "shield_resources_preserved": {n: sha(data) for n, data in original.items()},
        "equipment_sources": sources, "geometry": mesh_info,
        "texture_contract": {"version": 1, "physical_size": [size, size], "logical_size": [64, 32],
                             "flags": 3, "scales": [1, 1], "format": "RGBA with I8 intensity-alpha semantics"},
        "texture_sources": {"core": {"file": core_source.name, "sha256": sha(core_source.read_bytes())},
                            "flame": {"file": flame_source.name, "sha256": sha(flame_source.read_bytes())}},
        "activation": "Default-off Din Fire Sword; active whenever the age-matching sword or full/broken BGS is drawn",
        "audio": "No sword audio added; Shield SFX remains charging-only and guard-only",
        "preserved": ["original sword/hilt/grip/gems", "stowed equipment", "normal reach; normal damage with Fire Damage off",
                      "every HD256 shield resource", "other weapon owners and ceremonies"],
        "resources": {n: sha(data) for n, data in added.items()}, "overrides": [],
    }
    files["DinFireWeaponsPOC1.json"] = (json.dumps(manifest, indent=2, ensure_ascii=False) + "\n").encode()
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(name, (2026, 9, 25, 0, 0, 0)); info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, data)
    print(json.dumps({"archive": str(output), "sha256": sha(output.read_bytes()), "sword_resources": len(added),
                      "sword_texture_size": size, "shield_bytes_preserved": True, "runtime_tested": False}, indent=2))


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    for name in ("shield", "child", "adult", "core_source", "flame_source", "output"):
        p.add_argument(name, type=Path)
    p.add_argument("--size", type=int, default=1024, choices=(256, 512, 1024, 2048, 4096))
    a = p.parse_args()
    build(a.shield, a.child, a.adult, a.core_source, a.flame_source, a.output, a.size)
