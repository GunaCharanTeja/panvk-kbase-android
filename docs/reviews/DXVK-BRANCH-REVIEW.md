# DXVK branch review: `9add58a66ee..jica98-adopt`

Target: PanVK on Mali G615 (CSF, v11) running DXVK. This was a read-only review, and nothing in the tree was modified.
Line numbers refer to `jica98-adopt` at `52f02bc2718` (worktree `work/mesa-jica98`) unless stated otherwise.

Scope: gpu_prerast series (`c8ad8dc0e49`..`cf5aa478003`), pipeline statistics (`cf68902b6fb`..`d33a343037f`), and 8 jica98 commits (`bf831a90e67`..`52f02bc2718`). The tiler-heap fix `9add58a66ee` is the base and was read for context only.

Method: codex gpt-6.1-sol did a first pass over five topic chunks. Every finding below was re-checked by hand against the code. Findings that were refuted or could not be confirmed were dropped (see the end of this document).

Count: **High 3 · Medium 5 · Low 6**

---

## High

### H1. Pipeline-statistics End inside a render pass can deadlock the GPU when there are gpu_prerast draws and a tiler OOM
- **Where:** `src/panfrost/vulkan/csf/panvk_vX_cmd_query.c:1083-1093`, introduced by `c0d49182bde`
- **Mechanism:** `panvk_cmd_end_pstats_query()` always emits a barrier that makes the FRAGMENT subqueue wait on the COMPUTE subqueue (`deps.dst[FRAGMENT].wait_subqueue_mask = COMPUTE`). It does this even inside a render pass, where the availability signal itself is deferred. As a result, the fragment CS stalls mid-pass, before `issue_fragment_jobs()` runs.
  - The tiler OOM exception handler is registered on the fragment CS only in `issue_fragment_jobs()` (`panvk_vX_cmd_draw.c:5243-5254`). Until then a VT tiler OOM stays pending.
  - The gpu_prerast arena is a single device-wide slot (`panvk_vX_device.c:590-603`, `available = 1`). The compute CS for draw N+1 blocks in `cs_sync32_wait(arena.available > 0)` (`panvk_vX_cmd_draw.c:3775`) until VT releases the arena after draw N has finished tiling (`:3514-3518`).
- **Failure:** Take one render pass with an active pipeline-stats query, at least 2 GS or non-fill draws, and heap exhaustion while draw N is tiling. The wait chain is then circular: VT waits for the OOM handler, the handler needs the fragment CS, the fragment CS waits on compute, and compute waits for VT to release the arena. The result is a GPU hang or device lost.
- **Fix:** Inside a render pass, do not make the fragment CS wait on compute at End time. Move that wait into the deferred path (`cmd_pstats_flush_pending()`, which runs after `issue_fragment_jobs()`). Alternatively, emit the compute→fragment wait only after the OOM handler has been registered.

### H2. `vkCmdResetQueryPool` of 9–32 queries leaves the tail queries marked available (stale results)
- **Where:** `src/panfrost/vulkan/csf/panvk_vX_cmd_query.c:107-114` (`reset_queries_batch`). This is a pre-existing bug (`5a07217f034`), but it now also covers pipeline statistics (`c0d49182bde`, `:1169-1170`).
- **Mechanism:** Each query takes 2 registers (8 B). The tail loop advances `i` by 8 queries but stores at a byte offset of `i * sizeof(uint32_t)`, which is 4 B per query instead of 8 B. For `query_count` = 9, the second store lands on query 4 and query 8 is never zeroed. For 16, queries 12–15 are skipped. Counts above 32 take the `cs_while` path and are fine.
- **Failure:** Suppose DXVK resets a range of 16 queries and then reuses them. Availability for queries 12–15 still holds 1 from the previous use. `GetQueryPoolResults` or `CmdCopyQueryPoolResults` with `WAIT_BIT` then returns old or partial data immediately. This affects occlusion, primitives-generated and pipeline-statistics queries (availability words). For occlusion, the report words are also left un-zeroed.
- **Fix:** Store at offset `i * regs_per_query * sizeof(uint32_t)`.

### H3. Oversized gpu_prerast draws are silently dropped: indexed strips with restart, fans, and triangle-strip-adjacency
- **Where:** `src/panfrost/vulkan/csf/panvk_vX_cmd_draw.c:4133-4151`, introduced by `42353836a13` (and reached for wireframe by `bff7a7889fa`)
- **Mechanism:** `gpu_prerast_split()` calls `gpu_prerast_unhandled()` and returns `true` in two cases:
  - an indexed strip (`overlap != 0`) with `restart_enable`;
  - a fan or `TRIANGLE_STRIP_ADJACENCY` whose vertex count exceeds the arena capacity.

  `gpu_prerast_unhandled()` only calls `mesa_loge_once` and `assert`, so a release build emits nothing for the draw.
- **Failure:** D3D11 strip topologies always have a strip cut. DXVK therefore enables `primitiveRestartEnable` on indexed strips. Any large indexed triangle or line strip drawn with a GS bound, or with D3D wireframe (`FillMode = WIREFRAME` becomes `polygonMode = LINE`), disappears. The capacity is at most 65536 records and less when the VS outputs are large, so this is reachable with terrain and strip meshes.
- **Fix:** Pass the chunk's first-primitive parity and strip end to the kernels and split on restart segments. Until then, a safer fallback is to run the draw unchunked and have the kernel clamp rather than skip, and to make this loud (e.g. `vk_command_buffer_set_error`) instead of silent.

---

## Medium

### M1. Chunking changes the shader-visible `BaseVertex` / `BaseInstance`
- **Where:** `csf/panvk_vX_cmd_draw.c:4100-4103` (instance split) and `:4161` (non-indexed vertex split), introduced by `42353836a13`. The sysvals come from the chunk at `src/panfrost/vulkan/panvk_vX_cmd_draw.c:846-847`.
- **Mechanism:** Each chunk is re-issued with `instance.base = base + i` or `vertex.base = base + i`. `vs.base_instance` and `vs.first_vertex` are taken from those values, so `gl_BaseInstance` and `gl_BaseVertex` change for every chunk. `shaderDrawParameters` is exposed.
- **Failure:** DXVK computes `SV_InstanceID` as `InstanceIndex - BaseInstance` (and `SV_VertexID` as `VertexIndex - BaseVertex`). Large instanced or non-indexed draws that take the GS or wireframe path therefore restart the IDs at 0 in every chunk. The result is wrong per-instance data from structured buffers and wrong geometry.
- **Fix:** Keep the API base in separate sysvals (the ones the shader sees) and put the chunk offset into the execution offset or attribute offset only.

### M2. An in-pass barrier with a VS, GS or ALL_GRAPHICS destination now trips asserts, and in release builds VT signals without waiting
- **Where:** `csf/panvk_vX_cmd_buffer.c:341-350` (added `ALL_GRAPHICS` and `GEOMETRY_SHADER`, and the condition is now "arena exists" rather than a debug flag), introduced by `b9b2e41d5a9`. The interaction is with `:493-500` and `:699`.
- **Mechanism:** Inside a render pass, COMPUTE is now added as a destination subqueue. It then waits on every source subqueue.
  - With a by-region `ALL_GRAPHICS→ALL_GRAPHICS` self-dependency, the compute wait mask contains FRAGMENT. That fires the assert at `:494`.
  - Line `:499` then strips the VT self-wait, while compute still waits on VT. That fires `assert(deps.src[i].wait_sb_mask)` at `:699`. In release builds, VT signals its sync point without first waiting for its in-flight IDVS jobs.
- **Failure:** Debug builds abort on a common self-dependency pattern once geometryShader or fillModeNonSolid is enabled, and DXVK enables both. Release builds lose VS→(lowered VS) write ordering inside the pass.
- **Fix:** Inside a render pass, either do not add COMPUTE as a destination, or keep the VT self-wait whenever another subqueue waits on VT. Also mask FRAGMENT out of the COMPUTE wait for by-region dependencies.

### M3. Indirect gpu_prerast draws are truncated at the arena capacity without any error
- **Where:** `csf/panvk_vX_cmd_draw.c:4071-4086` (documented as a `ponytail:` shortcut) and the kernel caps (`record_capacity`, `:3921-3923`), introduced by `42353836a13`
- **Failure:** A `DrawIndexedInstancedIndirect` with a GS or in wireframe whose GPU-side `count × instances` exceeds the capacity has its tail primitives silently missing.
- **Fix:** Loop over GPU-side chunks in the CS, or at least record an error or counter so the truncation can be detected.

### M4. Blocking `vkGetQueryPoolResults` can hang forever on a shared kbase fd
- **Where:** `src/panfrost/vulkan/panvk_vX_query_pool.c:183-189`, introduced by `cf68902b6fb`, together with `src/panfrost/lib/kmod/kbase_kmod.c:796-811`
- **Mechanism:** `kbase_kmod_csf_wait_event()` does `ppoll` and then a blocking `read()` on the device fd. The queue wait (`panvk_vX_gpu_queue.c:949`) uses the same function. If two threads both see POLLIN for one pending notification, one of them consumes it and the other blocks in `read()` without any timeout. This defeats `PANVK_QUERY_TIMEOUT` and can starve the other waiter if no further CSF events arrive.
- **Failure:** An app thread calls `GetQueryPoolResults(WAIT)` while DXVK's submission or finish thread waits on a fence. Either thread can stall indefinitely once the GPU goes idle.
- **Fix:** Open the fd non-blocking (or use `recv` with `MSG_DONTWAIT`) and treat `EAGAIN` as a spurious wakeup. Alternatively, serialise the poll and read under a device mutex with a single consumer.

### M5. A single device-wide gpu_prerast arena is shared by both graphics queues and synchronised only with CSG-scope syncs
- **Where:** `panvk_vX_device.c:590-603` (one arena), `csf/panvk_vX_cmd_draw.c:3772-3777` (acquire) and `:3514-3518` (release). Both use `MALI_CS_SYNC_SCOPE_CSG`. `queueCount = 2` is set at `panvk_physical_device.c:1518`.
- **Mechanism:** Each VkQueue is its own CSG. The acquire/release counter is updated with CSG scope. The driver's own comment at `panvk_vX_query_pool.c:177-178` notes that CSG-scoped syncs do not wake waiters outside the group.
- **Failure:** When both queues run GS or wireframe work, a waiter in queue B may not be woken when queue A releases the arena. The result is a long stall or a hang until an unrelated event arrives. vkd3d-proton and multi-queue apps can hit this; DXVK with a single queue does not. The cross-CSG wake behaviour still needs to be confirmed on hardware, but the sharing itself is certain.
- **Fix:** Use one arena per queue (and keep a per-queue pointer in the subqueue context), or use SYSTEM scope for the arena acquire and release.

---

## Low

### L1. Indirect-count gate uses a signed comparison
- **Where:** `csf/panvk_vX_cmd_draw.c:3807-3813` (`42353836a13` path)
- **Mechanism:** The code computes `count - draw_index` and tests it with `LEQUAL 0` as a signed value.
- **Failure:** A count buffer value of 2^31 or more (legal, since the spec clamps it to `maxDrawCount`) drops every draw.
- **Fix:** Use `cs_umin32(count, maxDrawCount)` first, or an unsigned compare.

### L2. Triangle and line lists with restart are chunked at fixed alignment
- **Where:** `csf/panvk_vX_cmd_draw.c:4112-4123` and `:4147` (`42353836a13`)
- **Mechanism:** `primitiveTopologyListRestart = true` is exposed, but the restart check only rejects strips. A restart index inside a list shifts primitive assembly. The chunks at `i % 3 == 0` and `gpu_prerast_prim_base = i/3` then assemble the wrong vertices and PrimitiveIDs.
- **Fix:** Apply the same restart rejection (or the future restart-aware split) to lists.

### L3. Conditional rendering is not applied to gpu_prerast compute or to pipeline-stats updates
- **Where:** `gpu_prerast_launch_cs` (around `csf/panvk_vX_cmd_draw.c:3522`) has no `panvk_cond_render`. Statistics are added in `:4181-4184` (`c0d49182bde`) and `csf/panvk_vX_cmd_dispatch.c:357`.
- **Failure:** When the predicate is false (D3D11 predication through DXVK), the lowered VS and GS kernels still run, so their SSBO side effects happen. The statistics counters are also incremented for skipped draws and dispatches.
- **Fix:** Wrap the prerast dispatches and the statistics updates in `panvk_cond_render`.

### L4. Pipeline-stats availability is signalled before the fragment jobs in three cases (documented)
- **Where:** `csf/panvk_vX_cmd_query.c:1105-1112` (more than 8 pending queries), `csf/panvk_vX_cmd_draw.c:5569-5572` (suspend), and `csf/panvk_vX_cmd_buffer.c:1193-1195` (secondary overflow). All come from `c0d49182bde`.
- **Failure:** The FS-invocations result can be read as available while it is still incomplete.
- **Fix:** Use a growable pending list, and carry it across suspend/resume.

### L5. FS-invocation statistic comes from the occlusion sample counter (documented)
- **Where:** `csf/panvk_vX_cmd_query.c:940-962` (`c0d49182bde`)
- **Failure:** The counter misses fragments killed by late depth tests, counts samples when MSAA is on, and reads 0 while an occlusion query is active.

### L6. `kbase_gpu_waits_disabled()` has an unsynchronised static lazy-init
- **Where:** `csf/panvk_vX_gpu_queue.c:2674-2680` (`f811df2629a`)
- **Mechanism:** This is a benign data race. A related issue is that `kbase_gpu_waits[j][*]` is filled in for subqueues that have no work in the submission (`:2772-2775`). Those entries persist, forcing `flush_id = 0` (a full flush) on that subqueue's next job; this costs performance but is not a correctness problem.
- **Fix:** Use `DEBUG_GET_ONCE_BOOL_OPTION`, and fill in `kbase_gpu_waits` only for the subqueues this submission actually uses.

---

## Checked and found correct (no finding)
- **Chunk split for lists, line strips and triangle strips without restart** (`gpu_prerast_split`): the triangle-strip step is even so parity is preserved; the overlap of 2, 1 and 3 is correct; `prim_base = i / prim_div` gives the right PrimitiveID base; the loop bound `i + overlap < count` is correct. The per-draw indirect loop passes `draw_index` correctly.
- **Arena reuse inside one queue:** acquire on compute waits for `available > 0`, and release on VT happens after `cs_wait_slots(all_iters)`. VT also waits on compute before the generated draw (`gpu_prerast_wait_compute`).
- **Same-queue GPU semaphore waits (`f811df2629a`):** only binary waits (`wait_value == 0`) owned by the same queue are converted, so wait-before-signal cannot occur; a timeline wait still goes through the CPU path. A SIGNALED state is skipped. The 64-bit `seqno > target-1` comparison is correct. Signals from other queues or external sources fall back to `vk_sync_wait`.
- **Budget cache (`bf831a90e67`):** at worst a torn pair of atomics returns a value up to 100 ms stale. That is harmless.
- **Timestamp frequency from cntfrq (`8ea47bfd934`):** the scaling uses a double factor (`pan_kmod.h:1101`), so there is no u64 overflow. The commit also fixes the earlier `timestampComputeAndGraphics = true` with period 0 on kbase. On a non-aarch64 build the frequency stays 0, as before.
- **Texture-invalidate skip (`cdb3a3be507`):** only `SHADER_READ` and `SHADER_SAMPLED_READ` are stripped, and only for buffers without `UNIFORM_TEXEL_BUFFER` usage. Storage and uniform reads are coherent with LS/L2 (`add_memory_dependency`), so nothing that needs the invalidate loses it.
- **ZS preload INTERSECT on v11 (`057f73fc249`):** it uses the same `always_load()` rule as the colour preload (`:883-885`) and the v7.2 path (`:851-854`): LOAD_OP_CLEAR or partial cases (`.always`) fall back to EARLY_ZS_ALWAYS, and there is a `PANVK_ZS_PRELOAD_ALWAYS` escape. v10 is unchanged, so this is a deliberate v10/v11 divergence.
- **SSBO alignment of 4 on v9+ (`49a7443f2a0`):** no remaining 16 B assumption was found in the NIR alignment options. The change still needs HW validation on G615.
- **Robust SSBO vectorizer (`87e0e5c2093`):** it is off by default, behind `PANVK_DEBUG=robust_ssbo_vec`, and runs on FS only. A focused review found no concrete robustness2 violation.
- **Vectorizer upper bound (`52f02bc2718`):** it proves `base + diff` cannot wrap in u32 using `nir_addition_might_overflow`. This gives the same guarantee as the existing wrap check and does not weaken it.

## Dropped (refuted or not confirmed)
- **Dynamic polygon-mode shader cache collision:** `extendedDynamicState3PolygonMode = false`, so this is unreachable.
- **Flat-shading provoking vertex and `gl_FrontFacing` for polygon-mode lines/points:** the spec requirement could not be confirmed with certainty.
- **VS-invocation overcount on restart indices:** vertex-shader invocation counts are allowed to be approximate.

<!-- biblioklept -->
