#!/usr/bin/env python3
"""Compare imported Vulkan Profiles requirements with a capability capture."""
import argparse
import json
import pathlib
import re
import sys


MISSING = object()
SUPPORTED_SECTIONS = {"extensions", "features", "properties"}

# Vulkan registry limittype values for every property used by the pinned DXVK
# 2.7.1 and 3.1.1 profiles. Unknown properties fail closed.
PROPERTY_COMPARISONS = {
    # Existing vkd3d reports define this legacy gate as a required floor.
    "bufferImageGranularity": "max",
    "degenerateTrianglesRasterized": "exact",
    "denormBehaviorIndependence": "exact",
    "filterMinmaxSingleComponentFormats": "max",
    "fragmentShadingRateNonTrivialCombinerOps": "max",
    "fullyCoveredFragmentShaderInputVariable": "max",
    "graphicsPipelineLibraryFastLinking": "max",
    "graphicsPipelineLibraryIndependentInterpolationDecoration": "max",
    "maxBufferSize": "max",
    "maxCustomBorderColorSamplers": "max",
    "maxDescriptorSetInlineUniformBlocks": "max",
    "maxDescriptorSetUpdateAfterBindInlineUniformBlocks": "max",
    "maxInlineUniformBlockSize": "max",
    "maxInlineUniformTotalSize": "max",
    "maxMultiviewInstanceIndex": "max",
    "maxMultiviewViewCount": "max",
    "maxPerStageDescriptorInlineUniformBlocks": "max",
    "maxPerStageDescriptorUpdateAfterBindInlineUniformBlocks": "max",
    "maxPerStageDescriptorUpdateAfterBindSampledImages": "max",
    "maxPerStageDescriptorUpdateAfterBindStorageBuffers": "max",
    "maxPerStageDescriptorUpdateAfterBindStorageImages": "max",
    "maxPushConstantsSize": "max",
    "maxPushDescriptors": "max",
    "maxTimelineSemaphoreValueDifference": "max",
    "requiredSubgroupSizeStages": "bitmask",
    "residencyAlignedMipSize": "min",
    "residencyNonResidentStrict": "max",
    "residencyStandard2DBlockShape": "max",
    "residencyStandard3DBlockShape": "max",
    "robustBufferAccessUpdateAfterBind": "max",
    "shaderDenormFlushToZeroFloat32": "max",
    "shaderDenormPreserveFloat16": "max",
    "shaderDenormPreserveFloat32": "max",
    "shaderDenormPreserveFloat64": "max",
    "storageTexelBufferOffsetSingleTexelAlignment": "exact",
    "subgroupSize": "max",
    "subgroupSupportedOperations": "bitmask",
    "subgroupSupportedStages": "bitmask",
    "transformFeedbackDraw": "max",
    "transformFeedbackQueries": "max",
    "transformFeedbackRasterizationStreamSelect": "max",
    "transformFeedbackStreamsLinesTriangles": "max",
    "uniformTexelBufferOffsetSingleTexelAlignment": "exact",
}


def flatten(value, prefix=""):
    out = {}
    if isinstance(value, dict):
        for key, child in value.items():
            out.update(flatten(child, f"{prefix}.{key}" if prefix else key))
    elif isinstance(value, list):
        for index, child in enumerate(value):
            out.update(flatten(child, f"{prefix}[{index}]"))
    else:
        out[prefix] = value
    return out


def _lookup(mapping, path):
    value = mapping
    for part in path:
        match = re.fullmatch(r"([^\[]+)(?:\[(\d+)\])?", part)
        if not match or not isinstance(value, dict) or match.group(1) not in value:
            return MISSING
        value = value[match.group(1)]
        if match.group(2) is not None:
            index = int(match.group(2))
            if not isinstance(value, list) or index >= len(value):
                return MISSING
            value = value[index]
    return value


def actual_for(path, caps):
    parts = path.split(".")
    section, structure, fields = parts[0], parts[1], parts[2:]
    if section == "extensions" and len(parts) == 2:
        return next(
            (item.get("specVersion", 0) for item in caps.get("extensions", []) if item.get("name") == structure),
            0,
        )

    stores = {
        "features": "featureStructures",
        "properties": "propertyStructures",
    }
    store = caps.get(stores.get(section, ""), {})
    value = _lookup(store.get(structure, {}), fields)
    if value is not MISSING:
        return value

    # Captures retain promoted core/extension aliases in these complete flat maps.
    flat = caps.get(section, {})
    value = _lookup(flat, fields)
    if value is not MISSING:
        return value

    if section == "properties" and structure == "VkPhysicalDeviceProperties":
        value = _lookup(caps.get("coreProperties", {}), fields)
        if value is not MISSING:
            if isinstance(value, str):
                try:
                    return int.from_bytes(bytes.fromhex(value), "little")
                except ValueError:
                    return MISSING
            return value
        value = _lookup(caps.get("properties", {}), fields)
        if value is not MISSING:
            return value
    return MISSING


def compare(actual, required, comparison):
    if actual is MISSING:
        return "UNKNOWN"
    if comparison == "exact":
        return "PASS" if actual == required else "FAIL"
    if comparison == "max":
        return "PASS" if type(actual) is type(required) and actual >= required else "FAIL"
    if comparison == "min":
        return "PASS" if type(actual) is type(required) and actual <= required else "FAIL"
    if comparison == "bitmask":
        if type(actual) is type(required) is int:
            return "PASS" if actual & required == required else "FAIL"
        return "PASS" if isinstance(actual, str) and actual == required else "FAIL"
    if comparison == "range":
        valid = isinstance(actual, list) and isinstance(required, list) and len(actual) == len(required) == 2 and actual[0] <= required[0] and actual[1] >= required[1]
        return "PASS" if valid else "FAIL"
    if comparison == "alignment":
        valid = type(actual) is type(required) is int and actual > 0 and actual <= required and required % actual == 0
        return "PASS" if valid else "FAIL"
    if comparison == "array-exact":
        return "PASS" if actual == required else "FAIL"
    return "UNKNOWN"


def comparison_for(path, required):
    section = path.split(".", 1)[0]
    if section == "features":
        return "exact" if isinstance(required, bool) else None
    if section == "extensions":
        return "max" if isinstance(required, int) and not isinstance(required, bool) else None
    if section == "properties":
        leaf = re.sub(r"\[\d+\]$", "", path.rsplit(".", 1)[-1])
        comparison = PROPERTY_COMPARISONS.get(leaf)
        return "exact" if "[" in path and comparison == "bitmask" else comparison
    return None


def item_status(item):
    return item.get("status", compare(item["actual"], item["required"], item.get("comparison")))


def satisfies(actual, required, comparison="max"):
    """Compatibility helper for existing callers; new code supplies a comparator."""
    return compare(actual, required, comparison) == "PASS"


def item_satisfies(item):
    return item_status(item) == "PASS"


def api_satisfies(actual, required):
    if not isinstance(actual, int) or not isinstance(required, str):
        return False
    actual_version = ((actual >> 22) & 0x7f, (actual >> 12) & 0x3ff, actual & 0xfff)
    try:
        required_version = tuple(int(x) for x in required.split("."))
    except ValueError:
        return False
    return actual_version >= required_version


def evaluate(caps, imported, property_comparisons=None):
    doc = imported["document"]
    property_comparisons = {**PROPERTY_COMPARISONS, **(property_comparisons or {})}
    results = {}
    for profile_name, profile in doc.get("profiles", {}).items():
        api_required = profile.get("api-version")
        api_actual = caps.get("device", {}).get("apiVersion", MISSING)
        api_status = "UNKNOWN" if api_actual is MISSING else "PASS" if api_satisfies(api_actual, api_required) else "FAIL"
        rows = [{
            "capability": "profile",
            "path": "api-version",
            "required": api_required,
            "actual": None if api_actual is MISSING else api_actual,
            "comparison": "api-version",
            "status": api_status,
        }]
        feature_levels = {}
        for capability_name in profile.get("capabilities", []):
            capability = doc.get("capabilities", {}).get(capability_name)
            capability_rows = []
            if not isinstance(capability, dict):
                capability_rows.append({
                    "capability": capability_name,
                    "path": capability_name,
                    "required": "defined capability",
                    "actual": None,
                    "comparison": None,
                    "status": "UNKNOWN",
                })
            else:
                unsupported = set(capability) - SUPPORTED_SECTIONS
                for section in sorted(unsupported):
                    capability_rows.append({
                        "capability": capability_name,
                        "path": section,
                        "required": capability[section],
                        "actual": None,
                        "comparison": None,
                        "status": "UNKNOWN",
                    })
                for path, required in flatten({key: capability[key] for key in capability if key in SUPPORTED_SECTIONS}).items():
                    actual = actual_for(path, caps)
                    if path.startswith("properties."):
                        leaf = re.sub(r"\[\d+\]$", "", path.rsplit(".", 1)[-1])
                        comparison = property_comparisons.get(leaf)
                        if "[" in path and comparison == "bitmask":
                            comparison = "exact"
                    else:
                        comparison = comparison_for(path, required)
                    capability_rows.append({
                        "capability": capability_name,
                        "path": path,
                        "required": required,
                        "actual": None if actual is MISSING else actual,
                        "comparison": comparison,
                        "status": compare(actual, required, comparison),
                    })
            rows.extend(capability_rows)
            if capability_name.startswith("fl_") or "level" in capability_name:
                statuses = {item_status(item) for item in capability_rows}
                feature_levels[capability_name] = "FAIL" if "FAIL" in statuses else "UNKNOWN" if "UNKNOWN" in statuses else "PASS"
        # `fail` remains the historical non-pass collection for existing report
        # consumers; each row's status distinguishes FAIL from UNKNOWN.
        failed = [item for item in rows if item_status(item) != "PASS"]
        unknown = [item for item in rows if item_status(item) == "UNKNOWN"]
        passed = [item for item in rows if item_status(item) == "PASS"]
        status = "FAIL" if any(item_status(item) == "FAIL" for item in failed) else "UNKNOWN" if unknown else "PASS"
        results[profile_name] = {
            "status": status,
            "pass": passed,
            "fail": failed,
            "unknown": unknown,
            "optional": [],
            "featureLevel": {"label": profile.get("label", profile_name), "capabilities": feature_levels},
        }
    return {"schemaVersion": 2, "profileSource": imported["provenance"], "profiles": results}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capabilities", required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--output")
    parser.add_argument("--select", metavar="PROFILE", help="exit nonzero unless this profile passes")
    args = parser.parse_args()
    profile_path = pathlib.Path(args.profile)
    imported = json.loads(profile_path.read_text())
    result = evaluate(json.loads(pathlib.Path(args.capabilities).read_text()), imported)
    text = json.dumps(result, indent=2) + "\n"
    if args.output:
        pathlib.Path(args.output).write_text(text)
    else:
        print(text, end="")
    if args.select:
        selected = result["profiles"].get(args.select)
        if selected is None:
            parser.error(f"unknown profile: {args.select}")
        if selected["status"] != "PASS":
            print(f"selected profile {args.select}: {selected['status']}", file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
