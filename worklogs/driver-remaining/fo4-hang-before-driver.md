# Fallout 4 hangs before loading the driver (beta.15 attempt, 2026-10-05)

## Root cause (beta.16 attempt, 2026-10-05 evening): two Wine bugs, not the driver

Both are in the bundled Proton 11.0-2-arm64ec. Neither involves DRM. The game folder was not modified.

1. **Hang: nsiproxy deadlock.** `Fallout4.exe` (via `steam_api64.dll`/`Galaxy64.dll`) calls
   `GetAdaptersAddresses`. This sends `IOCTL 0x121000` (NSI enumerate-all) to `\\.\Nsi` in `winedevice.exe`.
   On Android the nsiproxy netlink `bind` fails (`err:nsi:poll_events bind failed, errno 13`). After that, an
   nsiproxy thread sits in `poll()` on a pipe forever. It holds the interface-list mutex while it waits.
   `debuggerd -b` on `winedevice.exe` shows the ioctl thread blocked in `MutexLockWithTimeout` inside `nsiproxy.so`,
   and the other thread blocked in `poll` inside `nsiproxy.so`. The game's main thread waits for the ioctl
   forever (`pipe_read`, 0% CPU).
   - Repro without the game: `C:\windows\system32\ipconfig.exe` hangs the same way.
   - Workaround (shortcut env only, no prefix change): keep the launcher's default overrides and add
     `nsiproxy.sys=d`, i.e.
     `WINEDLLOVERRIDES=mscoree,mshtml=d;d3d8,d3d9,d3d10core,d3d11,dxgi=n,b;nsiproxy.sys=d`.
     With it, `ipconfig.exe` exits at once and FO4 gets past the hang.
2. **Crash: `OpenSCManagerW` from any x86_64 (ARM64EC) process.** Galaxy64 opens the service manager on a
   worker thread. The `svcctl` RPC reply unmarshal writes to address `0x10` in `rpcrt4` (`rpcrt4+0x64aac`,
   `str xzr, [x19]`), so the process dies (`exit=5`) about 15 s after launch, before any D3D11 device exists.
   - A minimal x86_64 test exe that only calls `OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT)` crashes the
     same way. Source: `/var/tmp/panvk/fo4/sctest/t.c`, built with clang + lld-link.
   - Fixing it needs a fixed Wine/Proton build (ARM64EC `rpcrt4`). Only one Proton (11.0-2-arm64ec) is
     installed. Per the rules this stops here: it needs user-supplied files.

Evidence: `/var/tmp/panvk/fo4/out/g1..g4` (WINEDEBUG logs), `/var/tmp/panvk/fo4/wdev-bt.txt` (winedevice backtrace),
`/var/tmp/panvk/fo4/nsitest*.log`, `/var/tmp/panvk/fo4/sctest/run1.log`.

Device: Poco X6 Pro (Mali-G615 MC6), PanPlay debug build, Proton 11.0-2-arm64ec, game arch x86_64.
Game: GOG Fallout 4 GOTY at `/storage/emulated/0/Download/1DM/Others/Fallout 4 GOTY/Fallout 4 GOTY`
(Wine `D:\1DM\Others\Fallout 4 GOTY\Fallout 4 GOTY`; `d:` maps to `/storage/emulated/0/Download`).

## What was done

1. Imported the game's `Fallout-4-registry-www.regfiles.net.reg` into the PanPlay prefix
   (`files/container/.wine`): copied to `C:\fo4.reg`, ran `C:\windows\regedit.exe /S C:\fo4.reg`
   through a launcher shortcut, verified with `reg.exe query "HKLM\SOFTWARE\Wow6432Node\Bethesda Softworks\Fallout4"`.
2. The .reg file only has a placeholder: `"InstalledPath"="Install directory"`. Set the real path with
   `reg.exe add ... /v "Installed Path" /d "D:\1DM\Others\Fallout 4 GOTY\Fallout 4 GOTY\" /f`
   and the same for `InstalledPath`. Both values are now present in the prefix.
3. Launched `Fallout4Launcher.exe` and `Fallout4.exe` (shortcut, `DXVK_HUD=full`, imported beta.15 rc4).

## Result

- Both exes start and then hang: black screen, process in `pipe_read`, 2 threads, 0% CPU, ~238 MB RSS.
- No `Fallout4_d3d11.log`/`Fallout4_dxgi.log`; `libvulkan_panfrost.so` is never mapped by the game process.
- Same black screen on bundled beta.14 (earlier FO4 agent run), so this is not a driver regression.
- `WINEDEBUG=+loaddll,+seh,err`: after loading and unloading a few system DLLs (netutils, rsaenh), the last
  activity is `CreateToolhelp32Snapshot` (heap-list snapshot unimplemented) and then a long run of
  `RtlInitializeExtendedContext2` SEH contexts on the main thread. This looks like an anti-tamper/DRM or
  emulation self-check (exception-driven), not graphics init.

## Next attempt

- Run with `WINEDEBUG=+seh,+relay` limited to the main thread, or under FEX with a different TSO/SMC mode,
  to see which exception loops. Try `steam_api64.dll`/`Galaxy64.dll` stubs (GOG build ships both).
- Try the x86_64 build with a full (non-arm64ec) Proton if available.
- Note: once wine exits, harness Enter presses land in PanPlay's "Session logs" screen (Send to cloud dialog).
  `/var/tmp/panvk/fo4/fo4.py` now presses Enter only while a Fallout4 process runs. No upload record was
  created (checked /admin/uploads: latest row 09:01Z, runs were ~10:34Z).

## Evidence

- `/var/tmp/panvk/fo4/out/f4r4a`, `f4r4b`, `f4r4c` (Fallout4.exe, rc4), `f4dbg` (WINEDEBUG), `f4l1` (launcher),
  `b14bundled` (beta.14). Wine run logs: PanPlay `files/logs/run-20261005-16*.log`, sessions under `files/sessions/`.
- Harness: `/var/tmp/panvk/fo4/fo4.py` (`FO4_EXE=Fallout4.exe` to start the game exe directly).
