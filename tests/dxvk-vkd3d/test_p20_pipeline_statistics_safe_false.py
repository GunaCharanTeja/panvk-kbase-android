#!/usr/bin/env python3
import pathlib
import re
import unittest

from _mesa_tree import mesa_root

ROOT = pathlib.Path(__file__).resolve().parents[2]
MESA = mesa_root()
PHYSICAL_DEVICE = MESA / "src/panfrost/vulkan/panvk_vX_physical_device.c"
QUERY_POOL = MESA / "src/panfrost/vulkan/panvk_vX_query_pool.c"
CSF_QUERY = MESA / "src/panfrost/vulkan/csf/panvk_vX_cmd_query.c"
JM_QUERY = MESA / "src/panfrost/vulkan/jm/panvk_vX_cmd_query.c"
CMD_QUERY_H = MESA / "src/panfrost/vulkan/panvk_cmd_query.h"
PANVK = MESA / "src/panfrost/vulkan"
COMPILER = MESA / "src/panfrost/compiler"
GALLIUM = MESA / "src/gallium/drivers/panfrost"
PERF = MESA / "src/panfrost/perf"

PIPELINE_STATS = re.compile(r"VK_QUERY_TYPE_PIPELINE_STATISTICS")
STATS_FEATURE_TRUE = re.compile(r"\.pipelineStatisticsQuery\s*=\s*true")


# pipelineStatisticsQuery is exposed on v10+. CSF records counters in
# panvk_pstats_query_state. JM has no pipeline-statistics query.
# VK_EXT_primitives_generated_query (csf-v11/073) is not in this HEAD.
class PipelineStatisticsSafeFalseTest(unittest.TestCase):
    def test_feature_remains_disabled(self):
        source = PHYSICAL_DEVICE.read_text()
        self.assertRegex(
            source, r"\.pipelineStatisticsQuery\s*=\s*PAN_ARCH >= 10,"
        )
        self.assertNotRegex(source, STATS_FEATURE_TRUE)
        self.assertRegex(source, r"\.occlusionQueryPrecise\s*=\s*true,")

    def test_query_pool_create_rejects_pipeline_statistics(self):
        source = QUERY_POOL.read_text()
        self.assertIn("VK_QUERY_TYPE_OCCLUSION", source)
        self.assertIn("VK_QUERY_TYPE_TIMESTAMP", source)
        self.assertIn("VK_QUERY_TYPE_PRIMITIVES_GENERATED_EXT", source)
        self.assertIn("VK_QUERY_TYPE_PIPELINE_STATISTICS", source)
        self.assertIn("panvk_pstats_query_state", source)
        self.assertIn('UNREACHABLE("Unsupported query type")', source)

    def test_csf_begin_end_copy_have_no_pipeline_statistics(self):
        source = CSF_QUERY.read_text()
        self.assertIn("VK_QUERY_TYPE_PIPELINE_STATISTICS", source)
        self.assertIn("panvk_cmd_begin_pstats_query", source)
        self.assertIn("panvk_cmd_end_pstats_query", source)
        self.assertIn("panvk_copy_pstats_query_results", source)
        self.assertIn("VK_QUERY_TYPE_OCCLUSION", source)
        self.assertIn("VK_QUERY_TYPE_PRIMITIVES_GENERATED_EXT", source)
        self.assertIn('UNREACHABLE("Unsupported query type")', source)

    def test_jm_query_has_no_pipeline_statistics(self):
        source = JM_QUERY.read_text()
        self.assertNotIn("VK_QUERY_TYPE_PIPELINE_STATISTICS", source)

    def test_cmd_query_state_has_no_pipeline_stats(self):
        source = CMD_QUERY_H.read_text()
        self.assertIn("panvk_occlusion_query_state", source)
        self.assertIn("panvk_prims_generated_query_state", source)
        self.assertIn("panvk_pstats_query_state", source)
        self.assertIn("panvk_pstats_counter_addr", source)
        self.assertNotIn("PIPELINE_STATISTICS", source)

    def test_panvk_has_no_pipeline_statistics_type(self):
        matches = []
        for path in PANVK.rglob("*.[ch]"):
            text = path.read_text(errors="replace")
            if PIPELINE_STATS.search(text):
                matches.append(str(path.relative_to(MESA)))
        self.assertEqual(
            sorted(matches),
            [
                "src/panfrost/vulkan/csf/panvk_vX_cmd_query.c",
                "src/panfrost/vulkan/panvk_vX_query_pool.c",
            ],
            "unexpected PIPELINE_STATISTICS files; reassess P20:\n"
            + "\n".join(matches),
        )

    def test_no_software_stat_instrumentation(self):
        patterns = (
            re.compile(r"pipeline_stats_flags"),
            re.compile(r"hk_pipeline_stat"),
            re.compile(r"VK_QUERY_PIPELINE_STATISTIC_"),
            re.compile(r"PIPE_STAT_QUERY"),
        )
        matches = []
        for root in (PANVK, COMPILER, GALLIUM):
            for path in root.rglob("*.[ch]"):
                text = path.read_text(errors="replace")
                for pattern in patterns:
                    if pattern.search(text):
                        matches.append(
                            f"{path.relative_to(MESA)}: {pattern.pattern}"
                        )
        self.assertEqual(
            sorted(matches),
            [
                "src/panfrost/vulkan/csf/panvk_vX_cmd_draw.c: VK_QUERY_PIPELINE_STATISTIC_",
                "src/panfrost/vulkan/csf/panvk_vX_cmd_query.c: VK_QUERY_PIPELINE_STATISTIC_",
            ],
            "unexpected pipeline-stat path; reassess P20:\n" + "\n".join(matches),
        )

    def test_mali_perf_is_not_wired_into_panvk_queries(self):
        pattern = re.compile(r"pan_perf_|mali_perf_")
        matches = []
        for path in PANVK.rglob("*.[ch]"):
            if pattern.search(path.read_text(errors="replace")):
                matches.append(str(path.relative_to(MESA)))
        self.assertEqual(
            matches,
            [],
            "Mali perf counters now used by PanVK queries; reassess P20:\n"
            + "\n".join(matches),
        )
        self.assertTrue((PERF / "pan_perf.c").exists())

    def test_prims_generated_is_not_pipeline_statistics(self):
        source = CSF_QUERY.read_text()
        self.assertIn("panvk_cmd_begin_prims_generated_query", source)
        self.assertIn("VK_QUERY_TYPE_PRIMITIVES_GENERATED_EXT", source)
        phys = PHYSICAL_DEVICE.read_text()
        self.assertNotIn("EXT_primitives_generated_query", phys)
        self.assertNotRegex(phys, r"\.primitivesGeneratedQuery\s*=\s*true")


if __name__ == "__main__":
    unittest.main()
