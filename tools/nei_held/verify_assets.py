"""Verify actual held display lists against review GLBs and both archive paths."""
import argparse
import importlib.util
import json
from pathlib import Path
import zipfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('gi_asset_verifier', ROOT.parent/'nei_gi/verify_assets.py')
gi = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gi)
PREFIX = 'objects/nei_held_redesign/'


def verify():
    items = sorted(p.parent.name for p in (ROOT/'CHECKPOINTS').glob('*/checkpoint.json'))
    assert items, 'No held checkpoints'
    present = sorted(p.name for p in (gi.ASSETS/PREFIX).iterdir() if p.is_dir())
    assert items == present, (items, present)
    held = gi.verify(items, PREFIX, ROOT/'CHECKPOINTS')
    band = ET.parse(gi.ASSETS/PREFIX/'gust_jar_band/gi_dl').getroot()
    assert not any(n.tag == 'SetPrimColor' for n in band), 'Band overwrites live charge color'
    combines = [n for n in band if n.tag == 'SetCombineLERP']
    assert combines and all(n.get('A1') == 'G_CCMUX_COMBINED' and
                            n.get('C1') == 'G_CCMUX_PRIMITIVE' for n in combines)
    return {'gi':gi.verify(), 'held':held, 'runtime_tested':False}


def verify_archive(path):
    sources = {}
    for prefix in (gi.PREFIX, PREFIX):
        for file in sorted((gi.ASSETS/prefix).rglob('*')):
            if file.is_file():
                sources[file.relative_to(gi.ASSETS).as_posix()] = file.read_bytes()
    with zipfile.ZipFile(path) as z:
        assert len(z.namelist()) == len(set(z.namelist())) == len(sources)*2
        assert set(z.namelist()) == set(sources) | {'alt/'+p for p in sources}
        for name, data in sources.items():
            assert z.read(name) == z.read('alt/'+name) == data
    return len(sources)*2


if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--archive', type=Path)
    p.add_argument('--report', type=Path)
    args=p.parse_args()
    report=verify()
    if args.archive:
        report['archive_entries']=verify_archive(args.archive)
    if args.report:
        args.report.write_text(json.dumps(report,indent=2)+'\n')
    print(f"PASS: {len(report['gi'])} GI models and {len(report['held'])} held components; exact resource/GLB parity")
