#!/bin/sh
set -eu

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SERIAL="${ANDROID_SERIAL:-Y5WWBMJVOZSK4HU8}"
CHROOT="${CTS_CHROOT:-/data/local/tmp/chrootAlpine}"
BIN="${CTS_BINARY:-/root/panvk-vk-gl-cts-build/external/vulkancts/modules/vulkan/deqp-vk}"
REMOTE="${CTS_RUN_DIR:-/root/panvk-vk-cts-run}"

if [ "$#" -ne 2 ]; then
    echo "usage: $0 TEST_LIST OUTPUT_DIR" >&2
    exit 2
fi
LIST="$1"
OUT="$2"
test -s "$LIST" || { echo "test list missing or empty: $LIST" >&2; exit 1; }
mkdir -p "$OUT"

test "$(adb -s "$SERIAL" get-state 2>/dev/null)" = device || {
    echo "target device unavailable: $SERIAL" >&2
    exit 1
}
test "$(adb -s "$SERIAL" shell getprop ro.product.device | tr -d '\r')" = duchamp || {
    echo "refusing non-duchamp device: $SERIAL" >&2
    exit 1
}
test "$(adb -s "$SERIAL" shell getprop ro.product.cpu.abi | tr -d '\r')" = arm64-v8a || {
    echo "refusing non-arm64 device: $SERIAL" >&2
    exit 1
}
adb -s "$SERIAL" shell test -e /dev/mali0 || {
    echo "missing /dev/mali0 on $SERIAL" >&2
    exit 1
}
adb -s "$SERIAL" shell su -c "test -x '$CHROOT$BIN'" || {
    echo "device-built deqp-vk not executable: $BIN" >&2
    exit 1
}

adb -s "$SERIAL" shell su -c "mkdir -p '$CHROOT$REMOTE'"
adb -s "$SERIAL" push "$LIST" "$CHROOT$REMOTE/cases.txt" >/dev/null
HELPER=/tmp/panvk-run-vk-cts.sh
adb -s "$SERIAL" shell su -c "cat > '$CHROOT$HELPER'" <<'EOF'
#!/bin/sh
set -eu
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export PATH
REMOTE=$1
BIN=$2
cd "$(dirname "$BIN")"
exec "$BIN" --deqp-caselist-file="$REMOTE/cases.txt" --deqp-log-filename="$REMOTE/results.qpa" --deqp-log-images=disable
EOF
adb -s "$SERIAL" shell su -c "chmod 755 '$CHROOT$HELPER'"

set +e
adb -s "$SERIAL" shell su -c "chroot '$CHROOT' '$HELPER' '$REMOTE' '$BIN'" >"$OUT/console.txt" 2>&1
STATUS=$?
set -e
adb -s "$SERIAL" pull "$CHROOT$REMOTE/results.qpa" "$OUT/results.qpa" >/dev/null 2>&1 || :
adb -s "$SERIAL" shell su -c "rm -f '$CHROOT$HELPER'"

python3 - "$LIST" "$OUT" "$STATUS" "$SERIAL" <<'PY'
import hashlib, json, pathlib, re, sys

case_list, out, status, serial = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
text = "\n".join(p.read_text(errors="replace") for p in (out / "console.txt", out / "results.qpa") if p.exists())
codes = re.findall(r'StatusCode="([^"]+)"', text, re.IGNORECASE)
def count(*names):
    return sum(code.lower() in names for code in codes)
result = {
    "deviceSerial": serial,
    "device": "duchamp",
    "executionEnvironment": "device-native aarch64 Alpine chroot",
    "testList": str(case_list),
    "testListSha256": hashlib.sha256(case_list.read_bytes()).hexdigest(),
    "exitStatus": status,
    "pass": count("pass", "qualitywarning", "compatibilitywarning"),
    "fail": count("fail", "internalerror"),
    "skip": count("skip"),
    "not-supported": count("notsupported"),
    "crash": int(status != 0 and "device lost" not in text.lower()),
    "device-lost": len(re.findall(r"device[ _-]*lost", text, re.IGNORECASE)),
}
(out / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(result, indent=2))
PY

exit "$STATUS"
