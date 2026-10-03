# Architectural Blocker: variableMultisampleRate Across Secondary Command Buffers

Status: Unsolved / Blocked (Patch 086 rejected and removed; no claimed solution)  
Target: PanVK CSF (PAN_ARCH >= 10, Mali-G615)  
Reference: Review086 blocker analysis

## Executive Summary

`variableMultisampleRate` allows `rasterizationSamples` to vary dynamically within a dynamic rendering pass when no attachments are bound. Patch 074 enabled this for primary command buffers by calling `split_render_ctx()` on sample count transitions. An attempt (Review086 / patch 086) to extend this across secondary command buffers (`VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT`) and resumed render passes (`VK_RENDERING_RESUMING_BIT`) introduced fatal architectural defects and has been removed.

Supporting variable multisample rates across secondary command buffers is blocked by three fundamental architectural constraints in PanVK CSF.

---

## Concrete Architectural Blockers and Evidence

### Blocker 1: Bypassing `cmd_select_tile_size` Leaves `tile_size_px = 0`

- **Mechanism**: In `panvk_vX_cmd_draw.c`, `panvk_per_arch(cmd_select_tile_size)` contains:
  ```c
  if (fb->sample_count == 0) {
     fb->sample_count = render->fb.nr_samples;
     GENX(pan_select_fb_tile_size)(fb);
     ...
  } else {
     assert(fb->sample_count == render->fb.nr_samples);
  }
  ```
- **Failure**: Manually assigning `fb->sample_count = rasterization_samples` prior to `get_render_ctx()` causes `cmd_select_tile_size()` to take the `else` branch. As a result, `GENX(pan_select_fb_tile_size)(fb)` is bypassed.
- **Evidence**: `fb->layout.tile_size_px` remains 0. When hardware registers and framebuffer descriptors (FBD) are emitted with `tile_size_px == 0`, Mali CSF hardware receives invalid tile dimensions, producing GPU faults or corrupted binning.

### Blocker 2: Mixed Sample Counts in a Single Secondary Command Buffer

- **Mechanism**: Secondary command buffers are recorded out-of-line into monolithic CS builder streams (`panvk_get_cs_builder(secondary, j)`). At primary execution time (`vkCmdExecuteCommands`), the primary invokes the secondary CS stream via `cs_call(prim_b, addr, size)`.
- **Failure**: If a single secondary command buffer contains draws with varying sample rates (e.g., Draw 1 @ 2x, Draw 2 @ 4x):
  1. The secondary has no primary render context during recording and cannot execute CPU `split_render_ctx()`.
  2. Recording-time CPU metadata collapses to a single scalar (`secondary->state.gfx.render.fb.layout.sample_count`), reflecting only the final draw.
  3. When executed, the primary applies this scalar to the entire secondary invocation. Prior draws (Draw 1) execute with an incorrect sample count context.
  4. Because `cs_call` executes the entire recorded secondary CS stream atomically on the GPU, the host driver cannot insert `split_render_ctx()` (which flushes tiling, dispatches fragment jobs, and reallocates tiler descriptors on the CPU) at intra-secondary draw boundaries.
- **Evidence**: Preserving ordered sample segments requires segmenting secondary command buffer recording into discrete CS streams at sample rate changes and emitting dynamic execution transitions at segment boundaries—an abstraction PanVK CSF does not possess.

### Blocker 3: Resumed Primary `tiler == 0` Incurs Colliding Context Without Flush

- **Mechanism**: Dynamic rendering with `VK_RENDERING_RESUMING_BIT` inherits GPU execution state across command buffer boundaries (`inherits_render_ctx(primary) == true`). On the host CPU, `primary->state.gfx.render.tiler` is 0 because the tiler descriptor was emitted into GPU registers by the suspending command buffer.
- **Failure**:
  1. Checking `!primary->state.gfx.render.tiler` in `cmd_prepare_exec_cmd_for_draws()` evaluates to true for a resumed primary.
  2. Invoking `get_render_ctx(primary)` allocates and binds a new tiler descriptor and FBD without an explicit GPU transition or flush of the inherited GPU context.
  3. The resumed primary's CPU metadata is 0 (`fb.layout.sample_count == 0`), making it impossible for the CPU to verify whether the inherited GPU context matches the secondary's sample count.
- **Evidence**: Overwriting active GPU tiler registers without an explicit GPU flush or transition breaks the CSF execution model and causes state desynchronization between CPU metadata and GPU register state.

---

## Resolution

- **Patch 086**: Fully removed from `patches/csf-v11/` and reverted on `work/mesa-secondary-msaa`.
- **Status**: Retain no claimed solution. `variableMultisampleRate` remains supported strictly within primary command buffers without attachments (patch 074).
- **Prerequisites for Future Work**:
  1. Secondary command buffer segmentation infrastructure to split and chain CS chunks at sample rate changes.
  2. In-band GPU transition/flush protocol for inherited dynamic rendering contexts (`VK_RENDERING_RESUMING_BIT`).
  3. Correct dynamic tile-size selection integration in `cmd_select_tile_size` for attachmentless variable-rate passes.
