import importlib.util, json, pathlib, unittest

ROOT=pathlib.Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location("evaluate",ROOT/"scripts/evaluate-consumer-profile.py"); evaluate=importlib.util.module_from_spec(spec);spec.loader.exec_module(evaluate)

class ProfileTests(unittest.TestCase):
 def test_machine_generated_pass_fail(self):
  fixture=ROOT/"tests/consumer-compat/fixtures"; result=evaluate.evaluate(json.loads((fixture/"capabilities.json").read_text()),json.loads((fixture/"profile.json").read_text()),{"demoLimit":"max"})
  self.assertEqual(result["profiles"]["VP_fixture_pass"]["status"],"PASS")
  self.assertEqual(result["profiles"]["VP_fixture_fail"]["status"],"FAIL")
  self.assertTrue(result["profiles"]["VP_fixture_fail"]["fail"])
 def test_complete_structure_values_are_used(self):
  caps={"device":{"apiVersion":4194304},"featureStructures":{"VkPhysicalDeviceDemoFeatures":{"demoFeature":True}},"propertyStructures":{"VkPhysicalDeviceDemoProperties":{"demoLimit":8}},"extensions":[]}
  self.assertIs(evaluate.actual_for("features.VkPhysicalDeviceDemoFeatures.demoFeature",caps),True)
  self.assertEqual(evaluate.actual_for("properties.VkPhysicalDeviceDemoProperties.demoLimit",caps),8)
 def test_core_property_summary_is_used(self):
  caps={"coreProperties":{},"properties":{"maxPushConstantsSize":256},"features":{},"extensions":[]}
  self.assertEqual(evaluate.actual_for("properties.VkPhysicalDeviceProperties.maxPushConstantsSize",caps),256)
 def test_numeric_comparison_is_explicit(self):
  self.assertTrue(evaluate.satisfies(8,4,"max"))
  self.assertFalse(evaluate.satisfies(8,4,"min"))
 def test_api_requirement(self):self.assertTrue(evaluate.api_satisfies((1<<22)|(4<<12)|363,"1.3.0"))
 def test_included_optional_named_capability_fails_profile(self):
  caps={"device":{"apiVersion":4194304},"features":{"required":True,"optional":False},"extensions":[]}
  profile={"provenance":{},"document":{"capabilities":{"required":{"features":{"VkPhysicalDeviceFeatures":{"required":True}}},"optional":{"features":{"VkPhysicalDeviceFeatures":{"optional":True}}}},"profiles":{"VP_test":{"api-version":"1.0.0","capabilities":["required","optional"]}}}}
  result=evaluate.evaluate(caps,profile)["profiles"]["VP_test"]
  self.assertEqual(result["status"],"FAIL")
  self.assertEqual(result["fail"][0]["capability"],"optional")

if __name__=="__main__":unittest.main()
