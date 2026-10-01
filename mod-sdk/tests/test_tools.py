import json
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

from PIL import Image

TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))

import otex  # noqa: E402


def run_tool(script, *arguments):
    return subprocess.run([sys.executable, str(TOOLS / script), *map(str, arguments)],
                          capture_output=True, text=True, check=False)


class PackModTests(unittest.TestCase):
    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory()
        self.addCleanup(self.workspace.cleanup)
        self.root = Path(self.workspace.name)
        self.mod = self.root / "probe"
        self.mod.mkdir()
        (self.mod / "manifest.json").write_text(json.dumps({"name": "probe"}), encoding="utf-8")
        self.binary = self.root / "probe.dll"
        self.binary.write_bytes(b"test native payload")
        self.output = self.root / "probe.o2r"

    def pack(self, platform="windows_x64", binary=None, output=None):
        return run_tool("pack_mod.py", "--mod-directory", self.mod, "--binary", binary or self.binary,
                        "--platform", platform, "--output", output or self.output)

    def test_assets_keep_their_path_and_binary_goes_under_its_platform(self):
        asset = self.mod / "assets" / "probe" / "icons" / "item"
        asset.parent.mkdir(parents=True)
        asset.write_bytes(b"test resource")
        result = self.pack()
        self.assertEqual(result.returncode, 0, result.stderr)
        with zipfile.ZipFile(self.output) as archive:
            self.assertEqual(set(archive.namelist()),
                             {"manifest.json", "bin/windows_x64/probe.dll", "probe/icons/item"})
            manifest = json.loads(archive.read("manifest.json"))
            self.assertEqual(manifest["binaries"], {"windows_x64": "bin/windows_x64/probe.dll"})

    def test_png_named_with_a_format_is_packaged_as_a_texture_resource(self):
        source = self.mod / "assets" / "textures" / "icon" / "gIconTex.rgba32.png"
        source.parent.mkdir(parents=True)
        Image.new("RGBA", (32, 32), (255, 0, 0, 255)).save(source)
        result = self.pack()
        self.assertEqual(result.returncode, 0, result.stderr)
        with zipfile.ZipFile(self.output) as archive:
            texture = archive.read("textures/icon/gIconTex")
        self.assertEqual(texture, otex.encode(source, "rgba32"))

    def test_png_with_an_unsupported_format_is_rejected(self):
        source = self.mod / "assets" / "gTex.ci4.png"
        source.parent.mkdir(parents=True)
        Image.new("RGBA", (8, 8)).save(source)
        self.assertNotEqual(self.pack().returncode, 0)
        self.assertFalse(self.output.exists())

    def test_plain_png_is_packaged_untouched(self):
        source = self.mod / "assets" / "preview.png"
        source.parent.mkdir(parents=True)
        Image.new("RGBA", (8, 8)).save(source)
        self.assertEqual(self.pack().returncode, 0)
        with zipfile.ZipFile(self.output) as archive:
            self.assertEqual(archive.read("preview.png"), source.read_bytes())

    def test_reserved_paths_are_rejected_before_writing(self):
        for reserved in ("manifest.json", "bin/windows_x64/probe.dll"):
            with self.subTest(reserved=reserved):
                asset = self.mod / "assets" / reserved
                asset.parent.mkdir(parents=True, exist_ok=True)
                asset.write_bytes(b"{}")
                self.assertNotEqual(self.pack().returncode, 0)
                self.assertFalse(self.output.exists())
                asset.unlink()

    def test_missing_binary_is_rejected_before_writing(self):
        self.binary.unlink()
        self.assertNotEqual(self.pack().returncode, 0)
        self.assertFalse(self.output.exists())

    def test_empty_mod_identity_is_rejected(self):
        (self.mod / "manifest.json").write_text('{"name": " "}', encoding="utf-8")
        self.assertNotEqual(self.pack().returncode, 0)
        self.assertFalse(self.output.exists())

    def test_platform_packages_merge_into_one_universal_package(self):
        linux_binary = self.root / "probe.so"
        linux_binary.write_bytes(b"linux payload")
        windows_package = self.root / "windows" / "probe.o2r"
        linux_package = self.root / "linux" / "probe.o2r"
        self.assertEqual(self.pack(output=windows_package).returncode, 0)
        self.assertEqual(self.pack("linux_x64", linux_binary, linux_package).returncode, 0)

        result = run_tool("merge_mod_packages.py", windows_package, linux_package, "--output", self.output)
        self.assertEqual(result.returncode, 0, result.stderr)
        with zipfile.ZipFile(self.output) as archive:
            manifest = json.loads(archive.read("manifest.json"))
            self.assertEqual(manifest["binaries"], {"windows_x64": "bin/windows_x64/probe.dll",
                                                    "linux_x64": "bin/linux_x64/probe.so"})
            self.assertEqual(archive.read("bin/linux_x64/probe.so"), b"linux payload")

    def test_merge_refuses_packages_of_different_manifests(self):
        other = self.root / "other.o2r"
        self.assertEqual(self.pack().returncode, 0)
        (self.mod / "manifest.json").write_text(json.dumps({"name": "other"}), encoding="utf-8")
        self.assertEqual(self.pack("linux_x64", output=other).returncode, 0)
        result = run_tool("merge_mod_packages.py", self.output, other, "--output", self.root / "merged.o2r")
        self.assertNotEqual(result.returncode, 0)


class OtexTests(unittest.TestCase):
    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory()
        self.addCleanup(self.workspace.cleanup)
        self.png = Path(self.workspace.name) / "icon.png"

    def test_rgba32_round_trip_keeps_every_pixel(self):
        image = Image.new("RGBA", (4, 2))
        image.putpixel((1, 0), (10, 20, 30, 40))
        image.save(self.png)
        data = otex.encode(self.png, "rgba32")
        self.assertEqual(data[4:8], b"XETO")
        self.assertEqual(len(data), 0x50 + 4 * 2 * 4)
        self.assertEqual(otex.decode(data).getpixel((1, 0)), (10, 20, 30, 40))

    def test_ia4_packs_two_pixels_per_byte_high_nibble_first(self):
        image = Image.new("LA", (2, 1))
        image.putpixel((0, 0), (255, 255))
        image.putpixel((1, 0), (0, 0))
        image.save(self.png)
        data = otex.encode(self.png, "ia4")
        self.assertEqual(data[0x50:], bytes([0xF0]))


if __name__ == "__main__":
    unittest.main()
