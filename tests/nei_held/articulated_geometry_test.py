"""Geometry contracts for the articulated held models, independent of the game."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
import xml.etree.ElementTree as ET

import numpy as np

REPO = Path(__file__).resolve().parents[2]
BUILDER = REPO / "tools/nei_held/SOURCE/articulated.py"


def rendered(model, part):
    # Interpret the same vertex quantization and pushed resource matrix that
    # the game consumes; assertions below use independent world dimensions.
    return part["p"] * 16 * model.native_scale * model.draw_scale


class ArticulatedGeometryTests(unittest.TestCase):
    def models(self):
        self.assertTrue(BUILDER.is_file(), "articulated model builder is not implemented")
        if not hasattr(self.__class__, "built"):
            spec = importlib.util.spec_from_file_location("articulated", BUILDER)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            self.__class__.builder = module
            self.__class__.built = module.build()
        return self.__class__.built

    def test_switch_tip_rejoins_docked_model_at_native_actor_socket(self):
        models = self.models()
        whole = models["switch_hook"]
        body = models["switch_hook_body"]
        tip = models["switch_hook_tip"]
        self.assertEqual(sum(len(p["tri"]) for p in whole.parts), 3184)
        self.assertEqual(sum(len(p["tri"]) for p in body.parts), 1796)
        self.assertEqual(sum(len(p["tri"]) for p in tip.parts), 1388)
        # Native Rz(-90) Ry(-90) maps actor XYZ to hand YZX. The tip's
        # origin must meet the actual held-actor socket, not the grip origin.
        actor_to_hand = np.array([[0, 1, 0], [0, 0, 1], [1, 0, 0]])
        for docked, moving in zip(whole.parts[len(body.parts):], tip.parts):
            joined = rendered(tip, moving) @ actor_to_hand.T + [1, 16.4, 0]
            np.testing.assert_allclose(joined, rendered(whole, docked), atol=0.01)
            np.testing.assert_array_equal(moving["tri"], docked["tri"])
        np.testing.assert_allclose(whole.markers["grip_world"], [0, 0, 0], atol=0.01)
        np.testing.assert_allclose(whole.markers["dock_world"], [1, 16.4, 0], atol=0.01)

    def test_whip_active_head_keeps_eyes_and_faces_along_positive_z(self):
        models = self.models()
        tip = models["whip_tip"]
        self.assertEqual(sum(len(p["tri"]) for p in tip.parts), 220)
        self.assertEqual({p["mat"] for p in tip.parts}, {"orange_tip", "green_eye", "black_detail"})
        vertices = np.concatenate([rendered(tip, p) for p in tip.parts])
        np.testing.assert_allclose(vertices.min(axis=0), [-1.5, -0.9, -0.6], atol=0.07)
        np.testing.assert_allclose(vertices.max(axis=0), [1.5, 1.5, 5.4], atol=0.07)
        # Nose was at -X in the approved coil; it must lead the moving rope.
        head = rendered(tip, tip.parts[0])
        nose = head[head[:, 2] > 5.2]
        self.assertGreater(len(nose), 0)
        self.assertLess(np.ptp(nose[:, 0]), 0.8)

    def test_whip_handle_and_coil_share_grip_and_rope_socket(self):
        models = self.models()
        coil = models["whip"]
        handle = models["whip_handle"]
        self.assertEqual(sum(len(p["tri"]) for p in coil.parts), 3410)
        self.assertEqual(sum(len(p["tri"]) for p in handle.parts), 560)
        for grip, coiled_grip in zip(handle.parts, coil.parts):
            np.testing.assert_array_equal(grip["p"], coiled_grip["p"])
            np.testing.assert_array_equal(grip["tri"], coiled_grip["tri"])
        np.testing.assert_allclose(handle.markers["rope_socket_world"], [0, 7.2, 0], atol=0.001)
        self.assertEqual({p["mat"] for p in handle.parts}, {"purple_wrap", "dark_ferrule"})

    def test_active_braid_is_a_straight_repeatable_segment(self):
        segment = self.models()["whip_segment"]
        self.assertEqual({p["mat"] for p in segment.parts}, {"orange_braid"})
        vertices = np.concatenate([rendered(segment, p) for p in segment.parts])
        np.testing.assert_allclose(vertices[:, 2].min(), -7.5, atol=0.001)
        np.testing.assert_allclose(vertices[:, 2].max(), 7.5, atol=0.001)
        self.assertLessEqual(np.linalg.norm(vertices[:, :2], axis=1).max(), 1.25)
        self.assertLess(sum(len(p["tri"]) for p in segment.parts), 100)

    def test_exported_component_dependencies_and_scales_are_self_contained(self):
        models = self.models()
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            self.builder.export_models(models, output, checkpoints=False)
            for model in models.values():
                # Stored coordinates retain 1/256-world-unit precision. The
                # resource matrix cancels each existing caller scale once.
                self.assertAlmostEqual(model.draw_scale * model.native_scale * 256, 1, places=4)
                entry = output / "RESOURCES" / model.entry
                self.assertTrue(entry.is_file())
                root = ET.parse(entry).getroot()
                for command in root:
                    path = command.attrib.get("Path")
                    if path:
                        self.assertTrue(path.startswith("objects/nei_held_redesign/"), path)
                        self.assertTrue((output / "RESOURCES" / path).is_file(), path)
                self.assertEqual(sum(c.tag == "Matrix" for c in root), 1)
                self.assertEqual(sum(c.tag == "PopMatrix" for c in root), 1)


if __name__ == "__main__":
    unittest.main()
