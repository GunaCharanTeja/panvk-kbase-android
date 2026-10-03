# 091 depthBounds

Status: **blocked, not implemented**. `depthBounds` stays off. No patch 091.

## Why

Vulkan depth bounds compare the depth value already stored in the depth attachment against [min, max]. The fragment is discarded if the stored value is outside.

- Hardware: the Valhall/v10 and v12-v14 genxml Depth/stencil descriptor (ZSD/DCD) has no depth-bounds fields. Only depth clamp mode/source, cull, bias and compare function exist. No CSF register or tiler setting exists for it either.
- FS fallback: needs the stored depth. The Valhall ISA has ZS_EMIT (write only) and LD_TILE for colour only, so a fragment shader cannot read tile-resident Z. Reading the depth attachment as an input attachment or image is not possible for a depth buffer that is also the active ZS target.
- Testing the incoming fragment depth instead is a different test (it is depth clamp/clip), so it would give wrong results. Not a valid fallback.
- An exact emulation (copy depth to a sampled image per draw, sample in the FS, and order draws) needs a per-draw depth copy and fragment serialisation. That is a large redesign of the render-pass and tile flow, not a driver patch of this size.

## Decision

Do not advertise `depthBounds`. `vkCmdSetDepthBounds` and the dynamic/static state stay unimplemented. Revisit if a hardware control or a tile-Z load is found.
