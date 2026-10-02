# Gate 088 device verification

## Valid run: PID 2723

Stamp `validation/driver-remaining/088-device/stamp-2723.txt` was written while the process was `State: S (sleeping)` and the log still ended at `READY`. Gate `/tmp/088-go-2723` was created only after that.

- Uptime at stamp: `236197.57`. `UPTIME_AT_GO 236198.06` (0.49 s later)
- Tgid 2723, Pid 2723, PPid 2720
- Maps: `72b21c0000-72b35ff000 r-xp` inode 4467661
- ICD sha256 `0457150b935d38ad8faab4db7a12088acb38998cf8fb30130320164934b98dc4`
- ICD Build ID `2afe54d4f472dc9ce2563e0fcc17becd45040f7d`
- Binary sha256 `aa2a1ba5b9c4fc980bd50e01c0d6ce341a38c323c59f880906c9ad208c83ac17`
- `LD_LIBRARY_PATH=/tmp/val-085087088/icd`, device `192.168.1.34:40501`
- Source `device/gs-tess-primitive-id.c` `32b0d44d11cc8bdac3cb51a25351da04a9453341b74d631a8448d339d0719cfc`

Every allocation uses a type set in that resource's `memoryTypeBits` (`0x7`). Device-local SSBO is type 0 (`flags=0x1`). Host readback, both indirect buffers, the vertex buffer, and the color image are type 1 (`flags=0x7`).

Same recorded command buffer is submitted twice for direct, indirect (2 patches × 2 instances), and the 600-patch direct draw. A separate recorded indirect draw is 600 patches × 1 instance (arena crossing). Full readback is `ids-<case>.bin`: uint32 count, then every ID.

| case | n | histogram | sha256 |
|---|---|---|---|
| direct | 24 | ID 0,1 ×12 | `940090d966001d7539aadd8efded926a1a48b5d3900cbbb3693e34273640d7a3` |
| direct-replay | 24 | same | `41e53d43d6f843b7c87393d5240d4dd714f04146f512ae70b39a87066b90cfcd` |
| indirect | 24 | same | `16847e7c4ecd01a8eb07bb364f44b67c912b388418020473ef888c0e5336ef55` |
| indirect-replay | 24 | same | `22a96f1fba62e14eded2cc062370441deb58ff2c4346adea458701d1367806aa` |
| chunk | 3600 | ID 0..599 ×6 | `b04e54fe992e21a0a594a2841c5d254f891a951e13aa222542e5569be90cefed` |
| chunk-replay | 3600 | same | `4b59c09fa14628fed3e3c2742a90750d10618d9a34ed60669bf62430f6c0ef53` |
| indirect-chunk | 3600 | same | `c6f095c83ab2a0e3d5d7615d83670184b8be356d51e4da25da99a91dbb6c2565` |

`RESULT PASS`. Log: `validation/driver-remaining/088-device/run.log`.

Untested, not in this device-gate scope: an old-ICD negative control. PID 788 is withdrawn; it ran the previous binary and did not replay indirect or the chunk, and did not check `memoryTypeBits`.
