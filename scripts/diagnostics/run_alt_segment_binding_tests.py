#!/usr/bin/env python3
"""Run the OoT segment binder through the production Alt/cache decisions.

Only archive I/O, deserialization, and thread scheduling are fixtures. The cache
decision functions, resource helpers, and gSPSegment body are real.
"""
import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

from source_fixture import function

ROOT = Path(__file__).resolve().parents[2]


def helper_function(source, name):
    declaration = re.search(r'^(?:extern "C" )?(?:static )?(?:bool|uint8_t|char\*|void|std::shared_ptr<Ship::IResource>)\s+'
                            + re.escape(name) + r'\(', source, re.M)
    if declaration is None:
        raise RuntimeError("Missing production helper: " + name)
    return function(source[declaration.start():], name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", choices=("oot",), default="oot")
    args = parser.parse_args()
    manager = (ROOT / "libultraship/src/ship/resource/ResourceManager.cpp").read_text()
    load_start = manager.index("std::shared_ptr<IResource> ResourceManager::LoadResourceProcess(const ResourceIdentifier&")
    load_end = manager.index("std::shared_ptr<IResource> ResourceManager::LoadResource(uint64_t", load_start)
    cache_start = manager.index("std::variant<ResourceManager::ResourceLoadError, std::shared_ptr<IResource>>\n"
                                "ResourceManager::CheckCache")
    cache_end = manager.index("std::shared_ptr<std::vector<std::shared_ptr<IResource>>>", cache_start)
    cache = manager[load_start:load_end] + manager[cache_start:cache_end]
    failed = False
    with tempfile.TemporaryDirectory(prefix="alt-segment-binding-") as temporary:
        build = Path(temporary)
        (build / "alt_segment_cache.inc").write_text(cache)
        (build / "spdlog").mkdir()
        (build / "spdlog/spdlog.h").write_text("#pragma once\n#define SPDLOG_TRACE(...) ((void)0)\n")
        for game in (args.game,):
            helper_path = "soh/soh/ResourceManagerHelpers.cpp"
            wrapper_path = "soh/soh/GbiWrap.cpp"
            helper = (ROOT / helper_path).read_text()
            names = ["ResourceMgr_IsAltAssetsEnabled", "ResourceMgr_FileAltExists",
                     "ResourceMgr_GetResourceByNameHandlingMQ"]
            if "static void ResourceMgr_PreloadAltWhenItExists(" in helper:
                names += ["ResourceMgr_PreloadAltWhenItExists"]
            names += ["ResourceMgr_LoadIfDListByName"]
            production = "\n".join(helper_function(helper, name) for name in names)
            production += helper_function((ROOT / wrapper_path).read_text(), "gSPSegment")
            (build / "alt_segment_helpers.inc").write_text(production)
            binary = build / (game + "_alt_segment_test")
            subprocess.run([
                *shlex.split(os.environ.get("CXX", "c++")), "-std=c++20", "-Wall", "-Wextra", "-Werror",
                "-Wno-unused-parameter", "-Wno-unused-function", "-Wno-pointer-arith", "-DF3DEX_GBI_2",
                "-fsanitize=undefined", "-fno-sanitize-recover=all", "-I" + str(build),
                "-I" + str(ROOT / "libultraship/include"),
                str(ROOT / "soh/tests/alt_segment_binding_test.cpp"),
                str(ROOT / "libultraship/src/ship/resource/Resource.cpp"),
                str(ROOT / "libultraship/src/fast/resource/type/DisplayList.cpp"),
                str(ROOT / "libultraship/src/fast/resource/type/Texture.cpp"), "-o", str(binary),
            ], check=True)
            result = subprocess.run([str(binary), game])
            failed |= result.returncode != 0
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
