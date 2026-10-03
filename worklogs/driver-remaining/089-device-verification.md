# Gate 089 device verification

Device: Mali-G615 MC6, panvk-test APK, `adb -s 192.168.1.34:40501`. Source: `device/gs-viewport-depth.{vert,geom,frag,c}`, APK test `gs_viewport_depth`.

## Test

32x32 target. vp0 = left half, depth range [0, 0.25]. vp1 = right half, [0.5, 1]. The GS selects the viewport. Case A: depth clamp on, clip off. Case B: clamp and clip on. Case C: clamp off. Expected depth: A 0.25 / 0.5; B and C 0.125 / 0.75 (left / right).

## Baseline driver (series without 089, `build/android-vpsa2-final`)

RESULT FAIL. A: depthL 0.375 (want 0.25), depthR 0.0 (want 0.5). B and C: depthL 0.375 (want 0.125), depthR 0.25 (want 0.75). 1024 bad pixels in each case. Cause: clamp and clip used the union of all viewport depth ranges (bug 076).

## Driver with 089 (`tmp/build-089-series`)

ICD sha256 `9eba1118...a450fb`. A, B, C pass with exact values 0.25 / 0.5 and 0.125 / 0.75.

- Repeats: gs_viewport_depth 6/6, swapchain_lifecycle 6/6 (`tmp/rep-089.log`).
- First run after a fresh install: 4/4 pass (`tmp/first-089.log`).
- Full APK autorun: 11/12 on one run. On the very first run after install, case B once drew nothing (depth 1.0, all color bad), and swapchain_lifecycle failed once in the same "all" run. Neither reproduced in 11 later gs runs and 6 swapchain runs. **Unreproduced intermittent failure**, cause unknown.

## Work-tree driver (`tmp/build-089`, also has the uncommitted `kbase_kmod.c` change)

Full autorun 12/12. gs and swapchain repeats 6/6 each.

## Host and build

- Host libpan test (`tmp/vp-runs`): `vp_runs ok`.
- Full csf-v11 series applies cleanly on Mesa 5a07217f034b.
- Android ninja build: 1134/1134.

## Not covered

- No CTS (same as 088).
- Known pre-existing issue, not caused by 089: in `panvk_vX_cmd_draw.c` around the `cs_if(pred)` tessellation conditional, a false predicate skips prepare_draw's GPU state writes but still clears their dirty flags. From the 087 series; sol flagged it in review. Follow-up.

Log: `validation/driver-remaining/089-device/run.log`.
