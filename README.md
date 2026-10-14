# PanVK Kbase Android Vulkan Driver

Open Mesa **PanVK** Vulkan driver communicating directly with the vendor `mali_kbase` kernel interface (`/dev/mali0`) on Android, with native support for **DirectX 9, 10, 11 (DXVK)** and **DirectX 12 (VKD3D-Proton)**, featuring GPU **BC1–BC7 texture conversion** (ASTC 4x4 LDR/HDR, AFRC for BC2/BC3, exact R8/RG8 for BC4/BC5).

[![Release 1.1.2-FC](https://img.shields.io/badge/Release-1.1.2--FC-brightgreen)](#)
[![Vulkan 1.4](https://img.shields.io/badge/Vulkan-1.4-AC162C?logo=vulkan&logoColor=white)](#)
[![GPU Mali-G615](https://img.shields.io/badge/GPU-Mali--G615%20(v11%20CSF)-0091BD?logo=arm&logoColor=white)](#)
[![BCn on GPU](https://img.shields.io/badge/BC1--BC7-ASTC%20%7C%20AFRC%20%7C%20R8%2FRG8-success)](#)
[![DXVK D3D9/10/11](https://img.shields.io/badge/DXVK-D3D9%20%7C%2010%20%7C%2011-555)](#)
[![VKD3D D3D12](https://img.shields.io/badge/VKD3D--Proton-D3D12-7B1FA2)](#)
[![Mesa 26.3-devel](https://img.shields.io/badge/Mesa-26.3--devel-6E4C9A)](#)
[![Maintainer GunaCharanTeja](https://img.shields.io/badge/Maintainer-GunaCharanTeja-blue)](https://github.com/GunaCharanTeja/panvk-kbase-android)

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

### Upstream RC2 / Beta.18 changes (Patches 131–171)
* **Removed in 1.1.2-FC: register-allocation speedups (Patches 152–154, 160–164)**: with them Red Dead Redemption (D3D12) loses the device while loading (GPU read translation fault, bisected on device). Very large shaders compile slower again until a validated replacement lands.
* **Persistent Mesa Shader Disk Cache on Android (Patch 170)**: Keeps Mesa's on-disk shader cache on by default on Android, so later launches reuse shaders compiled earlier instead of recompiling them. The cache is keyed by the driver build ID and GPU; `MESA_SHADER_CACHE_DISABLE=true` turns it off.
* **Android Property Query Caching (Patch 171)**: Caches `os_get_option()` lookups instead of querying Android properties on every call (upstream measured 128 ms of stutter during image creation).
* **CSF Prerast Producer Scoreboard Synchronization (Patch 132)**: Adds missing iterator scoreboard barrier before viewport runs, fixing race conditions in multi-viewport scenes.
* **GPU Model & Small-Core Identification (Patches 131, 140)**: Core-count device naming for Immortalis/small GPUs; adds the Mali-G710 model row.

### Patches 124–130, 172–174: BC1–BC7 transcoding (40–78% less texture memory than RGBA8)
Mali has no BCn sampler, so BC textures are converted on the GPU at upload time into formats the hardware samples natively. Each format uses the path that measured best on device:

| BC format | Stored as | Memory (2048², full mips) | Upload, Mali-G615 MC2 |
| :--- | :--- | :--- | :--- |
| BC1 | ASTC 4x4 LDR | 8.0 MB (−67% vs RGBA8) | real textures 1.4–1.8 ms |
| BC2, BC3 | **AFRC 12 bpp** RGBA8 shadow (Patch 174) | 13.3 MB (−50%) | 7.7–8.8 ms |
| BC4 / BC5 (UNORM and SNORM) | **R8 / R8G8, exact** (Patch 172) | 8.0 / 16.0 MB | 2.7–6.0 ms |
| BC6H UFLOAT / SFLOAT | ASTC 4x4 **HDR** | 10.7 MB (−78% vs RGBA16F) | 40 ms |
| BC7 | ASTC 4x4 LDR | 10.7 MB (−60%) | real 44–60 ms |

* **Quality (PSNR vs. the BC decode, real game textures):** BC1 57–66 dB, BC2/BC3 (AFRC) 39–52 dB, BC4/BC5 exact, BC6H 50–54 dB (realistic blocks), BC7 41–54 dB. ASTC paths map BC endpoints and indices onto ASTC directly where the block allows it, otherwise a principal-axis fit with least-squares refinement; sRGB stays sRGB, BC1 RGB samples alpha = 1, BC6H uses real HDR ASTC (CEM 11).
* **AFRC (Patch 174)** needs a GPU with AFRC (Mali-G615 has it); cube, sparse and 3D images keep the ASTC transcode. `PANVK_BC_AFRC=0` puts BC2/BC3 back on ASTC (lowest memory), `PANVK_BC_AFRC=32` uses 16 bpp.
* **Transcoders** (one thread per 4x4 block, specialised per format, each BC block decoded once):
  * `panvk_bc_s3tc_astc`: BC1 (and BC2/BC3 without AFRC).
  * `panvk_bc_alpha_fs`: BC2/BC3 fragment decode into the AFRC shadow.
  * `panvk_bc_rgtc_snorm`: BC4/BC5 → R8/R8G8 (UNORM and SNORM).
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
| **Direct3D 11** | DXVK 2.x | **Supported** | Vertex stores & atomics, push descriptors, dynamic states, pure ASTC 4x4 BCn transcoding. |
| **Direct3D 12** | VKD3D-Proton | **Supported** | Descriptor buffers, placed heaps, ExecuteIndirect (DGC), robustness2, pure ASTC 4x4 BCn transcoding. |

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

