# Gate 090 device verification

Device: Mali-G615 MC6, panvk-test APK, `adb -s 192.168.1.34:40501`. Source: `device/vs-viewport-index.{c,vert,tesc,tese,frag}` (plus `-inst.vert`, `-pt.vert`), APK test `vs_viewport_index`.

## Test

32x32 target. vp0 = left half, depth [0, 0.25]. vp1 = right half, [0.5, 1]. The VS (or TES) writes ViewportIndex. Cases: A VS clamp, B clamp+clip, C no clamp, D instanced VS, E TES clamp, F TES no clamp. Expected depth A/D/E 0.25 / 0.5, B/C/F 0.125 / 0.75.

## Baseline (089 series)

FAIL. `shaderOutputViewportIndex` reported 0 and pipeline creation fails (r=-13).

## Driver with 090

- vs_viewport_index: 6/6 runs, all six cases exact.
- gs_viewport_depth: 2/2 pass (no regression).
- `--es autorun all`: first run 12/13, only swapchain_lifecycle failed; second run 13/13.
- swapchain_lifecycle repeats: 3 runs gave PASS, PASS, FAIL. This is the known unreproduced intermittent from 089 (it uses no viewport shader). Not caused by 090, cause still unknown.
- During bring-up the first device run hung the GPU. Cause: no gpu_prerast arena when only shaderOutputViewportIndex is enabled. Fixed.

## Host and build

- Host libpan test (`tmp/vp-runs`): `vp_runs ok`, includes an instanced case.
- Full csf-v11 series applies cleanly on Mesa 5a07217f034b.
- Android ninja build of libvulkan_panfrost.so clean, no warnings.

## Not covered

- No CTS. dEQP-VK dynamic_state / draw shader_viewport_index not run (skipped, time).
- PrimitiveID across runs; v12+.
