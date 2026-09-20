# DXVK Native early probe

`dxvk_native_probe.cpp` is the compile-time starting point for DX3 and later
runtime work. It creates an SDL2 Vulkan window, then requests either a D3D9
device or D3D11 feature levels 11.1, 11.0, and 10.1 in that order.

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

Do not treat compilation as runtime proof. DX3 must set `DXVK_WSI_DRIVER=SDL2`
and force the recorded PanVK glibc ICD before running this executable.
