#!/bin/sh
# Run from the repository root. Test tools only; no driver build.
set -eu
export ADB_SERIAL=${ADB_SERIAL:-192.168.1.34:40501}
export ANDROID_CC=${ANDROID_CC:-/opt/android-sdk/ndk/30.0.14904198/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android35-clang}
export X11_LIBDIR=${X11_LIBDIR:-/var/tmp/panvk/x11-sysroot/lib}
export MINGW_CC=${MINGW_CC:-/var/tmp/panvk/launcher-tests/llvm-mingw-20260922-ucrt-ubuntu-22.04-x86_64/bin/arm64ec-w64-mingw32-clang}
export DXSMOKE_ARCH=arm64ec
for DXSMOKE_API in d3d8 d3d9 d3d10 d3d11; do
    export DXSMOKE_API
    export PROOF_OUT="$(pwd)/apps/panvk-launcher/tests/results/arm64ec-matrix/$DXSMOKE_API"
    python3 apps/panvk-launcher/tests/results/proof-synchronized/capture.py
done
python3 apps/panvk-launcher/tests/results/arm64ec-matrix/check.py
