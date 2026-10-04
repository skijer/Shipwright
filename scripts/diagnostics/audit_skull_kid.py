"""Read-only binary-V0 O2R audit. Does not simulate the renderer or prove crash freedom.

Field order follows soh/soh/resource/importer/{Skeleton,SkeletonLimb,Animation}Factory.cpp.
Usage: python3 scripts/diagnostics/audit_skull_kid.py mm.o2r --overlay oot.o2r
Additional --overlay arguments can inspect mod archives, without assuming load priority.
"""
import argparse
import contextlib
import hashlib
import json
import struct
import zipfile

SKULL = 'objects/object_stk/gSkullKidSkel'
ANIMATIONS = ['objects/object_stk2/gSkullKidRecliningFloatAnim',
              'objects/object_stk2/gSkullKidFloatingArmsCrossedAnim']


class Reader:
    def __init__(self, data, kind):
        if len(data) < 64 or data[0] not in (0, 1):
            raise ValueError('missing/unsupported resource header')
        self.endian = '<' if data[0] == 0 else '>'
        actual, version = struct.unpack_from(self.endian + 'II', data, 4)
        if actual != kind or version != 0:
            raise ValueError(f'unsupported resource type/version {actual:08X}/{version}')
        self.data, self.offset = data, 64

    def take(self, count):
        if count < 0 or count > len(self.data) - self.offset:
            raise ValueError('truncated resource')
        result = self.data[self.offset:self.offset + count]
        self.offset += count
        return result

    def unpack(self, fmt):
        return struct.unpack(self.endian + fmt, self.take(struct.calcsize(self.endian + fmt)))

    def integer(self, fmt='I'):
        return self.unpack(fmt)[0]

    def string(self):
        return self.take(self.integer()).decode('utf-8').removeprefix('__OTR__')


def parse_skeleton(data):
    r = Reader(data, 0x4F534B4C)
    kind, limb_type, count, matrices, table_type, table_count = r.unpack('BBIIBI')
    if kind not in (0, 1) or limb_type != 1 or table_type != 1 or not 0 < count < 255:
        raise ValueError('expected standard-limb normal/flex skeleton')
    if table_count != count:
        raise ValueError('limb table/header count mismatch')
    return dict(kind=kind, count=count, matrices=matrices, paths=[r.string() for _ in range(count)])


def parse_limb(data):
    r = Reader(data, 0x4F534C42)
    kind, _ = r.unpack('BB')
    r.string()  # skin display list
    _, skin_count = r.unpack('HI')
    if kind != 1 or skin_count != 0:
        raise ValueError('expected standard limb without skin modifiers')
    r.string()  # second skin display list
    r.unpack('fffHHH')
    r.string()  # legacy child pointer
    r.string()  # legacy sibling pointer
    dl = r.string()
    r.string()  # second display list
    x, y, z, child, sibling = r.unpack('hhhBB')
    return dict(position=[x, y, z], child=child, sibling=sibling, dl=dl)


def parse_animation(data):
    r = Reader(data, 0x4F414E4D)
    if r.integer() != 0:
        raise ValueError('expected normal animation')
    frames, values = r.unpack('hI')
    r.take(values * 2)
    joint_count = r.integer()
    if joint_count > (len(data) - r.offset) // 6:
        raise ValueError('truncated joint array')
    indices = [r.unpack('HHH') for _ in range(joint_count)]
    static_max = r.integer('H')
    return dict(frames=frames, values=values, indices=indices, static_max=static_max)


def audit(read, skeleton_path=SKULL, animations=ANIMATIONS, overlays=()):
    result = dict(skeleton=skeleton_path, errors=[], conflicts=[], animations=[])
    errors = result['errors']
    checked = {}

    def get(path, parser):
        try:
            data = read(path)
            checked[path] = data
            return parser(data)
        except KeyError:
            errors.append(f'{path}: missing resource')
        except (ValueError, struct.error) as exc:
            errors.append(f'{path}: {exc}')
        return None

    skel = get(skeleton_path, parse_skeleton)
    if skel is None:
        return result
    limbs = [get(path, parse_limb) for path in skel['paths']]
    result['limbs'] = skel['count']
    result['matrix_slots_allocated'] = skel['matrices']
    result['matrix_slots_used'] = sum(bool(limb and limb['dl']) for limb in limbs)
    if skel['kind'] == 1 and result['matrix_slots_used'] > skel['matrices']:
        errors.append('flex matrix array is smaller than drawable limb count')
    if limbs[0] and limbs[0]['sibling'] != 255:
        errors.append('root sibling is ignored by the skeleton renderer')
    visited = set()

    def visit(index):
        if index == 255:
            return
        if index >= len(limbs):
            errors.append(f'limb link {index}: out of range')
            return
        if index in visited:
            errors.append(f'limb {index}: cycle or duplicate traversal')
            return
        visited.add(index)
        limb = limbs[index]
        if limb:
            visit(limb['child'])
            if index != 0:
                visit(limb['sibling'])

    visit(0)
    if len(visited) != len(limbs):
        errors.append('unreachable limbs in skeleton table')
    for path in animations:
        anim = get(path, parse_animation)
        if anim is None:
            continue
        result['animations'].append(dict(path=path, frames=anim['frames'], joints=len(anim['indices'])))
        if len(anim['indices']) < skel['count'] + 1:
            errors.append(f'{path}: joint array too short (needs translation plus every limb)')
        if anim['frames'] <= 0:
            errors.append(f'{path}: non-positive frame count')
        for joint in anim['indices'][:skel['count'] + 1]:
            for index in joint:
                last = index if index < anim['static_max'] else index + anim['frames'] - 1
                if last >= anim['values']:
                    errors.append(f'{path}: index {index} overruns frame data')
    for name, overlay_read in overlays:
        for path, data in checked.items():
            try:
                if overlay_read(path) != data:
                    result['conflicts'].append(dict(archive=name, path=path))
            except KeyError:
                pass
    result['resource_sha256'] = {path: hashlib.sha256(data).hexdigest() for path, data in checked.items()}
    result['scope'] = 'Skeleton/animation structure only; no renderer execution or display-list/texture validation.'
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mm_archive')
    parser.add_argument('--overlay', action='append', default=[])
    args = parser.parse_args()
    with contextlib.ExitStack() as stack:
        mm = stack.enter_context(zipfile.ZipFile(args.mm_archive))
        overlays = [(path, stack.enter_context(zipfile.ZipFile(path)).read) for path in args.overlay]
        result = audit(mm.read, overlays=overlays)
    print(json.dumps(result, indent=2))
    return int(bool(result['errors']))


if __name__ == '__main__':
    raise SystemExit(main())
