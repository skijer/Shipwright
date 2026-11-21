import argparse
import json
import re
import sys
from pathlib import Path

BEGIN_MARKER = "<!-- unbound-mods:begin -->"
END_MARKER = "<!-- unbound-mods:end -->"
ALL_MODS_PACKAGE = "unbound-mods-all.zip"


def read_mods(mods_directory):
    mods = []
    for manifest_path in sorted(mods_directory.glob("*/manifest.json")):
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        mods.append((manifest_path.parent.name, manifest.get("author", ""), manifest.get("description", "")))
    return mods


def table_cell(text):
    return str(text).replace("|", "\\|").replace("\n", " ")


def mod_cell(name, download_base):
    if not download_base:
        return f"`{name}`"
    return f"[`{name}`]({download_base}/{name}.o2r)"


def render_table(mods, download_base, status_line):
    lines = [BEGIN_MARKER, f"### Mods ({len(mods)})", ""]
    if download_base:
        lines += [f"**[Download all mods ({ALL_MODS_PACKAGE})]({download_base}/{ALL_MODS_PACKAGE})** · "
                  "click a mod's name to download only its `.o2r`. Every package runs on Windows, Linux and macOS.", ""]
    if status_line:
        lines += [status_line, ""]
    lines += ["| Mod | Author | Description |", "|---|---|---|"]
    lines += [f"| {mod_cell(name, download_base)} | {table_cell(author)} | {table_cell(description)} |"
              for name, author, description in mods]
    lines.append(END_MARKER)
    return "\n".join(lines)


def replace_section(body, section):
    pattern = re.compile(re.escape(BEGIN_MARKER) + ".*?" + re.escape(END_MARKER), re.S)
    if pattern.search(body):
        return pattern.sub(lambda _: section, body)
    return f"{body.rstrip()}\n\n{section}\n" if body.strip() else f"{section}\n"


def main():
    parser = argparse.ArgumentParser(description="Render the mods in a folder as a markdown table.")
    parser.add_argument("mods_directory", type=Path)
    parser.add_argument("--download-base", default="", help="URL the <mod>.o2r and the all-mods zip are published under")
    parser.add_argument("--status", default="", help="A line shown above the table, e.g. a build status")
    parser.add_argument("--update-body", type=Path, help="Rewrite the mods section of this markdown file in place")
    args = parser.parse_args()

    section = render_table(read_mods(args.mods_directory), args.download_base.rstrip("/"), args.status)
    if args.update_body:
        body = args.update_body.read_text(encoding="utf-8") if args.update_body.exists() else ""
        args.update_body.write_text(replace_section(body, section), encoding="utf-8")
    else:
        sys.stdout.reconfigure(encoding="utf-8")
        print(section)


if __name__ == "__main__":
    main()
