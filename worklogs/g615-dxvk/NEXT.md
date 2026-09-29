# NEXT

TASK: DX7. clip/cull + multiViewport DONE (exposed). fillModeNonSolid, GS next.
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

APPLY: fresh pin 5a07217f + `apply-patches.sh --profile g615-v11-csf` ->
`OK applied=25`, `src/` identical to device tree. `work/mesa` branch
`dx6-dx7-base`: 60ab1b989de (021+022), db594edfc3e (023), 1ffd2ab868b (024).

ICD: `985f71f1ffa3dedae3ffcfec001357ec26f8249cd1dc2ed1dde900d7c26a6d9d`.

Harness: `tests/dxvk/vulkan/dx7_harness.h` (direct ICD, 64x64 RGBA readback,
per-pixel expected image).

NEXT: fillModeNonSolid (no HW polygon mode), GS on gpu_prerast. DX8: BC CTS,
clip/cull CTS, multi-viewport with GS.
