#!/usr/bin/env python3
"""Animation bounds/dependencies and optional actual-donor compatibility tests."""

import argparse
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zipfile

SPEC = importlib.util.spec_from_file_location("young_epona_assets", Path(__file__).with_name("build_young_epona_assets.py"))
assets = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(assets)


def resource(kind, body, endian="<"):
    return bytes([endian == ">", 0, 0, 0]) + struct.pack(
        endian + "IIQ", kind, 0, 0xDEADBEEFDEADBEEF
    ) + bytes(44) + body


def normal(frames=2, joints=47, values=(0, 100, 101, 200, 201), index=(0, 1, 3), endian="<"):
    body = struct.pack(endian + "IhI", 0, frames, len(values))
    body += struct.pack(endian + "h" * len(values), *values)
    body += struct.pack(endian + "I", joints)
    body += struct.pack(endian + "HHH", *index) * joints
    return resource(assets.OANM, body + struct.pack(endian + "H", 1), endian)


def link(path, frames=38):
    encoded = path.encode("ascii")
    return resource(assets.OANM, struct.pack("<IhI", 1, frames, len(encoded)) + encoded)


def player(frames=38, stride=67):
    count = frames * stride
    return resource(assets.OPAM, struct.pack("<I", count) + bytes(count * 2))


def bundle():
    result = {}
    for _, symbol, frames in assets.HORSE_CLIPS:
        result[assets.ASSET_ROOT + symbol] = normal(frames=frames, values=tuple(range(200)), index=(0, 1, 100))
    for _, symbol in assets.MOUNT_CLIPS:
        result[assets.ASSET_ROOT + symbol] = link("__OTR__" + assets.ASSET_ROOT + symbol + "Data")
        result[assets.ASSET_ROOT + symbol + "Data"] = player()
    return result


class LoaderBoundsTests(unittest.TestCase):
    def test_path_crc64_uses_libultraship_nonfinalized_variant(self):
        self.assertEqual(assets.path_crc64(""), 0xFFFFFFFFFFFFFFFF)
        self.assertEqual(assets.path_crc64("123456789"), 0x9D13A61C0E5B0FF5)

    def test_last_dynamic_sample_is_in_bounds(self):
        self.assertEqual(assets.root_y(assets.parse_normal(normal())), [100, 101])

    def test_big_endian_resource_uses_declared_byte_order(self):
        self.assertEqual(assets.root_y(assets.parse_normal(normal(endian=">"))), [100, 101])

    def test_last_frame_overrun_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "frame data"):
            assets.parse_normal(normal(index=(0, 1, 4)))

    def test_wrong_joint_count_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "47 joint"):
            assets.parse_normal(normal(joints=46))

    def test_truncated_values_or_indices_are_rejected(self):
        for blob in (normal()[:78], normal()[:-3]):
            with self.subTest(size=len(blob)), self.assertRaises(ValueError):
                assets.parse_normal(blob)

    def test_zero_frames_and_trailing_data_are_rejected(self):
        for blob in (normal(frames=0), normal() + b"extra"):
            with self.subTest(size=len(blob)), self.assertRaises(ValueError):
                assets.parse_normal(blob)

    def test_unsupported_version_is_rejected(self):
        blob = bytearray(normal())
        struct.pack_into("<I", blob, 8, 1)
        with self.assertRaisesRegex(ValueError, "version"):
            assets.parse_normal(bytes(blob))

    def test_player_frame_stride_matches_soh(self):
        self.assertEqual(assets.parse_player(player(), 38, 22)["entries"], 2546)
        for blob in (player(stride=66), player(frames=37), player()[:-2], player() + b"\0\0"):
            with self.subTest(size=len(blob)), self.assertRaises(ValueError):
                assets.parse_player(blob, 38, 22)

    def test_mount_references_are_rewritten_and_resolved(self):
        original = link("__OTR__misc/link_animetion/gPlayerAnim_cl_uma_leftup_Data")
        target = "__OTR__" + assets.ASSET_ROOT + "gYoungEponaMountLeftAnimData"
        converted = assets.rewrite_link(original, target)
        self.assertEqual(assets.parse_link(converted)["data_path"], target)
        self.assertEqual(converted[:70], original[:70])
        assets.validate_bundle(bundle())

    def test_unconverted_or_missing_player_data_is_rejected(self):
        result = bundle()
        key = assets.ASSET_ROOT + "gYoungEponaMountLeftAnim"
        result[key] = link("__OTR__misc/link_animetion/gPlayerAnim_cl_uma_leftup_Data")
        with self.assertRaisesRegex(ValueError, "dependency"):
            assets.validate_bundle(result)
        result = bundle()
        del result[key + "Data"]
        with self.assertRaisesRegex(ValueError, "resource set"):
            assets.validate_bundle(result)

    def test_native_alt_or_metadata_collision_is_rejected(self):
        result = bundle()
        key = next(iter(result))
        for name in (key, "alt/" + key, key + ".meta", "alt/" + key + ".meta"):
            with self.subTest(path=name), self.assertRaisesRegex(ValueError, "collision"):
                assets.check_collisions(result, {"existing": [name]})
        result["alt/" + key] = result.pop(key)
        with self.assertRaisesRegex(ValueError, "resource set"):
            assets.validate_bundle(result)

    def test_archive_is_deterministic_and_crc_clean(self):
        with tempfile.TemporaryDirectory() as temporary:
            first, second = [Path(temporary) / x for x in ("one.o2r", "two.o2r")]
            assets.write_archive(first, bundle())
            assets.write_archive(second, bundle())
            self.assertEqual(first.read_bytes(), second.read_bytes())
            with zipfile.ZipFile(first) as archive:
                self.assertIsNone(archive.testzip())
                assets.validate_bundle({n: archive.read(n) for n in archive.namelist()})

    def test_paths_cannot_overwrite_inputs(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "mm.o2r"
            source.write_bytes(b"preserve me")
            alias = Path(temporary) / "alias.o2r"
            alias.symlink_to(source)
            with self.assertRaisesRegex(ValueError, "overwrite"):
                assets.check_output_paths([source], [alias])
            self.assertEqual(source.read_bytes(), b"preserve me")


class ActualArchiveTests(unittest.TestCase):
    def test_actual_donor_rig_animation_bounds_and_dependencies(self):
        if not getattr(sys, "young_epona_mm", None):
            self.skipTest("pass --mm and --oot for actual donor checks")
        with zipfile.ZipFile(sys.young_epona_mm) as mm, zipfile.ZipFile(sys.young_epona_oot) as oot:
            result, evidence = assets.build_bundle(mm, oot)
            self.assertEqual(len(assets.validate_bundle(result)), 8)
            self.assertEqual(evidence["horse_rig"]["limb_count"], 46)
            self.assertTrue(all(r["byte_identical"] for r in evidence["native_child_animations"]))
            self.assertTrue(all(r["identical_raw_y"] for r in evidence["jump_root_motion"]))
            collision_names = {"mm": mm.namelist(), "oot": oot.namelist()}
            if sys.young_epona_tp:
                with zipfile.ZipFile(sys.young_epona_tp) as tp:
                    collision_names["tp_adult_poc3"] = tp.namelist()
            assets.check_collisions(result, collision_names)
            with tempfile.TemporaryDirectory() as temporary:
                output = Path(temporary) / "restored.o2r"
                assets.write_archive(output, result)
                self.assertEqual(assets.file_sha256(output), assets.ACCEPTED_PACK_SHA256)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--mm", type=Path)
    parser.add_argument("--oot", type=Path)
    parser.add_argument("--tp", type=Path)
    args, remaining = parser.parse_known_args()
    if bool(args.mm) != bool(args.oot):
        parser.error("--mm and --oot must be supplied together")
    sys.young_epona_mm, sys.young_epona_oot, sys.young_epona_tp = args.mm, args.oot, args.tp
    unittest.main(argv=[sys.argv[0], *remaining])
