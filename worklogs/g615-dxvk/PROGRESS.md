# G615 DXVK / vkd3d progress

Snapshot: 2026-10-02, beta.7 is current (`g615-v11-csf-v0.1.0-beta.7`, patches through 082, vertexPipelineStoresAndAtomics). Device: Mali G615 (PAN_ARCH 11, gpu_id 0xb8a31030).

## Done and device-proven

| Area | Proof |
|---|---|
| DX0-DX4 base, contracts | host + device tests |
| DX5 GPU vertex shader (gpu_prerast) | 13/13 IDVS and prerast paths |
| BC1-7 GPU decode (default on) | CTS BC subset 1863 pass / 0 fail; copy_and_blit 9620 / 0 |
| Clip/cull distance, multiViewport, fillModeNonSolid | device matrices 0 fail |
| Zero-initialized memory | 408 pass / 0 fail |
| Tiler heap fix (043) | 380k render passes, 150k submits |
| Pipeline statistics queries (049-054) | CTS 14,098 pass / 0 fail |
| Upstream backports (incremental_present, swapchain_colorspace, image_compression_control) | device probes 0 fail |
| Tessellation + transform feedback integrated (`work/mesa-dxint` `dx-integrate` on e2fde360503, `csf-v11/065-068`) | matrices 0 fail (tess 24/24, xfb 17/17 incl. tes_capture); CTS tessellation 526/0, transform_feedback 15793/0 (133695 cases, 2 intermittent DeviceLost, pre-existing), geometry 189/0, conditional_rendering 922/0, statistics_query 15374/0, draw subset 3446/0; DXVK Native v3.1.1 FL 11_0 (`0xb000`) |
| `VK_EXT_memory_priority` + `VK_EXT_pageable_device_local_memory` (069) | CTS 224/0, 202/0; api.info 7799/0 |
| `alphaToOne` (070) | CTS 123/0 |
| `maxGeometryShaderInvocations` 64 (071) | geometry 193/0, instanced 20/0 |
| `VK_EXT_multi_draw` (072) | CTS 12704/0 |
| `VK_EXT_primitives_generated_query` (073) | CTS 75206/0 |
| `vertexPipelineStoresAndAtomics` on v10-v12 via gpu_prerast (078-082, `work/mesa-vpsa2-final` `dx-vpsa2-final`, 2026-10-02) | CTS atomic_operations `*_vertex*` 66/0 (was NotSupported), message_passing `*vert*` 320/0, shader_access `*vertex*` 2086/0, signal_order subset 1303/0; 12132-case tess/geometry/xfb.simple/draw list 7940/0, 0 DeviceLost (beta.6 p5-reg: 7619 Pass, 5 Fail, 1 DeviceLost; 315 tess cases newly supported); 65 newly enabled tess cases 5x 65/0; `quads_fractional_odd_spacing` 24/24; chroot vertex_stores 10/10, gpu_prerast_slice 0 fails (IDVS + prerast); APK autorun 11/11 x2 incl. new `vertex_stores` |

Patches exported and clean-applying: `patches/csf-v11/` up to `073` (069-073 on `work/mesa-p3` `dx-p3`); fresh pin + apply through 068 = `work/mesa-dxint` tree `81bdf03edb7`.
Details: `validation/g615-v11-csf/dxvk/DX9-TRANSFORM-FEEDBACK.md`, `DX10-TESSELLATION.md`.

Integration gaps: GS primitive drop; JICA98 0005 (same-queue semaphore waits) review; swapchain test (in progress elsewhere); `depthBounds`; `shaderOutputViewportIndex` (GS-written viewport index dropped); XFB intermittent DeviceLost; XFB 65536-record cap per non-tess draw; `robustImageAccess2` (vkd3d-proton device-create blocker).

## TODO (stopped 2026-10-01 00:45 IST)

### Device proof + JICA98 (~80%)
- JICA98 port merged into `dx6-dx7-base`; patches exported and clean-applying up to `csf-v11/064`.
- Stopped while applying a fixup to JICA98 0005 (same-queue semaphore waits on GPU, default-on, high risk). No uncommitted changes left in `work/mesa`.
- Next: finish the 0005 review (pending cross-subqueue waits, racy opt-out check, signal target coverage), run sync CTS; if it fails, make 0005 off by default.
- Still to run: full DEVICE-TEST-PLAN.md steps 1-7 results write-up and P23 matrix update.

### vertexPipelineStoresAndAtomics (done 2026-10-02, not released)
- Final: `work/mesa-vpsa2-final` branch `dx-vpsa2-final` (5 commits on `dx-p5`), exported as `patches/csf-v11/078-082`; fresh pin + apply = 83 patches, tree equal to the branch. Released in beta.7.
- Intermittent tess DeviceLost root cause: on kbase each subqueue is its own CSG. A CS blocked on SYNC_WAIT whose group gets evicted is re-evaluated by kbase on the CPU through a permanent kernel mapping, which only `BASE_MEM_CSF_EVENT` memory has. The tess ready/free words (per-draw `panlib_tess_draw`) and arena `available` words (arena header) were ordinary memory, so an evicted vertex/tiler group never resumed. Debugfs proof: group CSG NR -1, `SYNC_LIVE_VALUE 0xffffffffffffffff` (kbase's "no mapping" value, not an underflow). Fix 082: arena syncs in a device CSF-event BO, tess syncs in the per-queue syncobjs BO. Earlier tries (cache-line isolation, flush before signal) reverted; matching take scope kept in 082.
- Older worktrees `work/mesa-vpsa`, `mesa-vpsa2`, `mesa-vpsa2-atomic`, `mesa-vpsa2-tess` are superseded.
- Same latent class, not fixed: the render desc ringbuf syncobj (`init_render_desc_ringbuf`, rw pool) and VkEvent syncobjs (`panvk_vX_event.c`, rw_nc pool) are cross-subqueue SYNC_WAIT targets outside CSF event memory on kbase; an evicted waiter on them can hang the same way.
- Tess follow-ups (per-instance GS PrimitiveIdIn after TES, conditional rendering on the compute loop, exact prims-generated count) not started.


## TODO (stopped, resume later)

### vkd3d-proton native smoke (deferred)
- Deferred. vkd3d/D3D12 and FL12 stay deferred (sparse NO-GO on kbase).
- Scripts (untracked): `scripts/vkd3d/build-vkd3d-proton.sh` (chroot build with d3d12 tests), `scripts/vkd3d/run-d3d12-smoke.sh`, `tests/vkd3d/d3d12-smoke.list`.
- Build finished (`BUILD_OK` in scratchpad `vkd3d/build-host.log`), but the built `libvkd3d-proton-d3d12.so` and `tests/d3d12` binary are not in the scratchpad; they are likely in the device chroot.
- No device run yet (`run1.log` empty).
- Known hard blocker while deferred: `robustImageAccess2=false`. `VK_EXT_primitives_generated_query` landed in 073.

### X11 present teardown hang (~20%)
- Symptom: DXVK Native hangs in `destroySwapchain` waiting in `x11_wait_for_present` (see `validation/g615-v11-csf/dxvk/DX8-NATIVE-WORKLOAD.md`).
- Worktree `work/mesa-present`, branch `dx-present`, base e2fde360503.
- Uncommitted: `src/vulkan/wsi/wsi_common_x11.c` +7 lines, `X11DBG`-gated debug logging around present, completion, error and wait events. Debug only; do not ship.
- No root cause yet.
- Next: capture an `X11DBG` trace of the hang under Xvfb, find which present/idle event never arrives.

### robustImageAccess2 (~20%)
- Hard requirement for vkd3d-proton device creation (`panvk_vX_physical_device.c:631`).
- Worktree `work/mesa-ria2`, branch `dx-ria2`, base e2fde360503.
- Uncommitted: 2 files, +2/-2. Null image descriptors use the texture subdescriptor, and `robustImageAccess2` is enabled on arch 11+.
- Not proven. Do not expose until `dEQP-VK.robustness.robustness2.*` image/texel subsets and `dEQP-VK.robustness.image_robustness.*` pass with 0 fail.
- The agent was starting a build on the device when stopped; check for leftover processes on the device.

## Remaining after that

1. DXVK FL11_1 check on the beta.7 build (078-082 released in beta.7).
2. Feature bit: `shaderOutputViewportIndex`.
3. Wine (x86_64 under box64/FEX in the Alpine chroot), then first game test at FL11_0 / SM6.0.
4. SM6.2+ denorm control, Winlator packaging.
5. FL12_0 deferred. Sparse is NO-GO on kbase (sparse feasibility doc).

## Follow-ups after beta.6

- 075: Device-wide KCPU export FIFO causes head-of-line blocking; possible deadlock with wait-before-signal timeline usage. Fix: per-export KCPU queue or skip queue when payload already signalled.
- 075: If fence enqueue fails after wait enqueue succeeds, orphaned wait stays queued; add rollback/queue reset.
- 075: `kcpu_export.lock` not destroyed on `dev_create` failure paths.
- 075 (pre-existing): Re-export of pending permanently imported fence dups fd, reset drops original waiter without close.
- 076: GS-selected viewport uses union of all viewport depth ranges for clip/clamp; use selected viewport's range.
- DeviceLost: 0 in 4 CTS runs + beta.6 device test, not proven fixed; run ~20 stress runs of repro list in `tmp/HANDOFF-devicelost.md`.
- 077: Measure game perf cost of system-scope cross-subqueue signals.
- release.yml fails on manual releases (needs dist/ provenance files); beta.5 and beta.6 both failed same way.
