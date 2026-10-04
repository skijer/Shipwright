"""Replay actual Skull Kid commands through the production patcher, read-only.

Usage: python3 -B scripts/diagnostics/test_skull_cull_archive.py patch.so mm.o2r
Render-mode and texture command rewriting is verified. Nested/vertex resources are boundary fixtures;
this does not execute the full resource loader, renderer, or audio engine.
"""
import ctypes as c
import struct
import sys
import zipfile

from audit_skull_kid import Reader, SKULL, parse_limb, parse_skeleton


class Command(c.Structure):
    _fields_ = [('w0', c.c_uint32), ('w1', c.c_size_t)]


class Stats(c.Structure):
    _fields_ = [(name, c.c_size_t) for name in
                ('nested', 'vertices', 'unresolved', 'malformed', 'render_mode', 'textures')]


Resolver = c.CFUNCTYPE(c.c_size_t, c.c_void_p, c.c_int, c.c_uint64, c.POINTER(c.c_size_t))
TWO_WORD = {0x20, 0x24, 0x25, 0x27, 0x31, 0x32, 0x33, 0x35, 0x36, 0x42}
TEXTURE_IMAGE = c.create_string_buffer(b'test texture pixels')


@Resolver
def resolve(context, kind, value, size):
    size[0] = 65536
    if kind == 2:
        return {0: 0x30000000, 2: 0x30000020}.get(value, 0)
    if kind == 3:
        return c.addressof(TEXTURE_IMAGE)
    return 0x20000000  # Deliberately not a real vertex/nested resource.


def main(library, archive):
    patch = c.CDLL(library).Test_PatchMmDisplayList
    patch.argtypes = [c.POINTER(Command), c.c_size_t, Resolver, c.c_void_p, c.POINTER(Stats)]
    patch.restype = c.c_bool
    total = 0
    textures = 0
    with zipfile.ZipFile(archive) as z:
        paths = [parse_limb(z.read(p))['dl'] for p in parse_skeleton(z.read(SKULL))['paths']]
        paths = {p for p in paths if p and not p.endswith('/gSkullKidHeadWithLipsDL')}
        paths.update('objects/object_stk/' + name for name in
                     ('gSkullKidNormalHeadDL', 'gSkullKidNormalEyesDL', 'gSkullKidMajorasMask1DL'))
        for path in sorted(paths):
            reader = Reader(z.read(path), 0x4F444C54)
            reader.take(8)  # Microcode byte and alignment padding in binary V0.
            raw = reader.take(len(reader.data) - reader.offset)
            words = list(struct.iter_unpack(reader.endian + 'II', raw))
            commands = (Command * len(words))(*(Command(*word) for word in words))
            expected = []
            expected_textures = []
            i = 0
            while i < len(words):
                w0, w1 = words[i]
                opcode = w0 >> 24
                if opcode == 0xDF:
                    break
                if opcode == 0x3D and w1 >> 24 == 0x0C:
                    target = {0: 0x30000000, 2: 0x30000020}[w1 & 0xFFFFFF]
                    expected.append((i, 0xDE000000 | (w0 & 0x10000), target))
                elif opcode == 0x20:
                    expected_textures.append((i, 0x25000000 | (w0 & 0xFFFFFF)))
                i += 2 if opcode in TWO_WORD else 1
            stats = Stats()
            if not patch(commands, len(words), resolve, None, c.byref(stats)):
                raise RuntimeError(f'{path}: production patcher rejected commands')
            if (stats.unresolved or stats.malformed or stats.render_mode != len(expected) or
                    stats.textures != len(expected_textures)):
                raise RuntimeError(f'{path}: unexpected patch statistics')
            for index, w0, w1 in expected:
                if (commands[index].w0, commands[index].w1) != (w0, w1):
                    raise RuntimeError(f'{path}: wrong render-mode rewrite at command {index}')
            for index, w0 in expected_textures:
                if ((commands[index].w0, commands[index].w1) != (w0, c.addressof(TEXTURE_IMAGE)) or
                        (commands[index + 1].w0, commands[index + 1].w1) != (0, 0)):
                    raise RuntimeError(f'{path}: wrong texture rewrite at command {index}')
            total += stats.render_mode
            textures += stats.textures
    if total == 0:
        raise RuntimeError('No render-mode commands exercised')
    if textures == 0:
        raise RuntimeError('No texture commands exercised')
    print(f'PASS archived Skull Kid rewriting: {total} render-mode calls and {textures} textures across {len(paths)} display lists')


if __name__ == '__main__':
    main(*sys.argv[1:])
