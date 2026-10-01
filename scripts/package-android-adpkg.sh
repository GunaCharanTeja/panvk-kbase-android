#!/bin/sh
# package-android-adpkg.sh — Android flat ZIP + provenance
# Usage: ./scripts/package-android-adpkg.sh --profile g615-v11-csf --version v0.1.0-beta.2
# minApi=35 until an API 29 rebuild is proven (P16).
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROFILE=""; VER="v0.0.0-dev"; PUBLISHED=false
while [ $# -gt 0 ]; do case "$1" in --profile) PROFILE="$2"; shift 2;; --version) VER="$2"; shift 2;; --published) PUBLISHED=true; shift;; *) echo "unknown $1" >&2; exit 2;; esac; done
[ -n "$PROFILE" ] || { echo "--profile required" >&2; exit 2; }
SRC="$ROOT/dist/android-$PROFILE/libvulkan_panfrost.so"
[ -f "$SRC" ] || { echo "build first: $SRC missing" >&2; exit 1; }
MESA_SHA="$(python3 -c "import json;print(json.load(open('$ROOT/sources.lock'))['mesaCommit'])")"
PATCH_SERIES_ID="$(python3 "$ROOT/scripts/compute-patch-series-id.py" --profile "$PROFILE")"
OUT="$ROOT/dist/PanVK-Kbase-Android-$PROFILE-$VER-${MESA_SHA:0:8}.adpkg.zip"
STAGE="$(mktemp -d)"; trap 'rm -rf "$STAGE"' EXIT
cp "$SRC" "$STAGE/libvulkan_panfrost.so"
python3 - "$STAGE/meta.json" "$PROFILE" "$MESA_SHA" "$PATCH_SERIES_ID" "$ROOT" "$VER" "$PUBLISHED" <<'EOF'
import sys, json, pathlib
out, profile, sha, psid, root, version, published = sys.argv[1:]
prof = {}
pp = pathlib.Path(root) / "profiles" / f"{profile}.json"
if pp.is_file():
    prof = json.loads(pp.read_text())
meta = {"schemaVersion":1,"name":"PanVK Kbase G615","author":"panvk-kbase-android",
 "packageVersion":version.removeprefix("v"),"vendor":"Mesa","driverVersion":"Mesa 26.3.0-devel","minApi":35,
 "description":"Experimental PanVK Kbase driver for supported Mali devices. minApi=35 until API 29 rebuild is proven.",
 "libraryName":"libvulkan_panfrost.so","abi":"android-aarch64-bionic","backend":"panvk-kbase",
 "kernelInterface":"mali_kbase","profile":profile,
 "gpuId":prof.get("gpuId","0xb8a31030"),"panArch":prof.get("panArch",11),
 "frontend":prof.get("frontend","CSF"),"kbaseUapi":prof.get("kbaseUapi","1.21"),
 "sourceCommit":sha,"patchSeriesId":psid,"prerelease":True,"published":published == "true",
 "bcCompatibility":{"included":False,"reason":"Phase 6 direct-PanVK composition and correctness workloads blocked"}}
json.dump(meta, open(out,'w'), indent=2)
open(out,'a').write('\n')
EOF
MATRIX=""
for c in \
  "$ROOT/dist/android-$PROFILE/runtime-feature-matrix.json" \
  "$ROOT/dist/runtime-feature-matrix.json" \
  "$ROOT/validation/$PROFILE/runtime-feature-matrix.json"; do
  [ -f "$c" ] && MATRIX="$c" && break
done
[ -n "$MATRIX" ] || { echo "missing runtime-feature-matrix.json (P10/P13)" >&2; exit 1; }
cp "$MATRIX" "$STAGE/runtime-feature-matrix.json"
cp "$ROOT/dist/extension-gap.json" "$STAGE/extension-gap.json"
(cd "$STAGE" && sha256sum libvulkan_panfrost.so > SHA256SUMS.txt)
set -- --profile "$PROFILE" --abi android-aarch64-bionic --stage "$STAGE"
[ "$PUBLISHED" = false ] || set -- "$@" --published
python3 "$ROOT/scripts/generate-release-manifest.py" "$@"
(cd "$STAGE" && zip -q "$OUT" libvulkan_panfrost.so meta.json MANIFEST.json SHA256SUMS.txt SOURCE.json VALIDATION.json runtime-feature-matrix.json extension-gap.json)
"$ROOT/scripts/validate-package.sh" "$OUT"
echo "OK $OUT"
