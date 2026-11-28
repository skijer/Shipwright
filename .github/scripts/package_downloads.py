import argparse
import json
import zipfile
from pathlib import Path


PLATFORMS = ("windows", "mac", "linux")
PLATFORM_LABELS = {"windows": "Windows", "mac": "macOS", "linux": "Linux"}
TAGS = {"public": "nei-unbound-release", "modders": "nei-unbound-modders"}


def WriteZip(source, output):
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(source.rglob("*")):
            if path.is_file():
                info = zipfile.ZipInfo.from_file(path, path.relative_to(source).as_posix())
                if path.suffix.lower() == ".appimage":
                    info.create_system = 3
                    info.external_attr = 0o100755 << 16
                info.compress_type = zipfile.ZIP_DEFLATED
                archive.writestr(info, path.read_bytes())


def PackageDownloads(artifacts, output, metadata):
    prefix = metadata["project"]
    if prefix not in ("soh", "2ship"):
        raise ValueError("Unknown project")
    sources = []
    for edition in TAGS:
        name = prefix if edition == "public" else f"{prefix}-unbound-unsafe"
        for platform in PLATFORMS:
            source = artifacts / f"{name}-{platform}"
            required = {"windows": f"{prefix}.exe", "mac": "SoH.dmg" if prefix == "soh" else "2Ship.dmg",
                        "linux": f"{prefix}.appimage"}[platform]
            if edition == "modders" and platform != "windows":
                required = required.replace(".dmg", "-unbound-unsafe.dmg").replace(".appimage", "-unbound-unsafe.appimage")
            if not (source / required).is_file():
                raise ValueError(f"Missing build: {source / required}")
            sources.append((edition, source, f"{name}-{platform}.zip"))
    library = artifacts / f"{prefix}-windows" / f"{prefix}.lib"
    if not library.is_file():
        raise ValueError(f"Missing SDK import library: {library}")
    for edition, source, filename in sources:
        target = output / edition
        target.mkdir(parents=True, exist_ok=True)
        WriteZip(source, target / filename)
    with zipfile.ZipFile(output / "modders" / "unbound-mod-sdk-windows.zip", "w", zipfile.ZIP_DEFLATED) as archive:
        archive.write(library, library.name)
    for edition in TAGS:
        (output / edition / "notes.md").write_text(DescribeEdition(edition, metadata), encoding="utf-8")


def DescribeEdition(edition, metadata):
    prefix = metadata["project"]
    repository = f"https://github.com/{metadata['repository']}"
    tag = TAGS[edition]
    name = prefix if edition == "public" else f"{prefix}-unbound-unsafe"
    title = "Shipwright: Unbound" if prefix == "soh" else "2Ship: Unbound"
    rom = "Ocarina of Time" if prefix == "soh" else "Majora's Mask"
    links = " · ".join(f"[{label}]({repository}/releases/download/{tag}/{name}-{platform}.zip)"
                       for platform, label in PLATFORM_LABELS.items())
    if edition == "public":
        lines = [f"# {title}", "", f"Extract the ZIP and launch the game. You need your own {rom} ROM.", "",
                 f"**Download:** {links}", "",
                 f"Mod developers: [unsafe builds and SDK]({repository}/releases/tag/{TAGS['modders']})"]
    else:
        lines = [f"# {title} — Modders", "",
                 f"**For mod developers only.** Players: use the [regular builds]({repository}/releases/tag/{TAGS['public']}).", "",
                 "These builds ask before loading unsigned mods.", "", f"**Download:** {links}", "",
                 f"[Windows mod SDK]({repository}/releases/download/{tag}/unbound-mod-sdk-windows.zip) · "
                 f"[Documentation]({repository}/tree/unbound-modapi/mod-sdk)"]
    # update-downloads reads the run id back to never replace a newer build with an older one.
    lines += ["", f"<!-- run_id: {metadata['run_id']} commit: {metadata['commit']} -->"]
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifacts", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--metadata", type=Path, required=True)
    args = parser.parse_args()
    PackageDownloads(args.artifacts, args.output, json.loads(args.metadata.read_text(encoding="utf-8")))
