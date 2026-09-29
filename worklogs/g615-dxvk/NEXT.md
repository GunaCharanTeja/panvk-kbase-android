# NEXT

TASK: DX7. clip/cull + multiViewport + fillModeNonSolid DONE (exposed). GS next.
BRANCH: `feature/g615-dxvk-complete`
ADB: `192.168.1.34:41369` serial `Y5WWBMJVOZSK4HU8` duchamp Mali-G615 `/dev/mali0`

DX6: BC1-7 GPU compute decode (`csf-v11/022`), default on since `csf-v11/039`
(`textureCompressionBC=1`; `PANVK_DEBUG=no_bc_emul` opts out). G615 has no HW
BC (BC mask 0). 16/16 device + host verify; CTS BC subset 1863 pass / 0 fail.
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
Open: `memory.zero_initialize_device_memory.image_transition` fails for non-BC
formats too (248/408) -> general zeroInitializeDeviceMemory bug.

DX7:
- `csf-v11/023`: clip/cull distance. VS writes CLIP_DIST0/1 + per-vertex
  cull-negative flags CULL_DIST0/1; FS discards (clip < 0, flag >= 1-2^-16 =
  all vertices negative). Link keyed on slot section. 13/13 IDVS + gpu_prerast.
  `validation/g615-v11-csf/dxvk/DX7-CLIP-CULL.md`.
- `csf-v11/024`: multiViewport, maxViewports 16; viewport index is always 0
  while no stage can write ViewportIndex. 5/5. `DX7-MULTIVIEWPORT.md`.
  Revisit with GS (DCD "Scissor array enable").
- `csf-v11/025`: fillModeNonSolid. Non-FILL pipelines get GPU_POLYGON compute
  kernel on gpu_prerast (assemble list/strip/fan+restart, det(x,y,w) facing,
  cull, emit line/point indices + indirect draw); passthrough draws
  LINES/POINTS. 17/17 pixel-exact vs GPU LINE_LIST/POINT_LIST reference.
  `DX7-FILL-MODE.md`. Stale P10/P11/P14/P20/P21/P23 "safe_false" guards now
  assert exposure + device proof doc.

APPLY: fresh pin 5a07217f + `apply-patches.sh --profile g615-v11-csf` ->
`OK applied=41`, `src/` identical to work tree. `work/mesa` branch
`dx6-dx7-base`: 60ab1b989de (021+022), db594edfc3e (023), 1ffd2ab868b (024),
62fb43f7dc7 (025), 72e29e7ad14..89274dbd817 (026-040).

ICD: `6731e2ec70b8763dbefbf0f8c70ba5279d34e65691aa95b0769f269e2b26aa32` (PAN_ARCH 11).

Harness: `tests/dxvk/vulkan/dx7_harness.h` (direct ICD, 64x64 RGBA readback,
per-pixel expected image).

NEXT: GS on gpu_prerast (poly_nir_lower_gs + libpoly; honk hk_cmd_draw.c
reference). DX8: BC CTS, clip/cull CTS, polygon-mode CTS, multi-viewport with
GS, fill-mode for multi-draw/indirect-count, dynamic polygon mode.
