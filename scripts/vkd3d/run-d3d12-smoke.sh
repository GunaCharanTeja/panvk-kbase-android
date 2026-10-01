#!/bin/sh
# Run the vkd3d-proton d3d12 smoke subset (tests/vkd3d/d3d12-smoke.list) in the
# Alpine glibc chroot against a PanVK ICD. Build first: build-vkd3d-proton.sh.
# ICD: snapshot of $ICD_SRC (chroot path, default main build) into
# /tmp/vkd3d/icd, manifest /tmp/vkd3d/icd.json. No VKD3D_FEATURE_LEVEL /
# VKD3D_SHADER_MODEL overrides; pass extra env via VKD3D_EXTRA_ENV only for
# labeled diagnosis.
# Usage: DEVICE_LOCK=/path/device.lock scripts/vkd3d/run-d3d12-smoke.sh OUTDIR
set -eu

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SERIAL="${ANDROID_SERIAL:-Y5WWBMJVOZSK4HU8}"
CHROOT="${VKD3D_CHROOT:-/data/local/tmp/chrootAlpine}"
ICD_SRC="${ICD_SRC:-/tmp/build-glibc/src/panfrost/vulkan/libvulkan_panfrost.so}"
LIST="${VKD3D_LIST:-$ROOT/tests/vkd3d/d3d12-smoke.list}"
EXTRA="${VKD3D_EXTRA_ENV:-}"
OUT="${1:?usage: run-d3d12-smoke.sh OUTDIR}"
L="${DEVICE_LOCK:-}"
export ANDROID_SERIAL="$SERIAL"
mkdir -p "$OUT"

tests=$(grep -vE '^\s*(#|$)' "$LIST" | tr '\n' ' ')
cat > "$OUT/run.sh" <<EOF
#!/bin/sh
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
set -e
cd /tmp/vkd3d
mkdir -p icd logs && rm -f logs/*
cp "$ICD_SRC" icd/libvulkan_panfrost.so
set +e
echo '{"file_format_version":"1.0.1","ICD":{"library_path":"/tmp/vkd3d/icd/libvulkan_panfrost.so","api_version":"1.4.0"}}' > icd.json
echo "ICD_SHA256 \$(sha256sum icd/libvulkan_panfrost.so | cut -d' ' -f1)"
echo "VKD3D_COMMIT \$(cat build/COMMIT)"
[ -n "$EXTRA" ] && echo "DIAGNOSTIC_ENV $EXTRA"
for t in $tests; do
    env VK_DRIVER_FILES=/tmp/vkd3d/icd.json VKD3D_DEBUG=warn VKD3D_TEST_MATCH=\$t $EXTRA \
        timeout 120 ./build/tests/d3d12 > logs/\$t.log 2>&1
    rc=\$?
    echo "RESULT \$t rc=\$rc \$(grep -E 'tests executed' logs/\$t.log | tail -1)"
done
EOF

if [ -n "$L" ]; then
    i=0; while ! mkdir "$L" 2>/dev/null; do i=$((i+1)); [ $i -gt 120 ] && { echo LOCK_TIMEOUT; exit 1; }; sleep 60; done
    trap 'rm -f "$L/owner"; rmdir "$L"' EXIT
    trap 'exit 1' HUP INT TERM
    echo vkd3d-smoke > "$L/owner"
fi
adb push "$OUT/run.sh" /data/local/tmp/vkd3d-run.sh >/dev/null
adb shell su -c "'cp /data/local/tmp/vkd3d-run.sh $CHROOT/tmp/vkd3d/run.sh && chmod 755 $CHROOT/tmp/vkd3d/run.sh'"
adb shell su -c "'chroot $CHROOT /tmp/vkd3d/run.sh'" > "$OUT/summary.txt"
grep -q '^ICD_SHA256' "$OUT/summary.txt" || { cat "$OUT/summary.txt"; echo SETUP_FAIL; exit 1; }
cat "$OUT/summary.txt"
# logs collected before the lock is released (next run wipes logs/)
adb shell su -c "'cd $CHROOT/tmp/vkd3d && tar cf /data/local/tmp/vkd3d-logs.tar logs'"
adb pull /data/local/tmp/vkd3d-logs.tar "$OUT/logs.tar" >/dev/null
tar xf "$OUT/logs.tar" -C "$OUT"
grep '^RESULT' "$OUT/summary.txt" | awk '{n++; if ($3=="rc=0") p++} END {print "SMOKE " p+0 "/" n+0 " processes rc=0"}'
