import importlib.util
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location("evaluate_dxvk", ROOT / "scripts/evaluate-dxvk.py")
DXVK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(DXVK)
CAPS = json.loads((ROOT / "validation/g615-v11-csf/consumer-capabilities.json").read_text())


class Dxvk271ProfilesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.result = DXVK.evaluate_official(CAPS, "2.7.1")

    def test_common_remains_pass(self):
        self.assertEqual(self.result["common"]["status"], "PASS")

    def test_baseline_ladder_is_available(self):
        profiles = self.result["profiles"]
        for name in (
            "VP_DXVK_d3d9_baseline",
            "VP_DXVK_d3d10_level_10_1_baseline",
            "VP_DXVK_d3d11_level_11_0_baseline",
            "VP_DXVK_d3d11_level_11_1_baseline",
        ):
            self.assertIn(name, profiles)
            self.assertEqual(profiles[name]["status"], "FAIL")
            self.assertTrue(profiles[name]["blockers"])

    def test_d3d9_blockers_match_current_capture(self):
        self.assertEqual(
            set(self.result["profiles"]["VP_DXVK_d3d9_baseline"]["blockers"]),
            {
                "geometryShader",
                "fillModeNonSolid",
                "shaderClipDistance",
                "shaderCullDistance",
                "textureCompressionBC",
            },
        )


if __name__ == "__main__":
    unittest.main()
