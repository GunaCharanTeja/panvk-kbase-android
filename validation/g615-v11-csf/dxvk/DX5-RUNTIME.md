# DX5 runtime session

Status: `BLOCKED_CREATEDEVICE_SIGSEGV`

Serial `Y5WWBMJVOZSK4HU8`. Sequential ADB only. Identity once. No server
ops, USB reset, reboot, polling, or `PANVK_DEBUG=kbase_diag|trace`.

## ADB

| # | Category | Result |
|---|---|---|
| 1 | identity | PASS: serial match, `duchamp`, `2311DRK48I`, `arm64-v8a`, `/dev/mali0` |
| 2 | inspect | `/tmp/mesa` + `/tmp/build-glibc` present; patched 018 sources at mtime 1970; `pan_nir_lower_vs_inputs.c.o` stale 2026-09-20; `libpoly_nir` absent; `mesa_clc`/`vtn_bindgen2` missing |
| 3 | push | `/tmp/dx5-nopoly2.tar.gz` 29989 bytes, sha256 `10b6907d94c9948de449ab8c4a1885a10e288c4987f8143706c855c4899e9e92` into chroot `/tmp` |
| 4 | build | extract + `touch` compiler/shader; ninja `-j2` ICD **LINK PASS**; `pan_nir_lower_vs_inputs_poly` in `libpanfrost_compiler.a`; harness clang PASS |
| 5 | runtime | IDVS and `PANVK_DEBUG=gpu_prerast` both SIGSEGV 139 after exposure print |
| 6 | locate | progress-print rebuild failed (broken quotes); leftover harness still 139 |
| 7 | probe | `vkCreateDevice` SIGSEGV 139 after `P create_device` for both IDVS and prerast |
| 8 | tombstone | newest tombstone is Samba `librpcsx-android.so` SIGTRAP, **not** the harness |

Total: `8`. Transport errors: `0`. Disconnects: `0`. Kernel USB symptoms: `0`.

## Candidate hashes

```text
path: /tmp/build-glibc/src/panfrost/vulkan/libvulkan_panfrost.so
size: 20053320
sha256: 61ab189087f9d34bfde2969c2e5d707f5725f0ad3a9e687257c532d7610e973a
```

Linked after current-mtime overlay of
`pan_nir_lower_vs_inputs.c` and `panvk_vX_shader.c` with poly NIR helpers
inlined (no `libpoly_nir` / `mesa_clc`). This ICD still allocated a 64 MiB
host-mapped arena in every `vkCreateDevice`. Tracked 018 now gates that
allocation on `PANVK_DEBUG=gpu_prerast` and shrinks it to 256 KiB. That
fix is **not** in the linked ICD.

Patch series:

```text
sha256:bea66eb362507ae981fe877c82f1f76a5811b7c274335d4bd08701383eefc798
```

018 file:

```text
sha256:35f73906605b9c6a17e1d3f84659c3c99e5661e278b3baf776d37afd9dd4330d
```

## Matrix

All cases `NOT_RUN` (`vkCreateDevice` SIGSEGV 139):

direct, indexed, instanced, base vertex, first instance, zero counts,
repeated indices, primitive restart, GPU-written indirect, replay,
simultaneous, IDVS before/after.

Exposure print before crash:

```text
ICD device=Mali-G615 MC6 id=0xb8a31030 api=1.4.363
EXPOSURE geometryShader=0 fillModeNonSolid=0 shaderClipDistance=0 shaderCullDistance=0
```

## No-readback / order

Not proven at runtime. Source path still has CSF compute seqno wait before
IDVS and GPU SYNC32 arena acquire/release, with no host map between stages.

## Remaining blocker

ICD **linked**. Runtime `vkCreateDevice` SIGSEGV 139 on both IDVS and
`PANVK_DEBUG=gpu_prerast`. Likely cause in the linked binary: 64 MiB
`gpu_prerast_arena` mapped at every device create. Tracked 018 now:

1. inlines poly NIR helpers into `panvk_vX_shader.c` (no `libpoly_nir`)
2. allocates the arena only under `PANVK_DEBUG=gpu_prerast`
3. shrinks arena to 256 KiB / 4096 invocations for the DX5 slice

Follow-up must overlay those 018 changes with current mtime, ninja `-j2`,
rerun CreateDevice then the matrix. ADB budget is exhausted this session.

USB: connected, no symptom. DX6 not started.
