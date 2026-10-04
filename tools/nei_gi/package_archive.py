"""Package the validated, committed GI assets without rebuilding the checkpoints."""
import argparse
from pathlib import Path
import zipfile
from verify_assets import ASSETS, PREFIX, verify, verify_archive

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("output", type=Path)
args = parser.parse_args()
verify()
args.output.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(args.output, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for source in sorted((ASSETS / PREFIX).rglob("*")):
        if source.is_file():
            name = source.relative_to(ASSETS).as_posix()
            for entry in (name, "alt/" + name):
                info = zipfile.ZipInfo(entry, (2026, 9, 27, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                archive.writestr(info, source.read_bytes())
verify_archive(args.output)
print(args.output)
