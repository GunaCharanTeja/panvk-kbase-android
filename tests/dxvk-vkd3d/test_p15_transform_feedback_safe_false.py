#!/usr/bin/env python3
import pathlib
import re
import unittest

from _mesa_tree import mesa_root

ROOT = pathlib.Path(__file__).resolve().parents[2]
MESA = mesa_root()
PHYSICAL_DEVICE = MESA / "src/panfrost/vulkan/panvk_vX_physical_device.c"
SHADER = MESA / "src/panfrost/vulkan/panvk_vX_shader.c"
QUERY = MESA / "src/panfrost/vulkan/csf/panvk_vX_cmd_query.c"
QUERY_POOL = MESA / "src/panfrost/vulkan/panvk_vX_query_pool.c"
DEFAULTS = MESA / "src/panfrost/vulkan/00-panvk-defaults.conf"
PANVK = MESA / "src/panfrost/vulkan"
COMPILER = MESA / "src/panfrost/compiler"
GALLIUM = MESA / "src/gallium/drivers/panfrost"
REMOVE_XFB = COMPILER / "pan_nir_xfb.c"


# VK_EXT_transform_feedback, transformFeedback, and geometryStreams are
# exposed on v10+. vertexPipelineStoresAndAtomics stays off on v10.
# XFB is a CSF capture path, not NIR lower-to-stores.
class TransformFeedbackSafeFalseTest(unittest.TestCase):
    def test_extension_not_advertised(self):
        source = PHYSICAL_DEVICE.read_text()
        self.assertRegex(
            source, r"\.EXT_transform_feedback\s*=\s*PAN_ARCH >= 10,"
        )
        self.assertNotRegex(source, r"\.EXT_transform_feedback\s*=\s*true,")
        self.assertNotIn("KHR_transform_feedback", source)

    def test_features_remain_disabled(self):
        source = PHYSICAL_DEVICE.read_text()
        self.assertRegex(source, r"\.transformFeedback\s*=\s*PAN_ARCH >= 10,")
        self.assertRegex(
            source,
            r"\.geometryStreams\s*=\s*PAN_ARCH >= 10 && !PANVK_DEBUG\(NO_GS\),",
        )
        self.assertRegex(
            source,
            r"\.geometryShader\s*=\s*PAN_ARCH >= 10 && !PANVK_DEBUG\(NO_GS\),",
        )
        self.assertNotRegex(source, r"\.transformFeedback\s*=\s*true,")
        self.assertNotRegex(source, r"\.geometryStreams\s*=\s*true,")
        self.assertRegex(
            source,
            r"\.vertexPipelineStoresAndAtomics\s*=\s*\n\s*\(PAN_ARCH >= 13",
        )

    def test_angle_driconf_admits_missing_extension(self):
        source = DEFAULTS.read_text()
        self.assertIn("VK_EXT_transform_feedback", source)
        self.assertIn("we currently don't support this extension", source)

    def test_panvk_has_no_xfb_entrypoints(self):
        patterns = (
            re.compile(r"CmdBindTransformFeedbackBuffers"),
            re.compile(r"CmdBeginTransformFeedback"),
            re.compile(r"CmdEndTransformFeedback"),
            re.compile(r"CmdDrawIndirectByteCount"),
        )
        matches = []
        for path in PANVK.rglob("*.[ch]"):
            text = path.read_text(errors="replace")
            for pattern in patterns:
                if pattern.search(text):
                    matches.append(f"{path.relative_to(MESA)}: {pattern.pattern}")
        self.assertEqual(
            sorted(matches),
            [
                "src/panfrost/vulkan/csf/panvk_vX_cmd_draw.c: CmdBeginTransformFeedback",
                "src/panfrost/vulkan/csf/panvk_vX_cmd_draw.c: CmdBindTransformFeedbackBuffers",
                "src/panfrost/vulkan/csf/panvk_vX_cmd_draw.c: CmdDrawIndirectByteCount",
                "src/panfrost/vulkan/csf/panvk_vX_cmd_draw.c: CmdEndTransformFeedback",
            ],
            "unexpected PanVK XFB entrypoints; reassess P15:\n" + "\n".join(matches),
        )

    def test_panvk_does_not_lower_xfb(self):
        patterns = (
            re.compile(r"nir_lower_xfb_to_stores"),
            re.compile(r"nir_io_add_intrinsic_xfb_info"),
            re.compile(r"pan_nir_remove_xfb"),
            re.compile(r"has_transform_feedback_varyings"),
        )
        matches = []
        for path in PANVK.rglob("*.[ch]"):
            text = path.read_text(errors="replace")
            for pattern in patterns:
                if pattern.search(text):
                    matches.append(f"{path.relative_to(MESA)}: {pattern.pattern}")
        self.assertEqual(
            matches,
            [],
            "PanVK now consumes XFB lowering; reassess P15:\n" + "\n".join(matches),
        )

    def test_compiler_strip_does_not_emit_stores(self):
        source = REMOVE_XFB.read_text()
        self.assertIn("pan_nir_remove_xfb", source)
        self.assertIn("has_transform_feedback_varyings = false", source)
        self.assertNotIn("nir_store_global", source)
        self.assertNotIn("nir_lower_xfb_to_stores", source)

    def test_query_path_is_todo(self):
        source = QUERY.read_text()
        self.assertEqual(source.count("TODO: transform feedback"), 0)
        self.assertIn("panvk_cmd_begin_xfb_query", source)
        self.assertIn("panvk_cmd_end_xfb_query", source)
        self.assertIn("VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT", source)
        pool = QUERY_POOL.read_text()
        self.assertIn("VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT", pool)

    def test_shader_compile_is_vs_fs_cs_only(self):
        source = SHADER.read_text()
        self.assertIn('UNREACHABLE("Unknown shader stage")', source)
        self.assertIn('assert(!"Unsupported stage")', source)

    def test_gallium_xfb_is_not_panvk(self):
        shader = (GALLIUM / "pan_shader.c").read_text()
        self.assertIn("nir_lower_xfb_to_stores", shader)
        self.assertIn("pan_nir_remove_xfb(nir)", shader)
        launch = (GALLIUM / "pan_cmdstream.c").read_text()
        self.assertIn("TODO: XFB with index buffers", launch)
        self.assertIn("panfrost_launch_xfb", launch)
        csf = (GALLIUM / "pan_csf.c").read_text()
        self.assertIn("csf_launch_xfb", csf)

    def test_gallium_has_no_geometry_streams_cap(self):
        source = (GALLIUM / "pan_screen.c").read_text()
        self.assertIn("max_stream_output_buffers = PIPE_MAX_SO_BUFFERS", source)
        self.assertNotIn("max_vertex_streams", source)
        self.assertNotIn("MESA_SHADER_GEOMETRY", source)


if __name__ == "__main__":
    unittest.main()
