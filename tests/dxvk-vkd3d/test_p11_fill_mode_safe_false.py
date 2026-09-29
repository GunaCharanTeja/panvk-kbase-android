#!/usr/bin/env python3
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
MESA = ROOT / "work" / "mesa"
PHYSICAL_DEVICE = MESA / "src/panfrost/vulkan/panvk_vX_physical_device.c"
PANVK = MESA / "src/panfrost/vulkan"
VALHALL_XML = MESA / "src/panfrost/genxml/v11.xml"


PROOF = ROOT / "validation/g615-v11-csf/dxvk/DX7-FILL-MODE.md"


# DX7 (patch 025): fillModeNonSolid is exposed on v10+ through the GPU
# polygon kernel on the gpu_prerast path (no hardware polygon mode exists),
# only after the fill_mode device draw test passed. Dynamic polygon mode stays
# off because the variant is chosen at pipeline compile time.
class FillModeSafeFalseTest(unittest.TestCase):
    def test_features_remain_disabled(self):
        source = PHYSICAL_DEVICE.read_text()
        self.assertRegex(source, r"\.fillModeNonSolid\s*=\s*PAN_ARCH >= 10,")
        self.assertRegex(
            source, r"\.extendedDynamicState3PolygonMode\s*=\s*false,"
        )
        self.assertNotRegex(source, r"\.fillModeNonSolid\s*=\s*true,")
        self.assertIn("FILL_MODE_FAILS=0", PROOF.read_text())

    def test_panvk_has_no_polygon_mode_implementation(self):
        pattern = re.compile(r"polygon_mode|polygonMode", re.IGNORECASE)
        matches = []
        for path in PANVK.rglob("*.[ch]"):
            if path == PHYSICAL_DEVICE:
                continue
            source = path.read_text(errors="replace")
            if pattern.search(source):
                matches.append(str(path.relative_to(MESA)))
        self.assertEqual(
            sorted(matches),
            [
                "src/panfrost/vulkan/csf/panvk_gpu_prerast.c",
                "src/panfrost/vulkan/panvk_vX_shader.c",
            ],
            "unexpected PanVK polygon-mode path; reassess P11:\n"
            + "\n".join(matches),
        )

    def test_valhall_has_no_polygon_raster_mode_field(self):
        source = VALHALL_XML.read_text()
        self.assertNotRegex(
            source,
            re.compile(
                r'<field[^>]+name="[^"]*(?:polygon mode|fill mode)[^"]*"',
                re.IGNORECASE,
            ),
        )
        self.assertRegex(
            source,
            r'<enum name="Draw Mode">[\s\S]*<value name="Polygon" value="13"/>',
        )

    def test_gallium_panfrost_ignores_polygon_fill(self):
        pattern = re.compile(r"fill_front|fill_back|PIPE_POLYGON_MODE")
        matches = []
        gallium = MESA / "src/gallium/drivers/panfrost"
        for path in gallium.rglob("*.[ch]"):
            source = path.read_text(errors="replace")
            if pattern.search(source):
                matches.append(str(path.relative_to(MESA)))
        self.assertEqual(
            matches,
            [],
            "Gallium panfrost now consumes polygon fill; reassess P11:\n"
            + "\n".join(matches),
        )

    def test_draw_uses_input_topology_not_polygon_mode(self):
        for rel in (
            "src/panfrost/vulkan/csf/panvk_vX_cmd_draw.c",
            "src/panfrost/vulkan/jm/panvk_vX_cmd_draw.c",
        ):
            source = (MESA / rel).read_text()
            self.assertIn("translate_prim", source)
            self.assertNotRegex(source, r"rs->polygon_mode|polygonMode")


if __name__ == "__main__":
    unittest.main()
