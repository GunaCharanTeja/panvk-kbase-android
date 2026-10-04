# PanVK Kbase `g615-v11-csf-v0.1.0-beta.13` (prerelease, beta tier)

**Universal v10-v14 build. Only v11 (Mali-G615) is tested. v10/v12/v13/v14 are built but UNTESTED. Testers needed.**

Mesa `5a07217f034b` + series up to 100 (`patchSeriesId sha256:b85576a0bebec3b56c9de083a6afd2ff8c26feafece2a844079b7bca254e3d9c`).
Tested only on Poco X6 Pro, Mali-G615 MC6 (v11), mali_kbase CSF UAPI 1.21. Android minApi 35.

## Changes since beta.12

- **Universal ICD for v10-v14.** Both Android and glibc `.so` files contain
  `libpanvk_v10` through `libpanvk_v14`. The kbase device path now admits v14
  without `PAN_I_WANT_A_BROKEN_VULKAN_DRIVER` (004). v13 was already admitted.
- **Release name in driverInfo (new 099).** The full string is
  `PanVK-kbase beta.13 (Mesa 26.3.0-devel (git-5a07217f03))`.
  It is visible in the DXVK HUD Version line and PanProbe.
- **PanProbe GS regression fixed (new 100).** `gs_viewport_depth` case A
  failed since beta.11. All three cases passed on beta.9/beta.10.
  On-device bisect: reverting 098 did not help; reverting 097 fixed it.
  Dropping only `PAN_KMOD_BO_FLAG_ALLOC_ON_FAULT` from the `gpu_prerast`
  arenas fixed it in 6/6 runs. GPU-fault growth on MediaTek kbase lost the
  first `gpu_prerast` (GS) draw's output; nothing was drawn.
  Patch 100 commits the arenas up front on kbase. The shared-TLS fix from
  097 stays. Arenas cost 160 MiB per VkDevice again. Upgrade path:
  `KBASE_IOCTL_MEM_COMMIT` on first use.
- **kbase CS work-register fallback.** Implausible firmware values now fall
  back to 128 on v12+, or 96 on v10/v11. 96 was too small for v12+ streams,
  which use registers up to 123. G615 reports a plausible value; no warning.

## Which GPUs are recognised

| Arch | Recognised by the Mesa model table | Needs your gpu_id |
|---|---|---|
| v10 | G610, G310 | G710, G510 |
| v11 | **G615 (tested)**, G715 | — |
| v12 | G720 (variant 4) | G620, Immortalis-G720 |
| v13 | G725 (variant 4) | G625, Immortalis-G925 |
| v14 | G1-Ultra (14.8.0 v4), G1-Premium (14.8.1 v4), G1-Pro (14.8.3 v1/v4) | — |

## Call for testers

Testers are needed for v10/v12/v13/v14. Open PanProbe (the panvk-test APK)
once first. Then run these commands from a PC with USB debugging on:

```sh
adb shell 'getprop ro.product.model; getprop ro.soc.model; cat /sys/class/misc/mali0/device/gpuinfo'
adb logcat -d | grep -E 'Unknown gpu_id|kbase: '
```

The `gpuinfo` line gives the GPU_ID. The `Unknown gpu_id (...) or variant (...)`
line gives the variant that the Mesa model table needs.
Send results and PanPlay session log ZIPs to the
[Telegram testers group](https://t.me/+E-NhUATmkqE5ODg1), or an issue.

## Validation (G615 only)

- PanProbe: all 17/17 tests pass, three runs in a row. `gs_viewport_depth`
  A/B/C, `vs_viewport_index` and `depth_bounds` pass.
- PanProbe info: Mali-G615 MC6, Vulkan 1.4.363, driver `PanVK-kbase beta.13`,
  188 extensions, GPU id `0xB8A31030` (v11).
- CTS `memory.mapping.*`, `memory.map_placed.*`, `synchronization{,2}.basic.*`:
  4520 pass / 0 fail / 13 NotSupported, same as beta.12.
- CTS geometry + clipping + viewport subsets: geometry 195 pass / 0 fail / 4 NotSupported; pipeline and dynamic-state viewport cases 177 pass / 0 fail; clipping 180 pass / 128 fail; draw `shader_viewport_index` 328 pass / 60 fail / 6 NotSupported. All 188 failures (user clip/cull distances through GS or tessellation, and `shader_viewport_index.fragment_shader_2..16`) fail the same way on the beta.10 and beta.12 binaries, so they are old bugs, not regressions.
- PanPlay game tests skipped this time. The user tests PanPlay.

## Known issues

- v10/v12/v13/v14 are untested. We have no hardware for these arches.
- Old kbase may lack `GET_CPU_GPU_TIMEINFO`, so timestamp queries read 0.
- 4 KiB pages are assumed. 16 KiB-page kernels are unsupported.
- Firmware interface differences are not validated per arch.
- Queue-group create tries the 112-byte layout on uAPI >= 1.25, then the
  1.6 32-byte layout. The 40-byte 1.18 layout is not tried.
- Tiler heap init uses the legacy 16-byte layout, not the 24-byte layout.
- Queue submission on kbase is synchronous.

## Install

Full instructions: [`docs/RUN-PC-GAMES-ON-MALI.md`](https://github.com/zenithblue-oss/panvk-kbase-android/blob/main/docs/RUN-PC-GAMES-ON-MALI.md).

- **Android:** use `PanVK-Kbase-Android-g615-v11-csf-v0.1.0-beta.13-5a07217f.adpkg.zip` with apps that load drivers (minApi 35).
- **Linux/glibc (emulators, chroots):** use `PanVK-Kbase-g615-v11-csf-v0.1.0-beta.13-5a07217f-EMULATOR.zip`.
- **PanProbe:** `panvk-test-g615-v11-csf-v0.1.0-beta.13.apk` bundles this driver (stripped by the APK build).
- Verify downloads with `SHA256SUMS`.
