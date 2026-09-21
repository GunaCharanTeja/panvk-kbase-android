# DX5 runtime session

Status: `BLOCKED_DEVICE_COMPILE`

Serial `Y5WWBMJVOZSK4HU8`. Sequential ADB only. Identity once. No server
ops, USB reset, reboot, polling, or `PANVK_DEBUG=kbase_diag|trace`.

## ADB

| # | Category | Result |
|---|---|---|
| 1 | identity | PASS: serial match, `duchamp`, `2311DRK48I`, `arm64-v8a`, `/dev/mali0` |
| 2 | inspect | chroot wrapper present; `/tmp/build-glibc`, `/tmp/mesa` |
| 3 | inspect | meson srcdir `/tmp/mesa`, builddir `/tmp/build-glibc`, existing ICD 19919456 bytes |
| 4 | push | `dx5-overlay.tar.gz` 159150 bytes, sha256 `958399d00520835f5f9058d7751815f05adcc70e420c5d37998769fea4b97140` |
| 5 | build | overlay extract + ninja `-j2`; FAILED `typeof` compound literal and `panvk_cmd_draw` used before definition |
| 6 | push | `dx5-cmd-draw-fix.tar.gz` 40898 bytes, sha256 `d4c1e72c87933e9403b748466f394b610fb2b33263c105572c29df09084b9d80` |
| 7 | build | cmd_draw overlay + ninja `-j2`; FAILED `nir_metadata_preserve` implicit declaration |
| 8 | build | on-device `nir_progress` replace + ninja `-j2`; FAILED `PAN_ARCH<10` compile of `panvk_gpu_prerast_passthrough_nir` / `shader->gpu_prerast` |

Total: `8`. Transport errors: `0`. Disconnects: `0`. Kernel USB symptoms: `0`.

## Candidate hashes

Reuse ICD before overlay (DX3-era, not DX5):

```text
path: /tmp/build-glibc/src/panfrost/vulkan/libvulkan_panfrost.so
size: 19919456
```

No DX5 ICD hash. Ninja never linked `libvulkan_panfrost.so` after 018 overlay.

Patch series (unchanged applying 018 from `e4651fc`):

```text
sha256:03dca5661c0517f5a658f2a62311494183aae392ba1209f8a5f355495dbba0c4
```

## Matrix

All cases `NOT_RUN` (ICD did not build):

direct, indexed, instanced, base vertex, first instance, zero counts,
repeated indices, primitive restart, GPU-written indirect, replay,
simultaneous, IDVS before/after.

## No-readback / order

Not proven at runtime. Source path still has CSF compute seqno wait before
IDVS and GPU SYNC32 arena acquire/release, with no host map between stages.

## Remaining blocker

Device ninja of overlaid 018 fails:

1. `typeof(generated.index)` compound literal rejected by aarch64 gcc 15.
2. `panvk_cmd_draw` called from `gpu_prerast_draw` before its definition.
3. `nir_metadata_preserve` is not in this Mesa; `nir_progress` is.
4. `PAN_ARCH < 10` objects compile `PANVK_VS_VARIANT_GPU_PASSTHROUGH` without
   the `#if PAN_ARCH >= 10` helpers / `gpu_prerast` member.

A follow-up applying 018 must compile v6/v7 and v10–v14, then rerun the
matrix under `PANVK_DEBUG=gpu_prerast` only.

USB: connected, no symptom. DX6 not started.
