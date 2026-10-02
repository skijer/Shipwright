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

import check_mod_binary  # noqa: E402
import mod_signature  # noqa: E402
import otex  # noqa: E402
import scan_mod_sources  # noqa: E402
import sign_queue  # noqa: E402
import sign_request  # noqa: E402


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


def has_module(name):
    try:
        __import__(name)
        return True
    except ImportError:
        return False


SIGNED_FIELDS = {
    "repository": "someone/their-mods",
    "commit": "0123456789abcdef0123456789abcdef01234567",
    "author": "someone",
    "workflow_run": "https://github.com/skijer/Shipwright/actions/runs/42",
}


@unittest.skipUnless(has_module("cryptography"), "needs the cryptography package")
class ModSignatureTests(unittest.TestCase):
    def setUp(self):
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

        self.workspace = tempfile.TemporaryDirectory()
        self.addCleanup(self.workspace.cleanup)
        self.root = Path(self.workspace.name)
        self.package = self.root / "probe.o2r"
        self.signed = self.root / "signed" / "probe.o2r"
        self.write_package({"manifest.json": b'{"name": "probe"}', "bin/windows_x64/probe.dll": b"payload",
                            "probe/icon": b"pixels"})
        self.key = Ed25519PrivateKey.generate()
        self.public_key = self.key.public_key().public_bytes_raw()

    def write_package(self, files, path=None):
        with zipfile.ZipFile(path or self.package, "w") as archive:
            for name, data in files.items():
                archive.writestr(name, data)

    def test_signed_package_verifies_against_its_key(self):
        mod_signature.sign_package(self.package, self.signed, SIGNED_FIELDS, self.key)
        statement = mod_signature.verify_package(self.signed, self.public_key)
        self.assertEqual(statement["author"], "someone")

    def test_any_changed_byte_breaks_the_signature(self):
        mod_signature.sign_package(self.package, self.signed, SIGNED_FIELDS, self.key)
        with zipfile.ZipFile(self.signed) as archive:
            files = {name: archive.read(name) for name in archive.namelist()}
        files["probe/icon"] = b"pixelz"
        self.write_package(files, self.signed)
        with self.assertRaisesRegex(ValueError, "changed after signing"):
            mod_signature.verify_package(self.signed, self.public_key)

    def test_another_key_does_not_verify(self):
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

        mod_signature.sign_package(self.package, self.signed, SIGNED_FIELDS, self.key)
        other = Ed25519PrivateKey.generate().public_key().public_bytes_raw()
        with self.assertRaisesRegex(ValueError, "does not match"):
            mod_signature.verify_package(self.signed, other)

    def test_all_zero_key_verifies_nothing(self):
        mod_signature.sign_package(self.package, self.signed, SIGNED_FIELDS, self.key)
        with self.assertRaisesRegex(ValueError, "all zeros"):
            mod_signature.verify_package(self.signed, bytes(32))

    def test_malformed_fields_are_not_signed(self):
        fields = dict(SIGNED_FIELDS, author="someone\nelse")
        with self.assertRaisesRegex(ValueError, "author"):
            mod_signature.sign_package(self.package, self.signed, fields, self.key)

    def test_meta_files_cannot_be_signed(self):
        self.write_package({"manifest.json": b"{}", "probe/model.meta": b"{}"})
        with self.assertRaisesRegex(ValueError, ".meta"):
            mod_signature.sign_package(self.package, self.signed, SIGNED_FIELDS, self.key)

    def test_content_hash_frames_every_path_and_its_data(self):
        self.assertNotEqual(mod_signature.content_hash({"ab": b"c"}), mod_signature.content_hash({"a": b"bc"}))

    def test_generated_key_header_holds_the_public_half(self):
        header = self.root / "TrustedModKey.h"
        private_key = self.root / "signing.key"
        public = mod_signature.generate_key(private_key, header)
        self.assertEqual(mod_signature.read_trusted_key(header), public)
        self.assertEqual(len(private_key.read_text(encoding="ascii").strip()), 64)

    def test_repository_header_is_well_formed(self):
        self.assertEqual(len(mod_signature.read_trusted_key()), 32)


class SignRequestTests(unittest.TestCase):
    BODY = ("### Mod repository\n\nSomeone/their-mods\n\n### Commit\n\n"
            "0123456789ABCDEF0123456789ABCDEF01234567\n\n### Mods folder\n\n_No response_\n")

    def test_issue_form_body_becomes_a_request(self):
        request = sign_request.validate_request(sign_request.parse_issue_body(self.BODY))
        self.assertEqual(request, {"repository": "Someone/their-mods", "directory": "mods",
                                   "commit": "0123456789abcdef0123456789abcdef01234567"})

    def test_folder_outside_the_repository_is_refused(self):
        request = sign_request.parse_issue_body(self.BODY.replace("_No response_", "mods/../.."))
        with self.assertRaisesRegex(ValueError, "inside the repository"):
            sign_request.validate_request(request)

    def test_shell_characters_are_refused(self):
        request = sign_request.parse_issue_body(self.BODY.replace("Someone/their-mods", "a/b;rm -rf ~"))
        with self.assertRaisesRegex(ValueError, "repository"):
            sign_request.validate_request(request)


class ScanModSourcesTests(unittest.TestCase):
    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory()
        self.addCleanup(self.workspace.cleanup)
        self.mods = Path(self.workspace.name) / "mods"
        self.mod = self.mods / "probe"
        (self.mod / "assets").mkdir(parents=True)
        (self.mod / "manifest.json").write_text('{"name": "probe"}', encoding="utf-8")
        (self.mod / "probe.c").write_text('#include "soh/ModApi/ModApi.h"\n#pragma once\n', encoding="utf-8")
        (self.mod / "assets" / "icon").write_bytes(b"pixels")

    def test_plain_mod_passes(self):
        self.assertEqual(scan_mod_sources.check_layout(self.mods) + scan_mod_sources.check_directives(self.mods), [])

    def test_build_scripts_and_executables_are_refused(self):
        (self.mod / "CMakeLists.txt").write_text("", encoding="utf-8")
        (self.mod / "assets" / "helper").write_bytes(b"MZ\x90\x00")
        problems = scan_mod_sources.check_layout(self.mods)
        self.assertEqual(len(problems), 2, problems)

    def test_operating_system_headers_and_linker_pragmas_are_refused(self):
        (self.mod / "probe.c").write_text('#include <windows.h>\n#pragma comment(lib, "ws2_32")\n'
                                          '#include "../../secret.h"\n// #include <unistd.h>\n', encoding="utf-8")
        problems = scan_mod_sources.check_directives(self.mods)
        self.assertEqual(len(problems), 3, problems)

    def test_forbidden_calls_are_found_after_macros_expand(self):
        root = Path(self.workspace.name).resolve()
        preprocessed = (f'# 1 "{root / "mods" / "probe" / "probe.c"}"\n'
                        'void Probe(void) { system("x"); puts("fopen"); }\n'
                        '# 1 "/unbound/soh/include/functions.h"\n'
                        'int remove(const char* path);\n')
        problems, _ = scan_mod_sources.scan_tokens(preprocessed, root, self.mods.resolve())
        self.assertEqual(problems, {f"{Path('mods/probe/probe.c')}: uses 'system'"})

    def test_variables_and_members_named_like_forbidden_calls_pass(self):
        root = Path(self.workspace.name).resolve()
        preprocessed = (f'# 1 "{root / "mods" / "probe" / "probe.c"}"\n'
                        's32 remove; remove = 1; if (remove) { list.remove(2); queue->rename (3); }\n'
                        'std::remove(items.begin(), items.end(), 0); unlink ("file"); __asm__ volatile;\n')
        problems, _ = scan_mod_sources.scan_tokens(preprocessed, root, self.mods.resolve())
        self.assertEqual(problems, {f"{Path('mods/probe/probe.c')}: uses 'unlink'",
                                    f"{Path('mods/probe/probe.c')}: uses '__asm__'"})

    def test_services_are_traced_to_the_permission_they_need(self):
        root = Path(self.workspace.name).resolve()
        preprocessed = (f'# 1 "{root / "mods" / "probe" / "probe.c"}"\n'
                        'void Probe(void) { sApi->OpenMicrophone(48000); sApi->PickUserFile(0, 0, 0, 0); }\n')
        _, used = scan_mod_sources.scan_tokens(preprocessed, root, self.mods.resolve())
        self.assertEqual(used, {"probe": {"microphone", "files"}})

    def test_unknown_permissions_in_the_manifest_are_refused(self):
        (self.mod / "manifest.json").write_text('{"name": "probe", "permissions": ["microphone", "network"]}',
                                                encoding="utf-8")
        self.assertEqual(scan_mod_sources.check_manifests(self.mods),
                         ["probe/manifest.json: 'network' is not a permission the game knows"])


class CheckModBinaryTests(unittest.TestCase):
    def test_game_and_runtime_imports_are_allowed(self):
        self.assertTrue(check_mod_binary.is_allowed_windows_import("soh.exe", "Player_GetStrength"))
        self.assertTrue(check_mod_binary.is_allowed_windows_import("vcruntime140.dll", "memcpy"))
        self.assertTrue(check_mod_binary.is_allowed_windows_import("kernel32.dll", "IsDebuggerPresent"))
        self.assertTrue(check_mod_binary.is_allowed_windows_import("api-ms-win-crt-runtime-l1-1-0.dll",
                                                                   "_execute_onexit_table"))

    def test_system_reach_is_refused(self):
        self.assertFalse(check_mod_binary.is_allowed_windows_import("kernel32.dll", "CreateProcessW"))
        self.assertFalse(check_mod_binary.is_allowed_windows_import("ws2_32.dll", "connect"))
        self.assertFalse(check_mod_binary.is_allowed_windows_import("api-ms-win-crt-stdio-l1-1-0.dll", "fopen"))
        self.assertFalse(check_mod_binary.is_allowed_windows_import("msvcp140.dll", "__std_fs_open_handle"))

    def test_trust_system_internals_are_refused(self):
        self.assertFalse(check_mod_binary.is_allowed_windows_import("soh.exe", "ModPermissions_RegisterMod"))
        self.assertFalse(check_mod_binary.is_allowed_windows_import(
            "soh.exe", "?ModPermissions_FindCaller@@YAPEBUModIdentity@@PEBX@Z"))
        self.assertTrue(check_mod_binary.is_allowed_windows_import("soh.exe", "Microphone_Open"))


class SignQueueTests(unittest.TestCase):
    def issue(self, author="someone", labels=()):
        return {"number": 1, "author": author, "labels": {"sign-mod", *labels}, "body": ""}

    def test_trusted_authors_go_straight_to_the_queue(self):
        self.assertEqual(sign_queue.triage(self.issue("Skijer"), {"skijer"}, set()), "queue")

    def test_new_authors_wait_for_approval_once(self):
        self.assertEqual(sign_queue.triage(self.issue(), set(), set()), "await")
        self.assertIsNone(sign_queue.triage(self.issue(labels=["awaiting-approval"]), set(), set()))

    def test_approval_label_queues_a_waiting_request(self):
        issue = self.issue(labels=["awaiting-approval", "approved"])
        self.assertEqual(sign_queue.triage(issue, set(), set()), "queue")

    def test_revoked_authors_are_rejected_even_when_trusted(self):
        self.assertEqual(sign_queue.triage(self.issue("Someone"), {"someone"}, {"someone"}), "reject")

    def test_queued_requests_are_left_alone(self):
        self.assertIsNone(sign_queue.triage(self.issue(labels=["sign-queued"]), set(), set()))

    def test_revoked_authors_are_read_from_the_game_header(self):
        with tempfile.TemporaryDirectory() as directory:
            header = Path(directory) / "RevokedMods.h"
            header.write_text('kRevokedModAuthors = {\n    "badactor",\n    "Other",\n};\n'
                              'kRevokedModPackages = {\n    "abc",\n};\n', encoding="utf-8")
            self.assertEqual(sign_queue.read_revoked_authors(header), {"badactor", "other"})

    def test_repository_lists_parse(self):
        self.assertIn("skijer", sign_queue.read_trusted_authors())
        self.assertEqual(sign_queue.read_revoked_authors(), set())


if __name__ == "__main__":
    unittest.main()
