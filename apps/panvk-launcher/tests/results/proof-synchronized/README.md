# Synchronized d3d11 proof

Result: API PASS, visual FAIL. Both mapped 320x240 client drawables contain
76,800 black pixels, zero orange pixels. Android screenshot shows a black X11
surface; its orange battery overlay is not client output. Client-only raw PPM
analysis excludes all Android overlays. XImage masks are ff0000/ff00/ff, matching
the helper's RGB extraction.

New i686 dxsmoke built from current sources with existing portable LLVM MinGW.
Hold: 12,000 ms. Existing dxenv.sh, layer disabled, existing driver untouched.
DXVK reports v3.1.1-arm64ec. Installed i686 d3d11.dll and dxgi.dll hashes match
/var/tmp/panvk/dxtests/dxvk-arm64ec/syswow64 exactly. identity.txt records hashes
and the actual ICD. This does not independently authenticate a beta8 package
label; the tested driver is the recorded existing m4wsi binary.

Actual observation times are recorded in timestamps.json. Both captures finish
while the launch process is alive; exit status is 0. These are host observation timestamps, not
invented GPU or device frame timestamps. Capture command intervals overlap;
they do not establish identical frame acquisition instants.

The exact original helper source is bundled as xreadback.c; the environment
script is bundled as dxenv.sh, with inherited layers explicitly disabled.
environment.txt records its effective launch environment; manifest.json binds
all saved artifacts and identity records by SHA-256 (integrity, not authenticity).
Stale remote and local client PPMs are cleared before each capture.
The NDK helper is pushed mode 755 to
/data/local/tmp/xreadback. Android denies app-UID execution at that location;
the same binary was copied into the app container and executed there.
No driver build, replacement, or source change.

Reproduce from /home/abhaybyte/repos/panvk (connected device, existing X11/Wine
session required):

```sh
export ADB_SERIAL=192.168.1.34:40501
export ANDROID_CC=/opt/android-sdk/ndk/30.0.14904198/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android35-clang
export MINGW_CC=/var/tmp/panvk/launcher-tests/llvm-mingw-20260922-ucrt-ubuntu-22.04-x86_64/bin/i686-w64-mingw32-clang
export X11_LIBDIR=/var/tmp/panvk/x11-sysroot/lib
python3 apps/panvk-launcher/tests/results/proof-synchronized/capture.py
python3 apps/panvk-launcher/tests/results/proof-synchronized/verify.py
python3 apps/panvk-launcher/tests/results/proof-synchronized/test-verify.py
python3 apps/panvk-launcher/tests/results/proof-synchronized/test-hold.py
```

Run CLI commands through ctx_execute. commands.txt is the exact successful
build/push/launch/poll/capture/retrieval transcript. capture.py reproduces it;
verify.py independently checks saved evidence offline using only Python stdlib.
Dependencies: adb, Python 3, Android NDK compiler, LLVM MinGW compiler, Android
imagefs libX11 link sysroot. Compiler names default to PATH; X11_LIBDIR is
required. ADB_SERIAL is optional only with a single attached device. Device
dependencies remain the installed launcher, Proton 11.0-2-arm64ec, DXVK payload,
m4wsi ICD/driver and live Termux:X11 session at DISPLAY=:0. Bundle does not
include these external runtimes. No untracked /tmp source or env script needed.
stdout and stderr are captured separately: concurrent libc diagnostics must
not split the Present HRESULT line. test-hold.py additionally needs host cc;
it compiles the current hold_ms body, checking invalid input and clamp boundaries.
