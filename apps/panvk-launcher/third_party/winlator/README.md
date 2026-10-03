# Winlator X server (vendored, modified)

- Upstream: https://github.com/brunodev85/winlator
- Commit: `db6d7aa446b5607b48ac2ee36dc4333799ff3dc7` (2026-04-14, last commit that still carries the app source; later commits removed it)
- License: LGPL-2.1 (`LICENSE` in this directory, copied from upstream)

Copied (Java) into `app/src/main/java/com/winlator/`: `xserver/`, `xconnector/`, `math/`, `sysvshm/`,
`renderer/` (GLRenderer, Texture, shaders), `widget/XServerView`, `widget/TouchpadView`,
`core/{Callback,CursorLocker,StringUtils,ArrayUtils,UnitUtils,FileUtils(trimmed)}`.
Copied (C) into `app/src/main/cpp/winlator/`: `drawable.c`, `sysvshared_memory.c`, `xconnector_epoll.c`.
`res/drawable-nodpi/cursor.png` is upstream `drawable-hdpi/cursor.png`.

Modifications (marked `panvk-launcher` in the sources):
- removed DRI3 extension, GPUImage (AHardwareBuffer), XR hooks;
- `winhandler/WinHandler` is a no-op stub (upstream talks to a helper exe inside Wine);
- `Keyboard`: inline gamepad-source check instead of `ExternalController`;
- `GLRenderer`: rebuilds scene on surface creation, resource id points at our `R`;
- `CursorLocker`: daemon timer plus `stop()`;
- `TouchpadView`: screen size from `DisplayMetrics`.

LGPL note: these files stay LGPL-2.1. Source is in this repository, so recipients can modify and rebuild the APK.
