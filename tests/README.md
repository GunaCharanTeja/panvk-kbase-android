# Tests

- `kbase-probe/` — Gate B input (P2). C probe, NDK-built, run via adb shell
  AND inside loader-test APK (app UID). Records ROOT/SHELL + TARGET APP UID
  results separately.
- `vulkan-smoke/` — Gates C+D: dlopen, vk_icdGetInstanceProcAddr,
  vkCreateInstance (exact PanVK device, never lavapipe/vendor-accidental),
  vkCreateDevice, queue, mapped buffer, destroy.
- `compute/` — Gate E: 1x1x1 dispatch writing 0x12345678, 10/10 exact.
- `offscreen/` — Gate F: partial-triangle RGBA8 readback (no clear/blit false
  positive), then sampling/depth/stencil/blend/MSAA/MRT/indirect per arch.
- `ahb/` — Gate G: real AHB allocate -> properties -> import -> bind -> GPU
  write -> CPU verify -> release.
- `android-surface/` — Gate H: real ANativeWindow swapchain, >=300 frames,
  resize/recreate, clean shutdown, second launch.
- `sync/` — fence, binary + timeline semaphores, multi-submit, queue idle,
  acquire/present sync, 300-frame soak, second run.
- `android-loader-app/` — tiny APK proving the release ZIP hot-loads from an
  ordinary app UID (generic dlopen path + AdrenoTools-compat path).
- `runtime-features/` — P10/P11: on-device ICD dump → runtime-feature-matrix.json
  with upstreamImplemented / runtimeExposed / runtimeTested. Parser tests here.
- `features/` — focused advertised-feature workloads. `sync-features.c` covers
  timeline semaphore values/chains and synchronization2 submission/barriers.
- `bcn/` — complete BC1-BC7 compatibility contract and evidence self-check.
  Advertisement is rejected unless every required format, operation, edge case,
  and stress case has `PASS` evidence.
- `dxvk/` — DXVK-only profile, Vulkan semantic, and DXVK Native validation.
  Kept separate from historical combined `dxvk-vkd3d/` evidence.

Fault safety for all: hard timeout, small allocations, tiny images, single
queue first, bounded frames, one change per run.
