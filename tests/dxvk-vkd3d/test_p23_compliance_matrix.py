#!/usr/bin/env python3
import importlib.util
import json
import pathlib
import subprocess
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/evaluate-dxvk-vkd3d-compliance-matrix.py"
JSON_OUT = ROOT / "validation/g615-v11-csf/p23-dxvk-vkd3d-compliance-matrix.json"
MD_OUT = ROOT / "validation/g615-v11-csf/P23-DXVK-VKD3D-COMPLIANCE-MATRIX.md"
CAPS = ROOT / "validation/g615-v11-csf/consumer-capabilities.json"
PHYSICAL = ROOT / "work/mesa/src/panfrost/vulkan/panvk_vX_physical_device.c"
P13 = ROOT / "validation/g615-v11-csf/p13-d3d9.json"
P16 = ROOT / "validation/g615-v11-csf/p16-d3d10.json"
P18 = ROOT / "validation/g615-v11-csf/p18-d3d11-fl11.json"
P19 = ROOT / "validation/g615-v11-csf/p19-vkd3d-profile-baseline.json"
P21 = ROOT / "validation/g615-v11-csf/p21-d3d12-feature-level.json"
FALSE_BITS = (
    "geometryShader",
    "tessellationShader",
    "fillModeNonSolid",
    "multiViewport",
    "shaderClipDistance",
    "shaderCullDistance",
    "textureCompressionBC",
    "pipelineStatisticsQuery",
    "sparseBinding",
    "transformFeedback",
    "geometryStreams",
    "robustImageAccess2",
    "vertexPipelineStoresAndAtomics",
)
INDEXING_BITS = (
    "descriptorIndexing",
    "shaderInputAttachmentArrayDynamicIndexing",
    "shaderUniformTexelBufferArrayDynamicIndexing",
    "shaderStorageTexelBufferArrayDynamicIndexing",
    "shaderUniformBufferArrayNonUniformIndexing",
    "shaderSampledImageArrayNonUniformIndexing",
    "shaderStorageBufferArrayNonUniformIndexing",
    "shaderStorageImageArrayNonUniformIndexing",
    "shaderInputAttachmentArrayNonUniformIndexing",
    "shaderUniformTexelBufferArrayNonUniformIndexing",
    "shaderStorageTexelBufferArrayNonUniformIndexing",
    "descriptorBindingSampledImageUpdateAfterBind",
    "descriptorBindingStorageImageUpdateAfterBind",
    "descriptorBindingStorageBufferUpdateAfterBind",
    "descriptorBindingUniformTexelBufferUpdateAfterBind",
    "descriptorBindingStorageTexelBufferUpdateAfterBind",
    "descriptorBindingUpdateUnusedWhilePending",
    "descriptorBindingPartiallyBound",
    "descriptorBindingVariableDescriptorCount",
    "runtimeDescriptorArray",
)
FL11_1_BLOCKERS = {
    "VK_EXT_transform_feedback",
    "fillModeNonSolid",
    "geometryShader",
    "multiViewport",
    "shaderClipDistance",
    "shaderCullDistance",
    "textureCompressionBC",
    "transformFeedback",
    "geometryStreams",
    "tessellationShader",
    "vertexPipelineStoresAndAtomics",
}


def load_module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class P23ComplianceMatrixTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run([sys.executable, str(SCRIPT)], check=True, cwd=ROOT)
        cls.doc = json.loads(JSON_OUT.read_text())
        cls.caps = json.loads(CAPS.read_text())
        cls.p13 = json.loads(P13.read_text())
        cls.p16 = json.loads(P16.read_text())
        cls.p18 = json.loads(P18.read_text())
        cls.p19 = json.loads(P19.read_text())
        cls.p21 = json.loads(P21.read_text())

    def test_feature_bits_not_spoofed(self):
        for name in FALSE_BITS:
            self.assertIs(self.caps["features"][name], False, name)
            self.assertIs(self.doc["featureBitsUnchanged"][name], False, name)
        source = PHYSICAL.read_text()
        for name in (
            "geometryShader",
            "tessellationShader",
            "pipelineStatisticsQuery",
        ):
            self.assertRegex(source, rf"\.{name}\s*=\s*false,")
        # DX7 device-proven bits (validation/g615-v11-csf/dxvk/DX7-*.md).
        for name in (
            "fillModeNonSolid",
            "multiViewport",
            "shaderClipDistance",
            "shaderCullDistance",
        ):
            self.assertRegex(source, rf"\.{name}\s*=\s*PAN_ARCH >= 10,")
        self.assertNotRegex(source, r"\.sparseBinding\s*=\s*true")
        self.assertNotRegex(source, r"\.textureCompressionBC\s*=\s*true")

    def test_gates_stay_unmerged(self):
        self.assertEqual(
            self.doc["gatesUnmerged"],
            ["HARD_REQUIREMENTS", "PROFILE_BASELINE", "DEVICE_CREATE", "FEATURE_LEVEL"],
        )
        self.assertEqual(self.doc["result"], "FAIL")
        self.assertEqual(self.doc["smoke"], "BLOCKED")
        dumped = json.dumps(self.doc)
        self.assertNotIn('"HARD_REQUIREMENTS": "PASS", "PROFILE_BASELINE": "PASS"', dumped)
        v214 = self.doc["vkd3d"]["2.14.1"]
        self.assertEqual(v214["HARD_REQUIREMENTS"]["status"], "PASS")
        self.assertEqual(v214["PROFILE_BASELINE"]["status"], "FAIL")
        self.assertEqual(v214["DEVICE_CREATE"]["status"], "FAIL")
        self.assertEqual(v214["FEATURE_LEVEL"]["status"], "NOT_AVAILABLE")

    def test_matches_p13_d3d9(self):
        for version in ("1.10.3", "2.7.1", "3.1.1"):
            expected = self.p13["versions"][version]["status"]
            if version == "1.10.3":
                actual = self.doc["dxvk"][version]["D3D9"]["status"]
            else:
                actual = self.doc["dxvk"][version]["D3D9_PROFILE"]["status"]
            self.assertEqual(actual, expected, version)
            self.assertEqual(actual, "FAIL", version)

    def test_matches_p16_d3d10(self):
        for version in ("1.10.3", "2.7.1", "3.1.1"):
            expected = self.p16["versions"][version]["d3d10"]["status"]
            if version == "1.10.3":
                actual = self.doc["dxvk"][version]["D3D10"]["status"]
                fl10 = self.doc["dxvk"][version]["D3D11_FL10_1"]["status"]
            else:
                actual = self.doc["dxvk"][version]["D3D10_10_1_PROFILE"]["status"]
                fl10 = None
            self.assertEqual(actual, expected, version)
            self.assertEqual(actual, "FAIL", version)
            if fl10 is not None:
                self.assertEqual(fl10, self.p16["versions"][version]["d3d11Fl10x"]["status"])

    def test_matches_p18_d3d11_fl11(self):
        for version in ("1.10.3", "2.7.1", "3.1.1"):
            expected = self.p18["versions"][version]["status"]
            if version == "1.10.3":
                actual = self.doc["dxvk"][version]["D3D11_FL11_0"]["status"]
            else:
                actual = self.doc["dxvk"][version]["D3D11_11_0_PROFILE"]["status"]
            self.assertEqual(actual, expected, version)
            self.assertEqual(actual, "FAIL", version)

    def test_matches_p21_vkd3d_gates(self):
        for version in ("2.0", "2.14.1", "3.0.1"):
            src = self.p21["versions"][version]
            dst = self.doc["vkd3d"][version]
            self.assertEqual(dst["HARD_REQUIREMENTS"]["status"], src["HARD_REQUIREMENTS"]["status"], version)
            self.assertEqual(dst["DEVICE_CREATE"]["status"], src["DEVICE_CREATE"]["status"], version)
            self.assertEqual(dst["DEVICE_CREATE"]["blockers"], src["DEVICE_CREATE"]["blockers"], version)
            if version != "2.0":
                self.assertEqual(dst["PROFILE_BASELINE"]["status"], src["PROFILE_BASELINE"]["status"], version)
                self.assertEqual(dst["FEATURE_LEVEL"]["status"], src["FEATURE_LEVEL"]["status"], version)
                self.assertEqual(dst["FEATURE_LEVEL"]["d3dFeatureLevel"], "NOT_AVAILABLE", version)
        self.assertEqual(self.doc["vkd3d"]["2.0"]["PROFILE_BASELINE"]["status"], "NOT_APPLICABLE")
        self.assertEqual(self.doc["p19DeviceCreateNote"]["p19Blockers"], ["robustImageAccess2"])
        self.assertIn("transformFeedbackQueries", self.doc["p19DeviceCreateNote"]["p21Blockers"])
        self.assertEqual(self.doc["p19DeviceCreateNote"]["matrixUses"], "P21")

    def test_common_and_fl11_1_match_evaluator(self):
        evaluator = load_module(
            ROOT / "scripts/evaluate-consumer-profile.py", "evaluate_consumer_profile"
        )
        for version in ("2.7.1", "3.1.1"):
            imported = json.loads(
                (ROOT / f"validation/g615-v11-csf/profiles/dxvk-{version}.json").read_text()
            )
            result = evaluator.evaluate(self.caps, imported)
            d3d9 = result["profiles"]["VP_DXVK_d3d9_baseline"]
            common_fail = [
                item for item in d3d9["fail"] if item.get("capability") == "dxvk_common_required"
            ]
            self.assertEqual(common_fail, [])
            self.assertEqual(self.doc["dxvk"][version]["COMMON"]["status"], "PASS")
            fl11_1 = result["profiles"]["VP_DXVK_d3d11_level_11_1_baseline"]
            self.assertEqual(fl11_1["status"], "FAIL")
            self.assertEqual(self.doc["dxvk"][version]["D3D11_11_1_PROFILE"]["status"], "FAIL")
            self.assertEqual(
                set(self.doc["dxvk"][version]["D3D11_11_1_PROFILE"]["blockers"]),
                FL11_1_BLOCKERS,
            )
            self.assertEqual(self.doc["dxvk"][version]["MAX_FEATURE_LEVEL"], "NOT_AVAILABLE")

    def test_worker_report_core_bits(self):
        report = self.doc["workerReport"]
        self.assertEqual(report["PANVK"]["COMMIT"], "fc8a759e7d1b2b8de01c0e96f1fdc5e3950ba1a3")
        self.assertEqual(report["PANVK"]["MESA_COMMIT"], "5a07217f034b3e50d8c7c7794f97a2df1742613b")
        self.assertEqual(
            report["PANVK"]["PATCH_SERIES"],
            "sha256:c0bbdeef591b206a2f3ae36dc6191c08854399039075f8d69f103e33c0eca2f8",
        )
        self.assertEqual(report["PANVK"]["GPU_ID"], "0xb8a31030")
        self.assertEqual(report["PANVK"]["KBASE_UAPI"], "1.21")
        self.assertEqual(report["CORE"]["VULKAN_API"], "1.4.363")
        for key in (
            "GEOMETRY_SHADER",
            "TESSELLATION_SHADER",
            "FILL_MODE_NON_SOLID",
            "MULTI_VIEWPORT",
            "CLIP_DISTANCE",
            "CULL_DISTANCE",
            "BC",
            "PIPELINE_STATS",
            "SPARSE_BINDING",
        ):
            self.assertIs(report["CORE"][key], False, key)
        self.assertIs(report["ROBUSTNESS2"]["EXTENSION"], True)
        self.assertIs(report["ROBUSTNESS2"]["ROBUST_BUFFER_ACCESS2"], True)
        self.assertIs(report["ROBUSTNESS2"]["ROBUST_IMAGE_ACCESS2"], False)
        self.assertIs(report["ROBUSTNESS2"]["NULL_DESCRIPTOR"], True)
        self.assertIs(report["DESCRIPTORS"]["DESCRIPTOR_INDEXING"], True)
        self.assertIs(report["DESCRIPTORS"]["ALL_REQUIRED_INDEXING_BITS"], True)
        for name in INDEXING_BITS:
            self.assertIs(self.caps["features"][name], True, name)
        self.assertEqual(report["DESCRIPTORS"]["MAX_UAB_STORAGE_BUFFER"], 1048576)
        self.assertEqual(report["DESCRIPTORS"]["MAX_UAB_SAMPLED_IMAGE"], 1048576)
        self.assertEqual(report["DESCRIPTORS"]["MAX_UAB_STORAGE_IMAGE"], 1048576)
        self.assertEqual(report["DESCRIPTORS"]["MAX_UAB_SAMPLER"], 1048576)
        self.assertEqual(report["PUSH"]["MAX_PUSH_CONSTANTS"], 256)
        self.assertEqual(report["PUSH"]["MAX_PUSH_DESCRIPTORS"], 32)
        self.assertIs(report["TRANSFORM_FEEDBACK"]["EXTENSION"], False)
        self.assertIs(report["TRANSFORM_FEEDBACK"]["TRANSFORM_FEEDBACK"], False)
        self.assertIs(report["TRANSFORM_FEEDBACK"]["GEOMETRY_STREAMS"], False)
        self.assertIs(report["TRANSFORM_FEEDBACK"]["QUERIES"], False)
        self.assertIn("robustImageAccess2", report["NEXT_SINGLE_BLOCKER"])

    def test_markdown_worker_report(self):
        md = MD_OUT.read_text()
        self.assertIn("DXVK 1.10.3:", md)
        self.assertIn("D3D9=FAIL", md)
        self.assertIn("COMMON=PASS", md)
        self.assertIn("D3D9_PROFILE=FAIL", md)
        self.assertIn("HARD_REQUIREMENTS=PASS", md)
        self.assertIn("PROFILE_BASELINE=FAIL", md)
        self.assertIn("DEVICE_CREATE=FAIL", md)
        self.assertIn("FEATURE_LEVEL=NOT_AVAILABLE", md)
        self.assertIn("COMMIT=fc8a759e7d1b2b8de01c0e96f1fdc5e3950ba1a3", md)
        self.assertIn("NEXT_SINGLE_BLOCKER=", md)
        self.assertIn("P23 exits `FAIL`", md)
        self.assertIn("stay unmerged", md)


if __name__ == "__main__":
    unittest.main()
