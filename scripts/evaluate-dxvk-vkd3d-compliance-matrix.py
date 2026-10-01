#!/usr/bin/env python3
"""P23: final DXVK/vkd3d compliance matrix. Reads existing phase JSON. Does not flip features."""
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CAPS_PATH = ROOT / "validation/g615-v11-csf/consumer-capabilities.json"
P13_PATH = ROOT / "validation/g615-v11-csf/p13-d3d9.json"
P16_PATH = ROOT / "validation/g615-v11-csf/p16-d3d10.json"
P18_PATH = ROOT / "validation/g615-v11-csf/p18-d3d11-fl11.json"
P19_PATH = ROOT / "validation/g615-v11-csf/p19-vkd3d-profile-baseline.json"
P21_PATH = ROOT / "validation/g615-v11-csf/p21-d3d12-feature-level.json"
P22_PATH = ROOT / "validation/g615-v11-csf/p22-kbase-sparse-feasibility.json"
PROFILES = {
    "2.7.1": ROOT / "validation/g615-v11-csf/profiles/dxvk-2.7.1.json",
    "3.1.1": ROOT / "validation/g615-v11-csf/profiles/dxvk-3.1.1.json",
}
JSON_OUT = ROOT / "validation/g615-v11-csf/p23-dxvk-vkd3d-compliance-matrix.json"
MD_OUT = ROOT / "validation/g615-v11-csf/P23-DXVK-VKD3D-COMPLIANCE-MATRIX.md"
TXT_OUT = ROOT / "validation/g615-v11-csf/p23-dxvk-vkd3d-compliance-matrix-2026-09-20.txt"

PINNED_MESA = "5a07217f034b3e50d8c7c7794f97a2df1742613b"
AUDITED_MESA = "e1f3f372c4a661cd0e71a0cdafc4d469d56ecf35"
PATCH_SERIES = "sha256:c0bbdeef591b206a2f3ae36dc6191c08854399039075f8d69f103e33c0eca2f8"
PANVK_COMMIT = "fc8a759e7d1b2b8de01c0e96f1fdc5e3950ba1a3"
FL11_1 = "VP_DXVK_d3d11_level_11_1_baseline"
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
    "sparseResidencyBuffer",
    "sparseResidencyImage2D",
    "sparseResidencyImage3D",
    "sparseResidencyAliased",
    "shaderResourceResidency",
    "shaderResourceMinLod",
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

# Overlay only. Evaluation still reads the beta.3-era capture. Do not gate on this.
BETA6_STATUS = {
    "release": "g615-v11-csf-v0.1.0-beta.6",
    "patchesUpTo": "077",
    "exposedSinceCapture": [
        {
            "feature": "variableMultisampleRate",
            "patch": "074",
            "cts": "variable_rate 504/0 standalone; combined regression 6210/0",
        },
        {
            "feature": "sync_fd export (kbase KCPU queue)",
            "patch": "075",
            "cts": "1996/0 (was 113 ResourceError)",
        },
        {
            "feature": "multiViewport GS viewport index honoured",
            "patch": "076",
            "cts": "draw scissor 88/88",
        },
        {
            "feature": "system-scope subqueue sync signals on kbase",
            "patch": "077",
            "cts": "signal_order 1316/0 (0 timeouts)",
        },
        {
            "feature": "swapchain lifecycle test",
            "patch": "test-apk",
            "cts": "done (passes on device)",
        },
        {
            "feature": "VK_EXT_memory_priority + VK_EXT_pageable_device_local_memory",
            "patch": "069",
            "cts": "224/0, 202/0; api.info 7799/0",
        },
        {"feature": "alphaToOne", "patch": "070", "cts": "123/0"},
        {
            "feature": "maxGeometryShaderInvocations",
            "patch": "071",
            "cts": "geometry 193/0, instanced 20/0",
        },
        {"feature": "VK_EXT_multi_draw", "patch": "072", "cts": "12704/0"},
        {
            "feature": "VK_EXT_primitives_generated_query",
            "patch": "073",
            "cts": "75206/0",
        },
        {
            "feature": "geometryShader",
            "patch": "before-beta.5",
            "cts": "geometry 193/0, instanced 20/0",
        },
        {"feature": "tessellationShader", "patch": "before-beta.5", "cts": "526/0"},
        {
            "feature": "VK_EXT_transform_feedback",
            "patch": "before-beta.5",
            "cts": "15793/0, 2 intermittent DeviceLost",
        },
        {
            "feature": "textureCompressionBC",
            "patch": "before-beta.5",
            "cts": "1863/0, copy_and_blit 9620/0",
        },
        {
            "feature": "shaderClipDistance",
            "patch": "before-beta.5",
            "cts": "device matrix 0 fail",
        },
        {
            "feature": "shaderCullDistance",
            "patch": "before-beta.5",
            "cts": "device matrix 0 fail",
        },
        {"feature": "multiViewport", "patch": "before-beta.5", "cts": "device matrix 0 fail"},
        {
            "feature": "fillModeNonSolid",
            "patch": "before-beta.5",
            "cts": "device matrix 0 fail",
        },
        {
            "feature": "pipelineStatisticsQuery",
            "patch": "049-054",
            "cts": "statistics_query 15374/0",
        },
    ],
    "openItems": [
        "GS primitive drop",
        "VMR (vertexPipelineStoresAndAtomics / FL11_1, worktree dx-vpsa)",
        "JICA98 0005 (same-queue semaphore waits) review",
        "depthBounds",
        "shaderOutputViewportIndex (not exposed; GS viewport index honoured in 076)",
        "XFB intermittent DeviceLost",
        "XFB 65536-record cap per non-tess draw",
        "robustImageAccess2 (vkd3d-proton device-create blocker)",
    ],
    "deferred": [
        "vkd3d/D3D12",
        "FL12 (sparse NO-GO on kbase)",
    ],
    "note": (
        "Matrix rows below are evaluated from the beta.3-era "
        "consumer-capabilities.json capture; features listed here are "
        "device-proven in beta.6 but await a fresh capture. "
        "Swapchain lifecycle test done (passes on device); GS viewport index honoured (076)."
    ),
}
BETA5_STATUS = BETA6_STATUS


def load_evaluator():
    spec = importlib.util.spec_from_file_location(
        "evaluate_consumer_profile",
        ROOT / "scripts/evaluate-consumer-profile.py",
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def decode_hex_cstring(value):
    if not isinstance(value, str):
        return value
    try:
        raw = bytes.fromhex(value)
    except ValueError:
        return value
    return raw.split(b"\x00", 1)[0].decode("utf-8", "replace")


def decode_limit(value):
    if isinstance(value, bool) or value is None:
        return value
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        try:
            return int.from_bytes(bytes.fromhex(value), "little")
        except ValueError:
            return value
    return value


def leaf(path):
    return path.rsplit(".", 1)[-1]


def ext_present(caps, name):
    return any(item.get("name") == name for item in caps.get("extensions", []))


def gate(status, **extra):
    row = {"status": status}
    row.update(extra)
    return row


def evaluate_common(caps, evaluator, version):
    imported = json.loads(PROFILES[version].read_text())
    result = evaluator.evaluate(caps, imported)
    profile = result["profiles"]["VP_DXVK_d3d9_baseline"]
    rows = [item for item in profile["pass"] + profile["fail"] if item.get("capability") == "dxvk_common_required"]
    fails = [item for item in rows if not evaluator.item_satisfies(item)]
    return gate(
        "PASS" if not fails else "FAIL",
        sourceKind="official-profile-capability",
        sourceProfile="dxvk_common_required",
        sourceFile=str(PROFILES[version].relative_to(ROOT)),
        passCount=len(rows) - len(fails),
        failCount=len(fails),
        blockers=[leaf(item["path"]) for item in fails],
        note="COMMON is dxvk_common_required only. D3D9/D3D10/D3D11 profiles stay separate.",
    )


def evaluate_fl11_1(caps, evaluator, version):
    imported = json.loads(PROFILES[version].read_text())
    result = evaluator.evaluate(caps, imported)
    profile = result["profiles"][FL11_1]
    fails = profile["fail"]
    return gate(
        profile["status"],
        sourceKind="official-profile",
        sourceProfile=FL11_1,
        sourceFile=str(PROFILES[version].relative_to(ROOT)),
        passCount=len(profile["pass"]),
        failCount=len(fails),
        blockers=[leaf(item["path"]) for item in fails],
        d3dFeatureLevel="NOT_AVAILABLE",
        stockDxvkSmoke="BLOCKED",
    )


def dxvk_110(p13, p16, p18):
    d3d9 = p13["versions"]["1.10.3"]
    d3d10 = p16["versions"]["1.10.3"]["d3d10"]
    fl10 = p16["versions"]["1.10.3"]["d3d11Fl10x"]
    fl11 = p18["versions"]["1.10.3"]
    return {
        "D3D9": gate(d3d9["status"], source="P13", failCount=d3d9["failCount"], blockers=d3d9["blockers"], stockDxvkSmoke=d3d9["stockDxvkSmoke"], d3dFeatureLevel=d3d9["d3dFeatureLevel"]),
        "D3D10": gate(d3d10["status"], source="P16", failCount=d3d10["failCount"], blockers=d3d10["blockers"], d3dFeatureLevel=d3d10["d3dFeatureLevel"]),
        "D3D11_FL10_1": gate(fl10["status"], source="P16", failCount=fl10["failCount"], blockers=fl10["blockers"], d3dFeatureLevel=fl10["d3dFeatureLevel"]),
        "D3D11_FL11_0": gate(fl11["status"], source="P18", failCount=fl11["failCount"], blockers=fl11["blockers"], stockDxvkSmoke=fl11["stockDxvkSmoke"], d3dFeatureLevel=fl11["d3dFeatureLevel"]),
        "MAX_FEATURE_LEVEL": "NOT_AVAILABLE",
        "stockDxvkSmoke": "BLOCKED",
    }


def dxvk_official(version, p13, p16, p18, common, fl11_1):
    d3d9 = p13["versions"][version]
    d3d10 = p16["versions"][version]["d3d10"]
    fl11 = p18["versions"][version]
    return {
        "COMMON": common,
        "D3D9_PROFILE": gate(d3d9["status"], source="P13", sourceProfile=d3d9.get("sourceProfile"), failCount=d3d9["failCount"], blockers=d3d9["blockers"], stockDxvkSmoke=d3d9["stockDxvkSmoke"], d3dFeatureLevel=d3d9["d3dFeatureLevel"]),
        "D3D10_10_1_PROFILE": gate(d3d10["status"], source="P16", sourceProfile=d3d10.get("sourceProfile"), failCount=d3d10["failCount"], blockers=d3d10["blockers"], d3dFeatureLevel=d3d10["d3dFeatureLevel"]),
        "D3D11_11_0_PROFILE": gate(fl11["status"], source="P18", sourceProfile=fl11.get("sourceProfile"), failCount=fl11["failCount"], blockers=fl11["blockers"], stockDxvkSmoke=fl11["stockDxvkSmoke"], d3dFeatureLevel=fl11["d3dFeatureLevel"]),
        "D3D11_11_1_PROFILE": fl11_1,
        "MAX_FEATURE_LEVEL": "NOT_AVAILABLE",
        "stockDxvkSmoke": "BLOCKED",
    }


def vkd3d_from_p21(p21, version):
    item = p21["versions"][version]
    hard = item["HARD_REQUIREMENTS"]
    baseline = item["PROFILE_BASELINE"]
    device = item["DEVICE_CREATE"]
    feature_level = item["FEATURE_LEVEL"]
    out = {
        "HARD_REQUIREMENTS": gate(hard["status"], source="P21", failCount=hard.get("failCount", 0), blockers=hard.get("blockers", [])),
        "DEVICE_CREATE": gate(device["status"], source="P21", failCount=device.get("failCount", 0), blockers=device.get("blockers", []), stockVkd3dSmoke=item.get("stockVkd3dSmoke", device.get("stockVkd3dSmoke"))),
    }
    if version == "2.0":
        out["PROFILE_BASELINE"] = gate(baseline["status"], source="P21", note="v2.0 has no official VP_D3D12_VKD3D_PROTON_profile.json")
        out["FEATURE_LEVEL"] = gate(
            feature_level["status"],
            source="P21",
            d3dFeatureLevel=feature_level.get("d3dFeatureLevel"),
            impliedFeatureLevel=feature_level.get("impliedFeatureLevel"),
            note="FEATURE_LEVEL is stock CheckFeatureSupport. Implied FL is not FEATURE_LEVEL.",
        )
        return out
    out["PROFILE_BASELINE"] = gate(baseline["status"], source="P21", failCount=baseline.get("failCount", 0), blockers=baseline.get("blockers", []))
    out["FEATURE_LEVEL"] = gate(
        feature_level["status"],
        source="P21",
        d3dFeatureLevel=feature_level.get("d3dFeatureLevel"),
        impliedFeatureLevel=feature_level.get("impliedFeatureLevel"),
        fl11_0=feature_level.get("fl11_0"),
        fl11_1=feature_level.get("fl11_1"),
        fl12_0=feature_level.get("fl12_0"),
        tiledResourcesTier=feature_level.get("tiledResourcesTier"),
        note="FEATURE_LEVEL is stock CheckFeatureSupport. Implied FL is not FEATURE_LEVEL.",
    )
    return out


def worker_report(caps, dxvk, vkd3d, p21, p22):
    features = caps["features"]
    limits = caps.get("coreProperties", {}).get("limits", {})
    dip = caps.get("propertyStructures", {}).get("VkPhysicalDeviceDescriptorIndexingProperties", {})
    push = caps.get("propertyStructures", {}).get("VkPhysicalDevicePushDescriptorProperties", {})
    indexing_ok = all(features.get(name) is True for name in INDEXING_BITS)
    xfb_ext = ext_present(caps, "VK_EXT_transform_feedback")
    robustness2_ext = ext_present(caps, "VK_EXT_robustness2")
    queries = caps.get("propertyStructures", {}).get("VkPhysicalDeviceTransformFeedbackPropertiesEXT", {}).get("transformFeedbackQueries")
    if queries is None:
        queries = False
    return {
        "PANVK": {
            "COMMIT": PANVK_COMMIT,
            "MESA_COMMIT": PINNED_MESA,
            "PATCH_SERIES": PATCH_SERIES,
            "DEVICE": caps["capture"]["target"],
            "GPU_ID": "0x{:08x}".format(caps["device"]["deviceID"]),
            "KBASE_UAPI": p22["kbaseUapi"],
        },
        "CORE": {
            "VULKAN_API": "{}.{}.{}".format(
                (caps["device"]["apiVersion"] >> 22) & 0x7F,
                (caps["device"]["apiVersion"] >> 12) & 0x3FF,
                caps["device"]["apiVersion"] & 0xFFF,
            ),
            "GEOMETRY_SHADER": features["geometryShader"],
            "TESSELLATION_SHADER": features["tessellationShader"],
            "FILL_MODE_NON_SOLID": features["fillModeNonSolid"],
            "MULTI_VIEWPORT": features["multiViewport"],
            "CLIP_DISTANCE": features["shaderClipDistance"],
            "CULL_DISTANCE": features["shaderCullDistance"],
            "BC": features["textureCompressionBC"],
            "PIPELINE_STATS": features["pipelineStatisticsQuery"],
            "SPARSE_BINDING": features["sparseBinding"],
        },
        "ROBUSTNESS2": {
            "EXTENSION": robustness2_ext,
            "ROBUST_BUFFER_ACCESS2": features["robustBufferAccess2"],
            "ROBUST_IMAGE_ACCESS2": features["robustImageAccess2"],
            "NULL_DESCRIPTOR": features["nullDescriptor"],
        },
        "DESCRIPTORS": {
            "DESCRIPTOR_INDEXING": features["descriptorIndexing"],
            "ALL_REQUIRED_INDEXING_BITS": indexing_ok,
            "MAX_UAB_STORAGE_BUFFER": dip.get("maxDescriptorSetUpdateAfterBindStorageBuffers"),
            "MAX_UAB_SAMPLED_IMAGE": dip.get("maxDescriptorSetUpdateAfterBindSampledImages"),
            "MAX_UAB_STORAGE_IMAGE": dip.get("maxDescriptorSetUpdateAfterBindStorageImages"),
            "MAX_UAB_SAMPLER": dip.get("maxDescriptorSetUpdateAfterBindSamplers"),
            "MAX_UAB_ALL_POOLS": dip.get("maxUpdateAfterBindDescriptorsInAllPools"),
        },
        "PUSH": {
            "MAX_PUSH_CONSTANTS": decode_limit(limits.get("maxPushConstantsSize")),
            "MAX_PUSH_DESCRIPTORS": push.get("maxPushDescriptors"),
        },
        "TRANSFORM_FEEDBACK": {
            "EXTENSION": xfb_ext,
            "TRANSFORM_FEEDBACK": features["transformFeedback"],
            "GEOMETRY_STREAMS": features["geometryStreams"],
            "QUERIES": queries,
        },
        "DXVK_1_10_3": {
            "D3D9": dxvk["1.10.3"]["D3D9"]["status"],
            "D3D10": dxvk["1.10.3"]["D3D10"]["status"],
            "D3D11_FL10_1": dxvk["1.10.3"]["D3D11_FL10_1"]["status"],
            "D3D11_FL11_0": dxvk["1.10.3"]["D3D11_FL11_0"]["status"],
        },
        "DXVK_2_7_1": {
            "COMMON": dxvk["2.7.1"]["COMMON"]["status"],
            "D3D9": dxvk["2.7.1"]["D3D9_PROFILE"]["status"],
            "D3D10_10_1": dxvk["2.7.1"]["D3D10_10_1_PROFILE"]["status"],
            "D3D11_11_0": dxvk["2.7.1"]["D3D11_11_0_PROFILE"]["status"],
        },
        "DXVK_3_1_1": {
            "COMMON": dxvk["3.1.1"]["COMMON"]["status"],
            "D3D9": dxvk["3.1.1"]["D3D9_PROFILE"]["status"],
            "D3D10_10_1": dxvk["3.1.1"]["D3D10_10_1_PROFILE"]["status"],
            "D3D11_11_0": dxvk["3.1.1"]["D3D11_11_0_PROFILE"]["status"],
        },
        "VKD3D_2_0": {
            "HARD": vkd3d["2.0"]["HARD_REQUIREMENTS"]["status"],
            "DEVICE": vkd3d["2.0"]["DEVICE_CREATE"]["status"],
        },
        "VKD3D_2_14_1": {
            "HARD": vkd3d["2.14.1"]["HARD_REQUIREMENTS"]["status"],
            "BASELINE": vkd3d["2.14.1"]["PROFILE_BASELINE"]["status"],
            "DEVICE": vkd3d["2.14.1"]["DEVICE_CREATE"]["status"],
            "FEATURE_LEVEL": vkd3d["2.14.1"]["FEATURE_LEVEL"]["status"],
        },
        "VKD3D_3_0_1": {
            "HARD": vkd3d["3.0.1"]["HARD_REQUIREMENTS"]["status"],
            "BASELINE": vkd3d["3.0.1"]["PROFILE_BASELINE"]["status"],
            "DEVICE": vkd3d["3.0.1"]["DEVICE_CREATE"]["status"],
            "FEATURE_LEVEL": vkd3d["3.0.1"]["FEATURE_LEVEL"]["status"],
        },
        "UPSTREAM_BACKPORTS": [
            "None in P23. P5-P8 descriptor/robustness2-buffer/push/common gates were already present at pinned Mesa 5a07217. No P23 backport series.",
        ],
        "NEW_IMPLEMENTATIONS": [
            "None. P9-P17, P20, P22 left missing graphics/XFB/tess/pipeline-stats/sparse bits false. P23 did not implement features.",
        ],
        "UNSUPPORTED": [
            "geometryShader",
            "tessellationShader",
            "fillModeNonSolid",
            "multiViewport",
            "shaderClipDistance",
            "shaderCullDistance",
            "textureCompressionBC (native=false, emulated=false)",
            "VK_EXT_transform_feedback",
            "transformFeedback",
            "geometryStreams",
            "transformFeedbackQueries",
            "pipelineStatisticsQuery",
            "robustImageAccess2",
            "vertexPipelineStoresAndAtomics",
            "sparseBinding / sparseResidency* / shaderResourceResidency / shaderResourceMinLod / VK_QUEUE_SPARSE_BINDING_BIT (P22 NO-GO)",
        ],
        "CTS": [
            "NOT_TESTED/BLOCKED. No deqp-vk executable or matching CTS test list on host or G615. No Vulkan conformance claim.",
        ],
        "REGRESSIONS": [
            "None from P23. No PanVK patch. No feature-bit change. Stock DXVK/vkd3d smoke remains BLOCKED (Android ICD is not a legal host for those Windows binaries).",
        ],
        "NEXT_SINGLE_BLOCKER": "robustImageAccess2 (P6 left false; stock vkd3d-proton 2.14.1/3.0.1 DEVICE_CREATE returns E_INVALIDARG). DXVK D3D9 still independently FAILs on geometryShader, fillModeNonSolid, shaderClipDistance, shaderCullDistance, textureCompressionBC.",
    }


def render_report_block(report):
    lines = [
        "PANVK:",
        f"COMMIT={report['PANVK']['COMMIT']}",
        f"MESA_COMMIT={report['PANVK']['MESA_COMMIT']}",
        f"PATCH_SERIES={report['PANVK']['PATCH_SERIES']}",
        f"DEVICE={report['PANVK']['DEVICE']}",
        f"GPU_ID={report['PANVK']['GPU_ID']}",
        f"KBASE_UAPI={report['PANVK']['KBASE_UAPI']}",
        "",
        "CORE:",
        f"VULKAN_API={report['CORE']['VULKAN_API']}",
        f"GEOMETRY_SHADER={report['CORE']['GEOMETRY_SHADER']}",
        f"TESSELLATION_SHADER={report['CORE']['TESSELLATION_SHADER']}",
        f"FILL_MODE_NON_SOLID={report['CORE']['FILL_MODE_NON_SOLID']}",
        f"MULTI_VIEWPORT={report['CORE']['MULTI_VIEWPORT']}",
        f"CLIP_DISTANCE={report['CORE']['CLIP_DISTANCE']}",
        f"CULL_DISTANCE={report['CORE']['CULL_DISTANCE']}",
        f"BC={report['CORE']['BC']}",
        f"PIPELINE_STATS={report['CORE']['PIPELINE_STATS']}",
        f"SPARSE_BINDING={report['CORE']['SPARSE_BINDING']}",
        "",
        "ROBUSTNESS2:",
        f"EXTENSION={report['ROBUSTNESS2']['EXTENSION']}",
        f"ROBUST_BUFFER_ACCESS2={report['ROBUSTNESS2']['ROBUST_BUFFER_ACCESS2']}",
        f"ROBUST_IMAGE_ACCESS2={report['ROBUSTNESS2']['ROBUST_IMAGE_ACCESS2']}",
        f"NULL_DESCRIPTOR={report['ROBUSTNESS2']['NULL_DESCRIPTOR']}",
        "",
        "DESCRIPTORS:",
        f"DESCRIPTOR_INDEXING={report['DESCRIPTORS']['DESCRIPTOR_INDEXING']}",
        f"ALL_REQUIRED_INDEXING_BITS={report['DESCRIPTORS']['ALL_REQUIRED_INDEXING_BITS']}",
        f"MAX_UAB_STORAGE_BUFFER={report['DESCRIPTORS']['MAX_UAB_STORAGE_BUFFER']}",
        f"MAX_UAB_SAMPLED_IMAGE={report['DESCRIPTORS']['MAX_UAB_SAMPLED_IMAGE']}",
        f"MAX_UAB_STORAGE_IMAGE={report['DESCRIPTORS']['MAX_UAB_STORAGE_IMAGE']}",
        f"MAX_UAB_SAMPLER={report['DESCRIPTORS']['MAX_UAB_SAMPLER']}",
        f"MAX_UAB_ALL_POOLS={report['DESCRIPTORS']['MAX_UAB_ALL_POOLS']}",
        "",
        "PUSH:",
        f"MAX_PUSH_CONSTANTS={report['PUSH']['MAX_PUSH_CONSTANTS']}",
        f"MAX_PUSH_DESCRIPTORS={report['PUSH']['MAX_PUSH_DESCRIPTORS']}",
        "",
        "TRANSFORM_FEEDBACK:",
        f"EXTENSION={report['TRANSFORM_FEEDBACK']['EXTENSION']}",
        f"TRANSFORM_FEEDBACK={report['TRANSFORM_FEEDBACK']['TRANSFORM_FEEDBACK']}",
        f"GEOMETRY_STREAMS={report['TRANSFORM_FEEDBACK']['GEOMETRY_STREAMS']}",
        f"QUERIES={report['TRANSFORM_FEEDBACK']['QUERIES']}",
        "",
        "DXVK_1_10_3:",
        f"D3D9={report['DXVK_1_10_3']['D3D9']}",
        f"D3D10={report['DXVK_1_10_3']['D3D10']}",
        f"D3D11_FL10_1={report['DXVK_1_10_3']['D3D11_FL10_1']}",
        f"D3D11_FL11_0={report['DXVK_1_10_3']['D3D11_FL11_0']}",
        "",
        "DXVK_2_7_1:",
        f"COMMON={report['DXVK_2_7_1']['COMMON']}",
        f"D3D9={report['DXVK_2_7_1']['D3D9']}",
        f"D3D10_10_1={report['DXVK_2_7_1']['D3D10_10_1']}",
        f"D3D11_11_0={report['DXVK_2_7_1']['D3D11_11_0']}",
        "",
        "DXVK_3_1_1:",
        f"COMMON={report['DXVK_3_1_1']['COMMON']}",
        f"D3D9={report['DXVK_3_1_1']['D3D9']}",
        f"D3D10_10_1={report['DXVK_3_1_1']['D3D10_10_1']}",
        f"D3D11_11_0={report['DXVK_3_1_1']['D3D11_11_0']}",
        "",
        "VKD3D_2_0:",
        f"HARD={report['VKD3D_2_0']['HARD']}",
        f"DEVICE={report['VKD3D_2_0']['DEVICE']}",
        "",
        "VKD3D_2_14_1:",
        f"HARD={report['VKD3D_2_14_1']['HARD']}",
        f"BASELINE={report['VKD3D_2_14_1']['BASELINE']}",
        f"DEVICE={report['VKD3D_2_14_1']['DEVICE']}",
        f"FEATURE_LEVEL={report['VKD3D_2_14_1']['FEATURE_LEVEL']}",
        "",
        "VKD3D_3_0_1:",
        f"HARD={report['VKD3D_3_0_1']['HARD']}",
        f"BASELINE={report['VKD3D_3_0_1']['BASELINE']}",
        f"DEVICE={report['VKD3D_3_0_1']['DEVICE']}",
        f"FEATURE_LEVEL={report['VKD3D_3_0_1']['FEATURE_LEVEL']}",
        "",
        "UPSTREAM_BACKPORTS:",
    ]
    lines += [f"- {item}" for item in report["UPSTREAM_BACKPORTS"]]
    lines += ["", "NEW_IMPLEMENTATIONS:"]
    lines += [f"- {item}" for item in report["NEW_IMPLEMENTATIONS"]]
    lines += ["", "UNSUPPORTED:"]
    lines += [f"- {item}" for item in report["UNSUPPORTED"]]
    lines += ["", "CTS:"]
    lines += [f"- {item}" for item in report["CTS"]]
    lines += ["", "REGRESSIONS:"]
    lines += [f"- {item}" for item in report["REGRESSIONS"]]
    lines += ["", f"NEXT_SINGLE_BLOCKER={report['NEXT_SINGLE_BLOCKER']}"]
    return "\n".join(lines)


def render_beta6_section(status):
    rows = [
        "| Feature | Patch | CTS |",
        "|---|---|---|",
    ]
    for item in status["exposedSinceCapture"]:
        rows.append(f"| `{item['feature']}` | `{item['patch']}` | {item['cts']} |")
    open_items = "\n".join(f"- {item}" for item in status["openItems"])
    deferred = "\n".join(f"- {item}" for item in status["deferred"])
    body = [
        "## Beta.6 status (current)",
        "",
        f"Release `{status['release']}`. Patches through `{status['patchesUpTo']}`.",
        "",
        status["note"],
        "",
        "Exposed since the beta.3 capture:",
        "",
        *rows,
        "",
        "Open items:",
        "",
        open_items,
        "",
        "Deferred:",
        "",
        deferred,
    ]
    return "\n".join(body)


render_beta5_section = render_beta6_section


def render_md(doc):
    dxvk = doc["dxvk"]
    vkd3d = doc["vkd3d"]
    driver = doc["driver"]
    report = render_report_block(doc["workerReport"])
    status = doc.get("beta6Status", doc.get("beta5Status", {}))
    return "\n".join(
        [
            "# P23 Final DXVK/vkd3d compliance matrix",
            "",
            render_beta6_section(status),
            "",
            "## Result",
            "",
            f"`{doc['result']}`",
            "",
            "Machine-evaluated matrix only. No PanVK patch. No feature-bit change.",
            "HARD_REQUIREMENTS, PROFILE_BASELINE, DEVICE_CREATE, and FEATURE_LEVEL stay unmerged.",
            "Stock DXVK/vkd3d smoke is BLOCKED. No invented hardware results.",
            "",
            "```text",
            f"DXVK 1.10.3:",
            f"D3D9={dxvk['1.10.3']['D3D9']['status']}",
            f"D3D10={dxvk['1.10.3']['D3D10']['status']}",
            f"D3D11_FL10_1={dxvk['1.10.3']['D3D11_FL10_1']['status']}",
            f"D3D11_FL11_0={dxvk['1.10.3']['D3D11_FL11_0']['status']}",
            f"MAX_FEATURE_LEVEL={dxvk['1.10.3']['MAX_FEATURE_LEVEL']}",
            "",
            f"DXVK 2.7.1:",
            f"COMMON={dxvk['2.7.1']['COMMON']['status']}",
            f"D3D9_PROFILE={dxvk['2.7.1']['D3D9_PROFILE']['status']}",
            f"D3D10_10_1_PROFILE={dxvk['2.7.1']['D3D10_10_1_PROFILE']['status']}",
            f"D3D11_11_0_PROFILE={dxvk['2.7.1']['D3D11_11_0_PROFILE']['status']}",
            f"D3D11_11_1_PROFILE={dxvk['2.7.1']['D3D11_11_1_PROFILE']['status']}",
            f"MAX_FEATURE_LEVEL={dxvk['2.7.1']['MAX_FEATURE_LEVEL']}",
            "",
            f"DXVK 3.1.1:",
            f"COMMON={dxvk['3.1.1']['COMMON']['status']}",
            f"D3D9_PROFILE={dxvk['3.1.1']['D3D9_PROFILE']['status']}",
            f"D3D10_10_1_PROFILE={dxvk['3.1.1']['D3D10_10_1_PROFILE']['status']}",
            f"D3D11_11_0_PROFILE={dxvk['3.1.1']['D3D11_11_0_PROFILE']['status']}",
            f"D3D11_11_1_PROFILE={dxvk['3.1.1']['D3D11_11_1_PROFILE']['status']}",
            f"MAX_FEATURE_LEVEL={dxvk['3.1.1']['MAX_FEATURE_LEVEL']}",
            "",
            f"VKD3D 2.0:",
            f"HARD_REQUIREMENTS={vkd3d['2.0']['HARD_REQUIREMENTS']['status']}",
            f"DEVICE_CREATE={vkd3d['2.0']['DEVICE_CREATE']['status']}",
            "",
            f"VKD3D 2.14.1:",
            f"HARD_REQUIREMENTS={vkd3d['2.14.1']['HARD_REQUIREMENTS']['status']}",
            f"PROFILE_BASELINE={vkd3d['2.14.1']['PROFILE_BASELINE']['status']}",
            f"DEVICE_CREATE={vkd3d['2.14.1']['DEVICE_CREATE']['status']}",
            f"FEATURE_LEVEL={vkd3d['2.14.1']['FEATURE_LEVEL']['status']}",
            "",
            f"VKD3D 3.0.1:",
            f"HARD_REQUIREMENTS={vkd3d['3.0.1']['HARD_REQUIREMENTS']['status']}",
            f"PROFILE_BASELINE={vkd3d['3.0.1']['PROFILE_BASELINE']['status']}",
            f"DEVICE_CREATE={vkd3d['3.0.1']['DEVICE_CREATE']['status']}",
            f"FEATURE_LEVEL={vkd3d['3.0.1']['FEATURE_LEVEL']['status']}",
            "```",
            "",
            "## Adapter",
            "",
            f"- Device: `{driver['adapter']}`",
            f"- Target: `{doc['capture']['target']}`",
            f"- VendorID: `{driver['vendorID']}`",
            f"- DeviceID: `{driver['deviceIDHex']}`",
            f"- Driver: `{driver['driverName']}` / `{driver['driverInfo']}`",
            f"- Vulkan API: `{driver['apiVersionText']}`",
            f"- ICD SHA-256: `{doc['capture']['icdSha256']}`",
            "",
            "## Gate separation",
            "",
            "DXVK COMMON is `dxvk_common_required` only. D3D9/D3D10/D3D11 profiles are not COMMON.",
            "vkd3d HARD_REQUIREMENTS is each tag README. PROFILE_BASELINE is `VP_D3D12_FL_11_0_baseline`.",
            "DEVICE_CREATE is `vkd3d_init_device_caps` E_INVALIDARG. FEATURE_LEVEL is stock `CheckFeatureSupport`.",
            "Implied D3D_FEATURE_LEVEL_11_0 from P21 is not FEATURE_LEVEL and is not a PASS.",
            "P19 DEVICE_CREATE listed only `robustImageAccess2`. P21 also fails `transformFeedbackQueries`. P23 uses P21.",
            "",
            "## Worker report",
            "",
            "```text",
            report,
            "```",
            "",
            "## Sources",
            "",
            "- `validation/g615-v11-csf/consumer-capabilities.json`",
            "- `validation/g615-v11-csf/p13-d3d9.json`",
            "- `validation/g615-v11-csf/p16-d3d10.json`",
            "- `validation/g615-v11-csf/p18-d3d11-fl11.json`",
            "- `validation/g615-v11-csf/p19-vkd3d-profile-baseline.json`",
            "- `validation/g615-v11-csf/p21-d3d12-feature-level.json`",
            "- `validation/g615-v11-csf/p22-kbase-sparse-feasibility.json`",
            "- live `scripts/evaluate-consumer-profile.py` for COMMON and `VP_DXVK_d3d11_level_11_1_baseline`",
            "",
            "## Exit",
            "",
            "P23 exits `FAIL`. Matrix is complete. No spoofed feature bits. Next engineering step is not sparse.",
            "",
        ]
    )


def render_txt(doc):
    dxvk = doc["dxvk"]
    vkd3d = doc["vkd3d"]
    lines = [
        "P23 final DXVK/vkd3d compliance matrix",
        f"result={doc['result']}",
        f"adapter={doc['driver']['adapter']}",
        f"driver={doc['driver']['driverName']} {doc['driver']['driverInfo']}",
        f"api={doc['driver']['apiVersionText']}",
        f"icd={doc['capture']['icdSha256']}",
        f"DXVK_1_10_3 D3D9={dxvk['1.10.3']['D3D9']['status']} D3D10={dxvk['1.10.3']['D3D10']['status']} D3D11_FL10_1={dxvk['1.10.3']['D3D11_FL10_1']['status']} D3D11_FL11_0={dxvk['1.10.3']['D3D11_FL11_0']['status']} MAX={dxvk['1.10.3']['MAX_FEATURE_LEVEL']}",
        f"DXVK_2_7_1 COMMON={dxvk['2.7.1']['COMMON']['status']} D3D9={dxvk['2.7.1']['D3D9_PROFILE']['status']} D3D10_10_1={dxvk['2.7.1']['D3D10_10_1_PROFILE']['status']} D3D11_11_0={dxvk['2.7.1']['D3D11_11_0_PROFILE']['status']} D3D11_11_1={dxvk['2.7.1']['D3D11_11_1_PROFILE']['status']} MAX={dxvk['2.7.1']['MAX_FEATURE_LEVEL']}",
        f"DXVK_3_1_1 COMMON={dxvk['3.1.1']['COMMON']['status']} D3D9={dxvk['3.1.1']['D3D9_PROFILE']['status']} D3D10_10_1={dxvk['3.1.1']['D3D10_10_1_PROFILE']['status']} D3D11_11_0={dxvk['3.1.1']['D3D11_11_0_PROFILE']['status']} D3D11_11_1={dxvk['3.1.1']['D3D11_11_1_PROFILE']['status']} MAX={dxvk['3.1.1']['MAX_FEATURE_LEVEL']}",
        f"VKD3D_2_0 HARD={vkd3d['2.0']['HARD_REQUIREMENTS']['status']} DEVICE={vkd3d['2.0']['DEVICE_CREATE']['status']}",
        f"VKD3D_2_14_1 HARD={vkd3d['2.14.1']['HARD_REQUIREMENTS']['status']} BASELINE={vkd3d['2.14.1']['PROFILE_BASELINE']['status']} DEVICE={vkd3d['2.14.1']['DEVICE_CREATE']['status']} FEATURE_LEVEL={vkd3d['2.14.1']['FEATURE_LEVEL']['status']}",
        f"VKD3D_3_0_1 HARD={vkd3d['3.0.1']['HARD_REQUIREMENTS']['status']} BASELINE={vkd3d['3.0.1']['PROFILE_BASELINE']['status']} DEVICE={vkd3d['3.0.1']['DEVICE_CREATE']['status']} FEATURE_LEVEL={vkd3d['3.0.1']['FEATURE_LEVEL']['status']}",
        "P23_EXIT=FAIL",
    ]
    return "\n".join(lines) + "\n"


def driver_identity(caps):
    packed = caps["device"]["driverVersion"]
    driver = caps.get("propertyStructures", {}).get("VkPhysicalDeviceDriverProperties", {})
    return {
        "adapter": caps["device"]["name"],
        "vendorID": caps["device"]["vendorID"],
        "deviceID": caps["device"]["deviceID"],
        "deviceIDHex": f"0x{caps['device']['deviceID']:08x}",
        "driverVersionPacked": packed,
        "driverVersionHex": f"0x{packed:08x}",
        "apiVersion": caps["device"]["apiVersion"],
        "apiVersionText": "{}.{}.{}".format(
            (caps["device"]["apiVersion"] >> 22) & 0x7F,
            (caps["device"]["apiVersion"] >> 12) & 0x3FF,
            caps["device"]["apiVersion"] & 0xFFF,
        ),
        "driverID": driver.get("driverID"),
        "driverName": decode_hex_cstring(driver.get("driverName")),
        "driverInfo": decode_hex_cstring(driver.get("driverInfo")),
    }


def main():
    evaluator = load_evaluator()
    caps = json.loads(CAPS_PATH.read_text())
    p13 = json.loads(P13_PATH.read_text())
    p16 = json.loads(P16_PATH.read_text())
    p18 = json.loads(P18_PATH.read_text())
    p19 = json.loads(P19_PATH.read_text())
    p21 = json.loads(P21_PATH.read_text())
    p22 = json.loads(P22_PATH.read_text())
    common = {version: evaluate_common(caps, evaluator, version) for version in ("2.7.1", "3.1.1")}
    fl11_1 = {version: evaluate_fl11_1(caps, evaluator, version) for version in ("2.7.1", "3.1.1")}
    dxvk = {
        "1.10.3": dxvk_110(p13, p16, p18),
        "2.7.1": dxvk_official("2.7.1", p13, p16, p18, common["2.7.1"], fl11_1["2.7.1"]),
        "3.1.1": dxvk_official("3.1.1", p13, p16, p18, common["3.1.1"], fl11_1["3.1.1"]),
    }
    vkd3d = {version: vkd3d_from_p21(p21, version) for version in ("2.0", "2.14.1", "3.0.1")}
    report = worker_report(caps, dxvk, vkd3d, p21, p22)
    bits = {name: caps["features"][name] for name in FALSE_BITS}
    doc = {
        "schemaVersion": 1,
        "phase": "P23",
        "date": "2026-09-20",
        "beta6Status": BETA6_STATUS,
        "result": "FAIL",
        "smoke": "BLOCKED",
        "pinnedMesa": PINNED_MESA,
        "auditedMesa": AUDITED_MESA,
        "panvkCommit": PANVK_COMMIT,
        "patchSeries": PATCH_SERIES,
        "capabilities": str(CAPS_PATH.relative_to(ROOT)),
        "evaluator": "scripts/evaluate-consumer-profile.py",
        "capture": caps["capture"],
        "device": caps["device"],
        "driver": driver_identity(caps),
        "gatesUnmerged": ["HARD_REQUIREMENTS", "PROFILE_BASELINE", "DEVICE_CREATE", "FEATURE_LEVEL"],
        "featureBitsUnchanged": bits,
        "sources": {
            "P13": str(P13_PATH.relative_to(ROOT)),
            "P16": str(P16_PATH.relative_to(ROOT)),
            "P18": str(P18_PATH.relative_to(ROOT)),
            "P19": str(P19_PATH.relative_to(ROOT)),
            "P21": str(P21_PATH.relative_to(ROOT)),
            "P22": str(P22_PATH.relative_to(ROOT)),
        },
        "p19DeviceCreateNote": {
            "p19Blockers": p19["versions"]["2.14.1"]["DEVICE_CREATE"]["blockers"],
            "p21Blockers": p21["versions"]["2.14.1"]["DEVICE_CREATE"]["blockers"],
            "matrixUses": "P21",
        },
        "dxvk": dxvk,
        "vkd3d": vkd3d,
        "workerReport": report,
        "outOfScope": ["feature-bit flips", "missing-feature implementation", "stock DXVK/vkd3d launch", "CTS"],
    }
    JSON_OUT.write_text(json.dumps(doc, indent=2) + "\n")
    MD_OUT.write_text(render_md(doc))
    TXT_OUT.write_text(render_txt(doc))
    print(f"PASS output={JSON_OUT}")
    print(render_report_block(report))
    print("P23_EXIT=FAIL")


if __name__ == "__main__":
    main()
