#!/bin/sh
set -eu

SERIAL="${ANDROID_SERIAL:-Y5WWBMJVOZSK4HU8}"
CHROOT="${DXVK_CHROOT:-/data/local/tmp/chrootAlpine}"
JOBS="${DXVK_JOBS:-2}"

test "$SERIAL" = Y5WWBMJVOZSK4HU8 || {
    echo "refusing unexpected serial: $SERIAL" >&2
    exit 1
}
test "$JOBS" -le 2 || {
    echo "DXVK_JOBS must not exceed 2" >&2
    exit 1
}

adb -s "$SERIAL" shell su -c "chroot '$CHROOT' /bin/sh -s" <<EOF
set -eu
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

test "\$(uname -m)" = aarch64
test "\$(getconf LONG_BIT)" = 64

SRC=/root/panvk-dxvk-v3.1.1
BUILD=/root/panvk-dxvk-v3.1.1-build
COMMIT=b1a1c99ab52b687cf950d62c88bc2fa316b41663

apk info -e sdl2-dev >/dev/null
if test ! -d "\$SRC/.git"; then
    git clone --filter=blob:none --recurse-submodules --shallow-submodules \
        --branch v3.1.1 --single-branch \
        https://github.com/doitsujin/dxvk.git "\$SRC"
fi
test "\$(git -C "\$SRC" rev-parse HEAD)" = "\$COMMIT"
git -C "\$SRC" submodule update --init --recursive
test -z "\$(git -C "\$SRC" status --porcelain)"

if test ! -f "\$BUILD/build.ninja"; then
    meson setup "\$BUILD" "\$SRC" --buildtype release \
        -Dnative_sdl2=enabled -Dnative_sdl3=disabled -Dnative_glfw=disabled
fi
ninja -j "$JOBS" -C "\$BUILD"

git -C "\$SRC" submodule status --recursive
find "\$BUILD/src" -type f \( -name '*.so' -o -name '*.so.*' \) \
    -exec file {} \; -exec sha256sum {} \;
EOF
