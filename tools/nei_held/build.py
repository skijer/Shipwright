"""Export held components from approved source meshes; preserve all GI assets."""
import argparse
import shutil
import sys
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[1]
sys.path.insert(0, str(ROOT / 'SOURCE'))
from simple import BUILDERS
import meshkit
import preview


def build(names):
    meshkit.ROOT = preview.ROOT = ROOT
    for slug in names:
        m = BUILDERS[slug]()
        stats = meshkit.export_resources(m)
        if slug == 'gust_jar_band':
            path = ROOT / 'RESOURCES' / m.entry
            dl = ET.parse(path)
            for node in list(dl.getroot()):
                if node.tag == 'SetPrimColor':
                    dl.getroot().remove(node)
                elif node.tag == 'SetCombineLERP':
                    node.set('A1', 'G_CCMUX_COMBINED')
                    node.set('B1', 'G_CCMUX_0')
                    node.set('C1', 'G_CCMUX_PRIMITIVE')
                    node.set('D1', 'G_CCMUX_0')
            ET.indent(dl, space='  ')
            dl.write(path, encoding='unicode')
        preview.checkpoint(m, stats)
        source = ROOT / 'RESOURCES' / m.prefix
        dest = REPO / 'soh/assets/custom' / m.prefix
        shutil.copytree(source, dest, dirs_exist_ok=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('items', nargs='*')
    args = parser.parse_args()
    build(args.items or list(BUILDERS))
