#!/usr/bin/env python3
"""Reproducibly package the eight approved MM young Epona resources for SoH.

The source donors are checksum-pinned and opened read-only. OANM/OPAM v0,
Normal=0, Link=1, the 64-byte resource header, and signed length-prefixed paths
match the inspected SoH/2Ship AnimationFactory and PlayerAnimationFactory loaders.
Normal data and OPAM data are unchanged; only two Link dependency paths change.
Bounds follow SkelAnime_GetFrameData and AnimationContext_SetLoadFrame. This is
binary/source-contract verification, not game runtime or fence-clearance proof.
"""

import argparse
from contextlib import ExitStack
import hashlib
import json
from pathlib import Path
import struct
import zipfile
import zlib

OANM, OPAM, OSKL, OSLB = 0x4F414E4D, 0x4F50414D, 0x4F534B4C, 0x4F534C42
ASSET_ROOT = "objects/object_horse_link_child/rideable/"
HORSE_ROOT = "objects/object_horse_link_child/"
HORSE_CLIPS = (
    ("object_horse_link_child_Anim_005F64", "gYoungEponaStopAnim", 65),
    ("object_horse_link_child_Anim_004DE8", "gYoungEponaRearAnim", 33),
    ("object_horse_link_child_Anim_0035B0", "gYoungEponaLowJumpAnim", 23),
    ("object_horse_link_child_Anim_003D38", "gYoungEponaHighJumpAnim", 30),
)
MOUNT_CLIPS = (
    ("gPlayerAnim_cl_uma_leftup", "gYoungEponaMountLeftAnim"),
    ("gPlayerAnim_cl_uma_rightup", "gYoungEponaMountRightAnim"),
)
NATIVE_CLIPS = (
    ("gEponaIdleAnim", "gChildEponaIdleAnim"),
    ("gEponaWhinnyAnim", "gChildEponaWhinnyAnim"),
    ("gEponaWalkAnim", "gChildEponaWalkingAnim"),
    ("gEponaTrotAnim", "gChildEponaTrottingAnim"),
    ("gEponaGallopAnim", "gChildEponaGallopingAnim"),
)
EXPECTED_SHA256 = {
    "mm": "f10167e5682d74cc8da0137c524b1521f4e7ff47438b8888b63db6c7c6dc8d59",
    "oot": "ea80d61b223ee38075fe5383b652998f3b08164e903134922f63f205d995717b",
}
ACCEPTED_PACK_SHA256 = "d1990518522c4b781a9b62f3b2df21e9aec5df0eae99542892093359ecab945d"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def file_sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


class Reader:
    def __init__(self, data, kind):
        require(len(data) >= 64, "truncated resource header")
        require(data[0] in (0, 1), "unsupported resource byte order")
        self.data, self.endian, self.offset = data, "<" if data[0] == 0 else ">", 64
        actual, version = struct.unpack_from(self.endian + "II", data, 4)
        require(actual == kind, f"resource type {actual:08x}, expected {kind:08x}")
        require(version == 0, f"unsupported resource version {version}")

    def read(self, count):
        require(0 <= count <= len(self.data) - self.offset, "truncated resource payload")
        result = self.data[self.offset:self.offset + count]
        self.offset += count
        return result

    def unpack(self, fmt):
        return struct.unpack(self.endian + fmt, self.read(struct.calcsize(self.endian + fmt)))

    def number(self, fmt):
        return self.unpack(fmt)[0]

    def string(self):
        size = self.number("i")
        require(size >= 0, "negative string length")
        result = self.read(size).decode("ascii")
        require("\0" not in result, "embedded NUL in resource path")
        return result

    def finish(self):
        require(self.offset == len(self.data), "trailing resource data")


def parse_normal(data, joint_count=47):
    reader = Reader(data, OANM)
    require(reader.number("I") == 0, "expected Normal animation subtype 0")
    frames = reader.number("h")
    require(frames > 0, "animation must contain positive frames")
    values_count = reader.number("I")
    raw = reader.read(values_count * 2)
    values = struct.unpack(reader.endian + "h" * values_count, raw)
    count = reader.number("I")
    require(count == joint_count, f"animation needs {joint_count} joint entries (root plus skeleton)")
    indices = [reader.unpack("HHH") for _ in range(count)]
    static_max = reader.number("H")
    reader.finish()
    require(static_max <= values_count, "static index exceeds frame data")
    for joint, triple in enumerate(indices):
        for index in triple:
            last = index + (frames - 1 if index >= static_max else 0)
            require(last < values_count, f"joint {joint} indexes outside frame data at final frame")
    return {"frames": frames, "values": values, "indices": indices, "static_max": static_max}


def root_y(animation):
    index = animation["indices"][0][1]
    return [animation["values"][index + (f if index >= animation["static_max"] else 0)]
            for f in range(animation["frames"])]


def parse_link(data):
    reader = Reader(data, OANM)
    require(reader.number("I") == 1, "expected Link animation subtype 1")
    frames = reader.number("h")
    require(frames > 0, "Link animation must contain positive frames")
    path = reader.string()
    require(path.startswith("__OTR__"), "expected named Link data dependency")
    reader.finish()
    return {"frames": frames, "data_path": path}


def rewrite_link(data, path):
    require(parse_link(data)["frames"] == 38, "child mount must contain 38 frames")
    require(path.startswith("__OTR__" + ASSET_ROOT), "Link dependency must use young Epona namespace")
    encoded = path.encode("ascii")
    return data[:70] + struct.pack(("<" if data[0] == 0 else ">") + "i", len(encoded)) + encoded


def parse_player(data, frames, limb_count):
    reader = Reader(data, OPAM)
    entries = reader.number("I")
    stride = limb_count * 3 + 1  # sizeof(Vec3s) * limbCount + 2 bytes per frame.
    require(entries == frames * stride, f"player frame stride requires {frames * stride} s16 entries")
    reader.read(entries * 2)
    reader.finish()
    return {"entries": entries, "frames": frames, "frame_bytes": stride * 2, "joint_vectors": limb_count}


def parse_skeleton(data):
    reader = Reader(data, OSKL)
    kind, limb_type, count, display_lists, table_type, table_count = reader.unpack("BBIIBI")
    require(kind in (0, 1) and 0 < count <= 255 and count == table_count, "unsupported skeleton layout")
    limbs = [reader.string() for _ in range(table_count)]
    reader.finish()
    require(len(set(limbs)) == count, "duplicate skeleton limb dependency")
    return {"type": kind, "limb_type": limb_type, "limb_count": count, "limbs": limbs,
            "display_lists": display_lists, "table_type": table_type}


def parse_limb(data):
    reader = Reader(data, OSLB)
    reader.unpack("BB")
    reader.string()
    reader.number("H")
    modifications = reader.number("I")
    require(modifications <= len(data) // 10, "invalid skin modification count")
    for _ in range(modifications):
        reader.number("H")
        reader.read(reader.number("i") * 10)
        reader.read(reader.number("i") * 8)
    reader.string()
    reader.unpack("fffHHH")
    for _ in range(4):
        reader.string()
    transform = reader.unpack("hhhBB")
    reader.finish()
    return transform


def load_rig(archive, path):
    skeleton = parse_skeleton(archive.read(path))
    transforms = [parse_limb(archive.read(name)) for name in skeleton["limbs"]]
    for transform in transforms:
        require(all(i == 255 or i < skeleton["limb_count"] for i in transform[3:]),
                "limb hierarchy index is out of bounds")
    return skeleton, transforms


def build_bundle(mm, oot):
    mm_rig, mm_transforms = load_rig(mm, HORSE_ROOT + "gEponaSkel")
    oot_rig, oot_transforms = load_rig(oot, HORSE_ROOT + "gChildEponaSkel")
    require(mm_rig["limb_count"] == oot_rig["limb_count"] == 46, "horse rig must have 46 limbs")
    require(mm_transforms == oot_transforms, "MM/OoT young horse transforms or hierarchy differ")
    mm_player, mm_pt = load_rig(mm, "objects/object_link_child/gLinkHumanSkel")
    oot_player, oot_pt = load_rig(oot, "objects/object_link_child/gLinkChildSkel")
    require(mm_player["limb_count"] == oot_player["limb_count"] == 21, "player rig must have 21 limbs")
    require([x[3:] for x in mm_pt] == [x[3:] for x in oot_pt], "MM/OoT child player limb ordering differs")
    evidence = {"horse_rig": {"limb_count": 46, "joint_entries": 47, "transforms_and_hierarchy_equal": True,
                              "canonical_transform_sha256": sha256(json.dumps(mm_transforms).encode()),
                              "mm_limb_type": mm_rig["limb_type"], "oot_limb_type": oot_rig["limb_type"]},
                "player_rig": {"limb_count": 21, "frame_joint_vectors": 22, "hierarchy_equal": True,
                               "frame_stride_bytes": 134}, "native_child_animations": [], "jump_root_motion": []}
    for mm_name, oot_name in NATIVE_CLIPS:
        source, target = mm.read(HORSE_ROOT + mm_name), oot.read(HORSE_ROOT + oot_name)
        parse_normal(source)
        parse_normal(target)
        require(source == target, f"native child animation differs: {mm_name}")
        evidence["native_child_animations"].append({"mm": HORSE_ROOT + mm_name, "oot": HORSE_ROOT + oot_name,
                                                    "sha256": sha256(source), "byte_identical": True})
    result = {}
    for donor, symbol, frames in HORSE_CLIPS:
        blob = mm.read(HORSE_ROOT + donor)
        require(parse_normal(blob)["frames"] == frames, f"unexpected frame count for {donor}")
        result[ASSET_ROOT + symbol] = blob
    for donor, symbol in MOUNT_CLIPS:
        header = mm.read("objects/gameplay_keep/" + donor)
        parsed = parse_link(header)
        require(parsed["data_path"] == "__OTR__misc/link_animetion/" + donor + "_Data", "unexpected mount data dependency")
        data = mm.read(parsed["data_path"][7:])
        parse_player(data, parsed["frames"], 22)
        result[ASSET_ROOT + symbol] = rewrite_link(header, "__OTR__" + ASSET_ROOT + symbol + "Data")
        result[ASSET_ROOT + symbol + "Data"] = data
    adult_rig = parse_skeleton(oot.read("objects/object_horse/gEponaSkel"))
    for young_symbol, adult_symbol in (("gYoungEponaLowJumpAnim", "gEponaJumpingAnim"),
                                        ("gYoungEponaHighJumpAnim", "gEponaJumpingHighAnim")):
        young_y = root_y(parse_normal(result[ASSET_ROOT + young_symbol]))
        adult_path = "objects/object_horse/" + adult_symbol
        adult_y = root_y(parse_normal(oot.read(adult_path), adult_rig["limb_count"] + 1))
        evidence["jump_root_motion"].append({
            "young": ASSET_ROOT + young_symbol, "adult": adult_path,
            "young_joint_entries": 47, "adult_joint_entries": adult_rig["limb_count"] + 1,
            "young_raw_y": young_y, "adult_raw_y": adult_y, "identical_raw_y": young_y == adult_y,
            "young_y_at_render_scale_0_00648": [round(y * .00648, 6) for y in young_y],
            "adult_y_at_scale_0_01": [round(y * .01, 6) for y in adult_y],
            "young_peak_at_render_scale": round(max(young_y) * .00648, 6),
            "adult_peak_at_0_01": round(max(adult_y) * .01, 6),
        })
    validate_bundle(result)
    return result, evidence


def exported_symbols():
    return [item[1] for item in HORSE_CLIPS] + [s for _, symbol in MOUNT_CLIPS for s in (symbol, symbol + "Data")]


def crc_table():
    table = []
    for byte in range(256):
        crc = byte << 56
        for _ in range(8):
            crc = ((crc << 1) ^ (0x42F0E1EBA9EA3693 if crc & (1 << 63) else 0)) & 0xFFFFFFFFFFFFFFFF
        table.append(crc)
    return table


CRC_TABLE = crc_table()


def path_crc64(path):
    # libultraship CRC64(string): all-ones seed, ECMA polynomial, no final xor.
    crc = 0xFFFFFFFFFFFFFFFF
    for byte in path.encode("utf-8"):
        crc = CRC_TABLE[(crc >> 56) ^ byte] ^ ((crc << 8) & 0xFFFFFFFFFFFFFFFF)
    return crc


def validate_bundle(bundle):
    require(set(bundle) == {ASSET_ROOT + s for s in exported_symbols()},
            "output resource set must contain exactly eight unique normal-path resources")
    report = {}
    for _, symbol, frames in HORSE_CLIPS:
        path = ASSET_ROOT + symbol
        parsed = parse_normal(bundle[path])
        require(parsed["frames"] == frames, f"wrong frame count for {symbol}")
        report[path] = {"resource_type": "OANM", "subtype": "Normal", "frames": frames,
                        "joint_entries": len(parsed["indices"]), "rotation_values": len(parsed["values"]),
                        "static_index_max": parsed["static_max"], "dependencies": []}
    for _, symbol in MOUNT_CLIPS:
        path, data_path = ASSET_ROOT + symbol, ASSET_ROOT + symbol + "Data"
        parsed = parse_link(bundle[path])
        require(parsed["frames"] == 38, "child mount needs 38 frames")
        require(parsed["data_path"] == "__OTR__" + data_path, "unresolved or external mount data dependency")
        report[path] = {"resource_type": "OANM", "subtype": "Link", "frames": 38, "dependencies": [data_path]}
        report[data_path] = {"resource_type": "OPAM", "dependencies": [], **parse_player(bundle[data_path], 38, 22)}
    for path, item in report.items():
        item.update({"bytes": len(bundle[path]), "sha256": sha256(bundle[path]),
                     "zip_crc32": f"{zlib.crc32(bundle[path]):08x}", "path_crc64": f"{path_crc64(path):016x}"})
    return report


def check_collisions(bundle, archive_names):
    targets = {path_crc64(path): path for path in bundle}
    require(len(targets) == len(bundle), "output resource path CRC64 collision")
    for label, names in archive_names.items():
        for name in names:
            canonical = name[4:] if name.startswith("alt/") else name
            canonical = canonical[:-5] if canonical.endswith(".meta") else canonical
            require(canonical not in bundle and path_crc64(canonical) not in targets,
                    f"resource collision with {label}: {name}")


def check_output_paths(inputs, outputs):
    resolved_inputs = {Path(path).resolve() for path in inputs}
    resolved_outputs = [Path(path).resolve() for path in outputs]
    require(len(set(resolved_outputs)) == len(resolved_outputs), "output paths must be distinct")
    for output in outputs:
        output = Path(output)
        require(output.resolve() not in resolved_inputs, f"output would overwrite input: {output}")
        for source in inputs:
            require(not (output.exists() and output.samefile(source)), f"output would overwrite input: {output}")


def write_archive(path, bundle):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name in sorted(bundle):
            info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            info.create_system = 3
            info.external_attr = 0o100644 << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, bundle[name], compresslevel=9)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mm", required=True, type=Path)
    parser.add_argument("--oot", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--collision-archive", type=Path, action="append", default=[])
    args = parser.parse_args()
    args.manifest = args.manifest or args.output.with_suffix(".manifest.json")
    inputs = [args.mm, args.oot, *args.collision_archive]
    check_output_paths(inputs, [args.output, args.manifest])
    before = {str(p): file_sha256(p) for p in inputs}
    for label in ("mm", "oot"):
        require(before[str(getattr(args, label))] == EXPECTED_SHA256[label],
                f"{label} checksum differs from the verified donor; inspect and repin before using another revision")
    with ExitStack() as stack:
        archives = [stack.enter_context(zipfile.ZipFile(p)) for p in inputs]
        integrity = []
        for path, archive in zip(inputs, archives):
            require(len(archive.namelist()) == len(set(archive.namelist())), f"duplicate ZIP paths: {path}")
            require(archive.testzip() is None, f"ZIP CRC failure: {path}")
            integrity.append({"file": path.name, "sha256": before[str(path)], "bytes": path.stat().st_size,
                              "resources": len(archive.namelist()), "all_zip_crcs_valid": True})
        bundle, evidence = build_bundle(archives[0], archives[1])
        check_collisions(bundle, {str(p): z.namelist() for p, z in zip(inputs, archives)})
        source_paths = {ASSET_ROOT + symbol: HORSE_ROOT + donor for donor, symbol, _ in HORSE_CLIPS}
        for donor, symbol in MOUNT_CLIPS:
            source_paths[ASSET_ROOT + symbol] = "objects/gameplay_keep/" + donor
            source_paths[ASSET_ROOT + symbol + "Data"] = "misc/link_animetion/" + donor + "_Data"
        resources = validate_bundle(bundle)
        for path, item in resources.items():
            source = archives[0].read(source_paths[path])
            item.update({"mm_source": source_paths[path], "source_sha256": sha256(source),
                         "source_zip_crc32": f"{zlib.crc32(source):08x}", "byte_identical_to_source": bundle[path] == source})
    write_archive(args.output, bundle)
    with zipfile.ZipFile(args.output) as archive:
        require(archive.testzip() is None, "output ZIP CRC failure")
        require(validate_bundle({n: archive.read(n) for n in archive.namelist()}) == validate_bundle(bundle),
                "written output differs from validated resources")
    require({str(p): file_sha256(p) for p in inputs} == before, "an input archive changed during packaging")
    manifest = {
        "schema": 1, "name": "Young Epona SoH POC1 assets", "namespace": ASSET_ROOT,
        "archive": {"file": args.output.name, "sha256": file_sha256(args.output),
                    "bytes": args.output.stat().st_size, "resources": len(bundle), "all_zip_crcs_valid": True},
        "inputs": integrity, "inputs_unchanged": True, "collisions": [],
        "collision_checks": "Native and alt paths, .meta sidecars, and libultraship path CRC64 against listed inputs",
        "format": {"header_bytes": 64, "versions": {"OANM": 0, "OPAM": 0}, "normal_subtype": 0, "link_subtype": 1,
                   "conversion": "Only two Link dependency paths/lengths rewritten; four horse and two OPAM resources unchanged"},
        "resources": resources, "compatibility": evidence,
        "limits": ["Source-conforming binary validation; this builder does not run the game.",
                   "Uses native OoT gChildEponaSkel and five native child horse animations.",
                   "Rider alignment, mount sides, timing, fences, and alt-assets behavior require runtime acceptance.",
                   "Matching raw jump Y does not prove clearance; .00648 displacement is 64.8% of adult .01 displacement."],
    }
    manifest["matches_pre_interruption_pack"] = manifest["archive"]["sha256"] == ACCEPTED_PACK_SHA256
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"archive": str(args.output), "manifest": str(args.manifest),
                      "matches_pre_interruption_pack": manifest["matches_pre_interruption_pack"], **manifest["archive"]}, indent=2))


if __name__ == "__main__":
    main()
