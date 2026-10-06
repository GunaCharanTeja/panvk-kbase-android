# Changelog

## Mali-G615 CSF PanVK Driver with VKD3D DX12 Support (Patch 108)

Mesa `5a07217f034b` + series up to Patch 108 (`108-vkd3d-dx12-essentials.patch`).
Targeted on OPPO CPH2763 (MediaTek MT6878), ARM Mali-G615 MC2 (Valhall v11 CSF).

### Highlights & Changes:
- **`VK_EXT_descriptor_buffer`**: Implemented direct hardware descriptor buffers, eliminating descriptor pool allocation overhead for D3D12 root signatures and descriptor tables (CBV, SRV, UAV, samplers). Packed descriptors emitted straight into Mali Valhall hardware resource tables.
- **`VK_EXT_device_generated_commands`**: Vulkan DGC hooks for GPU-driven drawing batches (`ExecuteIndirect`).
- **D3D12 Placed Resource Memory Isolation**: Restricted CRC tile checksum metadata buffers strictly to WSI swapchain images (`panvk_image.c`). Non-WSI images (render targets, placed textures, G-buffers) strictly preserve power-of-two memory sizes, eliminating `d3d12_resource_create_placed` out-of-memory crashes in VKD3D-Proton.
- **`VK_KHR_robustness2`**: Exposed `robustImageAccess2 = true`, completing full robustness coverage (`robustBufferAccess2`, `nullDescriptor`).
- **Clean Tree Rebase**: Synchronized with latest Mesa upstream and cleaned out legacy launcher APK artifacts.

## g615-v11-csf-v0.1.0-beta.16 (prerelease)

Mesa `5a07217f034b` + the series up to 107. Tested on Poco X6 Pro, Mali-G615 MC6 (v11),
mali_kbase CSF UAPI 1.21. Mali v9 stays experimental (unchanged).

### Performance (Need for Speed: Most Wanted in PanPlay, race window, DXVK HUD full)

The beta.15 save now finishes the prologue race, so the beta.15 numbers below were re-measured on
the same race window with the released beta.15 `.so`. Builds were run interleaved, 2 runs each.

| Build | DXVK HUD fps (4 samples x 2 runs) | mean | p99 frame time | peak GPU memory |
|---|---|---|---|---|
| beta.15 | 81.7 91.9 82.7 91.4 / 81.5 78.7 85.6 90.4 | 85.5 | 22 / 22 ms | 640 / 643 MB |
| beta.16 | 85.3 95.8 91.7 93.4 / 88.4 93.1 81.5 95.2 | 90.6 | 16 / 22 ms | 652 / 666 MB |

Driver-counted presents (instrumented builds): 88.7 -> ~92 fps. The submit thread was blocked
47% of the time on beta.15 and is now blocked 0.2%.

### Fixed

- Software WSI present (no dma-buf/DRI3, the launcher's X server) no longer blocks
  `vkQueuePresentKHR` until the frame's GPU work finishes (106). The X11 present thread
  waits for the image fence instead. Before, DXVK's submit thread could not queue the next
  frame, so CPU and GPU never overlapped.
- Up to three retired tiler heaps can be in flight (107). With only one, heap renewal
  often fell back to a full graphics drain inside `vkQueueSubmit` (35-55 ms stalls). If
  backpressure is still reached, the wait is only for the oldest retired heap.
- `driverInfo` reads `PanVK-kbase beta.16`.

### Notes

- GPU busy: the MediaTek GED node (`/sys/kernel/ged/hal/gpu_utilization`, ~96%) counts the
  time a CSG is resident, including waits. The engine split in `/proc/mtk_mali/gpu_utilization`
  and the DXVK HUD (`GPU: 57-66%`) show the real load. GPU timestamps per ring entry show the
  GPU idle ~32% of the frame, almost all with no work submitted.
- NFS is now limited by the game's main thread and wineserver round trips (~48% of its time in
  `pipe_read`). More fps here needs work on the Wine side, not on the GPU.

### Validation

- CTS (glibc build, same series): the 11331-case sync + memory gate set has the same 56 failures
  as beta.15. The wider 66,796-case sync/memory/query run has 52 new and 26 fixed failures vs beta.15,
  all `write_*_geometry_*` op tests. A rerun of those 78 cases fails 36 on beta.16 and 50 on beta.15, so
  they are flaky on both builds, not a regression.
- PanProbe on the Poco: 17/17 (Mesa debug env off).
- v9 tablet (Mali-G57 MC2): PanProbe 1/17, unchanged.

## g615-v11-csf-v0.1.0-beta.15 (prerelease)

Mesa `5a07217f034b` + the series up to 105. Tested on Poco X6 Pro, Mali-G615 MC6 (v11),
mali_kbase CSF UAPI 1.21. Mali v9 stays experimental (unchanged from beta.14).

### Performance (Need for Speed: Most Wanted in PanPlay, gameplay window, HUD shows `PanVK-kbase beta.15`)

| Build (2 runs each) | fps | p50 / p99 frame time | frames > 50 ms | frames > 100 ms | GPU busy / clock |
|---|---|---|---|---|---|
| beta.14 | 24.3 / 23.6 | 33 / 102 ms | 634 / 663 | 34 / 44 | 94% / ~840 MHz |
| beta.15 | 38.3 / 37.0 (40.1 on the final .so) | 22-24 / 58 ms | 77 / 83 (84) | 1 / 1 (1) | ~96% / ~1310 MHz |

### Fixed

- Tiler heap renewal no longer drains the graphics subqueues on the CPU (102).
  It ran on almost every submit (~5700 renewals in 140 s, 15 ms each). Two
  TILER_HEAP descriptor slots let in-flight work keep the retired heap; a
  bounded drain happens only if the retired heap stays busy for 4x the budget.
- Same-queue binary semaphores are waited for on the GPU by default (103).
  The CPU wait cost ~21 ms about once per frame. `PANVK_KBASE_GPU_SEMAPHORE_WAITS=0`
  restores CPU waits.
- The next tiler heap context is created on a worker thread (104), taking
  ~7 ms of `KBASE_IOCTL_CS_TILER_HEAP_INIT` off `vkQueueSubmit` ~12 times a second.
- `PANVK_DEBUG=trace` on kbase: the CS trace buffers are now reset before each
  submit (105). Before, every submit re-decoded all earlier traces (a 700 MB
  log for one PanProbe test).
- The CSF tracebuf is mapped at a kbase-assigned VA (101), fixing PanProbe with
  the Mesa debug env.
- `driverInfo` reads `PanVK-kbase beta.15`.

### Notes

- kbase submission already returned right after the ring kick; the remaining
  NFS limit is GPU time (~96% busy at ~1310 MHz). More fps needs GPU-side work (beta.16).
- Dropped: allocating the 160 MiB prerast arenas lazily. It saved ~480 MiB of GPU
  memory in NFS (3 VkDevices) but cost ~3 fps in interleaved runs.

### Validation

- CTS (glibc build of the same series): synchronization basic/smoke/timeline/implicit
  + memory, 11331 cases: 11257 pass / 56 fail / 18 not supported; the 56 fail on beta.14 too.
  The wider 66,796-case sync/memory/query run on 102+103: 0 new failures vs beta.14.
- PanProbe 17/17 on the Poco with the Mesa debug env off; with it on, all tests pass
  (the PanProbe app itself can run out of memory reading very large trace logs).
- v9 tablet (Mali-G57 MC2): PanProbe 1/17, unchanged.

### Known issues

- Fallout 4 (GOG GOTY, x86_64) hangs before it loads any graphics driver: black
  screen, no DXVK log. Same on beta.14. Outside the driver; see
  `worklogs/driver-remaining/fo4-hang-before-driver.md`.
- Mali v9 is experimental and partly broken. v10/v12/v13/v14 are untested.

## g615-v11-csf-v0.1.0-beta.13 (prerelease)

Mesa `5a07217f034b` + the series up to 100
(`patchSeriesId sha256:b85576a0bebec3b56c9de083a6afd2ff8c26feafece2a844079b7bca254e3d9c`).
Tested only on Poco X6 Pro, Mali-G615 MC6 (v11), mali_kbase CSF UAPI 1.21. Android minApi 35.

### Added

- Universal ICD for v10-v14. Both Android and glibc `.so` files contain
  `libpanvk_v10` through `libpanvk_v14`. The kbase device path now admits v14
  without `PAN_I_WANT_A_BROKEN_VULKAN_DRIVER` (004). v13 was already admitted.
  **v10/v12/v13/v14 are built but untested** (no hardware).
- Mesa recognises G610, G310 (v10), G615 (tested), G715 (v11), G720 variant 4
  (v12), G725 variant 4 (v13), G1-Ultra (14.8.0 v4), G1-Premium (14.8.1 v4)
  and G1-Pro (14.8.3 v1/v4) (v14). G710, G510 (v10), G620, Immortalis-G720
  (v12), G625 and Immortalis-G925 (v13) still need a tester's gpu_id.
- `driverInfo` is now `PanVK-kbase beta.13 (Mesa 26.3.0-devel (git-5a07217f03))`
  (new 099). It is visible in the DXVK HUD Version line and PanProbe.

### Fixed

- PanProbe `gs_viewport_depth` case A failed since beta.11. All three cases
  passed on beta.9/beta.10. On-device bisect: reverting 098 did not help;
  reverting 097 fixed it. Dropping only `PAN_KMOD_BO_FLAG_ALLOC_ON_FAULT` from
  the `gpu_prerast` arenas fixed it in 6/6 runs. GPU-fault growth on MediaTek
  kbase lost the first `gpu_prerast` (GS) draw's output; nothing was drawn.
  New 100 commits the arenas up front on kbase. The shared-TLS fix from 097
  stays. Arenas cost 160 MiB per VkDevice again. Upgrade path:
  `KBASE_IOCTL_MEM_COMMIT` on first use.
- Implausible kbase CS work-register counts now fall back to 128 on v12+,
  or 96 on v10/v11. 96 was too small for v12+ streams, which use registers
  up to 123. G615 reports a plausible value and logs no warning.

### Known issues

- v10/v12/v13/v14 are untested. Firmware interface differences are not
  validated per arch.
- Old kbase may lack `GET_CPU_GPU_TIMEINFO`: timestamp queries read 0.
- 4 KiB pages assumed. Queue-group create tries the 112-byte layout on
  uAPI >= 1.25, then the 1.6 32-byte layout. The 40-byte 1.18 layout is not
  tried. Tiler heap init uses the legacy 16-byte layout.
- Queue submission on kbase is synchronous.

### Validation (G615)

- PanProbe: 17/17 pass, three runs in a row. `gs_viewport_depth` A/B/C,
  `vs_viewport_index` and `depth_bounds` pass. Info: Mali-G615 MC6, Vulkan
  1.4.363, driver `PanVK-kbase beta.13`, 188 extensions, GPU id `0xB8A31030` (v11).
- CTS `memory.mapping.*`, `memory.map_placed.*`, `synchronization{,2}.basic.*`:
  4520 pass / 0 fail / 13 NotSupported, same as beta.12.
- CTS geometry + clipping + viewport subsets: geometry 195 pass / 0 fail / 4 NotSupported; pipeline and dynamic-state viewport cases 177 pass / 0 fail; clipping 180 pass / 128 fail; draw `shader_viewport_index` 328 pass / 60 fail / 6 NotSupported. All 188 failures (user clip/cull distances through GS or tessellation, and `shader_viewport_index.fragment_shader_2..16`) fail the same way on the beta.10 and beta.12 binaries, so they are old bugs, not regressions.
- PanPlay game tests skipped this time. The user tests PanPlay.

## g615-v11-csf-v0.1.0-beta.12 (prerelease)

Mesa `5a07217f034b` + the series up to 098
(`patchSeriesId sha256:0dda167ab137168856d453724821eebc044213b57832c4cbc393cb9f2bf4e868`).
Tested only on Poco X6 Pro, Mali-G615 MC6 (v11), mali_kbase CSF UAPI 1.21. Android minApi 35.

### Added
- One universal Android ICD for v10, v11 and v12. The per-arch backends
  (`libpanvk_v10`, `v11`, `v12`) were already linked into the `.so`, and the kbase
  device path already admitted v10-v13. **v10 and v12 are built but untested**
  (no hardware). GPUs recognised by the Mesa model table: G610, G310 (v10), G615,
  G715 (v11), G720 variant 4 (v12). G710, G510, G620 and Immortalis-G720 fail with
  `Unknown gpu_id` until a tester reports their gpu_id and variant.

### Fixed
- kbase: the CS work-register count reported by the firmware is used only when it
  is 96 or 128; anything else falls back to 96 with a warning. A Pixel 7 (G710)
  reports a bad value, and 256 wrapped to 0 in a `uint8_t`. No change on G615
  (no warning logged).

### Known issues
- panvk-test `gs_viewport_depth` case A fails (same on the beta.11 binary).
- Old kbase (CSF uAPI < ~1.13) may lack `GET_CPU_GPU_TIMEINFO`: timestamp queries read 0.
- 4 KiB pages assumed; synchronous queue submission (low GPU clocks in NFS:MW).

### Validation (G615)
- CTS `memory.mapping.*`, `memory.map_placed.*`, `synchronization{,2}.basic.*`:
  4520 pass / 0 fail / 13 NotSupported, same as beta.11.
- PanPlay i686 D3D9 cube renders; MiSide reaches the main menu (~55-60 fps).

## g615-v11-csf-v0.1.0-beta.11 (prerelease)

Mesa `5a07217f034b` + csf-v11 patches up to 098
(`patchSeriesId sha256:da34f4ae23daf7b7285c395dd2f3eabb156c04d5ffb9bdc5240189e3037b9c29`).
Poco X6 Pro, Mali-G615 MC6, mali_kbase CSF UAPI 1.21. Android minApi 35.

### Fixed
- `VK_ERROR_DEVICE_LOST` on tiler heap out-of-memory (new 098). The
  VERTEX_TILER_COMPLETED and FRAGMENT_COMPLETED heap operations left the
  kbase heap counters at `vt_start 1, vt_end 0, frag_end 16`. kbase rejects
  that with EINVAL on tiler OOM and terminates the group. On kbase, panvk now
  emits only VERTEX_TILER_STARTED; the queue's heap renewal (043) reclaims the
  chunks. Need for Speed Most Wanted hit this on every launch.
- Memory blow-up in long sessions (new 097). Each command pool allocated its
  own TLS BO (84 MiB in NFS:MW, about 20 of them), and the GPU prerast arenas
  committed 480 MB up front. TLS is now one device-wide BO that grows, and the
  arenas are grow-on-fault kbase regions. NFS:MW RSS: idle 1.2 GB -> 0.75 GB;
  gameplay 2.0 -> 4.1 GB+ (MemAvailable 0) -> flat 2.0-2.1 GB over 10 minutes.

### Results
- NFS:MW (i686 D3D9, PanPlay, FEX Extreme): intro 86 fps, race 40-44 fps,
  results 55 fps. No DEVICE_LOST and no app kills in 10 minutes.
- Regression: i686 D3D9 cube 46.7 fps, MiSide (x86_64 D3D11) 58 fps.
- Details: `worklogs/driver-remaining/097-098-nfs-memory-and-heap-ops.md`.

### Known issues
- GPU load is low (GED reports 0-11% busy at 265 MHz). Submission on kbase is
  synchronous, so the CPU and GPU do not overlap and DVFS stays at the lowest
  clock. This is the next bottleneck after FEX.

## g615-v11-csf-v0.1.0-beta.10 (prerelease)

Mesa `5a07217f034b` + csf-v11 patches up to 096
(`patchSeriesId sha256:4a6969c20c8152d751fb955fea54104ef4acacf4a56e546d32bbe94cbdd966a8`).
Poco X6 Pro, Mali-G615 MC6, mali_kbase CSF UAPI 1.21. Android minApi 35.

### Fixed
- 32-bit (i686 WoW64) games are fast and draw correctly. Placed maps
  (`VK_EXT_map_memory_placed`) now map the BO's dma-buf a second time at
  the requested address (new 091). This replaces the beta.9 shadow copy,
  which was merged word by word on every kick and wait. Need for Speed Most
  Wanted (DXVK D3D9) went from 0.5 fps to 39-66 fps, with clean DXVK HUD and
  game text. The i686 D3D10/D3D11 cubes now draw geometry instead of a grey
  frame.
- Tessellation state emission is no longer predicated on conditional
  rendering (093).
- Restart-strip chunk planning uses the whole workgroup, which fixes the
  094 device-lost flake (096).

### Added
- Large and indirect `gpu_prerast` draws (XFB, GS, tessellation) are
  chunked on the GPU, with no 65536-invocation cap (094).
- Attachment-less secondaries get the sample count of their context (095).

### Results
- NFS Most Wanted: 39-46 fps in gameplay and 66 fps in the main menu.
  i686 cubes D3D8/9/10/11 at 46-47 fps. x86_64 and ARM64EC cubes and
  MiSide are unchanged.
- CTS: `memory.mapping`, `memory.map_placed` and `synchronization{,2}.basic`
  give 4520 pass / 0 fail / 13 NotSupported. `map_placed` passes 13/13.
  For 093-096, the 36144-case list gives 17067 pass / 0 fail.
- Details: `worklogs/driver-remaining/091-placed-dma-heap.md`.

### Known issues
- Placed maps need `/dev/dma_heap/system`. Without it they fail with
  `VK_ERROR_MEMORY_MAP_FAILED`.
- With `memoryMapPlaced` enabled, every host-visible allocation comes from
  the dma-heap.

## g615-v11-csf-v0.1.0-beta.9 (prerelease)

Release notes are on the release page (patches up to 092: depthBounds,
VS/TES viewport index, per-viewport depth clamp, CSF event memory, and
placed maps through a shadow copy).

## g615-v11-csf-v0.1.0-beta.8 (prerelease)

Mesa `5a07217f034b` + csf-v11 patches up to 084
(`patchSeriesId sha256:374b7b830111a848515d3e0ec41a60b902bc57938deb420ecea7de8a03f58ecd`).
Poco X6 Pro, Mali-G615 MC6, mali_kbase CSF UAPI 1.21. Android minApi 35.

### Added
- X11 WSI in the Android ICD: `VK_KHR_xlib_surface` and `VK_KHR_xcb_surface`
  (083). The Android build now uses `-Dplatforms=android,x11` with
  `-Dxlib-lease=disabled`. The X11/XCB libraries are not linked or bundled.
  The driver dlopens them on the first X11 surface or presentation-support
  query, from the caller's library path (for example the launcher imagefs):
  `libxcb.so.1`, `libX11-xcb.so.1`, `libxcb-dri3.so.0`, `libxcb-present.so.0`,
  `libxcb-shm.so.0`, `libxcb-sync.so.1`, `libxcb-xfixes.so.0`,
  `libxcb-randr.so.0`, `libxshmfence.so.1`. The ICD `DT_NEEDED` list is
  unchanged. Presentation is software: the GPU renders and the CPU sends the
  image with X11 `PutImage` (no DRI3, no MIT-SHM). FIFO is not vsync-paced
  on this path.
- `scripts/prepare-x11-headers.sh`: header-only X11/XCB pkg-config prefix for
  the Android build.

### Fixed
- X11 software present path: the swapchain present id now advances, and
  `vkWaitForPresentKHR` timeouts return `VK_TIMEOUT` instead of
  `VK_ERROR_DEVICE_LOST` (084).

### Results (basic testing only)
With the release Android driver, under the launcher UID, the imagefs Vulkan
loader 1.4.315 and Termux:X11 (`DISPLAY=:0`): `vkCreateInstance` with
`VK_KHR_surface` + `VK_KHR_xlib_surface` and with `VK_KHR_xcb_surface`
returns `VK_SUCCESS`; an Xlib surface + swapchain presented 1500 frames
(about 343 fps, correct pixel readback and screenshot); Xlib and XCB resize
runs pass; `vkWaitForPresentKHR` returns `VK_SUCCESS`. The Android surface
path still works: the test APK autorun passes 11/11, including
`swapchain_lifecycle`.

### Known issues
The full extension audit, end-to-end presentation suite, repeated
launch/relaunch runs and the D3D8/9/10/11 matrix are still pending. Under
Proton 11 (i686 through wow64), Wine's winex11 fails to create the Vulkan
surface before the driver is called (it receives HWND `0xc0000005`; DXVK logs
"Presenter: Failed to create Vulkan surface"), so D3D presentation through
DXVK does not yet work in that setup. DXVK does create the instance and the
Mali-G615 device. All beta.7 known issues except the X11 entry still apply.

## g615-v11-csf-v0.1.0-beta.7 (prerelease)

Mesa `5a07217f034b` + csf-v11 patches up to 082
(`patchSeriesId sha256:0c47124314f24c46233d4135ff8f20dbde9b6571b60b0f5cdf6f3f8f4da8d0ce`).
Poco X6 Pro, Mali-G615 MC6, mali_kbase CSF UAPI 1.21. Android minApi 35.

### Added
- `vertexPipelineStoresAndAtomics` on v10-v12 (078). Vertex shaders that
  write storage buffers or use atomics run on the compute pre-raster path
  (`gpu_prerast`). This is a prerequisite for D3D11 feature level 11_1.
- The pre-raster arena is allocated when the feature is enabled (080).
- Test APK: `vertex_stores` test.

### Fixed
- Point-mode tessellation writes `gl_PointSize` (079).
- IDVS flags now come from the vertex shader variant that is actually bound
  (081).
- Intermittent `DeviceLost` in tessellation draws (082). On kbase each
  subqueue is its own command stream group. When a waiting group is evicted,
  kbase can only re-check its wait on the CPU if the sync word is in CSF event
  memory. The pre-raster arena and tessellation sync words were in ordinary
  memory, so an evicted group never resumed. They now live in CSF event memory.

### Results
CTS `atomic_operations` `*_vertex*`: 66 pass / 0 fail (38 NotSupported; the
first work-in-progress build was 1 pass / 65 fail, beta.6 reported the feature
as unsupported). Regression list of 12,132 cases (tessellation, geometry,
`transform_feedback.simple`, draw subset): 7940 pass / 0 fail / 0 DeviceLost,
against 7619 pass / 5 fail / 1 DeviceLost on the beta.6 baseline; 315
tessellation cases moved from NotSupported to Pass. A 5,613-case list
(atomics, memory model, shader access, `signal_order`): 3775 pass / 0 fail.
On-device run of the release APK: 11/11 in-app tests pass, including
`vertex_stores` and `swapchain_lifecycle`; the driver reports Mali-G615 MC6,
Mesa 26.3.0-devel (git-5a07217f03) and `vertexPipelineStoresAndAtomics = true`;
no DeviceLost or kbase faults.

### Known issues
The render descriptor ring buffer sync object and `VkEvent` sync objects are
still outside CSF event memory on kbase, so the same kind of hang is possible
there (pre-existing, not seen in these runs). Tessellation follow-ups are open:
per-instance geometry shader `PrimitiveIdIn` after tessellation, conditional
rendering on the compute loop, and an exact primitives-generated count. DXVK
feature level 11_1 has not yet been re-checked on this build. X11 surfaces
(`VK_KHR_xlib_surface`, `VK_KHR_xcb_surface`) are not in the Android package
yet. Carried over from beta.6: sync_file export (075) uses one device-wide KCPU
queue, so a pending export can delay later ones and could in theory deadlock
with wait-before-signal timelines; with a geometry-shader-selected viewport
(076), depth clip/clamp uses the union of all viewports' depth ranges;
system-scope signals (077) have an unmeasured game perf cost; dEQP draw
`depth_bias_patch_list_tri_line` fails (pre-existing); `depthBounds` is not
implemented; 2 intermittent `DeviceLost` in `transform_feedback` `query_copy`;
transform feedback is capped at 65,536 records per draw; the X11 present
teardown hang was seen once under Xvfb only; JICA98-derived patch 0005 is not
fully validated; `robustImageAccess2` is missing (no vkd3d-proton device;
deferred); sparse resources and FL 12_0 are impossible on Kbase.

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
