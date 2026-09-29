# NEXT

TASK: DX7. clip/cull + multiViewport + fillModeNonSolid DONE (exposed). GS next.
BRANCH: `feature/g615-dxvk-complete`
ADB: `192.168.1.34:41369` serial `Y5WWBMJVOZSK4HU8` duchamp Mali-G615 `/dev/mali0`

DX6: BC1-7 GPU compute decode, dev-gated `PANVK_DEBUG=bc_emul` (`csf-v11/022`),
16/16 device + host-reference verify. See `validation/g615-v11-csf/dxvk/DX6-BC-GPU-DECODE.md`.

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
`OK applied=26`, `src/` identical to work tree. `work/mesa` branch
`dx6-dx7-base`: 60ab1b989de (021+022), db594edfc3e (023), 1ffd2ab868b (024),
62fb43f7dc7 (025).

ICD: `9f1a5a57ad417dc5b36763378d2d75f183e8116c124989c7ebfaac17bd1a5a49`.

Harness: `tests/dxvk/vulkan/dx7_harness.h` (direct ICD, 64x64 RGBA readback,
per-pixel expected image).

NEXT: GS on gpu_prerast (poly_nir_lower_gs + libpoly; honk hk_cmd_draw.c
reference). DX8: BC CTS, clip/cull CTS, polygon-mode CTS, multi-viewport with
GS, fill-mode for multi-draw/indirect-count, dynamic polygon mode.
