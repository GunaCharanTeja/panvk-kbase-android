#!/usr/bin/env python3
import json
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
EXTRAS = ROOT / "validation/g615-v11-csf/p20-remaining-baseline-extras.json"
CAPS = ROOT / "validation/g615-v11-csf/consumer-capabilities.json"
PHYSICAL = (
    ROOT / "work/mesa/src/panfrost/vulkan/panvk_vX_physical_device.c"
)
P19 = ROOT / "validation/g615-v11-csf/p19-vkd3d-profile-baseline.json"

P20_OWNED = {"pipelineStatisticsQuery"}
PRIOR = {
    "VK_EXT_transform_feedback",
    "robustImageAccess2",
    "transformFeedback",
    "geometryStreams",
    "geometryShader",
    "tessellationShader",
    "fillModeNonSolid",
    "multiViewport",
    "textureCompressionBC",
    "shaderClipDistance",
    "shaderCullDistance",
    "transformFeedbackQueries",
}
EVALUATOR = {"bufferImageGranularity", "subgroupSupportedOperations[0]"}
LATER = {"P21", "P22"}


class P20RemainingBaselineExtrasTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.extras = json.loads(EXTRAS.read_text())
        cls.caps = json.loads(CAPS.read_text())
        cls.p19 = json.loads(P19.read_text())

    def test_bit_stays_false(self):
        self.assertIs(self.caps["features"]["pipelineStatisticsQuery"], False)
        source = PHYSICAL.read_text()
        self.assertRegex(source, r"\.pipelineStatisticsQuery\s*=\s*false,")
        self.assertNotRegex(source, r"\.pipelineStatisticsQuery\s*=\s*true")
        self.assertIs(self.extras["pipelineStatisticsQuery"], False)
        self.assertEqual(self.extras["result"], "BLOCKED_SAFE_FALSE")

    def test_p20_owns_only_pipeline_statistics(self):
        self.assertEqual(set(self.extras["p20Owned"]), P20_OWNED)
        owned = self.extras["p20Owned"]["pipelineStatisticsQuery"]
        self.assertEqual(owned["implementationStatus"], "NOT_IMPLEMENTED")
        self.assertIn("Keep false", owned["action"])

    def test_counters_not_claimed(self):
        counters = self.extras["p20Owned"]["pipelineStatisticsQuery"]["counters"]
        self.assertEqual(
            set(counters),
            {
                "IA_VERTICES",
                "IA_PRIMITIVES",
                "VS_INVOCATIONS",
                "GS_INVOCATIONS",
                "GS_PRIMITIVES",
                "CLIPPING_INVOCATIONS",
                "CLIPPING_PRIMITIVES",
                "FS_INVOCATIONS",
                "TCS_PATCHES",
                "TES_INVOCATIONS",
                "CS_INVOCATIONS",
            },
        )
        for name, status in counters.items():
            self.assertNotEqual(status, "IMPLEMENTED", name)

    def test_p19_baseline_blockers_are_classified(self):
        blockers = set(self.p19["versions"]["2.14.1"]["PROFILE_BASELINE"]["blockers"])
        classified = P20_OWNED | PRIOR | EVALUATOR
        self.assertEqual(blockers, classified)
        self.assertTrue(LATER.isdisjoint(blockers))

    def test_sparse_stays_later_phase(self):
        later = self.extras["laterPhasesNotStarted"]
        self.assertIn("sparse", later["P22"])
        self.assertNotIn("sparseBinding", self.extras["p20Owned"])
        self.assertNotIn("sparseBinding", self.extras["alreadyOwnedPriorPhases"])

    def test_no_feature_bit_flip_in_mesa(self):
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


if __name__ == "__main__":
    unittest.main()
