# PanVK Game Launcher Design (Android, Mali G615 First)

**Document Status:** Technical Architecture & Implementation Plan  
**Target Hardware Anchor:** MediaTek Dimensity 8300-Ultra (MT6897, Poco X6 Pro `duchamp`), Mali-G615 MC6 (`0xb8a31030`, Pan arch v11, CSF frontend, `/dev/mali0`, `mali_kbase` CSF uAPI 1.21)  
**Primary Source Evidence:** research notes (2026-10-01: `panvk-present-readiness.md`, `winlator-research/VERIFY.md`, `display-research/DISPLAY-ALTERNATIVES.md`), `README.md`, `worklogs/g615-dxvk/PROGRESS.md`, `docs/plans/PANVK_MASTER_ROADMAP.md`, `docs/plans/PANVK_UNIVERSAL_MALI_PLAN.md`

---

## 1. Goals / Non-Goals

### Current launcher acceptance boundary (2026-10-02)

Resolved normal ARM64EC clear/present smoke: upstream DXVK v3.1.1 one-line
`endCurrentPass(false)` before external rendering materializes deferred clears.
Normal DX8/9/10/11 CLI and actual launcher UI captures all 76800/76800 orange;
same-source unpatched DX11 control black. No staging/Flush/EVENT/fbread injection.
Driver unchanged. ARM64EC+i686 runtime built; i686 mapping/game support still
unverified. Evidence/provenance: `apps/panvk-launcher/tests/results/final-discrimination/runtime-fix/README.md`.
Earlier failure investigation below is historical, not the current smoke result.

### Goals
1. **Zero-CPU-copy presentation in normal operation:** Directly present GPU-rendered `AHardwareBuffer` (AHB) swapchains via `SurfaceControl` (`ASurfaceTransaction_setBuffer`) to SurfaceFlinger / Hardware Composer (HWC) without intermediate CPU readbacks, format conversions, or `memcpy` stalls (`DISPLAY-ALTERNATIVES.md:10, 33-35`).
2. **GPU-only execution path:** Production graphics execution executes strictly on the GPU hardware and compute engines; no CPU texture decoding, no CPU shader fallback, no software rasterization, and no CPU readback-and-rebuild loops (`README.md:126-127`, `docs/plans/PANVK_MASTER_ROADMAP.md:26-28`). Specifically:
   - BC1-7 texture compression uses native compute decode on the GPU (CTS BC subset 1863 pass / 0 fail; copy_and_blit 9620 / 0) (`README.md:134`, `worklogs/g615-dxvk/PROGRESS.md:11`).
   - Vertex, geometry, and tessellation stages run as pre-raster compute kernels on the GPU (geometry 189/0, tessellation 526/0) (`README.md:131-132`, `worklogs/g615-dxvk/PROGRESS.md:17`).
   - Transform feedback uses GPU capture kernels (CTS transform_feedback 15793/0 with 2 intermittent DeviceLost, pre-existing) (`README.md:133`, `worklogs/g615-dxvk/PROGRESS.md:17`).
   - DXVK Native v3.1.1 on G615 creates a D3D11 device at feature level 11_0 (`D3D11 HRESULT=0x00000000 feature_level=0xb000`) (`README.md:124-125`).
3. **Strict capability truth:** Expose only Vulkan extensions, features, and limits verified by CTS and on-device workloads; never expose fake feature bits or spoof D3D feature levels (`README.md:74-79`, `docs/plans/PANVK_MASTER_ROADMAP.md:32`).
4. **Sequenced hardware ladder:** Mali-G615 reference anchor first (M1 DXVK -> M2 vkd3d-proton -> Wine consumer testing), followed by universal Mali rollout per `docs/plans/PANVK_UNIVERSAL_MALI_PLAN.md:10-60`. Only G615 / arch 11 / CSF established; other waves per universal plan, unverified:
   - *Wave A0:* Valhall v11 CSF anchor (Mali-G615, MT6897, uAPI 1.21; our anchor profile `g615-v11-csf`).
   - *Waves A1–F:* Per universal plan, unverified (Wave A1 5th-gen v12 CSF; Wave B Valhall v9 JM; Wave C Bifrost v7 JM; Wave D Valhall v10 / other CSF; Wave E Bifrost v6 JM; Wave F Midgard v4/v5 JM; Utgard is Lima GLES2 only, no Vulkan planned).

### Non-Goals
1. **No VirGL or intermediate GL emulation:** PanVK operates directly as a native Vulkan ICD communicating with `/dev/mali0` (`README.md:3-5`).
2. **No CPU presentation fallback in production:** Completely eliminate patch `wsi/016` CPU `memcpy` (`panvk-present-readiness.md:23-25`) and X11 CPU SHM copies.
3. **No fake feature bits:** Features like `robustImageAccess2`, sparse, or geometry are never spoofed to bypass consumer checks (`README.md:77-79`, `worklogs/g615-dxvk/PROGRESS.md:56-62`).
4. **No root, system HAL replacement, or SELinux tampering:** Normal app-UID operation against `/dev/mali0` without root permissions or system modifications (`docs/plans/PANVK_UNIVERSAL_MALI_PLAN.md:166-168`).
5. **No Wayland compositor on Mali initially:** Bannerlator Wayland is Adreno-only; Mali lacks a public non-root dma-buf -> AHB wrapper (`DISPLAY-ALTERNATIVES.md:26, 30, 57`).
6. **No AVF / VM emulation:** Avoid AVF Linux Terminal GPU (gfxstream on Pixel 10 only) and virtio-gpu / Venus emulation layers (`DISPLAY-ALTERNATIVES.md:16, 40-41`).

---

## 2. Verified Architecture Diagram (ASCII)

```text
Game (Windows x86_64 PE)
  │ [Verified in Wine/Winlator ecosystem: VERIFY.md:14-17]
  ▼
DXVK v3.1.1 (FL11_0) / vkd3d-proton (D3D12)
  │ [DXVK Native FL11_0 proven; vkd3d native device run pending (README.md:124-125, PROGRESS.md:42-47); Wine integration UNPROVEN on PanVK]
  ▼
Wine (winex11.drv + winevulkan) under Box64 / FEX (Bionic / glibc container)
  │ [Verified: Guest X11 surface interception via implicit layer: VERIFY.md:14-17, 103-109]
  ▼
Vulkan Implicit Layer (e.g. VK_LAYER_WINLATOR_ahb_direct) or WSI Wrapper
  │ [Verified: Ludashi-Plus on Adreno: VERIFY.md:14-27; UNPROVEN on PanVK/Mali]
  ▼
PanVK Bionic ICD (libvulkan_panfrost.so, exports HAL HMI)
  │ [Verified: adrenotools __loader_android_create_namespace: VERIFY.md:120-130]
  │ [Verified: PanVK exports HMI: scripts/build-android.sh:2,38, VERIFY.md:130-131]
  ▼
Kernel Interface /dev/mali0 (mali_kbase, CSF uAPI 1.21)
  │ [Verified: PanVK UMM import/render: panvk-present-readiness.md:12-13]
  │ [UNPROVEN: Real GPU sync_fd export without /dev/sw_sync: panvk-present-readiness.md:17-18]
  ▼
AHardwareBuffer (AHB, USAGE_GPU_COLOR_OUTPUT | USAGE_COMPOSER_OVERLAY)
  │ [Verified: PanVK AHB import/render: validation/.../beta3-phase8-ahb-allocation-*.txt]
  │ [UNPROVEN on G615: AFBC modifier scanout correctness: panvk-present-readiness.md:11]
  ▼
SurfaceControl (ASurfaceTransaction_setBuffer with acquire fence)
  │ [Verified: Ludashi DisplayX / Bannerlator DirectScanout: VERIFY.md:48-58]
  │ [UNPROVEN with PanVK: Zero-copy setBuffer presenter: panvk-present-readiness.md:24, 40]
  ▼
SurfaceFlinger / HWC (Hardware Composer Zero-Copy Scanout)
```

### Verification Boundary: Other Projects vs PanVK
- **Verified in Other Projects:**
  - Wine guest X11 surface interception via implicit Vulkan layer `VK_LAYER_WINLATOR_ahb_direct` (`VERIFY.md:14-17`).
  - Isolated Android namespace creation via `liblinkernsbypass` calling private API `__loader_android_create_namespace` in `libdl_android` (`android_linker_ns.cpp:182`, `VERIFY.md:120-130`).
  - Interception of `vulkan.*` HAL loads via `libmain_hook.so` (`main_hook.c:3`, `hook_impl.cpp:47, 129`).
  - Zero-copy AHB presentation via `ASurfaceTransaction_setBuffer` in Ludashi DisplayX and Bannerlator DirectScanout (`VERIFY.md:48-58`).
  - Box64/FEX execution of x86_64 Windows PE binaries under Wine on ARM64 Android (`VERIFY.md:16-17`, `docs/plans/PANVK_UNIVERSAL_MALI_PLAN.md:141-152`).
- **Verified in PanVK:**
  - Native DXVK FL11_0 draw workloads passing on G615 hardware (`README.md:124-139`).
  - `/dev/mali0` kbase UMM dma-buf memory import (`BASE_MEM_IMPORT_TYPE_UMM`) with `COHERENT_SYSTEM` (`kbase_kmod.c:1445-1519`, `panvk-present-readiness.md:12-13`).
  - `VK_ANDROID_external_memory_android_hardware_buffer` allocation, import, and triangle render (`panvk-present-readiness.md:13`).
  - Driver export of Android HAL symbol `HMI` in bionic build (`scripts/build-android.sh:2,38`, `VERIFY.md:130-131`).
- **Unproven with PanVK (Requires Hardware Proof):**
  - Running PanVK directly inside a Wine/Box64/FEX container environment (`docs/plans/PANVK_MASTER_ROADMAP.md:67`).
  - Zero-copy AHB presentation to `SurfaceControl` without patch `wsi/016` CPU `memcpy` (`panvk-present-readiness.md:23-25`).
  - Kernel-backed `sync_fd` export/import without `/dev/sw_sync` (`panvk-present-readiness.md:17-18`).
  - Hardware Composer compatibility with Mali gralloc AFBC modifiers on G615 (Mali gralloc may select AFBC; needs target validation; `panvk-present-readiness.md:11, 41`).

---

## 3. Three Present Modes

### Mode Summary Matrix

| Present Mode | Data Flow | Copies | Sync (Fences) | PanVK Prerequisites | Primary Gap (`panvk-present-readiness.md` §6) | Fallback Hierarchy |
|---|---|:---:|---|---|---|---|
| **DIRECT_AHB** | DXVK -> AHB `VkImage` -> `ASurfaceTransaction_setBuffer` -> HWC | **0** | GPU completion `sync_fd` acquire fence; SurfaceControl release fence | Color-renderable AHB `VkImage`, real `SYNC_FD` export, AFBC/linear agreement | Real `sync_fd` without `/dev/sw_sync` (**L**) | On format/usage failure, fall back to **AHB_BLIT** |
| **AHB_BLIT** | DXVK -> Native `VkImage` -> `vkCmdBlitImage` -> AHB `VkImage` -> SurfaceControl | **1 GPU blit** (0 CPU) | Blit completion `sync_fd` acquire fence; SurfaceControl release fence | Working transfer/blit to imported AHB, BGRA->RGBA conversion, MSAA resolve | Format conversion & MSAA resolve (**S**); `sync_fd` (**L**) | On blit or AHB failure, fall back to **DRI3_VULKAN** |
| **DRI3_VULKAN** | DXVK -> X11 swapchain -> Unix socket AHB (mod 1255) -> X server FLIP | **0–1 GPU** (0 CPU) | X11 Present / DRI3 implicit sync or xshmfence | DRI3 raw dma-buf export (`PANVK_KBASE_DRI3`), linear modifier negotiation | Raw dma-buf proof (**M**); X11 teardown hang (**M**) | CPU SHM is **debug only**, never production default |

### 3.1 DIRECT_AHB (Zero-Copy Direct Scanout)
- **Exact Data Flow:**
  1. Game issues D3D calls; DXVK renders directly into swapchain `VkImage`s allocated from a host AHB pool.
  2. Swapchain AHBs are allocated with `AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_COMPOSER_OVERLAY` (`VERIFY.md:19-20`).
  3. Vulkan implicit layer intercepts `vkQueuePresentKHR`, emits an empty queue submit (or attaches DXVK completion semaphores), exports a `sync_fd` fence, and sends the buffer to `ASurfaceTransaction_setBuffer(txn, sc, ahb, acq_fence)` (`VERIFY.md:24-29`).
- **Number of Copies:** Zero-copy target; receiver blit and HWC overlay acceptance remain conditional (`VERIFY.md:31-36, 117-118`).
- **Sync (Fences):** The exported GPU completion fence is passed as `acquire_fence_fd`. Implement SurfaceControl release callback handling; amphora semaphore handoff describes BufferQueue dequeue fences (`DISPLAY-ALTERNATIVES.md:33-35`).
- **PanVK Requirements:** `VK_ANDROID_external_memory_android_hardware_buffer` (present, `panvk-present-readiness.md:6-9`), color-attachment-renderable AHB `VkImage`, genuine `SYNC_FD` export, and AFBC/linear modifier agreement with gralloc.
- **Gap Items & Sizes (`panvk-present-readiness.md:38-44` §6):**
  - *Gap 1 (Size: L):* Real GPU-completion `sync_fd` without `/dev/sw_sync`.
  - *Gap 2 (Size: M):* Swapchain-equivalent over AHB pool in layer/app; replaces `wsi/016` CPU `memcpy`.
  - *Gap 3 (Size: M):* Validate AHB usages (`COMPOSER_OVERLAY`), BGRA formats, and gralloc AFBC modifiers on Android.
  - *Gap 4 (Size: S):* Cache/coherency check (`COHERENT_SYSTEM` UMM import via `kbase_kmod.c:1445-1519`; verify no stale lines on HWC scanout).
  - *Gap 5 (Size: S):* Clean up signaler thread churn (replace per-export detached pthread in `panvk_physical_device.c:1027-1051`).
  - *Gap 6 (Size: S):* Plumb custom loader into DXVK under Wine.
- **Fallback Rules:** If AHB allocation or import fails (e.g. BGRA unsupported as AHB color attachment or AFBC scanout corruption occurs), immediately fall back to **AHB_BLIT**. Sync fallback (`vkWaitForFences` on CPU and passing fence `-1`) is strictly an interim debugging measure (`panvk-present-readiness.md:39`).

### 3.2 AHB_BLIT (One GPU Blit / Trojan-Blit)
- **Exact Data Flow:**
  1. DXVK renders into an internal driver-native `VkImage` (optimal driver tiling and native format).
  2. On present, the layer/driver executes a single `vkCmdBlitImage` (or compute blit) from the render image into an imported AHB `VkImage` (`VERIFY.md:26-27`).
  3. The imported AHB is submitted to `SurfaceControl` via `ASurfaceTransaction_setBuffer`.
- **Number of Copies:** One layer blit; receiver may perform an additional GPU blit (0 CPU copies) (`VERIFY.md:26-34`).
- **Sync (Fences):** Blit completion signals the `sync_fd` acquire fence for `SurfaceControl`. The release fence gates the subsequent blit into that specific AHB buffer.
- **PanVK Requirements:** Small RGBA8 AHB import/render proven (`tests/ahb/ahb.c`, `panvk-present-readiness.md:13`); blit conversion (e.g. BGRA source to RGBA AHB) and MSAA resolve need testing (`panvk-present-readiness.md:46, 48`).
- **Gap Items & Sizes (`panvk-present-readiness.md:45-49` §6):**
  - *Gap 1 (Size: S):* Small RGBA8 AHB import/render proven (`tests/ahb/ahb.c`); generic transfer/blit conversion and MSAA resolve need testing.
  - *Gap 2 (Size: L, tolerates S interim):* Same sync gap as DIRECT_AHB #1, but tolerates CPU wait fallback.
  - *Gap 3 (Size: S):* Format conversion via blit (BGRA source -> RGBA AHB) and MSAA resolve.
  - *Gap 4 (Size: S):* AHB usage validation; linear layout is fully acceptable for the blit target.
- **Fallback Rules:** Primary fallback from DIRECT_AHB when format swizzling or AFBC layout fails. If blit execution or AHB import fails, fall back to **DRI3_VULKAN**.

### 3.3 DRI3_VULKAN (X11 Direct Scanout / Compatibility Mode)
- **Exact Data Flow:**
  1. DXVK renders to an X11 window swapchain using `vkCreateXlibSurfaceKHR`.
  2. `leegao/bionic-vulkan-wrapper`'s Mesa fork implements this chain (allocates AHB swapchain images, sends handles over Unix socket at `:1930`, sets modifier 1255 at `:1932`, and creates pixmaps via `xcb_dri3_pixmap_from_buffers` at `:1949`; `VERIFY.md:103-109`); PanVK raw DRI3 remains unproven.
  3. The Java X server DRI3 extension forwards the buffer to the host compositor (`DRI3Extension.java:145-179`).
  4. The host Present extension flips the AHB pixmap directly to `SurfaceControl` (zero copy) or blits it via host Vulkan (`PresentExtension.java:280-314`, `VERIFY.md:55-58`).
- **Number of Copies:** **0–1 GPU copy** (0 if DirectScanout FLIP succeeds; 1 if host compositor blits; 0 CPU copies in DRI3 mode).
- **Sync (Fences):** X11 Present / DRI3 implicit sync or xshmfence. Because kbase has no implicit dma-buf fences on user BOs, requires an explicit CPU wait before `PresentPixmap` (`panvk-present-readiness.md:54`).
- **PanVK Requirements:** DRI3 raw dma-buf export (`PANVK_KBASE_DRI3=raw|termux`), server-side import compatibility, and linear modifier negotiation. Note: `can_present_on_device` requires `DRM_BUS_PLATFORM` (`panvk_wsi.c:36-48`).
- **Gap Items & Sizes (`panvk-present-readiness.md:50-55` §6):**
  - *Gap 1 (Size: M):* DRI3 on kbase is opt-in env only, no device proof: validate `PixmapFromBuffers` with raw dma-buf fd against Winlator X server.
  - *Gap 2 (Size: M):* Fix X11 present teardown hang (`x11_wait_for_present` in `wsi_common_x11.c`; `PROGRESS.md:49-55`).
  - *Gap 3 (Size: M):* Server-side import: host Vulkan driver must import PanVK dma-buf without cross-driver AFBC mismatch (force linear).
  - *Gap 4 (Size: M):* Sync: kbase lacks implicit fences; requires synchronization before `PresentPixmap`.
  - *Gap 5 (Size: S):* sw (SHM) fallback already default in `panvk_wsi.c`: works but incurs CPU copy.
- **Fallback Rules:** Used when standalone Android WSI layers fail. The CPU SHM path (`sw_device=true`) is strictly a **debug fallback** for triage, never a default mode.

---

## 4. Component Choices Table

| Repository | File(s) to Reuse / Reference | License | Reuse Verdict | Notes & Citations |
|---|---|---|---|---|
| `TripleJ160/Winlator-Ludashi-Plus` | `dlls/vulkan_layer/ahb_layer.c`, `AHardwareBufferPool.java`, `AHBSocketServerComponent.java`, `VulkanRendererContext.cpp` | MIT (BrunoSX) | **Reusable** | Core DAC WSI layer (Direct-Render & Trojan-Blit). Intercepts DXVK swapchain, imports AHB over Unix socket (`VERIFY.md:13-27`). |
| `StevenMXZ/Winlator-Ludashi` | `app/src/main/cpp/winlator/renderer/displayx.cpp`, `DRI3Extension.java`, `VulkanRendererContext.cpp` | MIT | **Reusable** | DisplayX SurfaceControl HWC presenter (`ASurfaceTransaction_setBuffer`) and DRI3 pixmap handling; landed in v4.0 (`VERIFY.md:48-51`). |
| `libadrenotools` (Billy Laws) | `driver.cpp`, `android_linker_ns.cpp`, `main_hook.c`, `hook_impl.cpp` | BSD-2-Clause | **Reusable** | Isolated namespace driver loading via `__loader_android_create_namespace` in `liblinkernsbypass` (`VERIFY.md:120-130`). |
| `brunodev85/winlator` | Container manager, X server core, Box64/Wine runner | LGPL-2.1 | **Reusable** | Official Winlator upstream (`b6b2259`); reuse launcher harness under LGPL-2.1 dynamic linking boundary (`VERIFY.md:88`). |
| `amphora-dev/amphora` | `VK_LAYER_AMPHORA_wsi`, `docs/05-AHB-IMPORT-PRESENT.md` | unverified | reference only | BufferQueue AHB import and release fence handoff patterns; warning on `SET_BUFFERS_FORMAT(BGRA=5)` device crash (`DISPLAY-ALTERNATIVES.md:35, 45`). |
| `ARM vulkan-wsi-layer` | `vulkan-wsi-layer` | unverified | reference only | Swapchain state-machine reference; no Android/X11 backend in tree (`DISPLAY-ALTERNATIVES.md:46`). |
| `The412Banner/Bannerlator` | `DRI3Extension.java`, `PresentExtension.java`, `renderer/DirectScanout.java` | GPL-3.0 | **Reference only** | DRI3 modifier 1255 zero-copy FLIP to SurfaceControl (`VERIFY.md:53-58`). Reference only unless project accepts GPL-3.0. |
| `utkarshdalal/GameNative` | `app/src/main/cpp/winlator/VulkanRenderer*.cpp`, `VulkanRenderer.java` | GPL-3.0 | **Reference only** | SurfaceFlinger ASR renderer and format conversion logic (`VERIFY.md:67-74`). Reference only unless project accepts GPL-3.0. |
| `The412Banner/winlator-contents` | `contents.json`, `wrappers.json` | None | **Do not copy** | Component JSON catalog; no license file (`VERIFY.md:82-83`). |
| `olegos2/mobox` | Scripts | None | **Do not copy** | Archived repository; last push 2024-11-23; archive date unknown; no license file (`VERIFY.md:89`). |
| `HorizonEmuTeam/Horizon-Emu` | APK only | None | **Do not copy** | No source/license; Turnip-only; VirGL TODO (`VERIFY.md:90`). |
| `Producdevity/gamehub-lite` | Launcher | None | **Do not copy** | No license file (`VERIFY.md:99`). |
| `The412Banner/BannerHub` | Launcher | None | **Do not copy** | No license file (`VERIFY.md:100`). |

### License Governance Rules
- **MIT / BSD-2-Clause:** Directly reusable in open-source components with standard copyright attribution.
- **LGPL-2.1 (`brunodev85/winlator`):** LGPL-2.1; reuse obligations not evaluated.
- **GPL-3.0 (`Bannerlator`, `GameNative`):** Reference only. No code lines may be copied into this project unless the resulting launcher distribution explicitly adopts GPL-3.0.
- **Unlicensed Repositories:** All rights reserved under default copyright; no code, scripts, or schemas may be copied.

---

## 5. Driver Work Items with Sizes (S/M/L)

### 1. Real `sync_fd` without `/dev/sw_sync` (Size: L; interim fallback S)
- **Defect & Location:** PanVK emulates `sync_fd` export via `/dev/sw_sync` and spawns a detached pthread per export (`kbase_export_signaler_thread`, `panvk_physical_device.c:1027-1114`). `/dev/sw_sync` is normally absent or denied for untrusted apps on user builds; verify target app access. Export validation remains untested (`validation/g615-v11-csf/beta2-sync-2026-09-19.txt:19`, `panvk-present-readiness.md:17-18`), and missing access triggers `VK_ERROR_OUT_OF_HOST_MEMORY`.
- **Solution:** Investigate KCPU fence export if target kernel supports `CONFIG_SYNC_FILE` and required fence ops (`KBASE_IOCTL_KCPU_QUEUE` or soft-fence atoms; `DISPLAY-ALTERNATIVES.md:49`, `panvk-present-readiness.md:39`).
- **Interim Fallback (Size: S):** Wait on CPU via `vkWaitForFences`, then pass fence `-1` to `ASurfaceTransaction_setBuffer` (`panvk-present-readiness.md:39`).

### 2. SurfaceControl AHB Presenter Replacing Patch `wsi/016` (Size: M)
- **Defect & Location:** Current native Android surface swapchain (`patches/wsi/016-android-surface-swapchain.patch:264-300, 326-459` in `panvk_wsi.c`) allocates private AHBs with `CPU_READ_OFTEN` and presents via `DeviceWaitIdle + ANativeWindow_lock + AHardwareBuffer_lock + CPU memcpy + unlockAndPost` (`panvk-present-readiness.md:23-25`).
- **Solution:** Implement a zero-copy swapchain path managing a ring of 2–4 AHBs. Present calls `ASurfaceTransaction_setBuffer(txn, sc, ahb, acquire_fence_fd)` and imports the transaction release fence back into PanVK queue submission (`panvk-present-readiness.md:40`).

### 3. AFBC / `COMPOSER_OVERLAY` / BGRA Handling (Size: M)
- **Defect & Location:** Mali gralloc may select AFBC; needs validation on target (`panvk-present-readiness.md:11, 41`, `DISPLAY-ALTERNATIVES.md:37`). Patch `patches/csf-v11/033-afbc-wsi-default.patch` is unvalidated on Android ("Gate H pending", `panvk-present-readiness.md:11`). AHB lacks public BGRA8888 (`DISPLAY-ALTERNATIVES.md:37`).
- **Solution:** Parse MTK IMapper v5 metadata via `patches/android/013-vendor-mapper-metadata.patch`. `PANVK_DEBUG=wsi_no_afbc` disables PanVK WSI AFBC default; separately verify/request linear gralloc AHB allocation (`panvk-present-readiness.md:11`). For DXVK B8G8R8A8 swapchains, render via RGBA views or shader swizzles in the layer; strictly avoid global R/B swap hacks (`DISPLAY-ALTERNATIVES.md:37`, `VERIFY.md:71-74`).

### 4. DRI3 Raw dma-buf Device Proof (Size: M)
- **Defect & Location:** `panvk_wsi.c:501-548` defaults kbase to `sw_device=true` (SHM CPU copy). DRI3 with raw dma-buf (`PANVK_KBASE_DRI3=raw|termux`) has no device proof on kbase (`panvk-present-readiness.md:28, 51`).
- **Solution:** Verify `xcb_dri3_pixmap_from_buffers` passing raw dma-buf fds to the Winlator X server. Enforce linear modifier negotiation and explicit CPU/fence sync before `PresentPixmap` (`panvk-present-readiness.md:51, 54`).

### 5. X11 Teardown Hang Fix (`x11_wait_for_present`) (Size: M)
- **Defect & Location:** DXVK Native hangs in `destroySwapchain` waiting in `x11_wait_for_present` in `src/vulkan/wsi/wsi_common_x11.c` (`worklogs/g615-dxvk/PROGRESS.md:49-55`, `panvk-present-readiness.md:29`).
- **Solution:** Trace present completion and idle event delivery under Xvfb/Winlator X server using `X11DBG` logging; ensure present events are properly generated and dispatched upon swapchain retirement (`PROGRESS.md:52-54`, `panvk-present-readiness.md:29, 52`).

### 6. `robustImageAccess2` for vkd3d-proton (Size: M)
- **Defect & Location:** Mandatory requirement for vkd3d-proton 2.14.1/3.0.1 device creation (`panvk_vX_physical_device.c:631`, `worklogs/g615-dxvk/PROGRESS.md:56-62`).
- **Solution:** Implement null image descriptors using texture subdescriptors on arch 11+ in worktree `work/mesa-ria2` (`PROGRESS.md:58-60`). Gate on CTS `dEQP-VK.robustness.robustness2.*` (image/texel subsets) and `dEQP-VK.robustness.image_robustness.*` passing with 0 failures (`PROGRESS.md:59-60`).

---

## 6. Launcher App Components (Brief)

- **Container & Prefix Manager:**
  - Manages isolated Wine prefixes, environment variables (`VK_ICD_FILENAMES`, `VK_LAYER_PATH`, `PANVK_DEBUG`), and Wine registry settings.
  - Controls execution profiles and isolates container filesystems from user storage.
- **Driver Manager:**
  - Ingests standalone `.so` libraries or `.adpkg.zip` packages generated by `scripts/package-android-adpkg.sh`.
  - *Critical packaging nuance:* adpkg.zip contains meta.json (libraryName) + evidence JSON; no ICD json (`panvk-present-readiness.md:33`).
  - Driver manager extracts `.so` to app private storage (`filesDir`) and loads it via `adrenotools_open_libvulkan(..., CUSTOM)` or direct `dlopen` + `dlsym(vk_icdGetInstanceProcAddr)` (`tests/android-loader-app/jni/panvk_loader_test.c:53-89`, `panvk-present-readiness.md:34-35`).
- **Box64 / FEX + Wine Packaging:**
  - Emulates x86_64 Windows PE executables on ARM64 Android (`docs/plans/PANVK_MASTER_ROADMAP.md:67`, `docs/plans/PANVK_UNIVERSAL_MALI_PLAN.md:131`).
  - Packages Wine ARM64/Bionic or a lightweight glibc chroot runtime environment.
- **Input & Audio Subsystems:**
  - *Input:* Translates virtual on-screen controls and physical Bluetooth/USB gamepads into Wine X11 / Wayland input events; provides relative pointer navigation.
  - *Audio:* Bridges Wine audio output to Android OpenSL ES, AAudio, or PulseAudio with low-latency buffering.

---

## 6a. Component sources (researched 2026-10-01)

- **Proton/Wine bionic:** https://github.com/GameNative/proton-wine/releases/tag/proton-11.0-2-20260928
  - Assets: `proton-11.0-2-arm64ec.wcp` (98,079,159 B, sha256 `fffa467241bdae3eacd6ceb7e8096bb7793d617ce53a198dae8bc63a3453f595`), `proton-11.0-2-x86_64.wcp`, `proton-wine-11.0-2-{arm64ec,x86_64}.wcp.xz`. Format `.wcp` = `tar.zst` (`.wcp.xz` = `tar.xz`).
  - Layout: `bin/` (wine, wineserver, symlinks), `lib/wine/{aarch64-unix,aarch64-windows,i386-windows}`, `share/wine`, `prefixPack.txz` (21 MB xz, default prefix), `profile.json` `{"type":"Proton","versionName":"11.0-2-arm64ec","wine":{"binPath":"bin","libPath":"lib","prefixPack":"prefixPack.txz"}}`. ~2246 entries, 807 MB unpacked, 14 relative symlinks.
  - ELF: interpreter `/system/bin/linker64`, Android API 28, NDK r27d, `NEEDED` only libc/libdl, `RUNPATH` `/data/data/com.termux/files/usr/lib` (must override via `LD_LIBRARY_PATH`). `win32u.so` dlopens `"libvulkan.so.1"`; `ntdll.so` contains FEX/ARM64EC hooks. License: repo has `LICENSE` + `COPYING.LIB` (Wine LGPL-2.1+); GitHub reports `NOASSERTION`.
- **GameNative app:** https://github.com/utkarshdalal/GameNative (GPL-3.0, reference only): `ContentsManager.java` extracts `.wcp` as XZ then ZSTD fallback; mirrors at https://downloads.gamenative.app/<file> (`proton-11.0-1-arm64ec.wcp`, `FEXCore-2609.wcp`, `box64-0.4.4.wcp`, `Wowbox64-0.4.4.wcp` ...).
- **Alternative packs:** https://github.com/Arihany/WinlatorWCPHub/releases (no license / `NOASSERTION`; `WINE` tag has `proton-11.0-1-custom-arm64ec.wcp`). Winlator-Ludashi / Cmod bionic forks: not checked (UNVERIFIED).
- **FEX:** https://github.com/FEX-Emu/FEX/releases/tag/FEX-2609 (MIT) has NO binary assets.
  - Prebuilt packs: https://github.com/Arihany/WinlatorWCPHub/releases/download/FEXCore/FEXCore-2609.wcp (1,363,666 B, sha256 `520c31b8ea601baf691da4577f53034e80f4b13bcbfe9d96167f2c402c1db9d1`) and https://downloads.gamenative.app/FEXCore-2609.wcp (tar.xz, 1,973,552 B, sha256 `64f5538b841f533a7c9f07446c9db0650a754059966080c91340e3fd281a29d7`).
  - Contents: `system32/libarm64ecfex.dll`, `system32/libwow64fex.dll`, `aarch64-unix/lib{arm64ec,wow64}fex.so`; `profile.json` `"files"` maps to `${system32}/` and `${libdir}/wine/aarch64-unix/`.
- **DXVK:** upstream https://github.com/doitsujin/dxvk/releases/tag/v3.1.1 (Zlib) ships x64/x32 only.
  - ARM64EC prebuilt: https://github.com/Arihany/WinlatorWCPHub/releases/download/DXVK-ARM64EC/dxvk-arm64ec-3.1.1.wcp (tar.zst, 6,621,711 B, sha256 `f3765e3589a5b84888d52cc03f26e93c2d3db3565def47dc63cb75a183aaaaaa`; `system32` d3d8/9/10core/11/dxgi + `syswow64`). Fallback: build arm64ec with llvm-mingw.
- **Runtime:** GameNative bionic downloads `imagefs_bionic.txz` (183,506,500 B) from https://downloads.gamenative.app/imagefs_bionic.txz (fallback r2.dev mirror) = rootfs with `usr/lib` (X11 libs, freetype, pulseaudio, Vulkan loader `libvulkan.so.1`, `share/vulkan/icd.d`).
  - Vulkan: GameNative sets `VK_ICD_FILENAMES=<imagefs>/usr/share/vulkan/icd.d/wrapper_icd.aarch64.json` (`libvulkan_wrapper.so` + adrenotools hooks `libmain_hook.so`/`libhook_impl.so`, env `ADRENOTOOLS_DRIVER_PATH`/`ADRENOTOOLS_DRIVER_NAME`) or `freedreno_icd.aarch64.json` for Turnip (`XServerScreen.kt:6169-6170`). X server: Winlator Java X server (`com.winlator.xserver`, upstream `brunodev85/winlator` LGPL-2.1).
- **Our path to PanVK:** winevulkan -> `win32u` dlopen `libvulkan.so.1` = Khronos loader (bionic build) -> `VK_ICD_FILENAMES=panvk_icd.json` -> `libvulkan_panfrost.so` (bionic, already bundled in app). No adrenotools needed (PanVK is a normal Mesa ICD exporting `vk_icdGetInstanceProcAddr`).
- **App status (M2):** Components tab installs `.wcp` (tar.zst / tar.xz, magic-detected) from https URL with sha256 pin or local SAF file, into `filesDir/contents/<type>/<versionName>/`.

### M3 / M4 plan

- **M3 status (DONE, device-proven 2026-10-02):**
  - Design: ONE fixed container filesDir/container (WINEPREFIX=.wine, HOME=container dir, marker .setup-ok). Auto-setup on first run: picks installed Proton + FEXCore, requires imagefs bionic; extracts prefixPack.txz, copies FEX dlls (profile.json files map) into system32, dosdevices c:, z:->/, d:->/storage/emulated/0/Download, e:->/storage/emulated/0; runs `wine wineboot -u` headless; success=exit 0 + system32 + system.reg. No container list/UI (user requirement).
  - Exec policy: targetSdk 28 (minSdk 28), wine exec'd directly via ProcessBuilder (bin/wine -> lib/wine/aarch64-unix/wine), no linker64 wrapper, no LD_PRELOAD redirect libs. Earlier agent observed `/system/bin/linker64 <wine>` at targetSdk>=29 fails ('could not exec the wine loader'); not re-proven this session. GameNative 'legacy' flavor uses same targetSdk 28; its 'modern' flavor uses LD_PRELOAD libredirect-bionic-wx.so (not used here). Android 16 device installs targetSdk 28 fine. Storage: READ/WRITE_EXTERNAL_STORAGE + requestLegacyExternalStorage (full /sdcard access at targetSdk 28), runtime-granted.
  - Env: WINEPREFIX, HOME, TMPDIR=<imagefs>/usr/tmp, PATH=<wine>/bin:<imagefs>/usr/bin:/system/bin, LD_LIBRARY_PATH=<imagefs>/usr/lib:/system/lib64:<wine>/lib (overrides Termux RUNPATH), FONTCONFIG_PATH=<imagefs>/etc/fonts, XDG_DATA_DIRS=<imagefs>/usr/share, USER=xuser, WINEDLLOVERRIDES=mscoree,mshtml=d;winex11.drv=d (headless), WINEDEBUG=fixme-all, LC_ALL=en_US.UTF-8, HODLL=libwow64fex.dll (only if present in system32), no DISPLAY. GameNative sets DISPLAY=:0, WINEESYNC, LD_PRELOAD etc. (M4).
  - imagefs: catalog entry 'imagefs_bionic rootfs (GameNative-hosted, licence unclear)', 183,506,500 B, sha256 368db62bfc58b72c97e5169bda9aa64d4246f07964c447e27c79a065e7e9c48b, installs to contents/imagefs/bionic (690 libs in usr/lib: X11/xcb, freetype, fontconfig, pulse, vulkan loader etc.). Licence not stated by GameNative; contents are mixed third-party (LGPL/MIT/GPL libs) -- do NOT redistribute with our app; user downloads it.
  - Runner: single process at a time under one lifecycle lock, merged stdout/stderr -> filesDir/logs/run-<ts>.log (newest 10 kept) and live in Logs tab; Stop = `wineserver -k` + destroyForcibly + kill of same-uid procs whose /proc/<pid>/environ has our WINEPREFIX. Wine tab: Launch explorer (`wine explorer /desktop=shell,1280x720`; headless fails cleanly with 'no driver could be loaded' error), `wine cmd /c ver`, Pick .exe (SAF OpenDocument; primary: external-storage URIs resolve to /storage/emulated/0 path, others are copied into prefix drive_c/imported/<ms>/), manual path field, Stop, recent list (8, SharedPreferences).
  - Device proof (model 2311DRK48I (Mali-G615), Android 16, ksu root adb): setup exit 0 -> container Ready; `cmd /c ver` -> `Microsoft Windows 10.0.19045`; path-run of i386 `whoami.exe` from /sdcard/Download (wow64+FEX) -> `LOCALHOST\xuser` exit=0; SAF-picked aarch64 `hostname.exe` -> `LOCALHOST` exit=0; i386 cmd.exe left waiting on stdin, Stop killed wine/wineserver/winedevice (none left in ps). No x86_64 console PE exists in the Proton pack (only aarch64/i386-windows) so x64-via-arm64ec-FEX not yet exercised. Known noise: 'getaddrinfo', rpcss/actxprxy, rundll32 sysarm32 errors during wineboot -u (non-fatal).
  - At M3 close, still open for M4: X server for explorer/GUI exes + DISPLAY, libvulkan.so.1 loader + PanVK ICD json, DXVK arm64ec copy into prefix, x64 test exe, sdcard-exe mmap behaviour for large games, audio. App display/ICD/DXVK status is below. x64 exe, large-exe mmap, and audio stay open.
- **M4 plan (winevulkan -> PanVK):**
  - Bundle/build Khronos Vulkan-Loader for bionic as `libvulkan.so.1`, write icd json pointing to bundled `libvulkan_panfrost.so`, set `VK_ICD_FILENAMES`.
  - Run a `vulkaninfo.exe` (arm64ec or x64 via FEX) or DXVK d3d11 triangle under Wine; gate = output reports "Mali-G615".
  - Display: needs X server (Winlator-style Java X server, LGPL-2.1) or headless offscreen for `vulkaninfo --summary`.
- **M4 app status (proven on device 2026-10-02; normal ARM64EC DX8/9/10/11 clear/present passes):**
  - App side is in place. Graphical launch uses external Termux:X11 (`com.termux.x11`), not an embedded X server. `DisplayServer.prepare` publishes `DISPLAY` from the first live socket (`<imagefs>/usr/tmp/.X11-unix/X<n>`, else `/tmp/.X11-unix/X<n>`) and sets Wine `TMPDIR` to that socket's directory. Graphics registry is `x11` for graphical runs and `null` for console. Stop keeps the cancellation latch and `STOPPING` until that `run` owner returns. ICD is `files/container/panvk_icd.json` via `VK_ICD_FILENAMES` and `VK_DRIVER_FILES`. targetSdk stays 28 so wine can exec from app data (W^X at 29+). Sideloaded; lint `ExpiredTargetSdkVersion` is suppressed for that reason only.
  - Root cause resolved: `imagefs/bionic/usr/lib/libandroid-sysvshm.so` crashed in `strncpy` on `getenv("ANDROID_SYSVSHM_SERVER")` returning `NULL` when unset, surfacing as `STATUS_ACCESS_VIOLATION` (`0xC0000005`) in `CreateWindowEx`. Fixed in `Containers.env()` by setting `ANDROID_SYSVSHM_SERVER=/dev/null`, forcing graceful non-SHM fallback in `winex11`.
  - Presentation mode truth: Operating strictly via software X11 copy (`PutImage` fallback). No zero-copy scanout or hardware vsync pacing claimed.
   - Driver & DXVK: PanVK Kbase G615 `0.1.0-beta.8` unchanged; local upstream DXVK v3.1.1 runtime ends the current pass before external rendering. Installed fixed WCP supplies ARM64EC and i686 DLLs; actual app disable/re-enable preserves matching component/prefix hashes for both architectures.
    - Current normal ARM64EC matrix: DX8/9/10/11 each have two 320x240 client captures, 76800/76800 orange, exit 0; actual launcher Run captures also pass all four. No staging, Flush/EVENT diagnostic synchronization or automatic fbread injection. Same-source unpatched DX11 control is black. Independent fresh verification repeats all four normal passes. Evidence: `apps/panvk-launcher/tests/results/final-discrimination/runtime-fix/README.md` and `fresh-verification/RESULTS.md`.
    - Historical `apps/panvk-launcher/tests/results/m4-matrix.md` i686 orange-frame claims remain retracted: those pixels were the status-bar battery. ARM64EC success does not prove i686 SAME_VA mapping, x86/WOW64 game compatibility, a real game workload, or zero-copy presentation.
   - Stop/relaunch lifecycle: historical observations only; independently unverified, as recorded in `apps/panvk-launcher/tests/results/m4-matrix.md`. No leak-free or hang-free lifecycle claim.

---

## 7. Mali Format Testing Checklist

- [ ] **RGBA8 / RGBX8 Formats:**
  - Verify native allocation, render-target usage, and sampling via common format tables (`vk_android.c:714-795`).
  - Confirm correct display without banding or alpha corruption.
- [ ] **BGRA Views / Swizzle:**
  - AHB has no public `AHARDWAREBUFFER_FORMAT_B8G8R8A8` format definition (`DISPLAY-ALTERNATIVES.md:37`).
  - DXVK defaults to B8G8R8A8 swapchains. Implement swapchain handling via RGBA views or shader swizzling in the layer.
  - **No global R/B swap: design rule.** Sources show only a generic swapRB toggle (GameNative; `VERIFY.md:71-74`).
- [ ] **sRGB Color Space:**
  - Verify sRGB encoding and decoding on AHB render targets.
  - Ensure SurfaceFlinger does not double-apply gamma curves during composition (`panvk-present-readiness.md:9, 41`).
- [ ] **Stride and Alignment:**
  - Query real buffer stride via `u_gralloc` and `patches/android/013-vendor-mapper-metadata.patch`.
  - Refuse unverified row-pitch calculations to prevent image shearing (`panvk-present-readiness.md:10`).
- [ ] **AHB Usage Flags:**
  - Ensure AHB allocations request `AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT | AHARDWAREBUFFER_USAGE_COMPOSER_OVERLAY` (`VERIFY.md:19-20`, `DISPLAY-ALTERNATIVES.md:33-35`).
- [ ] **Modifiers (AFBC vs Linear):**
  - Validate AHB import with MediaTek gralloc AFBC modifiers (`patches/csf-v11/033-afbc-wsi-default.patch`).
  - If visual corruption or HWC crashes occur, force linear layout: `PANVK_DEBUG=wsi_no_afbc` disables PanVK WSI AFBC default; separately verify/request linear gralloc AHB allocation (`panvk-present-readiness.md:11, 41`).
  - *Note:* Modifier 1255 is an internal Winlator token indicating zero-copy pixmaps, not a hardware Mali format (`VERIFY.md:56, 117`).
- [ ] **preTransform Capabilities (Proposed):**
  - Handle Android display orientation transformations (`VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR` vs pre-rotated buffers).
- [ ] **Acquire and Release Fences:**
  - Verify fence handoff: pass GPU completion `sync_fd` to `ASurfaceTransaction_setBuffer`, and wait on release fences before buffer reuse (`DISPLAY-ALTERNATIVES.md:33-35`).
- [ ] **Buffer Lifecycle and Resizing:**
  - Validate swapchain recreation and resizing across 2–4 AHB buffer rings without leaking GraphicBuffers or file descriptors (`panvk-present-readiness.md:40`).

---

## 8. Phased Plan with Proposed Launcher Gates and Device-Proof Requirements

Note: Gates L0–L5 are proposed launcher gates (new, distinct from existing project gates G0–G7 defined in `docs/plans/PANVK_UNIVERSAL_MALI_PLAN.md:181-191`). Existing 300/300 present proof (`validation/g615-v11-csf/beta3-phase8-wsi-2026-09-19.txt:15-36`) uses patch `wsi/016` CPU copy; proposed launcher gate L2 requires zero-copy presentation.

| Phase | Deliverable | Gate Requirement | Evidence File under `validation/` |
|---|---|---|---|
| **Phase 1: Driver Sync & Format Prerequisites** | Kernel-backed `sync_fd` (KCPU or soft-fence) and AHB format validation (RGBA8/BGRA swizzle, linear/AFBC). | Proposed launcher gate **L0/L1**: app-UID access + lifecycle + CTS `dEQP-VK.synchronization.*` 0 fail + AHB alloc/import render pass. | `validation/g615-v11-csf/launcher-phase1-sync-ahb.txt` |
| **Phase 2: Zero-Copy Presenter (DIRECT_AHB & AHB_BLIT)** | WSI implicit layer / native presenter replacing patch `wsi/016` (0 CPU memcpy), supporting DIRECT_AHB and AHB_BLIT with release fence handling. | Proposed launcher gate **L2 (Present):** ≥300 consecutive frames to `SurfaceControl` with zero CPU copy, no tearing, clean resize/destroy (note: existing 300/300 present proof in `beta3-phase8-wsi` uses CPU copy). | `validation/g615-v11-csf/launcher-phase2-presenter.txt` |
| **Phase 3: DXVK & X11 Teardown Stabilization** | Resolution of `x11_wait_for_present` hang; clean DXVK swapchain recreation and teardown under launcher environment. | Proposed launcher gate **L3:** DXVK Native D3D11/D3D9 draw workload pass + clean exit without hang (`worklogs/g615-dxvk/PROGRESS.md:49-55`). | `validation/g615-v11-csf/launcher-phase3-dxvk-x11.txt` |
| **Phase 4: vkd3d-proton Readiness (`robustImageAccess2`)** | Arch 11+ `robustImageAccess2` implementation via texture subdescriptor null descriptors. | Proposed launcher gate **L4:** CTS `dEQP-VK.robustness.robustness2.*` and `dEQP-VK.robustness.image_robustness.*` 0 fail + vkd3d-proton D3D12 device create PASS (`PROGRESS.md:56-62`). | `validation/g615-v11-csf/launcher-phase4-vkd3d-ria2.txt` |
| **Phase 5: End-to-End Game Launcher Integration** | Integrated launcher packaging (`.adpkg.zip` / `.so` loader), Wine/Box64/FEX execution, real D3D9/D3D11/D3D12 game execution. | Proposed launcher gate **L5 (Consumer):** Wine host + DXVK/vkd3d rendering real game frame workloads, FPS log, screenshot verification. | `validation/g615-v11-csf/launcher-phase5-game-validation.txt` |

### Phase Details & Execution Constraints
- **Phase 1 (Sync & Format):** Resolves the critical SELinux denial on `/dev/sw_sync`. Proves genuine synchronization and external memory AHB rendering on G615 hardware.
- **Phase 2 (Presenter):** Delivers the core zero-copy presentation engine, retiring the CPU `memcpy` implementation from patch `wsi/016`.
- **Phase 3 (DXVK & Teardown):** Eliminates swapchain destruction deadlocks under X11/Xvfb, ensuring clean lifecycle management during game exits and resolution changes.
- **Phase 4 (vkd3d-proton):** Completes D3D12 device-creation prerequisites by passing CTS image robustness subsets without regressions.
- **Phase 5 (End-to-End Integration):** Validates full Windows game execution under Box64/FEX and Wine with high performance and zero visual corruption.

---

## 9. Risks

1. **`/dev/sw_sync` Denial:** `/dev/sw_sync` is normally absent or denied for untrusted apps on user builds; verify target app access (`panvk-present-readiness.md:17-18`). Without a real kbase KCPU/`CONFIG_SYNC_FILE` implementation, `sync_fd` export fails with `VK_ERROR_OUT_OF_HOST_MEMORY`.
2. **AFBC / HWC Visual Artifacts:** Mali gralloc may select AFBC for `COMPOSER_OVERLAY` that SurfaceFlinger/HWC cannot decode if PanVK layout metadata mismatches (`panvk-present-readiness.md:11, 41`). `PANVK_DEBUG=wsi_no_afbc` disables PanVK WSI AFBC default, but linear gralloc allocation must be separately verified/requested on the target.
3. **HWC Overlay Rejection:** HWC overlay not guaranteed; receiver blit (DAC) is separate from SurfaceFlinger composition (`VERIFY.md:31-34, 117-118`, `DISPLAY-ALTERNATIVES.md:36`). SurfaceFlinger may reject direct overlay planes due to layer count, format, or transform constraints, falling back to GPU composition.
4. **Mali Not Proven for DAC Anywhere:** TripleJ160 Ludashi-Plus DAC and amphora were tested only on Adreno (e.g. Odin 2, Adreno 830/630); DAC has never been proven on Mali hardware (`VERIFY.md:36`, `DISPLAY-ALTERNATIVES.md:10`).
5. **Gralloc Vendor Differences:** Gralloc handle structures, metadata extensions, and atom strides vary significantly across MediaTek, Exynos, and Unisoc kernels (`docs/plans/PANVK_UNIVERSAL_MALI_PLAN.md:61-69`).
6. **x86 Wine Bionic vs glibc Mismatches:** DAC layer requires bionic Wine; adrenotools namespace loading does not establish glibc/bionic interop (`VERIFY.md:37, 120-130`).

---

## 10. Corrections to Original Research

Based on verified audits in research notes (2026-10-01: `winlator-research/VERIFY.md` and `display-research/DISPLAY-ALTERNATIVES.md`):

1. **`brorbw/Winlator` does not exist:** The repository returns 404. The official upstream repository is `brunodev85/winlator` (LGPL-2.1) (`VERIFY.md:88`).
2. **Ludashi DisplayX / HWC / X-bypass version:** DisplayX, HWC, X-server bypass, and DRI3/Pixmap rework landed in **v4.0** (2026-08-29), not v4.1 (`VERIFY.md:43-44`).
3. **Mali wrong-colors fix version:** The Mali color fix in Ludashi was **v3.1.h** (2026-06-22, credit @Pipetto-crypto), not v4.1 (`VERIFY.md:47`).
4. **DAC "no copy" claim is conditional:** The receiver app still executes a local GPU blit unless the AHB has `COMPOSER_OVERLAY` (`VulkanRendererContext.cpp:1527, 1782-1805`); `WINLATOR_SCANOUT_GPU_BLIT` defaults to `1` because direct overlay tore on Odin 2 (`VERIFY.md:31-34`).
5. **DAC shipped dark:** The DAC layer `.so` was not deployed into `imagefs` in any release prior to commit `f7a5d34` (2026-06-06); it was tested only on Adreno (`VERIFY.md:35-36`).
6. **`adrenotools` loading mechanism:** Uses private Android linker API `__loader_android_create_namespace` via `liblinkernsbypass` (`android_linker_ns.cpp:182`). Custom drivers must be an Android Vulkan HAL module exporting `HMI` (`VERIFY.md:120-130`).
7. **GameNative R/B swap is generic:** The `swapRB` toggle in `VulkanRendererContext.cpp:626-703` is generic, not Mali-specific (`VERIFY.md:71-74`).
8. **Bannerlator Wayland is Adreno-only:** Bundles Turnip; Mali stays on X11 (`README.md:137,146,187`, `VERIFY.md:61-62`, `DISPLAY-ALTERNATIVES.md:26`).
9. **Upstream `wineandroid.drv` has no Vulkan:** Upstream Wine lacks `vulkan.c` in `wineandroid.drv`; amphora is an experimental out-of-tree port (`DISPLAY-ALTERNATIVES.md:11, 18-19`).
10. **Star and Frost repos:** No canonical public repo found; unverifiable. Bannerlator derives from Star (`com.winlator.star`), while Frost provides only settings/driver repos (`VERIFY.md:95, 97`).
11. **Mobox is archived:** `olegos2/mobox` is archived; last push 2024-11-23; archive date unknown; no license file (`VERIFY.md:89`).
12. **Horizon Emu is APK-only:** `HorizonEmuTeam/Horizon-Emu` has no license and lists VirGL support as a TODO (`VERIFY.md:90`).
13. **`winlator-contents` has no license:** `The412Banner/winlator-contents` contains catalog JSONs but no license file (`VERIFY.md:82-83`).
