# G615 DXVK / vkd3d progress

Snapshot: 2026-10-02, after beta.8 (Mesa 5a07217f + csf-v11 up to 084). Device: Mali G615 (PAN_ARCH 11, gpu_id 0xb8a31030).

Launcher scope correction (2026-10-02): ARM64EC DX8/9/10/11 clear + source
readback + X11 Present passes, including actual app path-only launches. Evidence:
`apps/panvk-launcher/tests/results/arm64ec-matrix/README.md` and sibling
`arm64ec-ui-d3dN/` bundles. SUPERSEDED (2026-10-03): normal no-readback
presentation now passes via DXVK `endCurrentPass(false)` fix (commit e738515);
see `apps/panvk-launcher/tests/results/final-discrimination/runtime-fix/README.md`.
Earlier black images were deferred DXVK clears, not a driver sync gap. Native profile/feature
results and earlier paired i686 runs do not establish real-game compatibility.
i686 WOW64 staging/SAME_VA failure remains unresolved; ARM64EC pass is not its fix.

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
| `variableMultisampleRate` on v10+ (074) | No-attachment passes split the render context when the sample count changes. CTS variable_rate + mixed_attachment_samples 504/0; no-attachment / dynamic_rendering subset 1756/0. Limit: render contexts inherited by secondary or other command buffers are not split. |
| `SYNC_FD` export via kbase KCPU queue (075) | CQS wait then fence signal. Root cause: `/dev/sw_sync` is absent on the GKI kernel and the failed open was reported as out of host memory. api.external sync_fd + synchronization.cross_instance: 113 ResourceError -> 1996 pass / 0 fail. |
| Honour geometry shader viewport index on v10+ (076) | Root cause: a GS-written viewport index was dropped, so every primitive used viewport 0 / scissor 0. draw scissor tests 18 fail -> 88/88. |
| System scope for subqueue sync signals on kbase (077) | Root cause: a blocked CS sync wait was not re-evaluated after a sibling's CSG-scope signal. synchronization.signal_order 11-16 timeouts per run -> 1316 pass / 0 aborted. Likely also fixed the random DeviceLost in renderpasses.dynamic_rendering (2 per run -> 0 in 4 runs of 54367 results; link not proven; handoff `tmp/HANDOFF-devicelost.md`). |
| GS draw drop (046) | Already in the tree as patch 046 (same as dx7-prerast-fix `7b6da4ee60f`); geometry 193/0. |
| JICA98 0005 GPU semaphore waits | Stays off by default (opt-in `PANVK_KBASE_GPU_SEMAPHORE_WAITS=1`). Grok review found 3 bugs: an empty submit skips its wait and signals an old seqno; the same-subqueue skip can drop a wait that was never queued; a failed wait leaves stale wait-table entries. Sync CTS with it off: 1881/0. |
| APK (`apps/panvk-test`, beta.6) | Info layout fix; honest conformance row ("Not Khronos-certified (driver reports 0.0.0.0)"); Vulkan 1.3 and 1.4 core required features both met; new swapchain_lifecycle test on the Android surface: 300 frames at 64-86 FPS, recreate + 120 frames at 60 FPS, 10x create/destroy in 1.66 s, no hang (2/2 runs, 10/10 tests pass). So the X11 present hang does not happen on the Android present path. |
| Repo cleanup (beta.6) | `.gitignore` junk removed, stale worktrees removed, `patchSeriesId` refreshed, `VALIDATION.json` points to DXVK evidence, `build-android.sh` picks matching host tools, `tests/dxvk-vkd3d` read `PANVK_MESA`, all 15 tests pass. |
| Regression on the beta.6 build | geometry, tessellation, transform_feedback.simple, multisample variable_rate 0 fail; draw 1/10 sample 1 fail (already failing before). |
| `vertexPipelineStoresAndAtomics` on v10-v12 (078, 080) | VS with SSBO stores/atomics runs on gpu_prerast; prerast arena allocated when the feature is on. CTS `atomic_operations *_vertex*` 66/0 (was 1/65 on WIP). APK `vertex_stores` test passes. Prerequisite for FL11_1. |
| Point-mode TES `gl_PointSize` (079) | Fixed; 315 tessellation cases moved NotSupported -> Pass. |
| IDVS flags from the bound VS variant (081) | Fixed. |
| Tessellation DeviceLost (082) | Root cause: on kbase each subqueue is its own CSG; an evicted waiting group is only re-checked if its sync word is in CSF event memory. Prerast arena and tess sync words moved to CSF event memory. Regression 12,132 cases: 7940 pass / 0 fail / 0 DeviceLost (beta.6: 7619 / 5 / 1). 5,613-case atomics/memory model/signal_order list: 3775/0. |
| X11 WSI in the Android ICD (083, 084) | `VK_KHR_xlib_surface` + `VK_KHR_xcb_surface`; X11/XCB libs dlopened from the caller's path. Software present (`PutImage`, no DRI3/MIT-SHM). 084: present id advances; `vkWaitForPresentKHR` timeout returns `VK_TIMEOUT`, not DeviceLost. Termux:X11: 1500 frames at ~343 fps, Xlib + XCB resize pass. |
| APK | 11/11 tests pass (incl. `vertex_stores`, `swapchain_lifecycle`). |

### After beta.8 (local `main`, not pushed or released; independently reviewed 2026-10-03)

| Patch | Change | Device proof | Review |
|---|---|---|---|
| 085 | Descriptor-ring and `VkEvent` sync words in CSF event memory, plus host notification | Host/GPU event replay passes; image readbacks are correct. The descriptor-ring wrap is claimed but not visible in the logs. | OK, minor issues: no v10-v12 gate (also reaches v13/v14), and `SetEvent`/`ResetEvent` can return `VK_ERROR_DEVICE_LOST`, which the spec doesn't allow there. Recovery of an evicted CSG via notification is unproven. |
| 087 | Conditional rendering honoured in the tessellation compute loop; replay-safe scratch | Predicates 0,1,0 give counters 3,9,3, and inverted 9,3,9 (direct, indirect and inherited); confirmed in `run.log` | **BLOCKING.** The whole prerast draw (including state emission) is inside a GPU `cs_if`, but the dirty flags are cleared when the command buffer is recorded. When the predicate is false, the next draw runs with stale FS/depth/query state (`panvk_vX_cmd_draw.c:~4900`). Fix: make only the draw conditional, or mark all state dirty after the loop. Then add a device test with a skipped conditional tess draw followed by a normal draw. |
| 088 | TES patch IDs passed to GS `PrimitiveIdIn`; loads from invalid invocations guarded | Seven readback `.bin` files decode to IDs 0-599. The 600-patch arena crossing fits the data but isn't logged. | OK, minor issue: the `PAN_ARCH >= 10` guard also covers v13/v14. |

Checks:
- Series: all 88 patches apply cleanly on a fresh pin (086 was removed as unsafe; the gap doesn't matter). The result is identical to `tmp/worktrees/mesa-val`.
- Device ICD: SHA256 `0457150b...34b98dc4` and BuildID `2afe54d4...0f7d` match the claims.
- CTS with that ICD: 438 cases (tessellation primitive_discard, sync basic events, conditional_rendering draw): 433 pass / 0 fail / 5 NotSupported. These cases don't exercise the 087 bug.
- 089 (depth clamp/clip per GS-selected viewport) is in the series (`patches/csf-v11/089-*.patch`) and verified on the G615 (`worklogs/driver-remaining/089-device-verification.md`). No CTS run.
- Review files: `tmp/review-085-088/`.

Released: beta.6 (up to 077), beta.7 (up to 082), beta.8 (up to 084). See `CHANGELOG.md`.
Details: `validation/g615-v11-csf/dxvk/DX9-TRANSFORM-FEEDBACK.md`, `DX10-TESSELLATION.md`, `tmp/HANDOFF-devicelost.md`.

## What's left for DXVK (driver)

1. **Fix the 087 stale-state bug** (blocking), then push 085/087/088.
1a. **32-bit (i686/WOW64) `vkMapMemory` at a caller-chosen address**. STATUS 2026-10-03: DEVICE-PROVEN for the draw test. mremap of the SAME_VA VMA is refused by kbase (`mremap ... failed: Invalid argument`, get_unmapped_area rejects fixed), so `kbase_kmod.c` `bo_mmap` now maps an anonymous MAP_FIXED shadow at the requested address, merged word-wise with the BO (snapshot) before every queue kick, after CSF waits, on flush/invalidate and on unmap. i686 D3D9 and D3D11 `dxdraw` now render triangle + textured quad (XGetImage, pixels identical to ARM64EC/x86_64; no MESA errors); 64-bit regression unchanged. Not proven: a real 32-bit game, large placed maps (merge is O(bytes) per kick/wait), CTS memory_map. Evidence: `apps/panvk-launcher/tests/results/samevaresults/README.md`.
2. 089: DONE, device-verified (GS-selected viewport depth clamp/clip as ordered runs). Follow-up: in `panvk_vX_cmd_draw.c`, the `cs_if(pred)` tessellation conditional skips prepare_draw's GPU state writes on a false predicate but still clears their dirty flags (from 087; same bug as item 1).
3. Re-check DXVK FL11_1 (VPSA is in since 078).
4. `shaderOutputViewportIndex` from VS/TES (`shaderOutputLayer` is already on; the GS part is done in 076).
5. `depthBounds`: exact check via tile-buffer stored depth.
6. Prerast limits: 65536 invocations per draw (`PANVK_GPU_PRERAST_MAX_INVOCATIONS`), topology and indirect limits.
7. `variableMultisampleRate` in secondary / inherited command buffers. The unsafe 086 was removed; this needs execution segmentation.
8. Exact tessellation primitives-generated count: code may be present; needs a targeted check.
9. X11 present: DRI3 / MIT-SHM path instead of CPU `PutImage`; FIFO is not vsync-paced.
10. Optional cleanups: v10-v12 gating for 085/087/088, and no `DEVICE_LOST` return from `SetEvent`/`ResetEvent`.

## Open problems

- 2 intermittent DeviceLost in `transform_feedback query_copy_*`: not rerun since 077/082; may be the same CSF event-memory issue.
- `draw.*depth_bias_patch_list_tri_line` fails (pre-existing, root cause unknown).
- 077 system-scope signals raise an interrupt per cross-subqueue signal; game perf cost unmeasured.
- 075 sync_file export uses one device-wide KCPU queue: head-of-line blocking; possible deadlock with wait-before-signal timelines (not seen in CTS).
- 076: fixed by 089 (depth clip/clamp used the union of all viewports' depth ranges).
- 089: one unreproduced intermittent on the first run after a fresh install (case B drew nothing; swapchain_lifecycle failed once in the same run). Not seen in 11 later gs runs and 6 swapchain runs.
- 085: recovery of an evicted CSG via host notification is unproven.
- Proton 11 (i686 via wow64): winex11 fails to create the Vulkan surface before the driver is called (HWND `0xc0000005`; ARM64EC window crash since fixed via `ANDROID_SYSVSHM_SERVER=/dev/null`, i686 SAME_VA mapping still open); DXVK reports "Presenter: Failed to create Vulkan surface". Instance and device creation work.
- **32-bit apps draw nothing (driver side, PanVK launcher report 2026-10-03).** On i686 under WOW64, `vkMapMemory` returns `VK_ERROR_MEMORY_MAP_FAILED`. Log: `MESA: error: kbase: mapping a BO at a caller-chosen address is not supported (SAME_VA)`. Source: `patches/kbase-common/files/src/panfrost/lib/kmod/kbase_kmod.c:1780`. Wine WOW64 needs mappings below 4 GiB (placed via `VK_EXT_map_memory_placed` / a fixed address), but kbase SAME_VA ties the CPU VA to the GPU VA. UPDATE 2026-10-03: fixed in `kbase_kmod.c` via shadow mapping (mremap refused by kernel); see item 1a.
  - Effects: D3D11 `CreateBuffer` fails with `E_INVALIDARG`, and the D3D9 process dies. Any 32-bit app with vertex buffers, index buffers or textures shows only the clear color. Clears need no mapping, so the 32-bit clear-only smoke tests pass.
  - 64-bit works: ARM64EC and x86_64 (FEX), D3D9 and D3D11 draw a triangle and a textured quad correctly in real X11 pixels (`XGetImage`). This holds with 1-3 buffers, FLIP_DISCARD/SEQUENTIAL, depth, 4xMSAA, and 60 unpaced frames.
  - Test: `tests/dxdraw.c`. Evidence: `apps/panvk-launcher/tests/results/draw-test/`. Launcher notes: `docs/plans/PANVK_GAME_LAUNCHER.md`. Likely related to issue #2 (GTA IV is 32-bit) and the Proton i686 "SAME_VA mapping" item.
  - Scope: this is a driver fix only. The launcher is owned by another agent.
- [Issue #2](https://github.com/zenithblue-oss/panvk-kbase-android/issues/2): GTA IV (D3D9 via DXVK, Wine wow64) stutters then freezes. The log shows thousands of DXVK `Failed to allocate staging buffer memory, res -2` (`VK_ERROR_OUT_OF_DEVICE_MEMORY`). The device-local heap is sized from system RAM by `os_get_gpu_heap_size()`. Possible causes: kbase allocation failure under RAM pressure, or a driver BO leak. Needs the driver version, device RAM, the full log, and a heap-usage trace.
- JICA98 0005: off by default, 3 known bugs (fix or drop).
- Tiler geometry buffer padding of one page found empirically; root cause unknown.
- Swapchain lifecycle test on the Android surface does not change the extent; only an `oldSwapchain` recreate is tested.
- P12 test expects 32 GS invocations until `PANVK_MESA` defaults to the newest tree.
- Tests regenerate the P13/P16/P18/P19/P21 reports on every run.
- `/tmp` is a 7.5G tmpfs; big CTS/build data goes to the repo `tmp/` (gitignored).

## Deferred (vkd3d out of scope for now)

### vkd3d-proton native smoke
Deferred (vkd3d/D3D12 and FL12 out of scope for now; sparse is NO-GO on kbase). Chroot build script exists (`scripts/vkd3d/build-vkd3d-proton.sh`) and built on host, but no device run yet. Blocked by `robustImageAccess2=false`.

### robustImageAccess2
Deferred (vkd3d out of scope for now). Hard requirement for vkd3d-proton device creation (`panvk_vX_physical_device.c:631`). WIP in `work/mesa-ria2` branch `dx-ria2` (+2/-2 null image descriptors via texture subdescriptor, enabled on arch 11+), not proven. Keep disabled until CTS `dEQP-VK.robustness.robustness2.*` image/texel and `image_robustness.*` pass with 0 fail.

### X11 present teardown hang
Seen once under Xvfb in the glibc chroot (`x11_wait_for_present` in `destroySwapchain`). Not reproduced on Android: `swapchain_lifecycle` passes, and the beta.8 Android X11 software present path (084) returns `VK_TIMEOUT` instead of hanging or DeviceLost.
