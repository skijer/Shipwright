import argparse
import json
import zipfile
from pathlib import Path


PLATFORMS = ("windows", "mac", "linux")
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
    for edition, tag in TAGS.items():
        name = prefix if edition == "public" else f"{prefix}-unbound-unsafe"
        other = TAGS["modders" if edition == "public" else "public"]
        title = "Shipwright: Unbound" if prefix == "soh" else "2Ship: Unbound"
        policy = "Signed mods only." if edition == "public" else "Signed mods load normally; unsigned mods ask for permission."
        lines = [f"# {title} — {'Players' if edition == 'public' else 'Modder resources'}", "", policy, "",
                 "Download the ZIP for your system and extract it. On Windows launch the EXE; on macOS open the DMG; on Linux make the AppImage executable and launch it.", "",
                 "| System | Download |", "| --- | --- |"]
        base = f"https://github.com/{metadata['repository']}/releases"
        for platform in PLATFORMS:
            lines.append(f"| {platform.title()} | [Download ZIP]({base}/download/{tag}/{name}-{platform}.zip) |")
        lines += ["", "Complete first-run setup with your own supported ROM. Install signed mods from **Network > NEI-Store**.", "",
                  f"[{'Modder resources / unsafe builds' if edition == 'public' else 'Regular builds for players'}]({base}/tag/{other})", "",
                  f"Built from [`{metadata['commit'][:12]}`](https://github.com/{metadata['repository']}/commit/{metadata['commit']}) · [Build run](https://github.com/{metadata['repository']}/actions/runs/{metadata['run_id']})", "",
                  "This page and its ZIP download links stay the same when builds are updated."]
        if edition == "modders":
            lines += ["", f"[Windows mod SDK import library]({base}/download/{tag}/unbound-mod-sdk-windows.zip)", "",
                      f"[Mod SDK documentation](https://github.com/{metadata['repository']}/tree/unbound-modapi/mod-sdk)"]
        (output / edition / "notes.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
        (output / edition / "build-info.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifacts", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--metadata", type=Path, required=True)
    args = parser.parse_args()
    PackageDownloads(args.artifacts, args.output, json.loads(args.metadata.read_text(encoding="utf-8")))
