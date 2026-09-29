# DX5 GPU pre-raster vertical slice

Status: `IMPLEMENTED_DEV_ONLY` (linked; CreateDevice not proven)

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
4. Lifetime: device-owned arena with GPU `SYNC32` acquire/release, allocated
   only when `PANVK_DEBUG=gpu_prerast`. Command streams store parameters at
   submit time. Simultaneous-use is no longer rejected. Replay uses the same
   immutable arena address.

Poly NIR helpers used by the slice (`poly_nir_load_raw_vertex_id`,
`poly_nir_lower_sw_vs`, `poly_nir_lower_vs_before_gs`,
`poly_nir_lower_sysvals`) are inlined in `panvk_vX_shader.c`. The device
tree has no `mesa_clc`, so `libpoly_nir` is not linked.

Tracked 018 now gates the arena on `PANVK_DEBUG=gpu_prerast` and sizes it
to 256 KiB / 4096 invocations. Device rebuild of that overlay is still
pending (`scripts/dxvk/dx5-device-validate.sh`).

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

Runtime (2026-09-29, profile g615-v11-csf, patches 018+019+020, ICD
`24fb09610c651a9a2e2f986684467466e07c6fe8ef8d84551e8a5c7685c8c5a4`):
IDVS 13/13 PASS, `PANVK_DEBUG=gpu_prerast` 13/13 PASS, `MATRIX_FAILS=0`
on both paths, 9 consecutive runs (1 + 8). No CS_FAULT, no device loss.
See `DX5-RUNTIME.md`.

Root cause of empty records (fixed in `csf-v11/020`): lowered VS
`RUN_COMPUTE` used `gfx.tsd`, which is 0 until `prepare_draw()` runs after
`gpu_prerast_draw()`. The COMPUTE CSG raised `CS_FAULT` 0x58
DATA_INVALID_FAULT (data 0x1612) before any store. Also fixed:
`outputs_written`=0 (read before IO gathering → empty passthrough),
`load_vertex_id_zero_base` in the passthrough (no backend lowering →
SIGSEGV 139), output types, and the COMPUTE subqueue missing from
DRAW_INDIRECT/vertex-input barrier consumers.

`gpu_written_indirect` harness was invalid Vulkan (fill/copy inside the
render pass, no fill→copy barrier) and failed intermittently on both
paths; fixed in `tests/dxvk/vulkan/gpu_prerast_slice.c`.

## Ordering and readback

With `gpu_prerast`, barriers whose dst stages include DRAW_INDIRECT,
vertex input, or pre-raster shaders also make the COMPUTE subqueue wait.
Compute signals `PANVK_SUBQUEUE_COMPUTE` sync64. Vertex-tiler waits that
seqno before IDVS. Arena is released after IDVS scoreboards. No
`DeviceWaitIdle` or host mapping of generated records.

## Device and USB

See `DX5-RUNTIME.md`. This session: identity ADB on `Y5WWBMJVOZSK4HU8`
failed, then serial disappeared (`device not found`). Overlay, ninja,
CreateDevice, matrix `NOT_RUN`. USB disconnected. No kernel USB line
observed. No further ADB.

Previous linked ICD (not the gated 256 KiB overlay):

```text
ICD sha256: 61ab189087f9d34bfde2969c2e5d707f5725f0ad3a9e687257c532d7610e973a
size: 20053320
```
