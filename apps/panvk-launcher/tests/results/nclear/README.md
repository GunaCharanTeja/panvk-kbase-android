# Native clear vs DXVK presenter

Device `192.168.1.34:40501`. Driver unchanged: `files/m4wsi/libvulkan_panfrost.so`
sha256 `44300ee413dc8f3df5e8f44323fa41de45b1ef62af00feebacc8eae8b6da8655`
(`driver.sha`). Layers off (`VK_LOADER_LAYERS_DISABLE=~all~`). No `fb.bin`.

`apps/panvk-launcher/tests/nclear.c` presents 320x240 `B8G8R8A8_UNORM` orange
`{1, 0.5, 0.25, 1}`. `xfer` is `vkCmdClearColorImage`. `rp` is an empty render
pass, `loadOp=CLEAR`, no draws. One `XGetImage` after the presents, before
exit. Each swapchain image has its own render-finished semaphore. A single
shared semaphore is unsafe once `minImageCount` images are in flight.

| mode | XGetImage | raw |
|---|---|---|
| xfer 320x240 | 76800/76800 orange, center `0xff8040` | `nclear-xfer-320.raw` all `40 80 ff ff` |
| rp 320x240 | 76800/76800 orange, center `0xff8040` | `nclear-rp-320.raw` all `40 80 ff ff` |
| both at 640x400 | 256000/256000 orange | logs only; shots `shot-*-640.png` |

Existing `wsitest` (transfer clear, no mid-loop grab) center pixel `0xff7000`, `PASS xlib` (`wsitest-recheck.log`).

DXVK d3d11, layer off, installed option only: `DXVK_CONFIG=dxgi.syncInterval=0`
(string present in syswow64 `dxgi.dll`; `dxgi.numBackBuffers` is not).
Log: present mode `VK_PRESENT_MODE_IMMEDIATE_KHR`, 4 images, BGRA8.
API `PASS`, readback `SKIPPED`. Client `XGetImage` 320x240 orange=0/76800
(`dxvk-xget.log`, `dxvk-client.ppm`). `shot-dxvk-sync0.png` near-orange count 0.

Empty render-pass clear reaches X11. The black frame is the DXVK presenter path, not a missing native clear.

## Source vs destination, 2026-10-02 20:36

Same driver sha256 `44300ee4…da8655`. Layers off. `TRANSFER_SRC` is advertised (`caps usage=0x8009f`). Copy is `vkCmdCopyImageToBuffer` after a present-src to transfer-src barrier, then host invalidate on host-cached memory. Per-image render-finished semaphores.

| probe | source (GPU copy) | destination |
|---|---|---|
| nclear xfer | `nclear-xfer-src.raw` 76800/76800 `40 80 ff ff` | `nclear-xfer.raw` / XGetImage 76800/76800 `0xff8040` |
| nclear rp | `nclear-rp-src.raw` 76800/76800 `40 80 ff ff` | `nclear-rp.raw` / XGetImage 76800/76800 `0xff8040` |
| dxsmoke d3d11, held | not readable | client `0xe00001` `dx11-client-320.ppm` 0/76800 orange, 76800 black |

DX11 back-buffer readback does not run. `CheckFormatSupport` for fmt 87 (`B8G8R8A8_UNORM`) returns `0x32fef3f3`. `CreateTexture2D` staging, same format, returns `0x80070057` (`E_INVALIDARG`) on every frame. Log: `readback=STAGING_CREATE_FAIL`, process exit 4. Present itself is `S_OK`. With staging off, exit is 0 and the line is `readback=SKIPPED`.

Held capture (`DXSMOKE_HOLD=8000`, XGetImage after `Present frame=0`, before exit): windows `0xe00001` and `0xa0002d` are 320x240, orange 0/76800. `shot-dx11-hold.png` near-orange count 0. Installed `d3d11.dll` still matches `/var/tmp/panvk/dxtests/dxvk-arm64ec/syswow64` (`3f565278…96138`).

Official `v3.1.1` release assets are `dxvk-3.1.1.tar.gz` (x32/x64 DLLs) and `dxvk-3.1.1.tar.zst`. No source tarball. The matching source is git tag `v3.1.1` only. Replacing the installed arm64ec DLL would violate the pristine-DLL rule, so no DXVK patch was built. The proven gap is presenter input (native image is orange) versus presenter output (DX11 drawable is black). The D3D11 staging copy never starts.

`fbread` is not WSI proof. That `fb.bin` was not kept. Do not treat a missing copy as a measured zero.

## Sol Review Findings & Source Unknown Classification

Per Sol review findings, when a source buffer or swapchain image cannot be directly verified via staging copy or host-visible readback, the evidence status must be documented as `source=unknown` rather than recording a measured zero or treating missing readback as passing. In WOW64 i686 runs where staging texture creation failed (`E_INVALIDARG` / `0x80070057` due to 32-bit `SAME_VA` address space exhaustion), the backbuffer was strictly `source=unknown`.

## Exit Gate & Enum Results Check (2026-10-02)

`apps/panvk-launcher/tests/nclear.c` incorporates a hard exit gate and explicit Vulkan enum verification:
- **Enum Results Check**: `VkPresentInfoKHR` checks both queue return status (`r == VK_SUCCESS`) and per-swapchain result enum (`pResults[0] == VK_SUCCESS`).
- **Pixel Count Exit Gate**: Asserts exact matches `src_orange == W * H` (source copy) and `xi_orange == W * H` (drawable readback) with `zero == 0` and `other == 0`. Any mismatch terminates with non-zero exit code.
- Device run: `rp` and `xfer` modes both confirmed `76800/76800` orange on source and drawable, `EXITGATE: PASS`.

## ARM64EC Resolution

Under ARM64EC (64-bit Windows PE execution under Proton 11.0-2-arm64ec with `system32` ARM64EC DXVK 3.1.1), the WOW64 mapping restriction is eliminated:
- Direct3D 11 staging texture creation and `Map` succeed (`0x00000000`, pitch 1280).
- Backbuffer readback confirms 100% orange (`40 80 ff ff`, 76,800/76,800 pixels per frame, 614,400 across 8 frames).
- Presentation to X11 client window (`0xa0002d`) confirms 76,800/76,800 orange pixels.
- Live device screenshot confirms visible 320x240 client window (34,453 orange pixels at bbox x=6..325, y=132..371).
