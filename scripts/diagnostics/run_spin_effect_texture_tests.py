"""Run the real effect texture importers with production binary readers and ownership."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    sources = [
        "soh/tests/spin_effect_texture_test.cpp",
        "soh/soh/resource/importer/SpinEffectTextureFactory.cpp",
        "libultraship/src/fast/resource/factory/TextureFactory.cpp",
        "libultraship/src/fast/resource/type/Texture.cpp",
        "libultraship/src/ship/resource/ResourceFactoryBinary.cpp",
        "libultraship/src/ship/resource/Resource.cpp",
        "libultraship/src/ship/utils/binarytools/BinaryReader.cpp",
        "libultraship/src/ship/utils/binarytools/MemoryStream.cpp",
        "libultraship/src/ship/utils/binarytools/Stream.cpp",
    ]
    with tempfile.TemporaryDirectory(prefix="spin-effect-textures-") as directory:
        temp = Path(directory)
        log_header = temp / "spdlog/spdlog.h"
        log_header.parent.mkdir()
        # Logging is the only dependency replaced. Parsing, conversion and buffer
        # lifetime are production code, including truncated-payload boundaries.
        log_header.write_text("#pragma once\n" + "".join(
            f"#define SPDLOG_{level}(...) ((void)0)\n"
            for level in ("TRACE", "DEBUG", "INFO", "WARN", "ERROR", "CRITICAL")))
        binary = temp / "test"
        subprocess.run([
            os.environ.get("CXX", "c++"), "-std=c++20", "-O1", "-g", "-DNDEBUG",
            "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer",
            "-I" + str(temp), "-Ilibultraship/include", *sources, "-o", str(binary),
        ], cwd=ROOT, check=True)
        # LeakSanitizer cannot run under this host's ptrace supervisor. Keep
        # address/undefined checks active; buffer ownership is tested explicitly.
        environment = os.environ.copy()
        environment["ASAN_OPTIONS"] = environment.get("ASAN_OPTIONS", "") + ":detect_leaks=0"
        subprocess.run([str(binary), *sys.argv[1:]], check=True, env=environment)


if __name__ == "__main__":
    main()
