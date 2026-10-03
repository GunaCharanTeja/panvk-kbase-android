#!/bin/sh
set -eu
export ADB_SERIAL=${ADB_SERIAL:-192.168.1.34:40501}
CC=${ANDROID_CC:-/opt/android-sdk/ndk/30.0.14904198/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android35-clang}
X11=${X11_SYSROOT:-/var/tmp/panvk/x11-sysroot}
BUILD=$(mktemp -d)
trap 'rm -rf "$BUILD"' EXIT
for stage in vert frag; do
    glslangValidator -V apps/panvk-launcher/tests/sample-clear.$stage -o "$BUILD/sample-clear.$stage.spv"
    spirv-val "$BUILD/sample-clear.$stage.spv"
done
"$CC" -O2 -Wall -Wextra -I"$X11/include" apps/panvk-launcher/tests/nclear.c \
    -L"$X11/lib" -lX11 -ldl -o "$BUILD/nclear"
for name in nclear sample-clear.vert.spv sample-clear.frag.spv; do
    adb -s "$ADB_SERIAL" push "$BUILD/$name" "/data/local/tmp/$name"
    adb -s "$ADB_SERIAL" shell run-as dev.zenithblue.panvklauncher sh -c \
        "'cp /data/local/tmp/$name files/container/$name; chmod 755 files/container/$name'"
done
# Existing final-discrimination reproduction installs dxsmoke/dxenv/xreadback.
python3 apps/panvk-launcher/tests/results/final-discrimination/sample-capture.py
python3 apps/panvk-launcher/tests/results/final-discrimination/trace.py
python3 apps/panvk-launcher/tests/results/final-discrimination/sample-check.py
