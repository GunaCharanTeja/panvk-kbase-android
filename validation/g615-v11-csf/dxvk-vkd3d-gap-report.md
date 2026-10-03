# G615 DXVK/vkd3d Gap Report

## Status 2026-10-02

**Summary:** DXVK D3D11 FL11_0 reached (`0xb000`); `vertexPipelineStoresAndAtomics` landed in beta.7 (078); FL11_1 not yet re-checked with DXVK. Current release: beta.8 (patches up to 084).

### Gaps closed and device-proven:
- **`geometryShader`**: exposed and device-proven (patch 046, GS invocations 64 in patch 071 released in beta.5; geometry 189/0, instanced 20/0).
- **`tessellationShader`**: exposed and device-proven (patches 065-068; matrices 24/24, CTS tessellation 526/0).
- **Transform feedback** (`VK_EXT_transform_feedback`, `transformFeedback`, `geometryStreams`): exposed and device-proven (patches 065-068; matrices 17/17 incl. tes_capture, CTS transform_feedback 15793/0).
- **`pipelineStatisticsQuery`**: exposed and device-proven (patches 049-054; CTS 14,098 pass / 0 fail, statistics_query 15374/0).
- **`multiViewport`**: exposed and device-proven (device matrices 0 fail).
- **`fillModeNonSolid`**: exposed and device-proven (device matrices 0 fail).
- **Clip/cull distance** (`shaderClipDistance`, `shaderCullDistance`): exposed and device-proven (device matrices 0 fail).
- **BC (GPU decode)**: default on; CTS BC subset 1863 pass / 0 fail, copy_and_blit 9620 / 0.
- **`VK_EXT_memory_priority` + `VK_EXT_pageable_device_local_memory`**: exposed and device-proven (patch 069, released in beta.5; CTS 224/0, 202/0, api.info 7799/0).
- **`alphaToOne`**: exposed and device-proven (patch 070, released in beta.5; CTS 123/0).
- **`maxGeometryShaderInvocations` 64**: exposed and device-proven (patch 071, released in beta.5; geometry 193/0, instanced 20/0).
- **`VK_EXT_multi_draw`**: exposed and device-proven (patch 072, released in beta.5; CTS 12704/0).
- **`VK_EXT_primitives_generated_query`**: exposed and device-proven (patch 073, released in beta.5; CTS 75206/0).
- **`variableMultisampleRate` on v10+**: exposed and device-proven (patch 074, in tree on dx-p5, pending beta.6; splits render context on sample count change in no-attachment passes; CTS variable_rate + mixed_attachment_samples 504/0, no-attachment / dynamic_rendering subset 1756/0).
- **Correctness fixes**:
  - `SYNC_FD` export via kbase KCPU queue (patch 075, in tree on dx-p5, pending beta.6; CQS wait then fence signal; api.external sync_fd + synchronization.cross_instance 113 ResourceError -> 1996 pass / 0 fail).
  - Honour geometry shader viewport index on v10+ (patch 076, in tree on dx-p5, pending beta.6; draw scissor tests 18 fail -> 88/88).
  - System scope for subqueue sync signals on kbase (patch 077, in tree on dx-p5, pending beta.6; synchronization.signal_order 11-16 timeouts -> 1316 pass / 0 aborted).

### Remaining gaps:
- **`vertexPipelineStoresAndAtomics`**: CLOSED in beta.7 (patch 078; CTS `atomic_operations *_vertex*` 66/0).
- **`shaderOutputViewportIndex` from VS/TES**: GS viewport index fixed in patch 076; feature bit remains off pending VS/TES (2-3 h).
- **`depthBounds`**: exact check via tile-buffer stored depth pending (3-5 h).
- **XFB 65536 cap**: GPU chunking needed.
- **XFB `query_copy` DeviceLost**: 2 intermittent DeviceLost in CTS `transform_feedback query_copy_*` (pending rerun on dx-p5; may be resolved by patch 077 subqueue timeout fix).
- **`robustImageAccess2`**: vkd3d hard requirement, deferred (vkd3d out of scope for now; WIP in `work/mesa-ria2`, not proven).
- **Sparse / ROV** (`sparseBinding`, `sparseResidency*`, `VK_EXT_fragment_shader_interlock`): FL12 requirements, impossible/blocked on kbase (sparse is NO-GO on kbase).
- **`shaderFloat64` (fp64)**: unsupported on hardware / PanVK.

Generated from the direct-ICD capture and exact tagged official profiles. `UNKNOWN` is not failure proof or support proof.

## Provenance

- Device: `Mali-G615 MC6` / `0xb8a31030`
- Capture ICD SHA-256: `576e37de9a3b60dda9a972a6f91c3a50e09238bd31a9888e04215c7b554c803a`
- Pinned Mesa: `5a07217f034b3e50d8c7c7794f97a2df1742613b`
- Audited Mesa origin/main: `e1f3f372c4a661cd0e71a0cdafc4d469d56ecf35`
- Post-pin coherent requirement backports: none

## dxvk-1.10.3 source-derived

### D3D9: PASS

Source: `v1.10.3` `src/d3d9/d3d9_device.cpp:3887-3950`

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `geometryShader` | `True` | `True` | Exposed (046, 071) | HARDWARE_NATIVE | CLOSED (device-proven; CTS geometry 189/0, instanced 20/0) |
| `fillModeNonSolid` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `shaderClipDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `shaderCullDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `textureCompressionBC` | `True` | `True` | GPU decode default on | GPU_LOWERED | CLOSED (device-proven; CTS BC 1863/0, copy_and_blit 9620/0) |
| `multiViewport` | `True` | `True` | Exposed | HARDWARE_NATIVE | CLOSED (device-proven; device matrices 0 fail) |

### D3D10_10_1: PASS

Source: `v1.10.3` `src/d3d11/d3d11_device.cpp:1927-1992`

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `geometryShader` | `True` | `True` | Exposed (046, 071) | HARDWARE_NATIVE | CLOSED (device-proven; CTS geometry 189/0, instanced 20/0) |
| `fillModeNonSolid` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `shaderClipDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `shaderCullDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `textureCompressionBC` | `True` | `True` | GPU decode default on | GPU_LOWERED | CLOSED (device-proven; CTS BC 1863/0, copy_and_blit 9620/0) |
| `multiViewport` | `True` | `True` | Exposed | HARDWARE_NATIVE | CLOSED (device-proven; device matrices 0 fail) |
| `transformFeedback` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; CTS transform_feedback 15793/0) |
| `geometryStreams` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; matrices 17/17) |
| `VK_EXT_transform_feedback` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven) |

### D3D11_FL11_0: PASS

Source: `v1.10.3` `src/d3d11/d3d11_device.cpp:1994-2004`

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `geometryShader` | `True` | `True` | Exposed (046, 071) | HARDWARE_NATIVE | CLOSED (device-proven; CTS geometry 189/0, instanced 20/0) |
| `fillModeNonSolid` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `shaderClipDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `shaderCullDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `textureCompressionBC` | `True` | `True` | GPU decode default on | GPU_LOWERED | CLOSED (device-proven; CTS BC 1863/0, copy_and_blit 9620/0) |
| `multiViewport` | `True` | `True` | Exposed | HARDWARE_NATIVE | CLOSED (device-proven; device matrices 0 fail) |
| `transformFeedback` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; CTS transform_feedback 15793/0) |
| `geometryStreams` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; matrices 17/17) |
| `tessellationShader` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; CTS tessellation 526/0) |

## vkd3d-proton-2.0 source-derived

*(Status 2026-10-02: Deferred — vkd3d/D3D12 and FL12 out of scope for now. Blocked by `robustImageAccess2=false`; sparse is NO-GO on kbase).*

Source: `v2.0` `README.md:17-24`

Hard requirements: **PASS (reported values only; P5 workload proof pending)**

| Requirement | Current | Required |
|---|---:|---:|
| `apiVersion >= 1.1` | `True` | `True` |
| `VK_EXT_descriptor_indexing` | `True` | `True` |
| `VK_KHR_timeline_semaphore` | `True` | `True` |
| `required descriptor-indexing features` | `True` | `True` |
| `required UpdateAfterBind limits >= 1000000` | `True` | `True` |

## dxvk-2.7.1

Source: `v2.7.1` `https://raw.githubusercontent.com/doitsujin/dxvk/v2.7.1/VP_DXVK_requirements.json` (`1e219227adeba497e387fbad72847fef4d064667ede946f720d8e8a67bea6edf`)

### VP_DXVK_d3d9_baseline: PASS

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `features.VkPhysicalDeviceFeatures.geometryShader` | `True` | `True` | Exposed (046, 071) | HARDWARE_NATIVE | CLOSED (device-proven; CTS geometry 189/0, instanced 20/0) |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `True` | `True` | GPU decode default on | GPU_LOWERED | CLOSED (device-proven; CTS BC 1863/0, copy_and_blit 9620/0) |

### VP_DXVK_d3d9_optimal: PASS

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `features.VkPhysicalDeviceFeatures.geometryShader` | `True` | `True` | Exposed (046, 071) | HARDWARE_NATIVE | CLOSED (device-proven; CTS geometry 189/0, instanced 20/0) |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `True` | `True` | GPU decode default on | GPU_LOWERED | CLOSED (device-proven; CTS BC 1863/0, copy_and_blit 9620/0) |

### VP_DXVK_d3d10_level_10_1_baseline: PASS

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `1` | `1` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven) |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `True` | `True` | Exposed (046, 071) | HARDWARE_NATIVE | CLOSED (device-proven; CTS geometry 189/0, instanced 20/0) |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `True` | `True` | Exposed | HARDWARE_NATIVE | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `True` | `True` | GPU decode default on | GPU_LOWERED | CLOSED (device-proven; CTS BC 1863/0, copy_and_blit 9620/0) |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; CTS transform_feedback 15793/0) |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; matrices 17/17) |

### VP_DXVK_d3d11_level_11_0_baseline: PASS

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `1` | `1` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven) |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `True` | `True` | Exposed (046, 071) | HARDWARE_NATIVE | CLOSED (device-proven; CTS geometry 189/0, instanced 20/0) |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `True` | `True` | Exposed | HARDWARE_NATIVE | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `True` | `True` | GPU decode default on | GPU_LOWERED | CLOSED (device-proven; CTS BC 1863/0, copy_and_blit 9620/0) |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; CTS transform_feedback 15793/0) |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; matrices 17/17) |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; CTS tessellation 526/0) |

### VP_DXVK_d3d11_level_11_1_baseline: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `1` | `1` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven) |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `True` | `True` | Exposed (046, 071) | HARDWARE_NATIVE | CLOSED (device-proven; CTS geometry 189/0, instanced 20/0) |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `True` | `True` | Exposed | HARDWARE_NATIVE | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `True` | `True` | GPU decode default on | GPU_LOWERED | CLOSED (device-proven; CTS BC 1863/0, copy_and_blit 9620/0) |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; CTS transform_feedback 15793/0) |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; matrices 17/17) |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; CTS tessellation 526/0) |
| `features.VkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics` | `True` (beta.7) | `True` | Patch 078, CTS 66/0 | CLOSED | Re-check FL11_1 with DXVK |

### VP_DXVK_d3d11_level_11_1_optimal: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `1` | `1` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven) |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `True` | `True` | Exposed (046, 071) | HARDWARE_NATIVE | CLOSED (device-proven; CTS geometry 189/0, instanced 20/0) |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `True` | `True` | Exposed | HARDWARE_NATIVE | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `True` | `True` | GPU decode default on | GPU_LOWERED | CLOSED (device-proven; CTS BC 1863/0, copy_and_blit 9620/0) |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; CTS transform_feedback 15793/0) |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; matrices 17/17) |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; CTS tessellation 526/0) |
| `features.VkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics` | `True` (beta.7) | `True` | Patch 078, CTS 66/0 | CLOSED | Re-check FL11_1 with DXVK |

### VP_DXVK_d3d11_level_12_0_optimal: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `1` | `1` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven) |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `True` | `True` | Exposed (046, 071) | HARDWARE_NATIVE | CLOSED (device-proven; CTS geometry 189/0, instanced 20/0) |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `True` | `True` | Exposed | HARDWARE_NATIVE | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `True` | `True` | GPU decode default on | GPU_LOWERED | CLOSED (device-proven; CTS BC 1863/0, copy_and_blit 9620/0) |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; CTS transform_feedback 15793/0) |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; matrices 17/17) |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; CTS tessellation 526/0) |
| `features.VkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics` | `True` (beta.7) | `True` | Patch 078, CTS 66/0 | CLOSED | Re-check FL11_1 with DXVK |
| `features.VkPhysicalDeviceFeatures.shaderResourceResidency` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.shaderResourceMinLod` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.sparseBinding` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyBuffer` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyAliased` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage2D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard2DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyAlignedMipSize` | `None` | `False` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyNonResidentStrict` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |

## dxvk-3.1.1

Source: `v3.1.1` `https://raw.githubusercontent.com/doitsujin/dxvk/v3.1.1/VP_DXVK_requirements.json` (`d490930920a24fb9ac4a0585c68042929acd7e700e04d0a032c79d9fddf1c462`)

### VP_DXVK_d3d9_baseline: PASS

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `features.VkPhysicalDeviceFeatures.geometryShader` | `True` | `True` | Exposed (046, 071) | HARDWARE_NATIVE | CLOSED (device-proven; CTS geometry 189/0, instanced 20/0) |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `True` | `True` | GPU decode default on | GPU_LOWERED | CLOSED (device-proven; CTS BC 1863/0, copy_and_blit 9620/0) |

### VP_DXVK_d3d9_optimal: PASS

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `features.VkPhysicalDeviceFeatures.geometryShader` | `True` | `True` | Exposed (046, 071) | HARDWARE_NATIVE | CLOSED (device-proven; CTS geometry 189/0, instanced 20/0) |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `True` | `True` | GPU decode default on | GPU_LOWERED | CLOSED (device-proven; CTS BC 1863/0, copy_and_blit 9620/0) |

### VP_DXVK_d3d10_level_10_1_baseline: PASS

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `1` | `1` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven) |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `True` | `True` | Exposed (046, 071) | HARDWARE_NATIVE | CLOSED (device-proven; CTS geometry 189/0, instanced 20/0) |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `True` | `True` | Exposed | HARDWARE_NATIVE | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `True` | `True` | GPU decode default on | GPU_LOWERED | CLOSED (device-proven; CTS BC 1863/0, copy_and_blit 9620/0) |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; CTS transform_feedback 15793/0) |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; matrices 17/17) |

### VP_DXVK_d3d11_level_11_0_baseline: PASS

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `1` | `1` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven) |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `True` | `True` | Exposed (046, 071) | HARDWARE_NATIVE | CLOSED (device-proven; CTS geometry 189/0, instanced 20/0) |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `True` | `True` | Exposed | HARDWARE_NATIVE | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `True` | `True` | GPU decode default on | GPU_LOWERED | CLOSED (device-proven; CTS BC 1863/0, copy_and_blit 9620/0) |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; CTS transform_feedback 15793/0) |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; matrices 17/17) |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; CTS tessellation 526/0) |

### VP_DXVK_d3d11_level_11_1_baseline: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `1` | `1` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven) |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `True` | `True` | Exposed (046, 071) | HARDWARE_NATIVE | CLOSED (device-proven; CTS geometry 189/0, instanced 20/0) |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `True` | `True` | Exposed | HARDWARE_NATIVE | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `True` | `True` | GPU decode default on | GPU_LOWERED | CLOSED (device-proven; CTS BC 1863/0, copy_and_blit 9620/0) |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; CTS transform_feedback 15793/0) |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; matrices 17/17) |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; CTS tessellation 526/0) |
| `features.VkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics` | `True` (beta.7) | `True` | Patch 078, CTS 66/0 | CLOSED | Re-check FL11_1 with DXVK |

### VP_DXVK_d3d11_level_11_1_optimal: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `1` | `1` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven) |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `True` | `True` | Exposed (046, 071) | HARDWARE_NATIVE | CLOSED (device-proven; CTS geometry 189/0, instanced 20/0) |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `True` | `True` | Exposed | HARDWARE_NATIVE | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `True` | `True` | GPU decode default on | GPU_LOWERED | CLOSED (device-proven; CTS BC 1863/0, copy_and_blit 9620/0) |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; CTS transform_feedback 15793/0) |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; matrices 17/17) |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; CTS tessellation 526/0) |
| `features.VkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics` | `True` (beta.7) | `True` | Patch 078, CTS 66/0 | CLOSED | Re-check FL11_1 with DXVK |

### VP_DXVK_d3d11_level_12_0_optimal: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `1` | `1` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven) |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `True` | `True` | Exposed (046, 071) | HARDWARE_NATIVE | CLOSED (device-proven; CTS geometry 189/0, instanced 20/0) |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `True` | `True` | Exposed | HARDWARE_NATIVE | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `True` | `True` | GPU decode default on | GPU_LOWERED | CLOSED (device-proven; CTS BC 1863/0, copy_and_blit 9620/0) |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; CTS transform_feedback 15793/0) |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; matrices 17/17) |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; CTS tessellation 526/0) |
| `features.VkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics` | `True` (beta.7) | `True` | Patch 078, CTS 66/0 | CLOSED | Re-check FL11_1 with DXVK |
| `features.VkPhysicalDeviceFeatures.shaderResourceResidency` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.shaderResourceMinLod` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.sparseBinding` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyBuffer` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyAliased` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage2D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard2DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyAlignedMipSize` | `None` | `False` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyNonResidentStrict` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |

### VP_DXVK_d3d11_level_12_1_optimal: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `1` | `1` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven) |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `True` | `True` | Exposed (046, 071) | HARDWARE_NATIVE | CLOSED (device-proven; CTS geometry 189/0, instanced 20/0) |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `True` | `True` | Exposed | HARDWARE_NATIVE | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `True` | `True` | Exposed | GPU_LOWERED | CLOSED (device-proven; device matrices 0 fail) |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `True` | `True` | GPU decode default on | GPU_LOWERED | CLOSED (device-proven; CTS BC 1863/0, copy_and_blit 9620/0) |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; CTS transform_feedback 15793/0) |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; matrices 17/17) |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `True` | `True` | Exposed (065-068) | GPU_LOWERED | CLOSED (device-proven; CTS tessellation 526/0) |
| `features.VkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics` | `True` (beta.7) | `True` | Patch 078, CTS 66/0 | CLOSED | Re-check FL11_1 with DXVK |
| `features.VkPhysicalDeviceFeatures.shaderResourceResidency` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.shaderResourceMinLod` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.sparseBinding` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyBuffer` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyAliased` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage2D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard2DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyAlignedMipSize` | `None` | `False` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyNonResidentStrict` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_fragment_shader_interlock` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT.fragmentShaderPixelInterlock` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceConservativeRasterizationPropertiesEXT.degenerateTrianglesRasterized` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceConservativeRasterizationPropertiesEXT.fullyCoveredFragmentShaderInputVariable` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |

## vkd3d-2.14.1

*(Status 2026-10-02: Deferred — vkd3d/D3D12 and FL12 out of scope for now. Blocked by `robustImageAccess2=false`; sparse is NO-GO on kbase).*

Source: `v2.14.1` `https://raw.githubusercontent.com/HansKristian-Work/vkd3d-proton/v2.14.1/VP_D3D12_VKD3D_PROTON_profile.json` (`9ff2e08e82f03f214714591e0727a35c164639e71879c9721ca9e5b4bd0ddd17`)

### VP_D3D12_FL_11_0_baseline: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRobustness2FeaturesEXT.robustImageAccess2` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `False` | `True` | Hardware-dependent; G615 mask is zero | UNSUPPORTED_NATIVE | Keep false unless transparent BC emulation is complete |
| `features.VkPhysicalDeviceFeatures.pipelineStatisticsQuery` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `properties.VkPhysicalDeviceTransformFeedbackPropertiesEXT.transformFeedbackQueries` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.limits.bufferImageGranularity` | `64` | `65536` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[0]` | `None` | `VK_SUBGROUP_FEATURE_BASIC_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |

### VP_D3D12_FL_11_1_baseline: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRobustness2FeaturesEXT.robustImageAccess2` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `False` | `True` | Hardware-dependent; G615 mask is zero | UNSUPPORTED_NATIVE | Keep false unless transparent BC emulation is complete |
| `features.VkPhysicalDeviceFeatures.pipelineStatisticsQuery` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `properties.VkPhysicalDeviceTransformFeedbackPropertiesEXT.transformFeedbackQueries` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.limits.bufferImageGranularity` | `64` | `65536` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[0]` | `None` | `VK_SUBGROUP_FEATURE_BASIC_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |

### VP_D3D12_FL_12_0_baseline: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRobustness2FeaturesEXT.robustImageAccess2` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `False` | `True` | Hardware-dependent; G615 mask is zero | UNSUPPORTED_NATIVE | Keep false unless transparent BC emulation is complete |
| `features.VkPhysicalDeviceFeatures.pipelineStatisticsQuery` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `properties.VkPhysicalDeviceTransformFeedbackPropertiesEXT.transformFeedbackQueries` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.limits.bufferImageGranularity` | `64` | `65536` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard2DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyAlignedMipSize` | `None` | `False` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyNonResidentStrict` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[0]` | `None` | `VK_SUBGROUP_FEATURE_BALLOT_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[1]` | `None` | `VK_SUBGROUP_FEATURE_BASIC_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[2]` | `None` | `VK_SUBGROUP_FEATURE_VOTE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[3]` | `None` | `VK_SUBGROUP_FEATURE_SHUFFLE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[4]` | `None` | `VK_SUBGROUP_FEATURE_QUAD_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[5]` | `None` | `VK_SUBGROUP_FEATURE_ARITHMETIC_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedStages[0]` | `None` | `VK_SHADER_STAGE_COMPUTE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedStages[1]` | `None` | `VK_SHADER_STAGE_FRAGMENT_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.sparseBinding` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyAliased` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyBuffer` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage2D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.shaderResourceResidency` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.shaderResourceMinLod` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueFlags[0]` | `None` | `VK_QUEUE_SPARSE_BINDING_BIT` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueCount` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |

### VP_D3D12_FL_12_0_optimal: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRobustness2FeaturesEXT.robustImageAccess2` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `False` | `True` | Hardware-dependent; G615 mask is zero | UNSUPPORTED_NATIVE | Keep false unless transparent BC emulation is complete |
| `features.VkPhysicalDeviceFeatures.pipelineStatisticsQuery` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `properties.VkPhysicalDeviceTransformFeedbackPropertiesEXT.transformFeedbackQueries` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.limits.bufferImageGranularity` | `64` | `65536` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard2DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyAlignedMipSize` | `None` | `False` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyNonResidentStrict` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan13Properties.requiredSubgroupSizeStages[0]` | `None` | `VK_SHADER_STAGE_COMPUTE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.sparseBinding` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyAliased` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyBuffer` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage2D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.shaderResourceResidency` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.shaderResourceMinLod` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueFlags[0]` | `None` | `VK_QUEUE_SPARSE_BINDING_BIT` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueCount` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_descriptor_buffer` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_AMD_buffer_marker` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceDescriptorBufferFeaturesEXT.descriptorBuffer` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceDescriptorBufferFeaturesEXT.descriptorBufferPushDescriptors` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceExtendedDynamicState2FeaturesEXT.extendedDynamicState2PatchControlPoints` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT.swapchainMaintenance1` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |

### VP_D3D12_FL_12_1_baseline: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRobustness2FeaturesEXT.robustImageAccess2` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `False` | `True` | Hardware-dependent; G615 mask is zero | UNSUPPORTED_NATIVE | Keep false unless transparent BC emulation is complete |
| `features.VkPhysicalDeviceFeatures.pipelineStatisticsQuery` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `properties.VkPhysicalDeviceTransformFeedbackPropertiesEXT.transformFeedbackQueries` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.limits.bufferImageGranularity` | `64` | `65536` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard2DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyAlignedMipSize` | `None` | `False` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyNonResidentStrict` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[0]` | `None` | `VK_SUBGROUP_FEATURE_BALLOT_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[1]` | `None` | `VK_SUBGROUP_FEATURE_BASIC_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[2]` | `None` | `VK_SUBGROUP_FEATURE_VOTE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[3]` | `None` | `VK_SUBGROUP_FEATURE_SHUFFLE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[4]` | `None` | `VK_SUBGROUP_FEATURE_QUAD_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[5]` | `None` | `VK_SUBGROUP_FEATURE_ARITHMETIC_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedStages[0]` | `None` | `VK_SHADER_STAGE_COMPUTE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedStages[1]` | `None` | `VK_SHADER_STAGE_FRAGMENT_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.sparseBinding` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyAliased` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyBuffer` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage2D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.shaderResourceResidency` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.shaderResourceMinLod` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueFlags[0]` | `None` | `VK_QUEUE_SPARSE_BINDING_BIT` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueCount` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_fragment_shader_interlock` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT.fragmentShaderSampleInterlock` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT.fragmentShaderPixelInterlock` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |

### VP_D3D12_FL_12_2_baseline: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRobustness2FeaturesEXT.robustImageAccess2` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `False` | `True` | Hardware-dependent; G615 mask is zero | UNSUPPORTED_NATIVE | Keep false unless transparent BC emulation is complete |
| `features.VkPhysicalDeviceFeatures.pipelineStatisticsQuery` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `properties.VkPhysicalDeviceTransformFeedbackPropertiesEXT.transformFeedbackQueries` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[0]` | `None` | `VK_SUBGROUP_FEATURE_BALLOT_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[1]` | `None` | `VK_SUBGROUP_FEATURE_BASIC_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[2]` | `None` | `VK_SUBGROUP_FEATURE_VOTE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[3]` | `None` | `VK_SUBGROUP_FEATURE_SHUFFLE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[4]` | `None` | `VK_SUBGROUP_FEATURE_QUAD_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[5]` | `None` | `VK_SUBGROUP_FEATURE_ARITHMETIC_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedStages[0]` | `None` | `VK_SHADER_STAGE_COMPUTE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedStages[1]` | `None` | `VK_SHADER_STAGE_FRAGMENT_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.sparseBinding` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyAliased` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyBuffer` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage2D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.shaderResourceResidency` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.shaderResourceMinLod` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueFlags[0]` | `None` | `VK_QUEUE_SPARSE_BINDING_BIT` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueCount` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_fragment_shader_interlock` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT.fragmentShaderSampleInterlock` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT.fragmentShaderPixelInterlock` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_tracing_pipeline` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_acceleration_structure` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_deferred_host_operations` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_query` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_fragment_shading_rate` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_pipeline_library_group_handles` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_tracing_maintenance1` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_mesh_shader` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R32G32_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R32G32B32_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_A2B10G10R10_UNORM_PACK32.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8B8A8_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8B8A8_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipelineTraceRaysIndirect` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTraversalPrimitiveCulling` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayQueryFeaturesKHR.rayQuery` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR.rayTracingMaintenance1` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR.rayTracingPipelineTraceRaysIndirect2` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDevicePipelineLibraryGroupHandlesFeaturesEXT.pipelineLibraryGroupHandles` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.taskShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.meshShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.primitiveFragmentShadingRateMeshShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.pipelineFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.primitiveFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.attachmentFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.depthBounds` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage3D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceVulkan12Features.shaderOutputViewportIndex` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceConservativeRasterizationPropertiesEXT.degenerateTrianglesRasterized` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceConservativeRasterizationPropertiesEXT.fullyCoveredFragmentShaderInputVariable` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceFragmentShadingRatePropertiesKHR.fragmentShadingRateNonTrivialCombinerOps` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueFlags[0]` | `None` | `VK_QUEUE_TRANSFER_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.timestampValidBits` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.limits.bufferImageGranularity` | `64` | `65536` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard2DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard3DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyAlignedMipSize` | `None` | `False` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyNonResidentStrict` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |

### VP_D3D12_FL_12_2_optimal: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRobustness2FeaturesEXT.robustImageAccess2` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `False` | `True` | Hardware-dependent; G615 mask is zero | UNSUPPORTED_NATIVE | Keep false unless transparent BC emulation is complete |
| `features.VkPhysicalDeviceFeatures.pipelineStatisticsQuery` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `properties.VkPhysicalDeviceTransformFeedbackPropertiesEXT.transformFeedbackQueries` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceVulkan13Properties.requiredSubgroupSizeStages[0]` | `None` | `VK_SHADER_STAGE_COMPUTE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.sparseBinding` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyAliased` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyBuffer` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage2D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.shaderResourceResidency` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.shaderResourceMinLod` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueFlags[0]` | `None` | `VK_QUEUE_SPARSE_BINDING_BIT` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueCount` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_fragment_shader_interlock` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT.fragmentShaderSampleInterlock` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT.fragmentShaderPixelInterlock` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_tracing_pipeline` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_acceleration_structure` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_deferred_host_operations` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_query` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_fragment_shading_rate` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_pipeline_library_group_handles` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_tracing_maintenance1` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_mesh_shader` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R32G32_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R32G32B32_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_A2B10G10R10_UNORM_PACK32.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8B8A8_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8B8A8_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipelineTraceRaysIndirect` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTraversalPrimitiveCulling` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayQueryFeaturesKHR.rayQuery` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR.rayTracingMaintenance1` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR.rayTracingPipelineTraceRaysIndirect2` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDevicePipelineLibraryGroupHandlesFeaturesEXT.pipelineLibraryGroupHandles` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.taskShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.meshShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.primitiveFragmentShadingRateMeshShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.pipelineFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.primitiveFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.attachmentFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.depthBounds` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage3D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceVulkan12Features.shaderOutputViewportIndex` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceConservativeRasterizationPropertiesEXT.degenerateTrianglesRasterized` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceConservativeRasterizationPropertiesEXT.fullyCoveredFragmentShaderInputVariable` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceFragmentShadingRatePropertiesKHR.fragmentShadingRateNonTrivialCombinerOps` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueFlags[0]` | `None` | `VK_QUEUE_TRANSFER_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.timestampValidBits` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.limits.bufferImageGranularity` | `64` | `65536` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard2DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard3DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyAlignedMipSize` | `None` | `False` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyNonResidentStrict` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_descriptor_buffer` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_AMD_buffer_marker` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceDescriptorBufferFeaturesEXT.descriptorBuffer` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceDescriptorBufferFeaturesEXT.descriptorBufferPushDescriptors` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceExtendedDynamicState2FeaturesEXT.extendedDynamicState2PatchControlPoints` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT.swapchainMaintenance1` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |

### VP_D3D12_maximum_radv: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRobustness2FeaturesEXT.robustImageAccess2` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `False` | `True` | Hardware-dependent; G615 mask is zero | UNSUPPORTED_NATIVE | Keep false unless transparent BC emulation is complete |
| `features.VkPhysicalDeviceFeatures.pipelineStatisticsQuery` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `properties.VkPhysicalDeviceTransformFeedbackPropertiesEXT.transformFeedbackQueries` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceVulkan13Properties.requiredSubgroupSizeStages[0]` | `None` | `VK_SHADER_STAGE_COMPUTE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.sparseBinding` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyAliased` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyBuffer` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage2D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.shaderResourceResidency` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.shaderResourceMinLod` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueFlags[0]` | `None` | `VK_QUEUE_SPARSE_BINDING_BIT` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueCount` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_tracing_pipeline` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_acceleration_structure` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_deferred_host_operations` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_query` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_fragment_shading_rate` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_pipeline_library_group_handles` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_tracing_maintenance1` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_mesh_shader` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R32G32_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R32G32B32_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_A2B10G10R10_UNORM_PACK32.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8B8A8_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8B8A8_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipelineTraceRaysIndirect` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTraversalPrimitiveCulling` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayQueryFeaturesKHR.rayQuery` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR.rayTracingMaintenance1` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR.rayTracingPipelineTraceRaysIndirect2` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDevicePipelineLibraryGroupHandlesFeaturesEXT.pipelineLibraryGroupHandles` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.taskShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.meshShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.primitiveFragmentShadingRateMeshShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.pipelineFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.primitiveFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.attachmentFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.depthBounds` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage3D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceVulkan12Features.shaderOutputViewportIndex` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceConservativeRasterizationPropertiesEXT.degenerateTrianglesRasterized` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceConservativeRasterizationPropertiesEXT.fullyCoveredFragmentShaderInputVariable` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceFragmentShadingRatePropertiesKHR.fragmentShadingRateNonTrivialCombinerOps` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueFlags[0]` | `None` | `VK_QUEUE_TRANSFER_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.timestampValidBits` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.limits.bufferImageGranularity` | `64` | `65536` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard2DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard3DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyAlignedMipSize` | `None` | `False` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyNonResidentStrict` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_descriptor_buffer` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_AMD_buffer_marker` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceDescriptorBufferFeaturesEXT.descriptorBuffer` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceDescriptorBufferFeaturesEXT.descriptorBufferPushDescriptors` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceExtendedDynamicState2FeaturesEXT.extendedDynamicState2PatchControlPoints` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT.swapchainMaintenance1` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |

### VP_D3D12_maximum_nv: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRobustness2FeaturesEXT.robustImageAccess2` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `False` | `True` | Hardware-dependent; G615 mask is zero | UNSUPPORTED_NATIVE | Keep false unless transparent BC emulation is complete |
| `features.VkPhysicalDeviceFeatures.pipelineStatisticsQuery` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `properties.VkPhysicalDeviceTransformFeedbackPropertiesEXT.transformFeedbackQueries` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceVulkan13Properties.requiredSubgroupSizeStages[0]` | `None` | `VK_SHADER_STAGE_COMPUTE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.sparseBinding` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyAliased` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyBuffer` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage2D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.shaderResourceResidency` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.shaderResourceMinLod` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueFlags[0]` | `None` | `VK_QUEUE_SPARSE_BINDING_BIT` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueCount` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_fragment_shader_interlock` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT.fragmentShaderSampleInterlock` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT.fragmentShaderPixelInterlock` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_tracing_pipeline` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_acceleration_structure` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_deferred_host_operations` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_query` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_fragment_shading_rate` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_pipeline_library_group_handles` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_tracing_maintenance1` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_mesh_shader` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R32G32_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R32G32B32_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_A2B10G10R10_UNORM_PACK32.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8B8A8_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8B8A8_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipelineTraceRaysIndirect` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTraversalPrimitiveCulling` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayQueryFeaturesKHR.rayQuery` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR.rayTracingMaintenance1` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR.rayTracingPipelineTraceRaysIndirect2` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDevicePipelineLibraryGroupHandlesFeaturesEXT.pipelineLibraryGroupHandles` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.taskShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.meshShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.primitiveFragmentShadingRateMeshShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.pipelineFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.primitiveFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.attachmentFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.depthBounds` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage3D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceVulkan12Features.shaderOutputViewportIndex` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceConservativeRasterizationPropertiesEXT.degenerateTrianglesRasterized` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceConservativeRasterizationPropertiesEXT.fullyCoveredFragmentShaderInputVariable` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceFragmentShadingRatePropertiesKHR.fragmentShadingRateNonTrivialCombinerOps` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueFlags[0]` | `None` | `VK_QUEUE_TRANSFER_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.timestampValidBits` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.limits.bufferImageGranularity` | `64` | `65536` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard2DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard3DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyAlignedMipSize` | `None` | `False` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyNonResidentStrict` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_descriptor_buffer` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_AMD_buffer_marker` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceDescriptorBufferFeaturesEXT.descriptorBuffer` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceDescriptorBufferFeaturesEXT.descriptorBufferPushDescriptors` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceExtendedDynamicState2FeaturesEXT.extendedDynamicState2PatchControlPoints` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT.swapchainMaintenance1` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |

## vkd3d-3.0.1

*(Status 2026-10-02: Deferred — vkd3d/D3D12 and FL12 out of scope for now. Blocked by `robustImageAccess2=false`; sparse is NO-GO on kbase).*

Source: `v3.0.1` `https://raw.githubusercontent.com/HansKristian-Work/vkd3d-proton/v3.0.1/VP_D3D12_VKD3D_PROTON_profile.json` (`d37e753fa81e43251bbd576b17b888c1a7139b3fc8c16bd0e0ec9c7bed7a9db6`)

### VP_D3D12_FL_11_0_baseline: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRobustness2FeaturesEXT.robustImageAccess2` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `False` | `True` | Hardware-dependent; G615 mask is zero | UNSUPPORTED_NATIVE | Keep false unless transparent BC emulation is complete |
| `features.VkPhysicalDeviceFeatures.pipelineStatisticsQuery` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `properties.VkPhysicalDeviceTransformFeedbackPropertiesEXT.transformFeedbackQueries` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.limits.bufferImageGranularity` | `64` | `65536` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[0]` | `None` | `VK_SUBGROUP_FEATURE_BASIC_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |

### VP_D3D12_FL_11_1_baseline: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRobustness2FeaturesEXT.robustImageAccess2` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `False` | `True` | Hardware-dependent; G615 mask is zero | UNSUPPORTED_NATIVE | Keep false unless transparent BC emulation is complete |
| `features.VkPhysicalDeviceFeatures.pipelineStatisticsQuery` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `properties.VkPhysicalDeviceTransformFeedbackPropertiesEXT.transformFeedbackQueries` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.limits.bufferImageGranularity` | `64` | `65536` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[0]` | `None` | `VK_SUBGROUP_FEATURE_BASIC_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |

### VP_D3D12_FL_12_0_baseline: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRobustness2FeaturesEXT.robustImageAccess2` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `False` | `True` | Hardware-dependent; G615 mask is zero | UNSUPPORTED_NATIVE | Keep false unless transparent BC emulation is complete |
| `features.VkPhysicalDeviceFeatures.pipelineStatisticsQuery` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `properties.VkPhysicalDeviceTransformFeedbackPropertiesEXT.transformFeedbackQueries` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.limits.bufferImageGranularity` | `64` | `65536` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard2DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyAlignedMipSize` | `None` | `False` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyNonResidentStrict` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[0]` | `None` | `VK_SUBGROUP_FEATURE_BALLOT_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[1]` | `None` | `VK_SUBGROUP_FEATURE_BASIC_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[2]` | `None` | `VK_SUBGROUP_FEATURE_VOTE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[3]` | `None` | `VK_SUBGROUP_FEATURE_SHUFFLE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[4]` | `None` | `VK_SUBGROUP_FEATURE_QUAD_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[5]` | `None` | `VK_SUBGROUP_FEATURE_ARITHMETIC_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedStages[0]` | `None` | `VK_SHADER_STAGE_COMPUTE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedStages[1]` | `None` | `VK_SHADER_STAGE_FRAGMENT_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.sparseBinding` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyAliased` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyBuffer` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage2D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.shaderResourceResidency` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.shaderResourceMinLod` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueFlags[0]` | `None` | `VK_QUEUE_SPARSE_BINDING_BIT` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueCount` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |

### VP_D3D12_FL_12_0_optimal: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRobustness2FeaturesEXT.robustImageAccess2` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `False` | `True` | Hardware-dependent; G615 mask is zero | UNSUPPORTED_NATIVE | Keep false unless transparent BC emulation is complete |
| `features.VkPhysicalDeviceFeatures.pipelineStatisticsQuery` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `properties.VkPhysicalDeviceTransformFeedbackPropertiesEXT.transformFeedbackQueries` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.limits.bufferImageGranularity` | `64` | `65536` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard2DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyAlignedMipSize` | `None` | `False` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyNonResidentStrict` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan13Properties.requiredSubgroupSizeStages[0]` | `None` | `VK_SHADER_STAGE_COMPUTE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.sparseBinding` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyAliased` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyBuffer` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage2D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.shaderResourceResidency` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.shaderResourceMinLod` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueFlags[0]` | `None` | `VK_QUEUE_SPARSE_BINDING_BIT` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueCount` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_descriptor_buffer` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_AMD_buffer_marker` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_maintenance10` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceDescriptorBufferFeaturesEXT.descriptorBuffer` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceDescriptorBufferFeaturesEXT.descriptorBufferPushDescriptors` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceExtendedDynamicState2FeaturesEXT.extendedDynamicState2PatchControlPoints` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT.swapchainMaintenance1` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMaintenance10FeaturesKHR.maintenance10` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |

### VP_D3D12_FL_12_1_baseline: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRobustness2FeaturesEXT.robustImageAccess2` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `False` | `True` | Hardware-dependent; G615 mask is zero | UNSUPPORTED_NATIVE | Keep false unless transparent BC emulation is complete |
| `features.VkPhysicalDeviceFeatures.pipelineStatisticsQuery` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `properties.VkPhysicalDeviceTransformFeedbackPropertiesEXT.transformFeedbackQueries` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.limits.bufferImageGranularity` | `64` | `65536` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard2DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyAlignedMipSize` | `None` | `False` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyNonResidentStrict` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[0]` | `None` | `VK_SUBGROUP_FEATURE_BALLOT_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[1]` | `None` | `VK_SUBGROUP_FEATURE_BASIC_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[2]` | `None` | `VK_SUBGROUP_FEATURE_VOTE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[3]` | `None` | `VK_SUBGROUP_FEATURE_SHUFFLE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[4]` | `None` | `VK_SUBGROUP_FEATURE_QUAD_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[5]` | `None` | `VK_SUBGROUP_FEATURE_ARITHMETIC_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedStages[0]` | `None` | `VK_SHADER_STAGE_COMPUTE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedStages[1]` | `None` | `VK_SHADER_STAGE_FRAGMENT_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.sparseBinding` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyAliased` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyBuffer` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage2D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.shaderResourceResidency` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.shaderResourceMinLod` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueFlags[0]` | `None` | `VK_QUEUE_SPARSE_BINDING_BIT` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueCount` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_fragment_shader_interlock` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT.fragmentShaderSampleInterlock` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT.fragmentShaderPixelInterlock` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |

### VP_D3D12_FL_12_2_baseline: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRobustness2FeaturesEXT.robustImageAccess2` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `False` | `True` | Hardware-dependent; G615 mask is zero | UNSUPPORTED_NATIVE | Keep false unless transparent BC emulation is complete |
| `features.VkPhysicalDeviceFeatures.pipelineStatisticsQuery` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `properties.VkPhysicalDeviceTransformFeedbackPropertiesEXT.transformFeedbackQueries` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[0]` | `None` | `VK_SUBGROUP_FEATURE_BALLOT_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[1]` | `None` | `VK_SUBGROUP_FEATURE_BASIC_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[2]` | `None` | `VK_SUBGROUP_FEATURE_VOTE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[3]` | `None` | `VK_SUBGROUP_FEATURE_SHUFFLE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[4]` | `None` | `VK_SUBGROUP_FEATURE_QUAD_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[5]` | `None` | `VK_SUBGROUP_FEATURE_ARITHMETIC_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedStages[0]` | `None` | `VK_SHADER_STAGE_COMPUTE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedStages[1]` | `None` | `VK_SHADER_STAGE_FRAGMENT_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.sparseBinding` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyAliased` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyBuffer` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage2D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.shaderResourceResidency` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.shaderResourceMinLod` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueFlags[0]` | `None` | `VK_QUEUE_SPARSE_BINDING_BIT` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueCount` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_fragment_shader_interlock` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT.fragmentShaderSampleInterlock` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT.fragmentShaderPixelInterlock` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_tracing_pipeline` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_acceleration_structure` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_deferred_host_operations` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_query` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_fragment_shading_rate` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_pipeline_library_group_handles` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_tracing_maintenance1` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_mesh_shader` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R32G32_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R32G32B32_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_A2B10G10R10_UNORM_PACK32.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8B8A8_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8B8A8_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipelineTraceRaysIndirect` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTraversalPrimitiveCulling` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayQueryFeaturesKHR.rayQuery` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR.rayTracingMaintenance1` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR.rayTracingPipelineTraceRaysIndirect2` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDevicePipelineLibraryGroupHandlesFeaturesEXT.pipelineLibraryGroupHandles` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.taskShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.meshShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.primitiveFragmentShadingRateMeshShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.pipelineFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.primitiveFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.attachmentFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.depthBounds` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage3D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceVulkan12Features.shaderOutputViewportIndex` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceConservativeRasterizationPropertiesEXT.degenerateTrianglesRasterized` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceConservativeRasterizationPropertiesEXT.fullyCoveredFragmentShaderInputVariable` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceFragmentShadingRatePropertiesKHR.fragmentShadingRateNonTrivialCombinerOps` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueFlags[0]` | `None` | `VK_QUEUE_TRANSFER_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.timestampValidBits` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.limits.bufferImageGranularity` | `64` | `65536` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard2DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard3DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyAlignedMipSize` | `None` | `False` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyNonResidentStrict` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |

### VP_D3D12_FL_12_2_optimal: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRobustness2FeaturesEXT.robustImageAccess2` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `False` | `True` | Hardware-dependent; G615 mask is zero | UNSUPPORTED_NATIVE | Keep false unless transparent BC emulation is complete |
| `features.VkPhysicalDeviceFeatures.pipelineStatisticsQuery` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `properties.VkPhysicalDeviceTransformFeedbackPropertiesEXT.transformFeedbackQueries` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceVulkan13Properties.requiredSubgroupSizeStages[0]` | `None` | `VK_SHADER_STAGE_COMPUTE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.sparseBinding` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyAliased` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyBuffer` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage2D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.shaderResourceResidency` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.shaderResourceMinLod` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueFlags[0]` | `None` | `VK_QUEUE_SPARSE_BINDING_BIT` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueCount` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_fragment_shader_interlock` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT.fragmentShaderSampleInterlock` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT.fragmentShaderPixelInterlock` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_tracing_pipeline` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_acceleration_structure` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_deferred_host_operations` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_query` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_fragment_shading_rate` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_pipeline_library_group_handles` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_tracing_maintenance1` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_mesh_shader` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R32G32_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R32G32B32_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_A2B10G10R10_UNORM_PACK32.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8B8A8_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8B8A8_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipelineTraceRaysIndirect` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTraversalPrimitiveCulling` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayQueryFeaturesKHR.rayQuery` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR.rayTracingMaintenance1` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR.rayTracingPipelineTraceRaysIndirect2` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDevicePipelineLibraryGroupHandlesFeaturesEXT.pipelineLibraryGroupHandles` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.taskShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.meshShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.primitiveFragmentShadingRateMeshShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.pipelineFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.primitiveFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.attachmentFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.depthBounds` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage3D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceVulkan12Features.shaderOutputViewportIndex` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceConservativeRasterizationPropertiesEXT.degenerateTrianglesRasterized` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceConservativeRasterizationPropertiesEXT.fullyCoveredFragmentShaderInputVariable` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceFragmentShadingRatePropertiesKHR.fragmentShadingRateNonTrivialCombinerOps` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueFlags[0]` | `None` | `VK_QUEUE_TRANSFER_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.timestampValidBits` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.limits.bufferImageGranularity` | `64` | `65536` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard2DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard3DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyAlignedMipSize` | `None` | `False` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyNonResidentStrict` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_descriptor_buffer` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_AMD_buffer_marker` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_maintenance10` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceDescriptorBufferFeaturesEXT.descriptorBuffer` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceDescriptorBufferFeaturesEXT.descriptorBufferPushDescriptors` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceExtendedDynamicState2FeaturesEXT.extendedDynamicState2PatchControlPoints` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT.swapchainMaintenance1` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMaintenance10FeaturesKHR.maintenance10` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |

### VP_D3D12_maximum_radv: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRobustness2FeaturesEXT.robustImageAccess2` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `False` | `True` | Hardware-dependent; G615 mask is zero | UNSUPPORTED_NATIVE | Keep false unless transparent BC emulation is complete |
| `features.VkPhysicalDeviceFeatures.pipelineStatisticsQuery` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `properties.VkPhysicalDeviceTransformFeedbackPropertiesEXT.transformFeedbackQueries` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceVulkan13Properties.requiredSubgroupSizeStages[0]` | `None` | `VK_SHADER_STAGE_COMPUTE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.sparseBinding` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyAliased` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyBuffer` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage2D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.shaderResourceResidency` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.shaderResourceMinLod` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueFlags[0]` | `None` | `VK_QUEUE_SPARSE_BINDING_BIT` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueCount` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_tracing_pipeline` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_acceleration_structure` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_deferred_host_operations` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_query` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_fragment_shading_rate` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_pipeline_library_group_handles` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_tracing_maintenance1` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_mesh_shader` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R32G32_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R32G32B32_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_A2B10G10R10_UNORM_PACK32.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8B8A8_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8B8A8_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipelineTraceRaysIndirect` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTraversalPrimitiveCulling` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayQueryFeaturesKHR.rayQuery` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR.rayTracingMaintenance1` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR.rayTracingPipelineTraceRaysIndirect2` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDevicePipelineLibraryGroupHandlesFeaturesEXT.pipelineLibraryGroupHandles` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.taskShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.meshShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.primitiveFragmentShadingRateMeshShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.pipelineFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.primitiveFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.attachmentFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.depthBounds` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage3D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceVulkan12Features.shaderOutputViewportIndex` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceConservativeRasterizationPropertiesEXT.degenerateTrianglesRasterized` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceConservativeRasterizationPropertiesEXT.fullyCoveredFragmentShaderInputVariable` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceFragmentShadingRatePropertiesKHR.fragmentShadingRateNonTrivialCombinerOps` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueFlags[0]` | `None` | `VK_QUEUE_TRANSFER_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.timestampValidBits` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.limits.bufferImageGranularity` | `64` | `65536` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard2DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard3DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyAlignedMipSize` | `None` | `False` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyNonResidentStrict` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_descriptor_buffer` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_AMD_buffer_marker` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_maintenance10` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceDescriptorBufferFeaturesEXT.descriptorBuffer` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceDescriptorBufferFeaturesEXT.descriptorBufferPushDescriptors` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceExtendedDynamicState2FeaturesEXT.extendedDynamicState2PatchControlPoints` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT.swapchainMaintenance1` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMaintenance10FeaturesKHR.maintenance10` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |

### VP_D3D12_maximum_nv: FAIL

| Requirement | Current | Required | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|
| `extensions.VK_EXT_transform_feedback` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRobustness2FeaturesEXT.robustImageAccess2` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.geometryShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.tessellationShader` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.fillModeNonSolid` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.multiViewport` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.textureCompressionBC` | `False` | `True` | Hardware-dependent; G615 mask is zero | UNSUPPORTED_NATIVE | Keep false unless transparent BC emulation is complete |
| `features.VkPhysicalDeviceFeatures.pipelineStatisticsQuery` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderClipDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `features.VkPhysicalDeviceFeatures.shaderCullDistance` | `False` | `True` | ABSENT at pinned and origin/main | NOT_IMPLEMENTED | Design/implement after P5 gates; never expose early |
| `properties.VkPhysicalDeviceTransformFeedbackPropertiesEXT.transformFeedbackQueries` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceVulkan13Properties.requiredSubgroupSizeStages[0]` | `None` | `VK_SHADER_STAGE_COMPUTE_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.sparseBinding` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyAliased` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyBuffer` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage2D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceFeatures.shaderResourceResidency` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.shaderResourceMinLod` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueFlags[0]` | `None` | `VK_QUEUE_SPARSE_BINDING_BIT` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueCount` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_fragment_shader_interlock` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT.fragmentShaderSampleInterlock` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT.fragmentShaderPixelInterlock` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_tracing_pipeline` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_acceleration_structure` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_deferred_host_operations` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_query` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_fragment_shading_rate` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_pipeline_library_group_handles` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_ray_tracing_maintenance1` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_mesh_shader` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R32G32_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R32G32B32_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_SFLOAT.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16B16A16_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R16G16_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_A2B10G10R10_UNORM_PACK32.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8B8A8_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8_UNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8B8A8_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `formats.VK_FORMAT_R8G8_SNORM.VkFormatProperties.bufferFeatures[0]` | `None` | `VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipelineTraceRaysIndirect` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTraversalPrimitiveCulling` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayQueryFeaturesKHR.rayQuery` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR.rayTracingMaintenance1` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR.rayTracingPipelineTraceRaysIndirect2` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDevicePipelineLibraryGroupHandlesFeaturesEXT.pipelineLibraryGroupHandles` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.taskShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.meshShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMeshShaderFeaturesEXT.primitiveFragmentShadingRateMeshShader` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.pipelineFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.primitiveFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFragmentShadingRateFeaturesKHR.attachmentFragmentShadingRate` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceFeatures.depthBounds` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceFeatures.sparseResidencyImage3D` | `False` | `True` | Generic PanVK exists; custom Kbase equivalence unproven | BLOCKED_KBASE | Defer sparse; assess Kbase VM semantics only when required |
| `features.VkPhysicalDeviceVulkan12Features.shaderOutputViewportIndex` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceConservativeRasterizationPropertiesEXT.degenerateTrianglesRasterized` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceConservativeRasterizationPropertiesEXT.fullyCoveredFragmentShaderInputVariable` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceFragmentShadingRatePropertiesKHR.fragmentShadingRateNonTrivialCombinerOps` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.queueFlags[0]` | `None` | `VK_QUEUE_TRANSFER_BIT` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `queueFamiliesProperties[0].VkQueueFamilyProperties.timestampValidBits` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.limits.bufferImageGranularity` | `64` | `65536` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard2DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyStandard3DBlockShape` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyAlignedMipSize` | `None` | `False` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `properties.VkPhysicalDeviceProperties.sparseProperties.residencyNonResidentStrict` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_EXT_descriptor_buffer` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_AMD_buffer_marker` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `extensions.VK_KHR_maintenance10` | `None` | `1` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceDescriptorBufferFeaturesEXT.descriptorBuffer` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceDescriptorBufferFeaturesEXT.descriptorBufferPushDescriptors` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceExtendedDynamicState2FeaturesEXT.extendedDynamicState2PatchControlPoints` | `False` | `True` | Pinned runtime does not satisfy; no post-pin matching backport found | NOT_SATISFIED | Investigate in the owning feature phase |
| `features.VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT.swapchainMaintenance1` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |
| `features.VkPhysicalDeviceMaintenance10FeaturesKHR.maintenance10` | `None` | `True` | UNKNOWN | UNKNOWN | Extend capture or implementation audit before claiming support |

