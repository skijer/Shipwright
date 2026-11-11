import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

# The game's updater reads unbound-release.json from the newest release, after checking the release key's
# signature, and compares "version" with the gBuildForkVersion it was built with.
RELEASE_FORMAT = 1
PLATFORMS = ("windows_x64", "linux_x64", "darwin")
REPOSITORY_ROOT = Path(__file__).resolve().parents[2]


def read_version(root=REPOSITORY_ROOT):
    """The gBuildForkVersion a build of this checkout reports: '<SoH version>-<fork version>'."""
    text = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    base = re.search(r"^project\(\w+ VERSION ([0-9.]+)", text, re.MULTILINE)
    fork = re.search(r'^set\(PROJECT_FORK_VERSION "([^"]+)"', text, re.MULTILINE)
    if base is None or fork is None:
        raise ValueError("CMakeLists.txt names no project or fork version")
    return f"{base.group(1)}-{fork.group(1)}"


def build_manifest(version, page, release_base, assets):
    entries = {}
    for platform, path in assets.items():
        if platform not in PLATFORMS:
            raise ValueError(f"'{platform}' is not one of {', '.join(PLATFORMS)}")
        entries[platform] = {"file": path.name, "url": f"{release_base}/{path.name}", "size": path.stat().st_size,
                             "hash": hashlib.blake2b(path.read_bytes(), digest_size=64).hexdigest()}
    # Updaters up to unbound0.11 ignore a release without "modapi" and skip the mod rebuild only when it reads 2.
    return {"format": RELEASE_FORMAT, "version": version, "modapi": 2, "page": page, "assets": entries}


def main():
    parser = argparse.ArgumentParser(description="Describe an Unbound release for the in-game updater.")
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("version", help="Print the version a build of this checkout reports")
    build = commands.add_parser("build", help="Write the manifest payload for a set of release assets")
    build.add_argument("--version", required=True)
    build.add_argument("--page", required=True)
    build.add_argument("--release-base", required=True)
    build.add_argument("--asset", action="append", default=[], metavar="PLATFORM=PATH")
    build.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    if args.command == "version":
        print(read_version())
    else:
        assets = {}
        for entry in args.asset:
            platform, _, path = entry.partition("=")
            assets[platform] = Path(path)
        manifest = build_manifest(args.version, args.page, args.release_base.rstrip("/"), assets)
        args.output.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    try:
        main()
    except ValueError as error:
        sys.exit(f"error: {error}")
