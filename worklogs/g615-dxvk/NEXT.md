# NEXT

TASK: DX6 BC1-BC7 GPU decode. DONE (dev-gated). DX7 next.
BRANCH: `feature/g615-dxvk-complete`
ADB: `192.168.1.34:41369` serial `Y5WWBMJVOZSK4HU8` duchamp Mali-G615 `/dev/mali0`

DX6: HW has no BC (`TEXTURE_FEATURES_0=0xc7fe001e`). `csf-v11/022`: raw BC plane +
decoded plane, vk_meta compute decode after every copy into a BC image, views
redirect to decoded plane. `PANVK_DEBUG=bc_emul` exposes `textureCompressionBC`
+ 16 formats. Device: 16/16 raw/copy/blit PASS, host-reference verify 16/16
(BC6H/BC7 bit-exact vs Mesa CPU BPTC). See
`validation/g615-v11-csf/dxvk/DX6-BC-GPU-DECODE.md`.

FIX: `csf-v11/021`: every `vkCmdBlitImage` SIGSEGV (018 built passthrough VS for
all VS; store base = location). Variants gated on `PANVK_DEBUG=gpu_prerast`;
base = layout slot. DX5 matrix still IDVS 13/13, gpu_prerast 13/13.

APPLY: fresh pin 5a07217f + `apply-patches.sh --profile g615-v11-csf` -> `OK applied=23`,
tree identical to device tree. `work/mesa` branch `dx6-dx7-base` = pin+001..020
commit `dea61483d0f` (local only); stale pre-DX6 work/mesa diff saved in scratch.

ICD: `9603547a2998e2b9849702c232133a2020b71ed40017c3bd5edfc4737818e9d3`.

NEXT: DX7 GS on gpu_prerast path, clip/cull, fillModeNonSolid, multiViewport,
each with device draw tests. DX8: BC CTS (no skips) before default exposure.
