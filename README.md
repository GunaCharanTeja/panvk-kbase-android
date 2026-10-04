<h1 align="center">PanVK Kbase Android Driver</h1>

<p align="center">
  Open Mesa <b>PanVK</b> Vulkan driver for Android Mali GPUs, talking directly to the vendor <code>mali_kbase</code> kernel driver.
</p>

<p align="center">
  <a href="https://t.me/+E-NhUATmkqE5ODg1"><img src="https://img.shields.io/badge/Telegram-Join%20testers-26A5E4?style=for-the-badge&logo=telegram&logoColor=white" alt="Join the Telegram testers group"></a>
  <a href="https://github.com/zenithblue-oss/panvk-kbase-android/releases/tag/g615-v11-csf-v0.1.0-beta.13"><img src="https://img.shields.io/badge/driver-beta.13-orange?style=for-the-badge" alt="Driver beta.13"></a>
  <a href="https://github.com/zenithblue-oss/panvk-kbase-android/releases/tag/panplay-v1.0.3"><img src="https://img.shields.io/badge/PanPlay-1.0.3-blue?style=for-the-badge&logo=android&logoColor=white" alt="PanPlay 1.0.3"></a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Vulkan-1.4-AC162C?logo=vulkan&logoColor=white" alt="Vulkan 1.4">
  <img src="https://img.shields.io/badge/GPU-Mali--G615%20(v11)-0091BD?logo=arm&logoColor=white" alt="Mali-G615 v11">
  <img src="https://img.shields.io/badge/DXVK-D3D9%20%7C%2010%20%7C%2011-555" alt="DXVK D3D9, D3D10, D3D11">
  <img src="https://img.shields.io/badge/Mesa-26.3--devel-6E4C9A" alt="Mesa 26.3-devel">
</p>

> [!IMPORTANT]
> **🧪 Testers wanted!**
>
> 1. Install [PanPlay](https://github.com/zenithblue-oss/panvk-kbase-android/releases/tag/panplay-v1.0.3) and play your games.
> 2. After **every** run, whether the game crashed or not, open the session logs screen and tap **Share as ZIP**.
> 3. Send the ZIP to the **[Telegram testers group](https://t.me/+E-NhUATmkqE5ODg1)**, with the game name, your GPU, the FPS you saw and any glitches.

<p align="center">
  <img src="apps/panvk-launcher/tests/results/samevaresults/cube/x86_64-d3d11-cube-hud.png" alt="Direct3D 11 cube demo running through DXVK on PanVK (Mali-G615)" width="100%">
</p>

## About

A standalone patch and build layer around pinned upstream Mesa. It produces an
open Mesa PanVK driver that talks directly to Android's proprietary
`mali_kbase` kernel interface (`/dev/mali0`).

It is **not** a Samba-specific or Winlator-specific fork: one driver
source/patch stack, with adapter packages around it.

**Primary consumers**

| # | Consumer | Integration |
|---|---|---|
| 1 | **Samba S3** | Android/Bionic, in-process custom Vulkan driver |
| 2 | **Bachata S4** | ARM64 glibc Vulkan ICD inside the managed runtime |
| 3 | **NativeCode AI** | ARM64 glibc PRoot validation and development |

**Secondary** (only once the primary driver is correct): Winlator or
AdrenoTools-style loaders, GameHub or component injectors, and other Android
apps that can load an alternate Vulkan ICD.

## Apps

| | App | What it is | Download |
|---|---|---|---|
| <img src="apps/panvk-launcher/docs/panplay-logo-512.png" width="48" alt="PanPlay logo"> | **PanPlay** (`apps/panvk-launcher`) | Windows game launcher (Wine + DXVK + built-in X server) with the PanVK driver bundled | [PanPlay releases](https://github.com/zenithblue-oss/panvk-kbase-android/releases?q=panplay&expanded=true) |
| <img src="apps/panvk-test/docs/panprobe-logo-512.png" width="48" alt="PanProbe logo"> | **PanProbe** (`apps/panvk-test`) | Vulkan feature/extension info and on-device driver tests | [PanProbe releases](https://github.com/zenithblue-oss/panvk-kbase-android/releases?q=panprobe&expanded=true) |

Both apps are tested on the Mali-G615 only. See [apps/panvk-launcher/docs](apps/panvk-launcher/docs) for launcher usage.

## Reference device

| | |
|---|---|
| **Phone** | Poco X6 Pro (2311DRK48I, `duchamp`) |
| **SoC** | MediaTek Dimensity 8300-Ultra (MT6897) |
| **GPU** | Mali-G615 MC6, Pan arch v11, CSF frontend |
| **Kernel interface** | `/dev/mali0` (`mali_kbase`) |
| **GPU ID string** | `Mali-G615 6 cores r1p3 0xB8A3` |

## Supported GPUs

Only the **Mali-G615** (Mesa `PAN_ARCH` v11, CSF frontend; Arm's 4th
generation Valhall, announced 2022) is supported and validated, on the
reference device above. Other Mali GPUs (including G610/v10, G720/v12 and the
Bifrost/Valhall v7/v9 JM parts) have planned profiles or patch scaffolding but
are untested and unsupported. Arm's marketing generations (Utgard, Midgard,
Bifrost, Valhall 1st to 4th gen, 5th Gen, G1) and Mesa `PAN_ARCH` numbers are
different schemes; the full chronological GPU list, mappings, frontends and
upstream driver status are in
[`docs/MALI-GPU-ARCHITECTURES.md`](docs/MALI-GPU-ARCHITECTURES.md).

Status key:
- ✅ **Supported**: validated on a device.
- 🔨 **Built, untested**: compiled into the universal ICD and recognised by the Mesa model table, but never run on that GPU.
- 📋 **TODO**: a profile exists in `profiles/` and the port is planned.
- ❔ **Possible, not tried**: Mesa has a backend for this arch, but no profile or device test exists here.
- ❌ **Not possible**: no PanVK (Vulkan) backend exists for the arch.

One row per Mesa arch. Each GPU carries its own status mark.

| Mesa arch | Arm family / generation | Frontend | GPUs (status per GPU) | Notes |
|---|---|---|---|---|
| n/a | Pre-Utgard (fixed function) | n/a | ❌ Mali-55, ❌ Mali-110 | No programmable shaders |
| n/a (Lima) | Utgard | n/a | ❌ Mali-200, ❌ Mali-300, ❌ Mali-400 MP, ❌ Mali-450 MP, ❌ Mali-470 MP | GLES 2 only (Lima), no Vulkan |
| v4 | Midgard 1st to 3rd gen | JM | ❌ Mali-T604, ❌ T658, ❌ T622, ❌ T624, ❌ T628, ❌ T678, ❌ T720 | No PanVK backend |
| v5 | Midgard 3rd/4th gen | JM | ❌ Mali-T760, ❌ T820, ❌ T830, ❌ T860, ❌ T880 | No PanVK backend |
| v6 | Bifrost 1st/2nd gen | JM | ❌ Mali-G71, ❔ G72 | Mesa marks G71 unsupported; G72 experimental upstream |
| v7 | Bifrost 1st to 3rd gen | JM | 📋 Mali-G52, ❔ G31, ❔ G51, ❔ G76 | Profile `g52-v7-jm` (P25); needs the JM kbase path |
| v9 | Valhall 1st/2nd gen | JM | 📋 Mali-G57, ❔ G77, ❔ G68, ❔ G78, ❔ G78AE | Profile `g57-v9-jm` (P26); needs a v9 backend port and the JM kbase path |
| v10 | Valhall 3rd gen | CSF | 🔨 Mali-G610, 🔨 G310, ❔ G510, ❔ G710 | **Built, untested** (beta.13 universal ICD). G610/G310 are in the Mesa model table; G510/G710 need a tester's gpu_id |
| **v11** | **Valhall 4th gen** | **CSF** | ✅ **Mali-G615**, ❔ G715, ❔ Immortalis-G715 | G615 validated on Poco X6 Pro (Dimensity 8300). G715 and Immortalis-G715 share the same arch but are untested |
| v12 | 5th Gen | CSF | 🔨 Mali-G720, ❔ G620, ❔ Immortalis-G720 | **Built, untested** (beta.13 universal ICD). G720 (variant 4) is in the Mesa model table; G620/Immortalis-G720 need a tester's gpu_id |
| v13 | 5th Gen | CSF | 🔨 Mali-G725, ❔ G625, ❔ Immortalis-G925 | **Built, untested** (beta.13 universal ICD). G725 (variant 4) is in the Mesa model table; G625/Immortalis-G925 need a tester's gpu_id |
| v14 | 5th Gen, G1 series | CSF | 🔨 Mali G1-Ultra, 🔨 G1-Premium, 🔨 G1-Pro | **Built, untested** (beta.13 universal ICD). All three are in the Mesa model table; experimental upstream |
| v15 (unconfirmed) | G2 series | CSF | ❌ Mali G2-Ultra NX, ❌ G2-Premium NX (rumoured), ❌ G2-Pro NX (rumoured) | Not in Mesa yet |

"Possible" means Mesa has code for the arch. It does not mean the arch works
here: each one still needs a kbase profile, its kbase frontend path (JM or
CSF) and device validation. The JM parts (v6, v7, v9) also need the
job-manager kbase path, because the current driver uses CSF only.

## Layout

```text
sources.lock          exact Mesa commit + reference repos (no moving branches)
profiles/             per-GPU Kbase profiles (arch/gpu-id/frontend/uapi)
patches/              qualified patch families (never one unqualified blob)
meson/                cross files (android-aarch64, linux-aarch64-native)
scripts/              fetch / patch / build / package / validate / release
tests/                kbase-probe, vulkan-smoke, compute, offscreen, ahb,
                      android-surface, sync, android-loader-app, dxvk-vkd3d
docs/                 architecture, build, portability, matrix, profiles,
                      app-compat, release, Kbase sparse feasibility,
                      Mali GPU architectures (MALI-GPU-ARCHITECTURES.md)
validation/           G615 capability dumps, DXVK/vkd3d profiles and matrix
.github/workflows/   build / release / source-drift
```

## Quick start (Poco X6 Pro)

See `docs/BUILD-POCO-X6-PRO.md`.

```sh
./scripts/fetch-mesa.sh
./scripts/apply-patches.sh --profile g615-v11-csf
./scripts/build-android.sh --profile g615-v11-csf
./scripts/validate-binary.sh --abi android dist/android-g615-v11-csf/libvulkan_panfrost.so
./scripts/package-android-adpkg.sh --profile g615-v11-csf
```

## Release policy

Tiers: `dev` (build only) < `alpha` (compute/offscreen) < `beta` (WSI +
app-loader on one profile) < `rc` (primary consumer) < `stable` (multi-device
+ soak). First Poco build is never `stable`. Releases are immutable; a patch
change creates a new release even if the Mesa SHA is unchanged.

### Current status

Latest: **[`g615-v11-csf-v0.1.0-beta.13`](https://github.com/zenithblue-oss/panvk-kbase-android/releases/tag/g615-v11-csf-v0.1.0-beta.13)**
(prerelease, Mesa `5a07217f` plus the committed series up to 100). Each
release ships the Android and glibc drivers, an `.adpkg` package, an EMULATOR
zip, the test APK and screenshots. Full history: [`CHANGELOG.md`](CHANGELOG.md).

| Release | Highlights |
|---|---|
| **beta.13** | One universal Android ICD for v10-v14. **Only v11 (G615) is tested; v10/v12/v13/v14 are built but untested, and testers are needed.** The kbase path now admits v14 without `PAN_I_WANT_A_BROKEN_VULKAN_DRIVER`; v13 was already admitted. Fixes PanProbe `gs_viewport_depth` case A, which regressed in beta.11 (100). On kbase, `gpu_prerast` arenas are committed up front again. GPU-fault growth lost the first GS draw. The shared-TLS memory fix from 097 stays. The kbase CS register-count fallback is now 128 on v12+; 96 was too small. `driverInfo` now reads `PanVK-kbase beta.13 (Mesa 26.3.0-devel ...)`, shown in the DXVK HUD and PanProbe. |
| **beta.12** | One universal Android ICD for v10, v11 and v12. **Only v11 (G615) is tested; v10 and v12 are built but untested, and testers are needed.** The release notes give the command that reports your GPU ID. |
| **beta.11** | Fixes `VK_ERROR_DEVICE_LOST` on tiler heap OOM (098). Fixes the memory blow-up from per-pool TLS and eagerly committed prerast arenas (097). |
| **beta.10** | Fast and correct 32-bit WoW64 games: placed maps now map the BO's dma-buf again at the requested address (091) instead of a shadow copy. Need for Speed Most Wanted (DXVK D3D9) went from 0.5 fps to 39-66 fps, with clean HUD and text. Also GPU chunking of large and indirect prerast draws (094, 096), the tessellation conditional-state fix (093), and the sample count for attachment-less secondaries (095). |
| **beta.9** | `depthBounds` (092), `shaderOutputViewportIndex` from VS/TES (090), per-viewport depth clamp/clip (089), CSF event-memory sync words (085). |
| **beta.8** | X11 surfaces (`VK_KHR_xlib_surface`, `VK_KHR_xcb_surface`) for Wine/Proton launchers that display through Termux:X11. The X11/XCB libraries load at runtime from the launcher's library path and are not bundled. Presentation is a software copy (X11 `PutImage`). |

`g615-v11-csf-v0.1.0-beta.3` (code commit
`fc8a759e7d1b2b8de01c0e96f1fdc5e3950ba1a3`, published 2026-09-19) and its
assets stay frozen by project policy. See
`validation/g615-v11-csf/BETA3-PUBLICATION-ADDENDUM.md`.

## Capability truth

Two separate sets are stored per release: `UPSTREAM_MATRIX_CAPABILITIES`
(from `docs/features.txt` of the exact Mesa checkout) and
`RUNTIME_DEVICE_CAPABILITIES` (from on-device `vulkaninfo`/probe). Only
runtime-tested capabilities may be used for compatibility claims. No fake
feature bits.

## Runtime Features & Extensions (beta.3)

Full specification and per-capability test breakdown:
[`docs/RUNTIME-FEATURES.md`](docs/RUNTIME-FEATURES.md). Canonical
machine-readable evidence:
[`validation/g615-v11-csf/runtime-feature-matrix.json`](validation/g615-v11-csf/runtime-feature-matrix.json).

- **Vulkan API Version**: `1.4.363`
- **Total Extensions Exposed**: `194` (13 instance, 181 device); beta.2 tag: `178`
- **Driver**: Mesa `26.3.0-devel` (commit `5a07217f034b3e50d8c7c7794f97a2df1742613b`)
- **GPU**: Mali-G615 MC6 (`0xb8a31030`)
- **Kernel Interface**: `mali_kbase` (CSF uAPI 1.21, `/dev/mali0`)
- **Patch Series ID**: `sha256:c0bbdeef591b206a2f3ae36dc6191c08854399039075f8d69f103e33c0eca2f8`

### Phase 5 feature workloads

All 13 target groups are natively exposed and passed real Poco X6 Pro
workloads. Enumeration alone is not counted as a test.

| Feature group | Status | Concise workload evidence |
|---|:---:|---|
| Descriptor indexing | **PASS** | Runtime array, partially-bound and variable-count descriptors; non-uniform sampled-image/storage-buffer indexing; checksum `123` |
| Timeline semaphore | **PASS** | GPU chain `1..64`, CPU wait/signal, CPU-to-GPU wait, final counter `66` |
| Dynamic rendering | **PASS** | `vkCmdBeginRendering`/`vkCmdEndRendering`; 1,154 triangle pixels; checksum `0x9a7f1b8fec90af07` |
| Synchronization2 | **PASS** | `vkCmdPipelineBarrier2` and `vkQueueSubmit2`; timeline completion `67` |
| Buffer device address | **PASS** | Shader dereference at index 3; readback `0x2468ace0` |
| Push descriptors | **PASS** | `vkCmdPushDescriptorSetKHR` storage buffers; readback `0xb791f3dd` |
| Robust buffer access | **PASS** | Out-of-bounds index 64 from one bound uint returned `0x00000000` |
| Anisotropy | **PASS** | 4x anisotropy across 9 sampled formats; two deterministic runs |
| Wide lines | **PASS** | Width 5 rasterized 185 green pixels |
| Large points | **PASS** | Size 11 rasterized 121 yellow pixels |
| ETC2/EAC | **PASS** | ETC2 RGB/RGBA plus EAC R11/RG11 UNORM/SNORM sampling, filtering, and mip level 1 |
| ASTC LDR | **PASS** | ASTC 4x4 UNORM/SRGB sampling, filtering, mip level 1, exact checksums |
| ASTC HDR | **PASS** | `VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT`; pixel `[1, 2, 3, 1]`; filtering and mip level 1 |

This table describes the beta.3 release. Since then `textureCompressionBC`
has been exposed on `feature/g615-dxvk-complete` through GPU compute decode
(G615 has no BC hardware); see the DXVK section below. Other unsupported
features are listed in [`docs/RUNTIME-FEATURES.md`](docs/RUNTIME-FEATURES.md).

## DXVK / vkd3d-proton compliance (G615)

### Current state (after csf-v11/092)

DXVK Native v3.1.1 on the Poco X6 Pro creates a D3D11 device at
**feature level 11_0** (`D3D11 HRESULT=0x00000000 feature_level=0xb000`);
D3D11 and D3D9 draw workloads pass. Every feature below runs on the GPU,
with no CPU fallback or simulation, and is exposed only after device proof.

| Feature | Implementation | Device proof (CTS Pass / Fail) |
|---|---|---|
| `geometryShader` | VS/GS run as compute on the GPU pre-raster path, 64 invocations (071) | geometry 193 / 0; instanced 20 / 0 |
| `tessellationShader` | VS, TCS, tessellator and TES as compute, GPU chunking | tessellation 526 / 0 |
| `VK_EXT_transform_feedback` | GPU capture kernel, 4 streams, counters, queries | transform_feedback 15793 / 0 (2 intermittent DeviceLost) |
| `textureCompressionBC` | GPU compute decode of BC1-7 | BC subset 1863 / 0; copy_and_blit 9620 / 0 |
| `shaderClipDistance`, `shaderCullDistance` | NIR lowering | device matrix 0 fail |
| `multiViewport` | 16 viewports, GS viewport index honoured (076) | device matrix 0 fail, scissor 88 / 88 |
| `fillModeNonSolid` | GPU kernel builds line/point primitives | 17/17 pixel-exact |
| `pipelineStatisticsQuery` | 049-054 | statistics_query 15374 / 0 |
| `VK_KHR_incremental_present`, `VK_EXT_swapchain_colorspace`, `VK_EXT_image_compression_control` | upstream backports | device probes 0 fail |
| `VK_EXT_multi_draw` | 072 | multi_draw 12704 / 0 |
| `VK_EXT_primitives_generated_query` | 073 | primitives_generated_query 75206 / 0 |
| `VK_EXT_memory_priority`, `VK_EXT_pageable_device_local_memory` | 069 | 224 / 0, 202 / 0; api.info 7799 / 0 |
| `alphaToOne` | 070 | alphaToOne 123 / 0 |
| `variableMultisampleRate` | 074 | variable_rate 504 / 0 standalone (tmp/cts/p5-vmsr/summary.txt); combined regression run (geometry + tessellation + transform_feedback.simple + variable_rate: 15955 cases, 6210 pass, 9745 NotSupported, 0 fail; source tmp/cts/r6-geo/status.txt) |
| `sync_fd` export | 075 (kbase KCPU queue) | sync_fd 1996 / 0 |
| `vertexPipelineStoresAndAtomics` (v10-v12) | 078-082, vertex stage on the compute pre-raster path | atomic_operations `*_vertex*` 66 / 0; 12132-case tess/geometry/xfb/draw list 7940 / 0, 0 DeviceLost |
| per-viewport depth clamp/clip (beta.9) | 089, GS-selected viewports drawn as ordered runs | panvk-test `gs_viewport_depth` exact (no CTS) |
| `shaderOutputViewportIndex` from VS/TES (beta.9, v10/v11) | 090 | panvk-test `vs_viewport_index` 6/6 (no CTS) |
| `depthBounds` (beta.9, v10/v11) | 092, fragment-shader emulation | 2059 depth-bounds CTS cases: 1774 pass / 0 fail / 285 NotSupported; panvk-test `depth_bounds` 6/6 |

DXVK feature level 11_1 has not been re-checked on the beta.7 to beta.9
builds yet. The Android driver has X11 surfaces (beta.8), but under Proton 11
(i686 through wow64) Wine fails to create the Vulkan surface before the
driver is called, so DXVK presentation there is still blocked. `depthBounds`
lowered pipelines lose FPK, and `EarlyFragmentTests` shaders with depth writes
compare against the fragment's new depth (see the beta.9 release notes).
Still missing: `robustImageAccess2` (the vkd3d-proton device-create
blocker) and
sparse (FL12_0, `NO-GO` on Kbase). The X11 present teardown hang was seen once
under Xvfb only and is unverified on Android. Progress and TODOs:
[`worklogs/g615-dxvk/PROGRESS.md`](worklogs/g615-dxvk/PROGRESS.md). Roadmap:
[`docs/plans/PANVK_MASTER_ROADMAP.md`](docs/plans/PANVK_MASTER_ROADMAP.md).

### beta.3 evaluation (historical)

Machine-evaluated against stock tagged profiles. No fake feature bits.
Overall result: **FAIL**. DXVK 2.7.1/3.1.1 COMMON and vkd3d README hard
gates PASS; D3D9/D3D10/D3D11 profiles and vkd3d 2.14.1/3.0.1 device/profile
baseline FAIL. Stock DXVK/vkd3d smoke is BLOCKED (Android ICD is not a
legal host for those Windows binaries). CTS is BLOCKED.

Canonical report:
[`validation/g615-v11-csf/P23-DXVK-VKD3D-COMPLIANCE-MATRIX.md`](validation/g615-v11-csf/P23-DXVK-VKD3D-COMPLIANCE-MATRIX.md).
JSON:
[`validation/g615-v11-csf/p23-dxvk-vkd3d-compliance-matrix.json`](validation/g615-v11-csf/p23-dxvk-vkd3d-compliance-matrix.json).
Gap report:
[`validation/g615-v11-csf/dxvk-vkd3d-gap-report.md`](validation/g615-v11-csf/dxvk-vkd3d-gap-report.md).
Capability dump:
[`validation/g615-v11-csf/consumer-capabilities.json`](validation/g615-v11-csf/consumer-capabilities.json).

| Target | Result |
|---|---|
| DXVK 1.10.3 D3D9 / D3D10 / FL10.1 / FL11.0 | FAIL |
| DXVK 2.7.1 COMMON | PASS |
| DXVK 2.7.1 D3D9 / D3D10_10_1 / D3D11_11_0 / D3D11_11_1 | FAIL |
| DXVK 3.1.1 COMMON | PASS |
| DXVK 3.1.1 D3D9 / D3D10_10_1 / D3D11_11_0 / D3D11_11_1 | FAIL |
| vkd3d-proton 2.0 HARD / DEVICE_CREATE | PASS |
| vkd3d-proton 2.14.1 HARD | PASS |
| vkd3d-proton 2.14.1 PROFILE_BASELINE / DEVICE_CREATE | FAIL |
| vkd3d-proton 3.0.1 HARD | PASS |
| vkd3d-proton 3.0.1 PROFILE_BASELINE / DEVICE_CREATE | FAIL |
| MAX_FEATURE_LEVEL / D3D_FEATURE_LEVEL | NOT_AVAILABLE |

At beta.3, still false (no spoofing): `geometryShader`, `tessellationShader`,
`fillModeNonSolid`, `multiViewport`, `shaderClipDistance`,
`shaderCullDistance`, `textureCompressionBC`, transform feedback,
`pipelineStatisticsQuery`, `robustImageAccess2`, sparse.

Proven on G615: `VK_EXT_robustness2` buffer + `nullDescriptor`,
`maxPushConstantsSize=256`, `VK_KHR_push_descriptor` / `maxPushDescriptors=32`,
DXVK/vkd3d common easy gates. Advertised UAB limits are 1,048,576; 1M
descriptor stress is implemented but not yet run to completion on device.

Phase evidence:
[`P6`](validation/g615-v11-csf/P6-ROBUSTNESS2.md),
[`P7`](validation/g615-v11-csf/P7-PUSH-CONSTANTS-DESCRIPTORS.md),
[`P8`](validation/g615-v11-csf/P8-COMMON-GATES.md),
[`P9`](validation/g615-v11-csf/P9-BC-COMPATIBILITY.md),
[`P10`](validation/g615-v11-csf/P10-CLIP-CULL-DISTANCE.md),
[`P11`](validation/g615-v11-csf/P11-FILL-MODE-NON-SOLID.md),
[`P12`](validation/g615-v11-csf/P12-GEOMETRY-SHADER.md),
[`P13`](validation/g615-v11-csf/P13-D3D9.md),
[`P14`](validation/g615-v11-csf/P14-MULTI-VIEWPORT.md),
[`P15`](validation/g615-v11-csf/P15-TRANSFORM-FEEDBACK.md),
[`P16`](validation/g615-v11-csf/P16-D3D10.md),
[`P17`](validation/g615-v11-csf/P17-TESSELLATION-SHADER.md),
[`P18`](validation/g615-v11-csf/P18-D3D11-FL11.md),
[`P19`](validation/g615-v11-csf/P19-VKD3D-PROFILE-BASELINE.md),
[`P20`](validation/g615-v11-csf/P20-PIPELINE-STATISTICS.md),
[`P21`](validation/g615-v11-csf/P21-D3D12-FEATURE-LEVEL.md),
[`P22`](validation/g615-v11-csf/P22-KBASE-SPARSE-FEASIBILITY.md).
Sparse: [`docs/KBASE-SPARSE-FEASIBILITY.md`](docs/KBASE-SPARSE-FEASIBILITY.md)
(`NO-GO` on Kbase UAPI 1.21). Profiles:
[`validation/requirements/`](validation/requirements/).
Evaluators: `scripts/evaluate-vulkan-profile.py`,
`scripts/evaluate-dxvk-vkd3d-compliance-matrix.py`.
Workloads: [`tests/dxvk-vkd3d/`](tests/dxvk-vkd3d/).

At beta.3 the next blocker was `robustImageAccess2` (vkd3d 2.14.1/3.0.1
`DEVICE_CREATE`). It still is; geometry, fill, clip/cull and BC have since
landed (see Current state above).

### New extension workloads

These 16 beta.3 additions are exposed and workload-tested:

`VK_KHR_compute_shader_derivatives`, `VK_KHR_copy_memory_indirect`,
`VK_KHR_internally_synchronized_queues`, `VK_KHR_maintenance7`,
`VK_KHR_maintenance8`, `VK_KHR_maintenance9`, `VK_KHR_present_id2`,
`VK_KHR_present_wait2`, `VK_KHR_shader_constant_data`, `VK_KHR_shader_fma`,
`VK_KHR_shader_relaxed_extended_instruction`,
`VK_KHR_shader_untyped_pointers`, `VK_KHR_surface_maintenance1`,
`VK_KHR_swapchain_maintenance1`, `VK_KHR_unified_image_layouts`, and
`VK_GOOGLE_user_type`.

`VK_KHR_depth_clamp_zero_one`, `VK_KHR_pipeline_binary`, and
`VK_KHR_robustness2` are disabled and not exposed because required workload
proof is absent. `VK_GOOGLE_display_timing` is not exposed without a proven
Android timing implementation.

### beta.2 history

The immutable beta.2 tag exposed 12 instance and 166 device extensions (178
total). Its post-tag documentation commit is distinct from the tagged code as
recorded under [Current status](#current-status). Everything below is the
published beta.3 inventory generated from the canonical matrix.

<details>
<summary><b>Full beta.3 list: 194 extensions (13 instance + 181 device)</b></summary>

#### Instance extensions (13)

`VK_KHR_android_surface`, `VK_KHR_device_group_creation`,
`VK_KHR_external_fence_capabilities`, `VK_KHR_external_memory_capabilities`,
`VK_KHR_external_semaphore_capabilities`,
`VK_KHR_get_physical_device_properties2`,
`VK_KHR_get_surface_capabilities2`, `VK_KHR_surface`,
`VK_KHR_surface_maintenance1`, `VK_EXT_debug_report`, `VK_EXT_debug_utils`,
`VK_EXT_headless_surface`, `VK_EXT_surface_maintenance1`.

#### Device extensions (181)

`VK_KHR_8bit_storage`, `VK_KHR_16bit_storage`, `VK_KHR_bind_memory2`,
`VK_KHR_buffer_device_address`, `VK_KHR_calibrated_timestamps`,
`VK_KHR_compute_shader_derivatives`, `VK_KHR_cooperative_matrix`,
`VK_KHR_copy_commands2`, `VK_KHR_copy_memory_indirect`,
`VK_KHR_create_renderpass2`, `VK_KHR_dedicated_allocation`,
`VK_KHR_depth_stencil_resolve`, `VK_KHR_descriptor_update_template`,
`VK_KHR_device_group`, `VK_KHR_draw_indirect_count`,
`VK_KHR_driver_properties`, `VK_KHR_dynamic_rendering`,
`VK_KHR_dynamic_rendering_local_read`, `VK_KHR_external_fence`,
`VK_KHR_external_fence_fd`, `VK_KHR_external_memory`,
`VK_KHR_external_memory_fd`, `VK_KHR_external_semaphore`,
`VK_KHR_external_semaphore_fd`, `VK_KHR_format_feature_flags2`,
`VK_KHR_get_memory_requirements2`, `VK_KHR_global_priority`,
`VK_KHR_image_format_list`, `VK_KHR_imageless_framebuffer`,
`VK_KHR_index_type_uint8`, `VK_KHR_internally_synchronized_queues`,
`VK_KHR_line_rasterization`, `VK_KHR_load_store_op_none`,
`VK_KHR_maintenance1`, `VK_KHR_maintenance2`, `VK_KHR_maintenance3`,
`VK_KHR_maintenance4`, `VK_KHR_maintenance5`, `VK_KHR_maintenance6`,
`VK_KHR_maintenance7`, `VK_KHR_maintenance8`, `VK_KHR_maintenance9`,
`VK_KHR_map_memory2`, `VK_KHR_multiview`,
`VK_KHR_pipeline_executable_properties`, `VK_KHR_pipeline_library`,
`VK_KHR_present_id`, `VK_KHR_present_id2`, `VK_KHR_present_wait`,
`VK_KHR_present_wait2`, `VK_KHR_push_descriptor`,
`VK_KHR_relaxed_block_layout`, `VK_KHR_sampler_mirror_clamp_to_edge`,
`VK_KHR_sampler_ycbcr_conversion`, `VK_KHR_separate_depth_stencil_layouts`,
`VK_KHR_shader_atomic_int64`, `VK_KHR_shader_clock`,
`VK_KHR_shader_constant_data`, `VK_KHR_shader_draw_parameters`,
`VK_KHR_shader_expect_assume`, `VK_KHR_shader_float16_int8`,
`VK_KHR_shader_float_controls`, `VK_KHR_shader_float_controls2`,
`VK_KHR_shader_fma`, `VK_KHR_shader_integer_dot_product`,
`VK_KHR_shader_maximal_reconvergence`, `VK_KHR_shader_non_semantic_info`,
`VK_KHR_shader_quad_control`, `VK_KHR_shader_relaxed_extended_instruction`,
`VK_KHR_shader_subgroup_extended_types`, `VK_KHR_shader_subgroup_rotate`,
`VK_KHR_shader_subgroup_uniform_control_flow`,
`VK_KHR_shader_terminate_invocation`, `VK_KHR_shader_untyped_pointers`,
`VK_KHR_spirv_1_4`, `VK_KHR_storage_buffer_storage_class`,
`VK_KHR_swapchain`, `VK_KHR_swapchain_maintenance1`,
`VK_KHR_swapchain_mutable_format`, `VK_KHR_synchronization2`,
`VK_KHR_timeline_semaphore`, `VK_KHR_unified_image_layouts`,
`VK_KHR_uniform_buffer_standard_layout`, `VK_KHR_variable_pointers`,
`VK_KHR_vertex_attribute_divisor`, `VK_KHR_vulkan_memory_model`,
`VK_KHR_workgroup_memory_explicit_layout`,
`VK_KHR_zero_initialize_workgroup_memory`, `VK_EXT_4444_formats`,
`VK_EXT_astc_decode_mode`, `VK_EXT_attachment_feedback_loop_dynamic_state`,
`VK_EXT_attachment_feedback_loop_layout`, `VK_EXT_border_color_swizzle`,
`VK_EXT_buffer_device_address`, `VK_EXT_calibrated_timestamps`,
`VK_EXT_color_write_enable`, `VK_EXT_conditional_rendering`,
`VK_EXT_conservative_rasterization`, `VK_EXT_custom_border_color`,
`VK_EXT_debug_marker`, `VK_EXT_depth_bias_control`,
`VK_EXT_depth_clamp_control`, `VK_EXT_depth_clamp_zero_one`,
`VK_EXT_depth_clip_control`, `VK_EXT_depth_clip_enable`,
`VK_EXT_descriptor_indexing`, `VK_EXT_device_address_binding_report`,
`VK_EXT_device_memory_report`, `VK_EXT_dynamic_rendering_unused_attachments`,
`VK_EXT_extended_dynamic_state`, `VK_EXT_extended_dynamic_state2`,
`VK_EXT_extended_dynamic_state3`,
`VK_EXT_external_memory_acquire_unmodified`,
`VK_EXT_external_memory_dma_buf`, `VK_EXT_global_priority`,
`VK_EXT_global_priority_query`, `VK_EXT_graphics_pipeline_library`,
`VK_EXT_hdr_metadata`, `VK_EXT_host_image_copy`, `VK_EXT_host_query_reset`,
`VK_EXT_image_2d_view_of_3d`, `VK_EXT_image_drm_format_modifier`,
`VK_EXT_image_robustness`, `VK_EXT_image_sliced_view_of_3d`,
`VK_EXT_image_view_min_lod`, `VK_EXT_index_type_uint8`,
`VK_EXT_inline_uniform_block`, `VK_EXT_legacy_dithering`,
`VK_EXT_line_rasterization`, `VK_EXT_load_store_op_none`,
`VK_EXT_map_memory_placed`, `VK_EXT_memory_budget`,
`VK_EXT_multisampled_render_to_single_sampled`,
`VK_EXT_mutable_descriptor_type`, `VK_EXT_nested_command_buffer`,
`VK_EXT_non_seamless_cube_map`, `VK_EXT_physical_device_drm`,
`VK_EXT_pipeline_creation_cache_control`,
`VK_EXT_pipeline_creation_feedback`, `VK_EXT_pipeline_robustness`,
`VK_EXT_present_timing`, `VK_EXT_primitive_topology_list_restart`,
`VK_EXT_private_data`, `VK_EXT_provoking_vertex`,
`VK_EXT_queue_family_foreign`,
`VK_EXT_rasterization_order_attachment_access`, `VK_EXT_rgba10x6_formats`,
`VK_EXT_robustness2`, `VK_EXT_sampler_filter_minmax`,
`VK_EXT_scalar_block_layout`, `VK_EXT_separate_stencil_usage`,
`VK_EXT_shader_atomic_float`, `VK_EXT_shader_demote_to_helper_invocation`,
`VK_EXT_shader_image_atomic_int64`, `VK_EXT_shader_module_identifier`,
`VK_EXT_shader_replicated_composites`, `VK_EXT_shader_stencil_export`,
`VK_EXT_shader_subgroup_ballot`, `VK_EXT_shader_subgroup_vote`,
`VK_EXT_shader_tile_image`, `VK_EXT_shader_uniform_buffer_unsized_array`,
`VK_EXT_subgroup_size_control`, `VK_EXT_swapchain_maintenance1`,
`VK_EXT_texel_buffer_alignment`, `VK_EXT_texture_compression_astc_hdr`,
`VK_EXT_tooling_info`, `VK_EXT_vertex_attribute_divisor`,
`VK_EXT_vertex_input_dynamic_state`, `VK_EXT_ycbcr_2plane_444_formats`,
`VK_EXT_ycbcr_image_arrays`, `VK_EXT_zero_initialize_device_memory`,
`VK_ANDROID_external_memory_android_hardware_buffer`,
`VK_ANDROID_native_buffer`, `VK_ARM_rasterization_order_attachment_access`,
`VK_ARM_scheduling_controls`, `VK_ARM_shader_core_builtins`,
`VK_ARM_shader_core_properties`, `VK_GOOGLE_decorate_string`,
`VK_GOOGLE_hlsl_functionality1`, `VK_GOOGLE_user_type`,
`VK_VALVE_mutable_descriptor_type`.

</details>

## License

Mesa code remains under its upstream licenses (see `LICENSES/` and
`NOTICE.md`). Build/patch/test scaffolding in this repository is MIT unless
otherwise noted.
