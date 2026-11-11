"""Print the uninitialized C globals ("common" symbols) defined across a list of COFF object files.

MSVC stores such a global as an undefined external whose value is its size, so CMake's __create_def,
which only exports symbols that live in a section, leaves them out of the export table.
"""

import struct
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

SYMBOL_UNDEFINED_SECTION = 0
STORAGE_CLASS_EXTERNAL = 2
BIGOBJ_SIGNATURE = (0x0000, 0xFFFF)


def read_symbol_layout(header):
    first, second = struct.unpack_from("<HH", header, 0)
    if (first, second) == BIGOBJ_SIGNATURE:
        version = struct.unpack_from("<H", header, 4)[0]
        if version < 2:
            return None
        symbol_table, symbol_count = struct.unpack_from("<II", header, 48)
        return symbol_table, symbol_count, 20, "<8sIiHBB"
    symbol_table, symbol_count = struct.unpack_from("<II", header, 8)
    return symbol_table, symbol_count, 18, "<8sIhHBB"


def symbol_name(raw_name, string_table):
    if raw_name[:4] == b"\0\0\0\0":
        offset = struct.unpack_from("<I", raw_name, 4)[0]
        return string_table[offset:string_table.index(b"\0", offset)].decode("ascii", "replace")
    return raw_name.rstrip(b"\0").decode("ascii", "replace")


def read_symbols(object_path):
    with object_path.open("rb") as stream:
        layout = read_symbol_layout(stream.read(56))
        if layout is None:
            return None
        symbol_table, symbol_count, record_size, record_format = layout
        stream.seek(symbol_table)
        symbols = stream.read(symbol_count * record_size)
        string_table = stream.read()
    return symbols, symbol_count, record_size, record_format, string_table


def common_symbols(object_path):
    table = read_symbols(object_path)
    if table is None:
        return
    symbols, symbol_count, record_size, record_format, string_table = table
    index = 0
    while index < symbol_count:
        raw_name, value, section, _, storage_class, aux_count = struct.unpack_from(
            record_format, symbols, index * record_size)
        is_common = section == SYMBOL_UNDEFINED_SECTION and value != 0 and storage_class == STORAGE_CLASS_EXTERNAL
        if is_common:
            name = symbol_name(raw_name, string_table)
            if name[:1] not in ("?", "@", "."):
                yield name
        index += 1 + aux_count


def main():
    object_paths = [Path(line.strip()) for line in Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
                    if line.strip()]
    names = set()
    with ThreadPoolExecutor() as pool:
        for found in pool.map(lambda path: list(common_symbols(path)), object_paths):
            names.update(found)
    print("\n".join(sorted(names)))


if __name__ == "__main__":
    main()
