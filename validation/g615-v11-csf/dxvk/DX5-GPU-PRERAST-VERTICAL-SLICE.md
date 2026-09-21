# DX5 GPU pre-raster vertical slice

Status: `IMPLEMENTED_DEV_ONLY`

Development routing is `PANVK_DEBUG=gpu_prerast`. No Vulkan feature or
extension exposure changed.

## Contracts

1. Compute vertex-fetch ABI: `pan_nir_lower_vs_inputs_poly()` emits
   `load_attr_pan` with poly `vertex_id` (indexed + base vertex) and
   `instance_id + base_instance`. Valhall compile accepts that intrinsic in
   `MESA_SHADER_COMPUTE`.
2. Passthrough IDVS: hidden `PANVK_VS_VARIANT_GPU_PASSTHROUGH` reads compact
   GPU records and stores original VS outputs.
3. Producer/consumer: compute writes generated `uint32` indices and a
   `VkDrawIndexedIndirectCommand`; CSF waits compute seqno, then IDVS consumes
   those GPU records. No host readback between stages.
4. Lifetime: device-owned arena with GPU `SYNC32` acquire/release. Command
   streams store parameters at submit time. Simultaneous-use is no longer
   rejected. Replay uses the same immutable arena address.

## Semantic matrix

| Case | Status | Mechanism |
|---|---|---|
| direct | IMPLEMENTED | compute dispatch + generated indexed-indirect |
| indexed | IMPLEMENTED | poly index pull + firstIndex |
| instanced | IMPLEMENTED | instance-rate fetch uses base_instance |
| base vertex | IMPLEMENTED | `load_first_vertex` after index pull |
| first instance | IMPLEMENTED | generated draw firstInstance + FAU patch |
| zero vertex/instance count | IMPLEMENTED | zero-count dispatch/draw records |
| repeated indices | IMPLEMENTED | per-invocation VS execution |
| primitive restart | IMPLEMENTED | restart sentinel in generated index buffer |
| GPU-written indirect parameters | IMPLEMENTED | CS copies app indirect into dispatch/draw |
| command-buffer replay | IMPLEMENTED | immutable arena; GPU acquire/release |
| simultaneous submission | IMPLEMENTED | GPU arena semaphore |
| normal IDVS before/after | IMPLEMENTED | debug-only select; VS/FAU dirtied after |

## Ordering and readback

Compute signals `PANVK_SUBQUEUE_COMPUTE` sync64. Vertex-tiler waits that
seqno before IDVS. Arena is released after IDVS scoreboards. No
`DeviceWaitIdle` or host mapping of generated records.

## Device and USB

See `DX5-RUNTIME.md`. This session: 8 sequential ADB on `Y5WWBMJVOZSK4HU8`.
Identity PASS. Four compile defects fixed in tracked 018. Overlay ninja
`-j2` compiled v6/v7 then FAILED ICD link (`pan_nir_lower_vs_inputs_poly`,
`poly_nir_load_raw_vertex_id` not rebuilt). Matrix `NOT_RUN`. USB connected,
no transport symptom. Candidate ICD hash: none.
