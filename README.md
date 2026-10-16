# PanVK Kbase Android Vulkan Driver

Open Mesa **PanVK** Vulkan driver communicating directly with the vendor `mali_kbase` kernel interface (`/dev/mali0`) on Android, with native support for **DirectX 9, 10, 11 (DXVK)** and **DirectX 12 (VKD3D-Proton)**, featuring GPU **BC1–BC7 texture conversion** at native BC size (ETC2 for BC1–BC3, EAC for BC4/BC5, ASTC 4x4 LDR/HDR for BC7/BC6H).

[![Release 1.1.5-FC](https://img.shields.io/badge/Release-1.1.5--FC-brightgreen)](https://github.com/GunaCharanTeja/panvk-kbase-android/releases/latest)
[![Vulkan 1.4](https://img.shields.io/badge/Vulkan-1.4-AC162C?logo=vulkan&logoColor=white)](#)
[![GPU Mali-G615](https://img.shields.io/badge/GPU-Mali--G615%20(v11%20CSF)-0091BD?logo=arm&logoColor=white)](#)
[![BCn on GPU](https://img.shields.io/badge/BC1--BC7-ETC2%20%7C%20EAC%20%7C%20ASTC-success)](#)
[![DXVK D3D9/10/11](https://img.shields.io/badge/DXVK-D3D9%20%7C%2010%20%7C%2011-555)](#)
[![VKD3D D3D12](https://img.shields.io/badge/VKD3D--Proton-D3D12-7B1FA2)](#)
[![Mesa 26.3-devel](https://img.shields.io/badge/Mesa-26.3--devel-6E4C9A)](#)
[![Maintainer GunaCharanTeja](https://img.shields.io/badge/Maintainer-GunaCharanTeja-blue)](https://github.com/GunaCharanTeja/panvk-kbase-android)

---

## Download & install

Get the latest driver from **[Releases](https://github.com/GunaCharanTeja/panvk-kbase-android/releases/latest)**:

| You use | Download |
| :--- | :--- |
| Winlator or another app that loads custom drivers (adrenotools) | `PanVK-G615-<version>-adrenotools.zip` |
| Termux with Termux:X11 | `PanVK-G615-<version>-x11.zip` |

Winlator / adrenotools: don't unzip; install the zip from the app's driver settings, then pick **PanVK G615** for your container.

---

## Overview

This repository provides an open-source Mesa PanVK driver layer specifically engineered for modern ARM Mali Valhall GPUs with the Command Stream Frontend (CSF). It bypasses proprietary userland driver limitations by talking directly to `/dev/mali0` (`mali_kbase`), enabling desktop-grade Vulkan features required by Windows translation layers (DXVK, VKD3D-Proton, Wine, Box64, FEX) without requiring root or custom kernels.

---

## Reference Device & Hardware Specifications

| Component | Specification |
| :--- | :--- |
| **Device Model** | **OPPO CPH2763** (Codename `OP5F19L1`) |
| **SoC** | **MediaTek MT6878** |
| **GPU** | **ARM Mali-G615 MC2** |
| **GPU Architecture** | Panfrost Arch **v11** (Valhall 4th Gen) |
| **Frontend** | Command Stream Frontend (**CSF**) |
| **Kernel Interface** | `/dev/mali0` (`mali_kbase` UAPI 1.21, Mode: `crw-rw-rw-`) |
| **GPU ID** | `0xb8a31030` (`Mali-G615 2 cores r1p3`) |
| **Operating System** | Android 16 (API Level / SDK 36) |

---

## Key Enhancements

### 1.1.5-FC (Patches 212-219)
* **BCn textures at native size**: BC1 → ETC2 RGB8/RGB8A1, BC2/BC3 → ETC2 RGBA8, BC4/BC5 → EAC R11/RG11; BC6H/BC7 stay on ASTC 4x4. Every BC format now takes the same GPU memory as real BC hardware (half of 1.1.4-FC for BC1/BC4/BC5, 2.5x less for BC2/BC3).
* **Faster conversion**: BC1 4.3x, BC1 RGBA 7x, BC7 3.1x, BC6H 1.4x faster than 1.1.4-FC, faster than a plain RGBA8 decode where measured (BC1, BC6H, BC7); fewer stutters while textures stream in.
* **Full-rate filtering (Patch 212)** for ASTC-backed BC views (8-bit decode).
* `PANVK_BC_ETC=0` / `PANVK_BC_EAC=0` restore the 1.1.4-FC paths. See [CHANGELOG](CHANGELOG.md).

### 1.1.4-FC (Patches 185, 188, 197-210)
* **Red Dead Redemption GPU faults fixed (Patches 188, 197)**: no more GPU freeze/reset about a minute into the game.
* **Fewer GPU queue stalls (Patches 199, 204, 205, 208-210)**: the GPU's geometry, pixel and compute queues wait for each other far less: up to +22% FPS in RDR1, higher in gameplay too. On by default; `PANVK_LAZY_VTWAIT=0`, `PANVK_LAZY_CWAIT=0`, `PANVK_RP_MERGE=0` turn the parts off.
* Tested with vkd3d-proton (D3D12) and DXVK (D3D9/10/11) games. See [CHANGELOG](CHANGELOG.md) for details.

### 1.1.3-FC (Patches 176, 179, 180, 183)
* **Tiled BCn planes (Patch 176)**: decoded BCn textures use the U-interleaved layout Mali samples fastest: +24-40% FPS on texture-heavy scenes, same memory.
* **Less texture RAM (Patch 180)**: images filled only by uploads and BC copies drop the raw BC blocks: BC1/BC4/BC6H/BC7 2048² with mips 5.3 MiB (was 8-10.7 MiB), BC5 10.7 MiB (was 16).
* **Faster BC1 punch-through and BC7 uploads (Patch 179)**: 57 -> 36 ms and 76.7 -> 64.0 ms, at most 0.15 dB lower.
* **8 MiB tiler heap chunks (Patch 183)**: 8x fewer heap-growth stalls in heavy geometry: particle scenes 2x faster (GPU 145 -> 68 ms), same RAM. `PANVK_KBASE_TILER_CHUNK_KB=1024` restores the old layout.

### Upstream RC2 / Beta.18 changes (Patches 131–171)
* **Removed in 1.1.2-FC: register-allocation speedups (Patches 152–154, 160–164)**: with them Red Dead Redemption (D3D12) loses the device while loading (GPU read translation fault, bisected on device). Very large shaders compile slower again until a validated replacement lands.
* **Persistent Mesa Shader Disk Cache on Android (Patch 170)**: Keeps Mesa's on-disk shader cache on by default on Android, so later launches reuse shaders compiled earlier instead of recompiling them. The cache is keyed by the driver build ID and GPU; `MESA_SHADER_CACHE_DISABLE=true` turns it off.
* **Android Property Query Caching (Patch 171)**: Caches `os_get_option()` lookups instead of querying Android properties on every call (upstream measured 128 ms of stutter during image creation).
* **CSF Prerast Producer Scoreboard Synchronization (Patch 132)**: Adds missing iterator scoreboard barrier before viewport runs, fixing race conditions in multi-viewport scenes.
* **GPU Model & Small-Core Identification (Patches 131, 140)**: Core-count device naming for Immortalis/small GPUs; adds the Mali-G710 model row.

### Patches 124–130, 172–174, 212–219: BC1–BC7 transcoding (native BC memory)
Mali has no BCn sampler, so BC textures are converted on the GPU at upload time into formats the hardware samples natively. Each format uses the path that measured best on device:

| BC format | Stored as | Memory (2048², full mips) | Upload, Mali-G615 MC2 |
| :--- | :--- | :--- | :--- |
| BC1 RGB / RGBA | **ETC2 RGB8 / RGB8A1** (Patch 214, 215) | 2.7 MiB (−88% vs RGBA8) | 5.0 / 5.1 ms |
| BC2, BC3 | **ETC2 RGBA8**, EAC alpha (Patch 215) | 5.3 MiB (−75%) | 7.3 / 8.4 ms |
| BC4 / BC5 (UNORM and SNORM) | **EAC R11 / RG11** (Patch 213, 219) | 2.7 / 5.3 MiB | 2.7 / 3.7 ms |
| BC6H UFLOAT / SFLOAT | ASTC 4x4 **HDR** | 5.3 MiB (−88% vs RGBA16F) | 28 ms |
| BC7 | ASTC 4x4 LDR | 5.3 MiB (−75%) | 22 ms |

Memory: 2048² with full mips, same as native BC. Upload: GPU time for the bench's random 2048² set.

* **Quality (PSNR vs. the BC decode, real game textures):** BC1 37.5–37.9 dB, BC3 35–36 dB, BC4 49 dB, BC5 46 dB, BC6H 50–54 dB (realistic blocks), BC7 46.8 dB average; no visible difference in play. Alpha that reaches 0 or 255 stays exact. ASTC paths map BC endpoints and indices onto ASTC directly where the block allows it, otherwise a principal-axis fit with least-squares refinement; sRGB stays sRGB, BC1 RGB samples alpha = 1, BC6H uses real HDR ASTC (CEM 11).
* `PANVK_BC_ETC=0` puts BC1 back on ASTC and BC2/BC3 on the 1.1.4-FC AFRC shadow (Patch 174; `PANVK_BC_AFRC=0` then uses ASTC); `PANVK_BC_EAC=0` puts BC4/BC5 back on exact R8/R8G8 (Patch 172).
* **Transcoders** (one thread per 4x4 block, specialised per format, each BC block decoded once):
  * `panvk_bc_s3tc_etc`: BC1/BC2/BC3 → ETC2 (alpha through the EAC table).
  * `panvk_bc_rgtc_eac`: BC4/BC5 → EAC R11/RG11, with a 256 KiB endpoint table built at compile time (`panvk_bc_eac_lutgen`).
  * Fallbacks: `panvk_bc_s3tc_astc`, `panvk_bc_alpha_fs` (AFRC), `panvk_bc_rgtc_snorm` (R8/R8G8).
  * `panvk_bc_bc6_astc`: BC6H → ASTC HDR (decoder bit-exact with Mesa's CPU BPTC decoder, SFLOAT negatives → 0).
  * `panvk_bc_bc7_astc`: all 8 BC7 modes.
* **Patch 125:** implements `vkCmdBindDescriptorBufferEmbeddedSamplersEXT` (D3D12 static samplers under `VK_EXT_descriptor_buffer`), fixing black textures in VKD3D-Proton titles that use them. **Patch 175** writes texel-buffer descriptors from `vkGetDescriptorEXT` with stride and format (D3D12 typed buffers).

### Patch 123: VKD3D DirectX 12 Essentials
Our tree includes **Patch 123** (`123-vkd3d-dx12-essentials.patch`), which unlocks full DirectX 12 compatibility on the Mali-G615:

#### 1. `VK_EXT_descriptor_buffer` (D3D12 Root Signatures & Descriptors)
* **Direct GPU Pointer Binding:** Eliminates descriptor set allocation bottlenecks by packing CBV, SRV, UAV, and sampler descriptors into compact 32-byte hardware descriptors.
* **Hardware Table Emission:** Emits descriptor buffer device addresses directly into the Valhall hardware resource table (`res_table`).
* **Sub-allocation Support:** Implements `vkGetDescriptorSetLayoutSizeEXT`, `vkGetDescriptorSetLayoutBindingOffsetEXT`, `vkGetDescriptorEXT`, `vkCmdBindDescriptorBuffersEXT`, and `vkCmdSetDescriptorBufferOffsetsEXT`.

#### 2. D3D12 Placed Heap Memory Isolation (`panvk_image.c`)
* Restricts Mali Transaction Elimination (CRC checksum) buffers strictly to WSI swapchain images (`if (!wsi_info) return false;`).
* Eliminates the CRC size overhead that previously caused non-WSI placed textures, render targets, and G-buffers to exceed strict power-of-two D3D12 placed heap bounds (`d3d12_resource_create_placed` out-of-memory errors).

#### 3. `VK_EXT_device_generated_commands` (DGC / ExecuteIndirect)
* Implements Vulkan hooks for GPU-driven indirect drawing batches (`ExecuteIndirect`):
  * `vkCreateIndirectCommandsLayoutEXT` / `vkDestroyIndirectCommandsLayoutEXT`
  * `vkGetGeneratedCommandsMemoryRequirementsEXT`
  * `vkCmdPreprocessGeneratedCommandsEXT` / `vkCmdExecuteGeneratedCommandsEXT`
  * `vkCreateIndirectExecutionSetEXT` / `vkDestroyIndirectExecutionSetEXT`

#### 4. Robustness 2 & Full Desktop Features
* Unlocks `robustImageAccess2 = true`, completing `VK_KHR_robustness2` alongside `robustBufferAccess2` and `nullDescriptor`.
* Native hardware dual-source blending (`dualSrcBlend`).
* Native vertex pipeline shader storage and atomics (`vertexPipelineStoresAndAtomics`).
* Full 8-stage Subgroup WaveOps (`0xFF`).

#### 5. Kernel UAPI Architecture & Hardware Tiler Reclamation
* **Native Queue Group ABI 1.18:** Adds direct support for CSF uAPI 1.18–1.24 (MediaTek MT6878 and Google Tensor CSF devices), eliminating fallback to the legacy 2020 1.6 ABI.
* **Automated Hardware Tiler Reclamation:** Upgrades `union kbase_ioctl_cs_tiler_heap_init` to the 24-byte layout with `buf_desc_va` (uAPI 1.14+), activating the kernel's hardware chunk reclamation scanner to prevent tiler heap starvation without submit-thread CPU stalls.

---

## DirectX Translation Layer Compatibility

| API | Layer | Status | Key Features Utilized |
| :--- | :--- | :--- | :--- |
| **Direct3D 9** | DXVK 2.x | **Supported** | Hardware dual-source blending, 32-bit index buffers, clip distance. |
| **Direct3D 10** | DXVK 2.x | **Supported** | Shader resource views, stream output emulation, depth clamp. |
| **Direct3D 11** | DXVK 2.x | **Supported** | Vertex stores & atomics, push descriptors, dynamic states, BCn transcoding (ETC2/EAC/ASTC). |
| **Direct3D 12** | VKD3D-Proton | **Supported** | Descriptor buffers, placed heaps, ExecuteIndirect (DGC), robustness2, BCn transcoding (ETC2/EAC/ASTC). |

---

## Repository Structure

```text
├── sources.lock               # Pinned upstream Mesa 26.3.0-devel commit
├── profiles/
│   └── g615-v11-csf.json      # Hardware profile for Mali-G615 (OPPO CPH2763)
├── patches/
│   ├── common/                # Platform-independent driver fixes
│   ├── android/               # Android OS and Bionic integration
│   ├── kbase-common/          # Shared mali_kbase kernel interface logic
│   ├── csf/                   # CSF frontend job scheduling and queues
│   ├── csf-v11/               # Valhall v11 specific patches (001-171)
│   │   ├── 123-vkd3d-dx12-essentials.patch    # VKD3D D3D12 essentials
│   │   ├── 124-panvk-astc-bcn-transcode.patch # BC1-BC7 -> ASTC 4x4 transcoder
│   │   ├── 125-bind-descriptor-buffer-embedded-samplers.patch # D3D12 static samplers
│   │   └── 126..128-*.patch                    # BC1-BC7 transcoder speed-ups
│   ├── jm-v9/                 # Universal ICD Valhall JM v9 support (001-003)
│   └── wsi/                   # Window System Integration (Android AHB & X11)
├── scripts/
│   ├── fetch-mesa.sh          # Fetches pinned Mesa upstream source
│   ├── apply-patches.sh       # Deterministically applies patch series
│   ├── build-android.sh       # Cross-compiles Bionic Android ICD (.so)
│   └── build-glibc.sh         # Compiles ARM64 glibc ICD for Linux/Termux/PRoot
└── docs/                      # Technical architecture documentation
```

---

## Building

### Prerequisites
* Linux or Termux / PRoot environment with `ccache`, `ninja`, `meson`, `python3`, `mako`, `pyyaml`.
* Android NDK (r25c+ / SDK 30+).

### Quick Build
```bash
# 1. Fetch pinned upstream Mesa source
./scripts/fetch-mesa.sh

# 2. Bootstrap host compile tools
./scripts/bootstrap-host-tools.sh

# 3. Apply the complete patch series (including Patches 123 & 124)
./scripts/apply-patches.sh --profile g615-v11-csf

# 4. Build Android Bionic Vulkan ICD
./scripts/build-android.sh --profile g615-v11-csf

# 5. Build glibc Vulkan ICD (Termux / Linux / PRoot)
./scripts/build-glibc.sh --profile g615-v11-csf
```

---

## Credits & Acknowledgements

* **[Mesa 3D](https://www.mesa3d.org/) & [Panfrost](https://gitlab.freedesktop.org/mesa/mesa/-/tree/main/src/panfrost):**
  * **Alyssa Rosenzweig** & **Boris Brezillon** – Authors of Panfrost and the PanVK Command Stream Frontend (CSF) driver.
  * The Collabora graphics team and broader Mesa community.
* **PanVK Kbase Android Base:**
  * **[ZenithBlue](https://github.com/zenithblue-oss)** – Author of the original `panvk-kbase-android` driver foundation and CSF kbase UAPI interface.
    The BC2/BC3 AFRC shadow (Patch 174) follows their fragment-decode AFRC design (csf-v11/150, 151, 180), including their AFRC row-stride fix.
* **DirectX Translation Layer Projects:**
  * **[DXVK](https://github.com/doitsujin/dxvk)** – **Philip Rebohle (doitsujin)** & **Joshua Ashton** for Direct3D 9/10/11 translation to Vulkan.
  * **Hans-Kristian Arntzen** – BC7/BC6H partition, anchor and weight tables (MIT), used by the BC transcoders.
  * **[VKD3D-Proton](https://github.com/HansKristian-Work/vkd3d-proton)** – **Hans-Kristian Arntzen** and contributors for Direct3D 12 translation to Vulkan.
* **Mali-G615 Maintenance & Feature Suites (Patches 123 & 124):**
  * **GunaCharanTeja** – Silicon verification on physical Mali-G615 (OPPO CPH2763, MT6878), author of **Patch 123** (`123-vkd3d-dx12-essentials.patch` for D3D12 descriptor buffers and placed heap isolation) and **Patch 124** (`124-panvk-astc-bcn-transcode.patch` for BC1–BC7 → ASTC 4x4 transcoding, reworked with patches 125–130 and 172–175), CI workflow optimization, and release packaging.

---

## License

This project is licensed under the same MIT / X11 license terms as upstream Mesa. See [LICENSES/](LICENSES/) for details.

