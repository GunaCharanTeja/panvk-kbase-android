import importlib.util
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location("evaluate_dxvk", ROOT / "scripts/evaluate-dxvk.py")
DXVK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(DXVK)
CAPS = json.loads((ROOT / "validation/g615-v11-csf/consumer-capabilities.json").read_text())


class Dxvk311ProfilesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.result = DXVK.evaluate_official(CAPS, "3.1.1")

    def test_common_remains_pass(self):
        self.assertEqual(self.result["common"]["status"], "PASS")
        self.assertEqual(self.result["common"]["blockers"], [])

    def test_baseline_ladder_records_exact_blockers(self):
        profiles = self.result["profiles"]
        expected = {
            "VP_DXVK_d3d9_baseline": 5,
            "VP_DXVK_d3d10_level_10_1_baseline": 9,
            "VP_DXVK_d3d11_level_11_0_baseline": 10,
            "VP_DXVK_d3d11_level_11_1_baseline": 11,
        }
        for name, count in expected.items():
            self.assertEqual(profiles[name]["classification"], "BASELINE")
            self.assertEqual(profiles[name]["status"], "FAIL")
            self.assertEqual(profiles[name]["failCount"], count)
        self.assertIn(
            "vertexPipelineStoresAndAtomics",
            profiles["VP_DXVK_d3d11_level_11_1_baseline"]["blockers"],
        )

    def test_optional_profiles_are_separate(self):
        for name, result in self.result["profiles"].items():
            if not name.endswith("_baseline"):
                self.assertEqual(result["classification"], "OPTIMAL")


if __name__ == "__main__":
    unittest.main()
