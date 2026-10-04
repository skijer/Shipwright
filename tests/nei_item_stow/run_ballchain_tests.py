"""Compile and exercise the real Ball and Chain handler and equip input helpers."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def flags():
    result = ["-std=gnu2x", "-DF3DEX_GBI_2", "-DLOG_LEVEL_GAME_PRINTS=0"]
    result += ["-I" + str(ROOT / path) for path in
               ("soh", "soh/include", "soh/src", "soh/assets", "soh/mods", "libultraship/include")]
    for config in ("CMake/soh-cvars.cmake", "CMake/lus-cvars.cmake"):
        for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT / config).read_text()):
            result.append(f'-D{key}="{value}"')
    return result


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="nei-ballchain-tests-") as temporary:
        binary = str(Path(temporary) / "ballchain")
        subprocess.run([os.environ.get("CC", "cc"), *flags(),
                        "-Werror=implicit-function-declaration", "-ffunction-sections", "-fdata-sections",
                        str(ROOT / "tests/nei_item_stow/ballchain_audio_test.c"),
                        "-Wl,--gc-sections", "-lm", "-o", binary], check=True)
        subprocess.run([binary], check=True)
