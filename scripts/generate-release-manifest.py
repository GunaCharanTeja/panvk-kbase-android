#!/usr/bin/env python3
"""generate-release-manifest.py — MANIFEST.json + SOURCE.json.

Usage: generate-release-manifest.py --profile g615-v11-csf --abi android-aarch64-bionic --stage <dir>
VALIDATION.json comes from scripts/collect-validation.py (real logs). No skeleton.
"""
import argparse, json, hashlib, pathlib, subprocess, datetime, sys

ap = argparse.ArgumentParser()
ap.add_argument('--profile', required=True)
ap.add_argument('--abi', required=True)
ap.add_argument('--stage', required=True)
ap.add_argument('--published', action='store_true')
a = ap.parse_args()
stage = pathlib.Path(a.stage)
root = pathlib.Path(__file__).resolve().parent.parent

def sh(*cmd):
    try:
        return subprocess.check_output(cmd, text=True).strip()
    except Exception:
        return 'unknown'

# Releases are built from the sources.lock pin + patch series; work/mesa may be a dev tree.
lock = json.loads((root / 'sources.lock').read_text())
mesa_sha = lock.get('mesaCommit', '')
patch_series_id = sh('python3', str(root / 'scripts' / 'compute-patch-series-id.py'), '--profile', a.profile)
if not patch_series_id or patch_series_id.startswith('unknown'):
    patch_series_id = lock.get('patchSeriesId', 'unknown')
so = stage / 'libvulkan_panfrost.so'
sha = hashlib.sha256(so.read_bytes()).hexdigest() if so.exists() else 'missing'

matrix_name = None
if (stage / 'runtime-feature-matrix.json').exists():
    matrix_name = 'runtime-feature-matrix.json'

manifest = {
    'profile': a.profile, 'abi': a.abi,
    'mesaCommit': mesa_sha, 'mesaVersion': lock.get('mesaVersion'),
    'patchSeriesId': patch_series_id,
    'buildHost': sh('uname', '-a'), 'date': datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'ndk': sh('ls', '/opt/android-sdk/ndk'),
    'binarySha256': sha,
}
if matrix_name:
    manifest['runtimeFeatureMatrix'] = matrix_name
(stage / 'MANIFEST.json').write_text(json.dumps(manifest, indent=2) + '\n')

source = {
    'mesaRepo': lock.get('mesaRepo'),
    'mesaCommit': mesa_sha,
    'sourceCommit': mesa_sha,
    'patchSeriesId': patch_series_id,
    'referenceRepos': lock.get('referenceRepos'),
    'releaseStatus': {'prerelease': True, 'published': a.published},
    'bcCompatibilityIncluded': False,
}
if matrix_name:
    source['runtimeFeatureMatrix'] = matrix_name
(stage / 'SOURCE.json').write_text(json.dumps(source, indent=2) + '\n')

collector = root / 'scripts' / 'collect-validation.py'
validation_path = stage / 'VALIDATION.json'
collected = False
if collector.is_file():
    cmd = [
        sys.executable, str(collector),
        '--profile', a.profile,
        '--abi', a.abi,
        '--stage', str(stage),
        '--out', str(validation_path),
        '--require-beta-gates',
    ]
    try:
        subprocess.check_call(cmd)
        collected = validation_path.is_file()
    except Exception as e:
        print(f"collect-validation.py failed: {e}", file=sys.stderr)

if not collected:
    print(
        "FAIL: VALIDATION.json must come from scripts/collect-validation.py; "
        "refusing all-untested skeleton",
        file=sys.stderr,
    )
    sys.exit(1)

print(f"OK stage={stage} sha={sha[:16]} collected={collected}")
