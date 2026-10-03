# ARM64EC Direct3D 11 Visible Frame Proof

Device: `192.168.1.34:40501` (Xiaomi Poco X6 Pro, Mali-G615 MC6, PAN_ARCH 11).
Driver: PanVK Kbase G615 `0.1.0-beta.8` (`libvulkan_panfrost.so`, sha256 `44300ee413dc8f3df5e8f44323fa41de45b1ef62af00feebacc8eae8b6da8655`).
Display: Termux:X11 `:0` socket (`1216x2272`).
Container: Proton 11.0-2-arm64ec, imagefs bionic.
DXVK: Upstream stock `3.1.1-arm64ec` from `windows/system32/{d3d11,dxgi}.dll` (sha256 `9557e648...`, `7d134613...`).

## 1. Architecture Path & Root Cause Resolution

- **WOW64 i686 Root Cause**:
  In 32-bit WOW64 emulation under Wine, `vkMapMemory2KHR` requires placing host Vulkan memory into caller-chosen 32-bit virtual addresses (`VK_EXT_map_memory_placed`). Mali kbase operates under `SAME_VA` (CPU VA == GPU VA), where addresses are allocated in the 64-bit kernel VA range; caller-placed mmap returns `ENOTSUP` (`VK_ERROR_MEMORY_MAP_FAILED`). As a result, 32-bit DXVK staging texture allocations failed with `0x80070057` (`E_INVALIDARG`), leaving backbuffer pixels unreadable and presentation blank.
- **ARM64EC Resolution**:
  Building `dxsmoke.c` for `arm64ec-w64-mingw32` (`IMAGE_FILE_MACHINE_ARM64EC`, 0xA641) using the existing portable LLVM-MinGW toolchain (`arm64ec-w64-mingw32-clang`) targets the native 64-bit runtime. Wine executes ARM64EC natively without WOW64 thunking or 32-bit address truncation.
- **Direct3D 11 Backbuffer Verification (Source)**:
  `CreateTexture2D` with `D3D11_USAGE_STAGING` and `D3D11_CPU_ACCESS_READ` succeeds (`0x00000000`).
  `ID3D11DeviceContext::Map` succeeds (`0x00000000`, RowPitch 1280).
  Backbuffer readback confirms 100% orange (`40 80 ff ff` BGRA8, format 87):
  Every single frame (frames 0..7) has `orange=76800 zero=0 other=0 need=76800`.
  Total orange pixels read back across 8 frames: `614,400`.
  Log report: `readback=ORANGE_PASS (orange=614400 zero=0 other=0) frames=8`.

## 2. Presentation & Visible Frame Verification (Destination)

- **X11 Client Drawables**:
  - Top-level frame window `0xe00001`: `320x240+4+30`, `orange=76800/76800`.
  - Vulkan client window `0xa0002d`: `320x240+0+0`, `orange=76800/76800`.
  - Root window `0x511`: `1216x2272`, contains `76800/2762752` orange pixels.
- **Live Device Screen (`screenshot.png`)**:
  - Total near-orange pixels: `33,493` (exact `RGB(255,128,64)`: `29,726`).
  - Measured bounding box: `x=6..325, y=132..371`, exactly `320x240`.
  - Distinguishes conclusively from battery overlay (which accounts for only 748 pixels at bbox x=49..68, y=257..296).
  - Client drawable orange confirmed visible on device display.

## 3. Evidence Artifacts

- `d3d11.log`: Runtime stdout confirming `hr=0x00000000 feature=0xb000`, `Present frame=0..7 hr=0x00000000`, `readback=ORANGE_PASS`.
- `d3d11-stderr.log`: DXVK and driver logging confirming `DXVK: v3.1.1-arm64ec`, `panvk 26.2.99`, `Mali-G615 MC6`.
- `exit.txt`: Process exit code `0`.
- `xgetimage.log`: `XGetImage` capture log confirming `76800/76800` orange pixels on `0xe00001` and `0xa0002d`.
- `arm64ec-xread.ppm-0xa0002d.ppm`: Raw PPM dump of client window `0xa0002d`.
- `arm64ec-xread.ppm-0xe00001.ppm`: Raw PPM dump of frame window `0xe00001`.
- `screenshot.png`: Android screencap during hold window showing visible 320x240 orange window.
- `manifest.json`: SHA-256 integrity digests for all bundle files.
