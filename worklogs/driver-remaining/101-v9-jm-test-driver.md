# 101: Mali v9 (Valhall JM) Test Driver & On-Device Probe

Test driver bring-up and on-device validation for Mali v9 (Valhall Job Manager) using the ported backend from FristOneRR (`patches/jm-v9/`).

## Hardware & Environment

- **Target Device:** Lenovo TB336FU tablet (unrooted)
- **SoC:** MediaTek MT6835
- **GPU:** Mali-G57 MC2
- **GPU ID:** `0x90930010` (r0p1)
- **Kernel Interface:** `mali_kbase` Job Manager (JM) UAPI 11.0 (`/dev/mali0`)
- **Shader Cores:** `shader_present = 0x5` (bitmask 0b101 => 2 active cores, MC2)
- **Mesa Model Table:** Already identifies `PAN_PROD_ID(9, 0, 3)` as Mali-G57
- **Fork Provenance:** Ported from FristOneRR-Panvk-Source (`commit efd07ba`); upstream Mesa fork base identified as commit `84f09016` (2026-08-01)
- **Mesa Base:** Pinned Mesa `5a07217f` + `g615-v11-csf` series (commits up to 100) + `patches/jm-v9/`

## Probe Identity & Caps (PanProbe)

- **Device Name:** `Mali-G57 MC2` (correct)
- **Vendor ID / Device ID:** `0x13B5` / `0x90930010` (correct)
- **Driver Name / Info:** `panvk` / `PanVK-kbase beta.13 (Mesa 26.3.0-devel (git-bae3b79615))` (driverInfo string inherited from patch 099)
- **API Version:** `1.3.363` (overstated for v9: lacks mandatory core Vulkan 1.3/1.1 features such as geometry shaders, tessellation, shader clip distance; upstream reports ~1.1 for v6/v7/v9; conformanceVersion is `0.0.0.0`)
- **Timestamps:** `timestampPeriod = 0`, `timestampValidBits = 0` (unsupported)
- **Memory Heaps:** Heap 0 ~5.72 GiB DEVICE_LOCAL; Types: 0 (DEVICE_LOCAL), 1 (HOST_VISIBLE | HOST_COHERENT), 2 (HOST_VISIBLE | HOST_CACHED)
- **Extensions:** 170 device extensions, 16 instance extensions

## PanProbe Suite Results (1/17 Pass: vertex_stores SKIP only)

| Test Case | Result | Details |
|---|---|---|
| `gpu_prerast_slice` | FAIL | 11 cases pass (`idvs_before`, `direct`, `indexed`, `instanced`, `base_vertex`, `first_instance`, `zero_count`, `repeated_indices`, `primitive_restart`, `gpu_written_indirect`), then `vkWaitForFences` returns `VK_ERROR_DEVICE_LOST` (`r=-4`, line 588) on replay |
| `clip_cull` | FAIL | `CreateDevice` `r=-8` (`VK_ERROR_FEATURE_NOT_PRESENT`, feature not exposed on v9) |
| `multi_viewport` | FAIL | `CreateDevice` `r=-8` (`multiViewport` not exposed on v9) |
| `fill_mode` | FAIL | `CreateDevice` `r=-8` (`fillModeNonSolid` not exposed on v9) |
| `geometry` | FAIL | `CreateDevice` `r=-8` (`geometryShader` not exposed on v9) |
| `pipeline_stats` | FAIL | `CreateDevice` `r=-8` (`pipelineStatisticsQuery` not exposed on v9) |
| `tess_cond_state` | FAIL | `CreateDevice` `r=-7` (`VK_ERROR_EXTENSION_NOT_PRESENT`) |
| `tessellation` | FAIL | `tessellationShader` not exposed (JM tessellation deliberately not ported) |
| `xfb` | FAIL | `VK_EXT_transform_feedback` not exposed on v9 |
| `large_draw` | FAIL | `VK_EXT_transform_feedback` not exposed on v9 |
| `bc_decode` | FAIL | `textureCompressionBC = 0`; 16 BC format queries fail with `ifp=-11` |
| `vertex_stores` | SKIP | `vertexPipelineStoresAndAtomics` not supported (app counts as PASS) |
| `gs_viewport_depth` | FAIL | Required feature missing (`geometryShader = 0`, `multiViewport = 0`) |
| `vs_viewport_index` | FAIL | Required feature missing (`multiViewport = 0`, `shaderOutputViewportIndex = 0`) |
| `depth_bounds` | FAIL | `depthBounds` not reported |
| `vmr_secondary` | FAIL | Per-sample counts 0: `v9_cmd_draw` misses no-attachment `nr_samples` and `evaluate_per_sample` |
| `swapchain_lifecycle` | FAIL | `vkCreateSwapchainKHR` fails with `VK_ERROR_INVALID_EXTERNAL_HANDLE` (`-1000072003`, AHB import failure) |

## Vulkan CTS Results (bionic deqp-vk + shim)

Executed on Lenovo TB336FU using bionic `deqp-vk` with `cts/shim.c` forwarding `vkGetInstanceProcAddr` to `vk_icdGetInstanceProcAddr` from `/data/local/tmp/v9cts/libvulkan_panfrost.so`:

- **`dEQP-VK.api.smoke`:** 4/6 passed (66.7%).
  - Passed: `create_sampler`, `create_shader`, `triangle`, `asm_triangle_no_opname`.
  - Failed: `asm_triangle`, `unused_resolve_attachment` (image comparison failure).
- **`dEQP-VK.memory.mapping.suballocation.full`:** 102/102 passed (100.0%).
- **`dEQP-VK.synchronization.basic`:** 21 passed, 8 NotSupported (exclusive compute queue not supported).
- **`dEQP-VK.draw.renderpass.simple_draw`:** 4/4 passed (100.0%).
  - Passed: `simple_draw_triangle_list`, `simple_draw_triangle_strip`, `simple_draw_instanced_triangle_list`, `simple_draw_instanced_triangle_strip`.

## Decision & Release Status

- **Status:** **NOT RELEASED**.
- **ICD Integration:** Mali v9 is **NOT** folded into the universal ICD (`libpanvk_v10`..`libpanvk_v14`). It remains an isolated test build.

### Release Blockers

1. **Replay DEVICE_LOST:** `vkWaitForFences` device loss upon command buffer replay in `gpu_prerast` execution.
2. **Overstated API Version & Missing Core Features:** Driver advertises Vulkan 1.3.363 but lacks mandatory core 1.1/1.3 features (geometry shaders, tessellation, clip distance, multi-viewport, non-solid fill modes, pipeline statistics).
3. **Android WSI / Swapchain Failure:** `vkCreateSwapchainKHR` fails with `VK_ERROR_INVALID_EXTERNAL_HANDLE` due to Android Hardware Buffer (AHB) import issues on the JM backend.
4. **Secondary Command Buffer MSAA/VMR:** `v9_cmd_draw` does not support no-attachment rendering with explicit sample counts and `evaluate_per_sample`.
5. **Texture Compression Emulation:** BC1-BC7 formats are unsupported; GPU/compute decode emulation is not integrated on v9.
6. **Timestamps Unsupported:** `timestampPeriod` and `timestampValidBits` are 0 due to lack of kbase JM timeinfo plumbing.
7. **Tessellation Omission:** Job Manager tessellation was deliberately omitted during backend porting.

Evidence archived in `validation/v9-jm/`.
