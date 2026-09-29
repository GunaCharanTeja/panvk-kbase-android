#!/usr/bin/env python3
import json
import pathlib
import subprocess
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/evaluate-vkd3d-d3d12-feature-level.py"
JSON_OUT = ROOT / "validation/g615-v11-csf/p21-d3d12-feature-level.json"
MD_OUT = ROOT / "validation/g615-v11-csf/P21-D3D12-FEATURE-LEVEL.md"
CAPS = ROOT / "validation/g615-v11-csf/consumer-capabilities.json"
PHYSICAL = ROOT / "work/mesa/src/panfrost/vulkan/panvk_vX_physical_device.c"
FALSE_BITS = (
    "geometryShader",
    "fillModeNonSolid",
    "shaderClipDistance",
    "shaderCullDistance",
    "textureCompressionBC",
    "multiViewport",
    "transformFeedback",
    "geometryStreams",
    "tessellationShader",
    "pipelineStatisticsQuery",
    "robustImageAccess2",
    "sparseBinding",
    "sparseResidencyBuffer",
    "sparseResidencyImage2D",
    "sparseResidencyAliased",
    "shaderResourceResidency",
    "shaderResourceMinLod",
    "vertexPipelineStoresAndAtomics",
)
ROW_FIELDS = {
    "requirement",
    "current",
    "required",
    "status",
    "sourceVersion",
    "source",
    "upstreamPanVK",
    "implementationStatus",
    "action",
}
BASELINE_BLOCKERS = {
    "VK_EXT_transform_feedback",
    "robustImageAccess2",
    "transformFeedback",
    "geometryStreams",
    "geometryShader",
    "tessellationShader",
    "fillModeNonSolid",
    "multiViewport",
    "textureCompressionBC",
    "pipelineStatisticsQuery",
    "shaderClipDistance",
    "shaderCullDistance",
    "transformFeedbackQueries",
    "bufferImageGranularity",
    "subgroupSupportedOperations[0]",
}


class P21D3D12FeatureLevelTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run([sys.executable, str(SCRIPT)], check=True, cwd=ROOT)
        cls.doc = json.loads(JSON_OUT.read_text())
        cls.caps = json.loads(CAPS.read_text())

    def test_feature_bits_remain_false(self):
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

    def test_gates_stay_separate(self):
        self.assertEqual(
            self.doc["gatesUnmerged"],
            ["HARD_REQUIREMENTS", "PROFILE_BASELINE", "DEVICE_CREATE", "FEATURE_LEVEL"],
        )
        self.assertEqual(self.doc["result"], "FAIL")
        self.assertEqual(self.doc["smoke"], "BLOCKED")
        self.assertEqual(self.doc["d3dFeatureLevel"], "NOT_AVAILABLE")
        dumped = json.dumps(self.doc)
        self.assertNotIn('"HARD_REQUIREMENTS": "PASS", "PROFILE_BASELINE": "PASS"', dumped)

    def test_2_0_split_gates(self):
        item = self.doc["versions"]["2.0"]
        self.assertEqual(item["HARD_REQUIREMENTS"]["status"], "PASS")
        self.assertEqual(item["HARD_REQUIREMENTS"]["failCount"], 0)
        self.assertEqual(item["PROFILE_BASELINE"]["status"], "NOT_APPLICABLE")
        self.assertEqual(item["DEVICE_CREATE"]["status"], "PASS")
        self.assertEqual(item["FEATURE_LEVEL"]["status"], "NOT_AVAILABLE")
        self.assertEqual(item["FEATURE_LEVEL"]["d3dFeatureLevel"], "NOT_AVAILABLE")
        self.assertEqual(item["FEATURE_LEVEL"]["impliedFeatureLevel"], "D3D_FEATURE_LEVEL_11_0")
        self.assertEqual(item["stockVkd3dSmoke"], "BLOCKED")
        names = {row["requirement"] for row in item["HARD_REQUIREMENTS"]["requirements"]}
        self.assertIn("VK_KHR_timeline_semaphore", names)
        self.assertNotIn("robustImageAccess2", names)
        self.assertNotIn("VK_KHR_push_descriptor", names)

    def test_later_versions_split_gates(self):
        for version in ("2.14.1", "3.0.1"):
            item = self.doc["versions"][version]
            self.assertEqual(item["HARD_REQUIREMENTS"]["status"], "PASS", version)
            self.assertEqual(item["HARD_REQUIREMENTS"]["failCount"], 0, version)
            self.assertEqual(item["PROFILE_BASELINE"]["status"], "FAIL", version)
            self.assertEqual(item["DEVICE_CREATE"]["status"], "FAIL", version)
            self.assertEqual(item["FEATURE_LEVEL"]["status"], "NOT_AVAILABLE", version)
            self.assertEqual(item["FEATURE_LEVEL"]["d3dFeatureLevel"], "NOT_AVAILABLE", version)
            self.assertEqual(
                item["FEATURE_LEVEL"]["impliedFeatureLevel"],
                "D3D_FEATURE_LEVEL_11_0",
                version,
            )
            self.assertEqual(item["stockVkd3dSmoke"], "BLOCKED", version)
            self.assertIn("E_INVALIDARG", item["smokeReason"])
            self.assertIn("not a legal host", item["smokeReason"])
            self.assertIn("robustImageAccess2", item["DEVICE_CREATE"]["blockers"], version)
            self.assertIn("transformFeedbackQueries", item["DEVICE_CREATE"]["blockers"], version)
            self.assertEqual(
                set(item["PROFILE_BASELINE"]["blockers"]),
                BASELINE_BLOCKERS,
                version,
            )

    def test_required_row_fields(self):
        rows = []
        for version in ("2.0", "2.14.1", "3.0.1"):
            item = self.doc["versions"][version]
            rows.extend(item["HARD_REQUIREMENTS"]["requirements"])
            rows.extend(item["PROFILE_BASELINE"]["requirements"])
            rows.extend(item["DEVICE_CREATE"]["requirements"])
            rows.extend(item["FEATURE_LEVEL"]["requirements"])
        self.assertTrue(rows)
        for row in rows:
            self.assertTrue(ROW_FIELDS.issubset(row), row.keys())

    def test_implied_fl_cannot_climb(self):
        for version in ("2.0", "2.14.1", "3.0.1"):
            fl = self.doc["versions"][version]["FEATURE_LEVEL"]
            self.assertEqual(fl["fl11_0"], "DEFAULT", version)
            self.assertEqual(fl["fl11_1"], "FAIL", version)
            self.assertEqual(fl["fl12_0"], "FAIL", version)
            self.assertEqual(fl["fl12_1"], "FAIL", version)
            self.assertEqual(fl["fl12_2"], "FAIL", version)
            self.assertEqual(
                fl["tiledResourcesTier"],
                "D3D12_TILED_RESOURCES_TIER_NOT_SUPPORTED",
                version,
            )
            self.assertIn("vertexPipelineStoresAndAtomics", fl["blockers"], version)
            self.assertIn("sparseBinding", fl["blockers"], version)

    def test_sparse_stays_p22(self):
        dumped = json.dumps(self.doc)
        self.assertIn("P22", dumped)
        self.assertIn("Do not jump to sparse implementation", dumped)
        self.assertEqual(
            self.doc["versions"]["2.14.1"]["FEATURE_LEVEL"]["tiledResourcesTier"],
            "D3D12_TILED_RESOURCES_TIER_NOT_SUPPORTED",
        )
        self.assertNotIn("sparseBinding", self.doc["versions"]["2.14.1"]["HARD_REQUIREMENTS"]["blockers"])

    def test_vendor_profiles_not_merged(self):
        baseline = json.dumps(self.doc["versions"]["2.14.1"]["PROFILE_BASELINE"])
        self.assertNotIn("VP_D3D12_maximum_radv", baseline)
        self.assertNotIn("VP_D3D12_FL_12_0_baseline", baseline)
        self.assertIn(
            "VP_D3D12_FL_12_0_baseline",
            self.doc["versions"]["2.14.1"]["FEATURE_LEVEL"]["profiles"],
        )
        self.assertEqual(
            self.doc["versions"]["2.14.1"]["vendorMaximumProfilesOutOfScope"],
            ["VP_D3D12_maximum_radv", "VP_D3D12_maximum_nv"],
        )

    def test_3_0_1_baseline_includes_unused_attachments(self):
        rows = self.doc["versions"]["3.0.1"]["PROFILE_BASELINE"]["requirements"]
        names = {row["requirement"] for row in rows}
        self.assertIn("VK_EXT_dynamic_rendering_unused_attachments", names)

    def test_p20_pipeline_stats_status_carried(self):
        extras = self.doc["p20Extras"]
        self.assertEqual(extras["result"], "BLOCKED_SAFE_FALSE")
        self.assertEqual(
            extras["p20Owned"]["pipelineStatisticsQuery"]["implementationStatus"],
            "NOT_IMPLEMENTED",
        )
        md = MD_OUT.read_text()
        self.assertIn("P20", md)
        self.assertIn("pipelineStatisticsQuery", md)

    def test_adapter_and_report(self):
        driver = self.doc["driver"]
        self.assertEqual(driver["adapter"], "Mali-G615 MC6")
        self.assertEqual(driver["driverName"], "panvk")
        self.assertIn("Mesa 26.3.0-devel", driver["driverInfo"])
        self.assertEqual(driver["apiVersionText"], "1.4.363")
        self.assertEqual(driver["deviceIDHex"], "0xb8a31030")
        md = MD_OUT.read_text()
        self.assertIn("VKD3D_2_0_HARD=PASS", md)
        self.assertIn("VKD3D_2_0_BASELINE=NOT_APPLICABLE", md)
        self.assertIn("VKD3D_2_0_DEVICE=PASS", md)
        self.assertIn("VKD3D_2_0_FEATURE_LEVEL=NOT_AVAILABLE", md)
        self.assertIn("VKD3D_2_14_1_HARD=PASS", md)
        self.assertIn("VKD3D_2_14_1_BASELINE=FAIL", md)
        self.assertIn("VKD3D_2_14_1_DEVICE=FAIL", md)
        self.assertIn("VKD3D_2_14_1_FEATURE_LEVEL=NOT_AVAILABLE", md)
        self.assertIn("VKD3D_3_0_1_HARD=PASS", md)
        self.assertIn("VKD3D_3_0_1_BASELINE=FAIL", md)
        self.assertIn("These four gates stay separate", md)
        self.assertIn("P21 exits `FAIL`", md)
        self.assertIn("BLOCKED", md)


if __name__ == "__main__":
    unittest.main()
