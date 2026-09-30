# VK_EXT_transform_feedback on Mali G615 (panvk v11, CSF)

Status: implemented on the GPU. The extension is exposed on PAN_ARCH >= 10. There is no CPU fallback.

## Branch

Worktree `work/mesa-xfb`, branch `dx9-xfb`, based on `d33a343037f`:

| Commit | Summary |
|---|---|
| `977b170ebb0` | panvk: implement transform feedback on the gpu_prerast path |
| `6c3c88b174a` | panvk: expose VK_EXT_transform_feedback and GS point size on v10+ |
| `6fe44a0a3e8` | panvk/csf: pad the tiler geometry buffer allocation by one page |

## Design

- **Capture.** Draws with active XFB take the gpu_prerast path, where the VS and GS run as compute and write vertex records into the device arena.
  - The libpan kernel `panlib_xfb_capture` runs as one workgroup of 256 lanes.
  - It assembles primitives for all topologies, including adjacency and restart.
  - It uses a local scan to compute output offsets and writes only whole primitives that fit. Overflowing primitives are counted, not written.
- **Geometry shader.** GS records carry a tag holding the stream and restart bit. All 4 vertex streams are captured, and the rasterization stream is selectable.
- **Counters.**
  - Begin and End move counter buffers with CS load and store on the compute subqueue.
  - `vkCmdDrawIndirectByteCountEXT` uses a GPU kernel that builds a `VkDrawIndirectCommand`.
- **Queries.** `VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT` stores written = min(total, fit) and needed = total. Copy results reuse the pipeline statistics copy kernel.
- **Conditional rendering** gates the capture through the `active` flag in the kernel parameters. Pause and resume are supported.
- **Clip and cull distances.** vk_nir merges cull distances into the clip array. panvk gathers them with remapped components and moves culls to `CULL_DIST0/1`.
- **Base driver fix.** The tiler geometry buffer is 64 KiB, placed directly after the tiler heap descriptor. Under heavy GS point-size geometry, the tiler touched the first page after it. This caused CSF fault 0xc3 at the end of the buffer and DeviceLost. One padding page is now allocated.

## Device proof (duchamp, Y5WWBMJVOZSK4HU8, final ICD sha256 214ad8d9...)

### Draw tests

The tests live in `tests/dxvk/vulkan/xfb/` and compare readback byte for byte. `XFB_FAILS=0`.
- Passing cases:
  - VS capture: triangle list and strip.
  - GS with 4 streams.
  - Pause/resume, overflow, DrawIndirectByteCount, queries.
  - VS and GS without a position output.
  - 4 VS adjacency topologies.
  - Clip/cull capture.
  - Conditional rendering with predicate 0 and 1.
- `tes_capture` is SKIPped because this base has no tessellation. On the integrated series (below) it runs: `CASE tes_capture counter=96 mismatch=0 PASS`.

## Integration with tessellation (2026-10-01)

Worktree `work/mesa-dxint`, branch `dx-integrate` on `e2fde360503`: `2013d31d27e` tessellation (`dx8-tess` squashed), `3546d8a9525` transform feedback, `1a0ba245c74` tiler padding, `846989a5081` expose. Exported as `csf-v11/065-068`; fresh pin + `apply-patches.sh --profile g615-v11-csf` gives the same tree (`81bdf03edb7`). ICD SHA-256 `c867f50f9faceaad...`.

Tessellated draws with active capture go through the nested gpu_prerast path; each tessellator index range is captured after TES (and GS). gpu_prerast ABI version 6.

Device (G615): matrices slice/clip/mvp/fill (IDVS + gpu_prerast), geometry, pipeline stats, BC, tessellation 24/24, xfb 17/17 `XFB_FAILS=0`, all 0 fail. DXVK Native D3D11 `feature_level=0xb000`.

CTS on the integrated ICD:

```
transform_feedback.*        cases=133695 Pass=15793 Fail=0 NotSupported=117900 DeviceLost=2
tessellation.*              cases=1103   Pass=526   Fail=0 NotSupported=577
geometry.*                  cases=199    Pass=189   Fail=0 NotSupported=10
conditional_rendering.*     cases=1030   Pass=922   Fail=0 NotSupported=108
query_pool.statistics_query cases=17769  Pass=15374 Fail=0 NotSupported=2395
draw subset (m12 list)      cases=3874   Pass=3446  Fail=0 NotSupported=428
```

The 2 DeviceLost are intermittent `query_copy_0_251_32bits*` cases. They also occur on the `dx9-xfb` ICD (`214ad8d9`): 12-case list x5, dx9-xfb 3/5 runs with one DeviceLost, integrated 1/5. Pre-existing, not root-caused.

### CTS dEQP-VK.transform_feedback.* (26044 cases, deqp tree order)

```
simple (query ptsz)  cases=3708 Pass=2304 Fail=0 NotSupported=1404 Crash=0 Timeout=0 DeviceLost=0
simple (rest)        cases=4187 Pass=2677 Fail=0 NotSupported=1510 Crash=0 Timeout=0 DeviceLost=0
other groups A       cases=9000 Pass=5763 Fail=0 NotSupported=3237 Crash=0 Timeout=0 DeviceLost=0
other groups B       cases=9149 Pass=5021 Fail=0 NotSupported=4128 Crash=0 Timeout=0 DeviceLost=0
total                cases=26044 Pass=15765 Fail=0 NotSupported=10279
```

The NotSupported results are truthful:
- more than 4 streams, more than 4 buffers, or data size above 512;
- float64;
- `VK_EXT_primitives_generated_query` (not exposed).

A 215-case sample of the primitives_generated_query groups was entirely NotSupported. The remaining ~107.6k cases in those groups were not run.

### Other CTS

```
dEQP-VK.conditional_rendering.*  cases=1030 Pass=922 Fail=0 NotSupported=108
dEQP-VK.geometry.* (list)        cases=199  Pass=183 Fail=6 NotSupported=10
```

The 6 geometry failures are the known layered cube-attachment cases. They are not related to XFB.

### Regression matrices

All matrices run both with and without `PANVK_DEBUG=gpu_prerast`, and all have 0 failures:

| Matrix | Result |
|---|---|
| slice | 13/13 |
| clip-cull | 13/13 |
| mvp | 5/5 |
| fill-mode | 17/17 |
| geometry | 23/23 |
| pipeline statistics | 12/12 |
| BC | `BC_DEVICE_FAILS=0` (16 formats) |

## Gaps

- **65536-record limit.** Capture is limited to 65536 records per draw. Larger or indirect draws are truncated, and there is no GPU chunking.
- **Oversized restart strips.** Restart strips beyond capacity are dropped.
- `VK_EXT_primitives_generated_query` is not implemented.
- **Intermittent DeviceLost** in `transform_feedback.*.query_copy_*` (also on the pre-integration branch).
- **Geometry buffer padding.** The padding is empirical. It does not explain why the tiler goes past the encoded 64 KiB size.
