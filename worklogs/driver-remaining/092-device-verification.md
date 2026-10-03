# Gate 092 device verification

Device: Mali-G615 MC6 (v11), panvk-test APK, `adb -s 192.168.1.34:40501`. Source: `device/depth-bounds.{c,vert,frag}`, APK test `depth_bounds`. Log: `validation/driver-remaining/092-device/run.log`.

## Test

32x32, D32_SFLOAT. Pass 1 stores a depth gradient d(x) = (x + 0.5) / 32. Pass 2 draws with depth test ALWAYS and the depth bounds test on; a pixel must be green only where the stored depth is inside the bounds. Cases:

- A static [0.3, 0.6]; B dynamic (`vkCmdSetDepthBounds`); C dynamic [0.1, 0.2]
- D `vkCmdSetDepthBoundsTestEnable` off (all green); E enable on
- G no fragment shader, depth writes on, viewport depth [0.9, 0.9]: depth becomes 0.9 only inside the bounds
- F 4x MSAA resolved: each pixel's green = 255 * (in-bounds samples) / 4. 64 pixels straddle a bound, so testing one sample per pixel fails.

## Baseline (090 series)

FAIL: `depthBounds` reported 0.

## Driver with 092

- depth_bounds: 6/6 runs, all 7 cases pass (42/42).
- `--es autorun all`: 14/14 pass.
- From the app UI (no autorun): Run all 14/14 PASS, screenshots `app-tests-runall-{1..4}.png`. The Info view lists the features in its existing feature list: `app-info-core-{1,2}.png` (depthBounds, depthClamp, multiViewport), `app-info-vk12.png` (shaderOutputViewportIndex, shaderOutputLayer), `app-info-depthclip.png` (depthClipEnable). All shown as supported. The per-viewport depth clamp from 089 has no feature bit; the gs_viewport_depth test covers it.
- swapchain_lifecycle: failed twice in earlier `autorun all` runs on the 092 build (once while the device was shared with a launcher session), then passed 3/3 alone, in the final autorun all and in the UI run. This is the known intermittent from 089/090; it uses no depth bounds.
- Bring-up: the first version looped over each lane's own SampleMaskIn bits and failed case F on 3 pixels split between two triangles. A uniform sample index fixed it.

## CTS

Chroot Linux build of the same tree (`/tmp/d92`, ICD `/tmp/d92/icd.json`), `scripts/dxvk/cts-resume.sh`. 2059 cases with `depth_bound` in the name (`cts-cases.txt`): all dynamic_state ds_state depth bounds (monolithic, libraries, shader objects), pipeline extended_dynamic_state, and pipeline depth compare_ops `*_depth_bounds_test` for d16/d24s8/d32 (monolithic, color and nocolor), d32 for pipeline_library and shader_object_unlinked_spirv.

- First run: 1090 pass, 684 fail. All failures were `depth.nocolor` (pipelines without an FS). Fixed with the device-owned depth-bounds FS.
- Final run: 1774 pass, 0 fail, 285 NotSupported, 0 crash (`cts-summary.txt`).

## Host and build

- Full csf-v11 series applies cleanly on Mesa 5a07217f034b (variant B, 91 patches; the files match the built tree). 092 also applies on top of the other session's 091.
- Android ninja build of libvulkan_panfrost.so: no warnings in touched files.

## Not covered

- Other depth formats under the device test (CTS covers d16/d24s8).
- EarlyFragmentTests with depth writes (documented limit); v12+.
