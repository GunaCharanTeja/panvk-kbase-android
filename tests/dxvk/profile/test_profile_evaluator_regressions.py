import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
FIXTURE = ROOT / "validation/g615-v11-csf/dxvk/profile-evaluator-regressions/cases.json"
SPEC = importlib.util.spec_from_file_location("evaluator", ROOT / "scripts/evaluate-consumer-profile.py")
EVALUATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(EVALUATOR)


class ProfileEvaluatorRegressionsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fixture = json.loads(FIXTURE.read_text())

    def evaluate_case(self, name):
        case = self.fixture["profileCases"][name]
        result = EVALUATOR.evaluate(case["capabilities"], case["profile"])
        return result["profiles"][case["selectedProfile"]]

    def test_profile_cases(self):
        for name, case in self.fixture["profileCases"].items():
            with self.subTest(name=name):
                self.assertEqual(self.evaluate_case(name)["status"], case["expected"])

    def test_missing_field_is_unknown_and_non_pass(self):
        result = self.evaluate_case("missing-captured-field")
        self.assertEqual(result["status"], "UNKNOWN")
        self.assertEqual(result["unknown"][0]["path"], "features.VkPhysicalDeviceFeatures.geometryShader")

    def test_comparator_fixtures(self):
        for case in self.fixture["comparatorCases"]:
            with self.subTest(name=case["name"]):
                self.assertEqual(
                    EVALUATOR.compare(case["actual"], case["required"], case["comparison"]),
                    case["expected"],
                )

    def test_unknown_comparator_fails_closed(self):
        self.assertEqual(EVALUATOR.compare(8, 4, None), "UNKNOWN")

    def test_selected_profile_cli_failure_is_nonzero(self):
        case = self.fixture["profileCases"]["baseline-missing-feature"]
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            caps = directory / "caps.json"
            profile = directory / "profile.json"
            caps.write_text(json.dumps(case["capabilities"]))
            profile.write_text(json.dumps(case["profile"]))
            completed = subprocess.run(
                ["python3", str(ROOT / "scripts/evaluate-consumer-profile.py"),
                 "--capabilities", str(caps), "--profile", str(profile),
                 "--select", case["selectedProfile"]],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                text=True,
            )
        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("FAIL", completed.stderr)


if __name__ == "__main__":
    unittest.main()
