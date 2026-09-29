# DX5 overlay rebuild

Status: `DONE`
UTC: `2026-09-21T15:24:14Z`
ADB: `192.168.1.34:32913` (network)
On-device serial: `Y5WWBMJVOZSK4HU8`
Device: duchamp / 2311DRK48I / arm64-v8a / Mali-G615 6 cores `0xB8A3` / `/dev/mali0`

## Evidence

Identity `getprop` on device matched CONTRACT serial. Checksum-push of
`/tmp/opencode/dx5-validate.tar.gz` matched host
`dfdd306ed814d925a159969f1d5f627bf55a66d1c5dcac46b7bb413475ebb9ee`.
Script in tar `8a5056b6f2bdeda93e447a6c44d28ebb6a98a8868806155a1813dd4391d7b7cb`.

Chroot overlay of tracked 018 + `ninja -j2` only. CreateDevice and matrix
**not run**.

```text
OVERLAY_ARENA=#define PANVK_GPU_PRERAST_ARENA_SIZE (256ull * 1024)
OVERLAY_GATE=PANVK_DEBUG(GPU_PRERAST)
EXPOSURE_SOURCE=false
NINJA PASS
ICD_PATH=/tmp/build-glibc/src/panfrost/vulkan/libvulkan_panfrost.so
ICD_SIZE=20053320
ICD_SHA256=f38bee643aec947fe9b97288e0701182c70324149ca1a5cc29a8a6de7f1c40ca
ICD_STRINGS=gpu_prerast
STATUS=OVERLAY_NINJA_DONE
```

ICD ELF aarch64, BuildID `f2fea46665b9d9001cb281b83a5a638126b81bfc`.
Prior stale ICD `61ab189087f9d34bfde2969c2e5d707f5725f0ad3a9e687257c532d7610e973a`
replaced. Public feature bits remain false. No vkd3d, no universal Mali,
no release promotion.

## Next bounded microtask

`DX5-createdevice`: same ADB `192.168.1.34:32913` / serial
`Y5WWBMJVOZSK4HU8`. Identity once, then chroot CreateDevice probe on
default IDVS and `PANVK_DEBUG=gpu_prerast` against ICD
`f38bee643aec947fe9b97288e0701182c70324149ca1a5cc29a8a6de7f1c40ca`.
Stop before matrix. No overlay rebuild, no DX6.
