#!/bin/sh
set -eu
export ADB_SERIAL=${ADB_SERIAL:-192.168.1.34:40501}
export ANDROID_CC=${ANDROID_CC:-/opt/android-sdk/ndk/30.0.14904198/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android35-clang}
export X11_LIBDIR=${X11_LIBDIR:-/var/tmp/panvk/x11-sysroot/lib}
export MINGW_CC=${MINGW_CC:-/var/tmp/panvk/launcher-tests/llvm-mingw-20260922-ucrt-ubuntu-22.04-x86_64/bin/arm64ec-w64-mingw32-clang}
export DXSMOKE_ARCH=arm64ec DXSMOKE_API=d3d11
for mode in default flush event readback default-repeat; do
    export PROOF_OUT="$PWD/apps/panvk-launcher/tests/results/final-discrimination/$mode"
    export DXSMOKE_STAGING=0
    unset DXSMOKE_SYNC
    case $mode in
        flush|event) export DXSMOKE_SYNC=$mode;;
        readback) export DXSMOKE_STAGING=1;;
    esac
    python3 apps/panvk-launcher/tests/results/proof-synchronized/capture.py
done
python3 apps/panvk-launcher/tests/results/final-discrimination/check.py
