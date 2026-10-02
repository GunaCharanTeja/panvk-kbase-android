# M4 device matrix — 2026-10-02

Device: 192.168.1.34:40501 (2311DRK48I, arm64-v8a).
Launch path: app UI. Wine tab, path field or Recent Run, `graphics=true`.
Same container: Proton 11.0-2-arm64ec, imagefs bionic, DXVK 3.1.1-arm64ec on, bundled PanVK.
DISPLAY: `:0` socket `files/contents/imagefs/bionic/usr/tmp/.X11-unix/X0` (app-UID `termux-x11`, pid 19323, left running).
Termux:X11 activity came to the foreground on each graphical launch. Surface stayed black. No orange clear. See the pngs next to this file.

Built PEs are three, not four: one binary per arch, API chosen by argv. The UI Run path takes no args, so each API was a one-arg wrapper that `CreateProcess`es sibling `dxsmoke.exe`.

| API | PE | Result | HRESULT / exit | Notes |
|---|---|---|---|---|
| d3d8 | i686 | FAIL | `Direct3DCreate8` `0x80004005`, exit 1 | DXVK 3.1.1 loaded. `DxvkInstance::createInstance` failed. Log `run-20261002-125547.log`. |
| d3d9 | i686 | FAIL | no smoke HRESULT, child exit 3 | Same Vulkan instance failure. Watchdog `ExitProcess(3)` at 10s before the FAIL printf. Log `run-20261002-130316.log`. |
| d3d10 | i686 | FAIL | `D3D10CreateDeviceAndSwapChain` `0x80004005`, exit 1 | Same Vulkan instance failure. Log `run-20261002-130017.log`. |
| d3d11 | i686 | FAIL | `D3D11CreateDeviceAndSwapChain` `0x80004005`, exit 1 | DXVK enabled `VK_KHR_win32_surface` and friends, then instance create failed. Log `run-20261002-125511.log`. |
| d3d11 | aarch64 | FAIL | `LoadLibrary(d3d11.dll)` `0x800700c1`, exit 1 | Bad EXE format. Not an aarch64-native test. Log `run-20261002-125413.log`. |
| d3d8/9/10 | aarch64 | not run | | Same prefix DLL. d3d11 already proved it. |
| any | x86_64 | not run | | Proton here is aarch64-windows plus i386 wow64. No amd64 PE loader. |

## vkCreateInstance

Not a loader or DLL routing bug. No app env change. i686 DX8/9/10/11 stay FAIL for the same call.

`VkResult` **-7** `VK_ERROR_EXTENSION_NOT_PRESENT`. Unsupported extension: `VK_KHR_win32_surface`.

Reproduced with the app's imagefs `libvulkan.so.1` (loader 1.4.315), `VK_ICD_FILENAMES` / `VK_DRIVER_FILES` = `files/container/panvk_icd.json` (bundled `libvulkan_panfrost.so`), same `LD_LIBRARY_PATH` as `Containers.env`. Enumerated instance extensions include `VK_KHR_surface`, `VK_KHR_android_surface`, `VK_KHR_get_surface_capabilities2`, `VK_EXT_surface_maintenance1`, `VK_EXT_headless_surface`. No `VK_KHR_win32_surface`, no `VK_KHR_xlib_surface`, no `VK_KHR_xcb_surface`.

| enabled | `vkCreateInstance` |
|---|---|
| none | 0 |
| `VK_KHR_surface` only (what `winex11.so` requests) | 0 |
| `VK_KHR_surface` + `VK_KHR_xlib_surface` | -7 |
| DXVK set (`VK_EXT_surface_maintenance1`, `VK_KHR_get_surface_capabilities2`, `VK_KHR_surface`, `VK_KHR_win32_surface`) | -7 |

winevulkan translation evidence:

- i686 log `run-20261002-125511.log`: `Vulkan: Found vkGetInstanceProcAddr in winevulkan.dll @ 0x7ac33390`, then DXVK enables `VK_KHR_win32_surface`.
- `lib/wine/aarch64-unix/winevulkan.so` thunks `vkCreateWin32SurfaceKHR`. No `xlib` / `xcb` string. It does not `dlopen` the ICD. It calls `__wine_get_vulkan_driver` in `winex11.so`.
- `winex11.so` `dlopen`s `libvulkan.so.1` and its instance-extension list is `VK_KHR_surface`, `VK_KHR_get_physical_device_properties2`, `VK_KHR_external_memory_capabilities`, `VK_KHR_display`, `VK_EXT_direct_mode_display`, `VK_EXT_acquire_xlib_display`. It creates the Xlib surface itself (`X11DRV_VulkanInit`, `Failed to create Xlib surface`).
- `LD_LIBRARY_PATH` puts imagefs `usr/lib` first, so that `dlopen` is imagefs `libvulkan.so.1`, not `/system/lib64/libvulkan.so`. ICD json points at PanVK. Direct ICD receive of the win32 name is DXVK asking winevulkan for the Windows extension. Expected. Not a wrong DLL.
- PanVK NEEDED is Android only (`libnativewindow.so`, `libsync.so`, no `libX11` / `libxcb`). X11 WSI was not built in. `VK_KHR_xlib_surface` string in the .so is a Vulkan header name, not an advertised extension.

Also in those logs, not the failing call: `libGL.so.1` missing, `NtUserChangeDisplaySettings` returned -2. Screen stays the empty Termux:X11 surface. No device, no clear, no present.

## App / content blockers

- DXVK 3.1.1-arm64ec `system32/d3d11.dll` is ARM64EC, not aarch64-native. COFF machine `0x8664`, sections `.hexpthk` and `.a64xrm`, strings `arm64ec` and `x86_64`. Prefix `syswow64/d3d11.dll` is i386 (`0x014c`). Proton `lib/wine/aarch64-windows/d3d11.dll` is native arm64 (`0xAA64`) builtin. An aarch64 smoke PE loads `system32` and gets `0x800700c1`. Do not treat that as a native-DXVK result. i686 wow64 is the arch this DXVK build can serve.
- UI `Run` required storage permission even for `filesDir`. Dialog never showed, so the tap was a no-op. Fixed in `MainActivity.runExeWithPermission`: app-private paths skip that check. Rebuilt and reinstalled before the matrix above.
- `cmd /c ver` on the same container returned `Microsoft Windows 10.0.19045`, exit 0.

No pass. Fix is a PanVK X11 WSI build (`VK_KHR_xlib_surface`), not an app routing change.
