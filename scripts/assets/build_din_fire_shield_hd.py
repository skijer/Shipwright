"""Build the isolated 256px shield POC from immutable POC1 and full-size art.

Requires the named-resource texture bindings added after d54391b5. Whole-image
RGBA resources retain the hook's logical 64x32 flame / 32x32 icon coordinates.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile

from PIL import Image

ROOT = "objects/din_fire_shield/poc1/"
BASELINE_SHA = "a479e5a736b6b6ea02ea1a2f60808a80c18b0fc81dee5fe297b0a1942a445869"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def texture(original, source, flame, size):
    image = Image.open(source).convert("RGBA").resize((size, size), Image.Resampling.LANCZOS)
    if flame:
        # Encode I8's color/intensity-alpha semantics in the engine's RGBA
        # whole-image upload format. Reverse row order for positive-T motion.
        intensity = image.convert("L")
        if image.getchannel("A").getextrema() != (255, 255):
            from PIL import ImageChops
            intensity = ImageChops.multiply(intensity, image.getchannel("A"))
        image = Image.merge("RGBA", (intensity,) * 4).transpose(Image.Transpose.FLIP_TOP_BOTTOM)
    data = image.tobytes()
    header = bytearray(original[:64])
    struct.pack_into("<I", header, 8, 1)
    # IMG uploads the physical dimensions; RAW also makes previews interpret
    # the payload as RGBA. Unit scales preserve native UV and scroll periods.
    return bytes(header) + struct.pack("<IIIIffI", 6 if flame else 1, size, size, 3, 1., 1., len(data)) + data, image


def vertices(data, core):
    result = bytearray(data)
    count = struct.unpack_from("<I", data, 68)[0]
    for i in range(count):
        pos = 72 + i * 16
        x, y = struct.unpack_from("<hh", data, pos)
        struct.pack_into("<hh", result, pos + 8, round((y + 1000) * 1.024), round((x + 1000) * .512))
        if core:
            result[pos + 15] = {115: 215, 105: 215, 180: 205, 0: 0}[data[pos + 15]]
    return bytes(result)


def build(baseline, core, rim, icon, output, flame_size=256):
    assert sha(baseline.read_bytes()) == BASELINE_SHA, "Unexpected parent; preserve and inspect the actual baseline"
    with zipfile.ZipFile(baseline) as archive:
        assert archive.testzip() is None
        old = {name: archive.read(name) for name in archive.namelist()}
    files = dict(old)
    output.parent.mkdir(parents=True, exist_ok=True)
    sources = {}
    for name, source, flame, preview in (("FlowTex", core, True, f"Core_{flame_size}.png"),
                                        ("FlameTex", rim, True, f"Rim_{flame_size}.png"),
                                        ("IconTex", icon, False, "Icon_256.png")):
        files[ROOT + name], image = texture(old[ROOT + name], source, flame, flame_size if flame else 256)
        # User-facing maps have flame tips upward; archive rows follow UV T.
        if flame:
            image = image.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
        image.save(output.parent / preview)
        sources[name] = {"file": source.name, "sha256": sha(source.read_bytes())}
    for name in ("Surface", "Rim"):
        files[ROOT + name + "Vertices"] = vertices(old[ROOT + name + "Vertices"], name == "Surface")

    manifest = json.loads(old["DinFireShieldPOC1.json"])
    for key in ("source_texture", "source_texture_sha256", "source_flow", "source_flow_sha256"):
        manifest.pop(key, None)
    manifest.update({
        "candidate": f"Din Fire Shield SoH POC2 HD{flame_size}",
        "runtime_tested": False,
        "code_baseline": "d54391b55d8d8945cc4b00ffabae17d6677c333c",
        "requires_code_change": "Named OTR resource binding for shield flame textures and inventory icon",
        "new_build_required": True,
        "source_archive": baseline.name, "source_archive_sha256": BASELINE_SHA,
        "sources": sources, "icon_source_sha256": sources["IconTex"]["sha256"],
        "texture_contract": {"format": "I8 semantics in whole-image RGBA", "version": 1,
                             "width": flame_size, "height": flame_size, "flags": 3, "scales": [1, 1],
                             "logical_width": 64, "logical_height": 32, "logical_uv_units_per_texel": 32},
        "icon_contract": {"format": "RGBA32 whole-image", "version": 1, "width": 256, "height": 256,
                          "logical_width": 32, "logical_height": 32, "flags": 3, "scales": [1, 1],
                          "colors": "static default Din palette"},
        "uv_mapping": {"s": "round((hand_local_y + 1000) * 1.024)",
                       "t": "round((hand_local_x + 1000) * 0.512)",
                       "verdict": "local +X scrolling verified; held in-game direction awaits runtime test"},
        "preserved": ["all geometry positions, flags and RGB", "rim vertex alpha", "all display lists",
                      "get-item bracer", "attachment and size", "guard rules and cosmetics"],
    })
    manifest["requirements"][0] = "HD named-resource code hook (new build required)"
    manifest["resources"] = {name: sha(data) for name, data in files.items() if name != "DinFireShieldPOC1.json"}
    files["DinFireShieldPOC1.json"] = (json.dumps(manifest, indent=2) + "\n").encode()
    expected = {"DinFireShieldPOC1.json"} | {ROOT + name for name in
               ("FlowTex", "FlameTex", "IconTex", "SurfaceVertices", "RimVertices")}
    changed = {name for name in files if files[name] != old[name]}
    assert changed == expected
    for layer in ("Surface", "Rim"):
        before, after = old[ROOT + layer + "Vertices"], files[ROOT + layer + "Vertices"]
        for pos in range(72, len(before), 16):
            assert before[pos:pos+8] == after[pos:pos+8]
            assert before[pos+12:pos+15] == after[pos+12:pos+15]
            if layer == "Rim":
                assert before[pos+15] == after[pos+15]
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(name, (2026, 9, 25, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, data)
    report = {"archive": output.name, "sha256": sha(output.read_bytes()), "parent_sha256": BASELINE_SHA,
              "changed_entries": sorted(changed), "unchanged_entries": sorted(set(old) - changed),
              "geometry_positions_preserved": True, "rim_alpha_preserved": True,
              "new_build_required": True, "runtime_tested": False}
    print(json.dumps(report, indent=2))
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    for name in ("baseline", "core", "rim", "icon", "output"):
        parser.add_argument(name, type=Path)
    parser.add_argument("--flame-size", type=int, default=256, choices=(256, 512, 1024, 2048, 4096),
                        help="Physical flame resolution; logical UVs and game code stay unchanged")
    args = parser.parse_args()
    build(args.baseline, args.core, args.rim, args.icon, args.output, args.flame_size)
