"""Compile actual object/effect/batcher/matrix/interpolation boundaries together."""
import os
import re
from pathlib import Path
import subprocess
import tempfile
from run_tests import ROOT, flags

with tempfile.TemporaryDirectory(prefix="nei-rod-interpolation-") as temporary:
    common = ["-I" + str(ROOT / "tests/nei_used_fx/stubs"), *flags(), "-O1", "-g",
              "-ffunction-sections", "-fdata-sections"]
    sources = ["soh/mods/items/objects/object_firerod.c", "soh/mods/items/objects/object_icerod.c",
               "soh/src/code/sys_matrix.c", "soh/src/code/z_skin_matrix.c",
               "soh/soh/Enhancements/randomizer/NeiUsedMagicPresentation.cpp",
               "soh/soh/Enhancements/randomizer/NeiGiPresentation.cpp",
               "soh/soh/frame_interpolation.cpp", "tests/nei_used_fx/interpolation_test.cpp"]
    objects = []
    # Compile the published baseline under distinct names, avoiding inline ODR
    # collisions. Compare its complete real-renderer output with the candidate.
    base = "c77c18587a976f6d6cb5c8f91f27593286469218"
    prefix = "soh/soh/Enhancements/randomizer/"
    policy = subprocess.check_output(["git", "show", f"{base}:{prefix}NeiUsedMagicPolicy.h"],
                                     cwd=ROOT, text=True)
    policy = policy.replace('"NeiGiEffectPolicy.h"', '"soh/Enhancements/randomizer/NeiGiEffectPolicy.h"')
    policy = policy.replace("namespace NeiUsedMagic {", "namespace BaselineMagic {")
    (Path(temporary) / "baseline_policy.h").write_text(policy)
    source = subprocess.check_output(["git", "show", f"{base}:{prefix}NeiUsedMagicPresentation.cpp"],
                                     cwd=ROOT, text=True)
    source = source.replace('"NeiUsedMagicPolicy.h"', '"baseline_policy.h"')
    for header in ("NeiUsedMagicPresentation.h", "NeiGiRender.h"):
        source = source.replace(f'"{header}"', f'"soh/Enhancements/randomizer/{header}"')
    source = source.replace("NeiUsedMagic::", "BaselineMagic::")
    source = re.sub(r'void NeiUsedMagic_(\w+)\(', r'void Baseline_\1(', source)
    baseline = Path(temporary) / "baseline.cpp"
    baseline.write_text(source)
    sources.append(str(baseline))
    for index, source in enumerate(sources):
        is_c = source.endswith(".c")
        compiler = os.environ.get("CC" if is_c else "CXX", "cc" if is_c else "c++")
        output = str(Path(temporary) / f"{index}.o")
        extra = ["-Wno-incompatible-pointer-types", "-Wno-int-conversion"] if is_c else []
        subprocess.run([compiler, "-std=gnu2x" if is_c else "-std=c++20", *common, *extra,
                        "-c", str(ROOT / source), "-o", output], check=True)
        objects.append(output)
    binary = str(Path(temporary) / "interpolation")
    subprocess.run([os.environ.get("CXX", "c++"), *objects, "-Wl,--gc-sections", "-o", binary], check=True)
    subprocess.run([binary], check=True)
