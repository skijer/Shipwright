import struct
import unittest
from audit_skull_kid import audit, parse_animation, parse_limb, parse_skeleton


def resource(kind, body):
    header = bytearray(64)
    struct.pack_into('<II', header, 4, kind, 0)
    return bytes(header) + body


def string(value):
    value = value.encode()
    return struct.pack('<I', len(value)) + value


def skeleton(paths, matrices=1):
    return resource(0x4F534B4C, struct.pack('<BBIIBI', 1, 1, len(paths), matrices, 1, len(paths))
                    + b''.join(map(string, paths)))


def limb(child=255, sibling=255, dl='mesh'):
    # Standard-limb binary V0 fields, in SkeletonLimbFactory.cpp read order.
    body = struct.pack('<BB', 1, 0) + string('') + struct.pack('<HI', 0, 0)
    body += string('') + struct.pack('<fffHHH', 0, 0, 0, 0, 0, 0)
    body += string('') + string('') + string(dl) + string('')
    body += struct.pack('<hhhBB', 0, 0, 0, child, sibling)
    return resource(0x4F534C42, body)


def animation(joints=3, frames=2, index=0):
    body = struct.pack('<IhI', 0, frames, 2) + struct.pack('<HH', 0, 1)
    body += struct.pack('<I', joints) + struct.pack('<HHH', index, 0, 0) * joints
    body += struct.pack('<h', 1)
    return resource(0x4F414E4D, body)


class AuditTests(unittest.TestCase):
    def setUp(self):
        self.assets = {'skel': skeleton(['root', 'child']),
                       'root': limb(child=1, dl=''), 'child': limb(), 'anim': animation()}

    def inspect(self, **kwargs):
        return audit(self.assets.__getitem__, 'skel', ['anim'], **kwargs)

    def test_valid_tree_and_animation(self):
        result = self.inspect()
        self.assertEqual(result['errors'], [])
        self.assertEqual(result['limbs'], 2)
        self.assertEqual(result['matrix_slots_used'], 1)

    def test_cycle_is_reported_without_recursing_forever(self):
        self.assets['child'] = limb(child=0)
        self.assertTrue(any('cycle' in e for e in self.inspect()['errors']))

    def test_out_of_range_link_is_reported(self):
        self.assets['child'] = limb(sibling=8)
        self.assertTrue(any('out of range' in e for e in self.inspect()['errors']))

    def test_missing_limb_is_not_treated_as_complete(self):
        del self.assets['child']
        self.assertTrue(any('missing' in e for e in self.inspect()['errors']))

    def test_flex_matrix_underallocation(self):
        self.assets['skel'] = skeleton(['root', 'child'], matrices=0)
        self.assertTrue(any('matrix' in e for e in self.inspect()['errors']))

    def test_animation_joint_array_too_short(self):
        self.assets['anim'] = animation(joints=2)
        self.assertTrue(any('joint' in e for e in self.inspect()['errors']))

    def test_animation_dynamic_index_runs_past_frame_data(self):
        self.assets['anim'] = animation(index=1)
        self.assertTrue(any('frame data' in e for e in self.inspect()['errors']))

    def test_same_name_different_limb_is_reported_as_risk_not_proven_crash(self):
        overlay = dict(self.assets, child=limb(sibling=0))
        result = self.inspect(overlays=[('mod.o2r', overlay.__getitem__)])
        self.assertEqual(result['errors'], [])
        self.assertEqual(result['conflicts'], [{'archive': 'mod.o2r', 'path': 'child'}])

    def test_equal_overlay_is_not_a_conflict(self):
        self.assertEqual(self.inspect(overlays=[('same.o2r', self.assets.__getitem__)])['conflicts'], [])

    def test_truncated_resource_rejected(self):
        for parser, data in [(parse_skeleton, self.assets['skel']),
                             (parse_limb, self.assets['child']),
                             (parse_animation, self.assets['anim'])]:
            with self.subTest(parser=parser.__name__):
                with self.assertRaises(ValueError):
                    parser(data[:-1])


if __name__ == '__main__':
    unittest.main()
