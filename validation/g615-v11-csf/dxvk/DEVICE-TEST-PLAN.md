# G615 device test plan (pending, device offline)

Date: 2026-09-30. Device: duchamp Mali-G615 MC6, kbase CSF v11, ADB
`192.168.1.61:41161`, chroot `/data/local/tmp/chrootAlpine`. Run in the order
below. Record each result in the doc named per step and in `evidence-ledger.json`.

## 0. Setup

- Build: `work/mesa` `dx6-dx7-base` @ `d33a343037f` (patches to `csf-v11/054`).
  Last ICD SHA-256 `95dd86770f9762283ae869b1d88eb6eac4879b566c7a7052c34bd3f9f4d0e42e`.
  Branch builds (tess, cube, feat, `jica98-adopt`) get their own ICD; record
  each SHA-256.
- ICD in chroot: `/tmp/build-glibc/src/panfrost/vulkan/libvulkan_panfrost.so`,
  manifest `/tmp/bp-icd.json` (`VK_ICD_FILENAMES` / `VK_DRIVER_FILES`).
- `adb connect 192.168.1.61:41161`; export `ANDROID_SERIAL=192.168.1.61:41161`.
- CTS binary: `/root/panvk-vk-gl-cts-build/external/vulkancts/modules/vulkan/deqp-vk`.
- CTS runner, used by every CTS step below (host side, repo):
  `scripts/dxvk/cts-resume.sh [-i ICD_JSON] [-e VAR=VAL]... (-l CASELIST | -r REGEX) OUTPUT_DIR`
  - Same device checks and adb/su/chroot path as `run-vk-cts-subset.sh`
    (duchamp, arm64, `/dev/mali0`, `ANDROID_SERIAL`, `CTS_CHROOT`, `CTS_BINARY`).
  - `-r` builds the caselist on the device from `deqp-vk --deqp-runmode=txt-caselist`
    (cached in `/root/panvk-vk-cts-run/all-cases.txt`; delete it after a CTS
    rebuild), then `grep -E REGEX`. `-l` pushes a host caselist instead.
  - `-i` sets `VK_ICD_FILENAMES`/`VK_DRIVER_FILES` (default `/tmp/bp-icd.json`).
    `-e` (repeatable) adds env vars for an A/B arm. Use one `OUTPUT_DIR` per arm;
    its basename is the device run dir under `/root/panvk-vk-cts-run/`.
  - Resumes after a deqp abort/crash/DeviceLost (the old `/tmp/cab.sh` loop):
    the aborted case is logged and skipped, the rest reruns.
  - Writes `OUTPUT_DIR/{summary.txt,summary.json,results.qpa,console.txt,aborted.txt,status.txt}`.
    `summary.txt` has Pass/Fail/NotSupported/Crash/Timeout/DeviceLost/... counts,
    `aborted`, `missing`, `kto` (`timeout on subqueue`/device lost lines), then
    the failing case names. Exit 0 only when nothing failed, aborted or went missing.
  - A/B example (step 1.1):
    `scripts/dxvk/cts-resume.sh -r '^dEQP-VK\.geometry\.' out/1.1a` and
    `scripts/dxvk/cts-resume.sh -e PANVK_DEBUG=no_gs -r '^dEQP-VK\.geometry\.' out/1.1b`.
  - The old device-only `/tmp/cab.sh` and `/tmp/ctsg.sh` (cited in older docs)
    are replaced by this script. `run-vk-cts-subset.sh` stays for no-env runs.
- Probes:
  - `tests/dxvk/vulkan/<name>/build_spv.sh`, then
    `gcc -O2 -Wall -o /tmp/<bin> <name>.c -ldl`, then run
    `/tmp/<bin> /tmp/build-glibc/src/panfrost/vulkan/libvulkan_panfrost.so`.
    Each probe prints `<NAME>_FAILS=N`.
- Run one GPU job at a time. A concurrent stats CTS run once lost the device.
- Pass criteria for every CTS step: 0 Fail, 0 Crash, 0 DeviceLost, 0 Timeout,
  `aborted=0`, `kto=0` in `summary.txt`. NotSupported is fine only
  where the table says so. An A/B arm must not regress any case against its base arm.
- Case counts below come from `work/VK-GL-CTS` mustpass `vk-default` and may
  differ from the device CTS build.

## 1. Prerast re-validation (GS default-on, `csf-v11/042-048`)

| # | Run | Env arms | Pass criteria |
|---|---|---|---|
| 1.1 | CTS `^dEQP-VK\.geometry\.` (199) | (a) none (b) `PANVK_DEBUG=no_gs` | (a) >= 182 Pass. Only the 6 `geometry.layered.cube*` fails are allowed (fixed by step 5); `geometry.instanced.*` needing invocations > 32 is NotSupported. (b) all NotSupported, 0 crash. |
| 1.2 | CTS `^dEQP-VK\.draw\.(renderpass\|dynamic_rendering)\..*indirect` (1316+2558) | none | 0 Fail. No CTS draw-indirect case binds a GS: GS+indirect coverage is `tri_list_indirect` in 1.5. The over-capacity indirect gap stays open. |
| 1.3 | CTS `^dEQP-VK\.rasterization\..*polygon` (21) | none | 0 Fail |
| 1.4 | CTS `^dEQP-VK\.pipeline\.monolithic\.extended_dynamic_state\..*polygon_mode` (32) | none | 0 Fail. Dynamic polygon mode goes through the 047 split. |
| 1.5 | Probe `tests/dxvk/vulkan/geometry` (`/tmp/gst`) | (a) none (b) `PANVK_DEBUG=gs` (c) `PANVK_DEBUG=no_gs` | (a)(b) 23/23, `GEOMETRY_FAILS=0`. (c) clean skip or report of `geometryShader=0`, no crash. |
| 1.6 | DX7 fill-mode probe `tests/dxvk/vulkan/fill-mode` | (a) none (b) `PANVK_DEBUG=gpu_prerast` | 17/17 pixel-exact, `FILL_MODE_FAILS=0`, both arms identical (`DX7-FILL-MODE.md`) |

Update: `DX7-GS.md`, `DX7-FILL-MODE.md`.

## 2. Pipeline statistics (`csf-v11/049-054`), in isolation

Run nothing else on the GPU while this runs.

| # | Run | Pass criteria |
|---|---|---|
| 2.1 | Probe `tests/dxvk/vulkan/pipeline_stats` (`/tmp/pst`) | 12 CASE PASS, `PIPELINE_STATS_FAILS=0`, exit 0 |
| 2.2 | CTS `^dEQP-VK\.query_pool\.statistics_query` (17,769) via `cts-resume.sh -r` | >= 14,098 Pass, 0 Fail, 3,671 NotSupported. This includes the 8 `reset_after_copy.compute_shader_invocations.*cmdcopyquerypoolresults*` cases (054). |

Update: `DX8-PIPELINE-STATS.md`.

## 3. jica98 adopt (`work/mesa-jica98`, branch `jica98-adopt`, 8 WIP commits)

Build its ICD first. Each A/B pair runs on the same ICD.

| # | Run | A/B | Pass criteria |
|---|---|---|---|
| 3.1 | Timestamps: CTS `^dEQP-VK\.pipeline\.monolithic\.timestamp\.` (262). Also check `timestampPeriod` != 0 in a feature dump. | none | 0 Fail. `timestampPeriod` = 1e9/cntfrq. |
| 3.2 | Semaphores: CTS `^dEQP-VK\.synchronization2?\.timeline_semaphore\.` (2880+2873) and `^dEQP-VK\.synchronization2?\.basic\.(binary\|timeline)_semaphore\.` (22) | (a) none (GPU waits, 0005) (b) `PANVK_KBASE_CPU_SEMAPHORE_WAITS=1` | 0 Fail in both arms, (a) no regression vs (b). Also check `synchronization*` smoke for hangs. |
| 3.3 | Robustness: CTS `^dEQP-VK\.robustness\.robustness2\..*storage_buffer` (27,288; full `robustness2` is 81,844) and `^dEQP-VK\.robustness\.buffer_access\.` (1,212) | (a) none (b) `PANVK_DEBUG=robust_ssbo_vec` | 0 Fail in both. Covers SSBO align 4 (0006) and the 019/0003 vectorizer. (b) must equal (a). Then run the full `robustness2` once with (b). |
| 3.4 | Memory: CTS `dEQP-VK.info.device_memory_budget` and `^dEQP-VK\.memory\.pipeline_barrier\.` (104) | none | 0 Fail (0008 budget cache, 018 texture-invalidate skip) |
| 3.5 | ZS preload: CTS `^dEQP-VK\.renderpasses\.renderpass[12]\.suballocation\.(formats\.(d\|s8)[^.]*\|load_store_op_none\|attachment_allocation)\.` (2,294) and `^dEQP-VK\.renderpasses\.dynamic_rendering\.primary_cmd_buff\.suballocation\.formats\.(d\|s8)[^.]*\.` (348) | (a) none (INTERSECT) (b) `PANVK_ZS_PRELOAD_ALWAYS=1` | 0 Fail in both, (a) equal to (b). Then measure FPS/bandwidth in step 9. |
| 3.6 | Regression: step 1.5 geometry probe + step 2.1 on this ICD | none | unchanged |

Update: `docs/reviews/JICA98-FORK-REVIEW.md` (results section), then export as patches `csf-v11/055+`.

## 4. Tessellation (`work/mesa-tess`, `dx8-tess`)

Only once `tessellationShader=1` is wired.
- CTS `^dEQP-VK\.tessellation\.` (1,103): 0 Fail, 0 crash.
- Probe `tests/dxvk/vulkan/tessellation` (add it; the dir is empty) plus
  `tests/dxvk-vkd3d/build-p17-tessellation-shader.sh`.
- DXVK native `d3d11` must then report feature level 11_0 (`0xb000`), not `0xa100`.

## 5. Cube layered (`work/mesa-cube`, `fix-cube-layered`)

- CTS `^dEQP-VK\.geometry\.layered\.cube` (40): 0 Fail (the 6 current fails).
- Re-run step 1.1 (a): 0 Fail.

## 6. Feature bits (`work/mesa-feat`)

- `vertexPipelineStoresAndAtomics`: probe `tests/dxvk/vulkan/vertex-stores`
  (main gate; CTS names no single group for it), plus
  `^dEQP-VK\.(ssbo|image)\..*vert` (8) and `^dEQP-VK\.glsl\.atomic_operations\.`, 0 Fail.
- `maxGeometryShaderInvocations`: CTS `^dEQP-VK\.geometry\.instanced\.` (24):
  all Pass, 0 NotSupported for invocations 64/127.
- `shaderOutputViewportIndex`: CTS `^dEQP-VK\.draw\..*shader_viewport_index` (196),
  plus probe `tests/dxvk/vulkan/multi-viewport` with a GS writing ViewportIndex.
- fp64: CTS `^dEQP-VK\.glsl\..*(double|float64)` (333) and
  `^dEQP-VK\.spirv_assembly\..*float64` (717), 0 Fail.
- `^dEQP-VK\.api\.info\.` (8,177): 0 Fail (limits/feature consistency).

## 7. Submit stress and copy/blit regression (final merged ICD)

| # | Run | Pass criteria |
|---|---|---|
| 7.1 | `/tmp/submit_stress <icd> 150000 1` (`tests/dxvk/vulkan/submit_stress`) | PASS, 150,000 submits, ~74 s, 0 MESA errors |
| 7.2 | `/tmp/submit_stress <icd> 20000 19` | 380,000 RPs PASS |
| 7.3 | `scripts/dxvk/cts-resume.sh -l <cab4 caselist> out/cab5` (the `api.copy_and_blit` BC subset of `/tmp/cts-bc.txt`, 22,764 cases; pull `/tmp/cts-cab4.txt` as the list, or `-r 'api\.copy_and_blit'` for the full group) | 9,620 Pass / 0 Fail / 13,144 NotSupported / 0 DeviceLost (the `cab4` baseline) |
| 7.4 | `memory.zero_initialize_device_memory.image_transition` | 408 Pass / 0 Fail / 24 NotSupported |

Update: `SUBMIT-EXHAUSTION.md`, `DX6-BC-GPU-DECODE.md`.

## 8. DXVK native probes (`tests/dxvk/native`)

Build per `tests/dxvk/native/common/README.md` against DXVK 3.1.1 Native.
Start `Xvfb :99 -screen 0 1280x720x24 -nolisten tcp -ac`. Env:
`DISPLAY=:99 SDL_VIDEODRIVER=x11 DXVK_WSI_DRIVER=SDL2 VK_DRIVER_FILES=/tmp/bp-icd.json DXVK_LOG_LEVEL=warn`.

| Mode | Pass criteria |
|---|---|
| `d3d9`, `d3d9-workload` | exit 0, `HRESULT=0x00000000`, `triangle_pixel=1` |
| `d3d11`, `d3d11-workload` | exit 0, `red_pixel=1`. Feature level `0xa100`, or `0xb000` after tess. |
| `d3d9-present` | exit 0, no hang in `x11_wait_for_present` at swapchain destroy (open issue) |
| `d3d11-headless` | exit 0 |

Update: `DX8-NATIVE-WORKLOAD.md`.

## 9. Real games (last)

DXVK D3D9 and D3D11 titles on the final merged ICD, with the DXVK HUD
(`DXVK_HUD=fps,frametimes,gpuload`). Record FPS and any
`timeout on subqueue` / device loss. A/B `PANVK_ZS_PRELOAD_ALWAYS=1` and
`PANVK_KBASE_CPU_SEMAPHORE_WAITS=1` for perf. Pass: no hang or device loss
over 10 min, and timestamps are sane in the HUD.
