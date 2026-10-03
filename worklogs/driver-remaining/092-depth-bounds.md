# 092 depthBounds

Status: **verified on the device** (Mali-G615 MC6, panvk-test APK). Details: `092-device-verification.md`. CTS (2059 depth-bounds cases): 1774 pass, 0 fail, 285 NotSupported. Number 091 is used by another series patch, so this is 092. (The earlier "blocked" note was wrong: the FS can read the stored depth with LD_TILE.)

Patch: `patches/csf-v11/092-emulate-the-depth-bounds-test-in-the-fragment-shader.patch`. `depthBounds` is on for v10 and v11 (`PAN_ARCH >= 10 && < 12`, `panvk_vX_physical_device.c:326`).

## Approach

The hardware has no depth-bounds test (no ZSD/DCD field, v10.xml). The FS emulates it.

- `panvk_depth_bounds_lowered` (`panvk_vX_shader.c:2328`): the FS is lowered when the feature is enabled and the pipeline has the test statically on or `DEPTH_BOUNDS_TEST_ENABLE` dynamic. Shader objects (no state) are always lowered. The result is in the shader hash.
- `panvk_lower_depth_bounds` (`panvk_vX_shader.c:2346`): at the end of the FS, loop over the samples, load the stored depth of each with `load_tile_pan` (RT 255, float32, conversion 0, as the input-attachment lowering does), and clear the bit of each sample outside [min, max] (inclusive). The result goes to `gl_SampleMask`. An app `gl_SampleMask` becomes a temporary and is ANDed in. The coverage goes through ATEST/ZS_EMIT/blend as usual, so failing samples write no color, depth or stencil.
- The loop index is uniform (0 to the sample count). A first version looped over each lane's own `SampleMaskIn` bits. That gave wrong depths at pixels split between two triangles (3 bad pixels of 64 partial ones in the 4x test). LD_TILE needs the same sample index in all lanes.
- Sysvals (`panvk_vX_cmd_draw.c:990`): `ds.bounds`, `ds.enable` (test on and a depth attachment bound) and `ds.samples`, from `vk_dynamic_graphics_state`. So static state, `vkCmdSetDepthBounds` and `vkCmdSetDepthBoundsTestEnable` all use one variant and a uniform branch.
- No fragment shader: the device compiles one empty FS with this lowering (`create_depth_bounds_fs`, `panvk_vX_shader.c`, created in `panvk_vX_device.c`) and binds it when a pipeline or shader-object set has no FS. `fs_required` (`panvk_cmd_draw.h`) runs it only while the test is on and a depth attachment is bound, so depth-only draws keep running without an FS otherwise. Found by CTS: all 684 failures of the first run were `pipeline.*.depth.nocolor.*` (no FS).
- Draw (`csf/panvk_vX_cmd_draw.c:2613`): the DCD declares the shader depth read (zs_read for early-ZS, `no_shader_depth_read` on v11) only while `ds.enable` would be 1. This keeps the tile read ordered and the ZS update late when the test is on, and avoids it when it is off. DCD2 is now also rebuilt on `DS_DEPTH_BOUNDS_TEST_ENABLE` and render-state changes.

## Limits

- Cost when lowered but off: the FS always writes SampleMask, so it cannot use FPK, and depth-writing draws get a late ZS update. DXVK makes depth-bounds-enable dynamic, so all its pipelines pay this once the feature is enabled.
- Cost when on: one LD_TILE per sample per invocation.
- EarlyFragmentTests shaders are not changed. Their ZS update happens before the FS, so with depth writes on the test sees the fragment's new depth, not the old one. Correct when depth writes are off.
- v12-v14: feature off (not tested there).
