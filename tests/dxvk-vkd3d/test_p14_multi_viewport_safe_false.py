#!/usr/bin/env python3
import pathlib
import re
import unittest

from _mesa_tree import mesa_root

ROOT = pathlib.Path(__file__).resolve().parents[2]
MESA = mesa_root()
PHYSICAL_DEVICE = MESA / "src/panfrost/vulkan/panvk_vX_physical_device.c"
PANVK = MESA / "src/panfrost/vulkan"
COMPILER = MESA / "src/panfrost/compiler"
VALHALL_XML = MESA / "src/panfrost/genxml/v11.xml"
V12_XML = MESA / "src/panfrost/genxml/v12.xml"
COLLECT = MESA / "src/panfrost/compiler/pan_nir_collect_varyings.c"
VALHALL_H = MESA / "src/panfrost/compiler/bifrost/valhall/valhall.h"
SHADER_H = MESA / "src/panfrost/vulkan/panvk_shader.h"


PROOF = ROOT / "validation/g615-v11-csf/dxvk/DX7-MULTIVIEWPORT.md"


# DX7 (patch 024): multiViewport is exposed on v10+ because no exposed stage
# can write ViewportIndex, so viewport/scissor 0 is always correct; proven by
# the multi_viewport device draw test. ViewportIndex output stays off.
class MultiViewportSafeFalseTest(unittest.TestCase):
    def test_features_remain_disabled(self):
        source = PHYSICAL_DEVICE.read_text()
        self.assertRegex(source, r"\.multiViewport\s*=\s*PAN_ARCH >= 10,")
        self.assertRegex(source, r"\.shaderOutputViewportIndex\s*=\s*false,")
        self.assertRegex(source, r"\.maxViewports\s*=\s*PAN_ARCH >= 10 \? 16 : 1,")
        self.assertNotRegex(source, r"\.multiViewport\s*=\s*true,")
        self.assertNotRegex(source, r"\.shaderOutputViewportIndex\s*=\s*true,")
        self.assertIn("MULTI_VIEWPORT_FAILS=0", PROOF.read_text())

    def test_not_confused_with_multiview(self):
        source = PHYSICAL_DEVICE.read_text()
        self.assertRegex(source, r"\.multiViewport\s*=\s*PAN_ARCH >= 10,")
        self.assertRegex(source, r"\.multiview\s*=\s*true,")
        self.assertRegex(source, r"\.maxMultiviewViewCount\s*=")

    def test_csf_and_jm_program_viewport_zero_only(self):
        for rel in (
            "src/panfrost/vulkan/csf/panvk_vX_cmd_draw.c",
            "src/panfrost/vulkan/jm/panvk_vX_cmd_draw.c",
            "src/panfrost/vulkan/panvk_vX_cmd_draw.c",
        ):
            source = (MESA / rel).read_text()
            self.assertIn("viewports[0]", source)
            self.assertNotRegex(source, r"viewports\[[1-9]\]")
            self.assertNotRegex(source, r"scissors\[[1-9]\]")

    def test_sysvals_are_single_viewport(self):
        source = SHADER_H.read_text()
        self.assertIn("} scale, offset;", source)
        self.assertNotRegex(source, r"viewport\s*\[[0-9]+\]")
        self.assertNotRegex(source, r"scale\s*\[[0-9]+\]")

    def test_scissor_array_never_enabled(self):
        pattern = re.compile(r"scissor_array_enable\s*=")
        matches = []
        for path in PANVK.rglob("*.[ch]"):
            source = path.read_text(errors="replace")
            if pattern.search(source):
                matches.append(str(path.relative_to(MESA)))
        self.assertEqual(
            matches,
            [],
            "scissor_array_enable now assigned; reassess P14:\n"
            + "\n".join(matches),
        )

    def test_valhall_v11_has_single_scissor_no_viewport_sr(self):
        source = VALHALL_XML.read_text()
        self.assertIn('<value name="SCISSOR_BOX" value="42"/>', source)
        self.assertIn('<value name="LOW_DEPTH_CLAMP" value="44"/>', source)
        self.assertIn('<value name="HIGH_DEPTH_CLAMP" value="45"/>', source)
        self.assertNotIn("VIEWPORT_HIGH", source)
        self.assertNotIn("VIEWPORT_LOW", source)
        self.assertIn(
            '<field name="Scissor array enable" size="1" start="0:21" type="bool"/>',
            source,
        )
        v12 = V12_XML.read_text()
        self.assertIn('<value name="VIEWPORT_HIGH" value="44"/>', v12)
        self.assertIn('<value name="VIEWPORT_LOW" value="46"/>', v12)

    def test_compiler_viewport_slot_collides_with_layer(self):
        source = COLLECT.read_text()
        layer = re.search(
            r"\.location = VARYING_SLOT_LAYER,.*?\.offset = (\d+),",
            source,
            re.S,
        )
        viewport = re.search(
            r"\.location = VARYING_SLOT_VIEWPORT,.*?\.offset = (\d+),",
            source,
            re.S,
        )
        self.assertIsNotNone(layer)
        self.assertIsNotNone(viewport)
        self.assertEqual(layer.group(1), viewport.group(1))
        self.assertEqual(layer.group(1), "2")
        valhall = VALHALL_H.read_text()
        self.assertIn("case VARYING_SLOT_LAYER:", valhall)
        self.assertNotIn("VARYING_SLOT_VIEWPORT", valhall)

    def test_compiler_has_no_viewport_index_path(self):
        pattern = re.compile(r"viewport_index|ViewportIndex", re.IGNORECASE)
        matches = []
        for path in COMPILER.rglob("*.[ch]"):
            source = path.read_text(errors="replace")
            if pattern.search(source):
                matches.append(str(path.relative_to(MESA)))
        self.assertEqual(
            matches,
            [],
            "compiler now names ViewportIndex; reassess P14:\n"
            + "\n".join(matches),
        )

    def test_draw_enables_layer_not_viewport_index(self):
        source = (
            MESA / "src/panfrost/vulkan/csf/panvk_vX_cmd_draw.c"
        ).read_text()
        self.assertIn("cfg.layer_index_enable = writes_layer;", source)
        self.assertNotRegex(source, r"viewport_index_enable")
        self.assertIn("VARYING_BIT_LAYER", source)
        self.assertNotIn("VARYING_BIT_VIEWPORT", source)


if __name__ == "__main__":
    unittest.main()
