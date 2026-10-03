# Game shortcuts

Games tab in the launcher = library of Windows games. Each game is one JSON file
`/data/data/dev.zenithblue.panvklauncher/files/shortcuts/<id>.json` (+ `<id>.png` icon).

## UI

- Games tab (bottom bar) -> **Add game**: Browse (SAF picker, file is resolved/copied by the
  existing `ContainerManager.importUri`) or type a device path / `C:\...` path, name, args,
  env (`KEY=VALUE` per line, default `DXVK_HUD=full`), arch (Auto = detected from PE header;
  i386 / x86_64 / arm64ec), resolution (default or 800x600..1920x1080), driver (default or
  any installed driver).
- Tap card = launch through the normal Wine launch path (`runWineExe` -> `ContainerManager.runExe`).
- Long-press (or the ⋮ button) = Edit / Duplicate / Delete (delete removes the shortcut only).
- Icon = first icon resource of the exe (PNG or 1/4/8/24/32-bit DIB). Fallback = letter tile.

## JSON fields

```json
{"id":"g1a2b3c4d","name":"dxcube","exe":"/data/user/0/dev.zenithblue.panvklauncher/files/games/dxcube.exe",
 "args":"","env":{"DXVK_HUD":"full"},"arch":"x86_64","resolution":"","driver":"",
 "icon":"auto","created":0,"lastPlayed":0}
```

`exe`: Android path or `C:\path` (mapped to the container `drive_c`). `arch`: `auto|i386|x86_64|arm64ec`
(informational, shown in UI; the runtime picks WOW64/FEX/ARM64EC from the PE itself).
`icon`: `auto` = app extracts on next Games-tab open, `none` = no icon, else file name.
`resolution`: `""` = leave the launcher display resolution alone, else sets the display
resolution pref before launch (global pref, persists). `driver`: `""` = selected driver, else `Driver.id`.
Per-game `env` is applied last (wins over launcher defaults, except `DISPLAY`).

## Intent (debug builds only)

```
adb -s SERIAL shell am start -n dev.zenithblue.panvklauncher/.MainActivity \
    --es dev.zenithblue.panvklauncher.LAUNCH_SHORTCUT <id-or-name>
```

`MainActivity` is exported (it is the launcher entry), so the extra is ignored unless the app is
debuggable. Waits for container setup; refuses (logs) if Wine is already running.

## Host script `tools/launch-shortcut.py`

Serial: `-s SERIAL` or `$ADB_SERIAL` (default `192.168.1.34:40501`). Uses `run-as`, so debug APK.

```
tools/launch-shortcut.py --list
tools/launch-shortcut.py --add /data/user/0/dev.zenithblue.panvklauncher/files/games/dxcube.exe \
        --name dxcube [--args "..."] [--env DXCUBE_SECS=30 --env DXVK_HUD=full] [--res 1280x720] [--driver ID]
tools/launch-shortcut.py --launch dxcube --wait 25 --screenshot out.png --logs out-logs/
tools/launch-shortcut.py --launch dxcube --force-stop   # kill a running game/app first
tools/launch-shortcut.py --delete dxcube
```

`--add` detects arch from the PE header (re-adding the same name updates it). `--env` given
replaces the default `DXVK_HUD=full`. `--logs DIR` pulls the newest `files/logs/run-*.log`
(Wine + DXVK stderr), DXVK `<exe>_d3d*.log` from the exe dir, and the launcher logcat tag.
Screenshot = `adb exec-out screencap -p` of whatever is on screen.
