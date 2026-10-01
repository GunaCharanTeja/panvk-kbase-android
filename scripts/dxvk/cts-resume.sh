#!/bin/sh
# Usage: cts-resume.sh [-i ICD_JSON] [-e VAR=VAL]... (-l CASELIST | -r REGEX) OUTPUT_DIR
# Outputs: OUT/{cases.txt,results.qpa,console.txt,aborted.txt,status.txt,adb.txt,summary.json,summary.txt}
# -r caches the device CTS caselist in CTS_ALL_CASES; delete it after rebuilding deqp-vk.
set -eu

SERIAL="${ANDROID_SERIAL:-Y5WWBMJVOZSK4HU8}"
CHROOT="${CTS_CHROOT:-/data/local/tmp/chrootAlpine}"
BIN="${CTS_BINARY:-/root/panvk-vk-gl-cts-build/external/vulkancts/modules/vulkan/deqp-vk}"
REMOTE="${CTS_RUN_DIR:-/root/panvk-vk-cts-run}"
ALL_CASES="${CTS_ALL_CASES:-/root/panvk-vk-cts-run/all-cases.txt}"

usage() {
    echo "usage: $0 [-i ICD_JSON] [-e VAR=VAL]... (-l CASELIST | -r REGEX) OUTPUT_DIR" >&2
    exit 2
}

ICD="/tmp/bp-icd.json"
MODE=""
LIST=""
REGEX=""
ENV_ARGS=""
ENV_ENTRIES=""

while getopts "i:e:l:r:" opt; do
    case "$opt" in
        i)
            ICD="$OPTARG"
            ;;
        e)
            case "$OPTARG" in
                *[\'\"\$\`\\]*)
                    echo "error: -e value cannot contain quotes, \$, \` or \\: $OPTARG" >&2
                    exit 2
                    ;;
            esac
            if ! printf '%s\n' "$OPTARG" | grep -qE '^[A-Za-z_][A-Za-z0-9_]*='; then
                echo "error: invalid -e assignment (must match VAR=VAL): $OPTARG" >&2
                exit 2
            fi
            ENV_ARGS="$ENV_ARGS '$OPTARG'"
            if [ -z "$ENV_ENTRIES" ]; then
                ENV_ENTRIES="$OPTARG"
            else
                ENV_ENTRIES="$ENV_ENTRIES
$OPTARG"
            fi
            ;;
        l)
            [ -n "$MODE" ] && usage
            MODE="list"
            LIST="$OPTARG"
            ;;
        r)
            [ -n "$MODE" ] && usage
            MODE="regex"
            REGEX="$OPTARG"
            ;;
        *)
            usage
            ;;
    esac
done
shift $((OPTIND - 1))

if [ "$#" -ne 1 ] || [ -z "$MODE" ]; then
    usage
fi
OUT="$1"

if [ "$MODE" = "list" ]; then
    test -s "$LIST" || { echo "test list missing or empty: $LIST" >&2; exit 1; }
fi

if [ "$MODE" = "regex" ] && [ -z "$REGEX" ]; then
    echo "error: empty regex" >&2
    exit 2
fi

REMOTE="$REMOTE/$(basename "$OUT")"
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

adb -s "$SERIAL" shell su -c "mkdir -p '$CHROOT$REMOTE'; rm -f '$CHROOT$REMOTE/status.txt'"

if [ "$MODE" = "list" ]; then
    adb -s "$SERIAL" push "$LIST" "$CHROOT$REMOTE/cases.txt" >/dev/null
else
    printf '%s\n' "$REGEX" > "$OUT/regex.txt"
    adb -s "$SERIAL" push "$OUT/regex.txt" "$CHROOT$REMOTE/regex.txt" >/dev/null
    rm -f "$OUT/regex.txt"
fi

HELPER=/tmp/panvk-cts-resume.sh
adb -s "$SERIAL" shell su -c "cat > '$CHROOT$HELPER'" <<'EOF'
#!/bin/sh
set -u
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

REMOTE=$1
BIN=$2
ICD=$3
MODE=$4
ALL_CASES=$5

export VK_ICD_FILENAMES="$ICD"
export VK_DRIVER_FILES="$ICD"
export MALI_NO_MALI=0

mkdir -p "$REMOTE"
trap 'echo "EXIT=$?" >> "$REMOTE/status.txt"' EXIT

if [ "$MODE" = "regex" ]; then
    REGEX=$(cat "$REMOTE/regex.txt")
    if [ ! -s "$ALL_CASES" ]; then
        mkdir -p "$(dirname "$ALL_CASES")"
        cd "$(dirname "$BIN")"
        rm -f dEQP-VK-cases.txt
        "$BIN" --deqp-runmode=txt-caselist
        sed -n 's/^TEST: //p' dEQP-VK-cases.txt | tr -d '\r' > "$ALL_CASES"
        rm -f dEQP-VK-cases.txt
    fi
    grep -E "$REGEX" "$ALL_CASES" > "$REMOTE/cases.txt" || true
    if [ ! -s "$REMOTE/cases.txt" ]; then
        echo "0 cases matched regex: $REGEX" >&2
        exit 1
    fi
fi

if [ ! -s "$REMOTE/cases.txt" ]; then
    echo "cases list missing or empty: $REMOTE/cases.txt" >&2
    exit 1
fi

cp "$REMOTE/cases.txt" "$REMOTE/rem"
: > "$REMOTE/results.qpa"
: > "$REMOTE/console.txt"
: > "$REMOTE/aborted.txt"
rm -f "$REMOTE/rem2" "$REMOTE/part.qpa" "$REMOTE/part.log"

echo "cases=$(wc -l < "$REMOTE/cases.txt") start=$(date +%s)" > "$REMOTE/status.txt"
cd "$(dirname "$BIN")"
i=0
while [ -s "$REMOTE/rem" ]; do
    i=$((i+1))
    rm -f "$REMOTE/part.qpa" "$REMOTE/part.log"
    : > "$REMOTE/part.qpa"
    : > "$REMOTE/part.log"
    "$BIN" --deqp-caselist-file="$REMOTE/rem" --deqp-log-filename="$REMOTE/part.qpa" \
        --deqp-log-images=disable --deqp-log-shader-sources=disable > "$REMOTE/part.log" 2>&1
    rc=$?
    cat "$REMOTE/part.qpa" >> "$REMOTE/results.qpa"
    cat "$REMOTE/part.log" >> "$REMOTE/console.txt"
    echo "run$i rc=$rc" >> "$REMOTE/status.txt"
    last=$(grep '#beginTestCaseResult' "$REMOTE/part.qpa" 2>/dev/null | tail -1 | cut -d' ' -f2)
    [ -z "$last" ] && last=$(head -1 "$REMOTE/rem")
    n=$(grep -c '#beginTestCaseResult' "$REMOTE/part.qpa" 2>/dev/null || true)
    ends=$(grep -cE '#(end|terminate)TestCaseResult' "$REMOTE/part.qpa" 2>/dev/null || true)
    { [ "$n" -gt "$ends" ] || [ "$n" -eq 0 ]; } && echo "$last" >> "$REMOTE/aborted.txt"
    ln=$(grep -nxF "$last" "$REMOTE/rem" | head -1 | cut -d: -f1)
    [ -z "$ln" ] && break
    tail -n +$((ln+1)) "$REMOTE/rem" > "$REMOTE/rem2"
    mv "$REMOTE/rem2" "$REMOTE/rem"
done
echo "end=$(date +%s)" >> "$REMOTE/status.txt"
for s in Pass Fail NotSupported QualityWarning CompatibilityWarning InternalError Crash Timeout ResourceError DeviceLost; do
    echo "$s=$(grep -c "StatusCode=\"$s\"" "$REMOTE/results.qpa" 2>/dev/null || true)" >> "$REMOTE/status.txt"
done
echo "results=$(grep -c '#beginTestCaseResult' "$REMOTE/results.qpa" 2>/dev/null || true) aborted=$(wc -l < "$REMOTE/aborted.txt") kto=$(grep -ciE 'timeout on subqueue|device lost' "$REMOTE/console.txt" 2>/dev/null || true)" >> "$REMOTE/status.txt"
echo DONE >> "$REMOTE/status.txt"
EOF
adb -s "$SERIAL" shell su -c "chmod 755 '$CHROOT$HELPER'"

# regex travels via regex.txt, not argv, to dodge adb/su quoting.
# The helper runs detached on the device (nohup) so a dropped adb link or a
# host-side timeout does not kill a multi-hour run; the host polls status.txt.
set +e
adb -s "$SERIAL" shell su -c "nohup chroot '$CHROOT' /usr/bin/env$ENV_ARGS '$HELPER' '$REMOTE' '$BIN' '$ICD' '$MODE' '$ALL_CASES' >'$CHROOT$REMOTE/helper.log' 2>&1 </dev/null &" >"$OUT/adb.txt" 2>&1
fails=0
dead=0
st=""
while :; do
    sleep 20
    if ! adb -s "$SERIAL" get-state >/dev/null 2>&1; then
        fails=$((fails+1))
        [ "$fails" -ge 90 ] && { echo "device unreachable for 30 min" >&2; break; }
        continue
    fi
    fails=0
    st=$(adb -s "$SERIAL" shell su -c "grep '^EXIT=' '$CHROOT$REMOTE/status.txt' 2>/dev/null" 2>/dev/null | tr -d '\r')
    [ -n "$st" ] && break
    # helper gone without EXIT (device reboot, kill): stop polling
    if adb -s "$SERIAL" shell su -c "pgrep -f 'panvk-cts-resum[e]'" >/dev/null 2>&1; then
        dead=0
    else
        dead=$((dead+1))
        [ "$dead" -ge 3 ] && { echo "helper died without EXIT" >&2; break; }
    fi
done
STATUS=${st#EXIT=}
[ -n "$STATUS" ] || STATUS=1
adb -s "$SERIAL" shell su -c "cat '$CHROOT$REMOTE/helper.log'" >>"$OUT/adb.txt" 2>&1
set -e

adb -s "$SERIAL" pull "$CHROOT$REMOTE/cases.txt" "$OUT/cases.txt" >/dev/null 2>&1 || :
adb -s "$SERIAL" pull "$CHROOT$REMOTE/results.qpa" "$OUT/results.qpa" >/dev/null 2>&1 || :
adb -s "$SERIAL" pull "$CHROOT$REMOTE/console.txt" "$OUT/console.txt" >/dev/null 2>&1 || :
adb -s "$SERIAL" pull "$CHROOT$REMOTE/aborted.txt" "$OUT/aborted.txt" >/dev/null 2>&1 || :
adb -s "$SERIAL" pull "$CHROOT$REMOTE/status.txt" "$OUT/status.txt" >/dev/null 2>&1 || :
adb -s "$SERIAL" shell su -c "rm -f '$CHROOT$HELPER'"

if [ ! -s "$OUT/cases.txt" ]; then
    cat "$OUT/adb.txt" >&2
    exit 1
fi

set +e
python3 - "$OUT" "$SERIAL" "$ICD" "$STATUS" "$LIST" "$ENV_ENTRIES" <<'PY'
import hashlib, json, pathlib, re, sys

out = pathlib.Path(sys.argv[1])
serial = sys.argv[2]
icd = sys.argv[3]
status = int(sys.argv[4])
case_list_arg = sys.argv[5]
env_entries = sys.argv[6]

env_list = [line for line in env_entries.splitlines() if line]

cases_file = out / "cases.txt"
cases_text = cases_file.read_text(errors="replace") if cases_file.exists() else ""
all_cases = [line.strip() for line in cases_text.splitlines() if line.strip()]
cases_count = len(all_cases)
cases_sha256 = hashlib.sha256(cases_file.read_bytes()).hexdigest() if cases_file.exists() else ""

test_list_name = case_list_arg if case_list_arg else str(cases_file)

qpa_file = out / "results.qpa"
qpa_text = qpa_file.read_text(errors="replace") if qpa_file.exists() else ""
results_count = len(re.findall(r"^\s*#beginTestCaseResult", qpa_text, re.MULTILINE))

STATUS_CODES = [
    "Pass",
    "Fail",
    "NotSupported",
    "QualityWarning",
    "CompatibilityWarning",
    "InternalError",
    "Crash",
    "Timeout",
    "ResourceError",
    "DeviceLost",
]
code_map = {sc.lower(): sc for sc in STATUS_CODES}

case_status = {}
cur_case = None
for line in qpa_text.splitlines():
    line = line.strip()
    if line.startswith("#beginTestCaseResult"):
        parts = line.split(None, 1)
        cur_case = parts[1].strip() if len(parts) > 1 else None
    elif cur_case and 'StatusCode="' in line:
        m = re.search(r'StatusCode="([^"]+)"', line, re.IGNORECASE)
        if m:
            raw = m.group(1)
            case_status[cur_case] = code_map.get(raw.lower(), raw)
    elif line.startswith("#terminateTestCaseResult"):
        # watchdog/crash: no StatusCode, reason follows the marker
        if cur_case and cur_case not in case_status:
            reason = (line.split(None, 1) + ["Crash"])[1].strip()
            case_status[cur_case] = code_map.get(reason.lower(), reason)
        cur_case = None
    elif line.startswith("#endTestCaseResult"):
        cur_case = None

status_counts = {sc: sum(1 for st in case_status.values() if st == sc) for sc in STATUS_CODES}

aborted_file = out / "aborted.txt"
aborted_text = aborted_file.read_text(errors="replace") if aborted_file.exists() else ""
aborted_names = [line.strip() for line in aborted_text.splitlines() if line.strip()]
aborted_set = set(aborted_names)

missing_cases = [c for c in all_cases if c not in case_status and c not in aborted_set]
missing_count = len(missing_cases)

console_file = out / "console.txt"
console_text = console_file.read_text(errors="replace") if console_file.exists() else ""
kto = sum(1 for line in console_text.splitlines() if re.search(r"timeout on subqueue|device lost", line, re.IGNORECASE))

status_file = out / "status.txt"
status_text = status_file.read_text(errors="replace") if status_file.exists() else ""
runs = len(re.findall(r"^run\d+\s+rc=", status_text, re.MULTILINE))

BENIGN_STATUSES = {"Pass", "NotSupported", "QualityWarning", "CompatibilityWarning"}
failing_cases = {c for c, st in case_status.items() if st not in BENIGN_STATUSES} | aborted_set
failing = sorted(failing_cases)

summary_json = {
    "deviceSerial": serial,
    "device": "duchamp",
    "executionEnvironment": "device-native aarch64 Alpine chroot",
    "testList": test_list_name,
    "testListSha256": cases_sha256,
    "icd": icd,
    "env": env_list,
    "cases": cases_count,
    "results": results_count,
    "runs": runs,
}
for sc in STATUS_CODES:
    summary_json[sc] = status_counts[sc]
summary_json.update({
    "aborted": aborted_names,
    "missing": missing_cases,
    "kto": kto,
    "failing": failing,
    "exitStatus": status,
})

(out / "summary.json").write_text(json.dumps(summary_json, indent=2) + "\n")

summary_lines = [
    f"cases={cases_count}",
    f"results={results_count}",
    f"runs={runs}",
]
for sc in STATUS_CODES:
    summary_lines.append(f"{sc}={status_counts[sc]}")
summary_lines.extend([
    f"aborted={len(aborted_names)}",
    f"missing={missing_count}",
    f"kto={kto}",
    "FAILING:",
])
summary_lines.extend(failing)

summary_txt = "\n".join(summary_lines) + "\n"
(out / "summary.txt").write_text(summary_txt)
sys.stdout.write(summary_txt)

sys.exit(0 if status == 0 and not failing and not missing_cases and kto == 0 else 1)
PY
EXIT_CODE=$?
set -e

exit "$EXIT_CODE"
