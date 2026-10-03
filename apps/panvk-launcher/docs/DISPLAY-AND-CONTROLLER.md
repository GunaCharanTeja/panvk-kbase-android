# Built-in display server and controller input

Applies to PanPlay (`apps/panvk-launcher`). Status 2026-10-03. Display: implemented, device results in `tests/results/builtin-xserver/`. Controller: implemented, see "Controller status".

## Display: choice

In-process X server taken from Winlator (`com.winlator.xserver`, pure Java, ~8k lines) plus its epoll/ancillary-fd JNI connector, a GLES compositor (`XServerView`/`GLRenderer`) and `TouchpadView`.
Pinned: https://github.com/brunodev85/winlator `db6d7aa446b5607b48ac2ee36dc4333799ff3dc7` (last upstream commit with app source). Details in `third_party/winlator/README.md`.

- Wine (`winex11.drv`, libX11) connects to `<imagefs>/usr/tmp/.X11-unix/X0`, the dir Wine's TMPDIR already points at. `DISPLAY=:0`.
- Present path used by PanVK (Xlib/XCB `PutImage`) hits core X11 `PutImage` in `xserver/requests/DrawRequests`. MIT-SHM is served too (SysV broker socket `ANDROID_SYSVSHM_SERVER`, Winlator's `libandroid-sysvshm.so` already in the imagefs). Settings toggle "MIT-SHM present".
- Window map, keyboard and pointer events, resize of the root, cursor: all in the Java server. Input comes from `XServerActivity` (hardware keyboard, mouse, touchpad-style touch).
- Termux:X11 stays as fallback: Wine tab switch "Built-in X server (off = Termux:X11)". Default is built-in.

### License

| Piece | License | Consequence |
|---|---|---|
| Winlator X server, connector, renderer, TouchpadView (vendored) | LGPL-2.1 | Files keep LGPL-2.1, headers/README mark our edits, source is in this repo, so the APK can be rebuilt with a modified copy. No relicensing. |
| Launcher glue (`BuiltinXServer.kt`, `XServerActivity.kt`, gamepad files) | MIT (repo scaffolding) | Calls the LGPL code as a library. |
| Termux:X11 (rejected) | GPL-3.0 | Embedding makes the whole APK GPL-3.0 and conflicts with the MIT scaffolding policy in `NOTICE.md`. |
| GameNative's copy of the X server (rejected) | GPL-3.0 | Same problem. Upstream Winlator is the cleaner LGPL source. |
| Winlator-Ludashi | GitHub says MIT, but it is a fork of LGPL code | Not trusted to relicense; also carries HWC/DRI3 bypass code we do not need. |

Not legal advice. `NOTICE.md` should get a Winlator LGPL-2.1 entry before a release.

### Alternatives rejected

- Embed Termux:X11 (Lorie): C Xorg fork (`libXlorie`) with its own build chain, plus GPL-3.0. Too heavy and wrong license.
- Write a new X server: weeks of work, Winlator's passes real Wine already.
- Keep Termux:X11 only: separate app, needs a Termux TMPDIR socket, user setup. Kept as fallback.
- `fb.bin` framebuffer viewer (`ScreenActivity`): not an X server, cannot serve DISPLAY.
- Wayland/AHB zero-copy (see launcher plan): out of scope here, the Java server can be swapped later without changing the Wine side.

### Known limits

- No XKB, RANDR, RENDER, XFIXES, XInput2 extensions (Winlator does not ship them). DRI3 removed (needs AHardwareBuffer import, not available on Mali kbase).
- `WinHandler` is stubbed: no relative-mouse injection into Wine, no bring-to-front. Games that grab the mouse in relative mode need the touchpad cursor instead.
- Software present only (CPU copy). Performance claims in the test results are for that path.

## Controller

Chosen: SDL virtual joystick via an LD_PRELOAD shim (own code, `app/src/main/cpp/padshim.c`).

- Proton's `winebus.so` has an SDL backend only (no udev, Android apps cannot read `/dev/input`). Imagefs ships SDL 2.32.6 with `SDL_JoystickAttachVirtualEx`.
- `GamepadBridge` maps Android `KeyEvent`/`MotionEvent` (SOURCE_GAMEPAD/JOYSTICK) and the touch overlay to a 64-byte file in `files/gamepad_shm/gamepad.mem`.
- `libpadshim.so`, injected via `LD_PRELOAD` into Wine processes, waits for `winebus.so` to load, then attaches an Xbox 360 (045e:028e) virtual joystick and polls the file every 4 ms. Wine exposes it as XInput/DInput.
- Touch overlay (`GamepadOverlayView`): left/right stick, D-pad, A/B/X/Y, LB/RB/LT/RT, Start/Back. Shown when no hardware pad is connected; four-finger tap toggles.

Alternatives rejected: Winlator `winhandler.exe` UDP protocol (needs a helper exe and patched Wine, source gone upstream), GameNative `evshim` (GPL-3.0, hard-coded package path in the prebuilt copy in imagefs), evdev/uinput (no access), xinput DLL replacement (per-arch builds, fragile).

## Device results (2026-10-03, Mali-G615, 1280x720 X screen on 1920x1080 surface)

- dxcube arm64ec (D3D11, DXVK 3.1.1, PanVK): renders in the app, 40-51 FPS with the full HUD (software PutImage present). 5+ minute run, launcher stays foreground, no freezer kill.
- dxdraw x64: renders (HUD visible). Screenshots: `tests/results/builtin-xserver/`.
- Black display root cause was a missing core request (GetPointerMapping, opcode 117; Wine's X11 driver aborts window creation) plus a leaked Termux:X11 owning the abstract socket. Both fixed; the built-in server now kills own stale `termux-x11` processes at start.
- Overlay: size = dp base * scale, minimum 40dp touch targets, safe-area insets and cutout respected, search for the biggest non-overlapping layout, otherwise drops LT/RT, then right stick, then LB/RB. Checked at 320, 480 and 640 dpi, portrait and landscape (`controller-dpi-*.png`). HIDE/SHOW button collapses it.
- XInput: `xinput_probe` sees pad0 through the SDL virtual pad; overlay A gives 0x1000, Y 0x8000, left stick axis values arrive. A physical Android pad uses the same path (not tested, no pad on hand).
- FPS vs Termux:X11: not measured in this session (Termux:X11 path unchanged and kept as fallback).
