# 096 Plan restart strip chunks with the whole workgroup

Patch: `patches/csf-v11/096-plan-restart-strip-chunks-with-the-whole-workgroup.patch`. 086 stays unused.

## Bug (the 094 restart-strip flake)

`restart_direct` / `restart_indirect` in APK `large_draw` lost the device in a few percent of runs. Instrumented runs (CS stores of per-iteration counters into a debug block, dumped from the kbase timeout path) showed the same state in every failure:

- compute loop: chunk N planned, lowered and published (`READY` given N times), then waiting on `FREE`;
- vertex/tiler loop: N-1 chunks drawn, blocked in the `SYNC_WAIT` on `READY`;
- `READY` = 1 and `FREE` = 0 in memory;
- kbase ended the vertex/tiler group with `CS_FATAL 0x41 CS_UNRECOVERABLE` (GPU soft reset in dmesg) or a `0x72` group fatal.

The chunk data in the dumps was correct and complete. The planning math, restart handling, offsets and the 2048 instance cap were not involved.

## Root cause

`panlib_chunk_step` ran on one invocation (`KERNEL(1)`) and scanned the whole chunk window, up to 65536 indices, for restart indices. On restart draws every planner job took long; a full restart run took about 2.8 s. During that job the vertex/tiler subqueue sits in a cross-CSG `SYNC_WAIT` inside the render pass (each panvk subqueue is its own kbase CSG). With long compute jobs in that position the waiting group is lost or killed by kbase/firmware.

Causality check: the `strip_indirect` chunk loop never failed (no restart scan). With a test-only busy loop added to the planner kernel it failed (`CSF group 0 fatal error: status 0x00000072`, 1/1), and passed without it on the same build.

## Fix

The restart scan runs on a 256-invocation workgroup. Each invocation scans one segment of the window and stores its first-strip length, closed-strip primitives, last restart index and trailing run in local memory. Every invocation then combines the 256 segment results in order. Invocation 0 plans the chunk as before with the combined result. The launch stays `panlib_1d(1)` (one workgroup). The CS `SYNC_WAIT` handshake is unchanged.

A restart run now takes about 0.36 s.

Rejected: polling the sync words with CS loads instead of `SYNC_WAIT`. It still failed (2 in about 80 runs) and it hung the tessellation loops every time.

## Evidence (chroot ICD, `LARGE_DRAW_ONLY=restart`, both restart cases per run)

| driver | runs | failed |
|---|---|---|
| 095 (serial planner) | 27 + 100 | 2 + 4 |
| parallel planner (dev build) | 250 | 0 |
| 096 final | 250 (65 s total) | 0 |

096 final, other runs:

- chroot full `large_draw` (all 14 cases): 20/20 pass.
- APK `large_draw`: 12/12 consecutive pass.
- APK `autorun all`: 17/17 on 2 runs. UI Run all (app started normally): 17/17, screenshots `validation/driver-remaining/096-device/app-tests-runall-{1..5}.png`.
- CTS, chroot ICD of 093-096, `tmp/g615-gap/cts-all.txt` (36144 cases: draw, transform_feedback, geometry, tessellation, conditional_rendering, multisample, dynamic_state, query_pool and more): 17067 pass, 0 fail, 19077 NotSupported, 0 DeviceLost. Same as the 092 baseline.
- 093 discrimination test `tess_cond_state`: see `093-tess-state-emission.md`.

## Limits

- The underlying kbase/firmware behaviour (a long compute job while the vertex/tiler CSG waits across CSGs mid-render-pass) is not fixed. Other long single-workgroup jobs in that position could hit it. The 2048-instance cap of 094 (8192+ instances hung) is likely the same mechanism; the cap stays.
- The serial part (invocation 0) still walks restart indices one by one only to skip leading restarts of a window.
