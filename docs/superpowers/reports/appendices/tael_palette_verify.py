#!/usr/bin/env python3
"""Compile and run the actual Tael draw commands and existing matrix regression.

Usage: source build-env.sh; python tael_palette_verify.py BUILD_DIR
Uses normal build headers and flags. Only temporary files are written outside
the repository; this never invokes Ninja or changes its outputs.
"""
from pathlib import Path
import argparse
import os
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[4]


def braced_definition(source, pattern, label, array=False):
    match = re.search(pattern, source, re.M)
    if not match:
        raise RuntimeError("Missing production definition: " + label)
    depth, end = 1, match.end()
    while depth:
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
        end += 1
    if array:
        while source[end].isspace():
            end += 1
        if source[end] != ";":
            raise RuntimeError("Missing initializer terminator: " + label)
        end += 1
    return source[match.start():end] + "\n"


def function(source, name):
    return braced_definition(
        source, r"^[^\n;{}]*\b" + re.escape(name) + r"\([^;{}]*\)\s*\{", name
    )


def insert(source, marker, value):
    if source.count(marker) != 1:
        raise RuntimeError("Expected one fixture insertion point: " + marker)
    return source.replace(marker, value)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_dir", type=Path)
    args = parser.parse_args()
    ninja = (args.build_dir.resolve() / "build.ninja").read_text()
    rule = re.search(
        r"^build soh/CMakeFiles/soh.dir/src/overlays/actors/ovl_En_Viewer/"
        r"z_en_viewer\.c\.o:[^\n]*\n((?:  [^\n]*\n)+)", ninja, re.M
    )
    if not rule:
        raise RuntimeError("Missing production viewer compiler flags in build.ninja")
    flags = []
    for _, value in re.findall(r"^  (DEFINES|FLAGS|INCLUDES) = (.*)$", rule[1], re.M):
        flags.extend(shlex.split(value))
    flags += ["-ffunction-sections", "-fdata-sections", "-DNDEBUG"]
    compiler = shlex.split(os.environ.get("CC", "cc"))
    viewer = (ROOT / "soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c").read_text()
    helpers = ROOT / "soh/src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.c"

    with tempfile.TemporaryDirectory(prefix="tael-palette-") as temporary:
        work = Path(temporary)

        def run_fixture(name, source):
            path = work / (name + ".c")
            path.write_text(source)
            executable = work / name
            subprocess.run(
                [*compiler, *flags, str(path), str(helpers), "-Wl,--gc-sections",
                 "-lm", "-o", str(executable)], cwd=ROOT, check=True
            )
            subprocess.run([str(executable)], cwd=ROOT, check=True)

        source = (ROOT / "soh/tests/static_story_tael_palette_test.c").read_text()
        source = insert(source, "/* PRODUCTION_TAEL_SETUP */", braced_definition(
            viewer, r"^static Gfx sStaticStoryTatlSetupDL\[\]\s*=\s*\{",
            "sStaticStoryTatlSetupDL", array=True
        ))
        source = insert(source, "/* PRODUCTION_TAEL_DRAW */",
                        function(viewer, "EnViewer_DrawStaticTatl"))
        run_fixture("tael_palette", source)

        matrix = (ROOT / "soh/src/code/sys_matrix.c").read_text()
        skin = (ROOT / "soh/src/code/z_skin_matrix.c").read_text()
        source = (ROOT / "soh/tests/static_story_tatl_matrix_test.c").read_text()
        matrix_functions = "\n".join(function(skin, name) for name in
                                     ("SkinMatrix_SetTranslate", "SkinMatrix_SetScale"))
        matrix_functions += "\n".join(function(matrix, name) for name in
                                      ("Matrix_Translate", "Matrix_Scale", "Matrix_MultVec3f"))
        source = insert(source, "/* PRODUCTION_MATRIX_FUNCTIONS */", matrix_functions)
        source = insert(source, "/* PRODUCTION_TATL_CALLBACK */",
                        function(viewer, "EnViewer_StaticTatlOverrideLimbDraw"))
        run_fixture("tael_matrix", source)


if __name__ == "__main__":
    main()
