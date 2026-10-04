"""Read the delivered bytes independently; reject unsafe references/geometry."""
from pathlib import Path
import hashlib
import json
import struct
import sys
import zipfile


def hash_path(path):
    value = (1 << 64) - 1
    for byte in path.encode("ascii"):
        value ^= byte << 56
        for _ in range(8):
            high = value & (1 << 63)
            value = (value << 1) & ((1 << 64) - 1)
            if high:
                value ^= 0x42F0E1EBA9EA3693
    return value


def texture_pixels(tex, kind, native_size):
    resource, version = struct.unpack_from("<II", tex, 4)
    assert resource == 0x4F544558
    if version == 0:
        width, height = native_size
        size = width * height * (4 if kind == 1 else 1)
        assert struct.unpack_from("<IIII", tex, 64) == (kind, width, height, size)
        assert len(tex) == 80 + size
        return tex[80:], 4 if kind == 1 else 1, (width, height)
    assert version == 1
    fmt, width, height, flags, hscale, vscale, size = struct.unpack_from("<IIIIffI", tex, 64)
    assert fmt == kind and flags == 3 and hscale == vscale == 1
    assert width > 0 and height > 0 and size == width * height * 4
    assert len(tex) == 92 + size
    pixels = tex[92:]
    if kind == 6:
        assert pixels[0::4] == pixels[1::4] == pixels[2::4] == pixels[3::4]
    return pixels, 4, (width, height)


def validate(path):
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
        assert len(archive.namelist()) == len(set(archive.namelist()))
        files = {name: archive.read(name) for name in archive.namelist()}
    root = "objects/din_fire_shield/poc1/"
    wanted = {root + name for name in ("SurfaceDL", "SurfaceVertices", "RimDL", "RimVertices", "FlameTex", "FlowTex",
                                      "GIBracerDL", "GIBracerVertices", "IconTex")}
    assert set(files) == wanted | {"DinFireShieldPOC1.json"}
    manifest = json.loads(files["DinFireShieldPOC1.json"])
    assert not manifest["overrides"]
    for name in wanted:
        assert manifest["resources"][name] == hashlib.sha256(files[name]).hexdigest()
    dimensions = {}
    for name in ("FlameTex", "FlowTex"):
        pixels, channels, dimensions[name] = texture_pixels(files[root + name], 6, (64, 32))
        assert max(pixels[::channels]) > 180
        if name == "FlameTex":
            assert min(pixels[::channels]) == 0
        assert dimensions[name] == (manifest["texture_contract"]["width"], manifest["texture_contract"]["height"])
    pixels, channels, dimensions["IconTex"] = texture_pixels(files[root + "IconTex"], 1, (32, 32))
    assert min(pixels[3::4]) == 0 and max(pixels[3::4]) == 255
    assert dimensions["IconTex"] == (manifest["icon_contract"]["width"], manifest["icon_contract"]["height"])
    hashes = {hash_path(name): name for name in files}
    total = 0
    for layer in ("Surface", "Rim", "GIBracer"):
        vertices = files[root + layer + "Vertices"]
        assert struct.unpack_from("<I", vertices, 4)[0] == 0x4F415252
        kind, count = struct.unpack_from("<II", vertices, 64)
        assert kind == 0x19 and len(vertices) == 72 + count * 16
        records = [struct.unpack_from("<hhhHhhBBBB", vertices, 72 + i * 16) for i in range(count)]
        if layer == "GIBracer":
            assert all(all(-60 <= p <= 60 for p in v[:3]) and v[9] == 255 for v in records)
        else:
            assert all(-1500 <= v[0] <= 1500 and -1500 <= v[1] <= 1500 and -200 <= v[2] <= 0 for v in records)
            assert min(v[9] for v in records) == 0 and max(v[9] for v in records) > 100
        data = files[root + layer + "DL"]
        assert struct.unpack_from("<I", data, 4)[0] == 0x4F444C54 and data[64] == 4
        pc = 72
        loaded = 0
        triangles = 0
        ended = False
        while pc < len(data):
            a, b = struct.unpack_from("<II", data, pc)
            op = a >> 24
            pc += 8
            if op == 0x32:
                hi, lo = struct.unpack_from("<II", data, pc); pc += 8
                assert hashes[(hi << 32) | lo] == root + layer + "Vertices"
                loaded = (a >> 12) & 255
                assert 0 < loaded <= 30 and b % 16 == 0 and b + loaded * 16 <= count * 16
                assert ((a & 0xFF) >> 1) == loaded
            elif op == 0x05:
                indices = [(a >> shift) & 255 for shift in (16, 8, 0)]
                assert all(i % 2 == 0 and i // 2 < loaded for i in indices)
                triangles += 1
            elif op == 0xDF:
                assert pc == len(data)
                ended = True
            else:
                raise AssertionError(f"unexpected material/matrix/segment opcode {op:02x}")
        assert ended and triangles * 3 == count
        assert triangles == manifest["geometry"][layer.lower() + "_triangles"]
        total += triangles
    result = {"archive": str(path), "sha256": hashlib.sha256(Path(path).read_bytes()).hexdigest(),
              "private_resources": len(wanted), "triangles": total, "texture_dimensions": dimensions,
              "runtime_tested": False}
    print(json.dumps(result, indent=2))
    return result


if __name__ == "__main__":
    validate(Path(sys.argv[1]))
