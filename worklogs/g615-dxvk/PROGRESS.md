# G615 DXVK / vkd3d progress

Snapshot: 2026-10-05, after beta.16 (Mesa 5a07217f + csf-v11 up to 107; Mali v9 JM experimental since beta.14). Device: Mali G615 (PAN_ARCH 11, gpu_id 0xb8a31030). The device is now being tested with Fallout 4 (no results yet; pre-driver hang notes in worklogs/driver-remaining/fo4-hang-before-driver.md).

Launcher scope correction (2026-10-02): ARM64EC DX8/9/10/11 clear + source
readback + X11 Present passes, including actual app path-only launches. Evidence:
`apps/panvk-launcher/tests/results/arm64ec-matrix/README.md` and sibling
`arm64ec-ui-d3dN/` bundles. SUPERSEDED (2026-10-03): normal no-readback
presentation now passes via DXVK `endCurrentPass(false)` fix (commit e738515);
see `apps/panvk-launcher/tests/results/final-discrimination/runtime-fix/README.md`.
Earlier black images were deferred DXVK clears, not a driver sync gap. Native profile/feature
results and earlier paired i686 runs do not establish real-game compatibility.
i686 WOW64 staging/SAME_VA failure was later fixed by 091 in beta.10; i686 games now run in PanPlay.

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
| JICA98 0005 GPU semaphore waits | Superseded by 103: GPU waits are default since beta.15; `PANVK_KBASE_GPU_SEMAPHORE_WAITS=0` restores CPU waits. Earlier opt-in implementation had 3 review bugs (empty submit, same-subqueue skip, stale wait table); CTS with it off: 1881/0. |
| APK (`apps/panvk-test`, beta.6) | Info layout fix; honest conformance row ("Not Khronos-certified (driver reports 0.0.0.0)"); Vulkan 1.3 and 1.4 core required features both met; new swapchain_lifecycle test on the Android surface: 300 frames at 64-86 FPS, recreate + 120 frames at 60 FPS, 10x create/destroy in 1.66 s, no hang (2/2 runs, 10/10 tests pass). So the X11 present hang does not happen on the Android present path. |
| Repo cleanup (beta.6) | `.gitignore` junk removed, stale worktrees removed, `patchSeriesId` refreshed, `VALIDATION.json` points to DXVK evidence, `build-android.sh` picks matching host tools, `tests/dxvk-vkd3d` read `PANVK_MESA`, all 15 tests pass. |
| Regression on the beta.6 build | geometry, tessellation, transform_feedback.simple, multisample variable_rate 0 fail; draw 1/10 sample 1 fail (already failing before). |
| `vertexPipelineStoresAndAtomics` on v10-v12 (078, 080) | VS with SSBO stores/atomics runs on gpu_prerast; prerast arena allocated when the feature is on. CTS `atomic_operations *_vertex*` 66/0 (was 1/65 on WIP). APK `vertex_stores` test passes. Prerequisite for FL11_1. |
| Point-mode TES `gl_PointSize` (079) | Fixed; 315 tessellation cases moved NotSupported -> Pass. |
| IDVS flags from the bound VS variant (081) | Fixed. |
| Tessellation DeviceLost (082) | Root cause: on kbase each subqueue is its own CSG; an evicted waiting group is only re-checked if its sync word is in CSF event memory. Prerast arena and tess sync words moved to CSF event memory. Regression 12,132 cases: 7940 pass / 0 fail / 0 DeviceLost (beta.6: 7619 / 5 / 1). 5,613-case atomics/memory model/signal_order list: 3775/0. |
| X11 WSI in the Android ICD (083, 084) | `VK_KHR_xlib_surface` + `VK_KHR_xcb_surface`; X11/XCB libs dlopened from the caller's path. Software present (`PutImage`, no DRI3/MIT-SHM). 084: present id advances; `vkWaitForPresentKHR` timeout returns `VK_TIMEOUT`, not DeviceLost. Termux:X11: 1500 frames at ~343 fps, Xlib + XCB resize pass. |
| APK | 17/17 tests pass after 096 (`autorun all` x2 and UI Run all), incl. `gs_viewport_depth`, `vs_viewport_index`, `depth_bounds`, `large_draw`, `vmr_secondary`, `tess_cond_state`. |
| Placed maps / i686 (091, beta.10) | dma-heap zero-copy maps replace beta.9 word-wise shadow merges; NFS Most Wanted 0.5 -> 39-66 fps, clean HUD/text; i686 D3D8-11 cubes 46-47 fps; memory/map_placed/basic sync CTS 4520/0/13 NotSupported. History: 32-bit blank draws / SAME_VA map failure fixed; `worklogs/driver-remaining/091-placed-dma-heap.md`. |
| Tessellation, prerast, VMR (093-096, beta.10) | State emission outside conditional rendering (093); GPU-chunked direct/indirect prerast draws (094); attachment-less VMR secondaries (095); parallel restart-strip planner (096). CTS 17067/0; APK 17/17. |
| GS-selected viewport depth (089) | Depth clamp/clip emitted as ordered viewport runs, device-verified (`worklogs/driver-remaining/089-device-verification.md`); fixes 076's union-of-depth-ranges bug. |
| VS/TES viewport index (090) and depthBounds (092) | Implemented on v10/v11, device-verified on G615, including 4x MSAA and no-FS depth bounds; `worklogs/driver-remaining/090-vs-viewport-index.md`, `092-depth-bounds.md`. |
| NFS memory / heap ops (097-098, beta.11) | Device-wide TLS and grow-on-fault prerast arenas (097); only VERTEX_TILER_STARTED heap ops on kbase (098). NFS memory blow-up and DEVICE_LOST fixed; gameplay RSS flat 2.0-2.1 GB over 10 minutes. Arena growth superseded by 100. |
| Universal ICD / driverInfo (beta.12-13, 099) | Android v10-v12 ICD in beta.12, Android + glibc v10-v14 in beta.13; 099 adds release tag to driverInfo. CS register fallback: 96 on v10/v11, 128 on v12+. G615 validated; other arches were built but untested at release. |
| Up-front prerast arenas (100, beta.13) | Fixes `gs_viewport_depth` case A (6/6); shared TLS stays. 160 MiB per VkDevice; PanProbe 17/17 x3. |
| Experimental v9 JM (beta.14, jm-v9 001-003) | JM kbase atom submission + v9 backend shipped; G57 tablet PanProbe 1/17, unchanged in beta.15/16. |
| CSF trace buffers (101, 105, beta.15) | 101 maps tracebuf at kbase-assigned VA; 105 resets CS trace buffers per submit. PanProbe passes with Mesa debug env; large trace logs can exhaust app memory. |
| Submit waits / heap creation (102-104, beta.15) | 102 renews tiler heap without graphics drain; 103 waits same-queue semaphores on GPU by default (`PANVK_KBASE_GPU_SEMAPHORE_WAITS=0` restores CPU waits); 104 creates next heap on a worker thread. NFS 24 -> 38-40 fps; PanProbe 17/17. |
| Software WSI / retired heaps (106-107, beta.16) | 106 moves present fence wait to X11 present thread (submit thread blocked 47% -> 0.2%); 107 allows up to three retired tiler heaps in flight. NFS race HUD 85.5 -> 90.6 fps, p99 22 -> 16 ms (second run 22 ms); PanProbe 17/17; CTS sync + memory gate: 56 known failures, same as beta.15. |

### After beta.8 (085-092 released in beta.9; 093-096 released in beta.10; reviewed 2026-10-03)

| Patch | Change | Device proof | Review |
|---|---|---|---|
| 085 | Descriptor-ring and `VkEvent` sync words in CSF event memory, plus host notification | Host/GPU event replay passes; image readbacks are correct. The descriptor-ring wrap is claimed but not visible in the logs. | OK, minor issues: no v10-v12 gate (also reaches v13/v14), and `SetEvent`/`ResetEvent` can return `VK_ERROR_DEVICE_LOST`, which the spec doesn't allow there. Recovery of an evicted CSG via notification is unproven. |
| 087 | Conditional rendering honoured in the tessellation compute loop; replay-safe scratch | Predicates 0,1,0 give counters 3,9,3, and inverted 9,3,9 (direct, indirect and inherited); confirmed in `run.log` | Was BLOCKING (state emission inside the GPU `cs_if` while dirty flags were cleared at record time, so a false predicate left the next draw with stale FS/depth/query state). Fixed by 093; proven by APK `tess_cond_state`. |
| 093 | Tessellation state emission no longer inside the conditional-rendering `cs_if` (fixes the 087 blocker) | CTS full list 17067 pass / 0 fail. APK `tess_cond_state` fails on 092 (chroot 3/3, APK 2/2) and passes with 093 (chroot 3/3, APK 2/2). Worklog `worklogs/driver-remaining/093-tess-state-emission.md` | Fixed. Lowering jobs and primitives-generated of a skipped conditional tess/chunked draw still run (pre-existing). |
| 094 | prerast draws chunked on the GPU, direct and indirect (no 65536-invocation cap) | APK `large_draw` 14 cases (XFB, GS, tess, restart strips, indirect, count, multi-indirect), CTS 17067 / 0. Worklog `worklogs/driver-remaining/094-prerast-chunking.md` | Restart-strip flake fixed by 096. |
| 095 | Attachment-less secondaries carry their sample count (VMR) | APK `vmr_secondary` 8/8, CTS 17067 / 0. Worklog `worklogs/driver-remaining/095-secondary-vmr.md` | Mixed counts in one secondary, cross-cmdbuf resume. |
| 096 | Restart-strip chunk planner scans with a 256-invocation workgroup (fixes the 094 flake) | chroot restart cases 250/250 (095: 6 of 127 failed), full `large_draw` 20/20, APK `large_draw` 12/12, `autorun all` 17/17 x2, UI Run all 17/17 (`validation/driver-remaining/096-device/`), CTS 36144-case list 17067 / 0 / 0 DeviceLost. Worklog `worklogs/driver-remaining/096-parallel-chunk-planner.md` | Root cause: long single-invocation planner job while the vertex/tiler CSG waits across CSGs. The kbase/firmware behaviour itself stays. |
| 088 | TES patch IDs passed to GS `PrimitiveIdIn`; loads from invalid invocations guarded | Seven readback `.bin` files decode to IDs 0-599. The 600-patch arena crossing fits the data but isn't logged. | OK, minor issue: the `PAN_ARCH >= 10` guard also covers v13/v14. |

Checks:
- Series: all 95 committed patches (001-096, no 086 or 091) apply cleanly on a fresh pin with no fuzz (CI run 37112739575 green). 086 was removed as unsafe. 091 is now `allocate-placeable-host-memory-from-the-dma-heap` (beta.10; the earlier `sync-placed-map-shadows` 091 was dropped). 075 was regenerated against the shadow `kbase_kmod.c` (commit 848ca8c).
- Device ICD: SHA256 `0457150b...34b98dc4` and BuildID `2afe54d4...0f7d` match the claims.
- CTS with that ICD: 438 cases (tessellation primitive_discard, sync basic events, conditional_rendering draw): 433 pass / 0 fail / 5 NotSupported. These cases don't exercise the 087 bug.
- 089 (depth clamp/clip per GS-selected viewport) is in the series (`patches/csf-v11/089-*.patch`) and verified on the G615 (`worklogs/driver-remaining/089-device-verification.md`). No CTS run.
- Review files: `tmp/review-085-088/`.

Released: beta.6 (up to 077), beta.7 (up to 082), beta.8 (up to 084), beta.9 (up to 092, tag `g615-v11-csf-v0.1.0-beta.9`), beta.10 (up to 096 including the new 091 dma-heap placed maps, tag `g615-v11-csf-v0.1.0-beta.10`), beta.11 (up to 098: device-wide TLS and grow-on-fault prerast arenas (097), only VERTEX_TILER_STARTED heap ops on kbase (098); fixes NFS:MW memory blow-up and DEVICE_LOST; tag `g615-v11-csf-v0.1.0-beta.11`, bundled in PanPlay 1.0.3), beta.12 (universal v10/v11/v12 Android ICD, v10/v12 built but untested; kbase CS register-count fallback; tag `g615-v11-csf-v0.1.0-beta.12`), beta.15 (prerelease, up to 105: heap renewal without CPU drain (102), GPU-side same-queue semaphore waits (103), worker-thread heap creation (104), kbase trace reset (105), tracebuf VA fix (101); NFS MW 24 -> 38-40 fps; tag `g615-v11-csf-v0.1.0-beta.15`), beta.14 (prerelease, experimental v9 JM; tag `g615-v11-csf-v0.1.0-beta.14`), beta.13 (prerelease, up to 100: universal v10-v14 Android and glibc ICDs; only G615 v11 tested; release name in driverInfo (099); up-front prerast arenas fix gs_viewport_depth case A (100), shared TLS stays; v12+ CS register fallback is 128; PanProbe 17/17 x3, CTS memory + sync 4520/0/13 NotSupported, CTS geometry + clipping + viewport: geometry 195 pass / 0 fail / 4 NotSupported; pipeline and dynamic-state viewport cases 177 pass / 0 fail; clipping 180 pass / 128 fail; draw `shader_viewport_index` 328 pass / 60 fail / 6 NotSupported. All 188 failures (user clip/cull distances through GS or tessellation, and `shader_viewport_index.fragment_shader_2..16`) fail the same way on the beta.10 and beta.12 binaries, so they are old bugs, not regressions.; PanPlay game tests skipped, user tests PanPlay; tag `g615-v11-csf-v0.1.0-beta.13`), beta.16 (prerelease, up to 107: software WSI present fence wait on the X11 present thread (106), up to three retired tiler heaps in flight (107); NFS race HUD 85.5 -> 90.6 fps, p99 22 -> 16 ms; PanProbe 17/17; CTS sync + memory gate has the same 56 known failures as beta.15; tag `g615-v11-csf-v0.1.0-beta.16`). See `CHANGELOG.md`.
Details: `validation/g615-v11-csf/dxvk/DX9-TRANSFORM-FEEDBACK.md`, `DX10-TESSELLATION.md`, `tmp/HANDOFF-devicelost.md`.

## What's left for DXVK (driver)

3. Re-check DXVK FL11_1 (VPSA is in since 078).
4. `shaderOutputViewportIndex`: v12+ keeps the bit off (no run splitting; see item 16). FS `gl_PrimitiveID` still restarts per viewport run.
5. `depthBounds`: v10/v11 emulation loses FPK; EarlyFragmentTests shaders with depth writes see the new depth. v12+ keeps the bit off (see item 16).
6. Prerast limits: triangle fans over the cap still unsplit; chunks hold at most 2048 instances (G615 hangs on larger grids); chunk draws skipped under rasterizer discard; GS primitive ID after restarts approximate.
7. `variableMultisampleRate`: mixed sample counts inside one secondary use the first count (warning logged); contexts resumed across command buffers remain open. The unsafe 086 stays removed.
8. Exact tessellation primitives-generated count: code may be present; needs a targeted check.
9. X11 present: DRI3 / MIT-SHM path instead of CPU `PutImage`; FIFO is not vsync-paced. Present fence wait moved off the submit thread in 106.
10. Optional cleanups: v10-v12 gating for 085/087/088, and no `DEVICE_LOST` return from `SetEvent`/`ResetEvent`.
11. **Next driver task: asynchronous kbase queue submission. STATUS: NOT STARTED.**
  - Beta.15/16 removed the CPU waits around submit (102-107); NFS is now bound by the game main thread / wineserver IPC. The ring kick already returns immediately; asynchronous submission work remains to be scoped.
12. **Mali v9 (Valhall JM, G57/G68/G77/G78): experimental since beta.14, finish to PanProbe 17/17.**
  - G57 MC2 tablet (kbase JM 11.0, gpu_id 0x90930010): PanProbe 1/17 on beta.14/15/16. DXVK rejects Vulkan 1.1; needs Vulkan 1.3 + GS, multiViewport, fillModeNonSolid and BC. Details: [docs/universal/v9-jm/README.md](../../docs/universal/v9-jm/README.md); earlier tests: `worklogs/driver-remaining/101-v9-jm-test-driver.md`, `validation/v9-jm/` (CTS smoke 4/6, mapping 102/102, basic sync 21 pass / 8 NotSupported, simple draw 4/4).
  - JM replay: DATA_INVALID_FAULT (event 0x58) in `gpu_prerast_slice` "replay" (same command buffer submitted twice). Suspected GPU-written malloc-job payload not restored; 4-8 h diagnosis + 1-2 days fix. VMR secondaries count only sample 0, not samples 1-3; update `fb.nr_samples` / `evaluate_per_sample` (1-2 days + mixed-rate validation).
  - Feature work: audit/report Vulkan 1.3 (1-3 days); port compute pre-raster to JM job chains for GS, vertex stores, multi-viewport, clip/cull, fill mode and large draws (15-30 working days); tessellation (5-15 days), XFB (3-7 days), pipeline stats, depth bounds and tess cond state still open. Earlier phase estimates were too small; timestamps report zero valid bits, and API 1.1 hides driverInfo properties.
  - BC, mapper and EXEC_INIT gaps are items 14, 15 and 17. Risks: GPU-side indirect job patching, replay correctness, fork maturity and one test device; full PanProbe 17/17 remains the gate.
13. **Upload zip log gaps (apps, next app release).** Status: TODO (found 2026-10-05 by checking upload ids 28/29, 1.2.2).
  - PanProbe `logcat.txt` is only ~1.6 KB (probably filtered to PanProbe lines). It misses system/driver/lowmemorykiller lines around crashes. Capture a wider logcat window (all tags, time-bounded to the run, size-capped). PanPlay's is ~67 KB for comparison.
  - The PanPlay zip has no separate DXVK logs (`d3d9.log`, `d3d11.log`, `dxgi.log`); DXVK output is maybe only in `wine-run.log`. Set `DXVK_LOG_PATH` to the session folder and include those files in the zip and manifest.
  - Keep zips under the 25 MiB own-storage cap where possible: cap and truncate large logs, and keep the full ones only when they fit.
  - Record kbase uAPI version, `TEXTURE_FEATURES` and the gralloc mapper init line in uploads. Fix PanProbe runner SIGBUS in `bc_decode` when no Mali device is present.

New driver gaps from tester data ([docs/universal/README.md](../../docs/universal/README.md)); file:line references below use the beta.16 Mesa tree.

14. **BC emulation decision (v9/v10).** Native BC1 reported by kbase disables emulation, leaving `textureCompressionBC=false`; DXVK rejects G610. The partial native mask is inferred (uploads omit `TEXTURE_FEATURES`).
  - `src/panfrost/vulkan/panvk_physical_device.c:1815`, `:1837`; `src/panfrost/vulkan/panvk_vX_physical_device.c:294`, `:338`. Emulate unless full native BC mask is present; effort 4-8 h incl. validation. [Index](../../docs/universal/README.md).
15. **Vendor-neutral gralloc mapper.** Hard-coded MediaTek stable-C mapper5 causes `vkCreateSwapchainKHR` / AHB import `VK_ERROR_INVALID_EXTERNAL_HANDLE` on stock ROMs and vendor API < 34; Pixel/Tensor reports are consistent with this gap (cause not yet proven).
  - `src/util/u_gralloc/u_gralloc_fallback.c:83`, `:427`; `patches/android/013-vendor-mapper-metadata.patch`; `src/vulkan/runtime/vk_android.c:152`. Vendor-neutral discovery + mapper4: 24-48 h; quick configurable mapper names: 4-8 h (does not cover mapper4-only vendors). [Index](../../docs/universal/README.md).
16. **v12+ viewport depth and feature gates.** `depthBounds` and `shaderOutputViewportIndex` require `PAN_ARCH < 12`; per-viewport depth runs are absent. G720 MC7 PanProbe 14/17 (fails gs_viewport_depth, vs_viewport_index, depth_bounds).
  - `src/panfrost/vulkan/panvk_vX_physical_device.c:330`, `:443`; `src/panfrost/vulkan/csf/panvk_vX_cmd_draw.c:4388`, `:4502`, `:4709`. Effort: depth runs 16-32 h + viewport index 4-8 h + depth bounds 8-16 h. [Index](../../docs/universal/README.md).
17. **EXEC_INIT ordering.** `KBASE_IOCTL_MEM_EXEC_INIT` runs after `JIT_INIT`, producing EPERM on v9 JM and old v10 CSF; v9 shaders still execute, v10 shader allocation is untested.
  - `src/panfrost/lib/kmod/kbase_kmod.c:1391-1410`: initialize EXEC before JIT where needed and size the JM zone; effort 6-12 h incl. old/new CSF and JM checks. [Index](../../docs/universal/README.md).
18. **Exercise v10 old-CSF device / queue / heap paths.** G610 only reached physical-device enumeration; `vkCreateDevice`, queue-group creation, heap init and shader submission were never exercised. Need a G610 PanProbe run with logcat.
  - Check 112-byte / 32-byte / missing 40-byte queue-group layouts, legacy heap init and EXEC zone (`src/panfrost/lib/kmod/kbase_kmod.c:1391-1410`). Effort: tester verification, 0 code effort until failures are known. [Index](../../docs/universal/README.md).

## Open problems

- Tester data (27 uploads, 2026-10-05): v9 G57 1/17 experimental; v10 G610 enumerates but DXVK rejects (BC); v11 G615 MC2/MC6 stock ROMs 16/17 (swapchain mapper), dev device 17/17; v12 G720 MC7 14/17; v13/v14 no data. No `Unknown gpu_id` in uploads; G720 MC7 = `0xc8700010` recognised. Still-unseen gpu_ids: G710, G510, G310, G620, Immortalis-G720, G625, G725, Immortalis-G925, G1 family. Beta.16 results come from CHANGELOG; no beta.16 uploads yet. [docs/universal/README.md](../../docs/universal/README.md).

- CTS (beta.13, G615): 188 old failures, same on beta.10 and beta.12: `clipping.user_defined.clip_distance*`/`clip_cull_distance*` through GS or tessellation (128), and `draw.*shader_viewport_index.fragment_shader_2..16` (60). 16 of the clip cases pass when run alone, so some state leaks between cases. Evidence: `validation/driver-remaining/beta13-device/`.
- kbase CSF interface gaps for newer kernels: the 40-byte 1.18 queue-group create layout is not tried; the 24-byte tiler heap init layout is not used (legacy 16-byte layout used); 16 KiB pages are not supported (4 KiB assumed). Unverified without hardware; see [docs/universal/v10/README.md](../../docs/universal/v10/README.md).

- 2 intermittent DeviceLost in `transform_feedback query_copy_*`: not seen in the 096 CTS run (36144-case list, 0 DeviceLost); keep watching.
- kbase/firmware: a long compute job while the vertex/tiler CSG waits on another CSG can get that group killed (096 root cause). 096 shortens the planner; other long single-workgroup jobs in that position could still hit it. Likely also behind the 2048-instance chunk cap.
- `draw.*depth_bias_patch_list_tri_line` fails (pre-existing, root cause unknown).
- 077 system-scope signals raise an interrupt per cross-subqueue signal; game perf cost unmeasured.
- 075 sync_file export uses one device-wide KCPU queue: head-of-line blocking; possible deadlock with wait-before-signal timelines (not seen in CTS).
- 089: one unreproduced intermittent on the first run after a fresh install (case B drew nothing; swapchain_lifecycle failed once in the same run). Not seen in 11 later gs runs and 6 swapchain runs. `swapchain_lifecycle` also failed once each during 090 and 092 run-alls, then passed on rerun.
- 085: recovery of an evicted CSG via host notification is unproven.
- [Issue #2](https://github.com/zenithblue-oss/panvk-kbase-android/issues/2): GTA IV (D3D9 via DXVK, Wine wow64) stutters then freezes. The log shows thousands of DXVK `Failed to allocate staging buffer memory, res -2` (`VK_ERROR_OUT_OF_DEVICE_MEMORY`). The device-local heap is sized from system RAM by `os_get_gpu_heap_size()`. Possible causes: kbase allocation failure under RAM pressure, or a driver BO leak. Needs the driver version, device RAM, the full log, and a heap-usage trace.
- JICA98 0005: earlier 3 review bugs retained in the done-table history; superseded by 103 (GPU waits default since beta.15).
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
