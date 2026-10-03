# 095 variableMultisampleRate in secondary command buffers

Patch: `patches/csf-v11/095-give-attachment-less-secondaries-a-context-with-their-sample-count.patch`. 086 stays unused.

## Problem

A secondary command buffer is one CS stream and cannot change the render context. Attachment-less secondaries (UAV-only passes) with a sample count different from the primary's context ran with the wrong count.

## Change

`prepare_draw` records `vmr_samples` (and `vmr_mixed` when a secondary uses more than one count) for attachment-less draws. At `vkCmdExecuteCommands` the primary splits its render context (`split_render_ctx`) and sets `nr_samples` from the secondary's count. `cmd_inherit_render_state` resets the tracking.

## Test

APK `vmr_secondary` (`tests/dxvk/vulkan/vmr-secondary`): primary and secondary with 1, 2, 4 and 8 samples in several orders, attachment-less, written to a storage buffer. 8/8 cases pass on the 095 build (3 runs). `autorun all` 16/16. CTS with 093-095: 17067 pass / 0 fail, same as baseline (multisample/variable rate, secondary and no-attachment groups included).

## Limits

- Mixed sample counts inside one secondary: the first count is used and a warning is logged.
- Contexts resumed across command buffers: the sample count is unknown there and unchanged.
- v10/v11 (existing VMR gating); v12+ keeps the bit off.
