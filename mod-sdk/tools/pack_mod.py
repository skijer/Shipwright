import argparse
import json
import re
import zipfile
from pathlib import Path

PLATFORMS = ("windows_x64", "windows_x86", "linux_x64", "darwin")
RESERVED_PREFIXES = ("bin/",)
RESERVED_PATHS = ("manifest.json",)
TEXTURE_SOURCE = re.compile(r"^(?P<resource>.+)\.(?P<format>[a-z0-9]+)\.png$")


def read_manifest(mod_directory):
    manifest = json.loads((mod_directory / "manifest.json").read_text(encoding="utf-8"))
    if not isinstance(manifest, dict) or not isinstance(manifest.get("name"), str) or not manifest["name"].strip():
        raise ValueError("A mod manifest requires a nonempty name")
    return manifest


def read_asset(path, destination):
    texture = TEXTURE_SOURCE.match(destination)
    if texture is None:
        return destination, path.read_bytes()

    import otex

    if texture["format"] not in otex.FORMATS:
        raise ValueError(f"{path}: unsupported texture format '{texture['format']}' "
                         f"(supported: {', '.join(sorted(otex.FORMATS))})")
    return texture["resource"], otex.encode(path, texture["format"])


def collect_assets(mod_directory):
    assets = mod_directory / "assets"
    if not assets.is_dir():
        return {}
    payload = {}
    for path in sorted(path for path in assets.rglob("*") if path.is_file()):
        if path.is_symlink() or not path.resolve().is_relative_to(assets.resolve()):
            raise ValueError(f"Asset outside mod assets directory: {path}")
        destination, data = read_asset(path, path.relative_to(assets).as_posix())
        if destination in RESERVED_PATHS or destination.startswith(RESERVED_PREFIXES):
            raise ValueError(f"Reserved archive path: {destination}")
        if destination in payload:
            raise ValueError(f"Two assets are packaged as {destination}")
        payload[destination] = data
    return payload


def binary_archive_path(platform, binary):
    return f"bin/{platform}/{binary.name}"


def main():
    parser = argparse.ArgumentParser(description="Package one Unbound mod as an .o2r archive.")
    parser.add_argument("--mod-directory", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--platform", choices=PLATFORMS, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    manifest = read_manifest(args.mod_directory)
    if not args.binary.is_file():
        raise ValueError(f"Missing native binary: {args.binary}")
    payload = collect_assets(args.mod_directory)
    binary_path = binary_archive_path(args.platform, args.binary)
    manifest["binaries"] = {args.platform: binary_path}

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("manifest.json", json.dumps(manifest, indent=2))
        archive.write(args.binary, binary_path)
        for destination, data in payload.items():
            archive.writestr(destination, data)
    print(args.output)


if __name__ == "__main__":
    main()
