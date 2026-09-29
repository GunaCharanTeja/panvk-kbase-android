# DX5 runtime session

Status: `MATRIX_FAIL`

Serial `Y5WWBMJVOZSK4HU8` via network ADB `192.168.1.34:32913`.
Identity once. No overlay rebuild this chunk. CreateDevice IDVS +
`PANVK_DEBUG=gpu_prerast` both PASS r=0. Matrix SIGSEGV 139 both paths.

## ADB (this session)

| # | Category | Result |
|---|---|---|
| 1 | identity | PASS: `getprop ro.serialno=Y5WWBMJVOZSK4HU8`, `duchamp`, `2311DRK48I`, `arm64-v8a`, `/dev/mali0`, Mali-G615 6 cores `0xB8A3` |
| 2 | checksum-push | `/tmp/opencode/dx5-validate.tar.gz` sha256 `dfdd306ed814d925a159969f1d5f627bf55a66d1c5dcac46b7bb413475ebb9ee` into chroot `/tmp/dx5-validate.tar.gz` (match) |
| 3 | overlay+ninja | chroot overlay 018 + `ninja -j2`; CreateDevice/matrix truncated |

Transport errors: `0`. Disconnects: `0`.

## Device rebuild

```text
OVERLAY_ARENA=#define PANVK_GPU_PRERAST_ARENA_SIZE (256ull * 1024)
OVERLAY_GATE=PANVK_DEBUG(GPU_PRERAST)
EXPOSURE_SOURCE=false
NINJA PASS
path: /tmp/build-glibc/src/panfrost/vulkan/libvulkan_panfrost.so
size: 20053320
sha256: f38bee643aec947fe9b97288e0701182c70324149ca1a5cc29a8a6de7f1c40ca
ICD_STRINGS=gpu_prerast
ELF: ARM aarch64, BuildID[sha1]=f2fea46665b9d9001cb281b83a5a638126b81bfc
```

Replaced stale ICD `61ab189087f9d34bfde2969c2e5d707f5725f0ad3a9e687257c532d7610e973a`
(64 MiB arena SIGSEGV). Tracked 018 256 KiB / `PANVK_DEBUG(GPU_PRERAST)`
gate is now in the linked ICD.

Device script: `scripts/dxvk/dx5-device-validate.sh`
sha256 `8a5056b6f2bdeda93e447a6c44d28ebb6a98a8868806155a1813dd4391d7b7cb`

Staged overlay tarball:
`/tmp/opencode/dx5-validate.tar.gz`
sha256 `dfdd306ed814d925a159969f1d5f627bf55a66d1c5dcac46b7bb413475ebb9ee`

Patch series:

```text
sha256:16b3854736afc1c3b339d1c6aadb4d9d19236f71db497ca9ec39ea32d9547152
```

018 file:

```text
sha256:35f73906605b9c6a17e1d3f84659c3c99e5661e278b3baf776d37afd9dd4330d
```

Host unit `tests/dxvk/vulkan/test_gpu_prerast_contracts.py`: PASS.
Public feature bits in reconstructed Mesa remain false
(`geometryShader`, `fillModeNonSolid`, `shaderClipDistance`,
`shaderCullDistance`, `tessellationShader`).

## Matrix

`SLICE_COMPILE=PASS`. Same ICD `f38bee64…`.

| Path | RC | Note |
|---|---|---|
| MATRIX_IDVS | 139 | CreateDevice printed; SIGSEGV before CASE |
| MATRIX_PRERAST | 139 | CreateDevice printed; SIGSEGV before CASE |

Cases all `NOT_RUN` (crash, not skip):
direct, indexed, instanced, base vertex, first instance, zero counts,
repeated indices, primitive restart, GPU-written indirect, replay,
simultaneous, IDVS before/after.

CreateDevice default IDVS: `PASS r=0`
CreateDevice `PANVK_DEBUG=gpu_prerast`: `PASS r=0`

## Remaining

Next: diagnose slice SIGSEGV 139. Same ADB/serial/ICD.
No overlay rebuild unless required. No DX6.
