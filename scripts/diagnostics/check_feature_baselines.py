#!/usr/bin/env python3
"""Fail publication if a cumulative build omits a required feature history.

This checks ancestry, not runtime acceptance or the absence of later reverts.
The production regressions remain a separate required build gate.
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=ROOT)
    parser.add_argument("--manifest", type=Path, default=ROOT / "docs/required-feature-baselines.json")
    parser.add_argument("--ref", default="HEAD")
    args = parser.parse_args()
    env = dict(os.environ, GIT_NO_LAZY_FETCH="1")

    def git(*command):
        return subprocess.run(["git", "-C", str(args.repo), *command],
                              capture_output=True, text=True, env=env)

    try:
        data = json.loads(args.manifest.read_text())
        if not isinstance(data, dict) or data.get("version") != 1:
            raise ValueError("unsupported inventory version")
        rows = data.get("baselines")
        if not isinstance(rows, list) or not rows:
            raise ValueError("inventory must contain at least one feature")
        for row in rows:
            if (not isinstance(row, dict) or not isinstance(row.get("feature"), str)
                    or not row["feature"].strip() or not isinstance(row.get("commit"), str)
                    or not re.fullmatch(r"[0-9a-f]{40}", row["commit"])):
                raise ValueError("each feature needs a name and a full lowercase commit SHA")
        shallow = git("rev-parse", "--is-shallow-repository")
        if shallow.returncode or shallow.stdout.strip() != "false":
            raise ValueError("repository unavailable or shallow; check out full history (fetch-depth: 0)")
        resolved = git("rev-parse", "--verify", "--end-of-options", args.ref + "^{commit}")
        if resolved.returncode:
            raise ValueError("cannot resolve candidate ref: " + args.ref)
        candidate = resolved.stdout.strip()
        missing = 0
        for row in rows:
            result = git("merge-base", "--is-ancestor", row["commit"], candidate)
            if result.returncode:
                missing += 1
                reason = "not an ancestor" if result.returncode == 1 else "commit/history unavailable"
                print(f"FAIL: {row['feature']} ({row['commit'][:12]}): {reason}")
        if missing:
            print(f"FAIL: {missing} required feature baseline(s) absent from {candidate[:12]}")
            return 1
        print(f"PASS: all {len(rows)} required feature baselines are ancestors of {candidate[:12]}")
        return 0
    except (OSError, ValueError) as error:
        print("ERROR: " + str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
