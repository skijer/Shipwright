#!/usr/bin/env python3
"""Run actual viewer pose behavior with the real engine's angle smoothing."""
import argparse
import ctypes
import math
import re
import struct
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def function(source, name):
    match = re.search(r'^[^\n;{}]*\b' + re.escape(name) + r'\([^;{}]*\)\s*\{', source, re.M)
    if match is None:
        raise RuntimeError(f'Missing production function: {name}')
    end, depth = match.end(), 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


def run(*args):
    subprocess.run([str(arg) for arg in args], cwd=ROOT, check=True)


def verify_grounding(library):
    assert hasattr(library, 'StaticStoryMm_GetShapeYOffset'), 'Missing pose-specific Y anchor'
    get_type = library.StaticStoryActor_GetType
    get_type.argtypes = [ctypes.c_int16]
    get_type.restype = ctypes.c_int
    skull_kid = get_type(0x7E08)
    shape_y = library.StaticStoryMm_GetShapeYOffset
    shape_y.argtypes = [ctypes.c_int, ctypes.c_uint8]
    shape_y.restype = ctypes.c_float
    hover = library.StaticStoryMm_ComposeHoverY
    hover.argtypes = [ctypes.c_float, ctypes.c_uint16]
    hover.restype = ctypes.c_float
    # Independent witnesses from native MM and the encoded 3DS replacement.
    native_foot_min = -2550.3546125114976
    replacement_foot_min = -2537.3810622
    for phase in range(0, 65536, 256):
        for foot_min in [native_foot_min, replacement_foot_min]:
            world_foot_y = hover(345, phase) + (shape_y(skull_kid, 1) + foot_min) * 0.01
            assert world_foot_y >= 345, (phase, world_foot_y)
        # Reclining keeps exactly the original hover placement, even at its trough.
        assert shape_y(skull_kid, 0) == 0
        assert abs(hover(345, phase) - (345 + 10 * math.sin(phase * math.tau / 65536))) < 0.0001
    trough_clearance = hover(345, 0xC000) + (shape_y(skull_kid, 1) + native_foot_min) * 0.01 - 345
    assert 0 <= trough_clearance < 0.01, trough_clearance
    assert shape_y(skull_kid, 2) == 0 and shape_y(skull_kid, 255) == 0
    for actor_type in range(skull_kid + 5):
        if actor_type != skull_kid:
            for pose in range(4):
                assert shape_y(actor_type, pose) == 0
    print('PASS Skull Kid grounding: native and encoded 3DS feet, full hover phase, reclining/other poses unchanged')


def multiply(a, b):
    return [[sum(a[row][i] * b[i][column] for i in range(4)) for column in range(4)] for row in range(4)]


def animation(data):
    kind, count, words = struct.unpack_from('<IhI', data, 64)
    assert kind == 0
    values = struct.unpack_from(f'<{words}h', data, 74)
    offset = 74 + words * 2
    joints, = struct.unpack_from('<I', data, offset)
    indices = list(struct.iter_unpack('<HHH', data[offset + 4:offset + 4 + joints * 6]))
    static, = struct.unpack_from('<H', data, offset + 4 + joints * 6)
    return [[tuple(values[index + (frame if index >= static else 0)] for index in joint)
             for joint in indices] for frame in range(count)]


class Rig:
    def __init__(self, resources, skeleton):
        data, offset, self.names = resources[skeleton], 79, []
        while offset < len(data):
            size, = struct.unpack_from('<I', data, offset)
            offset += 4
            self.names.append(data[offset:offset + size].decode())
            offset += size
        self.offsets, self.children, self.siblings, self.dlists = [], [], [], []
        for name in self.names:
            limb = resources[name]
            x, y, z, child, sibling = struct.unpack_from('<hhhBB', limb, len(limb) - 8)
            self.offsets.append((x, y, z))
            self.children.append(child)
            self.siblings.append(sibling)
            size, = struct.unpack_from('<I', limb, 106)
            self.dlists.append(limb[110:110 + size].decode())
        self.parents = [-1] * len(self.names)

        def visit(limb, parent):
            self.parents[limb] = parent
            if self.children[limb] != 255:
                visit(self.children[limb], limb)
            if self.siblings[limb] != 255:
                visit(self.siblings[limb], parent)
        visit(0, -1)

    def world(self, frame):
        result = []
        for limb, (x, y, z) in enumerate(frame[1:]):
            sx, sy, sz = [math.sin(angle * math.pi / 32768) for angle in (x, y, z)]
            cx, cy, cz = [math.cos(angle * math.pi / 32768) for angle in (x, y, z)]
            tx, ty, tz = self.offsets[limb] if limb else frame[0]
            local = [[cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx, tx],
                     [sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx, ty],
                     [-sy, cy * sx, cy * cx, tz], [0, 0, 0, 1]]
            parent = self.parents[limb]
            assert parent < limb
            result.append(multiply(result[parent], local) if parent >= 0 else local)
        return result


def verify_archive(library, path):
    # Decode the unmodified donor independently of the converter and evaluate
    # the actual foot-list matrix commands, including vertices weighted to a shin.
    sys.path.insert(0, str(ROOT / 'scripts'))
    from bind_mm_fountain_animation import crc64
    with zipfile.ZipFile(path) as archive:
        resources = {name: archive.read(name) for name in archive.namelist()
                     if name.startswith(('objects/object_stk/', 'objects/object_stk2/', 'objects/object_zov/'))}
    hashes = {crc64(name): name for name in resources}
    skull = Rig(resources, 'objects/object_stk/gSkullKidSkel')
    matrix_limbs = [limb for limb, display_list in enumerate(skull.dlists) if display_list]
    vertices = []
    for limb in [4, 7]:
        data, offset, joint = resources[skull.dlists[limb]], 72, limb
        while offset < len(data):
            w0, w1 = struct.unpack_from('<II', data, offset)
            offset += 8
            opcode = w0 >> 24
            if opcode in (0x20, 0x31, 0x32, 0x33, 0x35, 0x36, 0x37):
                hi, lo = struct.unpack_from('<II', data, offset)
                offset += 8
                key = hi << 32 | lo
            if opcode == 0xDA:
                assert w1 >> 24 == 0x0D
                joint = matrix_limbs[(w1 & 0xFFFFFF) // 64]
            elif opcode == 0x32:
                array = resources[hashes[key]]
                count = (w0 >> 12) & 255
                assert 72 + w1 + count * 16 <= len(array)
                for index in range(count):
                    vertices.append((joint, (*struct.unpack_from('<hhh', array, 72 + w1 + index * 16), 1)))
            elif opcode == 0xDF:
                break
    skull_type = library.StaticStoryActor_GetType(0x7E08)
    for pose, name in enumerate(['gSkullKidRecliningFloatAnim', 'gSkullKidFloatingArmsCrossedAnim']):
        frames = animation(resources['objects/object_stk2/' + name])
        minimum = min(sum(matrix[joint][1][i] * vertex[i] for i in range(4))
                      for frame in frames for matrix in [skull.world(frame)] for joint, vertex in vertices)
        if pose == 1:
            clearance = (minimum + library.StaticStoryMm_GetShapeYOffset(skull_type, pose)) * 0.01 - 10
            assert 0 <= clearance < 0.01, (name, minimum, clearance)
        else:
            assert minimum > 0 and library.StaticStoryMm_GetShapeYOffset(skull_type, pose) == 0
        print(f'PASS donor {name}: {len(frames)} frames, minimum foot Y {minimum:.7f}')

    lulu = Rig(resources, 'objects/object_zov/gLuluSkel')
    yaw = library.StaticStoryMm_GetModelYawOffset
    yaw.argtypes = [ctypes.c_int, ctypes.c_uint8]
    yaw.restype = ctypes.c_int16
    correction = yaw(library.StaticStoryActor_GetType(0x7E1C), 1) * math.pi / 32768
    for frame in animation(resources['objects/object_zov/gLuluLookLeftLoopAnim']):
        assert frame[1] == (0, 0, 0), 'Body turn was incorrectly attributed to root yaw'
        matrix = lulu.world(frame)
        hips = [matrix[2][i][3] - matrix[6][i][3] for i in range(3)]
        heading = math.atan2(-hips[2], hips[0])
        assert abs(heading + correction) < 0.001, heading + correction
    print('PASS donor Lulu: corrected pelvis facing across all 30 look-left frames')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mm-archive', type=Path, help='Also verify real native foot and pelvis animation bounds')
    arguments = parser.parse_args()
    viewer = (ROOT / 'soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c').read_text()
    math_source = (ROOT / 'soh/src/code/z_lib.c').read_text()
    fixture = (ROOT / 'soh/tests/static_story_mm_pose_behavior_test.c').read_text()
    fixture = fixture.replace('/* PRODUCTION_POSE_FUNCTIONS */',
                              function(math_source, 'Math_SmoothStepToS') +
                              function(viewer, 'EnViewerStatic_UpdateTracking') +
                              function(viewer, 'EnViewer_DrawStaticMmActor'))
    flags = ['-std=gnu11', '-DF3DEX_GBI_2', '-DLOG_LEVEL_GAME_PRINTS=6', '-DNDEBUG',
             '-Ilibultraship/include', '-Isoh/include', '-Isoh/src', '-Isoh/assets', '-Isoh', '-Isoh/mods']
    with tempfile.TemporaryDirectory(prefix='mm-pose-behavior-') as directory:
        output = Path(directory)
        source = output / 'pose_behavior.c'
        source.write_text(fixture)
        run('cc', *flags, source,
            ROOT / 'soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c',
            ROOT / 'soh/src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.c',
            '-lm', '-o', output / 'pose_behavior')
        run(output / 'pose_behavior')
        run('cc', '-std=c11', '-shared', '-fPIC', '-Isoh/include',
            ROOT / 'soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c',
            ROOT / 'soh/src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.c',
            '-lm', '-o', output / 'pose_helpers.so')
        library = ctypes.CDLL(str(output / 'pose_helpers.so'))
        verify_grounding(library)
        if arguments.mm_archive:
            verify_archive(library, arguments.mm_archive)


if __name__ == '__main__':
    main()
