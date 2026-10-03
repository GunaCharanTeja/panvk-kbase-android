# 090 VS/TES ViewportIndex

Status: **verified on the device** (Mali-G615 MC6, panvk-test APK). Not run under CTS. Details: `090-device-verification.md`.

Patch: `patches/csf-v11/090-select-viewports-for-vs-and-tes-viewportindex-writes.patch`. `shaderOutputViewportIndex` is now on for v10 and v11 (`PAN_ARCH >= 10 && < 12`). It stays off on v12+ because run splitting is disabled there (the depth clamp is packed in the viewport register).

## Approach

- A VS or TES (last pre-raster stage, no GS) that writes ViewportIndex gets the gpu_prerast variants. The new `panvk_gpu_prerast_vs_vp_passthrough_nir` (`panvk_vX_shader.c`) reads the lowered-VS records, applies the selected viewport transform, and stores the scissor planes in a free clip slot (`vp_scissor_loc`). The FS discards on those planes, as for the GS path.
- The draw is split into ordered viewport runs with the 089 kernel `panlib_vp_runs` (`libpan/vp_runs.cl`), now instance aware (`per_instance`, records of instance k at k*n). Each run gets its viewport's LOW/HIGH_DEPTH_CLAMP (`csf/panvk_vX_cmd_draw.c`, `gpu_prerast_vp_prepare` and `gpu_prerast_draw_generated`). Tess-fed draws build the run list inside the compute loop (nested prerast).
- Arena: the gpu_prerast arena was allocated only for fillModeNonSolid, geometryShader, xfb or vertexPipelineStoresAndAtomics. A device with only shaderOutputViewportIndex had no arena and the first draw hung the GPU. `panvk_vX_device.c` now also allocates it for shaderOutputViewportIndex.
- Fixed in the same patch: the push-uniform dirty mask used the GS passthrough variant index for a VS/TES shader (`panvk_vX_cmd_draw.c`).
- Review (codex gpt-6.1-sol): fixed the record-capacity check for instanced runs and gated the feature to v10/v11.

## Limits

- `gl_PrimitiveID` in the FS restarts at each viewport run, and instances are flattened in the run list. Not covered by the test.
- Polygon-mode draws use the union depth range (no run list).
- v12-v14: feature off.
- Not CTS tested (dynamic_state/draw shader_viewport_index not run).
