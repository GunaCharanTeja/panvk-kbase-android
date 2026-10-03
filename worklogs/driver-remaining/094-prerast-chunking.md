# 094 Chunked prerast draws

Patch: `patches/csf-v11/094-chunk-large-and-indirect-gpu_prerast-draws-on-the-gpu.patch`. Number 086 stays unused.

## What changed

The gpu_prerast path (lowered VS, GS, transform feedback, tessellation emulation) was limited to the 65536 invocations of one arena, and one indirect draw could not be split because its counts are unknown while recording.

Indirect draws, and direct draws that need more than one chunk (instance windows, strips with primitive restart), now run two CS loops (compute and vertex/tiler). The arena is handed over with the tessellation ready/free syncs, like `gpu_prerast_tess`. Planner kernels in `draw_helper.cl` (`panlib_chunk_setup`, `panlib_chunk_step`, struct `panlib_chunk_draw` in `tess_draw.h`) read the draw and plan each chunk:

- instances: whole instances while they fit, else a vertex window of one instance;
- strips with restart: the window ends at its last restart index, or is cut mid-strip and repeats the strip overlap;
- `chunk_vertex` / `chunk_instance` keep firstVertex, firstInstance and vertexOffset the API values;
- GS primitive ID base, XFB offsets (existing counters) and primitives-generated (added per chunk) continue across chunks;
- the loop runs until the planner says done, so nothing is dropped.

The CPU split also handles triangle strips with adjacency. The compute CS copies the `more` flag with a CS store (`more_vt`) before it signals ready, so the vertex/tiler CS does not depend on a kernel store.

## Tests

APK `large_draw` (`tests/dxvk/vulkan/large-draw`, 14 cases): points, GS, tess, restart strips (direct and indirect), indirect count, multi-indirect, 60000 instances. Checks XFB records, primitives-generated and no-drop counts. The old driver rejects these draws (cap).

- APK build with 094: large_draw passes in most runs; `autorun all` 16/16.
- CTS (chroot ICD, `tmp/g615-gap/cts-all.txt`, 36144 cases): baseline 17067 pass / 0 fail / 19077 NotSupported; with 093-095: identical. Groups included: transform_feedback, geometry, tessellation, draw indirect, multisample/variable rate, secondary command buffers, no-attachment, primitives_generated.

## Limits and open problems

- Flake (fixed by 096): restart-strip cases (`restart_direct`, `restart_indirect`) lost the device in about 2-10% of runs (chroot loops: 1/25, 1/25, 0/60, 1/60; APK: 1 in 9). Cause: the single-invocation restart scan in `panlib_chunk_step` made each planner job long while the vertex/tiler group waited across CSGs; a long planner job also breaks the non-restart `strip_indirect` loop. See `096-parallel-chunk-planner.md`.
- Conditional rendering does not predicate the lowering jobs or the primitives-generated counter of a chunked or tessellated draw (only the raster launch). This predates 093/094.
- Chunks hold at most 2048 instances: lowering grids with 8192 or more instances hang the G615 (4000 works).
- Chunked draws under rasterizer discard skip the raster draw (thousands of tiny draws exhaust the tiler heap).
- Triangle fans above the cap are still not split (logged).
- Strip window overlap re-runs the VS for the repeated vertices; GS primitive ID after restarts is approximate.
- v10/v11 only through the existing prerast gating; v12+ unchanged.
