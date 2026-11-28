"""Checks that every MM enemy plays its sounds through mm_assets' MM engine, with valid ids on both sides.

An MM id handed to OoT's engine indexes past OoT's sfx tables and reads a garbage sample on the audio thread.
"""
import argparse
import re
import sys
from pathlib import Path

SDK = Path(__file__).resolve().parents[1]
DEFAULT_MODS = SDK / "mods"
DEFAULT_OOT_SFX_HEADER = SDK.parent / "soh" / "include" / "sfx.h"
MM_TABLES = Path("mm_assets") / "sfx" / "tables"

ENEMY_PREFIX = "mm_enemy_"
ADAPTER_SERVICE = "mm.sfx.play"
OOT_PLAY_FUNCTIONS = ("Audio_PlayActorSound2", "Audio_PlaySfxGeneral", "Audio_PlaySoundGeneral", "func_80078914",
                      "func_8002F974", "func_8002F828", "func_8002F7DC", "Sfx_PlaySfxCentered", "Sfx_PlaySfxAtPos")
OOT_DEFINE = re.compile(r"^#define\s+(NA_SE_\w+)\s+(0x[0-9A-Fa-f]+)", re.MULTILINE)
MM_TABLE_ENTRY = re.compile(r"/\*\s*(0x[0-9A-Fa-f]+)\s*\*/\s*DEFINE_SFX\(\s*(NA_SE_\w+)")
MM_DEFINE = re.compile(r"^#define\s+(MM_SE_\w+)\s+(0x[0-9A-Fa-f]+)", re.MULTILINE)
WRAPPER_DEFINITION = re.compile(r"^static\s+void\s+(\w+)\(([^)]*)\bu16\s+mmSfx,\s*u16\s+ootSfx\)\s*\{", re.MULTILINE)
SOUND_TOKEN = re.compile(r"\b(?:NA|MM)_SE_\w+")


def load_oot_ids(header):
    return {name: int(value, 16) for name, value in OOT_DEFINE.findall(header.read_text(encoding="utf-8"))}


def load_mm_ids(tables_directory):
    ids = {}
    for table in sorted(tables_directory.glob("*_table.h")):
        for value, name in MM_TABLE_ENTRY.findall(table.read_text(encoding="utf-8")):
            ids[name] = int(value, 16)
    return ids


def strip_comments(source):
    return re.sub(r"//[^\n]*|/\*.*?\*/", lambda match: re.sub(r"[^\n]", " ", match.group()), source, flags=re.S)


def line_of(source, offset):
    return source.count("\n", 0, offset) + 1


def find_closing(source, opening):
    pairs = {"(": ")", "{": "}"}
    closer = pairs[source[opening]]
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == source[opening]:
            depth += 1
        elif source[index] == closer:
            depth -= 1
            if depth == 0:
                return index
    return len(source)


def split_arguments(text):
    arguments = []
    depth = 0
    start = 0
    for index, character in enumerate(text):
        if character in "([{":
            depth += 1
        elif character in ")]}":
            depth -= 1
        elif character == "," and depth == 0:
            arguments.append(text[start:index].strip())
            start = index + 1
    arguments.append(text[start:].strip())
    return arguments


def find_calls(source, function):
    for match in re.finditer(rf"\b{function}\s*\(", source):
        opening = match.end() - 1
        yield match.start(), split_arguments(source[opening + 1:find_closing(source, opening)])


def find_wrappers(source):
    wrappers = {}
    for match in WRAPPER_DEFINITION.finditer(source):
        body_start = match.end() - 1
        wrappers[match.group(1)] = (body_start, find_closing(source, body_start))
    return wrappers


def is_inside_wrapper(offset, wrappers):
    return any(start <= offset <= end for start, end in wrappers.values())


def check_local_mm_defines(source, mm_ids, report):
    defined = {}
    for match in MM_DEFINE.finditer(source):
        name, value = match.group(1), int(match.group(2), 16)
        defined[name] = value
        table_name = "NA_SE_" + name[len("MM_SE_"):]
        expected = mm_ids.get(table_name)
        if expected is None:
            report(match.start(), f"{name} is not in MM's sfx tables")
        elif expected != value:
            report(match.start(), f"{name} is 0x{value:04X} but MM's {table_name} is 0x{expected:04X}")
    return defined


def check_wrapper_calls(source, wrappers, oot_ids, mm_defines, report):
    for wrapper in wrappers:
        for offset, arguments in find_calls(source, wrapper):
            if is_inside_wrapper(offset, {wrapper: wrappers[wrapper]}) or len(arguments) < 2:
                continue
            mm_argument, oot_argument = arguments[-2], arguments[-1]
            for token in SOUND_TOKEN.findall(mm_argument):
                if token.startswith("NA_SE_"):
                    report(offset, f"{wrapper} gets OoT's {token} as the MM sound")
                elif token not in mm_defines:
                    report(offset, f"{wrapper} plays {token}, which this mod never defines")
            for token in SOUND_TOKEN.findall(oot_argument):
                if token.startswith("MM_SE_"):
                    report(offset, f"{wrapper} would hand MM's {token} to OoT's engine")
                elif token not in oot_ids:
                    report(offset, f"{wrapper} falls back to {token}, which OoT does not define")


def check_direct_oot_calls(source, wrappers, report):
    for function in OOT_PLAY_FUNCTIONS:
        for offset, _ in find_calls(source, function):
            if not is_inside_wrapper(offset, wrappers):
                report(offset, f"{function} plays an OoT sound directly instead of through the MM adapter")


def check_enemy(mod_directory, oot_ids, mm_ids):
    problems = []
    sources = sorted(mod_directory.glob("*.c")) + sorted(mod_directory.glob("*.cpp"))
    combined = "".join(path.read_text(encoding="utf-8") for path in sources)
    if ADAPTER_SERVICE not in combined:
        problems.append(f"{mod_directory.name}: never asks for the {ADAPTER_SERVICE} service")
    for path in sources:
        source = strip_comments(path.read_text(encoding="utf-8"))

        def report(offset, message, path=path, source=source):
            problems.append(f"{mod_directory.name}/{path.name}:{line_of(source, offset)}: {message}")

        wrappers = find_wrappers(source)
        mm_defines = check_local_mm_defines(source, mm_ids, report)
        check_wrapper_calls(source, wrappers, oot_ids, mm_defines, report)
        check_direct_oot_calls(source, wrappers, report)
    return problems


def find_enemy_directories(mods_directory):
    return sorted(child for child in mods_directory.iterdir()
                  if child.is_dir() and child.name.startswith(ENEMY_PREFIX))


def check_all(mods_directory, oot_header):
    oot_ids = load_oot_ids(oot_header)
    mm_ids = load_mm_ids(mods_directory / MM_TABLES)
    problems = []
    for enemy in find_enemy_directories(mods_directory):
        problems.extend(check_enemy(enemy, oot_ids, mm_ids))
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--mods-directory", type=Path, default=DEFAULT_MODS)
    parser.add_argument("--oot-sfx-header", type=Path, default=DEFAULT_OOT_SFX_HEADER)
    arguments = parser.parse_args()
    problems = check_all(arguments.mods_directory, arguments.oot_sfx_header)
    for problem in problems:
        print(problem)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
