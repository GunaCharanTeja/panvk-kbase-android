# Universal Mali status (tester data)

This document indexes test results, driver execution stages, cross-architecture blockers, and hardware coverage for the PanVK universal Mali investigation. Data reflects a snapshot taken on 2026-10-05 following the beta.16 release (Mesa 5a07217f with csf-v11 series up to 107, and the experimental v9 JM backend active since beta.14). It synthesizes 27 upload records submitted by testers between 02:20 and 11:45 UTC across PanProbe (18 records) and PanPlay (9 records). Architecture plans, worklogs, and roadmap progression are tracked in [PANVK_UNIVERSAL_MALI_PLAN.md](../plans/PANVK_UNIVERSAL_MALI_PLAN.md).

All source file:line references throughout this document are relative to the Mesa root of the beta.16 tree (Mesa 5a07217f + csf-v11 series up to 107 + jm-v9 001-003), written as `src/panfrost/...:NNN`.

## Arch matrix

| Arch | GPUs seen | gpu_id(s) | Kernel / kbase interface | Records (devices) | Result (best driver build) | Top blockers | Doc link |
|---|---|---|---|---|---|---|---|
| v9 (Valhall JM) | Mali-G57 MC2 | 0x90930010 | 5.15 android13 / JM uAPI 11.0 | 13 (1 device) | Partial: PanProbe 1/17 (beta.14–beta.16); PanPlay cube exit 3/5 | Vulkan 1.1 reporting (DXVK needs 1.3), no compute pre-raster path, replay fault, VMR sample counting, BC disabled, gralloc mapper | [v9-jm/README.md](v9-jm/README.md) |
| v10 | Mali-G610 MC6 | 0xa8670000 | 5.10 android12 / CSF, uAPI not logged (likely old) | 2 (1 device) | Fails at adapter selection (beta.14): ICD loads, DXVK rejects missing BC | BC emulation disabled, gralloc mapper, untested old-CSF queue group/heap layouts, EXEC_INIT EPERM | [v10/README.md](v10/README.md) |
| v11 | Mali-G615 MC6, Mali-G615 MC2 | 0xb8a31030 | 6.1 android14, 6.1 custom / CSF (uAPI 1.21 on the dev device; not logged on tester devices) | 8 (4 devices) | Works on dev device: PanProbe 17/17, PanPlay cube exit 0 (beta.15–beta.16); stock ROMs 16/17 | Gralloc mapper load failed on stock ROMs (swapchain_lifecycle -1000072003) | [v11/README.md](v11/README.md) |
| v12 | Mali-G720 MC7 | 0xc8700010 | 6.6 android15 (4k) / CSF uAPI ~1.30 | 1 (1 device) | Partial: PanProbe 14/17 (beta.14); Android swapchain passes; games not tested | v12+ per-viewport depth runs (gs_viewport_depth), depthBounds and shaderOutputViewportIndex gates | [v12/README.md](v12/README.md) |
| v13 | None | None | None | 0 (no data) | No data (no folder) | No data | None (no data, no folder) |
| v14 | None | None | None | 0 (no data) | No data (no folder) | No data | None (no data, no folder) |
| Non-Mali | Adreno 825 | - | 6.6 android15 / Qualcomm kgsl | 1 (1 device) | Not applicable: ran on Qualcomm system driver, 0/1 geometry | Not applicable (PanVK not involved) | None (not applicable, no folder) |

### Architecture summary notes
- **v9 (13 records, 1 device):** Lenovo tablet TB336FU (MT8755). Progressed from unsupported on beta.13 to 1/17 on beta.14–beta.16. Blocked by Vulkan 1.1 reporting and absence of compute pre-raster lowering.
- **v10 (2 records, 1 device):** Xiaomi 23054RA19C (MT6896). Physical device enumerates cleanly in PanPlay, but DXVK aborts adapter selection when `textureCompressionBC` is missing.
- **v11 (8 records, 4 devices):** Developer G615 MC6 passes 17/17 on PanProbe and completes D3D9 cube exit 0 on PanPlay. Commercial stock ROMs pass 16/17, blocked only by gralloc mapper loading on swapchain creation.
- **v12 (1 record, 1 device):** Poco X7 Pro (MT6899, G720 MC7). Passes 14/17 on beta.14 including swapchain creation. Fails 3 tests gated by viewport-depth and depth-bounds features.
- **v13 & v14 (0 records):** No uploads or issue logs exist for 5th-gen CSF or G1 series. No folders are allocated.
- **Non-Mali (1 record):** Snapdragon SM8735 with Adreno 825 executed on Qualcomm's proprietary driver; PanVK was not involved.

## Cross-arch blockers

### 1. BC emulation decision
- **Affected archs & records:**
  - v9 Valhall JM: Lenovo tablet TB336FU (e.g. record `49abcff8`). PanProbe `bc_decode` logs failure for every format; `vulkaninfo` reports `textureCompressionBC = false`.
  - v10 Valhall CSF: Xiaomi device 23054RA19C (`559830af`, `0b151151`). DXVK skips adapter due to missing `textureCompressionBC`.
  - Contrast: v11 (G615) and v12 (G720) report `textureCompressionBC = true` via emulation and pass `bc_decode`.
- **Exact log lines:**
```
info: Found device: Mali-G610 MC6 (panvk 26.2.99)
info: Skipping: Device does not support required feature 'textureCompressionBC'
warn: DXVK: No adapters found.
err: Failed to initialize DXVK.
CUBE: FAIL create hr=0x80004005
```
```
FORMAT BC1_RGB_UNORM optimal=0x0 linear=0x0 ifp=-11 FAIL
```
- **Root cause:**
  - `panvk_bc_emul_enabled()` at `src/panfrost/vulkan/panvk_physical_device.c:1815-1826` turns software emulation off if kbase `TEXTURE_FEATURES` reports BC1 (DXT1, bit 7).
  - `has_texture_compression_bc()` at `src/panfrost/vulkan/panvk_vX_physical_device.c:294-302` requires all 10 BC format bits natively (`panvk_vX_physical_device.c:338`).
  - When emulation is disabled without full native support, `get_image_plane_format_features()` at `src/panfrost/vulkan/panvk_physical_device.c:1837-1839` returns 0 for every BC format, including native ones.
  - Texture features derive from `GET_GPUPROPS` (`src/panfrost/lib/kmod/kbase_kmod.c:372` -> `src/panfrost/lib/pan_props.c:84`), where full BC is mask `0x1ff80` (`src/panfrost/genxml/common.xml:76`). G57 and G610 likely report native BC1 without the remaining formats.
- **Fix options & effort:** Emulate all BC formats unless the complete native mask (`0x1ff80`) is present; otherwise expose per-format native support. Maintain consistency with image view/creation logic at `src/panfrost/vulkan/panvk_image.c:674`. Effort: 4–8 h including validation. Unblocks DXVK adapter selection on v10 (v9 remains blocked by Vulkan 1.1 requirements).

### 2. Android gralloc mapper hard-coded to MediaTek stable-C mapper
- **Affected archs & records:**
  - v9 tablet: all beta.14 and beta.15 runs (`49abcff8`, `6ff8855c`, `fad0cb63`).
  - v10: G610 in third-party Winlator fork (issue #5 comment).
  - v11 stock ROMs: Infinix X6857 (`7962f613`, `54cdc0c7`), 24090RA29G (`bd79c8af`), stock 2311DRK48G (`ec20d5f3`).
  - G715 on Google Tensor (issue #7).
  - Passing devices: G720 Poco X7 Pro (vendor API 202404) and developer G615 (`32144abe`, `71dc96c0`). All devices log harmless `No gralloc hwmodule detected`.
- **Exact log lines:**
```
FAIL vkCreateSwapchainKHR res=-1000072003 phase=create_swapchain
MESA: [P0A-V19-FULLPLANE] mapper load failed
MESA: [P0A-V19-FULLPLANE] complete metadata unavailable rc=-95; refusing guessed layout
No gralloc hwmodule detected (video buffers won't be supported)
```
- **Root cause:**
  - Introduced by `patches/android/013-vendor-mapper-metadata.patch`.
  - `src/util/u_gralloc/u_gralloc_fallback.c:83` hardcodes binder passthrough `open_hal("mapper", "mediatek")`, and `u_gralloc_fallback.c:95, 102` attempts `mapper.mediatek.so` directly.
  - On non-MediaTek platforms (e.g. Tensor) or devices lacking AIMapper stable-C v5 (vendor API < 34), `u_gralloc_fallback.c:106` logs mapper load failed and `u_gralloc_fallback.c:125-130` returns `-ENOTSUP` (-95).
  - `u_gralloc_fallback.c:425-431` refuses guessed layouts, causing `u_gralloc_get_buffer_basic_info()` to fail at `src/vulkan/runtime/vk_android.c:150-152` with `VK_ERROR_INVALID_EXTERNAL_HANDLE` (-1000072003).
  - Calling paths: swapchain creation (`src/panfrost/vulkan/panvk_image.c:803` -> `src/panfrost/vulkan/panvk_android.c:172/125`) and AHB dedicated import (`src/panfrost/vulkan/panvk_device_memory.c:68` -> `src/panfrost/vulkan/panvk_android.c:318/236` -> `src/vulkan/runtime/vk_android.c:687` -> `vk_android.c:152`).
  - Note: PanPlay is unaffected because it presents via X11 software WSI without AHB.
- **Fix options & effort:**
  1. Vendor-neutral mapper discovery (`allocator getIMapperLibrarySuffix` / declared passthrough instance, matching AOSP `Gralloc5.cpp`) plus HIDL mapper4 metadata backend for vendor API < 34: 24–48 h.
  2. Quick configuration: configurable mapper name list via `PANVK_MAPPER_NAMES` and `ro.hardware.gralloc` hints: 4–8 h (insufficient for HIDL mapper4-only vendors).
  3. Constrained fallback for verified linear single-plane RGBA: 6–12 h (blind `AHardwareBuffer_describe` guessing is unsafe without modifier/offset).

### 3. KBASE_IOCTL_MEM_EXEC_INIT ordering
- **Affected archs & records:**
  - v9 Valhall JM: all tablet records (`5ec9f0c1` through `49abcff8`).
  - v10 Valhall CSF: 23054RA19C (`559830af`, `0b151151`).
  - Never occurs on G615 (v11) or G720 (v12).
- **Exact log lines:**
```
MESA: warning: kbase: KBASE_IOCTL_MEM_EXEC_INIT failed: Operation not permitted (executable BO allocation will not work)
```
- **Root cause:**
  - In `src/panfrost/lib/kmod/kbase_kmod.c:1391-1396`, `JIT_INIT` runs prior to `EXEC_INIT` (`kbase_kmod.c:1404-1410`). JM requests 1024 pages and CSF requests 0x100000.
  - Older kbase kernels return `-EPERM` when initializing executable VA after JIT initialization or when the zone already exists; newer CSF sets up the zone automatically.
  - On the v9 JM kernel, shaders still run (many PanProbe cases draw correctly), so the warning is not fatal there. Shader BOs use `PAN_KMOD_BO_FLAG_EXECUTABLE` -> `BASE_MEM_PROT_GPU_EX` (`kbase_kmod.c:1624`); if a kernel does reject them, shader upload fails with `VK_ERROR_OUT_OF_DEVICE_MEMORY` at `src/panfrost/vulkan/panvk_vX_shader.c:3133`. The effect on v10 is unknown because no shader has run there yet.
- **Fix options & effort:** Call `KBASE_IOCTL_MEM_EXEC_INIT` before `JIT_INIT` where explicit initialization is needed, and size the JM zone (currently 4 MiB) sensibly. Effort: 6–12 h including JM, older CSF, and newer CSF verification. Low priority until v10 shader submission data confirms failure.

## Ranked fix plan (all archs)

| Rank | Item | Impact / Unblocks | Effort |
|---|---|---|---|
| 1 | BC emulation decision | Unblocks DXVK adapter selection on v10; unblocks BC formats on v9 | 4–8 h |
| 2 | Vendor-neutral gralloc mapper + mapper4 backend | Fixes Android-surface swapchain (`vkCreateSwapchainKHR`) and AHB imports on stock ROMs, Pixel (Tensor), and older vendor APIs | 24–48 h (proper) / 4–8 h (quick) |
| 3 | v12+ per-viewport depth runs + lift depthBounds/shaderOutputViewportIndex gates | Unblocks remaining 3 PanProbe failures on G720 (gs_viewport_depth, vs_viewport_index, depth_bounds) and v12+ feature parity | 16–32 h (runs) + 8–16 h (depthBounds) + 4–8 h (viewportIndex) |
| 4 | G610 PanProbe run on old CSF kbase | Exercises `vkCreateDevice`, queue group layout compatibility (112-byte vs 32-byte/40-byte), tiler heap init, and EXEC zone on 5.10 kernel | Tester verification (0 code effort) |
| 5 | v9 replay fault + VMR sample count | Fixes `gpu_prerast_slice` DEVICE_LOST crash on replay and enables sample counts 1–3 in `vmr_secondary` on Valhall JM | 4–8 h diagnosis + 1–2 days (replay); 1–2 days (VMR) |
| 6 | v9 Vulkan 1.3 reporting then JM compute pre-raster port | Step 1 (report Vulkan 1.3 core) unblocks DXVK entry on v9; Step 2 ports compute pre-raster path to JM job chains for GS, multi-viewport, fill mode, clip/cull | 1–3 days (audit & 1.3 reporting); 15–30 days (compute pre-raster port) |
| 7 | EXEC_INIT ordering | Prevents `-EPERM` warning and potential shader allocation failure on older kbase kernels | 6–12 h |
| 8 | Upload diagnostics | Logs kbase uAPI version, `TEXTURE_FEATURES`, mapper init lines, and broadens PanProbe logcat capturing | App/driver logging additions |

### Plan implementation notes
- **Item 1 (BC emulation):** Emulate all BC formats unless the full native mask (`0x1ff80`) is present. Code in `src/panfrost/vulkan/panvk_physical_device.c:1815-1826` and `panvk_vX_physical_device.c:294-302`. Maintain consistency with `panvk_image.c:674`.
- **Item 2 (Gralloc mapper):** Proper fix implements vendor-neutral mapper discovery in `src/util/u_gralloc/u_gralloc_fallback.c` (`allocator getIMapperLibrarySuffix` / declared passthrough instance, matching AOSP `Gralloc5.cpp`) plus HIDL mapper4 backend for vendor API < 34. Quick fallback via `PANVK_MAPPER_NAMES` or `ro.hardware.gralloc` hints provides partial relief.
- **Item 3 (v12+):** `depthBounds` emulation exists (patch 092) but requires validation before lifting gate `PAN_ARCH >= 10 && PAN_ARCH < 12` (`panvk_vX_physical_device.c:330`). `shaderOutputViewportIndex` gate at `panvk_vX_physical_device.c:443` requires lifting after viewport-run preparation support. `gs_viewport_depth` requires ordered replay runs updating packed VIEWPORT depth fields (`v12.xml:1954`, `csf/panvk_vX_cmd_draw.c:4388, 4502`).
- **Item 4 (G610 verification):** Needs PanProbe run with logcat on G610 to observe `vkCreateDevice`, queue group creation across layout versions (112-byte layout for >= 1.25 vs 32-byte for 1.6 vs 40-byte for 1.18), legacy 16-byte tiler heap allocation, and EXEC zone behavior on 5.10 kernel.
- **Item 5 (v9):** Replay crash occurs in `gpu_prerast_slice` after `gpu_written_indirect` when resubmitting cmdbuf (`tests/dxvk/vulkan/gpu_prerast_slice.c:488`, a repo test path, not Mesa). Likely fix (hypothesis, not yet proven by a descriptor dump): restore the GPU-written malloc-job draw payload (`src/panfrost/vulkan/jm/panvk_vX_cmd_draw.c:2309`) before resubmission. VMR secondary requires setting `flags_0.evaluate_per_sample` and updating `fb.nr_samples`.
- **Item 6 (v9):** Bundled DXVK requires Vulkan 1.3. Reporting 1.3 requires auditing feature booleans currently hidden by 1.1 reporting (`panvk_vX_physical_device.c:817`). Pre-raster compute lowering is CSF-only today (`src/panfrost/vulkan/meson.build:102`). Subsequent JM work includes tessellation (5–15 days), XFB (3–7 days), and indirect draws (2–5 days).
- **Item 7 (EXEC_INIT):** Call `KBASE_IOCTL_MEM_EXEC_INIT` before `JIT_INIT` in `src/panfrost/lib/kmod/kbase_kmod.c:1391-1410`. Size the JM zone (4 MiB now) sensibly.
- **Item 8 (Diagnostics):** Broaden PanProbe logcat collection and record `kbase` uAPI version and `TEXTURE_FEATURES` properties.

## Model table status

Hardware matching is performed in `src/panfrost/model/pan_model.c`. No "Unknown gpu_id" error appeared in any upload record (`src/panfrost/vulkan/panvk_physical_device.c:1213/1345`).

- **Vendor DDK and API levels seen:**
  - Mali-G57: DDK r38p1, first API level 33.
  - Mali-G610: first API level 31 (GL version not captured in PanPlay).
  - Mali-G615: DDK r44p1, first API level 34.
  - Mali-G720: DDK r49p1, first API level 202404.
  - All tested devices report `ro.hardware.gralloc = common`.
- **Recognised gpu_ids seen in uploads:**
  - `0x90930010`: arch 9.0, product 3, r0p1 -> `PAN_PROD_ID(9, 0, 3)` "G57" (`src/panfrost/model/pan_model.c:89`).
  - `0xa8670000`: arch 10.8, product 7, r0p0 -> `PAN_PROD_ID(10, 8, 7)` "G610" (`src/panfrost/model/pan_model.c:93`).
  - `0xb8a31030`: arch 11.8, product 3, r1p3 -> `PAN_PROD_ID(11, 8, 3)` v4 "G615" (`src/panfrost/model/pan_model.c:108`), shared by MC2 and MC6 variants.
  - `0xc8700010`: arch 12.8, product 0, r0p1 -> `PAN_PROD_ID(12, 8, 0)` v4 "G720" (`src/panfrost/model/pan_model.c:111`).
  - Mali-G715 MC7 (Pixel 8, issue #7): enumerates as `PAN_PROD_ID(11, 8, 2)` v4 (`src/panfrost/model/pan_model.c:106`).
- **Still-unseen GPUs in uploads:**
  - v10: G710, G510, G310
  - v12: G620, Immortalis-G720
  - v13: G625 (missing from model table), G725 (`src/panfrost/model/pan_model.c:113`), Immortalis-G925 (missing)
  - v14: G1 family (G1-Ultra/Premium/Pro present in `src/panfrost/model/pan_model.c:115-121`)

## GitHub issues cross-check

| Issue | Device / GPU | Backend data status | Likely cause | Doc link |
|---|---|---|---|---|
| #2 | Unspecified / GTA IV tester | No backend record | `VK_ERROR_OUT_OF_DEVICE_MEMORY` on staging buffer allocation; driver allocation leak or physical memory exhaustion | [v11/README.md](v11/README.md) |
| #4 | Poco X8 Pro (Dimensity 8500, G720-class) | No backend record (no upload shows Mali on system driver) | App did not load PanVK; fell back to vendor driver (reported Vulkan 1.3 / 150 extensions vs PanVK 1.4 / 188 extensions) | [v12/README.md](v12/README.md) |
| #5 | Mali-G610 MC6 & Mali-G615 (Poco X6 Pro) | 2 backend records on G610 (`559830af`, `0b151151`, both fail adapter selection); G615 heap usage not reproduced | G610 `vkCreateSwapchainKHR` assertion in Winlator fork matches gralloc blocker 2; G615 memory report relates to beta.13 upfront 160 MiB prerast arena commit | [v10/README.md](v10/README.md), [v11/README.md](v11/README.md) |
| #6 | Pixel 8 Pro (Mali-G715) | No backend record (no logs submitted) | Unknown (no logs); possibly the same gralloc mapper issue as #7 | [v11/README.md](v11/README.md) |
| #7 | Pixel 8 (Mali-G715 MC7, Google Tensor) | No backend record in uploads table; issue attachment log uses pre-beta.13 build | Black screen caused by `AllocateMemory` returning -1000072003 (`VK_ERROR_INVALID_EXTERNAL_HANDLE`) due to hard-coded MediaTek gralloc mapper on Tensor (or buffer/fd import failure) | [v11/README.md](v11/README.md) |

### Issue details and correlations
- **Issue #2:** Staging buffer allocation failure (`Failed to allocate staging buffer memory, res -2`) during GTA IV gameplay. Indicates device out of memory or a memory leak under repeated staging buffer creation. No upload record exists from this run.
- **Issue #4:** Poco X8 Pro running Winlator Mali 1.2 beta. The tester's screen showed Vulkan 1.3 and 150 extensions, which corresponds to the system Mali driver rather than PanVK (which reports Vulkan 1.4 with ~188 extensions and "PanVK-kbase"). PanVK was not loaded by the application.
- **Issue #5:** Covers two observations: a comment noting a `vkCreateSwapchainKHR` assertion failure on Mali-G610 in a Winlator fork (consistent with gralloc mapper failure), and tester reports of ~5–6 GB heap 0 usage on G615 under beta.13 compared to ~1.1 GB on beta.10. Patch 100 commits 160 MiB prerast arenas per `VkDevice` up front; the excessive heap usage reported was not reproduced in backend logs.
- **Issue #6:** Reports crash on Pixel 8 Pro (Mali-G715) without logs.
- **Issue #7:** Pixel 8 running a pre-beta.13 build (`git-5a07217f03`). Winlator wrapper logs reveal `AllocateMemory ... failed with result: -1000072003` (`VK_ERROR_INVALID_EXTERNAL_HANDLE`). This likely matches the gralloc mapper failure (Tensor has no `mapper.mediatek.so`), but a plain buffer/fd import failure is not ruled out.

## Records

Full set of 27 upload records from Cloudflare D1. Row `f907b333` is excluded from test results because it ran a non-release driver binary whose build ID differs from beta.14, failing immediately with 0/17 "no Mali". Tablet rows with repeated hardware specifications carry MT8755, Mali-G57 MC2, 0x90930010, kernel 5.15 android13, and Android 16.

| Anon id | Date (UTC) | App | Arch | Device | SoC | GPU | gpu_id | Kernel | Android | Driver | Result |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 5ec9f0c1 | 2026-10-05 02:20 | panprobe 1.2.0 | v9 | TB336FU | MT8755 | Mali-G57 MC2 | 0x90930010 | 5.15 android13 | 16 | beta.13 | PanProbe 0/1: "FAIL no Mali" (beta.13 has no v9) |
| dae37485 | 2026-10-05 02:24 | panprobe 1.2.0 | v9 | TB336FU | MT8755 | Mali-G57 MC2 | 0x90930010 | 5.15 android13 | 16 | beta.13 | 0/1 no Mali |
| 80b7a0c9 | 2026-10-05 02:25 | panprobe 1.2.0 | v9 | TB336FU | MT8755 | Mali-G57 MC2 | 0x90930010 | 5.15 android13 | 16 | beta.13 | 0/17 no Mali (+ app SIGBUS in bc_decode runner) |
| 079357e1 | 2026-10-05 02:29 | panplay 1.2.0 | v9 | TB336FU | MT8755 | Mali-G57 MC2 | 0x90930010 | 5.15 android13 | 16 | beta.13 | D3D8 x86 cube exit 5 |
| c4d22728 | 2026-10-05 02:47 | panprobe 1.2.0 | v9 | TB336FU | MT8755 | Mali-G57 MC2 | 0x90930010 | 5.15 android13 | 16 | beta.13 | 0/1 no Mali |
| 94f4d2bb | 2026-10-05 03:03 | panplay 1.2.0 | v9 | TB336FU | MT8755 | Mali-G57 MC2 | 0x90930010 | 5.15 android13 | 16 | beta.13 | D3D8 x86 cube: "Failed to enumerate physical devices, res -3", exit 137 |
| 70ce9223 | 2026-10-05 03:27 | panprobe 1.2.1 | v9 | TB336FU | MT8755 | Mali-G57 MC2 | 0x90930010 | 5.15 android13 | 16 | beta.13 | 0/1 |
| fad0cb63 | 2026-10-05 03:42 | panprobe 1.2.1 | v9 | TB336FU | MT8755 | Mali-G57 MC2 | 0x90930010 | 5.15 android13 | 16 | beta.14 | PanProbe 1/17 |
| ef4f82a2 | 2026-10-05 03:50 | panplay 1.2.1 | v9 | TB336FU | MT8755 | Mali-G57 MC2 | 0x90930010 | 5.15 android13 | 16 | beta.14 | D3D8 ARM64EC cube exit 5 |
| 6ff8855c | 2026-10-05 03:57 | panprobe 1.2.1 | v9 | TB336FU | MT8755 | Mali-G57 MC2 | 0x90930010 | 5.15 android13 | 16 | beta.14 | 1/17 |
| 7b576aa7 | 2026-10-05 04:05 | panplay 1.2.1 | v9 | TB336FU | MT8755 | Mali-G57 MC2 | 0x90930010 | 5.15 android13 | 16 | beta.14 | D3D9 ARM64EC cube exit 3: "Skipping: Device does not support Vulkan 1.3" |
| ec20d5f3 | 2026-10-05 04:14 | panprobe 1.2.1 | v11 | 2311DRK48G | MT6897 | Mali-G615 MC6 | 0xb8a31030 | 6.1 android14 | 16 | beta.14 | 0/1 (swapchain_lifecycle only run; FAIL) |
| 94a3d66c | 2026-10-05 04:17 | panprobe 1.2.1 | v12 | 2412DPC0AG | MT6899 | Mali-G720 MC7 | 0xc8700010 | 6.6 android15 | 16 | beta.14 | 14/17 (fail gs_viewport_depth, vs_viewport_index, depth_bounds) |
| 559830af | 2026-10-05 04:38 | panplay 1.2.1 | v10 | 23054RA19C | MT6896 | Mali-G610 MC6 | 0xa8670000 | 5.10 android12 | 15 | beta.14 | D3D10 ARM64EC cube exit 1 after 1 s |
| 0b151151 | 2026-10-05 04:41 | panplay 1.2.1 | v10 | 23054RA19C | MT6896 | Mali-G610 MC6 | 0xa8670000 | 5.10 android12 | 15 | beta.14 | D3D11 ARM64EC cube exit 1 after 1 s |
| e0944b04 | 2026-10-05 05:23 | panprobe 1.2.1 | v9 | TB336FU | MT8755 | Mali-G57 MC2 | 0x90930010 | 5.15 android13 | 16 | beta.14 | 1/17 |
| d3de3763 | 2026-10-05 05:29 | panplay 1.2.1 | v9 | TB336FU | MT8755 | Mali-G57 MC2 | 0x90930010 | 5.15 android13 | 16 | beta.14 | D3D9 ARM64EC cube exit 3 (Vulkan 1.3) |
| 7962f613 | 2026-10-05 08:21 | panprobe 1.2.0 | v11 | Infinix X6857 | MT6878 | Mali-G615 MC2 | 0xb8a31030 | 6.1 android14 | 16 | beta.13 (imported .so) | 16/17 (fail swapchain_lifecycle) |
| 29cd37fb | 2026-10-05 08:51 | panprobe 1.2.1 | none | 25053PC47G | SM8735 | Adreno 825 | - | 6.6 android15 | 16 | Qualcomm system driver | 0/1 geometry (not a Mali; PanVK not involved) |
| 54cdc0c7 | 2026-10-05 08:52 | panprobe 1.2.1 | v11 | Infinix X6857 | MT6878 | Mali-G615 MC2 | 0xb8a31030 | 6.1 android14 | 16 | beta.14 (imported .so) | 16/17 (fail swapchain_lifecycle) |
| bd79c8af | 2026-10-05 09:01 | panprobe 1.2.1 | v11 | 24090RA29G | MT6878 | Mali-G615 MC2 | 0xb8a31030 | 6.1 android14 | 16 | beta.14 | 16/17 (fail swapchain_lifecycle) |
| f907b333 | 2026-10-05 10:57 | panprobe 1.2.1 | v9 | TB336FU | MT8755 | Mali-G57 MC2 | 0x90930010 | 5.15 android13 | 16 | non-release .so (differs from beta.14) | 0/17 "no Mali"; EXCLUDED (non-release .so) |
| 32144abe | 2026-10-05 11:22 | panprobe 1.2.2 | v11 | 2311DRK48I | MT6897 | Mali-G615 MC6 | 0xb8a31030 | 6.1 custom kernel (dev device) | 16 | beta.15 | 17/17 |
| e63009bd | 2026-10-05 11:29 | panplay 1.2.2 | v11 | 2311DRK48I | MT6897 | Mali-G615 MC6 | 0xb8a31030 | 6.1 custom | 16 | beta.15 | D3D9 i686 cube exit 0 (works) |
| 49abcff8 | 2026-10-05 11:36 | panprobe 1.2.2 | v9 | TB336FU | MT8755 | Mali-G57 MC2 | 0x90930010 | 5.15 android13 | 16 | beta.15 | 1/17 |
| 71dc96c0 | 2026-10-05 11:42 | panprobe 1.2.2 | v11 | 2311DRK48I | MT6897 | Mali-G615 MC6 | 0xb8a31030 | 6.1 custom | 16 | beta.15 | 17/17 |
| f2fc9ca6 | 2026-10-05 11:45 | panplay 1.2.2 | v11 | 2311DRK48I | MT6897 | Mali-G615 MC6 | 0xb8a31030 | 6.1 custom | 16 | beta.15 | D3D9 i686 cube exit 0 |

## Methodology

- **Data source:** Cloudflare D1 database `panvk-uploads`, table `uploads` (27 rows spanning ids 2 to 29; ids 1 and 16 absent). Extracted columns include app, version, device_model, soc, gpu_model (GL_RENDERER), gpu_id, arch, driver_version, android_version, game, exit_code, and extra_json(glVersion).
- **Blob storage & verification:** Upload payloads were retrieved from Workers KV (26 blobs under path `blob/<sha256>`) and one external file host (1 blob). No URLs or access keys are recorded. All 27 archive zips were successfully fetched and verified against their SHA-256 digests.
- **Anonymous identifier:** Records are identified solely by an 8-character anonymous ID computed as the first 8 hex characters of `sha256("panvk:" + upload_sha256)`. Raw log files remain strictly in gitignored `tmp/universal-logs/`.
- **Extraction & code verification:** Analysis was performed using automated extraction scripts over the retrieved archives. Root-cause analyses were produced via model bridge (codex gpt-6.1-sol) and spot-verified directly against the driver source tree.
- **Source tree:** Mesa repository with the beta.16 patch series (base commit `5a07217f` + csf-v11 patches 001–107 + jm-v9 patches 001–003 + 099 version bump).

## Data gaps

- **No kbase uAPI version in uploads:** Upload records do not log the kernel kbase uAPI version. Kernel uAPI levels are known only from offline device worklogs (e.g., G57 tablet = JM uAPI 11.0; dev G615 = CSF 1.21).
- **No TEXTURE_FEATURES hardware mask:** Uploads omit the raw `TEXTURE_FEATURES` bitmask, preventing definitive hardware-level confirmation of reported native BC formats on G57 and G610.
- **Restricted PanProbe logcat:** PanProbe logcat captures are restricted to warning level and minimal byte counts (0 bytes on G720 record `94a3d66c`, 320 bytes on `bd79c8af`), dropping essential info-level messages such as mapper initialization paths.
- **No vulkaninfo in PanPlay:** PanPlay upload archives contain launcher and game exit logs but lack `vulkaninfo` diagnostic dumps.
- **No beta.16 tester uploads:** The D1 database snapshot does not yet include beta.16 user uploads; beta.16 status is verified via the repository CHANGELOG (dev G615 17/17, v9 tablet 1/17).
- **No data for v13 or v14 architectures:** No test devices or upload records exist for 5th-gen CSF (v13: G625, G725, Immortalis-G925) or v14 (G1 family).
