# Build for Mali-G615 (OPPO CPH2763, MediaTek MT6878, Valhall v11 CSF)

Target: ARM Mali-G615 (Valhall v11, CSF frontend, `/dev/mali0`, `mali_kbase`).

Prerequisites: `ccache ninja git meson python clang` + Android NDK (SDK 30+).

```sh
./scripts/fetch-mesa.sh
./scripts/bootstrap-host-tools.sh     # build machine = ARM64 glibc / host tools
./scripts/apply-patches.sh --profile g615-v11-csf
./scripts/build-android.sh --profile g615-v11-csf   # Bionic ICD: PanVK + Kbase + Android/X11 WSI
./scripts/build-glibc.sh --profile g615-v11-csf     # glibc ICD for Linux/Termux/PRoot
```

Incremental loop: `ninja -C build/android-bionic` + `ccache`.
All patches in `patches/csf-v11/` (including Patch 108 for VKD3D DirectX 12) are automatically applied.

