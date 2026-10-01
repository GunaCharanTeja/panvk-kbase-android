# P21 D3D12 feature/profile analysis

## Result

`FAIL`

Machine-evaluated HARD_REQUIREMENTS, PROFILE_BASELINE, DEVICE_CREATE, and FEATURE_LEVEL only.
These four gates stay separate. No merged PASS. No PanVK patch. No feature-bit change.
No sparse implementation. P22 was not started.

```text
VKD3D_2_0_HARD=PASS
VKD3D_2_0_BASELINE=NOT_APPLICABLE
VKD3D_2_0_DEVICE=PASS
VKD3D_2_0_FEATURE_LEVEL=NOT_AVAILABLE
VKD3D_2_0_IMPLIED=D3D_FEATURE_LEVEL_11_0
VKD3D_2_0_SMOKE=BLOCKED
VKD3D_2_14_1_HARD=PASS
VKD3D_2_14_1_BASELINE=FAIL
VKD3D_2_14_1_DEVICE=FAIL
VKD3D_2_14_1_FEATURE_LEVEL=NOT_AVAILABLE
VKD3D_2_14_1_IMPLIED=D3D_FEATURE_LEVEL_11_0
VKD3D_2_14_1_SMOKE=BLOCKED
VKD3D_3_0_1_HARD=PASS
VKD3D_3_0_1_BASELINE=FAIL
VKD3D_3_0_1_DEVICE=FAIL
VKD3D_3_0_1_FEATURE_LEVEL=NOT_AVAILABLE
VKD3D_3_0_1_IMPLIED=D3D_FEATURE_LEVEL_11_0
VKD3D_3_0_1_SMOKE=BLOCKED
```

## Adapter

- Device: `Mali-G615 MC6`
- Target: `Poco X6 Pro / duchamp`
- VendorID: `5045`
- DeviceID: `0xb8a31030`
- Driver: `panvk` / `Mesa 26.3.0-devel (git-5a07217f03)`
- DriverID: `20` (`VK_DRIVER_ID_MESA_PANVK`)
- DriverVersion packed: `109060195` (`0x06802063`)
- VK_VERSION decode: `26.2.99` (packed git encoding; `driverInfo` is authoritative)
- Vulkan API: `1.4.363`
- ICD SHA-256: `576e37de9a3b60dda9a972a6f91c3a50e09238bd31a9888e04215c7b554c803a`
- Capture: `direct vk_icdGetInstanceProcAddr probe` at `2026-09-19T20:12:31.551173+00:00`

## Gate separation

HARD_REQUIREMENTS comes from each tag's README. PROFILE_BASELINE comes from
`VP_D3D12_FL_11_0_baseline` in `VP_D3D12_VKD3D_PROTON_profile.json` for 2.14.1 and 3.0.1.
vkd3d-proton 2.0 has no official profile, so PROFILE_BASELINE is `NOT_APPLICABLE`.
DEVICE_CREATE comes from `vkd3d_init_device_caps` E_INVALIDARG gates in
`libs/vkd3d/device.c`. FEATURE_LEVEL is the D3D12 feature level that stock vkd3d
would report after a successful device create. Implied FL is a separate machine
evaluation of `d3d12_device_caps_init_feature_level` and must not be treated as
stock `CheckFeatureSupport`.

README HARD can PASS while DEVICE_CREATE FAILs. PROFILE_BASELINE is broader than
either gate. Implied FL 11_0 is the vkd3d default after a successful create; G615
cannot climb because `vertexPipelineStoresAndAtomics` is false (FL 11_1) and sparse
tiled resources are false (FL 12_0). P22 sparse/Kbase was not started.

## Initialization / stock vkd3d smoke

Stock vkd3d-proton tags `v2.0`, `v2.14.1`, and `v3.0.1` are Windows D3D12-on-Vulkan
layers. This Android ICD is not a legal host. Smoke is `BLOCKED` for all three.
v2.14.1 and v3.0.1 would also fail `vkd3d_init_device_caps` on `robustImageAccess2`
and `transformFeedbackQueries`. No game boot, no vkd3d init log, no reported D3D
feature level from stock `CheckFeatureSupport`.

P6 left `robustImageAccess2` false. P9-P17 left geometry, tessellation, XFB, BC,
clip/cull, fillModeNonSolid, and multiViewport false. P20 left
`pipelineStatisticsQuery` false. P21 did not flip them and did not implement sparse.

## vkd3d-proton 2.0

HARD_REQUIREMENTS: **PASS**. PROFILE_BASELINE: **NOT_APPLICABLE**. DEVICE_CREATE: **PASS**. FEATURE_LEVEL: **NOT_AVAILABLE**. Implied: **D3D_FEATURE_LEVEL_11_0**. Smoke: **BLOCKED**.

Source: `https://github.com/HansKristian-Work/vkd3d-proton/blob/v2.0/README.md` (`validation/requirements/vkd3d-proton-2.0.json`).

### HARD_REQUIREMENTS

Source: `v2.0 README.md:17-24 README.md hard requirements (Vulkan 1.1, VK_EXT_descriptor_indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, VK_KHR_timeline_semaphore)`

vkd3d-proton 2.0 has no VP_D3D12_VKD3D_PROTON_profile.json. HARD_REQUIREMENTS come from the tagged README. Recommended extensions are not this HARD gate. robustImageAccess2 is not a 2.0 DEVICE_CREATE check.

Pass `12` / fail `0`.

| Requirement | Current | Required | Status | Source version | Source file/profile | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|---|---|---|
| `apiVersion>=1.1` | `1.4.363` | `1.1` | PASS | `2.0` | `v2.0 README.md:17-24 README.md hard requirements (Vulkan 1.1, VK_EXT_descriptor_indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, VK_KHR_timeline_semaphore)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `descriptorIndexing` | `True` | `True` | PASS | `2.0` | `v2.0 README.md:17-24 README.md hard requirements (Vulkan 1.1, VK_EXT_descriptor_indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, VK_KHR_timeline_semaphore)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VkPhysicalDeviceDescriptorIndexingFeatures except UniformBuffer UAB` | `True` | `True` | PASS | `2.0` | `v2.0 README.md:17-24 README.md hard requirements (Vulkan 1.1, VK_EXT_descriptor_indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, VK_KHR_timeline_semaphore)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxDescriptorSetUpdateAfterBindStorageBuffers` | `1048576` | `1000000` | PASS | `2.0` | `v2.0 README.md:17-24 README.md hard requirements (Vulkan 1.1, VK_EXT_descriptor_indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, VK_KHR_timeline_semaphore)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxDescriptorSetUpdateAfterBindSampledImages` | `1048576` | `1000000` | PASS | `2.0` | `v2.0 README.md:17-24 README.md hard requirements (Vulkan 1.1, VK_EXT_descriptor_indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, VK_KHR_timeline_semaphore)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxDescriptorSetUpdateAfterBindStorageImages` | `1048576` | `1000000` | PASS | `2.0` | `v2.0 README.md:17-24 README.md hard requirements (Vulkan 1.1, VK_EXT_descriptor_indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, VK_KHR_timeline_semaphore)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxDescriptorSetUpdateAfterBindSamplers` | `1048576` | `1000000` | PASS | `2.0` | `v2.0 README.md:17-24 README.md hard requirements (Vulkan 1.1, VK_EXT_descriptor_indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, VK_KHR_timeline_semaphore)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxPerStageDescriptorUpdateAfterBindStorageBuffers` | `1048576` | `1000000` | PASS | `2.0` | `v2.0 README.md:17-24 README.md hard requirements (Vulkan 1.1, VK_EXT_descriptor_indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, VK_KHR_timeline_semaphore)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxPerStageDescriptorUpdateAfterBindSampledImages` | `1048576` | `1000000` | PASS | `2.0` | `v2.0 README.md:17-24 README.md hard requirements (Vulkan 1.1, VK_EXT_descriptor_indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, VK_KHR_timeline_semaphore)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxPerStageDescriptorUpdateAfterBindStorageImages` | `1048576` | `1000000` | PASS | `2.0` | `v2.0 README.md:17-24 README.md hard requirements (Vulkan 1.1, VK_EXT_descriptor_indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, VK_KHR_timeline_semaphore)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxPerStageDescriptorUpdateAfterBindSamplers` | `1048576` | `1000000` | PASS | `2.0` | `v2.0 README.md:17-24 README.md hard requirements (Vulkan 1.1, VK_EXT_descriptor_indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, VK_KHR_timeline_semaphore)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_KHR_timeline_semaphore` | `2` | `1` | PASS | `2.0` | `v2.0 README.md:17-24 README.md hard requirements (Vulkan 1.1, VK_EXT_descriptor_indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, VK_KHR_timeline_semaphore)` | Present in pinned G615 capture | IMPLEMENTED | None |

Highly recommended, not mandatory:

| Requirement | Current | Required | Status | Source version | Source file/profile | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|---|---|---|
| `VK_EXT_robustness2` | `1` | `1` | PASS | `2.0` | `v2.0 README.md:26-31 README.md recommended, not mandatory` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_KHR_buffer_device_address` | `1` | `1` | PASS | `2.0` | `v2.0 README.md:26-31 README.md recommended, not mandatory` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_EXT_extended_dynamic_state` | `1` | `1` | PASS | `2.0` | `v2.0 README.md:26-31 README.md recommended, not mandatory` | Present in pinned G615 capture | IMPLEMENTED | None |

### PROFILE_BASELINE

v2.0 has no official Vulkan profile; PROFILE_BASELINE is not a 2.0 gate

### DEVICE_CREATE

Source: `v2.0 libs/vkd3d/device.c vkd3d_init_device_caps (KHR_timeline_semaphore is the only E_INVALIDARG hard gate)`

Stock vkd3d-proton v2.0 is a Windows D3D12-on-Vulkan layer. This Android ICD is not a legal host for the tagged binaries. No stock vkd3d binary was launched. vkd3d_init_device_caps would not return E_INVALIDARG on the captured timeline semaphore bit, but that is not a stock smoke PASS.

| Requirement | Current | Required | Status | Source version | Source file/profile | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|---|---|---|
| `apiVersion>=1.1` | `1.4.363` | `1.1` | PASS | `2.0` | `v2.0 libs/vkd3d/device.c vkd3d_init_device_caps (KHR_timeline_semaphore is the only E_INVALIDARG hard gate)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_KHR_timeline_semaphore` | `2` | `1` | PASS | `2.0` | `v2.0 libs/vkd3d/device.c vkd3d_init_device_caps (KHR_timeline_semaphore is the only E_INVALIDARG hard gate)` | Present in pinned G615 capture | IMPLEMENTED | None |

### FEATURE_LEVEL

DEVICE_CREATE would not return E_INVALIDARG on captured 2.0 timeline-semaphore bits, but stock vkd3d still cannot legally run on this Android ICD. Stock D3D_FEATURE_LEVEL is not available because stock vkd3d never reaches CheckFeatureSupport on this ICD. The implied level below is a machine evaluation of d3d12_device_caps_init_feature_level against advertised Vulkan caps. It is not a stock CheckFeatureSupport result and must not be merged with HARD_REQUIREMENTS, PROFILE_BASELINE, or DEVICE_CREATE.

Stock `CheckFeatureSupport` D3D_FEATURE_LEVEL: `NOT_AVAILABLE`.
Implied `d3d12_device_caps_init_feature_level`: `D3D_FEATURE_LEVEL_11_0`.
Tiled resources tier: `D3D12_TILED_RESOURCES_TIER_NOT_SUPPORTED`.

FL11_0 default: `DEFAULT`. FL11_1: `FAIL`. FL12_0: `FAIL`. FL12_1: `FAIL`. FL12_2: `FAIL`.

| Requirement | Current | Required | Status | Source version | Source file/profile | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|---|---|---|
| `vertexPipelineStoresAndAtomics` | `False` | `True` | FAIL | `2.0` | `v2.0 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL11_1` | Pinned G615 reports false. vkd3d d3d12_device_caps_init_feature_level needs it for D3D_FEATURE_LEVEL_11_1. | NOT_IMPLEMENTED | Keep false. Do not expose until VS/GS UAV stores and atomics are proven. Blocks implied FL 11_1. |
| `sparseBinding` | `False` | `True` | FAIL | `2.0` | `v2.0 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_0 tiled tier>=2` | Pinned G615 reports false. Upstream PanVK sparse is pan-kmod/Panthor, not this Kbase backend. P22 not started. | NOT_IMPLEMENTED | Keep false. P22 Kbase sparse feasibility first. Do not jump to sparse implementation. |
| `sparseResidencyBuffer` | `False` | `True` | FAIL | `2.0` | `v2.0 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_0 tiled tier>=2` | Pinned G615 reports false. Depends on sparseBinding plus Kbase VM map/unmap. P22 not started. | NOT_IMPLEMENTED | Keep false. P22 Kbase sparse feasibility first. Do not jump to sparse implementation. |
| `sparseResidencyImage2D` | `False` | `True` | FAIL | `2.0` | `v2.0 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_0 tiled tier>=2` | Pinned G615 reports false. Depends on sparseBinding plus image sparse bind. P22 not started. | NOT_IMPLEMENTED | Keep false. P22 Kbase sparse feasibility first. Do not jump to sparse implementation. |
| `sparseResidencyAliased` | `False` | `True` | FAIL | `2.0` | `v2.0 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_0 tiled tier>=2` | Pinned G615 reports false. Depends on sparse alias mappings. P22 not started. | NOT_IMPLEMENTED | Keep false. P22 Kbase sparse feasibility first. Do not jump to sparse implementation. |
| `shaderResourceResidency` | `False` | `True` | FAIL | `2.0` | `v2.0 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_0 tiled tier>=2` | Pinned G615 reports false. Needed for D3D12 tiled-resource residency queries. P22 not started. | NOT_IMPLEMENTED | Keep false. P22 Kbase sparse feasibility first. Do not jump to sparse implementation. |
| `shaderResourceMinLod` | `False` | `True` | FAIL | `2.0` | `v2.0 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_0 tiled tier>=2` | Pinned G615 reports false. Needed for D3D12 tiled-resource min LOD. P22 not started. | NOT_IMPLEMENTED | Keep false. P22 Kbase sparse feasibility first. Do not jump to sparse implementation. |
| `VK_QUEUE_SPARSE_BINDING_BIT` | `False` | `True` | FAIL | `2.0` | `v2.0 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_0 tiled queue` | Capture has no sparse-binding queue family. P22 not started. | NOT_IMPLEMENTED | Keep absent. P22 Kbase sparse feasibility first. Do not jump to sparse implementation. |
| `VK_EXT_fragment_shader_interlock` | `None` | `1` | FAIL | `2.0` | `v2.0 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_1 ROVs` | Not advertised. vkd3d ROVsSupported needs fragmentShaderPixelInterlock and fragmentShaderSampleInterlock. | NOT_IMPLEMENTED | Keep absent. Needed only for implied FL 12_1 ROVs after FL 12_0. |
| `VK_EXT_mesh_shader` | `None` | `1` | FAIL | `2.0` | `v2.0 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_2 MeshShaderTier` | Not advertised. vkd3d FL 12_2 needs MeshShaderTier. | NOT_IMPLEMENTED | Keep absent. Higher than first D3D12 device-create gate. |
| `VK_KHR_ray_tracing_pipeline` | `None` | `1` | FAIL | `2.0` | `v2.0 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_2 RaytracingTier` | Not advertised. vkd3d FL 12_2 needs RaytracingTier 1.1. | NOT_IMPLEMENTED | Keep absent. Higher than first D3D12 device-create gate. |
| `VK_KHR_fragment_shading_rate` | `None` | `1` | FAIL | `2.0` | `v2.0 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_2 VariableShadingRateTier` | Not advertised. vkd3d FL 12_2 needs VariableShadingRateTier 2. | NOT_IMPLEMENTED | Keep absent. Higher than first D3D12 device-create gate. |

## vkd3d-proton 2.14.1

HARD_REQUIREMENTS: **PASS**. PROFILE_BASELINE: **FAIL**. DEVICE_CREATE: **FAIL**. FEATURE_LEVEL: **NOT_AVAILABLE**. Implied: **D3D_FEATURE_LEVEL_11_0**. Smoke: **BLOCKED**.

Source: `https://raw.githubusercontent.com/HansKristian-Work/vkd3d-proton/v2.14.1/VP_D3D12_VKD3D_PROTON_profile.json` (`validation/g615-v11-csf/profiles/vkd3d-2.14.1.json`).

### HARD_REQUIREMENTS

Source: `v2.14.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)`

README lists VK_EXT_robustness2 as an extension. robustImageAccess2 is a DEVICE_CREATE check in vkd3d_init_device_caps, not this HARD gate.

Pass `15` / fail `0`.

| Requirement | Current | Required | Status | Source version | Source file/profile | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|---|---|---|
| `apiVersion>=1.3` | `1.4.363` | `1.3` | PASS | `2.14.1` | `v2.14.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `descriptorIndexing` | `True` | `True` | PASS | `2.14.1` | `v2.14.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VkPhysicalDeviceDescriptorIndexingFeatures except UniformBuffer UAB` | `True` | `True` | PASS | `2.14.1` | `v2.14.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxDescriptorSetUpdateAfterBindStorageBuffers` | `1048576` | `1000000` | PASS | `2.14.1` | `v2.14.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxDescriptorSetUpdateAfterBindSampledImages` | `1048576` | `1000000` | PASS | `2.14.1` | `v2.14.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxDescriptorSetUpdateAfterBindStorageImages` | `1048576` | `1000000` | PASS | `2.14.1` | `v2.14.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxDescriptorSetUpdateAfterBindSamplers` | `1048576` | `1000000` | PASS | `2.14.1` | `v2.14.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxPerStageDescriptorUpdateAfterBindStorageBuffers` | `1048576` | `1000000` | PASS | `2.14.1` | `v2.14.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxPerStageDescriptorUpdateAfterBindSampledImages` | `1048576` | `1000000` | PASS | `2.14.1` | `v2.14.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxPerStageDescriptorUpdateAfterBindStorageImages` | `1048576` | `1000000` | PASS | `2.14.1` | `v2.14.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxPerStageDescriptorUpdateAfterBindSamplers` | `1048576` | `1000000` | PASS | `2.14.1` | `v2.14.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `samplerMirrorClampToEdge` | `True` | `True` | PASS | `2.14.1` | `v2.14.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `shaderDrawParameters` | `True` | `True` | PASS | `2.14.1` | `v2.14.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_EXT_robustness2` | `1` | `1` | PASS | `2.14.1` | `v2.14.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_KHR_push_descriptor` | `2` | `1` | PASS | `2.14.1` | `v2.14.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |

Highly recommended, not mandatory:

| Requirement | Current | Required | Status | Source version | Source file/profile | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|---|---|---|
| `VK_EXT_image_view_min_lod` | `1` | `1` | PASS | `2.14.1` | `v2.14.1 README.md highly recommended, not mandatory` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_EXT_mutable_descriptor_type` | `1` | `1` | PASS | `2.14.1` | `v2.14.1 README.md highly recommended, not mandatory` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_VALVE_mutable_descriptor_type` | `1` | `1` | PASS | `2.14.1` | `v2.14.1 README.md highly recommended, not mandatory` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_EXT_descriptor_buffer` | `None` | `1` | FAIL | `2.14.1` | `v2.14.1 README.md highly recommended, not mandatory` | Not advertised. README marks it highly recommended, not mandatory. | NOT_IMPLEMENTED | Do not make descriptor_buffer mandatory for the first compliance gate. |

### PROFILE_BASELINE

`VP_D3D12_FL_11_0_baseline` (`Minimum baseline (FL 11.0)`) is the official minimum D3D12 baseline.

Pass `78` / fail `15`.

| Requirement | Current | Required | Status | Source version | Source file/profile | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|---|---|---|
| `VK_EXT_transform_feedback` | `0` | `1` | FAIL | `2.14.1` | `v2.14.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / extensions.VK_EXT_transform_feedback` | ABSENT at pinned 5a07217 and origin/main e1f3f372; PanVK get_device_extensions omits it (P15 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep absent. Complete no-IDVS XFB variant, then CTS. Never expose early. |
| `robustImageAccess2` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceRobustness2FeaturesEXT.robustImageAccess2` | Pinned and origin/main e1f3f372 still report false (P6 PARTIAL / UNSUPPORTED) | UNSUPPORTED | Keep false. Complete robustImageAccess2 semantics, then CTS. Never expose early. |
| `transformFeedback` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | ABSENT at pinned 5a07217 and origin/main e1f3f372; PanVK omits EXT_transform_feedback (P15 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Complete no-IDVS XFB variant, then CTS. Never expose early. |
| `geometryStreams` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | ABSENT at pinned and origin/main; XFB streams need GS (P15 BLOCKED_SAFE_FALSE, waits on P12) | NOT_IMPLEMENTED | Keep false. Complete XFB plus geometryShader, then CTS. Never expose early. |
| `geometryShader` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceFeatures.geometryShader` | ABSENT at pinned 5a07217 and origin/main e1f3f372; v11 XML has no GS stage (P12 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Complete Valhall v11 GS or PanVK poly/compute lowering, then CTS. Never expose early. |
| `tessellationShader` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceFeatures.tessellationShader` | ABSENT at pinned 5a07217 and origin/main e1f3f372; v11 XML has no TCS/TES stage (P17 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Complete Valhall v11 TCS/TES or PanVK poly/compute lowering, then CTS. Never expose early. |
| `fillModeNonSolid` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceFeatures.fillModeNonSolid` | ABSENT at pinned and origin/main; no Valhall polygon-mode field (P11 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Post-vertex primitive expansion, then CTS. Never expose early. |
| `multiViewport` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceFeatures.multiViewport` | ABSENT at pinned and origin/main; v11 has no viewport array (P14 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Hardware proof of v11 ViewportIndex plus per-viewport transform/scissor/depth, then CTS. Never expose early. |
| `textureCompressionBC` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceFeatures.textureCompressionBC` | Hardware-dependent; G615 BC mask is 0. No complete PanVK emulation (P9 BLOCKED_SAFE_FALSE) | UNSUPPORTED_NATIVE | Keep false until transparent BC contract PASSes. Never advertise early. |
| `pipelineStatisticsQuery` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceFeatures.pipelineStatisticsQuery` | ABSENT at pinned 5a07217 and origin/main e1f3f372; P20 BLOCKED_SAFE_FALSE; CreateQueryPool UNREACHABLE for PIPELINE_STATISTICS | NOT_IMPLEMENTED | Keep false. Complete pipeline-statistics query path, then CTS. Never expose early. |
| `shaderClipDistance` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceFeatures.shaderClipDistance` | ABSENT at pinned and origin/main; no PanVK clip lowering (P10 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Primitive-stage clip, then pixel matrix and CTS. Never expose early. |
| `shaderCullDistance` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceFeatures.shaderCullDistance` | ABSENT at pinned and origin/main; no PanVK cull lowering (P10 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Primitive-stage cull, then pixel matrix and CTS. Never expose early. |
| `transformFeedbackQueries` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / properties.VkPhysicalDeviceTransformFeedbackPropertiesEXT.transformFeedbackQueries` | ABSENT at pinned and origin/main; XFB queries need EXT_transform_feedback (P15 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Complete XFB queries after transformFeedback, then CTS. Never expose early. Also a 2.14.1/3.0.1 DEVICE_CREATE E_INVALIDARG. |
| `bufferImageGranularity` | `64` | `65536` | FAIL | `2.14.1` | `v2.14.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / fl_11_0_properties / properties.VkPhysicalDeviceProperties.limits.bufferImageGranularity` | G615 reports 64. Profile lists 65536 as a numeric property. Smaller granularity is more capable. | EVALUATOR_DIRECTION | No driver change. Existing evaluator uses >=; actual 64 already satisfies a 65536 max-granularity. |
| `subgroupSupportedOperations[0]` | `None` | `VK_SUBGROUP_FEATURE_BASIC_BIT` | FAIL | `2.14.1` | `v2.14.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / subgroups_none / properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[0]` | G615 subgroupSupportedOperations=1791 includes VK_SUBGROUP_FEATURE_BASIC_BIT. Capture stores a bitmask, not a string array. | EVALUATOR_ARRAY | No driver change. Evaluator compares flattened array string against integer bitmask. |

### DEVICE_CREATE

Source: `v2.14.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)`

Stock vkd3d-proton v2.14.1 vkd3d_init_device_caps returns E_INVALIDARG when robustImageAccess2 or transformFeedbackQueries is false. G615 reports both false. Stock vkd3d-proton is a Windows D3D12-on-Vulkan layer; this Android ICD is not a legal host for the tagged binaries. No stock vkd3d binary was launched.

| Requirement | Current | Required | Status | Source version | Source file/profile | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|---|---|---|
| `apiVersion>=1.3` | `1.4.363` | `1.3` | PASS | `2.14.1` | `v2.14.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_EXT_vertex_attribute_divisor` | `3` | `3` | PASS | `2.14.1` | `v2.14.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `transformFeedbackQueries` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | ABSENT at pinned and origin/main; XFB queries need EXT_transform_feedback (P15 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Complete XFB queries after transformFeedback, then CTS. Never expose early. Also a 2.14.1/3.0.1 DEVICE_CREATE E_INVALIDARG. |
| `storageTexelBufferOffsetSingleTexelAlignment` | `True` | `True` | PASS | `2.14.1` | `v2.14.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `uniformTexelBufferOffsetSingleTexelAlignment` | `True` | `True` | PASS | `2.14.1` | `v2.14.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `samplerMirrorClampToEdge` | `True` | `True` | PASS | `2.14.1` | `v2.14.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `shaderDrawParameters` | `True` | `True` | PASS | `2.14.1` | `v2.14.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_EXT_robustness2` | `1` | `1` | PASS | `2.14.1` | `v2.14.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `robustBufferAccess2` | `True` | `True` | PASS | `2.14.1` | `v2.14.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `robustImageAccess2` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Pinned and origin/main e1f3f372 still report false (P6 PARTIAL / UNSUPPORTED) | UNSUPPORTED | Keep false. Complete robustImageAccess2 semantics, then CTS. Never expose early. |
| `nullDescriptor` | `True` | `True` | PASS | `2.14.1` | `v2.14.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_KHR_push_descriptor` | `2` | `1` | PASS | `2.14.1` | `v2.14.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |

### FEATURE_LEVEL

D3D_FEATURE_LEVEL is not available because DEVICE_CREATE fails. Stock D3D_FEATURE_LEVEL is not available because stock vkd3d never reaches CheckFeatureSupport on this ICD. The implied level below is a machine evaluation of d3d12_device_caps_init_feature_level against advertised Vulkan caps. It is not a stock CheckFeatureSupport result and must not be merged with HARD_REQUIREMENTS, PROFILE_BASELINE, or DEVICE_CREATE.

Stock `CheckFeatureSupport` D3D_FEATURE_LEVEL: `NOT_AVAILABLE`.
Implied `d3d12_device_caps_init_feature_level`: `D3D_FEATURE_LEVEL_11_0`.
Tiled resources tier: `D3D12_TILED_RESOURCES_TIER_NOT_SUPPORTED`.

FL11_0 default: `DEFAULT`. FL11_1: `FAIL`. FL12_0: `FAIL`. FL12_1: `FAIL`. FL12_2: `FAIL`.

| Requirement | Current | Required | Status | Source version | Source file/profile | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|---|---|---|
| `vertexPipelineStoresAndAtomics` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL11_1` | Pinned G615 reports false. vkd3d d3d12_device_caps_init_feature_level needs it for D3D_FEATURE_LEVEL_11_1. | NOT_IMPLEMENTED | Keep false. Do not expose until VS/GS UAV stores and atomics are proven. Blocks implied FL 11_1. |
| `sparseBinding` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_0 tiled tier>=2` | Pinned G615 reports false. Upstream PanVK sparse is pan-kmod/Panthor, not this Kbase backend. P22 not started. | NOT_IMPLEMENTED | Keep false. P22 Kbase sparse feasibility first. Do not jump to sparse implementation. |
| `sparseResidencyBuffer` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_0 tiled tier>=2` | Pinned G615 reports false. Depends on sparseBinding plus Kbase VM map/unmap. P22 not started. | NOT_IMPLEMENTED | Keep false. P22 Kbase sparse feasibility first. Do not jump to sparse implementation. |
| `sparseResidencyImage2D` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_0 tiled tier>=2` | Pinned G615 reports false. Depends on sparseBinding plus image sparse bind. P22 not started. | NOT_IMPLEMENTED | Keep false. P22 Kbase sparse feasibility first. Do not jump to sparse implementation. |
| `sparseResidencyAliased` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_0 tiled tier>=2` | Pinned G615 reports false. Depends on sparse alias mappings. P22 not started. | NOT_IMPLEMENTED | Keep false. P22 Kbase sparse feasibility first. Do not jump to sparse implementation. |
| `shaderResourceResidency` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_0 tiled tier>=2` | Pinned G615 reports false. Needed for D3D12 tiled-resource residency queries. P22 not started. | NOT_IMPLEMENTED | Keep false. P22 Kbase sparse feasibility first. Do not jump to sparse implementation. |
| `shaderResourceMinLod` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_0 tiled tier>=2` | Pinned G615 reports false. Needed for D3D12 tiled-resource min LOD. P22 not started. | NOT_IMPLEMENTED | Keep false. P22 Kbase sparse feasibility first. Do not jump to sparse implementation. |
| `VK_QUEUE_SPARSE_BINDING_BIT` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_0 tiled queue` | Capture has no sparse-binding queue family. P22 not started. | NOT_IMPLEMENTED | Keep absent. P22 Kbase sparse feasibility first. Do not jump to sparse implementation. |
| `VK_EXT_fragment_shader_interlock` | `None` | `1` | FAIL | `2.14.1` | `v2.14.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_1 ROVs` | Not advertised. vkd3d ROVsSupported needs fragmentShaderPixelInterlock and fragmentShaderSampleInterlock. | NOT_IMPLEMENTED | Keep absent. Needed only for implied FL 12_1 ROVs after FL 12_0. |
| `VK_EXT_mesh_shader` | `None` | `1` | FAIL | `2.14.1` | `v2.14.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_2 MeshShaderTier` | Not advertised. vkd3d FL 12_2 needs MeshShaderTier. | NOT_IMPLEMENTED | Keep absent. Higher than first D3D12 device-create gate. |
| `VK_KHR_ray_tracing_pipeline` | `None` | `1` | FAIL | `2.14.1` | `v2.14.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_2 RaytracingTier` | Not advertised. vkd3d FL 12_2 needs RaytracingTier 1.1. | NOT_IMPLEMENTED | Keep absent. Higher than first D3D12 device-create gate. |
| `VK_KHR_fragment_shading_rate` | `None` | `1` | FAIL | `2.14.1` | `v2.14.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_2 VariableShadingRateTier` | Not advertised. vkd3d FL 12_2 needs VariableShadingRateTier 2. | NOT_IMPLEMENTED | Keep absent. Higher than first D3D12 device-create gate. |

Official FL profiles remain FAIL and are not a D3D feature level:

| Profile | Status | Pass | Fail |
|---|---|---:|---:|
| `VP_D3D12_FL_11_0_baseline` | FAIL | 78 | 15 |
| `VP_D3D12_FL_11_1_baseline` | FAIL | 79 | 16 |
| `VP_D3D12_FL_12_0_baseline` | FAIL | 83 | 31 |
| `VP_D3D12_FL_12_1_baseline` | FAIL | 84 | 34 |
| `VP_D3D12_FL_12_2_baseline` | FAIL | 88 | 65 |

Vendor-maximum profiles `VP_D3D12_maximum_radv` and `VP_D3D12_maximum_nv` are not D3D feature levels.

## vkd3d-proton 3.0.1

HARD_REQUIREMENTS: **PASS**. PROFILE_BASELINE: **FAIL**. DEVICE_CREATE: **FAIL**. FEATURE_LEVEL: **NOT_AVAILABLE**. Implied: **D3D_FEATURE_LEVEL_11_0**. Smoke: **BLOCKED**.

Source: `https://raw.githubusercontent.com/HansKristian-Work/vkd3d-proton/v3.0.1/VP_D3D12_VKD3D_PROTON_profile.json` (`validation/g615-v11-csf/profiles/vkd3d-3.0.1.json`).

### HARD_REQUIREMENTS

Source: `v3.0.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)`

README lists VK_EXT_robustness2 as an extension. robustImageAccess2 is a DEVICE_CREATE check in vkd3d_init_device_caps, not this HARD gate.

Pass `15` / fail `0`.

| Requirement | Current | Required | Status | Source version | Source file/profile | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|---|---|---|
| `apiVersion>=1.3` | `1.4.363` | `1.3` | PASS | `3.0.1` | `v3.0.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `descriptorIndexing` | `True` | `True` | PASS | `3.0.1` | `v3.0.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VkPhysicalDeviceDescriptorIndexingFeatures except UniformBuffer UAB` | `True` | `True` | PASS | `3.0.1` | `v3.0.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxDescriptorSetUpdateAfterBindStorageBuffers` | `1048576` | `1000000` | PASS | `3.0.1` | `v3.0.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxDescriptorSetUpdateAfterBindSampledImages` | `1048576` | `1000000` | PASS | `3.0.1` | `v3.0.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxDescriptorSetUpdateAfterBindStorageImages` | `1048576` | `1000000` | PASS | `3.0.1` | `v3.0.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxDescriptorSetUpdateAfterBindSamplers` | `1048576` | `1000000` | PASS | `3.0.1` | `v3.0.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxPerStageDescriptorUpdateAfterBindStorageBuffers` | `1048576` | `1000000` | PASS | `3.0.1` | `v3.0.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxPerStageDescriptorUpdateAfterBindSampledImages` | `1048576` | `1000000` | PASS | `3.0.1` | `v3.0.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxPerStageDescriptorUpdateAfterBindStorageImages` | `1048576` | `1000000` | PASS | `3.0.1` | `v3.0.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `maxPerStageDescriptorUpdateAfterBindSamplers` | `1048576` | `1000000` | PASS | `3.0.1` | `v3.0.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `samplerMirrorClampToEdge` | `True` | `True` | PASS | `3.0.1` | `v3.0.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `shaderDrawParameters` | `True` | `True` | PASS | `3.0.1` | `v3.0.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_EXT_robustness2` | `1` | `1` | PASS | `3.0.1` | `v3.0.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_KHR_push_descriptor` | `2` | `1` | PASS | `3.0.1` | `v3.0.1 README.md hard requirements (Vulkan 1.3, descriptor indexing, 1_000_000 UpdateAfterBind except UniformBuffer, VkPhysicalDeviceDescriptorIndexingFeatures, samplerMirrorClampToEdge, shaderDrawParameters, VK_EXT_robustness2, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |

Highly recommended, not mandatory:

| Requirement | Current | Required | Status | Source version | Source file/profile | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|---|---|---|
| `VK_EXT_image_view_min_lod` | `1` | `1` | PASS | `3.0.1` | `v3.0.1 README.md highly recommended, not mandatory` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_EXT_mutable_descriptor_type` | `1` | `1` | PASS | `3.0.1` | `v3.0.1 README.md highly recommended, not mandatory` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_VALVE_mutable_descriptor_type` | `1` | `1` | PASS | `3.0.1` | `v3.0.1 README.md highly recommended, not mandatory` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_EXT_descriptor_buffer` | `None` | `1` | FAIL | `3.0.1` | `v3.0.1 README.md highly recommended, not mandatory` | Not advertised. README marks it highly recommended, not mandatory. | NOT_IMPLEMENTED | Do not make descriptor_buffer mandatory for the first compliance gate. |

### PROFILE_BASELINE

`VP_D3D12_FL_11_0_baseline` (`Minimum baseline (FL 11.0)`) is the official minimum D3D12 baseline.

Pass `80` / fail `15`.

| Requirement | Current | Required | Status | Source version | Source file/profile | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|---|---|---|
| `VK_EXT_transform_feedback` | `0` | `1` | FAIL | `3.0.1` | `v3.0.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / extensions.VK_EXT_transform_feedback` | ABSENT at pinned 5a07217 and origin/main e1f3f372; PanVK get_device_extensions omits it (P15 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep absent. Complete no-IDVS XFB variant, then CTS. Never expose early. |
| `robustImageAccess2` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceRobustness2FeaturesEXT.robustImageAccess2` | Pinned and origin/main e1f3f372 still report false (P6 PARTIAL / UNSUPPORTED) | UNSUPPORTED | Keep false. Complete robustImageAccess2 semantics, then CTS. Never expose early. |
| `transformFeedback` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.transformFeedback` | ABSENT at pinned 5a07217 and origin/main e1f3f372; PanVK omits EXT_transform_feedback (P15 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Complete no-IDVS XFB variant, then CTS. Never expose early. |
| `geometryStreams` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceTransformFeedbackFeaturesEXT.geometryStreams` | ABSENT at pinned and origin/main; XFB streams need GS (P15 BLOCKED_SAFE_FALSE, waits on P12) | NOT_IMPLEMENTED | Keep false. Complete XFB plus geometryShader, then CTS. Never expose early. |
| `geometryShader` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceFeatures.geometryShader` | ABSENT at pinned 5a07217 and origin/main e1f3f372; v11 XML has no GS stage (P12 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Complete Valhall v11 GS or PanVK poly/compute lowering, then CTS. Never expose early. |
| `tessellationShader` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceFeatures.tessellationShader` | ABSENT at pinned 5a07217 and origin/main e1f3f372; v11 XML has no TCS/TES stage (P17 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Complete Valhall v11 TCS/TES or PanVK poly/compute lowering, then CTS. Never expose early. |
| `fillModeNonSolid` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceFeatures.fillModeNonSolid` | ABSENT at pinned and origin/main; no Valhall polygon-mode field (P11 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Post-vertex primitive expansion, then CTS. Never expose early. |
| `multiViewport` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceFeatures.multiViewport` | ABSENT at pinned and origin/main; v11 has no viewport array (P14 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Hardware proof of v11 ViewportIndex plus per-viewport transform/scissor/depth, then CTS. Never expose early. |
| `textureCompressionBC` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceFeatures.textureCompressionBC` | Hardware-dependent; G615 BC mask is 0. No complete PanVK emulation (P9 BLOCKED_SAFE_FALSE) | UNSUPPORTED_NATIVE | Keep false until transparent BC contract PASSes. Never advertise early. |
| `pipelineStatisticsQuery` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceFeatures.pipelineStatisticsQuery` | ABSENT at pinned 5a07217 and origin/main e1f3f372; P20 BLOCKED_SAFE_FALSE; CreateQueryPool UNREACHABLE for PIPELINE_STATISTICS | NOT_IMPLEMENTED | Keep false. Complete pipeline-statistics query path, then CTS. Never expose early. |
| `shaderClipDistance` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceFeatures.shaderClipDistance` | ABSENT at pinned and origin/main; no PanVK clip lowering (P10 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Primitive-stage clip, then pixel matrix and CTS. Never expose early. |
| `shaderCullDistance` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceFeatures.shaderCullDistance` | ABSENT at pinned and origin/main; no PanVK cull lowering (P10 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Primitive-stage cull, then pixel matrix and CTS. Never expose early. |
| `transformFeedbackQueries` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / properties.VkPhysicalDeviceTransformFeedbackPropertiesEXT.transformFeedbackQueries` | ABSENT at pinned and origin/main; XFB queries need EXT_transform_feedback (P15 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Complete XFB queries after transformFeedback, then CTS. Never expose early. Also a 2.14.1/3.0.1 DEVICE_CREATE E_INVALIDARG. |
| `bufferImageGranularity` | `64` | `65536` | FAIL | `3.0.1` | `v3.0.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / fl_11_0_properties / properties.VkPhysicalDeviceProperties.limits.bufferImageGranularity` | G615 reports 64. Profile lists 65536 as a numeric property. Smaller granularity is more capable. | EVALUATOR_DIRECTION | No driver change. Existing evaluator uses >=; actual 64 already satisfies a 65536 max-granularity. |
| `subgroupSupportedOperations[0]` | `None` | `VK_SUBGROUP_FEATURE_BASIC_BIT` | FAIL | `3.0.1` | `v3.0.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / subgroups_none / properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[0]` | G615 subgroupSupportedOperations=1791 includes VK_SUBGROUP_FEATURE_BASIC_BIT. Capture stores a bitmask, not a string array. | EVALUATOR_ARRAY | No driver change. Evaluator compares flattened array string against integer bitmask. |

### DEVICE_CREATE

Source: `v3.0.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)`

Stock vkd3d-proton v3.0.1 vkd3d_init_device_caps returns E_INVALIDARG when robustImageAccess2 or transformFeedbackQueries is false. G615 reports both false. Stock vkd3d-proton is a Windows D3D12-on-Vulkan layer; this Android ICD is not a legal host for the tagged binaries. No stock vkd3d binary was launched.

| Requirement | Current | Required | Status | Source version | Source file/profile | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|---|---|---|
| `apiVersion>=1.3` | `1.4.363` | `1.3` | PASS | `3.0.1` | `v3.0.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_EXT_vertex_attribute_divisor` | `3` | `3` | PASS | `3.0.1` | `v3.0.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `transformFeedbackQueries` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | ABSENT at pinned and origin/main; XFB queries need EXT_transform_feedback (P15 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Complete XFB queries after transformFeedback, then CTS. Never expose early. Also a 2.14.1/3.0.1 DEVICE_CREATE E_INVALIDARG. |
| `storageTexelBufferOffsetSingleTexelAlignment` | `True` | `True` | PASS | `3.0.1` | `v3.0.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `uniformTexelBufferOffsetSingleTexelAlignment` | `True` | `True` | PASS | `3.0.1` | `v3.0.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `samplerMirrorClampToEdge` | `True` | `True` | PASS | `3.0.1` | `v3.0.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `shaderDrawParameters` | `True` | `True` | PASS | `3.0.1` | `v3.0.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_EXT_robustness2` | `1` | `1` | PASS | `3.0.1` | `v3.0.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `robustBufferAccess2` | `True` | `True` | PASS | `3.0.1` | `v3.0.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `robustImageAccess2` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Pinned and origin/main e1f3f372 still report false (P6 PARTIAL / UNSUPPORTED) | UNSUPPORTED | Keep false. Complete robustImageAccess2 semantics, then CTS. Never expose early. |
| `nullDescriptor` | `True` | `True` | PASS | `3.0.1` | `v3.0.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_KHR_push_descriptor` | `2` | `1` | PASS | `3.0.1` | `v3.0.1 libs/vkd3d/device.c vkd3d_init_device_caps E_INVALIDARG gates (VK_EXT_vertex_attribute_divisor spec>=3, transformFeedbackQueries, single texel alignment, samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |

### FEATURE_LEVEL

D3D_FEATURE_LEVEL is not available because DEVICE_CREATE fails. Stock D3D_FEATURE_LEVEL is not available because stock vkd3d never reaches CheckFeatureSupport on this ICD. The implied level below is a machine evaluation of d3d12_device_caps_init_feature_level against advertised Vulkan caps. It is not a stock CheckFeatureSupport result and must not be merged with HARD_REQUIREMENTS, PROFILE_BASELINE, or DEVICE_CREATE.

Stock `CheckFeatureSupport` D3D_FEATURE_LEVEL: `NOT_AVAILABLE`.
Implied `d3d12_device_caps_init_feature_level`: `D3D_FEATURE_LEVEL_11_0`.
Tiled resources tier: `D3D12_TILED_RESOURCES_TIER_NOT_SUPPORTED`.

FL11_0 default: `DEFAULT`. FL11_1: `FAIL`. FL12_0: `FAIL`. FL12_1: `FAIL`. FL12_2: `FAIL`.

| Requirement | Current | Required | Status | Source version | Source file/profile | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|---|---|---|
| `vertexPipelineStoresAndAtomics` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL11_1` | Pinned G615 reports false. vkd3d d3d12_device_caps_init_feature_level needs it for D3D_FEATURE_LEVEL_11_1. | NOT_IMPLEMENTED | Keep false. Do not expose until VS/GS UAV stores and atomics are proven. Blocks implied FL 11_1. |
| `sparseBinding` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_0 tiled tier>=2` | Pinned G615 reports false. Upstream PanVK sparse is pan-kmod/Panthor, not this Kbase backend. P22 not started. | NOT_IMPLEMENTED | Keep false. P22 Kbase sparse feasibility first. Do not jump to sparse implementation. |
| `sparseResidencyBuffer` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_0 tiled tier>=2` | Pinned G615 reports false. Depends on sparseBinding plus Kbase VM map/unmap. P22 not started. | NOT_IMPLEMENTED | Keep false. P22 Kbase sparse feasibility first. Do not jump to sparse implementation. |
| `sparseResidencyImage2D` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_0 tiled tier>=2` | Pinned G615 reports false. Depends on sparseBinding plus image sparse bind. P22 not started. | NOT_IMPLEMENTED | Keep false. P22 Kbase sparse feasibility first. Do not jump to sparse implementation. |
| `sparseResidencyAliased` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_0 tiled tier>=2` | Pinned G615 reports false. Depends on sparse alias mappings. P22 not started. | NOT_IMPLEMENTED | Keep false. P22 Kbase sparse feasibility first. Do not jump to sparse implementation. |
| `shaderResourceResidency` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_0 tiled tier>=2` | Pinned G615 reports false. Needed for D3D12 tiled-resource residency queries. P22 not started. | NOT_IMPLEMENTED | Keep false. P22 Kbase sparse feasibility first. Do not jump to sparse implementation. |
| `shaderResourceMinLod` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_0 tiled tier>=2` | Pinned G615 reports false. Needed for D3D12 tiled-resource min LOD. P22 not started. | NOT_IMPLEMENTED | Keep false. P22 Kbase sparse feasibility first. Do not jump to sparse implementation. |
| `VK_QUEUE_SPARSE_BINDING_BIT` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_0 tiled queue` | Capture has no sparse-binding queue family. P22 not started. | NOT_IMPLEMENTED | Keep absent. P22 Kbase sparse feasibility first. Do not jump to sparse implementation. |
| `VK_EXT_fragment_shader_interlock` | `None` | `1` | FAIL | `3.0.1` | `v3.0.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_1 ROVs` | Not advertised. vkd3d ROVsSupported needs fragmentShaderPixelInterlock and fragmentShaderSampleInterlock. | NOT_IMPLEMENTED | Keep absent. Needed only for implied FL 12_1 ROVs after FL 12_0. |
| `VK_EXT_mesh_shader` | `None` | `1` | FAIL | `3.0.1` | `v3.0.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_2 MeshShaderTier` | Not advertised. vkd3d FL 12_2 needs MeshShaderTier. | NOT_IMPLEMENTED | Keep absent. Higher than first D3D12 device-create gate. |
| `VK_KHR_ray_tracing_pipeline` | `None` | `1` | FAIL | `3.0.1` | `v3.0.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_2 RaytracingTier` | Not advertised. vkd3d FL 12_2 needs RaytracingTier 1.1. | NOT_IMPLEMENTED | Keep absent. Higher than first D3D12 device-create gate. |
| `VK_KHR_fragment_shading_rate` | `None` | `1` | FAIL | `3.0.1` | `v3.0.1 libs/vkd3d/device.c d3d12_device_caps_init_feature_level (starts at 11_0; 11_1 needs logicOp + vertexPipelineStoresAndAtomics + UAV slots>=64; 12_0 needs FL11_1 + tiled tier>=2 + resource binding tier>=2 + TypedUAVLoadAdditionalFormats; 12_1 needs ROVs + conservative raster; 12_2 needs SM6.5, VRS, mesh, raytracing, tiled tier 3) / FL12_2 VariableShadingRateTier` | Not advertised. vkd3d FL 12_2 needs VariableShadingRateTier 2. | NOT_IMPLEMENTED | Keep absent. Higher than first D3D12 device-create gate. |

Official FL profiles remain FAIL and are not a D3D feature level:

| Profile | Status | Pass | Fail |
|---|---|---:|---:|
| `VP_D3D12_FL_11_0_baseline` | FAIL | 80 | 15 |
| `VP_D3D12_FL_11_1_baseline` | FAIL | 81 | 16 |
| `VP_D3D12_FL_12_0_baseline` | FAIL | 85 | 31 |
| `VP_D3D12_FL_12_1_baseline` | FAIL | 86 | 34 |
| `VP_D3D12_FL_12_2_baseline` | FAIL | 90 | 65 |

Vendor-maximum profiles `VP_D3D12_maximum_radv` and `VP_D3D12_maximum_nv` are not D3D feature levels.

## Remaining blockers

| Feature | Gate | Owner | Implementation |
|---|---|---|---|
| `robustImageAccess2` | DEVICE_CREATE + PROFILE_BASELINE | P6 | `UNSUPPORTED` |
| `transformFeedbackQueries` | DEVICE_CREATE (2.14.1/3.0.1) + PROFILE_BASELINE | P15 | `BLOCKED_SAFE_FALSE` |
| `VK_EXT_transform_feedback` | PROFILE_BASELINE | P15 | `BLOCKED_SAFE_FALSE` |
| `transformFeedback` | PROFILE_BASELINE | P15 | `BLOCKED_SAFE_FALSE` |
| `geometryStreams` | PROFILE_BASELINE | P15 | `BLOCKED_SAFE_FALSE` |
| `geometryShader` | PROFILE_BASELINE | P12 | `BLOCKED_SAFE_FALSE` |
| `fillModeNonSolid` | PROFILE_BASELINE | P11 | `BLOCKED_SAFE_FALSE` |
| `shaderClipDistance` | PROFILE_BASELINE | P10 | `BLOCKED_SAFE_FALSE` |
| `shaderCullDistance` | PROFILE_BASELINE | P10 | `BLOCKED_SAFE_FALSE` |
| `textureCompressionBC` | PROFILE_BASELINE | P9 | `BLOCKED_SAFE_FALSE` (`native=false`, `emulated=false`) |
| `multiViewport` | PROFILE_BASELINE | P14 | `BLOCKED_SAFE_FALSE` |
| `tessellationShader` | PROFILE_BASELINE | P17 | `BLOCKED_SAFE_FALSE` |
| `pipelineStatisticsQuery` | PROFILE_BASELINE | P20 | `NOT_IMPLEMENTED` |
| `vertexPipelineStoresAndAtomics` | FEATURE_LEVEL 11_1 | later | `NOT_IMPLEMENTED` |
| `sparseBinding` / tiled resources | FEATURE_LEVEL 12_0 | P22 | `NOT_IMPLEMENTED` (not started) |
| `bufferImageGranularity` | PROFILE_BASELINE evaluator | n/a | `EVALUATOR_DIRECTION` (actual 64 is finer than 65536) |
| `subgroupSupportedOperations[0]` | PROFILE_BASELINE evaluator | n/a | `EVALUATOR_ARRAY` (bitmask 1791 includes BASIC) |

## Checks

- Official tagged profiles `validation/g615-v11-csf/profiles/vkd3d-2.14.1.json` and `vkd3d-3.0.1.json`.
- Source-derived `validation/requirements/vkd3d-proton-2.0.json`.
- Tagged README hard-device requirements for `v2.0`, `v2.14.1`, `v3.0.1`.
- Tagged `libs/vkd3d/device.c` `vkd3d_init_device_caps` and `d3d12_device_caps_init_feature_level`.
- Existing evaluators `scripts/evaluate-vkd3d-profile-baseline.py` and `scripts/evaluate-consumer-profile.py`.
- Capability dump `validation/g615-v11-csf/consumer-capabilities.json`.
- P19/P20 extras `validation/g615-v11-csf/p20-remaining-baseline-extras.json`.
- `tests/dxvk-vkd3d/test_p21_d3d12_feature_level.py`.

## Exit

P21 exits `FAIL`. Analysis complete. No missing feature was implemented. No bit was flipped.
HARD_REQUIREMENTS PASSes for 2.0, 2.14.1, and 3.0.1. PROFILE_BASELINE is `NOT_APPLICABLE` for 2.0 and FAILs for 2.14.1/3.0.1.
DEVICE_CREATE PASSes the 2.0 timeline-semaphore gate and FAILs 2.14.1/3.0.1 on `robustImageAccess2` and `transformFeedbackQueries`.
Stock FEATURE_LEVEL is `NOT_AVAILABLE`. Implied FL is `D3D_FEATURE_LEVEL_11_0` and cannot climb.
Stock vkd3d smoke is `BLOCKED`. Next engineering step is not P22 sparse; DEVICE_CREATE stays blocked until `robustImageAccess2` and XFB queries are real.
