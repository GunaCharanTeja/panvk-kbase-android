# DXVK Native early probe

`dxvk_native_probe.cpp` creates an SDL2 Vulkan window, then requests either a
D3D9 device or D3D11 feature levels 11.1, 11.0, and 10.1 in that order. The
`d3d9-workload` mode draws a fixed-function triangle and checks a GPU readback;
`d3d9-present` also presents the frame. `d3d11-workload` clears a render target,
copies it to staging memory, and checks a GPU readback. `d3d11-headless` skips window creation but
still initializes SDL2 video for stock DXVK's Vulkan extension provider.

Build only on the target AArch64 glibc environment against the pinned, stock
DXVK Native build:

```sh
c++ -std=c++17 dxvk_native_probe.cpp -o dxvk-native-probe \
  -I/root/panvk-dxvk-v3.1.1/include/native/directx \
  -I/root/panvk-dxvk-v3.1.1/include/native/windows \
  $(pkg-config --cflags --libs sdl2) \
  -L/root/panvk-dxvk-v3.1.1-build/src/d3d9 \
  -L/root/panvk-dxvk-v3.1.1-build/src/d3d11 \
  -Wl,-rpath,/root/panvk-dxvk-v3.1.1-build/src/d3d9 \
  -Wl,-rpath,/root/panvk-dxvk-v3.1.1-build/src/d3d11 \
  -ldxvk_d3d9 -ldxvk_d3d11
```

On the G615 Alpine chroot, start a virtual X display:

```sh
Xvfb :99 -screen 0 1280x720x24 -nolisten tcp -ac
```

Run the probe with `DISPLAY=:99`,
`SDL_VIDEODRIVER=x11`, `DXVK_WSI_DRIVER=SDL2`, and
`VK_DRIVER_FILES=/tmp/bp-icd.json` for the current PanVK glibc ICD. Run both
workload modes and record the exit codes, pixel checks, and DXVK feature level.
Xvfb supplies an SDL display; the workload readbacks verify GPU output.
