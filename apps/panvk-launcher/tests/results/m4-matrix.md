# M4 Device Integration Matrix — 2026-10-02

Device: `192.168.1.34:40501` (Xiaomi Poco X6 Pro `duchamp`, MT6897 / Dimensity 8300-Ultra, Mali-G615 MC6, arm64-v8a).
Launcher APK: `dev.zenithblue.panvklauncher` debug build (:app:assembleDebug, :app:lintDebug clean).
Container: Proton 11.0-2-arm64ec, imagefs bionic, DXVK 3.1.1-arm64ec (pristine upstream binaries restored).
Selected Driver: PanVK Kbase G615 `0.1.0-beta.8` (`libvulkan_panfrost.so`, sha256 `44300ee413dc8f3df5e8f44323fa41de45b1ef62af00feebacc8eae8b6da8655`, Mesa 26.3.0-devel / CSF frontend, patches 083-084 X11 WSI dlopen + present id).
Display: `:0` socket `files/contents/imagefs/bionic/usr/tmp/.X11-unix/X0` via external `com.termux.x11`.
Launch path: Actual app UI (Compose) on device, Wine tab (Recent Executables and Executable Path).

## Root Cause & Environment Fix

- Defect: `imagefs/bionic/usr/lib/libandroid-sysvshm.so` function `sysvshm_connect()` called `getenv("ANDROID_SYSVSHM_SERVER")` which returned `NULL` when unset; subsequent `strncpy(..., NULL, ...)` dereferenced `NULL` and crashed `CreateWindowEx` inside `winex11.so`, returning NTSTATUS `STATUS_ACCESS_VIOLATION` (`0xC0000005`) as the HWND value.
- Fix: `Containers.env()` sets `ANDROID_SYSVSHM_SERVER=/dev/null`. When set to any dummy path, broker connect fails gracefully and `winex11` falls back to standard non-SHM X11 transport.
- Presentation Mode Truth: Software X11 copy (`PutImage` fallback). No zero-copy scanout or hardware vsync pacing claimed.

## UI Test Matrix (Four i686 dxsmoke Wrappers)

Built i686 one-arg PE wrappers calling sibling `dxsmoke.exe` in prefix `C:\dxsmoke\i686\`:

| API | PE Arch | UI Trigger | HRESULT | Adapter / Details | Visible frame | Log |
|---|---|---|---|---|---|---|---|
| d3d8 | i686 | UI Recent Run | exit 0, hr=0x00000000 | AMD Radeon RX 6700 XT (D3D8 spoof), swapchain 320x240 B8G8R8A8, Clear 0x0, Present 0x0 | NOT SHOWN. All D3D exact (255,128,64)=0. Full-res orange count is 748 (previously cited as ~374/376), strictly from the floating battery icon at bbox x=49..68, y=257..296, not the clear | `run-20261002-172557.log` (historical unverified) |
| d3d9 | i686 | UI Path Run | exit 0, hr=0x00000000 | AMD Radeon RX 6700 XT (D3D9 spoof), swapchain 320x240 B8G8R8A8, Clear 0x0, Present 0x0 | NOT SHOWN. All D3D exact (255,128,64)=0. Same 748 floating battery orange count (bbox x=49..68, y=257..296) in `i686-d3d9-termux-x11.png` | `run-20261002-172754.log` (historical unverified) |
| d3d10 | i686 | UI Recent Run | exit 0, hr=0x00000000 | Mali-G615 MC6 (0x13b5:0x1030), swapchain 320x240 B8G8R8A8, Clear issued, Present 0x0 | NOT SHOWN. All D3D exact (255,128,64)=0. Same 748 floating battery orange count in `i686-d3d10-termux-x11.png` | `run-20261002-172903.log` (historical unverified) |
| d3d11 | i686 | UI Path Run | exit 0, hr=0x00000000 | Mali-G615 MC6 (0x13b5:0x1030, feature 0xb000), Clear issued, Present 0x0 | NOT SHOWN. All D3D exact (255,128,64)=0. Same 748 floating battery orange count in `i686-d3d11-termux-x11.png` | `run-20261002-173137.log` (historical unverified) |

HRESULT PASS is not a visible frame. Across all D3D captures, exact RGB (255,128,64) count is 0; the orange pixels belong exclusively to the floating battery icon (748 full-res pixels at bbox x=49..68, y=257..296).

## Visible-frame retest — 2026-10-02 18:24

Hold is test-only: `dxsmoke` reads `DXSMOKE_HOLD` (default 750, clamp 200..20000). Launcher env is unchanged. Retest used `DXSMOKE_HOLD=7000` on the pushed i686 `dxsmoke.exe`, Termux:X11 already foreground on `com.termux.x11`, socket `X0` owned by `u0_a255`. Capture was `screencap` during the hold, plus `XGetImage` of the mapped 320x240 child (no `xwd` in the image). Note: capture timing is unproven and not conclusively verified.

| API | Window | Present | X drawable | Phone frame (y=120..h-140, status bar excluded) |
|---|---|---|---|---|
| d3d8 | visible client 320x240 | hr=0x0, fbread 320x240 fmt 44 | 320x240 child and root orange=0/76800; exact (255,128,64)=0 | `proof-d3d8.png` full-res orange=748 (battery bbox x=49..68, y=257..296 only; previously cited as 376); exact (255,128,64)=0 |
| d3d9 | visible client 320x240 | hr=0x0, fbread 320x240 fmt 44 | orange=0/76800; exact (255,128,64)=0 | `proof-d3d9.png` full-res orange=748 (battery bbox only; previously cited as 376); exact (255,128,64)=0 |
| d3d10 | visible client 320x240 | hr=0x0, Mali-G615 MC6, fbread 320x240 fmt 44 | orange=0/76800; exact (255,128,64)=0 | `proof-d3d10.png` full-res orange=748 (battery bbox only; previously cited as 376); exact (255,128,64)=0 |
| d3d11 | visible client 320x240 | hr=0x0, feature 0xb000, fbread 320x240 fmt 44 | orange=0/76800; exact (255,128,64)=0 | `proof-d3d11.png` full-res orange=748 (battery bbox only; previously cited as 376); exact (255,128,64)=0 |

`fb.bin` after the d3d11 present: `PFB1` 320x240 format 44 stride 1280, 307200 payload bytes, all zero. Repeating d3d11 with `VK_INSTANCE_LAYERS` unset (no fbread) similarly left the drawable black; however, layer interception cannot be conclusively excluded without further evidence.

Control, same server, same UID, same hold: GDI `FillRect(RGB(255,128,64))` mapped a 312x206 window whose raw `XGetImage` is 64272/64272, all exact `(255,128,64)`. In phone screenshot `proof-x11-gdi.png`, an orange predicate matches ~36250 pixels (earlier cited 18060 was subsampled); actual exact `(255,128,64)` count is 33196 (review exact 33196; prior 333196 typo is incorrect). X11 present works. The D3D swapchain image never reaches that drawable.

No orange 320x240 window was shown. Do not treat `proof-d3d*.png` as a passing frame.

## Stop / Relaunch Lifecycle Verification (Historical / Unverified)

Note: Historical claims below are unverified as run logs (refs 173551 / 173634, or 173600 / 173645) and process scans are not preserved in repository evidence. Full process cleanup is unproven absent preserved evidence.

1. **Launch Interactive Process:** UI launched `syswow64/cmd.exe` through path runner.
2. **Process Proof:** Process table reported active under app UID (`u0_a255`):
   - `cmd.exe` (PID 14122)
   - `wineserver` (PID 14127)
   - `winedevice.exe` (PIDs 14136, 14162)
3. **UI State:** Wine Environment header showed `Running`, "Stop" button became red and clickable (`ui-running-stop-active.png`).
4. **Stop Trigger:** Clicked "Stop" in UI.
   - `wineserver -k` executed. Full process cleanup unconfirmed absent preserved process scan evidence.
   - UI returned to `Container: Ready` and enabled action buttons.
5. **Relaunch Verification:**
   - Console: `wine cmd /c ver` -> `Microsoft Windows 10.0.19045`, exit 0 (historical / unverified, referenced run-20261002-173551.log / 173600 not preserved in repo).
   - Graphics: Recent Run for `run-d3d11.exe` -> exit 0, Present hr=0x0 (historical / unverified, referenced run-20261002-173634.log / 173645 not preserved in repo, `ui-relaunch-d3d11.png`).
