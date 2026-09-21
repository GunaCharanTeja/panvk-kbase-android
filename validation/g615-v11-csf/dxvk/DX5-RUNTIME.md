# DX5 runtime session

Status: `BLOCKED_USB_DISCONNECT`

Serial `Y5WWBMJVOZSK4HU8`. Sequential ADB only. Identity once intended.
No server ops, USB reset, reboot, polling, or `PANVK_DEBUG=kbase_diag|trace`.
Stopped on transport disconnect. Overlay/rebuild/CreateDevice/matrix **not run**.

## ADB (this session)

| # | Category | Result |
|---|---|---|
| 1 | identity | FAIL: `adb -s Y5WWBMJVOZSK4HU8 shell` returned; host-expanded `getprop` printed empty serial, not `Y5WWBMJVOZSK4HU8` |
| — | follow-up | `adb -s Y5WWBMJVOZSK4HU8 get-state` → `error: device 'Y5WWBMJVOZSK4HU8' not found` |
| — | list | `adb devices -l` empty. `lsusb` has no Xiaomi/Android gadget (only Foxconn MediaTek Bluetooth `0489:e0cd`) |

Push, chroot script, ninja, CreateDevice, matrix, log pull: **NOT_RUN**.
Transport error: device not found. Disconnect: **yes**. Kernel USB symptom in `dmesg`: **none observed** (no further ADB).

## Host (no device compile)

Tracked 018 already contains the CreateDevice gate:

```text
PANVK_DEBUG(GPU_PRERAST) arena alloc
PANVK_GPU_PRERAST_ARENA_SIZE (256ull * 1024)
```

That overlay was **not** rebuilt on device this session. Linked ICD from the previous session remains:

```text
path: /tmp/build-glibc/src/panfrost/vulkan/libvulkan_panfrost.so
size: 20053320
sha256: 61ab189087f9d34bfde2969c2e5d707f5725f0ad3a9e687257c532d7610e973a
```

That binary still allocated a 64 MiB host-mapped arena on every
`vkCreateDevice` (SIGSEGV 139). Tracked 018 is not in it.

Device script: `scripts/dxvk/dx5-device-validate.sh`
sha256 `8a5056b6f2bdeda93e447a6c44d28ebb6a98a8868806155a1813dd4391d7b7cb`

Staged overlay tarball (host only, not pushed):
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

All cases `NOT_RUN` (USB disconnect before overlay):

direct, indexed, instanced, base vertex, first instance, zero counts,
repeated indices, primitive restart, GPU-written indirect, replay,
simultaneous, IDVS before/after.

CreateDevice default IDVS: `NOT_RUN`
CreateDevice `PANVK_DEBUG=gpu_prerast`: `NOT_RUN`

## Remaining blocker

`Y5WWBMJVOZSK4HU8` disconnected after the identity attempt.
Do not overlay/rebuild until the serial is present again.

Then run **one** `adb -s Y5WWBMJVOZSK4HU8 shell` identity that evaluates
`getprop` **on the device**, checksum-push
`scripts/dxvk/dx5-device-validate.sh` plus overlay/harness, one chroot
invocation of that script (`ninja -j2`, CreateDevice, matrix), optional
log pull. Max 4 ADB. Stop on disconnect.

USB: **disconnected**. DX6 not started.
