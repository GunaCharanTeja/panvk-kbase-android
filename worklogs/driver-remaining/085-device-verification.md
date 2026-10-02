# Gate 085 device verification

Verdict: **PASS** for q0 only. Events never cross queues. Host-notify eviction is **UNPROVEN**. This is not a full CSF-event closeout.

## Stamp, this run, before the fifo release

`READY` printed PID `30759`. Stamp `validation/driver-remaining/085-device/stamp-30759.ts` is `1790968266.241772305`. Fifo release `validation/driver-remaining/085-device/release-30759.ts` is `1790968266.512469291`.

Driver identity is `validation/driver-remaining/085-device/stamp-30759.txt`: Tgid `30759`, Pid `30759`, PPid `30754`. `/proc/30759/maps`:

```
71a94d0000-71aa90f000 r-xp ... /data/local/tmp/chrootAlpine/tmp/val-085087088/icd/libvulkan_panfrost.so
```

`LD_LIBRARY_PATH=/tmp/val-085087088/icd`. SHA256 `0457150b935d38ad8faab4db7a12088acb38998cf8fb30130320164934b98dc4`. BuildID `2afe54d4f472dc9ce2563e0fcc17becd45040f7d`. The run log `validation/driver-remaining/085-device/valid4.log` starts `PID 30759`. Same process. Failed attempts are not kept.

Mali-G615 MC6, vendor `0x13b5`, device `0xb8a31030`. `synchronization2=1`, enabled in `VkDeviceCreateInfo.pNext`. No launcher, vendor, or debugfs writes.

## Cases, q0 only

| Case | Order | Result |
|---|---|---|
| host | `vkSetEvent` after the host write, before submit. Compute reads the word | `0x085000a1` |
| host replay | `vkResetEvent`, new value, after the first fence | `0x085000a2` |
| gpu | `CmdSetEvent2` submitted on q0 and its fence waited, `GetEventStatus` = SET (3), then `CmdWaitEvents2` on q0. Both `COMPUTE_SHADER`, same buffer barrier | `0x085000b1` |
| gpu replay | `CmdResetEvent2` (COMPUTE), then `vkCmdPipelineBarrier2` ALL_COMMANDS to ALL_COMMANDS with no access flags, then the dispatch and `CmdSetEvent2`. After the first completion | `0x085000b2`, status 3 again |
| ring | 1700 `SIMULTANEOUS_USE` draws, readback after each submit | submit 0 red 64/64, submit 1 red 64/64 |

`RESULT PASS`, exit 0.

## Ring

512 KB, allocated only for `SIMULTANEOUS_USE`. v11 packed: framebuffer 128, render target 64, tiler context 128. One color draw reserves 320 bytes. 1700 draws is 544000 bytes, past 524288, so the pointer wraps inside the command buffer.

The counter is released per render, not per submit. `cs_sync32_add` of `calc_render_descs_size` runs in the fragment finish, once per render pass (`panvk_vX_cmd_draw.c`). Earlier text saying the counter refills when the fragment subqueue finishes a submit was wrong.

Shaders: `tests/offscreen/tri.vert.spv.h`, `tri.frag.spv.h`, unchanged. The copy shader is inlined. Rebuild from the repo root (NDK r30, API 35):

```
aarch64-linux-android35-clang -std=c11 -O2 -Wall -Wextra -Werror \
  device/csf-event-regression.c -ldl -o csf-event-regression
```

## Not claimed

`HOST` and `TOP_OF_PIPE` are absent from `panvk_get_subqueue_stages`, so a wait with only those stages emits no `SYNC_WAIT`. A host signal issued after the wait has been submitted is VUID-vkCmdWaitEvents2-pEvents-03839/03840/03841.

Host-notify eviction is **UNPROVEN**. Nothing here shows `KBASE_IOCTL_CS_EVENT_SIGNAL` resuming an evicted group. The PASS above is not that result.
