# Mali v11 (Valhall CSF) status

All file:line references throughout this document are relative to the Mesa root of the beta.16 tree (Mesa 5a07217f + csf-v11 series up to 107 + jm-v9 001-003), written as `src/panfrost/...:NNN`.

## Summary
Mali v11 (Valhall Command Stream Frontend) is the primary developed and released reference target architecture for PanVK. It serves as the baseline for the CSF driver series, implementing compute pre-rasterization lowering, geometry shading, tessellation, transform feedback, and DXVK D3D9 support. Across 8 telemetry records from 4 physical devices:
- Developer reference device (Poco X6 Pro, Mali-G615 MC6): PanProbe scores 17/17 on beta.15 and beta.16; PanPlay runs D3D9 i686 cube cleanly to exit code 0 (two runs).
- Commercial tester devices (Mali-G615 MC2 phones on MT6878, and a stock G615 MC6): PanProbe scores 16/17; the only failing test is `swapchain_lifecycle` on the Android surface due to the hard-coded MediaTek gralloc mapper (cross-arch blocker 2).
- Mali-G715 (Google Pixel 8 / 8 Pro, Tensor G3): Observed only via GitHub issues #6 and #7, reporting black screen or crashing under third-party launchers using pre-beta.13 builds.

## GPUs and devices tested

| Driver build | Anon ID(s) | Device | SoC | GPU | gpu_id -> Mesa model | Kernel | Android | App |
|---|---|---|---|---|---|---|---|---|
| beta.15 | `32144abe`, `71dc96c0` | 2311DRK48I (dev Poco X6 Pro) | MT6897 | Mali-G615 MC6 | `0xb8a31030` -> G615 | 6.1 custom (uAPI 1.21) | 16 | PanProbe 1.2.2 (17/17) |
| beta.15 | `e63009bd`, `f2fc9ca6` | 2311DRK48I (dev Poco X6 Pro) | MT6897 | Mali-G615 MC6 | `0xb8a31030` -> G615 | 6.1 custom (uAPI 1.21) | 16 | PanPlay 1.2.2 (D3D9 exit 0 x2) |
| beta.14 | `ec20d5f3` | 2311DRK48G (stock) | MT6897 | Mali-G615 MC6 | `0xb8a31030` -> G615 | 6.1 android14 | 16 | PanProbe 1.2.1 (0/1 swapchain) |
| beta.13 | `7962f613` | Infinix X6857 | MT6878 | Mali-G615 MC2 | `0xb8a31030` -> G615 | 6.1 android14 | 16 | PanProbe 1.2.0 (imported .so) |
| beta.14 | `54cdc0c7` | Infinix X6857 | MT6878 | Mali-G615 MC2 | `0xb8a31030` -> G615 | 6.1 android14 | 16 | PanProbe 1.2.1 (imported .so) |
| beta.14 | `bd79c8af` | 24090RA29G | MT6878 | Mali-G615 MC2 | `0xb8a31030` -> G615 | 6.1 android14 | 16 | PanProbe 1.2.1 (16/17) |

### Hardware decode and model mapping
- All tested G615 devices report hardware identifier `0xb8a31030`, which decodes to architecture 11.8, product 3, revision r1p3. This matches `PAN_PROD_ID(11,8,3)` v4 in `src/panfrost/model/pan_model.c:108` ("G615", shared across 6-core MC6 and 2-core MC2 configurations).
- Mali-G715 (Google Pixel 8 / 8 Pro, Tensor G3, issues #6 and #7) enumerates as `PAN_PROD_ID(11,8,2)` v4 in `src/panfrost/model/pan_model.c:106`.
- No record reported an "Unknown gpu_id" error (`src/panfrost/vulkan/panvk_physical_device.c:1213/1345`).

## Kernel / kbase interface seen
- **Kernel version:** Linux 6.1 android14 stock GKI across commercial phones, plus one custom Linux 6.1 kernel on the developer reference device.
- **kbase interface:** Command Stream Frontend (CSF) backend. Only the developer device's CSF uAPI version (1.21) is known from worklog records; kbase uAPI version is not logged in upload telemetry.
- **Vendor DDK:** GLES r44p1 (`GL_VERSION` from vendor driver).
- **Vendor API level:** `ro.board.first_api_level = 34` (Android 14), `ro.hardware.gralloc = common`.
- **vulkaninfo comparison:** G615 MC2 on beta.14 (`bd79c8af`) and G615 MC6 on beta.15 (`71dc96c0`) report identical Vulkan extension sets (188 extensions) and matching core feature flags. The only difference is `shaderDeviceClock` (true on MC2, false on dev MC6). This property reflects kernel timestamp coherency (`src/panfrost/lib/kmod/kbase_kmod.c:430` -> `src/panfrost/vulkan/panvk_vX_physical_device.c:661`), not an architectural difference.

## Per-test results

### PanProbe test suite (17 tests)

The table below summarizes results across driver builds and devices. The commercial stock MC6 record (`ec20d5f3`) executed only the swapchain test case:

| Test | G615 MC2 beta.13 (`7962f613`) | G615 MC2 beta.14 (`54cdc0c7`, `bd79c8af`) | G615 MC6 stock beta.14 (`ec20d5f3`) | G615 MC6 dev beta.15 (`32144abe`, `71dc96c0`) | Notes |
|---|---|---|---|---|---|
| `gpu_prerast_slice` | PASS | PASS | Not run | PASS | Compute pre-rasterization and indirect slicing pass |
| `clip_cull` | PASS | PASS | Not run | PASS | Clip and cull distance shaders pass |
| `multi_viewport` | PASS | PASS | Not run | PASS | Multi-viewport rendering passes |
| `fill_mode` | PASS | PASS | Not run | PASS | Point, line, and fill modes pass |
| `bc_decode` | PASS | PASS | Not run | PASS | BC emulation (GPU decode) passes |
| `geometry` | PASS | PASS | Not run | PASS | Geometry shaders lower to compute pass |
| `tessellation` | PASS | PASS | Not run | PASS | Tessellation control and evaluation pass |
| `xfb` | PASS | PASS | Not run | PASS | Transform feedback streaming passes |
| `pipeline_stats` | PASS | PASS | Not run | PASS | Pipeline statistics queries pass |
| `vertex_stores` | PASS | PASS | Not run | PASS | Vertex pipeline stores and atomics pass |
| `gs_viewport_depth` | PASS | PASS | Not run | PASS | Viewport depth clamp passes |
| `vs_viewport_index` | PASS | PASS | Not run | PASS | Viewport index routing passes |
| `depth_bounds` | PASS | PASS | Not run | PASS | Depth bounds testing passes |
| `large_draw` | PASS | PASS | Not run | PASS | Large draw with transform feedback passes |
| `vmr_secondary` | PASS | PASS | Not run | PASS | Variable multisample rate execution passes |
| `tess_cond_state` | PASS | PASS | Not run | PASS | Conditional rendering with tessellation passes |
| `swapchain_lifecycle` | FAIL | FAIL | FAIL | PASS | Fails on stock ROMs (-1000072003, blocker 2); passes on dev device |

PanProbe 17/17 on the developer G615 MC6 was reconfirmed on beta.16 per the repository CHANGELOG.

### PanPlay execution runs

| Driver build | Anon ID | Device | Target executable | Exit code | Observed behavior / WSI presentation |
|---|---|---|---|---|---|
| beta.15 | `e63009bd` | 2311DRK48I | D3D9 i686 cube | 0 | Clean execution, renders and presents via X11 software WSI |
| beta.15 | `f2fc9ca6` | 2311DRK48I | D3D9 i686 cube | 0 | Clean execution, renders and presents via X11 software WSI |

## Failures and log excerpts

### Stock Android surface swapchain failure (`bd79c8af`, `ec20d5f3`)
```
FAIL vkCreateSwapchainKHR res=-1000072003 phase=create_swapchain
```
Associated logcat output from stock G615 (`54cdc0c7`):
```
MESA: [P0A-V19-FULLPLANE] mapper load failed
MESA: [P0A-V19-FULLPLANE] complete metadata unavailable rc=-95; refusing guessed layout
```

### Passing swapchain lifecycle on dev device (`71dc96c0`)
```
SWAPCHAIN 2090x440 images=3 format=37
FPS 114.6
PHASE present_main ms=2618 frames=300
RESIZE extent unchanged, recreated with oldSwapchain
PHASE resize ms=17
FPS_RESIZED 115.9
PHASE present_resized ms=1035
PHASE destroy ms=1
PHASE cycle ms=997
PHASE teardown ms=15
PASS swapchain_lifecycle
```

### GitHub Issue #7 log excerpt (Pixel 8, Mali-G715 MC7)
Winlator wrapper log with pre-beta.13 driver (`git-5a07217f03`):
```
AllocateMemory ... failed with result: -1000072003
```

## Root causes
- **Cross-arch blocker 2 (Gralloc mapper):** Introduced by `patches/android/013-vendor-mapper-metadata.patch` (requires AIMapper stable-C v5, vendor API >= 34). In `src/util/u_gralloc/u_gralloc_fallback.c:83`, binder passthrough attempts `open_hal("mapper", "mediatek")`, and direct dlopen of `mapper.mediatek.so` is attempted at `u_gralloc_fallback.c:95, 102`. On stock commercial ROMs where namespace isolation or library location fails, `u_gralloc_fallback.c:106` emits `mapper load failed`, causing initialization to return `-ENOTSUP` (-95) at `u_gralloc_fallback.c:125-130`. The fallback implementation refuses guessed layouts at `u_gralloc_fallback.c:425-431`, causing `u_gralloc_get_buffer_basic_info()` to return `VK_ERROR_INVALID_EXTERNAL_HANDLE` (-1000072003) at `src/vulkan/runtime/vk_android.c:150-152`. This failure blocks swapchain image creation (`src/panfrost/vulkan/panvk_image.c:803` -> `src/panfrost/vulkan/panvk_android.c:172/125`) and AHardwareBuffer dedicated memory imports (`src/panfrost/vulkan/panvk_device_memory.c:68` -> `src/panfrost/vulkan/panvk_android.c:318/236` -> `src/vulkan/runtime/vk_android.c:687` -> `vk_android.c:152`).
- **PanPlay immunity:** PanPlay is unaffected by gralloc mapper failures because it presents through X11 software WSI without touching Android AHardwareBuffer surfaces.
- **Developer device divergence:** Why the developer device passes `swapchain_lifecycle` while stock commercial G615 ROMs fail is not known, because the mapper initialization Info log is not captured in PanProbe logcat (telemetry logging is restricted to warning level).
- **GitHub issue cross-check:**
  - **Issue #5 (G615 memory):** In third-party launchers on beta.13, a tester reported ~5–6 GB heap 0 usage vs ~1.1 GB on beta.10 with low GPU utilization. Beta.13 commits 160 MiB prerast arenas per `VkDevice` up front (patch 100); this excessive consumption was not reproduced in backend logs.
  - **Issue #6 (Pixel 8 Pro G715):** Crashing / non-working status reported without logs.
  - **Issue #7 (Pixel 8 G715 MC7):** Black screen in a third-party launcher using pre-beta.13 Mesa (`git-5a07217f03`). Wrapper logs show `AllocateMemory ... failed with result: -1000072003` (`VK_ERROR_INVALID_EXTERNAL_HANDLE`), consistent with gralloc mapper failure on Google Tensor where `mapper.mediatek.so` is absent (a buffer/fd import failure is also possible).
  - **Issue #2 (GTA IV):** Open issue reporting staging buffer allocation failure (`VK_ERROR_OUT_OF_DEVICE_MEMORY`); no backend upload record exists.

## What the driver lacks on this arch
- Vendor-neutral gralloc mapper discovery and HIDL mapper4 metadata support; all other PanProbe features pass (17/17).
- Next driver task: asynchronous kbase queue submission (not yet started).
- Real-world 3D gaming validation: Fallout 4 testing on the developer device is currently in progress (no results yet).

## Fix plan

| Rank | Item | Effort | Unblocks |
|---|---|---|---|
| 1 | Vendor-neutral gralloc mapper + HIDL mapper4 backend (or quick configurable mapper names) | 24–48 h (proper) / 4–8 h (quick) | Unblocks `swapchain_lifecycle` and Android-surface WSI on stock ROMs and G715 |
| 2 | Obtain PanProbe zip and logcat from a Mali-G715 (Pixel 8) on beta.16 | Data collection | Verifies CSF kbase uAPI and mapper behavior on Google Tensor |
| 3 | Investigate Issue #5 memory consumption with a heap-usage trace | not estimated | Identifies whether upfront 160 MiB prerast arena commits cause heap 0 exhaustion |
| 4 | Async kbase queue submission (next planned driver task, not started) | not estimated | See `worklogs/g615-dxvk/PROGRESS.md` |

### Plan implementation notes
- **Rank 1 (Gralloc mapper):** Proper fix implements vendor-neutral mapper discovery in `src/util/u_gralloc/u_gralloc_fallback.c` (`allocator getIMapperLibrarySuffix` / declared passthrough instance, matching AOSP `Gralloc5.cpp`) plus HIDL mapper4 metadata backend for vendor API < 34. Quick fallback via configurable mapper name list (`PANVK_MAPPER_NAMES`) or `ro.hardware.gralloc` hints provides partial relief for non-MediaTek or differently named libraries.
- **Rank 2 (G715 verification):** Pixel 8 runs Google Tensor G3 with Google's proprietary gralloc implementation rather than MediaTek's mapper. A PanProbe run with logcat will verify whether CSF queues initialize cleanly and whether gralloc mapper fallback is the only remaining blocker.
- **Rank 3 (Issue #5 memory):** In beta.13, patch 100 added upfront allocation of 160 MiB prerast arenas per `VkDevice`. If a launcher creates multiple devices or allocations fail to release, heap 0 usage escalates rapidly. A memory allocation trace under a third-party launcher will isolate the allocation origin.
- **Rank 4 (Async queue submission):** The next planned driver task (not started). beta.15/beta.16 already removed the CPU waits around submit (patches 102-107); scope and design are tracked in `worklogs/g615-dxvk/PROGRESS.md`.

## Open questions / data needed from testers
- **Mali-G615 MC2 beta.16 verification:** Need PanProbe beta.16 runs from G615 MC2 owners to confirm status on the latest release.
- **Mali-G715 hardware data:** Need PanProbe archive zips and logcat from Pixel 8 / 8 Pro owners running beta.16.
- **Stock ROM mapper trace:** Need a full Info-level logcat from a stock G615 device to observe the mapper load path and determine why `mapper.mediatek.so` fails.

## Links
- [Universal Mali status](../README.md)
- [G615 DXVK Progress Worklog](../../../worklogs/g615-dxvk/PROGRESS.md)
- [PanVK Changelog](../../../CHANGELOG.md)
