import argparse
import json
import zipfile
from pathlib import Path


def read_package(path):
    with zipfile.ZipFile(path) as archive:
        manifest = json.loads(archive.read("manifest.json"))
        files = {name: archive.read(name) for name in archive.namelist() if name != "manifest.json"}
    return manifest, files


def merge(packages):
    merged_manifest = None
    merged_files = {}
    for path in packages:
        manifest, files = read_package(path)
        binaries = manifest.pop("binaries", {})
        if merged_manifest is None:
            merged_manifest = dict(manifest, binaries={})
        elif {key: value for key, value in merged_manifest.items() if key != "binaries"} != manifest:
            raise ValueError(f"{path} was built from a different manifest")
        for platform, binary_path in binaries.items():
            if platform in merged_manifest["binaries"]:
                raise ValueError(f"{path} repeats the {platform} binary")
            merged_manifest["binaries"][platform] = binary_path
        for name, data in files.items():
            if name in merged_files and merged_files[name] != data and not name.startswith("bin/"):
                raise ValueError(f"{path} ships a different {name}")
            merged_files[name] = data
    if merged_manifest is None:
        raise ValueError("Nothing to merge")
    return merged_manifest, merged_files


def main():
    parser = argparse.ArgumentParser(
        description="Merge per-platform packages of the same mod into one .o2r that loads on every platform.")
    parser.add_argument("packages", type=Path, nargs="+")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    manifest, files = merge(args.packages)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("manifest.json", json.dumps(manifest, indent=2))
        for name in sorted(files):
            archive.writestr(name, files[name])
    print(args.output)


if __name__ == "__main__":
    main()
