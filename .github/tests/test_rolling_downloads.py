import importlib.util
import tempfile
import unittest
import zipfile
from pathlib import Path

spec = importlib.util.spec_from_file_location("package_downloads", Path(__file__).parents[1] / "scripts/package_downloads.py")
packaging = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packaging)


class RollingDownloadsTest(unittest.TestCase):
    def CreateBuilds(self, artifacts, prefix):
        for unsafe in (False, True):
            name = prefix + ("-unbound-unsafe" if unsafe else "")
            mac = "SoH" if prefix == "soh" else "2Ship"
            suffix = "-unbound-unsafe" if unsafe else ""
            files = {"windows": f"{prefix}.exe", "mac": f"{mac}{suffix}.dmg", "linux": f"{prefix}{suffix}.appimage"}
            for platform, filename in files.items():
                folder = artifacts / f"{name}-{platform}"
                folder.mkdir(parents=True)
                (folder / filename).write_bytes(f"{name}-{platform}".encode())
        (artifacts / f"{prefix}-windows" / f"{prefix}.lib").write_bytes(b"import library")

    def test_both_projects_produce_complete_stable_downloads(self):
        for prefix in ("soh", "2ship"):
            with self.subTest(prefix=prefix), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                artifacts = root / "artifacts"
                self.CreateBuilds(artifacts, prefix)
                metadata = {"project": prefix, "repository": "owner/game", "commit": "a" * 40, "run_id": 123}
                packaging.PackageDownloads(artifacts, root / "out", metadata)
                for edition, tag in (("public", "nei-unbound-release"), ("modders", "nei-unbound-modders")):
                    folder = root / "out" / edition
                    name = prefix if edition == "public" else prefix + "-unbound-unsafe"
                    notes = (folder / "notes.md").read_text()
                    for platform in ("windows", "mac", "linux"):
                        filename = f"{name}-{platform}.zip"
                        self.assertIn(f"releases/download/{tag}/{filename}", notes)
                        with zipfile.ZipFile(folder / filename) as archive:
                            self.assertIsNone(archive.testzip())
                            if platform == "linux":
                                info = archive.infolist()[0]
                                self.assertEqual((info.external_attr >> 16) & 0o777, 0o755)
                    self.assertIn("a" * 12, notes)
                self.assertTrue((root / "out/modders/unbound-mod-sdk-windows.zip").is_file())
                self.assertFalse((root / "out/public/unbound-mod-sdk-windows.zip").exists())

    def test_missing_platform_prevents_partial_publication(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.CreateBuilds(root / "artifacts", "soh")
            (root / "artifacts/soh-unbound-unsafe-linux/soh-unbound-unsafe.appimage").unlink()
            with self.assertRaisesRegex(ValueError, "Missing build"):
                packaging.PackageDownloads(root / "artifacts", root / "out", {"project": "soh"})
            self.assertFalse((root / "out").exists())


if __name__ == "__main__":
    unittest.main()
