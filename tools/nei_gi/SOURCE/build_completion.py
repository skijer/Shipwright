"""Build only the four missing GI models (and optionally their two used models)."""
import argparse
import importlib.util
import shutil
from pathlib import Path
import meshkit
import preview
import completion

ROOT=Path(__file__).resolve().parents[1]
REPO=ROOT.parents[1]


def build(builders, root, install=False):
    meshkit.ROOT=preview.ROOT=root
    for slug,builder in builders.items():
        model=builder()
        stats=meshkit.export_resources(model)
        preview.checkpoint(model,stats)
        if install:
            shutil.copytree(root/'RESOURCES'/model.prefix,
                            REPO/'soh/assets/custom'/model.prefix,dirs_exist_ok=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('items',nargs='*',choices=list(completion.BUILDERS))
    parser.add_argument('--held',action='store_true')
    parser.add_argument('--install',action='store_true')
    args=parser.parse_args()
    names=args.items or list(completion.BUILDERS)
    build({n:completion.BUILDERS[n] for n in names},ROOT,args.install)
    if args.held:
        spec=importlib.util.spec_from_file_location('nei_held_completion',ROOT.parent/'nei_held/SOURCE/completion.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        build({n:module.BUILDERS[n] for n in names if n in module.BUILDERS},ROOT.parent/'nei_held',args.install)


if __name__=='__main__':main()
