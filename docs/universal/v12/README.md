# Mali v12 (5th-gen CSF) status

All file:line references throughout this document are relative to the Mesa root of the beta.16 tree (Mesa 5a07217f + csf-v11 series up to 107 + jm-v9 001-003), written as `src/panfrost/...:NNN`.

## Summary
The Mali v12 (5th-generation Valhall CSF) backend achieved the best non-G615 result in the test suite: PanProbe scores 14/17 on beta.14 on Mali-G720 MC7. The three failing tests are architectural feature gates (`depthBounds`, `shaderOutputViewportIndex`) and missing per-viewport depth runs on v12. Real-world 3D games have not yet been tested under PanPlay or DXVK. Mali-G720 MC7 has been observed; Mali-G620 and Immortalis-G720 belong to the same architecture but remain unseen.

## GPUs and devices tested

| Driver build | Anon ID(s) | Device | SoC | GPU | gpu_id -> Mesa model | Kernel | Android | App |
|---|---|---|---|---|---|---|---|---|
| beta.14 | `94a3d66c` | 2412DPC0AG (Poco X7 Pro) | MT6899 | Mali-G720 MC7 | `0xc8700010` -> G720 | 6.6 android15 (4 KiB pages) | 16 | PanProbe 1.2.1 |

The tested device SoC reports MT6899. The hardware identifier `0xc8700010` decodes to architecture 12.8, product 0, revision r0p1, which matches `PAN_PROD_ID(12,8,0)` v4 for "G720" in `src/panfrost/model/pan_model.c:111`.

Other 5th-generation Valhall CSF models—specifically Mali-G620 and Immortalis-G720 (found in MediaTek Dimensity 9300)—belong to the same v12 architecture but have not been observed in any uploaded records. Neither model has reported an "Unknown gpu_id" error (`src/panfrost/vulkan/panvk_physical_device.c:1213/1345`).

## Kernel / kbase interface seen
- **Kernel version:** Linux 6.6 android15 with 4 KiB MMU page size.
- **kbase interface:** Command Stream Frontend (CSF) backend; CSF uAPI ~1.30 per plan document (not logged in upload telemetry).
- **Vendor DDK:** GLES r49p1 (`GL_VERSION` from vendor driver).
- **Vendor API level:** `ro.board.first_api_level = 202404` (Android 15 vendor API), `ro.hardware.gralloc = common`.
- **Vulkan API version:** Vulkan 1.4.363 exposed by PanVK.
- **Queues & memory:** `queueCount = 2`; 8.9 GB device memory heap reported.
- **Swapchain:** Android surface swapchain creation passes (`swapchain_lifecycle` PASS; vendor-provided gralloc mapper functions properly).
- **Vulkaninfo comparison vs G615:** Exposes an identical set of 188 Vulkan extensions. Core and extension features match the G615 dev device except `depthBounds = false`, `shaderOutputViewportIndex = false`, and `shaderDeviceClock = false`. 16x MSAA is supported (31 sample counts vs 29 on G615). Note that `shaderDeviceClock` reflects kernel timestamp coherency (`src/panfrost/lib/kmod/kbase_kmod.c:430` -> `src/panfrost/vulkan/panvk_vX_physical_device.c:661`), not an architectural feature gate.

## Per-test results

The table below contrasts results between Mali-G720 MC7 on beta.14 (`94a3d66c`) and the reference Mali-G615 MC6 dev device on beta.15 (`32144abe`, `71dc96c0`, 17/17):

| Test | Mali-G720 beta.14 (`94a3d66c`) | Mali-G615 beta.15 (`32144abe`) | Notes / Failure description |
|---|---|---|---|
| `gpu_prerast_slice` | PASS | PASS | Passes all IDVS and GPU-written indirect slice tests |
| `clip_cull` | PASS | PASS | Passes clip and cull distance verification |
| `multi_viewport` | PASS | PASS | Passes multi-viewport rendering |
| `fill_mode` | PASS | PASS | Passes point, line, and polygon fill modes |
| `bc_decode` | PASS | PASS | Passes via BC emulation (GPU decode) |
| `geometry` | PASS | PASS | Passes geometry shader execution |
| `tessellation` | PASS | PASS | Passes tessellation control and evaluation shaders |
| `xfb` | PASS | PASS | Passes transform feedback streaming |
| `pipeline_stats` | PASS | PASS | Passes pipeline statistics queries |
| `vertex_stores` | PASS | PASS | Passes vertex pipeline stores and atomics |
| `gs_viewport_depth` | FAIL | PASS | `FAIL case A_clamp`, `B_clamp_clip`, `C_noclamp`, `RESULT FAIL` (depth clamp packed into viewport on v12) |
| `vs_viewport_index` | FAIL | PASS | `FAIL shaderOutputViewportIndex not reported`, `FAIL CreateGraphicsPipelines r=-13` |
| `depth_bounds` | FAIL | PASS | `FAIL depthBounds not reported`, `RESULT FAIL` (gated by `PAN_ARCH < 12`) |
| `large_draw` | PASS | PASS | Passes large draw execution with XFB |
| `vmr_secondary` | PASS | PASS | Passes variable multisample rate secondary execution |
| `tess_cond_state` | PASS | PASS | Passes tessellation conditional rendering |
| `swapchain_lifecycle` | PASS | PASS | Passes swapchain create, acquire, present, and resize on Android surface |

## Failures and log excerpts

### Viewport depth clamp failure (`gs_viewport_depth.log`)
```
PHYS 0 Mali-G720 MC7 vendor=0x13b5 device=0xc8700010
FEATURE geom=1 multiVp=1 depthClamp=1 clipExt=1 depthClipEnable=1
ALLOC image fmt=37 type=0 bits=0x7 flags=0x1
ALLOC image fmt=126 type=0 bits=0x7 flags=0x1
FAIL case A_clamp depthL=[0.375000..0.375000] expL=0.250000 depthR=[0.000000..0.000000] expR=0.500000 badDepth=1024 badColor=0
FAIL case B_clamp_clip depthL=[0.375000..0.375000] expL=0.125000 depthR=[0.250000..0.250000] expR=0.750000 badDepth=1024 badColor=0
FAIL case C_noclamp depthL=[0.375000..0.375000] expL=0.125000 depthR=[0.250000..0.250000] expR=0.750000 badDepth=1024 badColor=0
RESULT FAIL
```
All three cases write wrong depth for both viewports. This is consistent with the v12 path below (no per-viewport depth runs; one union depth range for all viewports). The exact numbers were not traced with a replay.

### Vertex shader viewport index rejection (`vs_viewport_index.log`)
```
PHYS 0 Mali-G720 MC7 vendor=0x13b5 device=0xc8700010
FEATURE multiVp=1 depthClamp=1 tess=1 shaderOutputViewportIndex=0 shaderOutputLayer=1 clipExt=1 depthClipEnable=1
FAIL shaderOutputViewportIndex not reported
ALLOC image fmt=37 type=0 bits=0x7 flags=0x1
ALLOC image fmt=126 type=0 bits=0x7 flags=0x1
FAIL CreateGraphicsPipelines r=-13 line=550
```
Pipeline creation fails with `r=-13` (`VK_ERROR_UNKNOWN` / pipeline compilation failure) when requesting unadvertised shader viewport index output.

### Depth bounds feature rejection (`depth_bounds.log`)
```
PHYS 0 Mali-G720 MC7 vendor=0x13b5 device=0xc8700010
FEATURE depthBounds=0
FAIL depthBounds not reported
RESULT FAIL
```
The test terminates immediately because `depthBounds` feature reporting is disabled by architecture version checks.

## Root causes
- **Depth bounds gate:** `panvk_vX_physical_device.c:330` explicitly restricts depth bounds support to `.depthBounds = PAN_ARCH >= 10 && PAN_ARCH < 12`. Emulation logic (patch 092: `LD_TILE` depth read and sample-masking at `src/panfrost/vulkan/panvk_vX_shader.c:2393` and `src/panfrost/vulkan/csf/panvk_vX_cmd_draw.c:2629`) is implemented for Valhall CSF but has not been validated on v12 hardware.
- **VS viewport index gate:** `panvk_vX_physical_device.c:443` disables `shaderOutputViewportIndex` on v12+. Viewport-run preparation returns false on v12+ at `src/panfrost/vulkan/csf/panvk_vX_cmd_draw.c:4709-4710`.
- **Per-viewport depth clamp packaging:** Mali v12 packs depth clamp values directly into the `VIEWPORT` descriptor (`src/panfrost/genxml/v12.xml:1954`). Per-run `LOW/HIGH_DEPTH_CLAMP` register updates from patch 089 are guarded by `#if PAN_ARCH < 12` at `src/panfrost/vulkan/csf/panvk_vX_cmd_draw.c:4388-4390`. Furthermore, run splitting is disabled on v12 at `src/panfrost/vulkan/csf/panvk_vX_cmd_draw.c:4502-4504` (noting that splitting runs would modify index buffers while still clamping to viewport 0; also `:4709`). The driver instead programs the union of all viewports' depth ranges at `src/panfrost/vulkan/csf/panvk_vX_cmd_draw.c:1038` (`panvk_prerast_depth_union`), which causes per-viewport depth tests to fail.
- **Timestamp coherency:** `shaderDeviceClock` is disabled on this device due to lack of kernel timestamp coherency (`src/panfrost/lib/kmod/kbase_kmod.c:430` -> `src/panfrost/vulkan/panvk_vX_physical_device.c:661`), rather than an architectural limitation.

## What the driver lacks on this arch
- Per-viewport depth support via packed `VIEWPORT` descriptor fields during run replay.
- Validated depth-bounds emulation on v12 (`LD_TILE` depth read and sample masking).
- Vertex/tessellation evaluation shader viewport index run splitting and pipeline preparation on v12+.
- Untested on v12: beta.15 and beta.16 heap and semaphore management updates (patches 102–107).
- Untested on v12: PanPlay and DXVK real-world game execution.
- Untested variants: Mali-G620 and Immortalis-G720.

## Fix plan

| Rank | Item | Effort | Unblocks |
|---|---|---|---|
| 1 | PanPlay game run on G720 with beta.16 | Data collection | Validates 3D gaming and patches 102–107 on v12 |
| 2 | Per-viewport depth runs on v12 (replay ordered runs updating packed VIEWPORT depth fields) | 16–32 h | Fixes `gs_viewport_depth` (PASS) |
| 3 | Enable `shaderOutputViewportIndex` on v12 (prepare viewport runs) | 4–8 h (after rank 2) | Fixes `vs_viewport_index` (PASS) |
| 4 | Validate depth-bounds emulation on v12 and lift gate | 8–16 h | Fixes `depth_bounds` (PASS, 17/17 PanProbe) |

The same architectural gates and fixes apply to Mali v13 and v14 (+8–16 h each).

## Open questions / data needed from testers
- **Hardware variants:** Need `gpu_id` values and variant strings for Immortalis-G720 (MediaTek Dimensity 9300) and Mali-G620.
- **Third-party launcher behavior (Issue #4):** On a Poco X8 Pro (Dimensity 8500, G720-class), a third-party launcher loaded the vendor system driver (Vulkan 1.3 / 150 extensions) rather than PanVK. A PanProbe test archive is needed from that device to capture PanVK behavior.
- **Verification of recent series:** Verify whether beta.15/16 patches 102–107 (tiler heap renewal without drain, GPU-side semaphore waits, worker-thread heap creation, trace reset, sw-WSI present-thread fence wait, three retired tiler heaps) work on v12 hardware.

## Links
- [Universal Mali status](../README.md)
- [Viewport Clamp Worklog](../../../worklogs/driver-remaining/089-viewport-clamp.md)
- [VS Viewport Index Worklog](../../../worklogs/driver-remaining/090-vs-viewport-index.md)
- [Depth Bounds Worklog](../../../worklogs/driver-remaining/092-depth-bounds.md)
