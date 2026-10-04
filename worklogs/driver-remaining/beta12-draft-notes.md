# beta.12 draft release notes (NOT RELEASED)

Status: released as `g615-v11-csf-v0.1.0-beta.12` (prerelease) on 2026-10-04 after the v11 gate below.

> **Warning: untested on v10 and v12. Testers needed.**
> This build is proven only on the Mali-G615 (v11, Poco X6 Pro). The v10 and v12 code paths compile and are linked into the same `libvulkan_panfrost.so`, but nobody has run them on v10 or v12 hardware with kbase. Expect failures. Please send `logcat -s MESA` output, the GPU name/ID line, and the kbase uAPI version.

## One ICD for v10, v11 and v12

- The Android ICD already built the per-arch backends `libpanvk_v10`, `v11`, `v12` (and v6/v7/v13/v14) into one `.so`. The kbase physical-device path admits v10-v13 without `PAN_I_WANT_A_BROKEN_VULKAN_DRIVER`. There was no G615-only GPU-ID gate in the driver.
- Patch audit: the arch guards in the series are `PAN_ARCH >= 10`, or explicit `< 12` / `>= 12`. On v12 the viewport-run, depthBounds and VS/TES viewportIndex features stay off, as before. The only `== 11` guard is the INTERSECT ZS preload (061), which is v11 only by design. The kbase ring wrapper has a v10 path (`cs_set_scoreboard_entry`) and a v11+ path (SB_SEL state), the same split as upstream.
- New fix (kbase): the CS work-register count from the firmware is used only when it is 96 or 128. Any other value falls back to 96 and logs `kbase: implausible CS work register count`. A Pixel 7 (G710, kbase 1.38) is known to report a bad value. Before this fix, 256 would wrap to 0 in a `uint8_t`. G615 must not log this warning (to be checked on the device).

## Which GPUs can work (by the Mesa model table, gpu_id + variant)

| Arch | Matches the table | Fails with "Unknown gpu_id" (needs a tester's gpu_id/variant) |
|---|---|---|
| v10 | Mali-G610 (variant 0), G310 v1-v5 | G710 (Dimensity 9000, Tensor G2), G510 |
| v11 | G615, G715 | Immortalis-G715 if its variant differs |
| v12 | G720 (variant 4 only) | G620, Immortalis-G720 / G720 parts with another variant |

Immortalis-G925 is v13, not v12.

### Call for testers: report your GPU ID

If you have a G710, G510, G620, Immortalis-G720 or any other v10/v12 Mali phone, run this from a PC with USB debugging on and paste the output in an issue:

```sh
adb shell 'getprop ro.product.model; getprop ro.soc.model; cat /sys/class/misc/mali0/device/gpuinfo'
adb logcat -d | grep -E 'Unknown gpu_id|kbase: '
```

Run the second line after opening PanProbe (panvk-test APK) once. The `gpuinfo` line gives the GPU_ID (for example `Mali-G615 6 cores r1p3 0xb8a31030`). The `Unknown gpu_id (...) or variant (...)` log line gives the variant that the Mesa model table needs. G725 (v13) is in the table but out of scope.

## Known gaps and risks (v10/v12)

- No hardware: v10 and v12 are compile-only.
- Old kbase (CSF uAPI < ~1.13, typical of r32-r38 v10 phones): `GET_CPU_GPU_TIMEINFO` may be missing, so timestamp queries read 0.
- uAPI >= 1.25 uses the 112-byte group-create struct, and falls back to the 1.6 ioctl if the kernel rejects it.
- The backend assumes 4 KiB pages. 16 KiB-page kernels are unsupported.
- Firmware differences (stream counts, GLB version) are logged but not validated per arch.
- Async queue submission is still TODO (see `worklogs/g615-dxvk/PROGRESS.md` item 11).

## README change on release

In the README architecture table (rows `v10` and `v12`), change the status from TODO (📋 / ❔) to **"built, untested"** for G610/G310 (v10) and G720 (v12). Keep ❔ for G710/G510/G620/Immortalis-G720 until a tester's gpu_id is in the model table.

## v11 regression gate (must pass before release)

CTS memory + synchronization subsets, 32-bit cube, MiSide, NFS:MW to a race. Result: passed, see `validation/driver-remaining/beta12-device/gate-summary.txt`. NFS:MW was not part of the final gate (device busy).

Build: `/var/tmp/panvk/uni/dist/libvulkan_panfrost.so` (pinned Mesa 5a07217f + full series + the kbase register-count fix).
