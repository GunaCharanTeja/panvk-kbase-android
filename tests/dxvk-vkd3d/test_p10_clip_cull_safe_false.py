#!/usr/bin/env python3
import pathlib
import re
import unittest

from _mesa_tree import mesa_root

ROOT = pathlib.Path(__file__).resolve().parents[2]
MESA = mesa_root()
PHYSICAL_DEVICE = (
    MESA / "src/panfrost/vulkan/panvk_vX_physical_device.c"
)
PANVK = MESA / "src/panfrost/vulkan"


PROOF = ROOT / "validation/g615-v11-csf/dxvk/DX7-CLIP-CULL.md"


# DX7 (patch 023): clip/cull distance exposed on v10+ only after the
# clip_cull device draw test passed; see DX7-CLIP-CULL.md.
class ClipCullSafeFalseTest(unittest.TestCase):
    def test_features_exposed_only_with_device_proof(self):
        source = PHYSICAL_DEVICE.read_text()
        self.assertRegex(source, r"\.shaderClipDistance\s*=\s*PAN_ARCH >= 10,")
        self.assertRegex(source, r"\.shaderCullDistance\s*=\s*PAN_ARCH >= 10,")
        self.assertNotRegex(source, r"\.shader(?:Clip|Cull)Distance\s*=\s*true,")
        proof = PROOF.read_text()
        self.assertIn("CLIP_CULL_FAILS=0", proof)

    def test_panvk_lowers_clip_cull_on_gpu(self):
        shader = (PANVK / "panvk_vX_shader.c").read_text()
        self.assertIn("panvk_lower_clip_cull_vs", shader)
        self.assertIn("panvk_lower_clip_cull_fs", shader)
        self.assertNotRegex(shader, re.compile(r"nir_lower_clip_fs\s*\("))


if __name__ == "__main__":
    unittest.main()
