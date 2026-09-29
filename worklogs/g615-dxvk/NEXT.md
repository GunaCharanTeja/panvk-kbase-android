# NEXT

TASK: DX5 dispatch order. PROVED: lowered compute dispatch is emitted and ordered before the passthrough IDVS load. No fix. Stop.
BRANCH: `feature/g615-dxvk-complete`
ADB: `192.168.1.34:32913` serial `Y5WWBMJVOZSK4HU8` duchamp Mali-G615 `/dev/mali0`

CHANGE: none. No patch edit, no rebuild.

PROOF (source of the shipped ICD, `/data/local/tmp/chrootAlpine/tmp/mesa`, matches `build/dx5-edit/mesa`):
- `panvk_vX_cmd_draw.c:3675` `launch_gfx_cs` -> `cmd_dispatch_shader` emits `RUN_COMPUTE` on `PANVK_SUBQUEUE_COMPUTE`, then `cmd_signal_barrier(PANVK_CSF_BARRIER_SYNC)` increments `relative_sync_point`.
- `panvk_vX_cmd_draw.c:3678` `gpu_prerast_wait_compute` on `PANVK_SUBQUEUE_VERTEX_TILER` waits `syncobjs[COMPUTE]` (`panvk_queue.h:24`, index 2) for `GREATER` than `progress_seqno_reg(COMPUTE) + relative_sync_point`. Same form as the stock `wait_finish_tiling` (`:4481`).
- The generated draw is indirect, so its `RUN_IDVS` comes from `launch_indirect_draw` (`:3298`), emitted after the wait. Vertex-tiler stream blocks before the passthrough load.
- `panvk_vX_shader.c:1584` flips the lowered variant to `MESA_SHADER_COMPUTE` before `panvk_compile_nir`, so `RUN_COMPUTE` executes it.
- `PANVK_DEBUG=cs` cannot confirm on device: shipped ICD has no `cs_dump` string.

BUILD: unchanged. ICD `de9af8d244e8341e49bbed5d06942d9385ec7c3dbad31aaa6ecdb081c2200f7c`.

DEVICE_TEST: re-ran `PANVK_DEBUG=gpu_prerast /tmp/dx5-slice` in the chroot. `CASE direct RGBA=0 0 0 0 FAIL`. `idvs_before` also `0 0 0 0` (handoff recorded clear-blue `0 0 255 255`). No DEVICE_LOST. `kbase_queue_wait_current: wait completed successfully`. Device result drifted; not explained by dispatch order.

NEXT FIX: not dispatch order. Record stays empty because the compute store does not land, not because the dispatch runs late. Next: why the lowered VS global store writes nothing (workgroup count, store opcode, or the record buffer). No DX6. No CPU raster.
