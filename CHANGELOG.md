# Changelog

## g615-v11-csf-v0.1.0-beta.6 (prerelease)

Mesa `5a07217f034b` + csf-v11 patches up to 077
(`patchSeriesId sha256:a20c23f542ac54f50614f71093cba26fcfbe1b04d93652340d1fa44b26ea0a29`).
Poco X6 Pro, Mali-G615 MC6, mali_kbase CSF UAPI 1.21. Android minApi 35.

### Added
- `variableMultisampleRate` on v10+ (074).
- `sync_file` fence export through a kbase KCPU queue (075), eliminating
  reliance on `/dev/sw_sync` which is absent on this kernel and previously
  caused exports to misreport as out of memory (CTS `sync_fd`: 1996 pass / 60644 NotSupported / 0 fail,
  was 113 ResourceError).
- Test APK: swapchain lifecycle test and Vulkan 1.3 and 1.4 core requirement
  gap checks.

### Fixed
- Geometry-shader-written viewport index is now honoured for scissors and
  viewports (076), fixing draw scissor tests (18 fail -> 88/88 pass).
- System-scope subqueue sync signals on kbase (077), preventing missed signal
  wakeups across subqueues and eliminating timeouts in `signal_order` (11–16
  timeouts per run -> 1316 pass, 0 timeouts).

### Results
CTS: sync_fd and cross_instance 1996 pass / 60644 NotSupported / 0 fail (113 ResourceError resolved), draw
scissor 88/88 pass (18 failures resolved), signal_order 1316/0 (0 timeouts),
signal_order+basic 1357/0. Regression run: 0 failures across 15,955 cases in
geometry, tessellation, transform_feedback.simple, and variable_rate (6210
pass). On-device validation of the release APK on Mali-G615 MC6 (Poco X6 Pro):
10/10 in-app tests pass (gpu_prerast_slice, clip_cull, multi_viewport,
fill_mode, bc_decode, geometry, tessellation, xfb, pipeline_stats,
swapchain_lifecycle); Vulkan 1.3 and 1.4 core requirements are met, with no
DeviceLost or kbase faults.

### Known issues
Random `DeviceLost` (subqueue timeout) has not been observed in 4 runs since the
patch 077 fix, but is not yet proven completely fixed; system-scope signals
raise an interrupt per cross-subqueue signal, and the performance impact on
games remains unmeasured; sync_file export (075) uses one device-wide KCPU
queue, so a pending export can delay later exports (head-of-line blocking);
with wait-before-signal timeline usage this can in theory deadlock (not seen in
CTS); with a geometry-shader-selected viewport (076), depth clipping/clamping
uses the union of all viewports' depth ranges, not the selected viewport's
range (wrong only when viewports have different depth ranges); dEQP draw
`depth_bias_patch_list_tri_line` still fails (pre-existing); `depthBounds` is
not implemented; 2 intermittent `DeviceLost` occurrences remain in dEQP
`transform_feedback` `query_copy`; transform feedback is capped at 65,536
records per draw; the X11 present teardown hang was seen once under Xvfb only
and remains unverified on Android; JICA98-derived patch 0005 is not fully
validated; `robustImageAccess2` is missing (blocking vkd3d-proton device
creation; deferred); and sparse resources or FL 12_0 are impossible on Kbase.

## g615-v11-csf-v0.1.0-beta.5 (prerelease)

Mesa `5a07217f034b` + csf-v11 patches up to 073
(`patchSeriesId sha256:c6d62dc2a6085b581ca20e68b54d9bbe2846f30e9df8dc54b1b1ef1d3492956a`).
Poco X6 Pro, Mali-G615 MC6, mali_kbase CSF UAPI 1.21. Android minApi 35.

### Added
- `VK_EXT_memory_priority` and `VK_EXT_pageable_device_local_memory` (069).
- `alphaToOne` (070).
- `maxGeometryShaderInvocations` raised to 64 (071).
- `VK_EXT_multi_draw` (072).
- `VK_EXT_primitives_generated_query` (073).
- Test APK: native Info tab (device header card; collapsible instance/device
  extensions with filter, features by struct with "show only supported",
  limits table, texture-format flag chips). Raw JSON only via Copy/Share.

### Results
CTS: memory_priority 224/0, pageable 202/0, api.info 7799/0, alphaToOne 123/0,
geometry 193/0 (GS invocations 64), instanced 20/0, multi_draw 12704/0,
primitives_generated_query 75206/0. Regression geometry + tessellation +
transform_feedback.simple 5706/0. Test APK: 9/9 tests passed.

### Known issues
`depthBounds` and `shaderOutputViewportIndex` not implemented (GS-written
viewport index dropped; viewport 0 used); 2 intermittent DeviceLost in
transform_feedback query_copy; XFB 65536-record cap; X11 present hang seen
once under Xvfb only, unverified on Android; JICA98 0005 not fully validated;
no `robustImageAccess2` (vkd3d deferred); sparse/FL12 impossible on Kbase.

## g615-v11-csf-v0.1.0-beta.4 (prerelease)

Mesa `5a07217f034b` + csf-v11 patches up to 068
(`patchSeriesId sha256:e5faa5ee87fbba401cad6ead5dc49a346325defa368692fd296f441618d14e17`).
Poco X6 Pro, Mali-G615 MC6, mali_kbase CSF UAPI 1.21. Android minApi 35.

### Added
- GPU pre-raster path: VS foundation (018, 020, 021), geometry shaders (042, 048),
  follow-up fixes (044-047), tessellation (065), transform feedback (066, 068).
- BC1-7 GPU compute decode, `textureCompressionBC` (022, 039, 040).
- `shaderClipDistance`/`shaderCullDistance` (023), `multiViewport` (024),
  `fillModeNonSolid` (025, 047), `pipelineStatisticsQuery` (049-054).
- Upstream backports: `VK_KHR_incremental_present` (028),
  `VK_EXT_swapchain_colorspace` (029), `VK_EXT_image_compression_control`
  (035-037), AFBC/modifier caps (030-033), common 019.
- jica98-derived performance changes (056-063): cached memory budget,
  `cntfrq` timestamp frequency, skipped non-texel texture-cache invalidation,
  opt-in same-queue GPU semaphore waits, SSBO offset alignment 4, v11
  INTERSECT ZS preload, robust SSBO vectorizer (`PANVK_DEBUG=robust_ssbo_vec`).
- PanVK test APK (`apps/panvk-test`).
- Docs: `docs/RUN-PC-GAMES-ON-MALI.md`, `docs/plans/PANVK_GAME_LAUNCHER.md`.

### Fixed
- Zero-initialized query images, BC decode, and kbase BO pages (034, 040, 041).
- kbase tiler heap renewal (043).
- CRC init BO unmapped via `pan_kmod_bo_munmap` (CSF fault 0xc3) (038).
- Tiler geometry buffer padded by one page (067).
- CRC invalidated on CLEAR/DONT_CARE (019); FAU flush before indirect draw (026).
- Release packaging takes `mesaCommit` from `sources.lock`, not `work/mesa` HEAD.

### Results
DXVK Native v3.1.1 creates a D3D11 device at FL 11_0; D3D11 and D3D9 draw
workloads pass. CTS: geometry 189/0, tessellation 526/0, transform_feedback
15793/0 (2 intermittent DeviceLost), BC subset 1863/0, copy_and_blit 9620/0,
statistics_query 15374/0, fillModeNonSolid 17/17. Test APK: 9/9 tests passed
in 3 of 4 runs.

### Known issues
Intermittent DeviceLost in transform_feedback query_copy; X11 present
teardown hang; `sync_fd` emulated via `/dev/sw_sync`; no `robustImageAccess2`
(vkd3d-proton device create fails); no `vertexPipelineStoresAndAtomics`;
Wine path untested; test APK system-driver option broken.
