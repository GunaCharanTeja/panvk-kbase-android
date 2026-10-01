#!/usr/bin/env python3
import json
import pathlib
import re
import unittest

from _mesa_tree import mesa_root

ROOT = pathlib.Path(__file__).resolve().parents[2]
DOC = ROOT / "docs/KBASE-SPARSE-FEASIBILITY.md"
JSON_OUT = ROOT / "validation/g615-v11-csf/p22-kbase-sparse-feasibility.json"
MD_OUT = ROOT / "validation/g615-v11-csf/P22-KBASE-SPARSE-FEASIBILITY.md"
CAPS = ROOT / "validation/g615-v11-csf/consumer-capabilities.json"
_MESA = mesa_root()
PHYSICAL = _MESA / "src/panfrost/vulkan/panvk_vX_physical_device.c"
KBASE_KMOD = _MESA / "src/panfrost/lib/kmod/kbase_kmod.c"
KBASE_KMOD_PATCH = (
    ROOT / "patches/kbase-common/files/src/panfrost/lib/kmod/kbase_kmod.c"
)
DEVICE_C = _MESA / "src/panfrost/vulkan/panvk_vX_device.c"
GATE_PATCH = ROOT / "patches/kbase-common/006-device-queue-wsi-kbase.patch"

FALSE_BITS = (
    "sparseBinding",
    "sparseResidencyBuffer",
    "sparseResidencyImage2D",
    "sparseResidencyImage3D",
    "sparseResidencyAliased",
    "sparseResidency2Samples",
    "sparseResidency4Samples",
    "sparseResidency8Samples",
    "sparseResidency16Samples",
    "shaderResourceResidency",
    "shaderResourceMinLod",
)
REQUIRED_OPS = (
    "reserveVA",
    "mapPages",
    "unmapPages",
    "rebindPages",
    "aliasMappings",
    "bufferSparseBind",
    "imageOpaqueBind",
    "imageSparseBind",
    "pageGranularity",
    "faultBehavior",
    "queueOrdering",
    "synchronization",
)
DOC_HEADINGS = (
    "Reserve VA",
    "Map pages",
    "Unmap pages",
    "Rebind pages",
    "Alias mappings",
    "Buffer sparse bind",
    "Image opaque bind",
    "Image sparse bind",
    "Page granularity",
    "Fault behavior",
    "Queue ordering",
    "Synchronization",
)


class P22KbaseSparseFeasibilityTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.doc = json.loads(JSON_OUT.read_text())
        cls.caps = json.loads(CAPS.read_text())
        cls.md = MD_OUT.read_text()
        cls.feas = DOC.read_text()

    def test_decision_is_no_go(self):
        self.assertEqual(self.doc["result"], "NO-GO")
        self.assertEqual(self.doc["implementation"], "NONE")
        self.assertFalse(self.doc["mesa26_2SparseEnabled"])
        self.assertFalse(self.doc["hasSparseOnKbase"])
        self.assertFalse(self.doc["ctsRun"])
        self.assertFalse(self.doc["g615SparseWorkloadRun"])
        self.assertIn("NO-GO", self.md)
        self.assertIn("NO-GO", self.feas)
        self.assertIn("Do not enable PanVK sparse", self.feas)

    def test_feature_bits_remain_false(self):
        for name in FALSE_BITS:
            self.assertIs(self.caps["features"][name], False, name)
            self.assertIs(self.doc["featureBitsUnchanged"][name], False, name)
            self.assertIn(name, self.md)
        self.assertFalse(self.doc["queueSparseBindingBit"])
        source = PHYSICAL.read_text()
        self.assertRegex(
            source,
            r"bool has_sparse = PAN_ARCH >= 10 && !device->kbase_node_path\[0\];",
        )
        self.assertRegex(source, r"\.sparseBinding = has_sparse,")
        self.assertRegex(source, r"\.shaderResourceResidency = false,")
        self.assertRegex(source, r"\.shaderResourceMinLod = false,")
        self.assertNotRegex(source, r"\.sparseBinding\s*=\s*true")
        self.assertNotRegex(source, r"has_sparse = PAN_ARCH >= 10;")

    def test_operation_map_complete(self):
        self.assertEqual(set(self.doc["operations"]), set(REQUIRED_OPS))
        blocked = {
            "INSUFFICIENT",
            "INFEASIBLE",
            "MISMATCH_NOT_SOLE_BLOCKER",
        }
        for name, row in self.doc["operations"].items():
            self.assertIn(row["feasibility"], blocked, name)
            self.assertTrue(row["vulkan"], name)
            self.assertTrue(row["kbase"], name)
        for heading in DOC_HEADINGS:
            self.assertIn(heading, self.feas, heading)
        self.assertEqual(
            self.doc["operations"]["bufferSparseBind"]["feasibility"],
            "INFEASIBLE",
        )
        self.assertEqual(
            self.doc["operations"]["synchronization"]["feasibility"],
            "INFEASIBLE",
        )

    def test_kbase_vm_bind_is_not_panthor(self):
        source = KBASE_KMOD.read_text()
        patch = KBASE_KMOD_PATCH.read_text()
        for text in (source, patch):
            self.assertIn("PAN_KMOD_VM_OP_MODE_ASYNC not supported", text)
            self.assertIn("cannot map BO at explicit VA", text)
            self.assertIn("Unmap happens automatically when the BO is freed", text)
            self.assertNotRegex(text, r"KBASE_IOCTL_MEM_COMMIT")
            self.assertNotIn("PAN_KMOD_VM_OP_OP_MAP_SPARSE", text)
        self.assertIn("kbase: sparse binding queues are not implemented yet", DEVICE_C.read_text())
        self.assertIn("!device->kbase_node_path[0]", GATE_PATCH.read_text())

    def test_remaining_bits_keep_false(self):
        remaining = self.doc["remainingSparseBits"]
        self.assertEqual(remaining["sparseBinding"], "KEEP_FALSE")
        self.assertEqual(remaining["sparseResidencyBuffer"], "KEEP_FALSE")
        self.assertEqual(remaining["sparseResidencyImage2D"], "KEEP_FALSE")
        self.assertEqual(remaining["sparseResidencyAliased"], "KEEP_FALSE")
        self.assertEqual(remaining["shaderResourceResidency"], "KEEP_FALSE")
        self.assertEqual(remaining["shaderResourceMinLod"], "KEEP_FALSE")
        self.assertEqual(remaining["VK_QUEUE_SPARSE_BINDING_BIT"], "KEEP_ABSENT")
        self.assertFalse(self.doc["backendComparison"]["emulateSparseHelpsKbase"])
        self.assertIn("KEEP_FALSE", self.md)
        self.assertNotIn("GO\n", self.md.split("NO-GO", 1)[0][-20:])

    def test_no_sparse_enable_in_source(self):
        source = PHYSICAL.read_text()
        self.assertIsNone(re.search(r"\.sparseBinding\s*=\s*true", source))
        kmod = KBASE_KMOD.read_text()
        self.assertNotIn("supported_vm_op_flags |= PAN_KMOD_VM_OP_OP_MAP_SPARSE", kmod)


if __name__ == "__main__":
    unittest.main()
