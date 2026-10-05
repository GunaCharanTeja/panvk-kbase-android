# Mali v10 (Valhall CSF) status

All file:line references throughout this document are relative to the Mesa root of the beta.16 tree (Mesa 5a07217f + csf-v11 series up to 107 + jm-v9 001-003), written as `src/panfrost/...:NNN`.

## Summary
The Mali v10 (Valhall CSF) backend successfully loads the ICD and enumerates Mali-G610 MC6 on Linux 5.10 (older CSF kbase), but DXVK rejects the adapter because `textureCompressionBC` is false. Consequently, `vkCreateDevice`, queue submission, tiler heap initialization, and shader execution remain completely unexercised on v10 hardware. No vulkaninfo dump was captured in telemetry because only PanPlay was executed. Mali-G610 MC6 has been observed; Mali-G710, Mali-G510, and Mali-G310 share the architecture but remain unseen.

## GPUs and devices tested

| Driver build | Anon ID(s) | Device | SoC | GPU | gpu_id -> Mesa model | Kernel | Android | App |
|---|---|---|---|---|---|---|---|---|
| beta.14 | `559830af` | 23054RA19C | MT6896 | Mali-G610 MC6 | `0xa8670000` -> G610 | 5.10 android12 | 15 | PanPlay 1.2.1 (D3D10) |
| beta.14 | `0b151151` | 23054RA19C | MT6896 | Mali-G610 MC6 | `0xa8670000` -> G610 | 5.10 android12 | 15 | PanPlay 1.2.1 (D3D11) |

The tested device SoC reports MT6896 (`ro.hardware mt6895`). The hardware identifier `0xa8670000` decodes to architecture 10.8, product 7, revision r0p0, which matches `PAN_PROD_ID(10,8,7)` for "G610" in `src/panfrost/model/pan_model.c:93`.

Other Mali v10 models belonging to the same Valhall CSF architecture—specifically Mali-G710 (used in Google Pixel 7 / Tensor G2), Mali-G510, and Mali-G310—have not been observed in any uploaded records. None of the records report an "Unknown gpu_id" error (`src/panfrost/vulkan/panvk_physical_device.c:1213/1345`).

## Kernel / kbase interface seen
- **Kernel version:** Linux 5.10 android12 (vendor Android 12 GKI / kernel tree).
- **kbase interface:** Command Stream Frontend (CSF) backend. The kbase uAPI version is not logged in upload telemetry (likely an older uAPI, < 1.18, unverified).
- **Vendor API level:** `ro.board.first_api_level = 31` (Android 12), `ro.hardware.gralloc = common`.
- **Gralloc module:** gralloc0 is absent (`MESA: No gralloc hwmodule detected (video buffers won't be supported)`).
- **EXEC_INIT warning:** `MESA: warning: kbase: KBASE_IOCTL_MEM_EXEC_INIT failed: Operation not permitted (executable BO allocation will not work)` on initialization.
- **Queues & heaps:** GPU queues and tiler heaps were not initialized due to early adapter rejection before `vkCreateDevice`.

## Per-test results

### PanPlay execution runs

| Driver build | Anon ID | Target executable | Exit code | Observed behavior / DXVK lines |
|---|---|---|---|---|
| beta.14 | `559830af` | D3D10 ARM64EC cube | 1 | DXVK rejects adapter (`textureCompressionBC` false), exits after 1 s |
| beta.14 | `0b151151` | D3D11 ARM64EC cube | 1 | DXVK rejects adapter (`textureCompressionBC` false), exits after 1 s |

Both PanPlay runs abort during adapter discovery before any GPU queues, command buffers, or device memory structures can be initialized.

### PanProbe test suite (17 tests)

No PanProbe test run has been uploaded for any Mali v10 device. All 17 suite tests remain unexercised:

| Test | Result | Notes |
|---|---|---|
| `gpu_prerast_slice` | Untested | No PanProbe run uploaded |
| `clip_cull` | Untested | No PanProbe run uploaded |
| `multi_viewport` | Untested | No PanProbe run uploaded |
| `fill_mode` | Untested | No PanProbe run uploaded |
| `bc_decode` | Untested | No PanProbe run uploaded |
| `geometry` | Untested | No PanProbe run uploaded |
| `tessellation` | Untested | No PanProbe run uploaded |
| `xfb` | Untested | No PanProbe run uploaded |
| `pipeline_stats` | Untested | No PanProbe run uploaded |
| `vertex_stores` | Untested | No PanProbe run uploaded |
| `gs_viewport_depth` | Untested | No PanProbe run uploaded |
| `vs_viewport_index` | Untested | No PanProbe run uploaded |
| `depth_bounds` | Untested | No PanProbe run uploaded |
| `large_draw` | Untested | No PanProbe run uploaded |
| `vmr_secondary` | Untested | No PanProbe run uploaded |
| `tess_cond_state` | Untested | No PanProbe run uploaded |
| `swapchain_lifecycle` | Untested | No PanProbe run uploaded |

## Failures and log excerpts

### DXVK adapter rejection (`wine-run.log`)
```
info:  Game: dxcube-arm64ec.exe
info:  DXVK: v3.1.1+
info:  Build: aarch64 clang 23.1.2
info:  Found device: Mali-G610 MC6 (panvk 26.2.99)
info:    Skipping: Device does not support required feature 'textureCompressionBC'
warn:  DXVK: No adapters found. Please check your device filter settings
warn:  and Vulkan drivers. A Vulkan 1.3 capable setup is required.
err:   Failed to initialize DXVK.
CUBE: FAIL create hr=0x80004005
exit=1
```

For the D3D11 test run (`0b151151`), DXGI factory initialization fails immediately as DXVK finds no compatible adapter:
```
err:   D3D11CreateDevice: Failed to create a DXGI factory
```

### MESA driver warnings
Logcat during early driver initialization records kbase and gralloc warnings:
```
W MESA: kbase: KBASE_IOCTL_MEM_EXEC_INIT failed: Operation not permitted (executable BO allocation will not work)
W MESA: No gralloc hwmodule detected (video buffers won't be supported)
I MESA: Using fallback gralloc implementation
```

## Root causes
- **Cross-arch blocker 1 (BC texture compression):** `panvk_bc_emul_enabled()` at `src/panfrost/vulkan/panvk_physical_device.c:1815-1826` disables software BC emulation if kbase `TEXTURE_FEATURES` exposes native BC1 (bit 7). However, `has_texture_compression_bc()` at `src/panfrost/vulkan/panvk_vX_physical_device.c:294-302` requires all 10 BC format bits natively before advertising `textureCompressionBC` at `:338`. With emulation turned off, `get_image_plane_format_features()` at `src/panfrost/vulkan/panvk_physical_device.c:1837-1839` returns 0 for every BC format, including native ones. Texture features come from `GET_GPUPROPS` (`src/panfrost/lib/kmod/kbase_kmod.c:372` -> `src/panfrost/lib/pan_props.c:84`), where BC1 is bit 7 and the full BC set is mask `0x1ff80` (`src/panfrost/genxml/common.xml:76`). Fixing this requires keeping `src/panfrost/vulkan/panvk_image.c:674` consistent. This is marked as an inference because no raw `TEXTURE_FEATURES` dump was captured in uploaded records.
- **Cross-arch blocker 2 relevance (Gralloc mapper), expected but not yet observed on v10 (no PanProbe run):** the mapper loader added by `patches/android/013-vendor-mapper-metadata.patch` needs an AIMapper stable-C v5 mapper (vendor API >= 34); this device has vendor API 31. In `src/util/u_gralloc/u_gralloc_fallback.c:83,95,102`, loading `mapper.mediatek.so` would then fail, triggering `-ENOTSUP` (`u_gralloc_fallback.c:125-130,425-431`) and `VK_ERROR_INVALID_EXTERNAL_HANDLE` (`src/vulkan/runtime/vk_android.c:150-152`). Calling paths include swapchain creation (`src/panfrost/vulkan/panvk_image.c:803` -> `src/panfrost/vulkan/panvk_android.c:172/125`) and AHB dedicated memory import (`src/panfrost/vulkan/panvk_device_memory.c:68` -> `src/panfrost/vulkan/panvk_android.c:318/236` -> `src/vulkan/runtime/vk_android.c:687` -> `:152`). In GitHub issue #5, a Mali-G610 user in a third-party Winlator fork hit `err:msvcrt:_wassert (L"!status && \"vkCreateSwapchainKHR\"")`. PanPlay is unaffected because it presents through X11 software WSI rather than Android AHardwareBuffer / Gralloc.
- **Cross-arch issue 3 (EXEC_INIT EPERM):** `src/panfrost/lib/kmod/kbase_kmod.c:1391-1410` executes JIT_INIT before EXEC_INIT; older CSF kbase returns `-EPERM` when initializing the executable VA zone after JIT. Setting `PAN_KMOD_BO_FLAG_EXECUTABLE` maps to `BASE_MEM_PROT_GPU_EX` at `src/panfrost/lib/kmod/kbase_kmod.c:1624`. An executable allocation failure would surface as `VK_ERROR_OUT_OF_DEVICE_MEMORY` at `src/panfrost/vulkan/panvk_vX_shader.c:3133`. This has unknown impact on v10 because no shader has been compiled or submitted yet.

## What the driver lacks on this arch
- Software BC emulation fallback when kbase exposes partial native BC texture features.
- Testing and support for old-CSF queue-group creation layouts: the driver supports the 112-byte layout (CSF >= 1.25) and falls back directly to the 32-byte layout (CSF 1.6); the intermediate 40-byte 1.18 layout is never attempted.
- Legacy 16-byte tiler heap initialization compatibility on older CSF kernels.
- Timestamps: `GET_CPU_GPU_TIMEINFO` may be absent on older CSF kbase; timestamp queries would then read 0.
- CS work-register count: an implausible firmware value falls back to 96 on v10/v11 (a Pixel 7 G710 reported a bad value before beta.12); unverified on a real v10 kernel.
- Android gralloc mapper metadata support for vendor API < 34 / HIDL mapper4 for Android-surface swapchains.

## Fix plan

| Rank | Item | Effort | Unblocks |
|---|---|---|---|
| 1 | BC emulation decision (emulate all BC formats unless full 0x1ff80 mask native) | 4–8 h | Unblocks DXVK adapter selection on v10 |
| 2 | PanProbe run on a G610 (data collection) then fix device/queue/heap issues | Effort unknown | Exercises `vkCreateDevice`, CSF queues, and tiler heaps |
| 3 | Gralloc mapper: vendor-neutral discovery / HIDL mapper4 backend | 24–48 h (quick: 4–8 h) | Unblocks `vkCreateSwapchainKHR` on Android surfaces |
| 4 | EXEC_INIT ordering fix (call before JIT_INIT) | 6–12 h | Eliminates kbase EPERM warning, avoids shader alloc failure |
| 5 | Old-CSF layout support (40-byte 1.18 queue group, legacy heap init as needed) | As needed | Supports older CSF vendor kernel releases |

## Open questions / data needed from testers
- **PanProbe zip on G610:** Need a full PanProbe archive from a Mali-G610 device, specifically capturing the Info page to identify the exact kbase uAPI version.
- **TEXTURE_FEATURES on G610:** Need a raw dump of kbase `TEXTURE_FEATURES` (`gpuinfo`) to verify exact native texture feature bits.
- **Other v10 hardware:** Need `gpu_id` values and variant strings from Mali-G710 (e.g. Google Pixel 7, Tensor G2), Mali-G510, and Mali-G310 hardware owners.
- **Queue-group layout:** Determine whether the 40-byte 1.18 CSF queue-group layout is required on 5.10 / Android 12 vendor kernels.

## Links
- [Universal Mali status](../README.md)
- [Universal Mali Plan](../../plans/PANVK_UNIVERSAL_MALI_PLAN.md)
