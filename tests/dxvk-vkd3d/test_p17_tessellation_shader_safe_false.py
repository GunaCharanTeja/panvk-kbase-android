#!/usr/bin/env python3
import pathlib
import re
import unittest

from _mesa_tree import mesa_root

ROOT = pathlib.Path(__file__).resolve().parents[2]
MESA = mesa_root()
PHYSICAL_DEVICE = MESA / "src/panfrost/vulkan/panvk_vX_physical_device.c"
SHADER = MESA / "src/panfrost/vulkan/panvk_vX_shader.c"
SHADER_H = MESA / "src/panfrost/vulkan/panvk_shader.h"
VALHALL_XML = MESA / "src/panfrost/genxml/v11.xml"
V12_XML = MESA / "src/panfrost/genxml/v12.xml"
PANVK = MESA / "src/panfrost/vulkan"
COMPILER = MESA / "src/panfrost/compiler"
GALLIUM = MESA / "src/gallium/drivers/panfrost"
PAN_SHADER_H = MESA / "src/panfrost/lib/pan_shader.h"


# tessellationShader is exposed on v10+. TCS/TES run as compute on the
# gpu_prerast path. multiviewTessellationShader stays false. Valhall v11
# has no tess hardware stage; v12 IDVS names stay v12-only.
class TessellationShaderSafeFalseTest(unittest.TestCase):
    def test_features_remain_disabled(self):
        source = PHYSICAL_DEVICE.read_text()
        self.assertRegex(source, r"\.tessellationShader\s*=\s*PAN_ARCH >= 10,")
        self.assertRegex(
            source,
            r"\.shaderTessellationAndGeometryPointSize\s*=\s*PAN_ARCH >= 10,",
        )
        self.assertRegex(source, r"\.multiviewTessellationShader\s*=\s*false,")
        self.assertNotRegex(source, r"\.tessellationShader\s*=\s*true,")
        self.assertNotRegex(source, r"Tesselation shaders not supported\.")
        self.assertRegex(
            source, r"\.maxTessellationGenerationLevel\s*=\s*PAN_ARCH >= 10 \? 64 : 0,"
        )
        self.assertRegex(
            source, r"\.maxTessellationPatchSize\s*=\s*PAN_ARCH >= 10 \? 32 : 0,"
        )
        self.assertRegex(
            source,
            r"\.maxTessellationControlPerVertexInputComponents\s*=\s*PAN_ARCH >= 10 \? 128 : 0,",
        )
        self.assertRegex(
            source,
            r"\.maxTessellationControlPerVertexOutputComponents\s*=\s*PAN_ARCH >= 10 \? 128 : 0,",
        )
        self.assertRegex(
            source,
            r"\.maxTessellationControlPerPatchOutputComponents\s*=\s*PAN_ARCH >= 10 \? 120 : 0,",
        )
        self.assertRegex(
            source,
            r"\.maxTessellationControlTotalOutputComponents\s*=\s*PAN_ARCH >= 10 \? 4096 : 0,",
        )
        self.assertRegex(
            source,
            r"\.maxTessellationEvaluationInputComponents\s*=\s*PAN_ARCH >= 10 \? 128 : 0,",
        )
        self.assertRegex(
            source,
            r"\.maxTessellationEvaluationOutputComponents\s*=\s*PAN_ARCH >= 10 \? 128 : 0,",
        )

    def test_panvk_compile_rejects_tess_stage(self):
        source = SHADER.read_text()
        self.assertIn("case MESA_SHADER_TESS_CTRL:", source)
        self.assertIn("case MESA_SHADER_TESS_EVAL:", source)
        self.assertIn("poly_nir_lower_tcs", source)
        self.assertIn("poly_nir_lower_tes", source)
        self.assertIn("panvk_lower_tes", source)
        self.assertIn('UNREACHABLE("Unknown shader stage")', source)
        self.assertIn('assert(!"Unsupported stage")', source)

    def test_vs_variant_is_hw_only(self):
        source = SHADER_H.read_text()
        self.assertIn("PANVK_VS_VARIANT_HW", source)
        self.assertIn("PANVK_VS_VARIANT_GPU_LOWERED", source)
        self.assertIn("MESA_SHADER_TESS_EVAL", source)
        self.assertNotIn("PANVK_VS_VARIANT_TCS", source)
        self.assertNotIn("PANVK_VS_VARIANT_TES", source)

    def test_panvk_has_no_tess_lowering(self):
        patterns = (
            re.compile(r"poly_nir_lower_tcs\s*\("),
            re.compile(r"poly_nir_lower_tes\s*\("),
            re.compile(r"nir_create_passthrough_tcs"),
            re.compile(r"MESA_SHADER_TESS_CTRL"),
            re.compile(r"MESA_SHADER_TESS_EVAL"),
            re.compile(r"VK_PRIMITIVE_TOPOLOGY_PATCH"),
        )
        matches = []
        for path in PANVK.rglob("*.[ch]"):
            source = path.read_text(errors="replace")
            for pattern in patterns:
                if pattern.search(source):
                    matches.append(f"{path.relative_to(MESA)}: {pattern.pattern}")
        self.assertEqual(
            sorted(matches),
            [
                "src/panfrost/vulkan/panvk_shader.h: MESA_SHADER_TESS_EVAL",
                "src/panfrost/vulkan/panvk_vX_nir_lower_descriptors.c: MESA_SHADER_TESS_CTRL",
                "src/panfrost/vulkan/panvk_vX_shader.c: MESA_SHADER_TESS_CTRL",
                "src/panfrost/vulkan/panvk_vX_shader.c: MESA_SHADER_TESS_EVAL",
                "src/panfrost/vulkan/panvk_vX_shader.c: poly_nir_lower_tes\\s*\\(",
            ],
            "unexpected PanVK tess path; reassess P17:\n" + "\n".join(matches),
        )

    def test_compiler_has_no_tess_stage_implementation(self):
        allowed = {
            "compiler/kraid/compile.rs": re.compile(
                r"support_indirect_inputs: \(1 << MESA_SHADER_TESS_CTRL\)"
            ),
        }
        matches = []
        for path in COMPILER.rglob("*.[chrs]"):
            source = path.read_text(errors="replace")
            if not re.search(r"MESA_SHADER_TESS_(CTRL|EVAL)", source):
                continue
            rel = str(path.relative_to(MESA))
            if rel in allowed and allowed[rel].search(source):
                leftover = re.sub(
                    r"support_indirect_inputs: \(1 << MESA_SHADER_TESS_CTRL\)\s*"
                    r"\n\s*\|\s*\(1 << MESA_SHADER_TESS_EVAL\)",
                    "",
                    source,
                )
                if re.search(r"MESA_SHADER_TESS_(CTRL|EVAL)", leftover):
                    matches.append(rel + ": extra TESS use beyond NIR option bits")
                continue
            matches.append(rel)
        self.assertEqual(
            matches,
            [],
            "compiler now implements tess stages; reassess P17:\n"
            + "\n".join(matches),
        )

    def test_pan_shader_stage_maps_unknown_to_compute(self):
        source = PAN_SHADER_H.read_text()
        self.assertIn("case MESA_SHADER_VERTEX:", source)
        self.assertIn("case MESA_SHADER_FRAGMENT:", source)
        self.assertIn("return MALI_SHADER_STAGE_COMPUTE;", source)
        self.assertNotIn("MESA_SHADER_TESS_CTRL", source)
        self.assertNotIn("MESA_SHADER_TESS_EVAL", source)

    def test_valhall_v11_shader_stage_has_no_tess(self):
        source = VALHALL_XML.read_text()
        match = re.search(r'<enum name="Shader stage">([\s\S]*?)</enum>', source)
        self.assertIsNotNone(match)
        block = match.group(1)
        self.assertIn('<value name="Compute" value="1"/>', block)
        self.assertIn('<value name="Fragment" value="2"/>', block)
        self.assertIn('<value name="Vertex" value="3"/>', block)
        self.assertNotIn("Tess", block)
        self.assertIsNone(
            re.search(r'<enum name="IDVS Pipeline Stage">', source),
            "v11 now has IDVS Pipeline Stage; reassess P17",
        )
        draw = re.search(r'<enum name="Draw Mode">([\s\S]*?)</enum>', source)
        self.assertIsNotNone(draw)
        self.assertNotIn("Patch", draw.group(1))

    def test_v12_idvs_tess_is_not_v11(self):
        source = V12_XML.read_text()
        match = re.search(
            r'<enum name="IDVS Pipeline Stage">([\s\S]*?)</enum>', source
        )
        self.assertIsNotNone(match)
        block = match.group(1)
        self.assertIn('<value name="Tess Control" value="3"/>', block)
        self.assertIn('<value name="Tess Eval" value="4"/>', block)
        v11 = VALHALL_XML.read_text()
        self.assertNotIn("Tess Control", v11)
        self.assertNotIn("Tess Eval", v11)

    def test_gallium_panfrost_skips_tess_caps(self):
        source = (GALLIUM / "pan_screen.c").read_text()
        self.assertIn("case MESA_SHADER_VERTEX:", source)
        self.assertIn("case MESA_SHADER_FRAGMENT:", source)
        self.assertIn("case MESA_SHADER_COMPUTE:", source)
        self.assertNotIn("MESA_SHADER_TESS_CTRL", source)
        self.assertNotIn("MESA_SHADER_TESS_EVAL", source)

    def test_poly_tess_not_wired_into_panfrost(self):
        pattern = re.compile(r"poly_nir_lower_tcs|poly_nir_lower_tes|poly_tess_")
        matches = []
        for root in (PANVK, GALLIUM, COMPILER):
            for path in root.rglob("*.[ch]"):
                if pattern.search(path.read_text(errors="replace")):
                    matches.append(str(path.relative_to(MESA)))
        self.assertEqual(
            sorted(matches),
            [
                "src/panfrost/vulkan/csf/panvk_vX_cmd_draw.c",
                "src/panfrost/vulkan/panvk_gpu_prerast.h",
                "src/panfrost/vulkan/panvk_vX_shader.c",
            ],
            "unexpected poly tess wiring; reassess P17:\n" + "\n".join(matches),
        )


if __name__ == "__main__":
    unittest.main()
