# Mali v9 (Valhall Job Manager) status

All file:line references throughout this document are relative to the Mesa root of the beta.16 tree (Mesa 5a07217f + csf-v11 series up to 107 + jm-v9 001-003), written as `src/panfrost/...:NNN`.

## Summary
The Mali v9 (Valhall Job Manager) backend has been experimental since beta.14. PanProbe scores 1/17 across beta.14, beta.15, and beta.16 (only `vertex_stores` passes). PanPlay cannot start any D3D game because the driver reports Vulkan 1.1 and lacks DXVK-required features (`geometryShader`, `multiViewport`, `fillModeNonSolid`, `multiDrawIndirect`, `textureCompressionBC`). Tested hardware consists of a Mali-G57 MC2 tablet (Lenovo TB336FU); Mali-G68, Mali-G77, and Mali-G78 belong to the same architecture but remain untested.

## GPUs and devices tested

| Driver build | Anon ID(s) | Device | SoC | GPU | gpu_id -> Mesa model | Kernel | Android | App |
|---|---|---|---|---|---|---|---|---|
| beta.13 (no v9 support) | `5ec9f0c1`, `dae37485`, `80b7a0c9`, `c4d22728`, `70ce9223` | TB336FU | MT8755 | Mali-G57 MC2 | `0x90930010` -> G57 | 5.15 android13 | 16 | PanProbe 1.2.0/1.2.1 |
| beta.13 (no v9 support) | `079357e1`, `94f4d2bb` | TB336FU | MT8755 | Mali-G57 MC2 | `0x90930010` -> G57 | 5.15 android13 | 16 | PanPlay 1.2.0 |
| beta.14 | `fad0cb63`, `6ff8855c`, `e0944b04` | TB336FU | MT8755 | Mali-G57 MC2 | `0x90930010` -> G57 | 5.15 android13 | 16 | PanProbe 1.2.1 |
| beta.14 | `ef4f82a2`, `7b576aa7`, `d3de3763` | TB336FU | MT8755 | Mali-G57 MC2 | `0x90930010` -> G57 | 5.15 android13 | 16 | PanPlay 1.2.1 |
| beta.15 | `49abcff8` | TB336FU | MT8755 | Mali-G57 MC2 | `0x90930010` -> G57 | 5.15 android13 | 16 | PanProbe 1.2.2 |
| Excluded | `f907b333` | TB336FU | MT8755 | Mali-G57 MC2 | `0x90930010` -> G57 | 5.15 android13 | 16 | PanProbe 1.2.1 (non-release .so) |

Mali-G68, Mali-G77, and Mali-G78 are the same Valhall Job Manager architecture (v9) but have no test submissions yet.

## Kernel / kbase interface seen
- **Kernel:** Linux 5.15 android13 (GKI 5.15).
- **kbase uAPI:** JM uAPI 11.0 (from worklog; uAPI version is not logged in upload telemetry).
- **Vendor DDK:** GLES r38p1 (`GL_VERSION` from vendor driver).
- **Vendor API level:** `ro.board.first_api_level = 33` (Android 13), `ro.hardware.gralloc = common`.
- **Queues:** `queueCount = 1` (single Valhall JM queue).
- **Timestamps:** `timestampValidBits = 0` (no JM GPU timeinfo plumbing implemented).
- **EXEC_INIT warning:** `MESA: warning: kbase: KBASE_IOCTL_MEM_EXEC_INIT failed: Operation not permitted (executable BO allocation will not work)` on every run (issue 3: JIT_INIT precedes EXEC_INIT in `src/panfrost/lib/kmod/kbase_kmod.c:1391-1410`).

## Per-test results

### PanProbe test suite (17 tests)

| Test | beta.14 (`fad0cb63`, `6ff8855c`, `e0944b04`) | beta.15 (`49abcff8`) | Result + reason from FACTS.md |
|---|---|---|---|
| `gpu_prerast_slice` | FAIL | FAIL | Passes until replay: JM atom 35 (`core_req=0x16`) and atom 36 (`core_req=0x1`) fail with event `0x58` (`DATA_INVALID_FAULT`), `Wait r=-4` (DEVICE_LOST) |
| `clip_cull` | FAIL | FAIL | `CreateDevice r=-8` (`VK_ERROR_FEATURE_NOT_PRESENT`: missing `shaderClipDistance`, `shaderCullDistance`) |
| `multi_viewport` | FAIL | FAIL | `CreateDevice r=-8` (`VK_ERROR_FEATURE_NOT_PRESENT`: missing `multiViewport`) |
| `fill_mode` | FAIL | FAIL | `CreateDevice r=-8` (`VK_ERROR_FEATURE_NOT_PRESENT`: missing `fillModeNonSolid`) |
| `bc_decode` | FAIL | FAIL | All BC formats unsupported (`ifp=-11`, `textureCompressionBC=false`, blocker 1) |
| `geometry` | FAIL | FAIL | `CreateDevice r=-8` (`VK_ERROR_FEATURE_NOT_PRESENT`: missing `geometryShader`) |
| `tessellation` | FAIL | FAIL | `FAIL tessellationShader not exposed` |
| `xfb` | FAIL | FAIL | `FAIL VK_EXT_transform_feedback not exposed` |
| `pipeline_stats` | FAIL | FAIL | `CreateDevice r=-8` (`VK_ERROR_FEATURE_NOT_PRESENT`: missing `pipelineStatisticsQuery`) |
| `vertex_stores` | PASS | PASS | Passes all verification cases |
| `gs_viewport_depth` | FAIL | FAIL | `FAIL required feature missing` (`geometryShader` / viewport depth clamp features not reported) |
| `vs_viewport_index` | FAIL | FAIL | `FAIL required feature missing` (`shaderOutputViewportIndex` not reported) |
| `depth_bounds` | FAIL | FAIL | `FAIL depthBounds not reported` |
| `large_draw` | FAIL | FAIL | `FAIL VK_EXT_transform_feedback not exposed` |
| `vmr_secondary` | FAIL | FAIL | `cnt[slot 1 sample 1] = 0, want 256` (samples 1-3 never counted; only sample 0 counted; `sec_1x` passes) |
| `tess_cond_state` | FAIL | FAIL | `CreateDevice r=-7` (`VK_ERROR_EXTENSION_NOT_PRESENT`: missing `VK_EXT_conditional_rendering`) |
| `swapchain_lifecycle` | FAIL | FAIL | `FAIL vkCreateSwapchainKHR res=-1000072003` (`VK_ERROR_INVALID_EXTERNAL_HANDLE`, blocker 2) |

### PanPlay execution runs

| Driver build | Anon ID | Target executable | Exit code | Observed behavior / DXVK lines |
|---|---|---|---|---|
| beta.13 | `079357e1` | D3D8 x86 cube | 5 | Driver initialization fails (beta.13 has no v9 support) |
| beta.13 | `94f4d2bb` | D3D8 x86 cube | 137 | `Failed to enumerate physical devices, res -3` |
| beta.14 | `ef4f82a2` | D3D8 ARM64EC cube | 5 | Wine exits with code 5 before DXVK logs an adapter line; not analysed further |
| beta.14 | `7b576aa7` | D3D9 ARM64EC cube | 3 | `Skipping: Device does not support Vulkan 1.3`, `DXVK: No adapters found`, uncaught `dxvk::DxvkError` |
| beta.14 | `d3de3763` | D3D9 ARM64EC cube | 3 | `Skipping: Device does not support Vulkan 1.3`, exit 3 |

## Failures and log excerpts

### JM atom DATA_INVALID_FAULT (`gpu_prerast_slice`)
```
CASE idvs_before RGBA=255 0 0 255 PASS
...
CASE gpu_written_indirect RGBA=255 0 0 255 PASS
MESA: error: kbase: JM atom 35 failed: event=0x58 core_req=0x16
MESA: error: kbase: JM atom 36 failed: event=0x58 core_req=0x1
FAIL Wait r=-4 line=588
```
Atom 35 (vertex/tiler, `core_req=0x16`) encounters event `0x58` (`DATA_INVALID_FAULT`) upon command buffer replay. Atom 36 (fragment, `core_req=0x1`) fails due to dependency failure.

### Feature and extension gate rejections
```
ICD device=Mali-G57 MC2 geometryShader=0 fillModeNonSolid=0 multiViewport=0 shaderClipDistance=0 shaderCullDistance=0 maxViewports=1 maxClip=0 maxCull=0 maxCombined=0
FAIL CreateDevice r=-8 line=160
```
```
FAIL CreateDevice r=-7 line=160
```
Tests querying unadvertised features fail device creation with `r=-8` (`VK_ERROR_FEATURE_NOT_PRESENT`). Tests requiring unsupported extensions (`VK_EXT_conditional_rendering` in `tess_cond_state`) fail with `r=-7` (`VK_ERROR_EXTENSION_NOT_PRESENT`).

### VMR secondary sample count failure (`vmr_secondary`)
```
FEATURE noAttachmentSamples=0xd sampleRateShading=1
  cnt[slot 1 sample 1] = 0, want 256
  cnt[slot 1 sample 2] = 0, want 256
  cnt[slot 1 sample 3] = 0, want 256
FAIL case prim_1x_4x_1x bad=3
...
PASS case sec_1x bad=0
...
RESULT FAIL
```

### Swapchain creation failure (`swapchain_lifecycle`)
```
FAIL vkCreateSwapchainKHR res=-1000072003 phase=create_swapchain
```
Logcat excerpt:
```
MESA: [P0A-V19-FULLPLANE] mapper load failed
MESA: [P0A-V19-FULLPLANE] complete metadata unavailable rc=-95; refusing guessed layout
```

### DXVK adapter rejection in PanPlay D3D9 (`wine-run.log`)
```
info:  Found device: Mali-G57 MC2 ( 26.2.99)
info:    Skipping: Device does not support Vulkan 1.3
warn:  DXVK: No adapters found. Please check your device filter settings
warn:  and Vulkan drivers. A Vulkan 1.3 capable setup is required.
libc++abi: terminating due to uncaught exception of type dxvk::DxvkError
exit=3
```

## Root causes
- **API version 1.1 & DXVK requirements:** `get_api_version()` returns 1.1 on `PAN_ARCH == 9` at `src/panfrost/vulkan/panvk_vX_physical_device.c:817` (patch `jm-v9/003`). Bundled DXVK (`components-build dxvk-src src/dxvk/dxvk_device_info.cpp:828-840`) requires Vulkan 1.3 and enforces `geometryShader`, `multiViewport`, `fillModeNonSolid`, and `multiDrawIndirect` even for D3D9. Vulkan 1.2/1.3 features and properties are hidden, producing empty driverName `( 26.2.99)`.
- **Replay fault (hypothesis):** In `tests/dxvk/vulkan/gpu_prerast_slice.c:488`, command buffers are submitted a second time. JM queue restore (`src/panfrost/vulkan/jm/panvk_vX_gpu_queue.c:88`) restores 16-byte job headers and tiler descriptors, but omits the GPU-written `MALLOC_VERTEX_JOB` draw payload where `vertex_array` was packed with `packet=true` (`src/panfrost/vulkan/jm/panvk_vX_cmd_draw.c:2309`, genxml `src/panfrost/genxml/v9.xml:1567`). A stale payload on the second submit would explain `DATA_INVALID_FAULT` (event `0x58`) on the vertex/tiler atom. Not yet confirmed by a descriptor dump.
- **VMR sample count:** Attachment-less rendering leaves `fb.nr_samples = 1` (`src/panfrost/vulkan/panvk_vX_cmd_draw.c:631/732`). v9 allocates the framebuffer without updating sample counts (`src/panfrost/vulkan/jm/panvk_vX_cmd_draw.c:2454`; legacy JM sets it at `:1299`) and never sets `flags_0.evaluate_per_sample` (`src/panfrost/vulkan/jm/panvk_vX_cmd_draw.c:2306`), leaving samples 1..3 uncounted.
- **Compute pre-raster pipeline is CSF-only:** Pre-raster shader lowering is restricted to CSF (`src/panfrost/vulkan/meson.build:102`, `src/panfrost/vulkan/panvk_shader.h:22`). Without it, JM has no lowering for `geometryShader`, `multiViewport`, `fillModeNonSolid`, `shaderClipDistance`, `shaderCullDistance`, and `vertexPipelineStoresAndAtomics`.
- **Tessellation silently dropped:** The JM draw dispatch contains placeholder handling that silently discards tessellation draws (`src/panfrost/vulkan/jm/panvk_vX_cmd_draw.c:2427-2429`), and `tessellationShader` is not advertised.
- **BC texture compression disabled (blocker 1):** `panvk_bc_emul_enabled()` at `src/panfrost/vulkan/panvk_physical_device.c:1815-1826` disables software BC emulation if kbase `TEXTURE_FEATURES` reports native BC1. However, `has_texture_compression_bc()` at `src/panfrost/vulkan/panvk_vX_physical_device.c:294-302` requires all 10 BC formats natively. Disabling emulation causes `get_image_plane_format_features()` (`src/panfrost/vulkan/panvk_physical_device.c:1837-1839`) to return 0 for all BC formats.
- **Android gralloc mapper mismatch (blocker 2):** MediaTek stable-C mapper in `src/util/u_gralloc/u_gralloc_fallback.c:83,95,102` fails on Android 13 (`ro.board.first_api_level = 33` < 34), returning `-ENOTSUP` (`u_gralloc_fallback.c:125-130,425-431`) and triggering `VK_ERROR_INVALID_EXTERNAL_HANDLE` (`src/vulkan/runtime/vk_android.c:150-152`).
- **EXEC_INIT EPERM (issue 3):** `src/panfrost/lib/kmod/kbase_kmod.c:1391-1396,1404-1410` calls JIT_INIT before EXEC_INIT; older JM kbase returns `-EPERM` for EXEC_INIT.
- **Indirect draws and firstInstance gaps:** JM indirect dispatch helpers exist (`src/panfrost/vulkan/jm/panvk_vX_cmd_draw.c:2575,2603`), but lack `VK_KHR_draw_indirect_count`, multi-draw gating, and per-instance attribute offsets for GPU `firstInstance` (`src/panfrost/vulkan/jm/panvk_vX_cmd_draw.c:1989`).

## What the driver lacks on this arch
- Vulkan 1.2, 1.3, and 1.4 API reporting and feature structs (pinned to 1.1).
- Compute pre-raster lowering on JM for geometry shading, multi-viewport, non-solid fill mode, and clip/cull distances.
- Tessellation shader execution (draws currently dropped).
- Transform feedback (`VK_EXT_transform_feedback`).
- Replay support without payload corruption on `MALLOC_VERTEX_JOB`.
- Multisample / VMR attachment-less sample evaluation.
- BC texture decompression fallback when only partial native BC is exposed.
- Android gralloc mapper support for vendor API < 34 / HIDL mapper4.
- Indirect count draws (`VK_KHR_draw_indirect_count`), multi-draw indirect, and GPU `firstInstance` attribute rebasing.
- Hardware timestamp plumbing (`timestampValidBits = 0`).
- Missing extensions vs G615 (170 vs 188): `VK_EXT_transform_feedback`, `VK_EXT_robustness2`, `VK_EXT_conditional_rendering`, `VK_EXT_multi_draw`, `VK_EXT_primitives_generated_query`, `VK_EXT_nested_command_buffer`, `VK_EXT_memory_priority`, `VK_EXT_pageable_device_local_memory`, `VK_EXT_sampler_filter_minmax`, `VK_KHR_draw_indirect_count`, `VK_KHR_cooperative_matrix`, `VK_KHR_copy_memory_indirect` and others.

## Fix plan

| Rank | Item | Effort | Unblocks |
|---|---|---|---|
| 1 | Replay fault: snapshot/restore `MALLOC_VERTEX_JOB` payload | 4–8 h diagnosis + 1–2 days | `gpu_prerast_slice` PASS |
| 2 | VMR sample count: update `fb.nr_samples` and `evaluate_per_sample` | 1–2 days | `vmr_secondary` PASS |
| 3 | BC emulation decision: fallback to emulation unless full mask is native | 4–8 h (shared) | `bc_decode`, DXVK BC feature requirement |
| 4 | Gralloc mapper: vendor-neutral discovery / HIDL mapper4 backend | 24–48 h (quick option: 4–8 h) | `swapchain_lifecycle` on Android surfaces |
| 5 | Audit and report Vulkan 1.3 core features on JM | 1–3 days | Exposes Vulkan 1.3 to DXVK |
| 6 | Port compute pre-raster lowering to JM job chains | 15–30 working days | GS, multiViewport, fillModeNonSolid, clip/cull, VPSA |
| 7 | Tessellation shader pipeline support on JM | 5–15 days | `tessellation`, `tess_cond_state` |
| 8 | Transform feedback support (`VK_EXT_transform_feedback`) | 3–7 days | `xfb`, `large_draw` |
| 9 | Draw indirect count, multiDraw, and GPU firstInstance offset | 2–5 days | Indirect draw conformance |
| 10 | Timestamp query plumbing for JM | not estimated | `timestampValidBits > 0` |
| 11 | EXEC_INIT ordering fix (call before JIT_INIT) | 6–12 h | Eliminates kbase EPERM warning |

DXVK requires Vulkan 1.3, `geometryShader`, `multiViewport`, `fillModeNonSolid`, `multiDrawIndirect`, and `textureCompressionBC` before it will accept the physical device adapter. Consequently, PanPlay games remain hard-blocked until rank 6 lands.

## Open questions / data needed from testers
- **Other v9 GPUs:** Need `gpu_id` values and kbase uAPI versions for Mali-G68, Mali-G77, and Mali-G78 devices.
- **TEXTURE_FEATURES on G57:** Need a raw dump of kbase `TEXTURE_FEATURES` from the Lenovo TB336FU tablet to confirm exact native BC support bits.
- **Replay hypothesis validation:** Need a descriptor and job memory dump before and after replay execution in `gpu_prerast_slice` to confirm `MALLOC_VERTEX_JOB` payload corruption.
- **DXVK D3D9 requirements:** Verify whether D3D9 translation on this bundled DXVK version strictly requires `geometryShader` when Vulkan 1.3 is advertised, or if feature gating can be relaxed.

## Links
- [Universal Mali status](../README.md)
- [v9 JM Test Driver Worklog](../../../worklogs/driver-remaining/101-v9-jm-test-driver.md)
- [G615 DXVK Progress Worklog](../../../worklogs/g615-dxvk/PROGRESS.md)
