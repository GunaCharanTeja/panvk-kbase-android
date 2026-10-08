# PanVK Kbase Android Vulkan Driver

Open Mesa **PanVK** Vulkan driver communicating directly with the vendor `mali_kbase` kernel interface (`/dev/mali0`) on Android, with native support for **DirectX 9, 10, 11 (DXVK)** and **DirectX 12 (VKD3D-Proton)**, featuring pure hardware-accelerated **ASTC 4x4 BC1–BC7 transcoding**.

[![Release 1.0.9-FC](https://img.shields.io/badge/Release-1.0.9--FC-brightgreen)](#)
[![Vulkan 1.4](https://img.shields.io/badge/Vulkan-1.4-AC162C?logo=vulkan&logoColor=white)](#)
[![GPU Mali-G615](https://img.shields.io/badge/GPU-Mali--G615%20(v11%20CSF)-0091BD?logo=arm&logoColor=white)](#)
[![ASTC 4x4 Transcoding](https://img.shields.io/badge/ASTC%204x4-Hardware%20Transcoded-success)](#)
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

## Key Enhancements: Patches 123 & 124

### Patch 124: Pure ASTC 4x4 BC1–BC7 Transcoding (75% VRAM & Bandwidth Reduction)
Directly transcodes every DirectX BC format into native hardware `VK_FORMAT_ASTC_4x4_UNORM_BLOCK` and `VK_FORMAT_ASTC_4x4_SRGB_BLOCK` using single-pass GPU compute transcoders:
* **75% VRAM Memory Footprint Reduction**: ASTC 4x4 consumes strictly 16 bytes per $4 \times 4$ block (identical to native BC3/BC5/BC7, and half of BC1/BC4 uncompressed), cutting texture memory footprint by 75% compared to legacy uncompressed RGBA8/R16G16/RGBA16F decode planes.
* **75% Texture Sampling Memory Bandwidth Savings**: Mali Valhall hardware texture samplers decompress ASTC silicon blocks on-the-fly, eliminating memory bus saturation and cache thrashing.
* **Full-Suite 14-Format Coverage**: Transcodes BC1 (RGB/RGBA UNORM & SRGB), BC2 (UNORM & SRGB), BC3 (UNORM & SRGB), BC4 (UNORM & SNORM), BC5 (UNORM & SNORM), BC6H (UFLOAT & SFLOAT), and BC7 (UNORM & SRGB).
* **Single-Pass GPU Compute Transcoders**:
  * `panvk_bc_s3tc_astc`: Handles BC1, BC2, BC3, BC4, and BC5 directly into ASTC 4x4 blocks.
  * `panvk_bc_bc6_astc`: Handles BC6H with fast FP16 range decoding directly into ASTC 4x4 blocks.
  * `panvk_bc_bc7_astc`: Handles all 8 BC7 modes directly into ASTC 4x4 blocks.
* **Legacy RGBA8 Decoding Completely Removed**: Zero uncompressed decode fallback paths or planes (`PANVK_BC_MODE` removed). ASTC 4x4 transcoding is the sole, unconditional mechanism.

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
│   ├── csf-v11/               # Valhall v11 specific patches (001-124)
│   │   ├── 123-vkd3d-dx12-essentials.patch    # VKD3D D3D12 essentials
│   │   └── 124-panvk-astc-bcn-transcode.patch # Pure ASTC 4x4 BC1-BC7 transcoder
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
* **DirectX Translation Layer Projects:**
  * **[DXVK](https://github.com/doitsujin/dxvk)** – **Philip Rebohle (doitsujin)** & **Joshua Ashton** for Direct3D 9/10/11 translation to Vulkan.
  * **[VKD3D-Proton](https://github.com/HansKristian-Work/vkd3d-proton)** – **Hans-Kristian Arntzen** and contributors for Direct3D 12 translation to Vulkan.
* **Mali-G615 Maintenance & Feature Suites (Patches 123 & 124):**
  * **GunaCharanTeja** – Silicon verification on physical Mali-G615 (OPPO CPH2763, MT6878), author of **Patch 123** (`123-vkd3d-dx12-essentials.patch` for D3D12 descriptor buffers and placed heap isolation) and **Patch 124** (`124-panvk-astc-bcn-transcode.patch` for pure ASTC 4x4 BC1–BC7 transcoding), CI workflow optimization, and release packaging.

---

## License

This project is licensed under the same MIT / X11 license terms as upstream Mesa. See [LICENSES/](LICENSES/) for details.

