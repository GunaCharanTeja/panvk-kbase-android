# P23 Final DXVK/vkd3d compliance matrix

## Beta.6 status (current)

Release `g615-v11-csf-v0.1.0-beta.6`. Patches through `077`.

Matrix rows below are evaluated from the beta.3-era consumer-capabilities.json capture; features listed here are device-proven in beta.6 but await a fresh capture. Swapchain lifecycle test done (passes on device); GS viewport index honoured (076).

Exposed since the beta.3 capture:

| Feature | Patch | CTS |
|---|---|---|
| `variableMultisampleRate` | `074` | variable_rate 504/0 standalone; combined regression 6210/0 |
| `sync_fd export (kbase KCPU queue)` | `075` | 1996/0 (was 113 ResourceError) |
| `multiViewport GS viewport index honoured` | `076` | draw scissor 88/88 |
| `system-scope subqueue sync signals on kbase` | `077` | signal_order 1316/0 (0 timeouts) |
| `swapchain lifecycle test` | `test-apk` | done (passes on device) |
| `VK_EXT_memory_priority + VK_EXT_pageable_device_local_memory` | `069` | 224/0, 202/0; api.info 7799/0 |
| `alphaToOne` | `070` | 123/0 |
| `maxGeometryShaderInvocations` | `071` | geometry 193/0, instanced 20/0 |
| `VK_EXT_multi_draw` | `072` | 12704/0 |
| `VK_EXT_primitives_generated_query` | `073` | 75206/0 |
| `geometryShader` | `before-beta.5` | geometry 193/0, instanced 20/0 |
| `tessellationShader` | `before-beta.5` | 526/0 |
| `VK_EXT_transform_feedback` | `before-beta.5` | 15793/0, 2 intermittent DeviceLost |
| `textureCompressionBC` | `before-beta.5` | 1863/0, copy_and_blit 9620/0 |
| `shaderClipDistance` | `before-beta.5` | device matrix 0 fail |
| `shaderCullDistance` | `before-beta.5` | device matrix 0 fail |
| `multiViewport` | `before-beta.5` | device matrix 0 fail |
| `fillModeNonSolid` | `before-beta.5` | device matrix 0 fail |
| `pipelineStatisticsQuery` | `049-054` | statistics_query 15374/0 |

Open items:

- GS primitive drop
- VMR (vertexPipelineStoresAndAtomics / FL11_1, worktree dx-vpsa)
- JICA98 0005 (same-queue semaphore waits) review
- depthBounds
- shaderOutputViewportIndex (not exposed; GS viewport index honoured in 076)
- XFB intermittent DeviceLost
- XFB 65536-record cap per non-tess draw
- robustImageAccess2 (vkd3d-proton device-create blocker)

Deferred:

- vkd3d/D3D12
- FL12 (sparse NO-GO on kbase)

## Result

`FAIL`

Machine-evaluated matrix only. No PanVK patch. No feature-bit change.
HARD_REQUIREMENTS, PROFILE_BASELINE, DEVICE_CREATE, and FEATURE_LEVEL stay unmerged.
Stock DXVK/vkd3d smoke is BLOCKED. No invented hardware results.

```text
DXVK 1.10.3:
D3D9=FAIL
D3D10=FAIL
D3D11_FL10_1=FAIL
D3D11_FL11_0=FAIL
MAX_FEATURE_LEVEL=NOT_AVAILABLE

DXVK 2.7.1:
COMMON=PASS
D3D9_PROFILE=FAIL
D3D10_10_1_PROFILE=FAIL
D3D11_11_0_PROFILE=FAIL
D3D11_11_1_PROFILE=FAIL
MAX_FEATURE_LEVEL=NOT_AVAILABLE

DXVK 3.1.1:
COMMON=PASS
D3D9_PROFILE=FAIL
D3D10_10_1_PROFILE=FAIL
D3D11_11_0_PROFILE=FAIL
D3D11_11_1_PROFILE=FAIL
MAX_FEATURE_LEVEL=NOT_AVAILABLE

VKD3D 2.0:
HARD_REQUIREMENTS=PASS
DEVICE_CREATE=PASS

VKD3D 2.14.1:
HARD_REQUIREMENTS=PASS
PROFILE_BASELINE=FAIL
DEVICE_CREATE=FAIL
FEATURE_LEVEL=NOT_AVAILABLE

VKD3D 3.0.1:
HARD_REQUIREMENTS=PASS
PROFILE_BASELINE=FAIL
DEVICE_CREATE=FAIL
FEATURE_LEVEL=NOT_AVAILABLE
```

## Adapter

- Device: `Mali-G615 MC6`
- Target: `Poco X6 Pro / duchamp`
- VendorID: `5045`
- DeviceID: `0xb8a31030`
- Driver: `panvk` / `Mesa 26.3.0-devel (git-5a07217f03)`
- Vulkan API: `1.4.363`
- ICD SHA-256: `576e37de9a3b60dda9a972a6f91c3a50e09238bd31a9888e04215c7b554c803a`

## Gate separation

DXVK COMMON is `dxvk_common_required` only. D3D9/D3D10/D3D11 profiles are not COMMON.
vkd3d HARD_REQUIREMENTS is each tag README. PROFILE_BASELINE is `VP_D3D12_FL_11_0_baseline`.
DEVICE_CREATE is `vkd3d_init_device_caps` E_INVALIDARG. FEATURE_LEVEL is stock `CheckFeatureSupport`.
Implied D3D_FEATURE_LEVEL_11_0 from P21 is not FEATURE_LEVEL and is not a PASS.
P19 DEVICE_CREATE listed only `robustImageAccess2`. P21 also fails `transformFeedbackQueries`. P23 uses P21.

## Worker report

```text
PANVK:
COMMIT=fc8a759e7d1b2b8de01c0e96f1fdc5e3950ba1a3
MESA_COMMIT=5a07217f034b3e50d8c7c7794f97a2df1742613b
PATCH_SERIES=sha256:c0bbdeef591b206a2f3ae36dc6191c08854399039075f8d69f103e33c0eca2f8
DEVICE=Poco X6 Pro / duchamp
GPU_ID=0xb8a31030
KBASE_UAPI=1.21

CORE:
VULKAN_API=1.4.363
GEOMETRY_SHADER=False
TESSELLATION_SHADER=False
FILL_MODE_NON_SOLID=False
MULTI_VIEWPORT=False
CLIP_DISTANCE=False
CULL_DISTANCE=False
BC=False
PIPELINE_STATS=False
SPARSE_BINDING=False

ROBUSTNESS2:
EXTENSION=True
ROBUST_BUFFER_ACCESS2=True
ROBUST_IMAGE_ACCESS2=False
NULL_DESCRIPTOR=True

DESCRIPTORS:
DESCRIPTOR_INDEXING=True
ALL_REQUIRED_INDEXING_BITS=True
MAX_UAB_STORAGE_BUFFER=1048576
MAX_UAB_SAMPLED_IMAGE=1048576
MAX_UAB_STORAGE_IMAGE=1048576
MAX_UAB_SAMPLER=1048576
MAX_UAB_ALL_POOLS=4294967295

PUSH:
MAX_PUSH_CONSTANTS=256
MAX_PUSH_DESCRIPTORS=32

TRANSFORM_FEEDBACK:
EXTENSION=False
TRANSFORM_FEEDBACK=False
GEOMETRY_STREAMS=False
QUERIES=False

DXVK_1_10_3:
D3D9=FAIL
D3D10=FAIL
D3D11_FL10_1=FAIL
D3D11_FL11_0=FAIL

DXVK_2_7_1:
COMMON=PASS
D3D9=FAIL
D3D10_10_1=FAIL
D3D11_11_0=FAIL

DXVK_3_1_1:
COMMON=PASS
D3D9=FAIL
D3D10_10_1=FAIL
D3D11_11_0=FAIL

VKD3D_2_0:
HARD=PASS
DEVICE=PASS

VKD3D_2_14_1:
HARD=PASS
BASELINE=FAIL
DEVICE=FAIL
FEATURE_LEVEL=NOT_AVAILABLE

VKD3D_3_0_1:
HARD=PASS
BASELINE=FAIL
DEVICE=FAIL
FEATURE_LEVEL=NOT_AVAILABLE

UPSTREAM_BACKPORTS:
- None in P23. P5-P8 descriptor/robustness2-buffer/push/common gates were already present at pinned Mesa 5a07217. No P23 backport series.

NEW_IMPLEMENTATIONS:
- None. P9-P17, P20, P22 left missing graphics/XFB/tess/pipeline-stats/sparse bits false. P23 did not implement features.

UNSUPPORTED:
- geometryShader
- tessellationShader
- fillModeNonSolid
- multiViewport
- shaderClipDistance
- shaderCullDistance
- textureCompressionBC (native=false, emulated=false)
- VK_EXT_transform_feedback
- transformFeedback
- geometryStreams
- transformFeedbackQueries
- pipelineStatisticsQuery
- robustImageAccess2
- vertexPipelineStoresAndAtomics
- sparseBinding / sparseResidency* / shaderResourceResidency / shaderResourceMinLod / VK_QUEUE_SPARSE_BINDING_BIT (P22 NO-GO)

CTS:
- NOT_TESTED/BLOCKED. No deqp-vk executable or matching CTS test list on host or G615. No Vulkan conformance claim.

REGRESSIONS:
- None from P23. No PanVK patch. No feature-bit change. Stock DXVK/vkd3d smoke remains BLOCKED (Android ICD is not a legal host for those Windows binaries).

NEXT_SINGLE_BLOCKER=robustImageAccess2 (P6 left false; stock vkd3d-proton 2.14.1/3.0.1 DEVICE_CREATE returns E_INVALIDARG). DXVK D3D9 still independently FAILs on geometryShader, fillModeNonSolid, shaderClipDistance, shaderCullDistance, textureCompressionBC.
```

## Sources

- `validation/g615-v11-csf/consumer-capabilities.json`
- `validation/g615-v11-csf/p13-d3d9.json`
- `validation/g615-v11-csf/p16-d3d10.json`
- `validation/g615-v11-csf/p18-d3d11-fl11.json`
- `validation/g615-v11-csf/p19-vkd3d-profile-baseline.json`
- `validation/g615-v11-csf/p21-d3d12-feature-level.json`
- `validation/g615-v11-csf/p22-kbase-sparse-feasibility.json`
- live `scripts/evaluate-consumer-profile.py` for COMMON and `VP_DXVK_d3d11_level_11_1_baseline`

## Exit

P23 exits `FAIL`. Matrix is complete. No spoofed feature bits. Next engineering step is not sparse.
