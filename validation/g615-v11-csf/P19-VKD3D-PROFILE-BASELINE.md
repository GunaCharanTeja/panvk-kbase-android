# P19 vkd3d-proton 2.14.1 / 3.0.1 Re-evaluation

## Result

`FAIL`

Machine-evaluated HARD_REQUIREMENTS, PROFILE_BASELINE, DEVICE_CREATE, and FEATURE_LEVEL only.
These four gates stay separate. No merged PASS. No PanVK patch. No feature-bit change. P20 was not started.

```text
VKD3D_2_14_1_HARD=PASS
VKD3D_2_14_1_BASELINE=FAIL
VKD3D_2_14_1_DEVICE=FAIL
VKD3D_2_14_1_FEATURE_LEVEL=NOT_AVAILABLE
VKD3D_2_14_1_SMOKE=BLOCKED
VKD3D_3_0_1_HARD=PASS
VKD3D_3_0_1_BASELINE=FAIL
VKD3D_3_0_1_DEVICE=FAIL
VKD3D_3_0_1_FEATURE_LEVEL=NOT_AVAILABLE
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
`VP_D3D12_FL_11_0_baseline` in `VP_D3D12_VKD3D_PROTON_profile.json`. DEVICE_CREATE comes from
`vkd3d_init_device_caps` in `libs/vkd3d/device.c`. FEATURE_LEVEL is the D3D12
feature level that stock vkd3d would report after a successful device create.

README HARD can PASS while DEVICE_CREATE FAILs: README names `VK_EXT_robustness2`
as an extension, but `vkd3d_init_device_caps` also requires `robustImageAccess2=true`.
PROFILE_BASELINE is broader than either gate and still requires geometry, tessellation,
XFB, BC, clip/cull, fillModeNonSolid, multiViewport, and pipelineStatisticsQuery.

## Initialization / stock vkd3d smoke

Stock vkd3d-proton tags `v2.14.1` and `v3.0.1` cannot legally create a D3D12 device
on this ICD: `robustImageAccess2` remains `false`, so `vkd3d_init_device_caps`
returns `E_INVALIDARG`. The tagged binaries are a Windows D3D12-on-Vulkan layer;
this Android ICD is not a legal host. No game boot, no vkd3d init log, no reported
D3D feature level. Smoke is `BLOCKED` for both versions.

P6 left `robustImageAccess2` false. P9-P17 left `textureCompressionBC`,
`shaderClipDistance`, `shaderCullDistance`, `fillModeNonSolid`, `geometryShader`,
`multiViewport`, `transformFeedback`, `geometryStreams`, and `tessellationShader`
false. P18 did not flip them. P19 did not flip them. `pipelineStatisticsQuery`
remains false for P20.

P21 D3D12 feature/profile analysis and P22 sparse/Kbase were not started.

## vkd3d-proton 2.14.1

HARD_REQUIREMENTS: **PASS**. PROFILE_BASELINE: **FAIL**. DEVICE_CREATE: **FAIL**. FEATURE_LEVEL: **NOT_AVAILABLE**. Smoke: **BLOCKED**.

Official profile: `https://raw.githubusercontent.com/HansKristian-Work/vkd3d-proton/v2.14.1/VP_D3D12_VKD3D_PROTON_profile.json` (`validation/g615-v11-csf/profiles/vkd3d-2.14.1.json`, `9ff2e08e82f03f214714591e0727a35c164639e71879c9721ca9e5b4bd0ddd17`).

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

`VP_D3D12_FL_11_0_baseline` (`Minimum baseline (FL 11.0)`) is the official minimum D3D12 baseline. Higher FL / optimal / vendor-maximum profiles are recorded only under FEATURE_LEVEL evidence and are P21.

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
| `pipelineStatisticsQuery` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceFeatures.pipelineStatisticsQuery` | ABSENT at pinned 5a07217 and origin/main e1f3f372; P20 not started | NOT_IMPLEMENTED | Keep false. Implement pipeline statistics with correct Vulkan counters, then CTS. Never expose early. |
| `shaderClipDistance` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceFeatures.shaderClipDistance` | ABSENT at pinned and origin/main; no PanVK clip lowering (P10 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Primitive-stage clip, then pixel matrix and CTS. Never expose early. |
| `shaderCullDistance` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceFeatures.shaderCullDistance` | ABSENT at pinned and origin/main; no PanVK cull lowering (P10 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Primitive-stage cull, then pixel matrix and CTS. Never expose early. |
| `transformFeedbackQueries` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / properties.VkPhysicalDeviceTransformFeedbackPropertiesEXT.transformFeedbackQueries` | ABSENT at pinned and origin/main; XFB queries need EXT_transform_feedback (P15 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Complete XFB queries after transformFeedback, then CTS. Never expose early. |
| `bufferImageGranularity` | `64` | `65536` | FAIL | `2.14.1` | `v2.14.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / fl_11_0_properties / properties.VkPhysicalDeviceProperties.limits.bufferImageGranularity` | G615 reports 64. Profile lists 65536 as a numeric property. Smaller granularity is more capable. | EVALUATOR_DIRECTION | No driver change. Existing evaluator uses >=; actual 64 already satisfies a 65536 max-granularity. |
| `subgroupSupportedOperations[0]` | `None` | `VK_SUBGROUP_FEATURE_BASIC_BIT` | FAIL | `2.14.1` | `v2.14.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / subgroups_none / properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[0]` | G615 subgroupSupportedOperations=1791 includes VK_SUBGROUP_FEATURE_BASIC_BIT. Capture stores a bitmask, not a string array. | EVALUATOR_ARRAY | No driver change. Evaluator compares flattened array string against integer bitmask. |

### DEVICE_CREATE

Source: `v2.14.1 libs/vkd3d/device.c vkd3d_init_device_caps (samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)`

Stock vkd3d-proton v2.14.1 vkd3d_init_device_caps requires robustImageAccess2. G615 reports that bit false, so D3D12 device creation returns E_INVALIDARG. Stock vkd3d-proton is a Windows D3D12-on-Vulkan layer; this Android ICD is not a legal host for the tagged binaries. No stock vkd3d binary was launched.

| Requirement | Current | Required | Status | Source version | Source file/profile | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|---|---|---|
| `apiVersion>=1.3` | `1.4.363` | `1.3` | PASS | `2.14.1` | `v2.14.1 libs/vkd3d/device.c vkd3d_init_device_caps (samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `samplerMirrorClampToEdge` | `True` | `True` | PASS | `2.14.1` | `v2.14.1 libs/vkd3d/device.c vkd3d_init_device_caps (samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `shaderDrawParameters` | `True` | `True` | PASS | `2.14.1` | `v2.14.1 libs/vkd3d/device.c vkd3d_init_device_caps (samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_EXT_robustness2` | `1` | `1` | PASS | `2.14.1` | `v2.14.1 libs/vkd3d/device.c vkd3d_init_device_caps (samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `robustBufferAccess2` | `True` | `True` | PASS | `2.14.1` | `v2.14.1 libs/vkd3d/device.c vkd3d_init_device_caps (samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `robustImageAccess2` | `False` | `True` | FAIL | `2.14.1` | `v2.14.1 libs/vkd3d/device.c vkd3d_init_device_caps (samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Pinned and origin/main e1f3f372 still report false (P6 PARTIAL / UNSUPPORTED) | UNSUPPORTED | Keep false. Complete robustImageAccess2 semantics, then CTS. Never expose early. |
| `nullDescriptor` | `True` | `True` | PASS | `2.14.1` | `v2.14.1 libs/vkd3d/device.c vkd3d_init_device_caps (samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_KHR_push_descriptor` | `2` | `1` | PASS | `2.14.1` | `v2.14.1 libs/vkd3d/device.c vkd3d_init_device_caps (samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |

### FEATURE_LEVEL

D3D_FEATURE_LEVEL is not available because DEVICE_CREATE fails. Stock vkd3d never reaches CheckFeatureSupport. Machine-evaluated FL profiles remain FAIL and are recorded separately.

| Profile | Status | Pass | Fail |
|---|---|---:|---:|
| `VP_D3D12_FL_11_0_baseline` | FAIL | 78 | 15 |
| `VP_D3D12_FL_11_1_baseline` | FAIL | 79 | 16 |
| `VP_D3D12_FL_12_0_baseline` | FAIL | 83 | 31 |
| `VP_D3D12_FL_12_1_baseline` | FAIL | 84 | 34 |
| `VP_D3D12_FL_12_2_baseline` | FAIL | 88 | 65 |

Machine-evaluated FL profiles do not become a D3D feature level.

## vkd3d-proton 3.0.1

HARD_REQUIREMENTS: **PASS**. PROFILE_BASELINE: **FAIL**. DEVICE_CREATE: **FAIL**. FEATURE_LEVEL: **NOT_AVAILABLE**. Smoke: **BLOCKED**.

Official profile: `https://raw.githubusercontent.com/HansKristian-Work/vkd3d-proton/v3.0.1/VP_D3D12_VKD3D_PROTON_profile.json` (`validation/g615-v11-csf/profiles/vkd3d-3.0.1.json`, `d37e753fa81e43251bbd576b17b888c1a7139b3fc8c16bd0e0ec9c7bed7a9db6`).

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

`VP_D3D12_FL_11_0_baseline` (`Minimum baseline (FL 11.0)`) is the official minimum D3D12 baseline. Higher FL / optimal / vendor-maximum profiles are recorded only under FEATURE_LEVEL evidence and are P21.

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
| `pipelineStatisticsQuery` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceFeatures.pipelineStatisticsQuery` | ABSENT at pinned 5a07217 and origin/main e1f3f372; P20 not started | NOT_IMPLEMENTED | Keep false. Implement pipeline statistics with correct Vulkan counters, then CTS. Never expose early. |
| `shaderClipDistance` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceFeatures.shaderClipDistance` | ABSENT at pinned and origin/main; no PanVK clip lowering (P10 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Primitive-stage clip, then pixel matrix and CTS. Never expose early. |
| `shaderCullDistance` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / features.VkPhysicalDeviceFeatures.shaderCullDistance` | ABSENT at pinned and origin/main; no PanVK cull lowering (P10 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Primitive-stage cull, then pixel matrix and CTS. Never expose early. |
| `transformFeedbackQueries` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / baseline_features / properties.VkPhysicalDeviceTransformFeedbackPropertiesEXT.transformFeedbackQueries` | ABSENT at pinned and origin/main; XFB queries need EXT_transform_feedback (P15 BLOCKED_SAFE_FALSE) | NOT_IMPLEMENTED | Keep false. Complete XFB queries after transformFeedback, then CTS. Never expose early. |
| `bufferImageGranularity` | `64` | `65536` | FAIL | `3.0.1` | `v3.0.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / fl_11_0_properties / properties.VkPhysicalDeviceProperties.limits.bufferImageGranularity` | G615 reports 64. Profile lists 65536 as a numeric property. Smaller granularity is more capable. | EVALUATOR_DIRECTION | No driver change. Existing evaluator uses >=; actual 64 already satisfies a 65536 max-granularity. |
| `subgroupSupportedOperations[0]` | `None` | `VK_SUBGROUP_FEATURE_BASIC_BIT` | FAIL | `3.0.1` | `v3.0.1 VP_D3D12_VKD3D_PROTON_profile.json VP_D3D12_FL_11_0_baseline / subgroups_none / properties.VkPhysicalDeviceVulkan11Properties.subgroupSupportedOperations[0]` | G615 subgroupSupportedOperations=1791 includes VK_SUBGROUP_FEATURE_BASIC_BIT. Capture stores a bitmask, not a string array. | EVALUATOR_ARRAY | No driver change. Evaluator compares flattened array string against integer bitmask. |

### DEVICE_CREATE

Source: `v3.0.1 libs/vkd3d/device.c vkd3d_init_device_caps (samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)`

Stock vkd3d-proton v3.0.1 vkd3d_init_device_caps requires robustImageAccess2. G615 reports that bit false, so D3D12 device creation returns E_INVALIDARG. Stock vkd3d-proton is a Windows D3D12-on-Vulkan layer; this Android ICD is not a legal host for the tagged binaries. No stock vkd3d binary was launched.

| Requirement | Current | Required | Status | Source version | Source file/profile | Upstream PanVK | Implementation | Action |
|---|---:|---:|---|---|---|---|---|---|
| `apiVersion>=1.3` | `1.4.363` | `1.3` | PASS | `3.0.1` | `v3.0.1 libs/vkd3d/device.c vkd3d_init_device_caps (samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `samplerMirrorClampToEdge` | `True` | `True` | PASS | `3.0.1` | `v3.0.1 libs/vkd3d/device.c vkd3d_init_device_caps (samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `shaderDrawParameters` | `True` | `True` | PASS | `3.0.1` | `v3.0.1 libs/vkd3d/device.c vkd3d_init_device_caps (samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_EXT_robustness2` | `1` | `1` | PASS | `3.0.1` | `v3.0.1 libs/vkd3d/device.c vkd3d_init_device_caps (samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `robustBufferAccess2` | `True` | `True` | PASS | `3.0.1` | `v3.0.1 libs/vkd3d/device.c vkd3d_init_device_caps (samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `robustImageAccess2` | `False` | `True` | FAIL | `3.0.1` | `v3.0.1 libs/vkd3d/device.c vkd3d_init_device_caps (samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Pinned and origin/main e1f3f372 still report false (P6 PARTIAL / UNSUPPORTED) | UNSUPPORTED | Keep false. Complete robustImageAccess2 semantics, then CTS. Never expose early. |
| `nullDescriptor` | `True` | `True` | PASS | `3.0.1` | `v3.0.1 libs/vkd3d/device.c vkd3d_init_device_caps (samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |
| `VK_KHR_push_descriptor` | `2` | `1` | PASS | `3.0.1` | `v3.0.1 libs/vkd3d/device.c vkd3d_init_device_caps (samplerMirrorClampToEdge, robustBufferAccess2, robustImageAccess2, nullDescriptor, shaderDrawParameters, VK_KHR_push_descriptor)` | Present in pinned G615 capture | IMPLEMENTED | None |

### FEATURE_LEVEL

D3D_FEATURE_LEVEL is not available because DEVICE_CREATE fails. Stock vkd3d never reaches CheckFeatureSupport. Machine-evaluated FL profiles remain FAIL and are recorded separately.

| Profile | Status | Pass | Fail |
|---|---|---:|---:|
| `VP_D3D12_FL_11_0_baseline` | FAIL | 80 | 15 |
| `VP_D3D12_FL_11_1_baseline` | FAIL | 81 | 16 |
| `VP_D3D12_FL_12_0_baseline` | FAIL | 85 | 31 |
| `VP_D3D12_FL_12_1_baseline` | FAIL | 86 | 34 |
| `VP_D3D12_FL_12_2_baseline` | FAIL | 90 | 65 |

Machine-evaluated FL profiles do not become a D3D feature level.

## Remaining blockers

| Feature | Gate | Owner | Implementation |
|---|---|---|---|
| `robustImageAccess2` | DEVICE_CREATE | P6 | `UNSUPPORTED` |
| `geometryShader` | PROFILE_BASELINE | P12 | `BLOCKED_SAFE_FALSE` |
| `fillModeNonSolid` | PROFILE_BASELINE | P11 | `BLOCKED_SAFE_FALSE` |
| `shaderClipDistance` | PROFILE_BASELINE | P10 | `BLOCKED_SAFE_FALSE` |
| `shaderCullDistance` | PROFILE_BASELINE | P10 | `BLOCKED_SAFE_FALSE` |
| `textureCompressionBC` | PROFILE_BASELINE | P9 | `BLOCKED_SAFE_FALSE` (`native=false`, `emulated=false`) |
| `multiViewport` | PROFILE_BASELINE | P14 | `BLOCKED_SAFE_FALSE` |
| `VK_EXT_transform_feedback` | PROFILE_BASELINE | P15 | `BLOCKED_SAFE_FALSE` |
| `transformFeedback` | PROFILE_BASELINE | P15 | `BLOCKED_SAFE_FALSE` |
| `geometryStreams` | PROFILE_BASELINE | P15 | `BLOCKED_SAFE_FALSE` (also waits on P12) |
| `transformFeedbackQueries` | PROFILE_BASELINE | P15 | `BLOCKED_SAFE_FALSE` |
| `tessellationShader` | PROFILE_BASELINE | P17 | `BLOCKED_SAFE_FALSE` |
| `pipelineStatisticsQuery` | PROFILE_BASELINE | P20 | `NOT_IMPLEMENTED` |
| `bufferImageGranularity` | PROFILE_BASELINE evaluator | n/a | `EVALUATOR_DIRECTION` (actual 64 is finer than 65536) |
| `subgroupSupportedOperations[0]` | PROFILE_BASELINE evaluator | n/a | `EVALUATOR_ARRAY` (bitmask 1791 includes BASIC) |

## Checks

- `scripts/evaluate-vulkan-profile.py` via `evaluate-consumer-profile.py` against `consumer-capabilities.json`.
- Official `validation/g615-v11-csf/profiles/vkd3d-2.14.1.json` and `vkd3d-3.0.1.json` `VP_D3D12_FL_11_0_baseline`.
- Tagged README hard-device requirements for `v2.14.1` and `v3.0.1`.
- Tagged `libs/vkd3d/device.c` `vkd3d_init_device_caps` for DEVICE_CREATE.
- `tests/dxvk-vkd3d/test_p19_vkd3d_profile_baseline.py`.

## Exit

P19 exits `FAIL`. HARD_REQUIREMENTS PASSes. PROFILE_BASELINE FAILs. DEVICE_CREATE FAILs on `robustImageAccess2`. FEATURE_LEVEL is `NOT_AVAILABLE`. Stock vkd3d smoke is `BLOCKED`.
Next engineering step is P20 `pipelineStatisticsQuery` for remaining baseline bits, but DEVICE_CREATE stays blocked until `robustImageAccess2` is real. P21+ was not started.
