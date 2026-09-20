import importlib.util
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location("evaluate_dxvk", ROOT / "scripts/evaluate-dxvk.py")
DXVK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(DXVK)
CAPS = json.loads((ROOT / "validation/g615-v11-csf/consumer-capabilities.json").read_text())


class Dxvk1103RequirementsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.result = DXVK.evaluate_legacy(CAPS)

    def test_all_source_derived_tiers_are_evaluated(self):
        self.assertEqual(set(self.result["tiers"]), {"D3D9", "D3D10_10_1", "D3D11_FL11_0"})
        for result in self.result["tiers"].values():
            self.assertEqual(result["status"], "FAIL")
            self.assertTrue(result["blockers"])

    def test_fl11_inherits_d3d10_requirements(self):
        d3d10 = self.result["tiers"]["D3D10_10_1"]
        fl11 = self.result["tiers"]["D3D11_FL11_0"]
        d3d10_requirements = {row["requirement"] for row in d3d10["requirements"]}
        fl11_requirements = {row["requirement"] for row in fl11["requirements"]}
        self.assertTrue(d3d10_requirements.issubset(fl11_requirements))
        self.assertIn("tessellationShader", fl11_requirements)

    def test_transform_feedback_extension_is_a_blocker(self):
        self.assertIn("VK_EXT_transform_feedback", self.result["tiers"]["D3D10_10_1"]["blockers"])


if __name__ == "__main__":
    unittest.main()
