"""Combine validated GI and held resources, preserving identical Alt copies."""
import argparse
from pathlib import Path
import zipfile
from verify_assets import gi, PREFIX, verify, verify_archive

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('output',type=Path)
args=p.parse_args()
report=verify()
args.output.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(args.output,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for prefix in (gi.PREFIX,PREFIX):
        for file in sorted((gi.ASSETS/prefix).rglob('*')):
            if not file.is_file():
                continue
            name=file.relative_to(gi.ASSETS).as_posix()
            for entry in (name,'alt/'+name):
                info=zipfile.ZipInfo(entry,(2026,9,27,0,0,0))
                info.compress_type=zipfile.ZIP_DEFLATED
                z.writestr(info,file.read_bytes())
print('PASS:',verify_archive(args.output),'identical base/Alt entries in',args.output)
