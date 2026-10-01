"""Trae al mod mm_assets el árbol de descripción de assets de Majora's Mask, que es lo que ZAPD necesita para
saber qué hay en cada offset de la ROM. Los nombres se copian tal cual: los recursos de MM salen con las mismas
rutas que en un mm.o2r de 2ship, para que un mod escrito contra ese archivo valga aquí sin tocar nada.
"""

import argparse
import shutil
from pathlib import Path

VERBATIM = ("filelists", "symbols", "Config_GC_US.xml", "Config_N64_US.xml", "EnumData.xml")
XML_VERSIONS = ("GC_US", "N64_US")
DEFAULT_DESTINATION = Path(__file__).resolve().parents[1] / "mods" / "mm_assets" / "assets" / "mm_zapd" / "assets"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True, help="carpeta mm/assets de 2ship")
    parser.add_argument("--destination", type=Path, default=DEFAULT_DESTINATION)
    arguments = parser.parse_args()

    if arguments.destination.exists():
        shutil.rmtree(arguments.destination)
    arguments.destination.mkdir(parents=True)

    for entry in VERBATIM:
        source = arguments.source / "extractor" / entry
        if source.is_dir():
            shutil.copytree(source, arguments.destination / entry)
        else:
            shutil.copy2(source, arguments.destination / entry)

    copied = 0
    for version in XML_VERSIONS:
        root = arguments.source / "xml" / version
        for source in sorted(root.rglob("*.xml")):
            destination = arguments.destination / "xml" / version / source.relative_to(root)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
            copied += 1
    print(f"{copied} XML copiados en {arguments.destination}")


if __name__ == "__main__":
    main()
