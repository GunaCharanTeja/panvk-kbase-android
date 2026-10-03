# Paired Runs Evidence & Sol Review Corrections

## 1. Sol Review Corrections Implemented
- **Staging Format & Decode**: Corrected staging texture format from `DXGI_FORMAT_R8G8B8A8_UNORM` to `td.Format` (`DXGI_FORMAT_B8G8R8A8_UNORM`, fmt 87). Direct3D 11 `CopyResource` requires identical source and destination formats. Mapped pixels decoded as BGRA (byte 0 Blue, byte 1 Green, byte 2 Red, byte 3 Alpha).
- **Explicit Outcome Separation**: API outcome (`API=PASS`) reported distinctly from readback outcome (`readback=SKIPPED` vs `readback=STAGING_CREATE_FAIL`). No unconditional visual PASS.
- **Device Removal Checks**: Verified `ID3D11Device_GetDeviceRemovedReason` pre-readback, pre-present, and post-present. Device removal triggers immediate API failure.
- **Positive Present Status**: Present status confirmed as `0x00000000` (`S_OK positive`) across all 8 frames.
- **Staging Control Variable**: `DXSMOKE_STAGING` environment variable toggles staging readback (`0` = skip, `1` = attempt).

## 2. Wine Vulkan & Extension Filtering Investigation
- **Installed Wine/Proton**: Proton 11.0-2-arm64ec (`win32u.so`, `winevulkan.so`).
- **WINE_VK_CONFIG & Extension Filtering**: Verified via disassembly and string symbol inspection. Installed Wine does NOT support `WINE_VK_CONFIG` or any extension filtering mechanism.
- **Root Cause of vkMapMemory2EXT Failure**:
  - In WOW64, Wine requires `VK_EXT_map_memory_placed` to place host Vulkan memory into caller-chosen 32-bit virtual addresses (`NtAllocateVirtualMemory`).
  - PanVK running on Mali kbase uses `SAME_VA` (GPU VA == CPU VA) where CPU mappings are established by kernel base at allocation time. Mapping a BO at a caller-chosen address is unsupported (`kbase_kmod_bo_mmap` returns `ENOTSUP`).
  - Wine logs: `win32u_vkMapMemory2KHR vkMapMemory2EXT failed: -5` (`VK_ERROR_MEMORY_MAP_FAILED`).
  - Without `VK_EXT_map_memory_placed`, Wine WOW64 drops 64-bit addresses (`host_ptr >> 32 != 0`) and unmaps them.

## 3. Paired Runs Results (Repeated 8 Frames)
- **Run 1 (DXSMOKE_STAGING=0)**:
  - API: PASS (frames 0..7 Present `0x00000000 S_OK positive`, device removed reason `0x00000000 S_OK`).
  - Readback: `SKIPPED`.
  - Exit code: 0.
  - X11 readback: 76,800 black pixels (all 0,0,0), 0 orange pixels.
- **Run 2 (DXSMOKE_STAGING=1)**:
  - API: PASS (frames 0..7 Present `0x00000000 S_OK positive`, device removed reason `0x00000000 S_OK`).
  - Readback: `STAGING_CREATE_FAIL` (`CreateTexture2D hr=0x80070057` due to DXVK 4MB staging allocation map failure on SAME_VA).
  - Exit code: 0.
  - X11 readback: 76,800 black pixels (all 0,0,0), 0 orange pixels.

## 4. fbread is not a WSI boundary
- A prior `VK_LAYER_panvk_fbread` intercept at `vkQueuePresentKHR` logged 320x240 format 44, 76,800 black pixels. That `fb.bin` is not in this bundle.
- The layer copies only when it accepts swapchain usage. Unsupported usage can skip or zero the copy. API `PASS` plus a black intercept does not show the image was black before present.
- Not a WSI isolation result. Native empty render pass vs transfer clear is `apps/panvk-launcher/tests/nclear.c`.
