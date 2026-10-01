#!/bin/sh
set -eu

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
REPOSITORY="https://github.com/KhronosGroup/VK-GL-CTS.git"
COMMIT="f6a29701220f34dd1407513bfe80d74ca7b392ce"
VULKAN_DOCS_COMMIT="4abe0260bbd8e59930786eed645481808a3fe6f1"
SERIAL="${ANDROID_SERIAL:-Y5WWBMJVOZSK4HU8}"
CHROOT="${CTS_CHROOT:-/data/local/tmp/chrootAlpine}"
OUT="${CTS_SRC:-/root/panvk-vk-gl-cts}"

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

adb -s "$SERIAL" shell su -c "test -x '$CHROOT/usr/bin/git' -a -x '$CHROOT/usr/bin/python3'" || {
    echo "device chroot lacks git or python3: $CHROOT" >&2
    exit 1
}

HELPER=/tmp/panvk-fetch-vk-gl-cts.sh
adb -s "$SERIAL" shell su -c "cat > '$CHROOT$HELPER'" <<'EOF'
#!/bin/sh
set -eu
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export PATH
OUT=$1
REPOSITORY=$2
COMMIT=$3
VULKAN_DOCS_COMMIT=$4
if [ ! -d "$OUT/.git" ]; then
    git clone --filter=blob:none --no-checkout "$REPOSITORY" "$OUT"
fi
git -C "$OUT" fetch --depth=1 origin "$COMMIT"
git -C "$OUT" checkout --detach "$COMMIT"
python3 "$OUT/external/fetch_sources.py"
test "$(git -C "$OUT" rev-parse HEAD)" = "$COMMIT"
test "$(git -C "$OUT/external/vulkan-docs/src" rev-parse HEAD)" = "$VULKAN_DOCS_COMMIT"
EOF
adb -s "$SERIAL" shell su -c "chmod 755 '$CHROOT$HELPER'"
adb -s "$SERIAL" shell su -c "chroot '$CHROOT' '$HELPER' '$OUT' '$REPOSITORY' '$COMMIT' '$VULKAN_DOCS_COMMIT'"
adb -s "$SERIAL" shell su -c "rm -f '$CHROOT$HELPER'"
printf 'device=%s\nVK-GL-CTS=%s\nVulkan-Headers-source=%s\n' "$SERIAL" "$COMMIT" "$VULKAN_DOCS_COMMIT"
