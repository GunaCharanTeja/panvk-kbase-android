# DX5 GPU pre-raster vertical slice

Status: `BLOCKED`

No DX5 vertical-slice implementation or runtime PASS is claimed. No Vulkan
feature or extension exposure changed.

## Completed correction

DX4 allocated a hidden `PANVK_VS_VARIANT_GPU_LOWERED` slot for every vertex
shader. For an input-bearing shader, its lowering predicate was false but the
ordinary hardware compile path still populated that slot. The tracked patch now
skips that slot and permits GPU-lowered routing only when
`lowered_vs_compiled` is true. This prevents an ordinary hardware VS from being
misidentified as the compute-style hidden variant.

## Blocking contracts

1. Input-bearing VS: `poly_nir_lower_sw_vs()` lowers vertex and instance IDs,
   not Vulkan vertex attribute fetch. Panfrost's `pan_nir_lower_vs_inputs()`
   produces `load_attr_pan`; the Valhall compiler requires that intrinsic to
   remain in `MESA_SHADER_VERTEX`. DX4 changes the hidden shader to
   `MESA_SHADER_COMPUTE`. A compute-compatible vertex-fetch ABI covering Vulkan
   formats, binding strides/divisors, robustness, base vertex, and first
   instance does not exist.
2. Passthrough IDVS: no shader reads `panvk_gpu_prerast_record` and exports its
   position/varyings to the existing tiler path.
3. Generated work: no GPU kernel writes generated index or indirect records;
   no CSF sequence dispatches the lowered VS then consumes those records.
4. Lifetime: current scratch allocation is command-buffer-pool-owned. DX4
   rejects `VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT`; submission-owned
   retirement required by replay and simultaneous submission is absent.

## Semantic matrix

| Case | Status | Reason |
|---|---|---|
| direct | BLOCKED | no generated-draw path |
| indexed | BLOCKED | no compute vertex-fetch/index ABI |
| instanced | BLOCKED | no compute attribute divisor ABI |
| base vertex | BLOCKED | no compute vertex-fetch ABI |
| first instance | BLOCKED | no compute attribute divisor ABI |
| zero vertex/instance count | BLOCKED | path absent |
| repeated indices | BLOCKED | path absent |
| primitive restart | BLOCKED | path absent |
| GPU-written indirect parameters | BLOCKED | producer/consumer path absent |
| command-buffer replay | BLOCKED | execution scratch absent |
| simultaneous submission | BLOCKED | explicitly rejected by DX4 validation |
| normal IDVS before/after | BLOCKED | internal state save/restore path absent |

## Ordering and readback

The existing contract names compute, indirect, index, vertex-input stage/access
dependencies, but no DX5 commands implement them. Therefore there is no
no-readback/order PASS. No host readback or queue-idle workaround was added.

## Device and USB

Device validation: `NOT_RUN`. A candidate capable of the requested workload was
not produced, so no ADB session was started. ADB calls: `0`. USB transport
symptoms: `NONE_OBSERVED` (USB unused).
