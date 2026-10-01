# Long-run DeviceLost: kbase tiler heap never renewed

Date 2026-09-29. Exported patch `csf-v11/043`. Mesa `work/mesa`
`dx6-dx7-base`, fix `9add58a66ee`
("panvk/csf: account tiler work so the kbase tiler heap gets renewed") on
`bdd0c26cc5e`. Device duchamp Mali-G615 MC6, ADB `192.168.1.61:41161`, chroot
`/data/local/tmp/chrootAlpine`, `/tmp/mesa` synced, `/tmp/bld.sh` `NINJA_RC=0`,
ICD sha256 `a9c8dfe43d13ca7ef407c6d0134f961856c8b3eb75fd02de1a003047c01aec31`.

## Symptom

In the full `api.copy_and_blit` BC run, `blit_image...bc2_srgb_block.r8g8b8a8_srgb.optimal_optimal_linear`
hit `kbase: timeout on subqueue 0: seqno 33910 ... target 33911`, then
DeviceLost.

## Root cause

`panvk_vX_gpu_queue.c` renews the kbase tiler heap only when
`submit->tiler_work_estimate != 0` (`bdd0c26cc5e` lines 2782 and 2792).
Nothing ever set `panvk_cmd_buffer.state.tiler_work_estimate`
(`panvk_cmd_buffer.h:600`) or the submit sum. The field has been dead since the
base import (`dea61483d0f`). The draw-side accounting from the g720 reference
was never ported. As a result the heap was never renewed. kbase has no
FRAGMENT_COMPLETED chunk recycling, so the heap exhausts after about
66k-71k render passes. The tiler then stalls in `RUN_FULLSCREEN`/IDVS, with no
fault and no OOM handler.

Evidence gathered at the hang:
- kbase CSG dump: VT CS `BLOCKED_REASON: DEFERRED` on the `HEAP_OPERATION`
  (VT completed) after `FINISH_TILING`. Tiler `STATUS 7`, `EP_EVT_STATUS 1`.
  The fragment CS was blocked in `SYNC_WAIT` on the VT syncobj.
- The `TILER_HEAP` descriptor readback was `size 0, base 0, bottom 0x40, top 0`,
  and `kbase_tiler_submit_count=0` / `work_count=0` (never renewed). Tiler
  contexts after RP05 of the hung command buffer had no polygon list written.
- The hang is independent of the GPU VA layout (a 16- or 1000-page shift
  moved every address and the hang stayed at seqno 33911), and of
  `PANVK_KBASE_HEAP_RENEW_INTERVAL=0/1/16` and `_WORK=1` (dead path). It is
  not memory growth: RSS stays near 140 MB, kbase ctx pages stay at
  22-26k, and the fd count stays at 9. Skipping the first 3000 cases moved
  the hang to another linear blit at seqno 28550, which matches
  cumulative render-pass tiler usage.
- The TRANSLATION_FAULT at `0x5ffb310d40` after the timeout is a
  teardown artefact: the tiler was still hung while BOs were freed.

## Fix (`9add58a66ee`)

- `get_tiler_desc()` accounts `td_count` per render pass that uses the
  tiler, including RUN_FULLSCREEN-only passes such as meta blits.
- `panvk_cmd_draw()` accounts `vertex.count * instance.count`, or
  `draw_count * 256` for indirect draws (as in `ref-g720-beta`).
- `CmdExecuteCommands` adds the secondaries' work. Submit init sums the
  command buffers' work into `submit->tiler_work_estimate`.

## Reproducer

`tests/dxvk/vulkan/submit_stress/submit_stress.c`: N submits, each with
RPS x (clear + triangle draw), a readback, and a fence wait. Every 1000
submits it checks the pixels.

```sh
gcc -O2 -Wall -o /tmp/submit_stress submit_stress.c -ldl   # in /tmp/dx7-h/submit_stress
/tmp/submit_stress /tmp/build-glibc/src/panfrost/vulkan/libvulkan_panfrost.so <N> <RPS>
```

| build | N x RPS | result |
|---|---|---|
| `bdd0c26cc5e` | 20000 x 19 | timeout subqueue 0 at seqno 3467 (~65.9k RPs), rc=1 |
| `bdd0c26cc5e` | 100000 x 1 | timeout subqueue 0 at seqno 70759, rc=1 |
| `9add58a66ee` | 20000 x 19 | PASS, 380000 RPs, 19.8 s, 0 MESA errors |
| `9add58a66ee` | 150000 x 1 | PASS, 150000 submits, 73.6 s, 0 MESA errors |

## CTS (`/tmp/cab.sh <regex> <tag>`, single deqp process unless it aborts)

| run | cases | Pass | Fail | NS | DeviceLost | processes |
|---|---|---|---|---|---|---|
| `api.copy_and_blit`, `bdd0c26cc5e` (cab3) | 22764 | 9619 | 0 | 13144 | 1 | 2 |
| prefix 1..17290, `bdd0c26cc5e` (sub.sh) | 17290 | 8185 | 0 | - | aborted at 17274 | 1 |
| prefix 1..17290, fix (sub.sh) | 17290 | 8196 | 0 | - | 0 | 1 |
| `api.copy_and_blit`, fix (cab4) | 22764 | **9620** | 0 | 13144 | **0** | 1 |
| BC batch 6348 (texture.compressed, texel_view_compatible, pipeline.monolithic, extended_usage_bit_compatibility, api.info), fix (big6) | 6348 | 1398 | 0 | 4950 | 0 | 1 |
| copy_and_blit + BC batch, fix (all7) | 29112 | 11018 | 0 | 18094 | 0 | 1 |

BC5 3D case `texel_view_compatible.graphic.basic.3d_image.texture_read.bc5_unorm_block.r32g32b32a32_uint`
passes on the fix, both alone and in big6/all7. The old build also passes
the 6348-case batch now (big6o: 1398/0/4950, no timeout), so that one-off
could not be reproduced. It is consistent with the same heap exhaustion in a
longer earlier process, but this is not proven.
