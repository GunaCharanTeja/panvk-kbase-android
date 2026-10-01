# jica98 fork review (PanVK kbase, Mali-G615 / CSF v11)

Date: 2026-09-30. Reviewed against `work/mesa` `dx6-dx7-base` @ `d33a343037f` (Mesa pin `5a07217f034`, csf-v11 patches up to 054).
Local clones (read-only, untrusted): `work/jica98/panvk-kbase-android.git` (mirror), `work/jica98/bachata-s4-drivers`, extracted patches in `work/jica98/export/`.

## 1. Repositories

| Repo | Relation | Relevance |
|---|---|---|
| `jica98/panvk-kbase-android` | GitHub fork of our `abhay-byte/panvk-kbase-android` | **The only relevant repo.** |
| `jica98/bachata-s4-drivers` | Turnip (Adreno/KGSL) glibc build scripts + CI | Not Mali. Skip. |
| `jica98/Bachata-S4` | PS4 emulator (closed source; ships APKs only) | Consumer of the driver. Not reviewed (no source). |
| `jica98/Bachata-S4-Runtimes` | `runtime.zip` release asset | Binary. Not reviewed. |

There is no separate Mesa fork. jica98 keeps Mesa changes as patch files inside the panvk repo, the same way we do.

## 2. Branches and unique commits

Fork `main`, `feature/g615-consumer-completion` and `feature/g615-beta3-capabilities` are identical to ours (0 unique commits). All three work branches fork from our `052a5d3` ("docs(g615): record DXVK/vkd3d compliance matrix").

| Branch | Last commit | Unique commits |
|---|---|---|
| `bachata/v2-perf-patches` | 2026-09-30 `11a7cbf` | 4 (`3a0bfef`, `7d54407`, `f1640a4`, `11a7cbf`) |
| `codex/robust-ssbo-experiment` | 2026-09-28 `a636d23` | 3 (`3a0bfef`, `68dccfc`, `a636d23`) |
| `codex/valhall-driver-performance` | 2026-09-27 `3a0bfef` | 1 (`3a0bfef`) |

That makes **6 distinct commits** in total (`3a0bfef` is shared by all three branches). They carry 10 Mesa-level changes:

| Commit | Date | Subject | Files, +/- |
|---|---|---|---|
| `3a0bfef` | 09-27 | perf(csf): avoid unused texture cache flushes | 9 files, +1037/-4. Adds `patches/csf/018-buffer-cache-usage.patch` and a probe test in `tests/synchronization/buffer-cache/`. The change to `017-disable-unvalidated-extensions.patch` only widens its context (the extension set is unchanged), and the `016` WSI change only moves hunk offsets. |
| `68dccfc` | 09-28 | feat(nir): add guarded SSBO vectorization | `patches/csf/019-robust-ssbo-vectorization.patch`, +1179/-1. `sources.lock` marks it as "Research branch ... not a generally qualified performance release". |
| `a636d23` | 09-28 | test(nir): add robust SSBO GPU oracle | 5 test files, +898 |
| `7d54407` | 09-30 | patches: add Bachata V2 performance series | `patches/bachata-v2-perf/0001..0007`, +1300 |
| `f1640a4` | 09-30 | patches: add PanVK budget query memory cache | `0008`, +74 |
| `11a7cbf` | 09-30 | record God of War III results | README only |

The Bachata V2 series applies on top of a "Bachata V2 composite" (patch-series sha `5afebe6b...`) that is **not published**. Any PS4-specific hacks probably live there and could not be reviewed. The series README reports these results on G615 MC6: Dark Souls Remastered 15.2 → 19.3 FPS; God of War III 26.9 FPS (0001-0007), then 30.8-36 FPS with 0008.

## 3. Categorisation

| Change | Category |
|---|---|
| 018 buffer-cache-usage | kbase/CSF submission: barrier cache flush (performance) |
| 019 robust SSBO vectorization | Shader compiler (NIR, Valhall), performance |
| 0001/0002 timestamp freq + ns factor | kbase: timestamps / queries (feature correctness) |
| 0001/0002 ZS preload INTERSECT | Performance: frame shaders |
| 0001/0002 utrace waits, clone `cs_wait_slots` | Debug/tracing |
| 0001 barrier stats, narrow-image-barrier experiment, `panvk-env.txt` loader, `panvk-utrace-on` gate | Dev hooks (all removed again by 0007) |
| 0003 vectorizer upper-bound robustness check | Shader compiler (common NIR), performance |
| 0004 X11 software WSI | WSI / packaging (X server path) |
| 0005 same-queue semaphore waits on GPU | kbase/CSF queue submission, performance/stall |
| 0006 minStorageBufferOffsetAlignment 4 | Features/limits, descriptors |
| 0008 cached budget query | Memory, CPU performance |

Nothing in the fork touches tessellation, transform feedback, `vertexPipelineStoresAndAtomics`, `shaderFloat64`, cube layered rendering, `maxGeometryShaderInvocations`, BC formats, GS, pipeline statistics or the tiler heap. The feature and extension tables are the same as ours apart from the 0006 limit. Every commit marks its code `Generated-by: LLM`.

## 4. Per-change review

### 4.1 `018-buffer-cache-usage` (commit `3a0bfef`): ADOPT
- **What it does.** In `CmdPipelineBarrier2`'s buffer-barrier loop (ours: `src/panfrost/vulkan/csf/panvk_vX_cmd_buffer.c:~598`), if the buffer lacks `VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT`, the patch strips `SHADER_READ|SHADER_SAMPLED_READ` from the destination access. This stops `add_memory_dependency` (`panvk_vX_cmd_buffer.c:398-447`) from emitting `MALI_CS_OTHER_FLUSH_MODE_INVALIDATE`, which invalidates the texture and other read-only L1 caches.
- **Correctness.** Sound. Only uniform texel buffers are read through the incoherent texture cache. Storage texel loads lower to global loads (`bifrost_nir.c:~1101`), and SSBO and UBO reads go through the load/store cache. Stage masks are unchanged, so execution dependencies stay intact. `TRANSFER_READ` and `VERTEX_ATTRIBUTE_READ` survive the strip. A `gpt-6.1-sol` source review agreed.
- **We already have it?** No. **GPU-side?** Yes. **Conflicts?** It sits next to our 040/045 hunks in the same file (045 changes `collect_cs_deps`/`add_execution_dependency`). The logic is independent, but the patch needs a trivial rebase.
- **Check before merging.** Our gpu_prerast VS-as-compute path must not read app buffers through a texture/attribute descriptor. The DXVK gain is modest, because DXVK often sets texel usage on buffers.

### 4.2 `019-robust-ssbo-vectorization` (commit `68dccfc`): ADAPT, high value for DXVK
- **Why it matters.** `bifrost_nir.c:920-922` turns SSBO vectorization **off** whenever robustness2 covers SSBOs. DXVK always enables robustBufferAccess2, so every scalar UAV/structured-buffer load stays scalar.
- **What it does.** Adds a new NIR pass, `nir_opt_vectorize_robust_ssbo_loads`. It groups 2-4 adjacent 32-bit read-only (`NON_WRITEABLE|CAN_REORDER`) `load_ssbo` instructions that share a resource, a base and a scale (1 or 4). It emits `if (size >= N*4 && size-N*4 >= low) wide_load else original scalar loads`, then joins the results with phis. Per-component robustness2 semantics are preserved exactly, and the overflow reasoning is sound. It is wired into `panvk_vX_shader.c` for `PAN_ARCH >= 9`, fragment shaders only, when storage-buffer robustness is enabled.
- **Limits.** It only handles fragment shaders. It bails out if the shader has any store, atomic or barrier, and it only looks at top-level blocks. It adds an `if` per group, and the offset parser is ad hoc. The upstream-quality path would teach `nir_opt_load_store_vectorize` a robust "guarded" mode instead of adding a second pass. It also includes 719 lines of gtest.
- **We have it?** No. **GPU-side?** Yes. **Conflicts:** our 018/020/021/023/025/042/046/047/051 all touch `panvk_vX_shader.c`. The insertion point (`~:885`, before the `allow_merging_workgroups` block) needs a manual rebase. It does not interact with prerast semantics, because it is FS-only.
- **Plan.** Take it behind a debug flag. Validate with CTS `dEQP-VK.robustness.robustness2.*` plus their oracle test, and measure DXVK titles. Then consider widening it to compute shaders.

### 4.3 `0005` same-queue semaphore waits on GPU: ADOPT (after CTS)
- **What it does.** Ours at `csf/panvk_vX_gpu_queue.c:2739` does `vk_sync_wait_many(..., UINT64_MAX)` on the **CPU** for every submit wait. That blocks the submit thread until the GPU drains, which is a big stall for DXVK, since it submits with timeline semaphores. The patch adds `panvk_kbase_sync_peek()` (`panvk_physical_device.c`, next to our `kbase_cpu_sync` at `:640-760`). When the wait's sync is pending and owned by the same queue, it emits `SYNC_WAIT64 GREATER seqno-1` in the ring entry for each producer subqueue. It also raises `kbase_target_seqnos` so that signals cover those waits. Other syncs still wait on the CPU, and `PANVK_KBASE_CPU_SEMAPHORE_WAITS=1` restores the old behaviour.
- **Correctness.** Sound on source review. The producer job cleans L2/LS before bumping its seqno (`panvk_vX_cmd_buffer.c:181`). Timeline waits reach the patch as unwrapped binary points with `wait_value==0`, so they benefit too. Peek runs under the mutex. Sol agreed.
- **Conflicts.** Our 043 (tiler-heap renewal) touches `gpu_queue.c` only at `panvk_queue_submit_init_storage` (`:2303`), so there is no textual overlap. **GPU-side:** it replaces a CPU wait with a GPU wait. Run `dEQP-VK.synchronization*.timeline_semaphore.*` and `*.semaphore*` before enabling it by default.

### 4.4 `0001`+`0002` kbase timestamp frequency: ADAPT (the timestamp part only)
- **Our gap.** `lib/kmod/kbase_kmod.c:373` sets `timestamp_frequency = 0`. As a result, `timestampPeriod = 0` (`panvk_physical_device.c:1136-1142`, `panvk_vX_physical_device.c:1005`), while `timestampValidBits = 64` and `timestampComputeAndGraphics = true`. DXVK timestamp queries (D3D `QUERY_TIMESTAMP`, the HUD, frame pacing) and utrace get nonsense values, and utrace is disabled (see the comment in `patches/csf/007`).
- **Fix in the fork.** On aarch64, read `cntfrq_el0` and set `timestamp_cycles_to_ns_factor = 1e9/freq`. This is consistent with kbase itself, which assumes the GPU timestamp ticks at the arch-timer frequency. A better approach is to calibrate once against `KBASE_IOCTL_GET_CPU_GPU_TIMEINFO` and fall back to cntfrq.
- **Take only this and the ns factor.** Leave out the utrace wait/`cs_wait_slots` debug hunks unless we want utrace.
- **Do not take the 0001 dev hooks.** These are the `panvk-env.txt` loader, the barrier stats, `PANVK_EXP_NARROW_IMG_BARRIERS` and the `panvk-utrace-on` file gate. 0007 removes them again.

### 4.5 `0001`/`0002` ZS preload INTERSECT: ADAPT (v11 only, benchmark)
- **What it does.** `panvk_vX_cmd_frame_shaders.c:812-824` uses `EARLY_ZS_ALWAYS` on v9-v12. The upstream comment there says INTERSECT is valid but has not been benchmarked. The fork switches to `INTERSECT` when `!always_load()`, with a `PANVK_ZS_PRELOAD_ALWAYS` opt-out. It also fixes the `pan_desc.h:408` force-clean-tile mapping for the new modes. The v12/v13 variants are untested on hardware.
- **Recommendation.** For v11, take the `INTERSECT` branch plus the `pan_desc.h` mapping, and measure bandwidth/FPS ourselves. It is GPU-side, has no prerast conflict, and `frame_shaders.c` is untouched by our series.

### 4.6 `0003` vectorizer upper-bound robustness check: ADOPT (small), with a caveat
This adds 7 lines to `check_for_robustness()` (ours: `src/compiler/nir/nir_opt_load_store_vectorize.c:1430-1460`). If `nir_addition_might_overflow(base, high_offset)` is false, combining the loads cannot change what counts as out of bounds, so vectorization is allowed. The logic is sound and uses existing helpers (`uub_ht` at `:222`, `nir.h:7288`). It is common Mesa code and it is not upstream. **For DXVK on panvk it does nothing today**, because the bifrost backend excludes SSBOs from `modes` under robustness (`bifrost_nir.c:920`). It only matters for other robust modes or if we relax that gate.

### 4.7 `0006` `minStorageBufferOffsetAlignment` 16 → 4 on v9+: ADAPT
This changes `panvk_vX_physical_device.c:980` and `panvk_vX_shader.c:429` (`min_ssbo_alignment`). The compiler side is safe: `mem_vectorize_cb` (`bifrost_nir.c:224-228`) already allows unaligned vectors on v9+. The stale comment about LOAD.i128 needing 16 B alignment should be fixed. This helps vkd3d-proton raw/structured views and DXVK SSBO suballocation. Validate against CTS `robustness2` with 4-byte offsets, because bounds checks use the descriptor offset and range. It conflicts textually with our 022-052 edits in the same property block, but only trivially.

### 4.8 `0008` cached memory budget: ADOPT (trivial)
This caches `os_get_available_system_memory()` (a `/proc/meminfo` read) for 100 ms using atomics in `panvk_GetPhysicalDeviceMemoryProperties2` (ours: `panvk_physical_device.c:~1701`). DXVK polls the budget every frame, and the fork measured a large FPS jump in GoW3. The race between the two atomics is benign.

### 4.9 `0004` X11 software WSI: SKIP
This only changes behaviour when `wsi->sw` is true (lavapipe or `WSI_DEBUG=sw`), which is not our hardware WSI path. It also changes the memory-type selection in common `wsi_select_host_memory_type` for **all** software WSI platforms, but only X11 invalidates the non-coherent mapping. That would be a latent bug on Wayland/headless software paths.

### 4.10 `0007` drop dev hooks: n/a
It only removes what 0001 added. If we cherry-pick pieces of 0001/0002 directly, we do not need it.

## 5. Candidate table

| # | Change | Verdict | Value to DXVK | Reason |
|---|---|---|---|---|
| 1 | 0005 GPU same-queue semaphore waits | **ADOPT** | High (removes CPU drain on every waited submit) | Sound, GPU-side, opt-out env, no overlap with 043 |
| 2 | 019 guarded robust SSBO vectorization | **ADAPT** | High (robustness2 disables SSBO vectorization today) | Correct semantics; FS-only and ad hoc; needs a debug gate, CTS and a rebase over our `panvk_vX_shader.c` edits |
| 3 | 0001/0002 timestamp frequency + ns factor | **ADAPT** | High for correctness (timestampPeriod is 0 today) | Take cntfrq/ns only; prefer TIMEINFO calibration |
| 4 | 018 skip texture-cache invalidate for non-texel buffers | **ADOPT** | Medium | Sound; trivial rebase near 040/045 |
| 5 | 0008 budget cache | **ADOPT** | Medium (per-frame budget polling) | Trivial, safe |
| 6 | 0006 SSBO offset alignment 4 | **ADAPT** | Medium (vkd3d/DXVK suballocation) | Compiler already allows unaligned; needs robustness CTS |
| 7 | ZS preload INTERSECT (v11) | **ADAPT** | Medium (bandwidth) | Valid per upstream comment; benchmark it; skip untested v12/v13 bits |
| 8 | 0003 vectorizer upper bound | ADOPT (low) | Low now | Sound but inert for panvk SSBOs under robustness |
| 9 | 0004 X11 SW WSI | SKIP | None | Only affects `wsi->sw`; latent non-coherent bug on other sw WSIs |
| 10 | 0001 dev hooks, 0007 | SKIP | None | Debug hooks; `panvk-env.txt` is a security concern |

## 6. Security flags

- **`0001` `panvk_load_env_file()`** reads `panvk-env.txt` from the driver `.so` directory (found via `dladdr`) and calls `setenv()` for every `KEY=VALUE` line. Anyone who can write that directory can inject arbitrary environment variables, and `setenv` is not thread-safe at runtime. 0007 removes it. **Never take 0001 wholesale.**
- The `0001` `panvk-utrace-on` file gate is also file-driven behaviour, and 0007 removes it too.
- Tests: `buffer_cache_driver_probe.cpp:711` and `robust_ssbo_driver_probe.cpp:451` `dlopen()` a driver path from argv. This is benign test harness code. The `build.sh` scripts only run glslang, spirv-val and g++, with no downloads.
- No binaries, obfuscation, telemetry or network code in the fork's unique commits.
- `bachata-s4-drivers/scripts/build-driver.sh:24` `git clone`s Mesa at build time, and its CI publishes releases. That repo is Turnip-only and irrelevant.
- The Bachata V2 base composite and the emulator are closed or unpublished. The published perf series was tested on top of unknown extra patches, so re-measure everything on our tree.

## 7. Suggested integration order

1. **0008** budget cache: trivial and isolated.
2. **Timestamp frequency** (cntfrq + ns factor, ideally with TIMEINFO calibration). Validate with CTS `dEQP-VK.query_pool.*timestamp*` and a DXVK HUD check.
3. **018** buffer-cache barrier. Rebase next to 040/045, then run the synchronization CTS subset and a prerast GS title.
4. **0005** GPU semaphore waits. Run `dEQP-VK.synchronization*`, both timeline and binary semaphores, then DXVK titles.
5. **0006** SSBO alignment 4, followed by `dEQP-VK.robustness.robustness2.*`.
6. **ZS INTERSECT (v11 only)**, gated and benchmarked.
7. **019** robust SSBO vectorization behind a debug flag. Run robustness2 CTS plus the jica98 oracle, then enable by default, and consider extending it to compute shaders.
8. **0003**: optional, together with 019 or a relaxed `bifrost_nir.c:920` gate.
