# NEXT

TASK: DX8. GS default-on, prerast fixes, pipeline stats, tiler-heap + zero-init
fixes DONE (patches to `csf-v11/054`). jica98 adopt branch built, not device-run.
Tessellation / cube layered / feature bits in progress. Device offline.
BRANCH: `feature/g615-dxvk-complete`
ADB: `192.168.1.61:41161` duchamp Mali-G615 MC6 `/dev/mali0` (was
`192.168.1.34:41369`, serial `Y5WWBMJVOZSK4HU8`). Offline 2026-09-30.
Pending device runs: `validation/g615-v11-csf/dxvk/DEVICE-TEST-PLAN.md`.

DX6: BC1-7 GPU compute decode (`csf-v11/022`), default on since `csf-v11/039`
(`textureCompressionBC=1`; `PANVK_DEBUG=no_bc_emul` opts out). G615 has no HW
BC (BC mask 0). 16/16 device + host verify; CTS BC subset 1863 pass / 0 fail.
Full `api.copy_and_blit` (22,764 cases, `/tmp/cab.sh 'api.copy_and_blit' cab4`,
after 043): 9,620 Pass / 0 Fail / 13,144 NotSupported / 0 DeviceLost (pre-043:
1 DeviceLost). Combined `all7`: 29,112 cases, 11,018 Pass / 0 Fail.
See `validation/g615-v11-csf/dxvk/DX6-BC-GPU-DECODE.md`.

Backports (`csf-v11/026-037`, upstream authors kept, `Backport-of:`):
fbb4993c5d7 (026), c9c207bfd3d (027), fb3853764b2 KHR_incremental_present (028),
eef8db800ac EXT_swapchain_colorspace (029), 6409a5b16af EXT_image_compression_control
+ deps 6c89f9d2b7e 2042227efab 98ed811046b 50bda0b8978 90be04d45a7 19f5cf79c5e
e3a315d636a (030-037). All three extensions device-proven
(`tests/dxvk/vulkan/backport-ext/`: WSI_EXT_FAILS=0, ICC_FAILS=0), so no hide patch.
50bda0b8978 (AFBC WSI default) kept: kbase glibc path OK; Android Gate H not run.
Dropped: 83cda621398 ANDROID_external_format_resolve (needs vk_android EFR chain
68bda968147.. that does not apply on the pin; Android-only, has_gralloc=false in
chroot). WIP on work/mesa branch `bp-efr-wip`.
Series order note: backports sit in csf-v11 (common/ placement breaks
kbase-common/006 and csf-v11/022).
Local fixes: 038 CRC init `pan_kmod_bo_munmap` (os_munmap killed kbase SAME_VA
GPU mapping -> CSF 0xc3), 040 BC decode on ZERO_INITIALIZED transition.
Zero-init: 041 `efce7cc4382` kbase zeroes BO pages at alloc. Fixed the general
zeroInitializeDeviceMemory bug: `memory.zero_initialize_device_memory.image_transition`
408 Pass / 0 Fail / 24 NotSupported (was 248 fail).

DX7:
- `csf-v11/023`: clip/cull distance. VS writes CLIP_DIST0/1 + per-vertex
  cull-negative flags CULL_DIST0/1; FS discards (clip < 0, flag >= 1-2^-16 =
  all vertices negative). Link keyed on slot section. 13/13 IDVS + gpu_prerast.
  `validation/g615-v11-csf/dxvk/DX7-CLIP-CULL.md`.
- `csf-v11/024`: multiViewport, maxViewports 16; viewport index is always 0
  while no stage can write ViewportIndex. 5/5. `DX7-MULTIVIEWPORT.md`.
  GS can now write it; `shaderOutputViewportIndex` is in the feature-bits work.
- `csf-v11/025`: fillModeNonSolid. Non-FILL pipelines get GPU_POLYGON compute
  kernel on gpu_prerast (assemble list/strip/fan+restart, det(x,y,w) facing,
  cull, emit line/point indices + indirect draw); passthrough draws
  LINES/POINTS. 17/17 pixel-exact vs GPU LINE_LIST/POINT_LIST reference.
  `DX7-FILL-MODE.md`. Stale P10/P11/P14/P20/P21/P23 "safe_false" guards now
  assert exposure + device proof doc.
- GS (`DX7-GS.md`): `csf-v11/042` GS on gpu_prerast (poly_nir_lower_gs + libpoly),
  `048` default-on v10+ (`PANVK_DEBUG=no_gs` opts out). Prerast fixes: `044` VS
  const qualifier, `045` compute subqueue is a gpu_prerast consumer whenever the
  arena exists, `046` split draws (never drop GS draws that do not fit one go),
  `047` non-fill polygon mode through the split. Smoke `tests/dxvk/vulkan/geometry`
  23/23 (GEOMETRY_FAILS=0). CTS `^dEQP-VK.geometry.` (with `PANVK_DEBUG=gs`,
  pre-048): 182 Pass / 6 Fail / 11 NotSupported of 199. 6 fails = layered
  cube/cube-array (-> cube fix); 8 instanced cases need
  `maxGeometryShaderInvocations` 64/127, device reports 32 (-> feature bits).
  048 default-on build not yet CTS-rerun on device.
- `csf-v11/043` tiler-heap renewal (`9add58a66ee`): heap was never renewed,
  exhausted after ~66k-71k render passes (hang at seqno 33911). Now 20000x19 =
  380,000 RPs PASS; `submit_stress` 150000x1 PASS (73.6 s, 0 MESA errors).
  `SUBMIT-EXHAUSTION.md`.

DX8:
- Pipeline statistics `csf-v11/049-054` (`DX8-PIPELINE-STATS.md`): 049 query
  result blocking wait, 050 stats queries on CSF, 051 GS + clipping counts in the
  gpu_prerast GS kernel, 052 expose `pipelineStatisticsQuery` v10+, 053 sync +
  complete reports, 054 order copy before reset. CTS
  `^dEQP-VK.query_pool.statistics_query` 14,098 Pass / 0 Fail / 3,671
  NotSupported (17,769). Probe `tests/dxvk/vulkan/pipeline_stats` 12/12
  (PIPELINE_STATS_FAILS=0). A run concurrent with other GPU tests lost the
  device once (not root-caused): run stats in isolation.
- DXVK Native probes (`DX8-NATIVE-WORKLOAD.md`, `tests/dxvk/native/`): D3D9 +
  D3D11 device creation and workload readbacks pass. Presentation still open.
- jica98 adopt (`docs/reviews/JICA98-FORK-REVIEW.md`): worktree `work/mesa-jica98`,
  branch `jica98-adopt`, 8 WIP commits on `d33a343037f`: bf831a90e67 budget cache
  100 ms (0008), 8ea47bfd934 timestamp freq from cntfrq (0001/0002),
  cdb3a3be507 skip texture invalidate for non-texel buffers (018), f811df2629a
  same-queue semaphore waits on GPU (0005; `PANVK_KBASE_CPU_SEMAPHORE_WAITS=1`
  opts out), 49a7443f2a0 SSBO offset align 4 on Valhall (0006), 057f73fc249 ZS
  preload INTERSECT v11 (`PANVK_ZS_PRELOAD_ALWAYS=1` opts out), 87e0e5c2093
  robust SSBO vectorizer behind `PANVK_DEBUG=robust_ssbo_vec` (019),
  52f02bc2718 vectorizer upper-bound check (0003). Built, not device-validated,
  not exported as patches.

In progress (Mesa worktrees):
- Tessellation: `work/mesa-tess` branch `dx8-tess`: 1aa832c4a84 clc split
  struct vars on kernel NIR only, 6801bb23188 GPU tess helper kernels,
  6c7cf3a2e40 build libpoly for tess lowering. Not wired / exposed yet.
- Cube layered: `work/mesa-cube` branch `fix-cube-layered` (target: the 6
  geometry layered cube/cube-array fails). No commits beyond jica98 head yet.
- Feature bits: `work/mesa-feat` (vertexPipelineStoresAndAtomics,
  maxGeometryShaderInvocations, shaderOutputViewportIndex, fp64). Worktree not
  created yet as of 2026-09-30.

APPLY: fresh pin 5a07217f + `apply-patches.sh --profile g615-v11-csf`.
`work/mesa` branch `dx6-dx7-base`: 60ab1b989de (021+022), db594edfc3e (023),
1ffd2ab868b (024), 62fb43f7dc7 (025), 72e29e7ad14..89274dbd817 (026-040),
89274dbd817..d33a343037f (041-054, 14 commits). Other worktrees:
`mesa-prerast` (`dx7-prerast-fix`, 044-048 source), `mesa-pstats`
(`dx8-pipeline-stats`, 050-052 source).

ICD: `95dd86770f9762283ae869b1d88eb6eac4879b566c7a7052c34bd3f9f4d0e42e`
(PAN_ARCH 11, `d33a343037f`).

Harness: `tests/dxvk/vulkan/dx7_harness.h` (direct ICD, 64x64 RGBA readback,
per-pixel expected image).

NEXT:
1. Device runs in `DEVICE-TEST-PLAN.md` order once the device is back.
2. Transform feedback (`tests/dxvk/vulkan/transform-feedback`,
   `tests/dxvk-vkd3d/build-p15-transform-feedback.sh`).
3. Prerast gaps: indirect draws over arena capacity, fans / adjacency strips,
   gl_DrawID on split draws, polygon mode with GS.
4. Finish tess / cube / feature bits; merge `dx8-tess`, `fix-cube-layered`,
   feature bits and `jica98-adopt` onto `dx6-dx7-base`.
5. Export patches `csf-v11/055+`; re-run `apply-patches.sh` on a fresh pin.
6. Real game workloads on device (DXVK D3D9/D3D11).

OPEN: D3D feature level 12 needs sparse binding/residency (unsupported on kbase)
and fragment shader interlock.
