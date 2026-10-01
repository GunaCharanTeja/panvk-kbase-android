#!/bin/sh
set -eu

SERIAL="${ANDROID_SERIAL:-Y5WWBMJVOZSK4HU8}"
CHROOT="${CTS_CHROOT:-/data/local/tmp/chrootAlpine}"
SRC="${CTS_SRC:-/root/panvk-vk-gl-cts}"
BUILD="${CTS_BUILD:-/root/panvk-vk-gl-cts-build}"
JOBS="${CTS_JOBS:-2}"
COMMIT="f6a29701220f34dd1407513bfe80d74ca7b392ce"

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

adb -s "$SERIAL" shell su -c "test -x '$CHROOT/usr/bin/cmake' -a -x '$CHROOT/usr/bin/ninja' -a -x '$CHROOT/usr/bin/clang++'" || {
    echo "device chroot lacks cmake, ninja, or clang++: $CHROOT" >&2
    exit 1
}

HELPER=/tmp/panvk-build-vk-cts.sh
adb -s "$SERIAL" shell su -c "cat > '$CHROOT$HELPER'" <<'EOF'
#!/bin/sh
set -eu
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export PATH
SRC=$1
BUILD=$2
COMMIT=$3
JOBS=$4
test "$(uname -m)" = aarch64
test "$(git -C "$SRC" rev-parse HEAD)" = "$COMMIT" || {
    echo "VK-GL-CTS checkout is not pinned commit $COMMIT" >&2
    exit 1
}
cmake -S "$SRC" -B "$BUILD" -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DDEQP_TARGET=vulkan_headless
ninja -j "$JOBS" -C "$BUILD" deqp-vk
BIN="$BUILD/external/vulkancts/modules/vulkan/deqp-vk"
test -x "$BIN"
file "$BIN"
sha256sum "$BIN"
EOF
adb -s "$SERIAL" shell su -c "chmod 755 '$CHROOT$HELPER'"
adb -s "$SERIAL" shell su -c "chroot '$CHROOT' '$HELPER' '$SRC' '$BUILD' '$COMMIT' '$JOBS'"
adb -s "$SERIAL" shell su -c "rm -f '$CHROOT$HELPER'"
