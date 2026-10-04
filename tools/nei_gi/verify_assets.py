"""Independently compare serialized GI resources with the preserved GLB checkpoints."""
from collections import Counter
import argparse
import json
import math
from pathlib import Path
import struct
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / "soh/assets/custom"
CHECKPOINTS = Path(__file__).resolve().parent / "CHECKPOINTS"
PREFIX = "objects/nei_gi_redesign/"
ITEMS = ("fire_rod", "ice_rod", "light_rod", "rocs_feather", "time_gate", "whip", "shovel",
         "gust_jar", "hylia_grace", "zonai_permafrost", "demise_destruction", "ball_and_chain",
         "deku_leaf", "mogma_mitts", "switch_hook", "beetle", "lantern",
         "spinner", "cane_of_somaria", "minish_cap", "rocs_cape")


def face_key(points):
    points = tuple(tuple(p) for p in points)
    return min(points[i:] + points[:i] for i in range(3))


def read_glb(path):
    data = path.read_bytes()
    assert struct.unpack_from("<4sII", data) == (b"glTF", 2, len(data))
    n, tag = struct.unpack_from("<I4s", data, 12)
    assert tag == b"JSON"
    doc = json.loads(data[20:20+n])
    size, tag = struct.unpack_from("<I4s", data, 20+n)
    assert tag == b"BIN\0"
    binary = data[28+n:]
    assert size == len(binary)
    for view in doc["bufferViews"]:
        assert view["byteLength"] > 0
        assert view.get("byteOffset", 0) + view["byteLength"] <= len(binary)

    def acc(index):
        a = doc["accessors"][index]
        view = doc["bufferViews"][a["bufferView"]]
        columns = {"SCALAR": 1, "VEC2": 2, "VEC3": 3}[a["type"]]
        fmt = "<" + {5126: "f", 5125: "I"}[a["componentType"]] * columns
        offset = view.get("byteOffset", 0) + a.get("byteOffset", 0)
        stride = struct.calcsize(fmt)
        return [struct.unpack_from(fmt, binary, offset + i * stride) for i in range(a["count"])]

    faces = {"opa": Counter(), "xlu": Counter()}
    for mesh in doc["meshes"]:
        for primitive in mesh["primitives"]:
            pos = acc(primitive["attributes"]["POSITION"])
            assert all(math.isfinite(v) and v == round(v) for p in pos for v in p)
            indices = [i[0] for i in acc(primitive["indices"])]
            assert len(indices) % 3 == 0 and max(indices) < len(pos)
            mat = doc["materials"][primitive["material"]]
            render_pass = "xlu" if mat.get("alphaMode") == "BLEND" else "opa"
            for i in range(0, len(indices), 3):
                faces[render_pass][face_key([pos[j] for j in indices[i:i+3]])] += 1
    return doc, faces


def verify(items=ITEMS, namespace=PREFIX, checkpoints=CHECKPOINTS):
    report = {}
    for slug in items:
        prefix = namespace + slug + "/"
        meta = json.loads((checkpoints / slug / "checkpoint.json").read_text())
        doc, expected = read_glb(checkpoints / slug / (slug + ".glb"))
        matrix = (ASSETS / prefix / "scale_mtx").read_bytes()
        assert len(matrix) == 128 and struct.unpack_from("<I", matrix, 4)[0] == 0x4F4D5458
        words = struct.unpack_from("<16I", matrix, 64)
        fixed = []
        for i in range(8):
            for shift in (16, 0):
                bits = (((words[i] >> shift) & 65535) << 16) | ((words[i+8] >> shift) & 65535)
                fixed.append((bits - (1 << 32) if bits >= 1 << 31 else bits) / 65536)
        want = [meta["matrix_scale"] if i in (0, 5, 10) else (1 if i == 15 else 0) for i in range(16)]
        assert fixed == want and doc["nodes"][-1]["scale"] == [meta["matrix_scale"]] * 3
        total, loads = 0, 0
        for render_pass in ("opa", "xlu"):
            if not expected[render_pass]:
                continue
            entry = "gi_dl" if render_pass == "opa" else "gi_xlu_dl"
            dl = ET.parse(ASSETS / prefix / entry).getroot()
            assert dl.tag == "DisplayList" and dl[-1].tag == "EndDisplayList"
            vertices = ET.parse(ASSETS / prefix / ("mesh_" + render_pass + "_vtx")).getroot()
            records = []
            for v in vertices:
                values = {k: int(v.get(k)) for k in ("X", "Y", "Z", "S", "T", "R", "G", "B", "A")}
                assert all(-32768 <= values[k] <= 32767 for k in ("X", "Y", "Z", "S", "T"))
                assert all(0 <= values[k] <= 255 for k in ("R", "G", "B", "A"))
                records.append(tuple(values[k] for k in ("X", "Y", "Z")))
            cache, faces, depth = {}, Counter(), 0
            current_alpha = 255
            for cmd in dl:
                if "Path" in cmd.attrib:
                    assert cmd.get("Path").startswith(prefix), (slug, "unrelated resource")
                    assert (ASSETS / cmd.get("Path")).is_file(), (slug, "missing resource", cmd.attrib)
                if cmd.tag == "Matrix":
                    assert cmd.get("Param") == "G_MTX_PUSH"
                    depth += 1
                elif cmd.tag == "PopMatrix":
                    depth -= 1
                    assert depth >= 0
                elif cmd.tag == "SetPrimColor":
                    current_alpha = int(cmd.get("A"))
                    if slug == "demise_destruction" and render_pass == "opa" and total == 0:
                        assert all(int(cmd.get(c)) == 0 for c in ("R", "G", "B"))
                elif cmd.tag == "LoadVertices":
                    offset, count, start = (int(cmd.get(k)) for k in ("VertexOffset", "Count", "VertexBufferIndex"))
                    assert 0 < count <= 32 and 0 <= start and start + count <= 32
                    assert 0 <= offset and offset + count <= len(records)
                    cache = {start+i: records[offset+i] for i in range(count)}
                    loads += 1
                elif cmd.tag in ("Triangle1", "Triangles2"):
                    assert (current_alpha < 255) == (render_pass == "xlu")
                    fields = [("V00", "V01", "V02")]
                    if cmd.tag == "Triangles2":
                        fields.append(("V10", "V11", "V12"))
                    for fields_one in fields:
                        p = [cache[int(cmd.get(k))] for k in fields_one]
                        a = [p[1][i]-p[0][i] for i in range(3)]
                        b = [p[2][i]-p[0][i] for i in range(3)]
                        cross = [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]
                        assert any(cross), (slug, "degenerate triangle")
                        faces[face_key(p)] += 1
                        total += 1
                elif cmd.tag == "LoadTextureBlock":
                    texture = (ASSETS / cmd.get("Path")).read_bytes()
                    kind, w, h, flags, sx, sy, size = struct.unpack_from("<IIIIffI", texture, 64)
                    assert kind == 1 and flags == 1 and sx == w / 32 and sy == h / 32
                    assert len(texture) == 92 + size and size == w * h * 4
                    assert cmd.get("Width") == "32" and cmd.get("Height") == "32"
            assert depth == 0, (slug, "matrix imbalance")
            assert faces == expected[render_pass], (slug, render_pass, "checkpoint geometry/winding mismatch")
        assert total == meta["triangles"] and loads == meta["vertex_loads"]
        report[slug] = {"triangles": total, "vertex_loads": loads, "passes": 2 if expected["xlu"] else 1}
    return report


def verify_archive(path):
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        base = {p.relative_to(ASSETS).as_posix() for p in (ASSETS / PREFIX).rglob("*") if p.is_file()}
        assert len(names) == len(set(names)) == len(base) * 2
        assert set(names) == base | {"alt/" + n for n in base}
        for name in base:
            assert archive.read(name) == archive.read("alt/" + name) == (ASSETS / name).read_bytes()
    return len(names)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    result = {"models": verify(), "runtime_tested": False}
    if args.archive:
        result["archive_entries"] = verify_archive(args.archive)
    if args.report:
        args.report.write_text(json.dumps(result, indent=2) + "\n")
    print(f"PASS: {len(ITEMS)} serialized GI models; geometry/winding, vertex cache, references, matrices, textures, and transparency")
    if args.archive:
        print("PASS: combined archive matches source, with identical base/Alt resources")
