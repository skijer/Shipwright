import argparse
import json
import re
from pathlib import Path

BEGIN_MARKER = "<!-- unbound-mods:begin -->"
END_MARKER = "<!-- unbound-mods:end -->"


def read_mods(mods_directory):
    mods = []
    for manifest_path in sorted(mods_directory.glob("*/manifest.json")):
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        mods.append((manifest_path.parent.name, manifest.get("author", ""), manifest.get("description", "")))
    return mods


def table_cell(text):
    return str(text).replace("|", "\\|").replace("\n", " ")


def render_table(mods, artifact_url):
    lines = [BEGIN_MARKER, f"### Mods ({len(mods)})", ""]
    if artifact_url:
        lines += [f"Universal `.o2r` packages: [unbound-mods artifact]({artifact_url})", ""]
    lines += ["| Mod | Author | Description |", "|---|---|---|"]
    lines += [f"| `{name}` | {table_cell(author)} | {table_cell(description)} |" for name, author, description in mods]
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
    parser.add_argument("--artifact-url", default="")
    parser.add_argument("--update-body", type=Path, help="Rewrite the mods section of this markdown file in place")
    args = parser.parse_args()

    section = render_table(read_mods(args.mods_directory), args.artifact_url)
    if args.update_body:
        body = args.update_body.read_text(encoding="utf-8") if args.update_body.exists() else ""
        args.update_body.write_text(replace_section(body, section), encoding="utf-8")
    else:
        print(section)


if __name__ == "__main__":
    main()
