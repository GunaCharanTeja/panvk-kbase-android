# DXVK Native on the G615 (2026-09-29)

Device: duchamp, Mali-G615 MC6, ADB `192.168.1.61:41161`, Alpine glibc chroot
`/data/local/tmp/chrootAlpine`. Mesa `work/mesa` `dx6-dx7-base` at
`d33a343037f` built with `/tmp/bld.sh` (`NINJA_RC=0`). ICD SHA-256:
`95dd86770f9762283ae869b1d88eb6eac4879b566c7a7052c34bd3f9f4d0e42e`.

Stock DXVK Native v3.1.1 is pinned at
`b1a1c99ab52b687cf950d62c88bc2fa316b41663`. D3D9 library SHA-256:
`39e9ac927eaef1cae2e276142c3773a857115f779e61937f8dfd03424ce4cd5e`;
D3D11 library SHA-256:
`013b135ceccd5403ac6d02fe1ee2b9997c4945b69a7fc384faa1a45ed45416df`.
The source is `tests/dxvk/native/common/dxvk_native_probe.cpp`; the compiled
probe SHA-256 is
`14de6cac4de5f7165e9371bd2144c79a102a2b6294d7e48c50fd8af76cfd8e20`.

The chroot ran `Xvfb :99 -screen 0 1280x720x24 -nolisten tcp -ac`. Each run
used `DISPLAY=:99`, `SDL_VIDEODRIVER=x11`, `DXVK_WSI_DRIVER=SDL2`,
`VK_DRIVER_FILES=/tmp/bp-icd.json`, and `DXVK_LOG_LEVEL=warn`.

| DXVK Native mode | Result | GPU work checked |
|---|---|---|
| `d3d9` | Exit 0, `D3D9 HRESULT=0x00000000` | Device creation |
| `d3d11` | Exit 0, `D3D11 HRESULT=0x00000000 feature_level=0xa100` | Device creation, feature level 10_1 |
| `d3d9-workload` | Exit 0, `D3D9_WORKLOAD HRESULT=0x00000000 triangle_pixel=1` | Fixed-function triangle over a blue clear; `GetRenderTargetData` readback checks red center and blue corner |
| `d3d11-workload` | Exit 0, `D3D11_WORKLOAD HRESULT=0x00000000 red_pixel=1` | Render-target clear, `CopyResource` to staging texture, and mapped pixel check |

The pixel checks are host test oracles after GPU execution. The production draw,
clear, copy and shader work runs through DXVK and PanVK on the GPU.

The earlier `DX3-REPORT.md` run had no display session and crashed while the
SDL2 extension provider initialized. With Xvfb, stock DXVK reaches PanVK and
creates D3D9 and D3D11 devices. DXVK reports feature level 10_1 because PanVK
still exposes `tessellationShader=0`.

Presentation remains open. `d3d9-present` reported
`D3D9_WORKLOAD HRESULT=0x00000000 triangle_pixel=1`, then waited during device
destruction. A GDB backtrace placed the main thread in
`dxvk::Presenter::destroySwapchain()` and the frame thread in PanVK's
`x11_wait_for_present()`. The process was killed after the wait. This virtual
X11 result does not establish presentation on the physical Android display.
