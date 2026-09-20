#!/usr/bin/env python3
"""Evaluate only DXVK requirements against a captured Vulkan capability set."""

import argparse
import copy
import importlib.util
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CAPS = ROOT / "validation/g615-v11-csf/consumer-capabilities.json"
DEFAULT_JSON = ROOT / "validation/g615-v11-csf/dxvk/DX1-REPORT.json"
DEFAULT_MD = ROOT / "validation/g615-v11-csf/dxvk/DX1-REPORT.md"
PROFILES = {
    "2.7.1": ROOT / "validation/g615-v11-csf/profiles/dxvk-2.7.1.json",
    "3.1.1": ROOT / "validation/g615-v11-csf/profiles/dxvk-3.1.1.json",
}
LEGACY = ROOT / "validation/requirements/dxvk-1.10.3.json"


def load_profile_evaluator():
    spec = importlib.util.spec_from_file_location(
        "evaluate_consumer_profile", ROOT / "scripts/evaluate-consumer-profile.py"
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def leaf(path):
    return path.rsplit(".", 1)[-1]


def extension_version(caps, name):
    return next(
        (item.get("specVersion", 0) for item in caps.get("extensions", []) if item.get("name") == name),
        None,
    )


def gate(status, rows, **metadata):
    failures = [row for row in rows if row["status"] == "FAIL"]
    result = {
        "status": status,
        "passCount": len(rows) - len(failures),
        "failCount": len(failures),
        "blockers": [row["requirement"] for row in failures],
        "requirements": rows,
    }
    result.update(metadata)
    return result


def evaluate_official(caps, version, evaluator=None):
    evaluator = evaluator or load_profile_evaluator()
    path = PROFILES[version]
    imported = json.loads(path.read_text())
    evaluated = evaluator.evaluate(caps, imported)
    profiles = {}
    for name, result in evaluated["profiles"].items():
        rows = []
        for item in result["pass"] + result["fail"]:
            rows.append(
                {
                    "requirement": leaf(item["path"]),
                    "path": item["path"],
                    "capability": item["capability"],
                    "required": item["required"],
                    "actual": item["actual"],
                    "status": "PASS" if evaluator.item_satisfies(item) else "FAIL",
                }
            )
        profiles[name] = gate(
            result["status"],
            rows,
            optional=result["optional"],
            label=result["featureLevel"]["label"],
            classification="BASELINE" if name.endswith("_baseline") else "OPTIMAL",
        )

    common_rows = [
        row
        for row in profiles["VP_DXVK_d3d9_baseline"]["requirements"]
        if row["capability"] == "dxvk_common_required"
    ]
    return {
        "sourceKind": "official-profile",
        "sourceFile": str(path.relative_to(ROOT)),
        "provenance": imported["provenance"],
        "common": gate(
            "PASS" if all(row["status"] == "PASS" for row in common_rows) else "FAIL",
            common_rows,
        ),
        "profiles": profiles,
    }


def legacy_features(manifest, tier_name):
    tier = manifest["tiers"][tier_name]
    features = []
    extensions = []
    if tier.get("inherits"):
        features, extensions = legacy_features(manifest, tier["inherits"])
    return (
        features + list(tier.get("requiredFeatures", [])),
        extensions + list(tier.get("requiredExtensions", [])),
    )


def evaluate_legacy(caps):
    manifest = json.loads(LEGACY.read_text())
    tiers = {}
    for name in manifest["tiers"]:
        features, extensions = legacy_features(manifest, name)
        rows = []
        for feature in features:
            actual = caps.get("features", {}).get(feature)
            rows.append(
                {
                    "requirement": feature,
                    "required": True,
                    "actual": actual,
                    "status": "PASS" if actual is True else "FAIL",
                }
            )
        for extension in extensions:
            actual = extension_version(caps, extension)
            rows.append(
                {
                    "requirement": extension,
                    "required": 1,
                    "actual": actual,
                    "status": "PASS" if actual is not None else "FAIL",
                }
            )
        tiers[name] = gate(
            "PASS" if all(row["status"] == "PASS" for row in rows) else "FAIL", rows
        )
    return {
        "sourceKind": "source-derived",
        "sourceFile": str(LEGACY.relative_to(ROOT)),
        "repository": manifest["repository"],
        "tag": manifest["tag"],
        "tagCommit": manifest["tagCommit"],
        "tiers": tiers,
    }


def evaluate(caps):
    evaluator = load_profile_evaluator()
    return {
        "schemaVersion": 1,
        "phase": "DX1",
        "scope": "DXVK_ONLY",
        "capabilities": str(DEFAULT_CAPS.relative_to(ROOT)),
        "device": caps.get("device", {}),
        "versions": {
            "1.10.3": evaluate_legacy(caps),
            "2.7.1": evaluate_official(caps, "2.7.1", evaluator),
            "3.1.1": evaluate_official(caps, "3.1.1", evaluator),
        },
    }


def render_markdown(report):
    lines = [
        "# DX1 DXVK-only profile report",
        "",
        "Generated by `scripts/evaluate-dxvk.py`. Profile results are capability gates, not CTS or DXVK Native results.",
        "",
        "| DXVK | Gate | Class | Status | Blockers |",
        "| --- | --- | --- | --- | --- |",
    ]
    legacy = report["versions"]["1.10.3"]
    for name, result in legacy["tiers"].items():
        lines.append(
            f"| 1.10.3 | `{name}` | BASELINE | {result['status']} | "
            f"{', '.join(f'`{item}`' for item in result['blockers']) or 'None'} |"
        )
    for version in ("2.7.1", "3.1.1"):
        item = report["versions"][version]
        common = item["common"]
        lines.append(
            f"| {version} | `COMMON` | BASELINE | {common['status']} | "
            f"{', '.join(f'`{name}`' for name in common['blockers']) or 'None'} |"
        )
        for name, result in item["profiles"].items():
            lines.append(
                f"| {version} | `{name}` | {result['classification']} | {result['status']} | "
                f"{', '.join(f'`{blocker}`' for blocker in result['blockers']) or 'None'} |"
            )
    lines += [
        "",
        "## Runtime status",
        "",
        "- Vulkan CTS: `NOT_TESTED` (DX2).",
        "- DXVK Native: `NOT_TESTED` (DX20-DX22).",
        "- Feature advertisement: unchanged.",
        "",
    ]
    return "\n".join(lines)


def report_summary(report):
    summary = copy.deepcopy(report)
    for version, result in summary["versions"].items():
        gates = result["tiers"].values() if version == "1.10.3" else (
            result["common"],
            *result["profiles"].values(),
        )
        for result in gates:
            result.pop("requirements", None)
            optional = result.pop("optional", [])
            if optional:
                result["optionalFailCount"] = sum(item["status"] == "FAIL" for item in optional)
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capabilities", type=Path, default=DEFAULT_CAPS)
    parser.add_argument("--json-output", type=Path, default=DEFAULT_JSON)
    parser.add_argument("--markdown-output", type=Path, default=DEFAULT_MD)
    args = parser.parse_args()
    report = evaluate(json.loads(args.capabilities.read_text()))
    args.json_output.write_text(json.dumps(report_summary(report), indent=2) + "\n")
    args.markdown_output.write_text(render_markdown(report))
    print(f"DXVK report: {args.json_output}")


if __name__ == "__main__":
    main()
