# GPU tessellation on the G615 (2026-09-30)

Device: Mali-G615 MC6, Alpine glibc chroot `/data/local/tmp/chrootAlpine`, ADB serial `Y5WWBMJVOZSK4HU8`. Mesa worktree `work/mesa-tess` branch `dx8-tess` on `dx6-dx7-base` `7722aaee230` (`d33a343037f` + cube-view commit). Commits: `9134169824a` "panvk: chunk GPU tessellation in CS loops", `01a54283d9b` "panvk: keep the TES point size write" on top of the dx8-tess tessellation commits `378bad20f31..23a5cde7581`. ICD SHA-256 `b3202ef52ae434a56512cc21cb357464d19d98c31f1cc7c8b0d062a5e5edca53`. Built in the chroot, `NINJA_RC=0`.

## Design

`tessellationShader` is implemented fully on the GPU through `gpu_prerast`, with no CPU fallback, no clamping, and no truncation.

Pipeline per draw:
- VS as compute (lowered variant) -> TCS as compute -> libpoly tessellator COUNT -> prefix sum/draw record (`panlib_tess_draw`) -> tessellator WITH_COUNTS emits indices -> TES rasterized as the hardware VS.
- With a GS or non-fill polygon mode, the TES runs again as a `gpu_prerast` VS followed by the GS kernel.

Chunking:
- The 96 MiB tessellation arena is sized for the worst case (level 64, patch size 32); `max_patches` per chunk derives from that.
- The compute subqueue runs a CS loop (`cs_while`): `panlib_tess_step` picks the next chunk (whole instances when an instance fits, otherwise a patch range of one instance), then VS, TCS, tessellator, and draw record run.
- When the TES output goes through `gpu_prerast` again, each chunk is further split into index ranges sized to the `gpu_prerast` arena and GS capacity.
- The vertex/tiler subqueue runs a matching loop that draws each range.
- The two loops handshake through two memory `sync32` objects (ready/free) in `struct panlib_tess_draw`, so the arenas are reused only after the previous range is drawn.

Parameters and limits:
- Indirect draw parameters, indirect count, oversized counts, instancing, `firstInstance`, base vertex, and `DrawID` are preserved.
- Chunk starts are added to the lowered-VS invocation IDs (`chunk_vertex`/`chunk_instance`).
- `gl_PrimitiveID` in TCS/TES is the patch index within its instance.
- 64-bit totals avoid overflow.
- The old CPU patch split and the TES->GS truncation are removed.
- Limits: `maxTessellationPatchSize` 32, `maxTessellationGenerationLevel` 64.

Fixes found on device:
- An empty VS (no outputs) made the draw return early. Fixed by not skipping PATCHES draws.
- The TES point size was overwritten by the default 1.0 because `outputs_written` was stale. Fixed by gathering info before `poly_nir_lower_tes`.
- Instance-rate attribute descriptors are patched with `firstInstance` on the compute queue for indirect `gpu_prerast` draws.

## Results

### Pixel-reference probe

`tests/dxvk/vulkan/tessellation` (`tessellation.c`): 24/24 PASS, `TESSELLATION_FAILS=0`.

Cases tested including:
- `quad_equal_l4`, `tri_equal_l3`, `quad_frac_odd_l3.5`, `quad_frac_even_l2.5`
- `isolines_4x4`, `point_mode_l4`, winding cases
- `patch_cp1_expand`, `patch_cp32`, `dynamic_cp_3_then_4`
- indexed, indirect, and instanced cases
- `grid_64x48_l16` (3072 patches, several chunks)
- `quad_l64`
- `quad_unequal` (outer 1/7/3/64, inner 5/2)
- `grid_inst3_indirect_first1`, `grid_indexed_inst2_direct_first1`, and `grid_indexed_inst2_indirect_first1` (768 patches per instance, chunks split instances)
- `grid_tes_to_gs_l16`, `tes_to_gs`
- `stats_quad_equal_l4`

### CTS `dEQP-VK.tessellation.*` (1103 cases)

Results:
- Total: 1103
- Pass: 526
- Fail: 0
- NotSupported: 577
- Crash: 0
- Timeout: 0
- DeviceLost: 0

NotSupported breakdown:
- 240: `shaderFloat64` (`tess_io`)
- 313: `vertexPipelineStoresAndAtomics` not exposed (invariance 192, user_defined_io 54, primitive_discard 44, tesscoord 18, fractional_spacing 4, misc_draw amber 1)
- 24: `VK_EXT_shader_object` (`misc_draw`)

Before this work the prototype scored Pass=406 Fail=120 on the same list (winding 48, shader_input_output, common_edge, geometry_interaction.point_size).

### Regression matrices

All pass:
- Slice IDVS: 13/13
- Slice gpu_prerast: 13/13
- Clip: 13/13 + 13/13
- Multi-viewport: 5/5 + 5/5
- Fill: 17/17 + 17/17
- Geometry: 23/23 + 23/23 (`PANVK_DEBUG=gs`)
- Pipeline stats: 12/12
- BC decode: 16 cases, `BC_DEVICE_FAILS=0`

### Geometry, statistics query, and draw parameters CTS

`dEQP-VK.geometry.*`, `query_pool.statistics_query.*tes*`, `draw.renderpass.shader_draw_parameters.*` (1340 cases):
- Pass: 1194
- Fail: 2
- NotSupported: 143
- DeviceLost: 1

The 2 fails are `geometry.layered.cube_array.{36_36_12,64_64_12}.secondary_cmd_buffer`, already listed as open in `DX7-GS.md` (they pass on `work/mesa` with `e2fde360503`).
The DeviceLost was `query_pool.statistics_query.clipping_invocations.primary.32bits_dstoffset_cmdcopyquerypoolresults_patch_list_tessellation`; it passed when rerun alone.

### DXVK Native v3.1.1

Same probe and setup as `DX8-NATIVE-WORKLOAD.md`, Xvfb :98:
- `d3d11`: exit 0 `D3D11 HRESULT=0x00000000 feature_level=0xb000` (FL 11_0, up from 10_1)
- `d3d11-workload`: exit 0 `red_pixel=1`
- `d3d9-workload`: exit 0 `triangle_pixel=1`

## Open

- `vertexPipelineStoresAndAtomics` is not exposed, so 313 tessellation CTS cases do not run.
- GS `gl_PrimitiveIDIn` after TES counts output primitives across the whole draw, not per instance.
- Conditional rendering does not gate the compute loop.
- Prims-generated with TES->GS counts the GS output draw per range the same approximate way as the existing GS path.
- Command buffers that patch attribute descriptors on the GPU are not safe to resubmit (existing behaviour).
- Exported as `csf-v11/065` (squashed) on the integrated series with transform feedback, see `DX9-TRANSFORM-FEEDBACK.md`. Integrated ICD: tessellation CTS 526 Pass / 0 Fail, pixel probe 24/24 `TESSELLATION_FAILS=0`.
