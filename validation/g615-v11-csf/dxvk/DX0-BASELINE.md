# DX0 capability and profile snapshot

## Scope

DX0 only. Branch `feature/g615-dxvk-complete` starts at
`052a5d37315e621082436fc9fa2745d0264347e2`.

`origin/main` was fetched on 2026-09-20 and still resolved to that commit. The
existing commits `c7b4b9b`, `3d6da7c`, `13ed680`, and `d3a2b4f` remain intact on
the branch. This revised DX0 commit follows them; their DX1-DX4 claims are not
reclassified as DX0 completion.

This snapshot re-evaluates the existing beta.3 direct-ICD capture. It does not
claim a new device run from the repository base commit. Runtime provenance:

- Driver commit: `fc8a759e7d1b2b8de01c0e96f1fdc5e3950ba1a3`
- Mesa commit: `5a07217f034b3e50d8c7c7794f97a2df1742613b`
- Device: Poco X6 Pro / duchamp, Mali-G615 MC6, GPU ID `0xb8a31030`
- Kbase UAPI: `1.21`
- Vulkan API: `1.4.363`
- Android ICD SHA-256: `576e37de9a3b60dda9a972a6f91c3a50e09238bd31a9888e04215c7b554c803a`
- Android ABI: ELF64 AArch64, Android API 35, NDK r30-beta1 (14904198), bionic
- Android Build ID: `2ba551018ebc18b8990c7431e3c99b0523f87d15`
- glibc ICD SHA-256: `95a019b21d42f9d91bf3ee697a4e84b09485fdd8f0c298dda4494356cb983ec0`
- glibc ABI: ELF64 AArch64, `libc.so.6`, maximum referenced symbol version `GLIBC_2.38`
- glibc Build ID: `0a87f02d2a958b9a480f9a5e1303a73e65e683b5`
- glibc toolchain: Meson 1.12.0, GCC 15.2.0, GNU ld.bfd 2.45.1

These are the existing beta.3 artifacts, not builds from the current branch.

## Clean reconstruction

The pinned Mesa commit plus the current tracked profile stack reconstructs in an
isolated temporary checkout: 18 patches apply, tracked overlays copy, Mesa HEAD
remains `5a07217f034b3e50d8c7c7794f97a2df1742613b`, and Mesa has no submodules.
Current patch-series ID:

```text
sha256:8d47072143f5651aeeac9e95f64dd64b389d9b2cf7a840803d2f9daf16d3810a
```

`scripts/build-glibc.sh` now reconstructs this source itself, keys source/build
paths by Mesa and patch fingerprints, limits builds to two jobs, uses only
serial-scoped ADB, checks the transferred archive hash, parameterizes device and
chroot paths, validates the AArch64 glibc ABI, and fails on a missing ICD or
manifest. Ignored `work/mesa` is not an input or deliverable.

Machine-readable evidence: `dx0-capability-profile-snapshot.json`.

## DXVK status

| Version | COMMON | D3D9 | D3D10 10.1 | D3D11 11.0 | D3D11 11.1 |
| --- | --- | --- | --- | --- | --- |
| 3.1.1 | PASS | FAIL | FAIL | FAIL | FAIL |
| 2.7.1 | PASS | FAIL | FAIL | FAIL | FAIL |
| 1.10.3 | N/A | FAIL | FAIL | FAIL | N/A |

DXVK 3.1.1 D3D9 blockers:

```text
geometryShader
fillModeNonSolid
shaderClipDistance
shaderCullDistance
textureCompressionBC
```

D3D10 10.1 additionally needs `multiViewport`, `VK_EXT_transform_feedback`,
`transformFeedback`, and `geometryStreams`. D3D11 11.0 additionally needs
`tessellationShader`. D3D11 11.1 additionally needs
`vertexPipelineStoresAndAtomics`.

## Truthfulness boundary

- All listed missing capability bits remain false; `VK_EXT_transform_feedback`
  remains absent.
- Vulkan CTS remains `NOT_TESTED/BLOCKED`: no `deqp-vk` evidence exists.
- DXVK Native remains `NOT_TESTED`; stock DXVK smoke remains `BLOCKED`.
- No PanVK source, feature bit, profile, or requirement manifest changed in DX0.

## Baseline regression identity

No new candidate runtime exists in DX0. Existing beta.3 evidence remains:

- Android: enumeration, device creation, compute 10/10, offscreen, AHB, Vulkan
  surface, WSI 300/300, and cold second process PASS.
- glibc: enumeration, compute 10/10, and offscreen PASS.
- Current DXVK capability status remains the table above. CTS remains blocked;
  DXVK Native remains not tested.

No baseline result was copied to the current branch artifacts. One pre-work ADB
availability check returned `device`; no runtime/device command followed.
