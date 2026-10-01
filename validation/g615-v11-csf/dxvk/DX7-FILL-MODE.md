# DX7 fillModeNonSolid

Status: exposed on v10+ (`fillModeNonSolid=1`). `extendedDynamicState3PolygonMode`
stays 0.
Patch: `patches/csf-v11/025-fill-mode-non-solid-gpu-polygon.patch`.

Valhall has no polygon-mode raster state. A pipeline with `polygonMode`
LINE/POINT compiles three extra VS variants: GPU_LOWERED, GPU_PASSTHROUGH and
GPU_POLYGON. Each draw then runs these steps, all on the GPU:

1. The lowered VS (compute) writes the post-VS records and generated indices
   into the device arena.
2. The polygon kernel (compute) waits on the lowered VS slots, then:
   - assembles the triangles: list, strip (odd triangles swapped) and fan,
     with primitive restart and instances;
   - takes facing from the clip-space determinant `det(x, y, w)`, negated
     for a negative viewport height;
   - applies `cullMode`/`frontFace`;
   - writes 6 indices per triangle (`AB BC CA`, or `A B C` + restart for
     points) and an indirect draw.
3. The passthrough VS draws LINES or POINTS indexed-indirect with restart.

There is no CPU readback and no CPU fallback.

Fixes included:
- The pipeline hash includes the non-solid state, because the variant set
  depends on it. Before this fix a cached FILL VS was reused.
- `build_dcd_flags` now takes the drawn primitive.
- The polygon kernel waits on the scoreboard. Without the wait, it read stale
  generated indices when restart was enabled.
- The segment start is 0 when restart is disabled.
- Arena ABI 2: 32 MB and 65536 invocations; record and index stores are
  bounded by capacity.

Limits (these keep the filled HW path, i.e. are not lowered):
- draws above the arena capacity
- multi-draw indirect and indirect-count
- VS with side effects

Lines use the HW line rasterizer (width 1). The polygon depth bias of lines
or points is not separately handled.

## Device proof (2026-09-29)

ICD sha256 `9f1a5a57ad417dc5b36763378d2d75f183e8116c124989c7ebfaac17bd1a5a49`
(Mali-G615 MC6, kbase CSF v11). Test: `tests/dxvk/vulkan/fill-mode/fill_mode.c`.

How each case is checked:
- The LINE/POINT draw result is compared pixel-exact against a GPU reference.
  The reference draws the same edges or vertices as a filled `LINE_LIST` or
  `POINT_LIST`.
- The reference includes only the triangles that the HW rasterizer keeps
  when each one is drawn filled with the same cull mode and viewport.
- Facing is never computed on the CPU.

```text
ICD device=Mali-G615 MC6 geometryShader=0 fillModeNonSolid=1 multiViewport=1 shaderClipDistance=1 shaderCullDistance=1 maxViewports=16 maxClip=8 maxCull=8 maxCombined=8
CASE list_line_cull_none tris=3 kept=3 ref_red=267 red=267 mismatch=0 PASS
CASE list_line_cull_back tris=3 kept=1 ref_red=85 red=85 mismatch=0 PASS
CASE list_line_cull_front tris=3 kept=2 ref_red=182 red=182 mismatch=0 PASS
CASE list_line_cull_both tris=3 kept=0 ref_red=0 red=0 mismatch=0 PASS
CASE list_line_cull_back_flipy tris=3 kept=2 ref_red=182 red=182 mismatch=0 PASS
CASE strip_line_cull_none tris=3 kept=3 ref_red=146 red=146 mismatch=0 PASS
CASE strip_line_cull_back tris=3 kept=3 ref_red=146 red=146 mismatch=0 PASS
CASE fan_line_cull_none tris=5 kept=5 ref_red=241 red=241 mismatch=0 PASS
CASE fan_line_cull_front tris=5 kept=4 ref_red=198 red=198 mismatch=0 PASS
CASE strip_restart_line tris=4 kept=4 ref_red=204 red=204 mismatch=0 PASS
CASE fan_restart_line_cull_front tris=4 kept=4 ref_red=148 red=148 mismatch=0 PASS
CASE fan_restart_line_cull_back tris=4 kept=0 ref_red=0 red=0 mismatch=0 PASS
CASE list_point_cull_none tris=3 kept=3 ref_red=9 red=9 mismatch=0 PASS
CASE fan_point_cull_back tris=5 kept=1 ref_red=3 red=3 mismatch=0 PASS
CASE strip_point_flipy_cull_front tris=3 kept=3 ref_red=5 red=5 mismatch=0 PASS
CASE indirect_list_line_cull_back tris=3 kept=1 ref_red=85 red=85 mismatch=0 PASS
CASE indirect_indexed_strip_restart_line tris=4 kept=4 ref_red=204 red=204 mismatch=0 PASS
FILL_MODE_FAILS=0
```

The output is identical with `PANVK_DEBUG=gpu_prerast`.

Regressions on the same ICD, all passing:

| Suite | Result |
| --- | --- |
| clip_cull, default and gpu_prerast | `CLIP_CULL_FAILS=0` |
| multi_viewport, default and gpu_prerast | `MULTI_VIEWPORT_FAILS=0` |
| DX5 gpu_prerast_slice, IDVS and PRERAST | `MATRIX_FAILS=0` |
| DX6 BC (`bc_emul`) | `BC_RC=0` |

The DX6 run without `bc_emul` still reports `fmt_fail=16`, as expected. dmesg
has no mali/kbase faults.

Not run: the CTS polygon-mode tests.
