# 091: placed maps backed by dma-heap memory (beta.10)

## Problem

32-bit games under Wine WoW64 (Proton 11 arm64ec + FEX) map host-visible
memory below 4 GiB with `VK_EXT_map_memory_placed`. kbase SAME_VA regions
cannot be moved or mapped twice (`MAP_FIXED` and `mremap` give EINVAL), so
beta.9 gave placed maps an anonymous shadow copy. The shadow was merged
word by word against a snapshot on every queue kick and wait.

- Speed: each kick and wait cost time in proportion to all placed bytes.
  DXVK maps whole memory chunks, so Need for Speed Most Wanted (D3D9, i686)
  ran at 0.5 fps on the title screen. The i686 D3D9 cube ran at about 1 fps.
- Correctness: commit 2161336 (old 091) moved the pull to GPU completion.
  It raced: a kick between the idle check and the lock skipped the pull.
  A doorbell without a kick pushed nothing. sync_file waits were not
  hooked. DXVK HUD text and game text were garbled, and i686 D3D10/D3D11
  showed a flat grey frame.

## Fix

- Patch 091 (`panvk_device_memory.c`): when the app enables
  `memoryMapPlaced`, host-visible memory is allocated as a shareable BO
  (`exclusive_vm == NULL`).
- On kbase, shareable BOs already come from `/dev/dma_heap/system` and are
  imported with `KBASE_IOCTL_MEM_IMPORT` (UMM). `kbase_kmod_bo_mmap` maps
  the same dma-buf a second time at the caller's address
  (`MAP_SHARED | MAP_FIXED`). CPU and GPU share pages, so there is no copy
  and no sync. `bo_munmap` unmaps only that alias.
- The shadow copy (about 200 lines) and the old 091 completion hook are
  deleted. Patch 075 context follows the smaller device struct.
- Coherency: import outflags `0x4540f` keep `BASE_MEM_COHERENT_SYSTEM`, so
  the kernel keeps CPU and GPU coherent and needs no `MEM_SYNC`.
- Known limit (marked `ponytail:` in `kbase_kmod.c`): without a dma-heap,
  placed maps fail with `VK_ERROR_MEMORY_MAP_FAILED`.

## Device results (Poco X6 Pro, Mali-G615 MC6, PanPlay, DXVK_HUD=full)

| Test | beta.9 | beta.10 |
|---|---|---|
| NFS Most Wanted (i686 D3D9) | 0.5 fps, title screen still loading after 85 s | 39-46 fps in menus and gameplay, 66 fps in the main menu; HUD and game text clean |
| i686 cube D3D8 / D3D9 / D3D10 / D3D11 | D3D9 about 1 fps; D3D10/11 grey frame | 47 / 47 / 47 / 46 fps, geometry correct |
| x86_64 and ARM64EC cubes, all four APIs | about 46 fps | 46-47 fps (no change) |
| MiSide (x86_64 D3D11) | about 44 fps | 47 fps |

Screenshots: `validation/driver-remaining/091-device/`.

## CTS

The CTS ran in a chroot with a glibc ICD built from the beta.10 tree.

- Cases: `dEQP-VK.memory.mapping.*`, `dEQP-VK.memory.map_placed.*`,
  `dEQP-VK.synchronization{,2}.basic.*` (4533 cases).
- Result: 4520 pass, 0 fail, 13 NotSupported. All 13 NotSupported are
  `*_cq` event cases, which need an exclusive compute queue.
- `map_placed`: 13/13 pass, including `gpu_access.read_write` and both
  unmap-reserve variants.

Details: `validation/driver-remaining/091-device/cts-summary.txt`.

## Build note

The host LLVM was upgraded from 22.1.8 to 23.1.1, so `mesa_clc` could not
find `libLLVM.so.22.1` (exit 127). The workaround needs no root: extract
`llvm-libs-22.1.8` and `clang-22.1.8` from the Arch archive into
`/var/tmp/panvk/llvm22`, then build with
`LD_LIBRARY_PATH=/var/tmp/panvk/llvm22/usr/lib` and
`HOST_TOOLS=tmp/rel9/src/build/host-tools/bin`.
