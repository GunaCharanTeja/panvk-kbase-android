# Gate 087 device verification

PID 32320 is withdrawn and invalid. Its stamp uptime was 9 s after `T go` (`CLOCK_MONOTONIC` and `/proc/uptime` differ, and the stamp shell ran late). Not evidence. PID 32589 supersedes 32320.

## Valid run: PID 32589

Stamp `validation/driver-remaining/087-device/stamp-32589.txt` was written while the process was `State: S (sleeping)` and `run.log` still ended at `READY` (no `T go` yet). Gate file `/tmp/087-go-32589` was created only after that.

- Uptime at stamp: `234763.86`
- Uptime printed by the test at release: `UPTIME_AT_GO 234763.95` (0.09 s later)
- `T go` `234754.831694578` is `CLOCK_MONOTONIC`, not uptime
- Tgid 32589, Pid 32589, PPid 32587
- Maps: `7860000000-786143f000 r-xp` `libvulkan_panfrost.so` inode 4467661
- ICD sha256 `0457150b935d38ad8faab4db7a12088acb38998cf8fb30130320164934b98dc4`
- Binary sha256 `a4460a73a7997ba258c8872b903e6be070ace963e1ec53f5a6af5eca7a4e502d`
- `LD_LIBRARY_PATH=/tmp/val-085087088/icd`

Queried and enabled, Vulkan 1.3 `synchronization2` only (no separate `VkPhysicalDeviceSynchronization2Features`, VUID-06532): `tessellationShader=1`, `vertexPipelineStoresAndAtomics=1`, `conditionalRendering=1`, `inheritedConditionalRendering=1`, `synchronization2=1`, `bufferDeviceAddress=1`. Dynamic rendering is not used.

Harness fixes versus the withdrawn run: HOST→TCS barrier is before `vkCmdBeginRenderPass` (VUID-01178). Image layout is `UNDEFINED` only as the render pass `initialLayout`; after the copy it returns to `COLOR_ATTACHMENT_OPTIMAL`, never `newLayout=UNDEFINED`. Counter is device-local with `TRANSFER_SRC|TRANSFER_DST`. Readback is `counter_rb` after a TCS-write→transfer-read barrier, the copy, then a transfer-write→host-read barrier. No validation layer is installed in the chroot.

Isolated non-tess triangle, own command buffer: `DIAG plain pixel 255 0 0 255 red=1024`.

TCS `atomicAdd`, one 3-vertex patch per draw. Two conditional draws plus one unconditional: skip 3, pass 9. Sequence is predicate 0,1,0 on the same `SIMULTANEOUS_USE` buffer. Red stays 1024 on a skip because the unconditional triangle covers the 32x32 target. The counter is the skip proof. `RAW` is the copied word.

| case | pred 0,1,0 counters |
|---|---|
| direct | 3, 9, 3 |
| indirect | 3, 9, 3 |
| inverted | 9, 3, 9 |
| inherited secondary | 3, 9, 3 |
| direct replay (fourth submit, pred 1) | 9 |

Timestamp on the third direct submit: `t0=3052000817036 t1=3052000836127`. Fences completed. `RESULT PASS`. Log: `validation/driver-remaining/087-device/run.log`.

Source sha256:

- `device/tess-conditional-regression.c` `e5e5b80b02fd6049caecb62dd228f79ff3bbc5c63941c2399ab5322146f1ae11`
- `device/tess-conditional.vert` `a1446467541cb3409100480c2702f1ebd414257ecc02494ecd363053e6316dc4`
- `device/tess-conditional.tesc` `6dca28f7123bd951395c81384160df817564a37bd550f2652bd423170ec6923b`
- `device/tess-conditional.tese` `109a4ccc0ba213f8579e5c5c1e35a8c09d601364696d4442ae7aaf9fa3c4f06c`
- `device/tess-conditional.frag` `d15b8fbbcf90a6f512955028b2556cfc1919060524e8357d7b70cc03d2ead8aa`

SPIR-V sha256 (embedded in the harness, not separate headers): vert `cc7376a59eb2e42f48dab00c444e27ebdf0263a3bb2978e83323810d3a0aff01`, tesc `88838e5a1511cd51f4f499211fff64a2de8113fcc2f2b3fa422fd6cb25d466c6`, tese `70a9129524904c8613cc91a69a40cefaff72bcff21cef0db056c4cd8ea2f752e`, frag `8ce7c482d6db3c2f0c44bef93958f2f00c73caa5e77ab13ca7920636d444d6dc`.

Scope: one G615 replay of tessellation conditional rendering. Concurrency is not tested. This is not full conformance.
