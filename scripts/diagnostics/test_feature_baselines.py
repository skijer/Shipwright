"""Reject incomplete cumulative histories using real temporary Git repositories."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest


CHECKER = Path(__file__).with_name("check_feature_baselines.py")


class FeatureBaselineTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.git("init", "-q")
        self.git("config", "user.name", "Fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        self.git("commit", "--allow-empty", "-qm", "base")
        self.base = self.git("rev-parse", "HEAD")
        self.git("commit", "--allow-empty", "-qm", "feature")
        self.feature = self.git("rev-parse", "HEAD")
        self.manifest = self.root / "baselines.json"
        self.inventory = {"version": 1, "baselines": [{"feature": "companion", "commit": self.feature}]}

    def git(self, *args):
        return subprocess.check_output(["git", *args], cwd=self.root, text=True).strip()

    def check(self, ref="HEAD"):
        self.manifest.write_text(json.dumps(self.inventory))
        return subprocess.run([
            "python3", str(CHECKER), "--repo", str(self.root), "--manifest", str(self.manifest), "--ref", ref,
        ], capture_output=True, text=True)

    def test_included_feature_passes(self):
        self.assertEqual(self.check().returncode, 0)

    def test_newer_date_does_not_replace_required_ancestry(self):
        self.git("checkout", "--detach", "-q", self.base)
        self.git("commit", "--allow-empty", "-qm", "new but incomplete branch")
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn("companion", result.stdout)

    def test_merge_second_parent_carries_feature(self):
        self.git("checkout", "--detach", "-q", self.base)
        self.git("commit", "--allow-empty", "-qm", "parallel feature")
        self.git("merge", "--no-ff", "-qm", "cumulative", self.feature)
        self.assertEqual(self.check().returncode, 0)

    def test_requested_ref_is_checked_instead_of_worktree_head(self):
        self.assertEqual(self.check(self.base).returncode, 1)

    def test_missing_object_fails_closed(self):
        self.inventory["baselines"][0]["commit"] = "a" * 40
        self.assertEqual(self.check().returncode, 1)

    def test_invalid_ref_is_an_error(self):
        self.assertEqual(self.check("missing-ref").returncode, 2)

    def test_malformed_or_empty_inventory_is_an_error(self):
        for data in ({"version": 1, "baselines": []}, {"version": 2, "baselines": []},
                     {"version": 1, "baselines": [{"feature": "x", "commit": "abc"}]}):
            with self.subTest(data=data):
                self.inventory = data
                self.assertEqual(self.check().returncode, 2)

    def test_shallow_history_is_not_reported_as_complete(self):
        shallow = self.root / "shallow"
        self.git("clone", "-q", "--depth=1", self.root.as_uri(), str(shallow))
        self.manifest.write_text(json.dumps(self.inventory))
        result = subprocess.run([
            "python3", str(CHECKER), "--repo", str(shallow), "--manifest", str(self.manifest),
        ], capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn("shallow", result.stderr.lower())


if __name__ == "__main__":
    unittest.main()
