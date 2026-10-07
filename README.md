# PanVK Kbase Android Vulkan Driver

Open Mesa **PanVK** Vulkan driver communicating directly with the vendor `mali_kbase` kernel interface (`/dev/mali0`) on Android, with native support for **DirectX 9, 10, 11 (DXVK)** and **DirectX 12 (VKD3D-Proton)**.

[![Vulkan 1.4](https://img.shields.io/badge/Vulkan-1.4-AC162C?logo=vulkan&logoColor=white)](#)
[![GPU Mali-G615](https://img.shields.io/badge/GPU-Mali--G615%20(v11%20CSF)-0091BD?logo=arm&logoColor=white)](#)
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

## Key Enhancements & Patch 108

Our tree includes **Patch 123** (`123-vkd3d-dx12-essentials.patch`), which unlocks full DirectX 12 compatibility on the Mali-G615:

### 1. `VK_EXT_descriptor_buffer` (D3D12 Root Signatures & Descriptors)
* **Direct GPU Pointer Binding:** Eliminates descriptor set allocation bottlenecks by packing CBV, SRV, UAV, and sampler descriptors into compact 32-byte hardware descriptors.
* **Hardware Table Emission:** Emits descriptor buffer device addresses directly into the Valhall hardware resource table (`res_table`).
* **Sub-allocation Support:** Implements `vkGetDescriptorSetLayoutSizeEXT`, `vkGetDescriptorSetLayoutBindingOffsetEXT`, `vkGetDescriptorEXT`, `vkCmdBindDescriptorBuffersEXT`, and `vkCmdSetDescriptorBufferOffsetsEXT`.

### 2. D3D12 Placed Heap Memory Isolation (`panvk_image.c`)
* Restricts Mali Transaction Elimination (CRC checksum) buffers strictly to WSI swapchain images (`if (!wsi_info) return false;`).
* Eliminates the CRC size overhead that previously caused non-WSI placed textures, render targets, and G-buffers to exceed strict power-of-two D3D12 placed heap bounds (`d3d12_resource_create_placed` out-of-memory errors).

### 3. `VK_EXT_device_generated_commands` (DGC / ExecuteIndirect)
* Implements Vulkan hooks for GPU-driven indirect drawing batches (`ExecuteIndirect`):
  * `vkCreateIndirectCommandsLayoutEXT` / `vkDestroyIndirectCommandsLayoutEXT`
  * `vkGetGeneratedCommandsMemoryRequirementsEXT`
  * `vkCmdPreprocessGeneratedCommandsEXT` / `vkCmdExecuteGeneratedCommandsEXT`
  * `vkCreateIndirectExecutionSetEXT` / `vkDestroyIndirectExecutionSetEXT`

### 4. Robustness 2 & Full Desktop Features
* Unlocks `robustImageAccess2 = true`, completing `VK_KHR_robustness2` alongside `robustBufferAccess2` and `nullDescriptor`.
* Native hardware dual-source blending (`dualSrcBlend`).
* Native vertex pipeline shader storage and atomics (`vertexPipelineStoresAndAtomics`).
* Full 8-stage Subgroup WaveOps (`0xFF`).

### 5. Kernel UAPI Architecture & Hardware Tiler Reclamation
* **Native Queue Group ABI 1.18:** Adds direct support for CSF uAPI 1.18–1.24 (MediaTek MT6878 and Google Tensor CSF devices), eliminating fallback to the legacy 2020 1.6 ABI.
* **Automated Hardware Tiler Reclamation:** Upgrades `union kbase_ioctl_cs_tiler_heap_init` to the 24-byte layout with `buf_desc_va` (uAPI 1.14+), activating the kernel's hardware chunk reclamation scanner to prevent tiler heap starvation without submit-thread CPU stalls.

---

## DirectX Translation Layer Compatibility

| API | Layer | Status | Key Features Utilized |
| :--- | :--- | :--- | :--- |
| **Direct3D 9** | DXVK 2.x | **Supported** | Hardware dual-source blending, 32-bit index buffers, clip distance. |
| **Direct3D 10** | DXVK 2.x | **Supported** | Shader resource views, stream output emulation, depth clamp. |
| **Direct3D 11** | DXVK 2.x | **Supported** | Vertex stores & atomics, push descriptors, dynamic states, BCn decode. |
| **Direct3D 12** | VKD3D-Proton | **Supported** | Descriptor buffers, placed heaps, ExecuteIndirect (DGC), robustness2. |

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
│   ├── csf-v11/               # Valhall v11 specific patches (001-123)
│   │   └── 123-vkd3d-dx12-essentials.patch # VKD3D D3D12 essentials
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

# 3. Apply the complete patch series (including Patch 108)
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
* **Mali-G615 Maintenance & DirectX 12 Patch 123:**
  * **GunaCharanTeja** – Silicon verification on physical Mali-G615 (OPPO CPH2763, MT6878), author of **Patch 123** (`123-vkd3d-dx12-essentials.patch`), `VK_EXT_descriptor_buffer` hardware packing, placed heap memory isolation, and driver maintenance.

---

## License

This project is licensed under the same MIT / X11 license terms as upstream Mesa. See [LICENSES/](LICENSES/) for details.

