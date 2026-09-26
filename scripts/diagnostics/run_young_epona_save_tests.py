#!/usr/bin/env python3
"""Exercise Young Epona saves through production SaveManager code and real JSON.

Full base v1-v4 loaders, the base saver, JSON traversal, initialization, and
in-memory file dispatch are extracted verbatim. Only unrelated engine startup
and the thread-pool dependency are fixtures; SaveContext and JSON are real.
Requires the normal nlohmann/json.hpp build dependency (or CPLUS_INCLUDE_PATH).
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile

from source_fixture import block_from

ROOT = Path(__file__).resolve().parents[2]


def extract(source, name):
    match = re.search(r"^(?:static )?[\w: *]+\b" + re.escape(name) + r"\s*\([^;{}]*\)\s*\{", source, re.M)
    if not match:
        raise RuntimeError(f"Production function not found: {name}")
    return block_from(source, match.start())


def main():
    source = (ROOT / "soh/soh/SaveManager.cpp").read_text()
    names = (
        "ResetYoungHorseSaveData", "SaveManager::InitFile", "SaveManager::InitFileImpl",
        "SaveManager::InitFileNormal", "SaveManager::InitFileDebug", "SaveManager::InitFileMaxed",
        "SaveManager::LoadBaseVersion1", "SaveManager::LoadBaseVersion2",
        "SaveManager::LoadBaseVersion3", "SaveManager::LoadBaseVersion4", "SaveManager::SaveBase",
        "SaveManager::LoadCharArray", "SaveManager::SaveArray", "SaveManager::SaveStruct",
        "SaveManager::LoadArray", "SaveManager::LoadStruct", "SaveManager::AddInitFunction",
        "SaveManager::AddLoadFunction", "SaveManager::AddSaveFunction",
        "SaveManager::SaveToJsonObject", "SaveManager::LoadFromJsonObject",
    )
    production = "\n\n".join(extract(source, name) for name in names)
    production += "\n" + extract((ROOT / "soh/soh/util.cpp").read_text(), "SohUtils::CopyStringToCharArray")
    # Preserve the actual base registration and init registration, while leaving
    # thread creation and unrelated randomizer startup outside the fixture.
    constructor = source[source.index("SaveManager::SaveManager() {"):source.index("void SaveManager::LoadRandomizer()")]
    registration = "\n".join(line for line in constructor.splitlines()
                             if 'coreSectionIDsByName["base"]' in line
                             or 'AddLoadFunction("base"' in line
                             or 'AddSaveFunction("base"' in line
                             or "AddInitFunction(InitFileImpl)" in line)
    production += "\nSaveManager::SaveManager() {\n" + registration + "\n}\n"
    flags = ["-std=c++20", "-O1", "-g", "-DF3DEX_GBI_2", "-DLOG_LEVEL_GAME_PRINTS=0",
             "-Ilibultraship/include", "-Isoh/include", "-Isoh/src", "-Isoh/assets", "-Isoh",
             "-fsanitize=undefined", "-fno-sanitize-recover=all"]
    for path in ("CMake/soh-cvars.cmake", "CMake/lus-cvars.cmake"):
        for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT / path).read_text()):
            flags.append(f'-D{key}="{value}"')
    with tempfile.TemporaryDirectory(prefix="young-epona-save-") as directory:
        build = Path(directory)
        (build / "young_epona_save_production.inc").write_text(production)
        # The real SaveManager declaration only needs this incomplete type. No
        # thread pool is constructed or exercised in these synchronous tests.
        (build / "BS_thread_pool.hpp").write_text("#pragma once\nnamespace BS { class thread_pool; }\n")
        binary = build / "young_epona_save_test"
        subprocess.run([os.environ.get("CXX", "c++"), *flags, "-I" + directory,
                        "soh/tests/young_epona_save_test.cpp", "-o", str(binary)], cwd=ROOT, check=True)
        subprocess.run([str(binary), str(build / "roundtrip.sav")], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
