# PanVK Probe Information: Mali-G57 MC2 (Valhall v9)

## Device & Driver Identity
- **Device Name:** Mali-G57 MC2
- **Vendor ID / Device ID:** 0x13B5 (5045, ARM) / `0x90930010` (2425552912)
- **Driver Name:** `panvk`
- **Driver Info:** PanVK-kbase beta.13 (Mesa 26.3.0-devel (git-bae3b79615))
- **API Version:** 1.3.363
- **Driver Version:** 109060195 (Mesa 26.3.0-devel)
- **Extension Counts:** Device: 170 | Instance: 16

## Notable Features
| Feature | Supported | Feature | Supported |
|---|---|---|---|
| `geometryShader` | false | `tessellationShader` | false |
| `multiViewport` | false | `fillModeNonSolid` | false |
| `depthBounds` | false | `textureCompressionBC` | false |
| `textureCompressionETC2` | true | `textureCompressionASTC_LDR` | true |
| `shaderClipDistance` | false | `robustBufferAccess` | true |
| `samplerAnisotropy` | true | `independentBlend` | true |
| `sampleRateShading` | true | `vertexPipelineStoresAndAtomics` | false |
| `shaderFloat64` | false | `shaderInt64` | true |

## Key Limits
- **maxImageDimension2D:** 65536
- **maxComputeWorkGroupInvocations:** 512
- **maxViewports:** 1
- **maxColorAttachments:** 8
- **maxBoundDescriptorSets:** 7
- **timestampPeriod:** 0 (Timestamps unsupported)

## Memory Heaps & Types
- **Heap 0:** 6,139,412,480 B (~5.72 GiB), Flags: `0x1` (DEVICE_LOCAL)
- **Type 0:** Heap 0, Flags: `0x1` (DEVICE_LOCAL)
- **Type 1:** Heap 0, Flags: `0x7` (DEVICE_LOCAL | HOST_VISIBLE | HOST_COHERENT)
- **Type 2:** Heap 0, Flags: `0xB` (DEVICE_LOCAL | HOST_VISIBLE | HOST_CACHED)

## Queue Families
- **Queue Family 0:** Count = 1, Flags = `0x7` (GRAPHICS | COMPUTE | TRANSFER), `timestampValidBits` = 0

## Hardware Truth Verification
| Identity Item | Hardware Truth | Driver Reported | Status | Analysis / Notes |
|---|---|---|---|---|
| **Device Name** | Mali-G57 MC2 | Mali-G57 MC2 | Correct | Matches marketing & hardware configuration |
| **Device ID** | `0x90930010` | `0x90930010` (2425552912) | Correct | Matches kbase GPUPROPS `gpu_id` |
| **Core Count** | 2 cores (`shader_present=0x5`) | 2 cores (MC2) | Correct | `shader_present` bitmask 0x5 has 2 cores active |
| **Arch & API Version** | Valhall v9 (JM) | Vulkan 1.3.363 | Overstated | Upstream PanVK targets Vulkan 1.4 only on v10+ (CSF). For v9 Job Manager, 1.3 is overstated/aspirational: lacks mandatory core features (geometry/tessellation shaders, clip distances), timestamps are 0-bit, and conformanceVersion is `0.0.0.0`. Upstream reports ~1.1 for v6/v7/v9. |
