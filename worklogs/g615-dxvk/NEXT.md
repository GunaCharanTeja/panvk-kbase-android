# NEXT

TASK: DX5 gpu_prerast VS compute store writes nothing. FIXED. Both paths 13/13.
BRANCH: `feature/g615-dxvk-complete`
ADB: `192.168.1.34:41369` serial `Y5WWBMJVOZSK4HU8` duchamp Mali-G615 `/dev/mali0`

ROOT CAUSE: lowered VS `RUN_COMPUTE` took `gfx.tsd`, still 0 (allocated later by `prepare_draw`). COMPUTE CSG (dmesg "Group 2") `CS_FAULT` 0x58 DATA_INVALID_FAULT, data 0x1612. No store ran.

CHANGE: `patches/csf-v11/020-gpu-prerast-tls-io-barrier.patch`
- per-dispatch TSD via `cmd_dispatch_prepare_tls`, launch via `cmd_dispatch_shader` (+ TLS pointer copy like `cmd_dispatch`).
- `outputs_written`/`record_stride` captured after `panvk_lower_nir` (was 0 → empty passthrough).
- passthrough: `nir_load_vertex_id` (zero_base: "Unhandled intrinsic" SIGSEGV 139), per-slot src types, 16-bit outputs widened, IO offset in location.
- with gpu_prerast, COMPUTE subqueue waits on DRAW_INDIRECT / vertex-input / pre-raster dst barriers.
- harness `gpu_written_indirect`: transfers moved outside render pass, fill→copy WAW barrier added (was UB; flaky on IDVS too).

APPLY: fresh pin 5a07217f + `scripts/apply-patches.sh --profile g615-v11-csf` → `OK applied=21`; tree byte-identical to device-validated tree.
HOST TESTS: `test_gpu_prerast_contracts.py`, `test_crc_invalidate_undefined_clear.py`, `test_nir_load_attr_pan_io_offset.py` PASS.

DEVICE: ICD `24fb09610c651a9a2e2f986684467466e07c6fe8ef8d84551e8a5c7685c8c5a4`. IDVS 13/13, gpu_prerast 13/13, `MATRIX_FAILS=0`, 8/8 consecutive runs after harness fix. No CS_FAULT, no device lost.

FORK: 023 (WAIT64 cross-queue) rejected: needs fork-only export/payload infra, default-on, G720-only proof. 027 rejected: sched_yield busy-wait, forced DVFS max, tiler renew 32768/16MB (disables renew, heap OOM risk), unjustified 2MB ring/30s timeout. 022: no load_attr_pan/nir_lower_io hunks; common/019 unaffected.

NEXT: gpu_prerast still debug-gated (`PANVK_DEBUG=gpu_prerast`); geometryShader/clip/cull stay false. Next: GS/XFB on this path, then real DXVK D3D11 app on gpu_prerast. No CPU raster.
