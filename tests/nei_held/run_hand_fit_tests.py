"""Compile real wrist and item draw sources at their graphics boundary."""
import os
from pathlib import Path
import subprocess
import tempfile

from run_articulated_tests import ROOT, flags


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="nei-hand-fit-tests-") as temporary:
        objects = []
        sources = ["helpers/equip_helper.c", "objects/object_whip.c", "objects/object_firerod.c",
                   "objects/object_icerod.c", "objects/object_lightrod.c", "objects/object_shovel.c",
                   "objects/object_spinner.c", "objects/object_cane_of_somaria.c"]
        for source in sources:
            output = str(Path(temporary) / (Path(source).stem + ".o"))
            # Somaria normally enters the item unity after these real headers.
            # Supply that same declaration context without its gameplay actors.
            headers = []
            if source == "objects/object_cane_of_somaria.c":
                for header in ("soh/include/variables.h", "soh/mods/actors/somaria_cubes.h", "soh/mods/actors/pacci_flip_vfx.h",
                               "soh/expansions/trirod/trirod.h"):
                    headers.extend(("-include", str(ROOT / header)))
            subprocess.run([os.environ.get("CC", "cc"), "-std=gnu2x", *flags()[1:],
                            "-Wno-int-conversion", "-Wno-incompatible-pointer-types",
                            "-ffunction-sections", "-fdata-sections", *headers, "-c",
                            str(ROOT / "soh/mods/items" / source), "-o", output], check=True)
            objects.append(output)
        binary = str(Path(temporary) / "hand-fit")
        subprocess.run([os.environ.get("CXX", "c++"), *flags(), "-ffunction-sections", "-fdata-sections",
                        str(ROOT / "tests/nei_held/hand_fit_runtime_test.cpp"), *objects,
                        "-Wl,--gc-sections", "-o", binary], check=True)
        subprocess.run([binary], check=True)
        subprocess.run([binary, "--missing-wrapper-resources"], check=True)
