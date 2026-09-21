# DX5 runtime session

Status: `BLOCKED_DEVICE_LINK`

Serial `Y5WWBMJVOZSK4HU8`. Sequential ADB only. Identity once. No server
ops, USB reset, reboot, polling, or `PANVK_DEBUG=kbase_diag|trace`.

## ADB

| # | Category | Result |
|---|---|---|
| 1 | identity | PASS: serial match, `duchamp`, `2311DRK48I`, `arm64-v8a`, `/dev/mali0` |
| 2 | inspect | chroot wrapper present; `/tmp/mesa`, `/tmp/build-glibc`, ninja yes; ICD 19919456 bytes (pre-DX5). Device tree already had `NO_typeof` / `NO_nir_metadata_preserve`; still compiled `panvk_gpu_prerast_passthrough_nir` |
| 3 | push | `dx5-compile-fix.tar.gz` 71751 bytes, sha256 `abdc549a0931139fd4c198179656a5ed5a53d2ab6380b1cf1e302ce8150787a2` |
| 4 | inspect | overlay hash matched on Android `/data/local/tmp`; chroot tar failed (`No such file`) because the archive is outside Alpine `/tmp` |
| 5 | push | copy into chroot `/tmp` + extract compile-fix overlay; `NO_typeof`, `NO_nir_metadata_preserve`, forward `panvk_cmd_draw`, `nir_progress`, `PAN_ARCH<10` `last_variant = PANVK_VS_VARIANT_HW` |
| 6 | build | ninja `-j2` ICD; v6/v7 compiled; link FAILED `undefined reference to pan_nir_lower_vs_inputs_poly` and `poly_nir_load_raw_vertex_id` (v10–v14) |
| 7 | push | `dx5-link-fix.tar.gz` 81298 bytes, sha256 `f8db8e0149cacc883bb7a9b4a49724b33af8123f92583b2f7b0851fd6f26e806` |
| 8 | build | remaining 018 compiler/poly/meson overlay extracted; ninja `-j2` still FAILED the same two undefined refs |

Total: `8`. Transport errors: `0`. Disconnects: `0`. Kernel USB symptoms: `0`.

## Candidate hashes

Reuse ICD before overlay (DX3-era, not DX5):

```text
path: /tmp/build-glibc/src/panfrost/vulkan/libvulkan_panfrost.so
size: 19919456
```

No DX5 ICD hash. Ninja never linked `libvulkan_panfrost.so` after the
compile-fixed 018 overlay.

Patch series:

```text
sha256:0ba00aa0301ecd9ee2f41ea59916afd6612146550baec68728a917098827664a
```

018 file:

```text
sha256:ddaef4efb7c701ade5feb2abdc419dd4f2ce076efe22ea1105a0ee53f25bef3e
```

## Matrix

All cases `NOT_RUN` (ICD did not link):

direct, indexed, instanced, base vertex, first instance, zero counts,
repeated indices, primitive restart, GPU-written indirect, replay,
simultaneous, IDVS before/after.

## No-readback / order

Not proven at runtime. Source path still has CSF compute seqno wait before
IDVS and GPU SYNC32 arena acquire/release, with no host map between stages.

## Remaining blocker

The four previous compile defects are gone in tracked 018 and on the
overlaid device tree:

1. no `typeof(...)` compound literal
2. `panvk_cmd_draw` forward-declared before `gpu_prerast_draw`
3. `nir_progress(true, impl, nir_metadata_none)` instead of `nir_metadata_preserve`
4. `PAN_ARCH<10` keeps `PANVK_VS_VARIANT_HW` last; passthrough/helpers are
   `#if PAN_ARCH >= 10`

Device ninja `-j2` now fails at ICD **link**:

```text
undefined reference to `pan_nir_lower_vs_inputs_poly'
undefined reference to `poly_nir_load_raw_vertex_id'
```

for `libpanvk_v10`–`v14`. Overlay tars used `--mtime='UTC 1970-01-01'`,
so ninja did not rebuild `pan_nir_lower_vs_inputs.c` / `poly_nir_lower_vs.c`
into those archives. Follow-up must touch those objects (or meson reconfigure)
then rerun the matrix under `PANVK_DEBUG=gpu_prerast` only. ADB budget is
exhausted this session.

USB: connected, no symptom. DX6 not started.
