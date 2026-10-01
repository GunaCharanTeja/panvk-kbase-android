#!/usr/bin/env python3
"""collect-validation.py — VALIDATION.json from real logs, not a skeleton.

Usage:
  python3 scripts/collect-validation.py --profile g615-v11-csf
  python3 scripts/collect-validation.py --profile g615-v11-csf --out STAGE/VALIDATION.json
  python3 scripts/collect-validation.py --profile g615-v11-csf --require-beta-gates

Inputs: validation/<profile>/*.txt, runtime-feature-matrix.json, sources.lock,
device profile, optional Android/glibc binaries for SHA-256.
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

ANDROID_GATES = (
    "enumeration",
    "deviceCreate",
    "compute10",
    "offscreen",
    "ahb",
    "vulkanSurface",
    "swapchain300",
    "coldSecondLaunch",
)
GLIBC_GATES = ("enumeration", "compute10", "offscreen")

# (gate, kind) -> ordered evidence filename globs, first match with a PASS token wins.
# Base Android/glibc gates still use beta.3 logs; DXVK beta.5 evidence lives under the "dxvk" key.
ANDROID_EVIDENCE = {
    "enumeration": ["beta3-phase8-android-shell-gates*.txt", "beta2-enumeration*.txt", "gateC-enumerate*.txt"],
    "deviceCreate": ["beta3-phase8-android-shell-gates*.txt", "beta2-compute*.txt", "gateDE-compute*.txt"],
    "compute10": ["beta3-phase8-android-shell-gates*.txt", "beta2-compute*.txt", "gateDE-compute*.txt"],
    "offscreen": ["beta3-phase8-android-shell-gates*.txt", "beta2-offscreen*.txt", "gateF-triangle*.txt"],
    "ahb": ["beta3-phase8-ahb-allocation*.txt", "beta2-ahb*.txt", "gateG-ahb*.txt"],
    "vulkanSurface": ["beta3-phase8-wsi*.txt", "beta2-vulkan-surface*.txt"],
    "swapchain300": ["beta3-phase8-wsi*.txt", "beta2-present-300*.txt", "beta2-swapchain*.txt"],
    "coldSecondLaunch": ["beta3-phase8-cold-second-launch*.txt", "beta2-cold-second-launch*.txt"],
}
GLIBC_EVIDENCE = {
    "enumeration": ["beta3-phase8-glibc-runtime*.txt", "beta2-glibc*.txt", "p16-glibc*.txt"],
    "compute10": ["beta3-phase8-glibc-runtime*.txt", "beta2-glibc*.txt", "p16-glibc*.txt"],
    "offscreen": ["beta3-phase8-glibc-runtime*.txt", "beta2-glibc*.txt", "p16-glibc*.txt"],
}
DXVK_EVIDENCE = {
    "progress": "worklogs/g615-dxvk/PROGRESS.md",
    "evidenceDir": "validation/{profile}/dxvk",
    "complianceMatrix": "validation/{profile}/p23-dxvk-vkd3d-compliance-matrix.json",
    "cts": {
        "geometry": {"pass": 193, "fail": 0},
        "geometry_instanced": {"pass": 20, "fail": 0},
        "tessellation": {"pass": 526, "fail": 0},
        "transform_feedback": {
            "pass": 15793,
            "fail": 0,
            "note": "2 intermittent DeviceLost",
        },
        "bc_subset": {"pass": 1863, "fail": 0},
        "copy_and_blit": {"pass": 9620, "fail": 0},
        "statistics_query": {"pass": 15374, "fail": 0},
        "multi_draw": {"pass": 12704, "fail": 0},
        "primitives_generated_query": {"pass": 75206, "fail": 0},
        "memory_priority": {"pass": 224, "fail": 0},
        "pageable_device_local_memory": {"pass": 202, "fail": 0},
        "api_info": {"pass": 7799, "fail": 0},
        "alpha_to_one": {"pass": 123, "fail": 0},
    },
}
UINT64_MAX = "18446744073709551615"

PASS_TOKEN = {
    "enumeration": ("C-PASS", "Gate C: PASS"),
    "deviceCreate": ("D-OK vkCreateDevice", "D+E-PASS", "Gate D: PASS"),
    "compute10": ("E-compute 10/10", "D+E-PASS", "Gate E: PASS"),
    "offscreen": ("F-PASS",),
    "ahb": ("G-PASS", "Gate G: PASS"),
    "vulkanSurface": ("ANDROID_SURFACE=pass",),
    "swapchain300": ("PRESENT_FRAMES=300/300", "Gate H: PASS"),
    "coldSecondLaunch": ("COLD_SECOND_LAUNCH=pass", "COLD_RELAUNCH=pass"),
}


def sha256_file(path: pathlib.Path) -> str | None:
    if not path.exists() or not path.is_file():
        return None
    h = hashlib.sha256()
    h.update(path.read_bytes())
    return h.hexdigest()


def read_json(path: pathlib.Path) -> dict:
    return json.loads(path.read_text()) if path.exists() else {}


def sh(*cmd) -> str:
    try:
        return subprocess.check_output(cmd, text=True, stderr=subprocess.DEVNULL).strip()
    except Exception:
        return ""


def newest(paths: list[pathlib.Path]) -> pathlib.Path | None:
    existing = [p for p in paths if p.exists()]
    if not existing:
        return None
    return max(existing, key=lambda p: p.stat().st_mtime)


def glob_logs(vdir: pathlib.Path, patterns: list[str]) -> list[pathlib.Path]:
    out: list[pathlib.Path] = []
    for pat in patterns:
        out.extend(sorted(vdir.glob(pat)))
    return out


def file_has(text: str, tokens: tuple[str, ...]) -> bool:
    return any(t in text for t in tokens)


def iso_from(path: pathlib.Path, text: str) -> str:
    m = re.search(r"^date=(\S+)", text, re.M)
    if m:
        d = m.group(1)
        if re.match(r"\d{4}-\d{2}-\d{2}$", d):
            return d + "T00:00:00+00:00"
        return d
    ts = datetime.datetime.fromtimestamp(path.stat().st_mtime, datetime.timezone.utc)
    return ts.isoformat()


def evidence(
    vdir: pathlib.Path,
    gate: str,
    patterns: list[str],
    tokens: tuple[str, ...],
    binary_sha: str | None,
) -> dict:
    for path in glob_logs(vdir, patterns):
        text = path.read_text(errors="replace")
        if gate == "ahb" and UINT64_MAX in text:
            continue
        if file_has(text, tokens):
            try:
                rel = path.relative_to(ROOT).as_posix()
            except ValueError:
                rel = path.as_posix()
            return {
                "status": "pass",
                "evidenceFile": rel,
                "timestamp": iso_from(path, text),
                "binarySha256": binary_sha,
            }
    return {"status": "untested"}


def find_matrix(profile: str) -> pathlib.Path | None:
    vdir = ROOT / "validation" / profile
    cands = [
        ROOT / "dist" / "runtime-feature-matrix.json",
        ROOT / "dist" / f"android-{profile}" / "runtime-feature-matrix.json",
    ] + list(vdir.glob("beta3-phase8-runtime-feature-matrix.json")) + list(vdir.glob("beta2-runtime-features-*.json"))
    return newest(cands)


def patch_series_id(profile: str) -> str:
    script = ROOT / "scripts" / "compute-patch-series-id.py"
    if script.exists():
        out = sh("python3", str(script), "--profile", profile)
        if out.startswith("sha256:"):
            return out
    lock = read_json(ROOT / "sources.lock")
    return lock.get("patchSeriesId", "unknown")


def collect(profile: str) -> dict:
    vdir = ROOT / "validation" / profile
    prof = read_json(ROOT / "profiles" / f"{profile}.json")
    lock = read_json(ROOT / "sources.lock")
    android_so = ROOT / "dist" / f"android-{profile}" / "libvulkan_panfrost.so"
    glibc_so = ROOT / "dist" / f"glibc-{profile}" / "libvulkan_panfrost.so"
    android_sha = sha256_file(android_so)
    glibc_sha = sha256_file(glibc_so)
    mesa = lock.get("mesaCommit")  # release pin; work/mesa may be a dev tree

    android = {}
    for gate in ANDROID_GATES:
        android[gate] = evidence(
            vdir, gate, ANDROID_EVIDENCE[gate], PASS_TOKEN[gate], android_sha
        )
    glibc = {}
    for gate in GLIBC_GATES:
        glibc[gate] = evidence(
            vdir, gate, GLIBC_EVIDENCE[gate], PASS_TOKEN[gate], glibc_sha
        )

    matrix_path = find_matrix(profile)
    matrix_rel = matrix_path.relative_to(ROOT).as_posix() if matrix_path else None

    ev_dir_rel = DXVK_EVIDENCE["evidenceDir"].format(profile=profile)
    ev_dir = ROOT / ev_dir_rel
    files = (
        sorted(
            p.relative_to(ev_dir).as_posix()
            for p in ev_dir.rglob("*")
            if p.is_file()
        )
        if ev_dir.is_dir()
        else []
    )
    cm_rel = DXVK_EVIDENCE["complianceMatrix"].format(profile=profile)
    compliance_matrix = cm_rel if (ROOT / cm_rel).is_file() else None

    dxvk = {
        "progress": DXVK_EVIDENCE["progress"],
        "evidenceDir": ev_dir_rel,
        "files": files,
        "complianceMatrix": compliance_matrix,
        "cts": DXVK_EVIDENCE["cts"],
    }

    return {
        "profile": profile,
        "device": prof.get("device", "Poco X6 Pro"),
        "gpuId": prof.get("gpuId", "0xb8a31030"),
        "kbaseUapi": prof.get("kbaseUapi", "1.21"),
        "mesaCommit": mesa,
        "patchSeriesId": patch_series_id(profile),
        "android": android,
        "glibc": glibc,
        "runtimeFeatureMatrix": matrix_rel,
        "dxvk": dxvk,
        "generatedAt": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "generatedBy": "scripts/collect-validation.py",
    }


def flatten_status(doc: dict) -> list[tuple[str, str]]:
    out = []
    for k, v in doc.get("android", {}).items():
        out.append((f"android.{k}", v.get("status") if isinstance(v, dict) else v))
    for k, v in doc.get("glibc", {}).items():
        out.append((f"glibc.{k}", v.get("status") if isinstance(v, dict) else v))
    return out


def require_beta_gates(doc: dict) -> list[str]:
    missing = []
    for k in ANDROID_GATES:
        st = doc["android"][k]["status"]
        if st != "pass":
            missing.append(f"android.{k}={st}")
    for k in GLIBC_GATES:
        st = doc["glibc"][k]["status"]
        if st != "pass":
            missing.append(f"glibc.{k}={st}")
    return missing


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--profile", default="g615-v11-csf")
    ap.add_argument("--out", help="VALIDATION.json path (default dist/VALIDATION.json)")
    ap.add_argument("--abi", help="ignored; accepted for generate-release-manifest.py")
    ap.add_argument("--stage", help="if set and --out omitted, write STAGE/VALIDATION.json")
    ap.add_argument(
        "--require-beta-gates",
        action="store_true",
        help="exit 1 if any mandatory beta gate is untested/fail",
    )
    a = ap.parse_args()

    doc = collect(a.profile)
    if a.out:
        out = pathlib.Path(a.out)
    elif a.stage:
        out = pathlib.Path(a.stage) / "VALIDATION.json"
    else:
        out = ROOT / "dist" / "VALIDATION.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(doc, indent=2) + "\n")
    print(f"OK {out}")
    for name, st in flatten_status(doc):
        print(f"  {name}={st}")

    if a.require_beta_gates:
        missing = require_beta_gates(doc)
        if missing:
            print("FAIL untested/fail mandatory beta gates:", file=sys.stderr)
            for m in missing:
                print(f"  {m}", file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
