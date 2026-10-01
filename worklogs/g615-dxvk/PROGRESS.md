# G615 DXVK / vkd3d progress

Snapshot: 2026-10-01, beta.6 is current (`g615-v11-csf-v0.1.0-beta.6`, patches through 077). Device: Mali G615 (PAN_ARCH 11, gpu_id 0xb8a31030).

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

Patches exported and clean-applying: `patches/csf-v11/` up to `073` (069-073 on `work/mesa-p3` `dx-p3`); fresh pin + apply through 068 = `work/mesa-dxint` tree `81bdf03edb7`.
Details: `validation/g615-v11-csf/dxvk/DX9-TRANSFORM-FEEDBACK.md`, `DX10-TESSELLATION.md`.

Integration gaps: GS primitive drop; VMR (`vertexPipelineStoresAndAtomics` / FL11_1, worktree `dx-vpsa`); JICA98 0005 (same-queue semaphore waits) review; swapchain test (in progress elsewhere); `depthBounds`; `shaderOutputViewportIndex` (GS-written viewport index dropped); XFB intermittent DeviceLost; XFB 65536-record cap per non-tess draw; `robustImageAccess2` (vkd3d-proton device-create blocker).

## TODO (stopped 2026-10-01 00:45 IST)

### Device proof + JICA98 (~80%)
- JICA98 port merged into `dx6-dx7-base`; patches exported and clean-applying up to `csf-v11/064`.
- Stopped while applying a fixup to JICA98 0005 (same-queue semaphore waits on GPU, default-on, high risk). No uncommitted changes left in `work/mesa`.
- Next: finish the 0005 review (pending cross-subqueue waits, racy opt-out check, signal target coverage), run sync CTS; if it fails, make 0005 off by default.
- Still to run: full DEVICE-TEST-PLAN.md steps 1-7 results write-up and P23 matrix update.

### vertexPipelineStoresAndAtomics + tess gaps (~40%)
- Worktree `work/mesa-vpsa`, branch `dx-vpsa` on `01a54283d9b`.
- Uncommitted: 4 files, +76/-25 (`csf/panvk_gpu_prerast.c`, `csf/panvk_vX_cmd_draw.c`, `panvk_vX_physical_device.c`, `panvk_vX_shader.c`).
- CTS batches: first 168 and 198 passed with one device loss; latest single-case debug runs (`v3h`..`v3k`) still show 1 fail out of 2 in coherence-related variants (`cached_before_coherent`, `force_simultaneous`, `implicit_others_inv`).
- Not proven. Do not expose until tessellation CTS reruns with 0 fail.
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

1. Export vertex-stores work as patches on top of `csf-v11/073`; prove clean apply.
2. Feature bit: `shaderOutputViewportIndex`.
3. Wine (x86_64 under box64/FEX in the Alpine chroot), then first game test at FL11_0 / SM6.0.
4. SM6.2+ denorm control, Winlator packaging.
5. FL12_0 deferred. Sparse is NO-GO on kbase (sparse feasibility doc).
