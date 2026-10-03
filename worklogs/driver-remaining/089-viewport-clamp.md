# 089 viewport clamp

Status: **verified on the device** (Mali-G615 MC6, panvk-test APK). Not run under CTS. Details: `089-device-verification.md`.

Patch: `patches/csf-v11/089-draw-gs-viewport-selections-as-ordered-runs.patch`. It replaces the FS-clipping / GPU geometric clipper emulation of GS viewport-index selection with GPU-built ordered viewport runs. Each run is drawn in the order the GS emitted it with that viewport's own depth clamp and clip range. This fixes bug 076 (clamp and clip used the union of all viewports' depth ranges).

The earlier attempt (`089-split-geometry-draws-per-selected-viewport.patch`, FS clip-distance kill packed into the scissor varying) was removed as unsafe. This patch supersedes it.

## Result

Test `device/gs-viewport-depth.c`: 32x32, vp0 = left half, depth [0, 0.25], vp1 = right half, [0.5, 1]; the GS picks the viewport.

| case | baseline (no 089) | with 089 |
|---|---|---|
| A clamp on, clip off | depthL 0.375 (want 0.25), depthR 0.0 (want 0.5) | pass, 0.25 / 0.5 |
| B clamp and clip on | depthL 0.375 (want 0.125), depthR 0.25 (want 0.75) | pass, 0.125 / 0.75 |
| C clamp off | same wrong values as B | pass, 0.125 / 0.75 |

Baseline: RESULT FAIL, 1024 bad pixels per case.

Other checks: host libpan test `vp_runs ok`; full csf-v11 series applies cleanly on Mesa 5a07217f034b; Android ninja build 1134/1134.

## Caveats

- One unreproduced intermittent failure on the first run after a fresh install: case B drew nothing (depth 1.0, all color bad), and swapchain_lifecycle failed once in the same "all" run. Neither reproduced in 11 later gs runs and 6 swapchain runs.
- Pre-existing, not from 089: in `panvk_vX_cmd_draw.c` around the `cs_if(pred)` tessellation conditional, a false predicate skips prepare_draw's GPU state writes but still clears their dirty flags (from 087; sol flagged it). Follow-up.
- No CTS.
- `device/copy.comp` is an unrelated untracked shader, not part of 089.
