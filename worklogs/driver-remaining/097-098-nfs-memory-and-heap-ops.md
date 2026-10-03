# 097/098: NFS:MW memory blow-up and DEVICE_LOST (beta.11)

Game: Need for Speed Most Wanted, i686 D3D9 under WoW64 (Proton 11 arm64ec,
FEX, DXVK 3.1.1), PanPlay 1.0.2 with beta.10.

## DEVICE_LOST (098)

dmesg on every launch showed kbase rejecting the tiler heap stats on OOM with
`vt_start 1, vt_end 0, frag_end 16` (frag_end > vt_end gives EINVAL), then
terminating the group. panvk emitted VERTEX_TILER_COMPLETED and
FRAGMENT_COMPLETED heap operations whose counts did not match the way kbase
accounts them. On kbase, panvk now emits only VERTEX_TILER_STARTED. Chunks are
reclaimed by the per-queue heap renewal from 043. After the fix: no
"Terminate ctx" lines in 10+ minutes of gameplay.

## Memory (097)

- TLS: each command pool's `tls_pool` allocated a BO sized for the largest
  shader's stack (84 MiB in NFS:MW). DXVK creates many pools, so about 20 such
  BOs built up. Mali TLS is indexed by core and thread slot, so one BO can be
  shared. TLS is now a device-wide BO list: the top BO is reused while large
  enough; a bigger one is pushed when needed, and older ones stay alive until
  device destroy.
- GPU prerast arenas: 480 MB committed at device creation. They are now
  `ALLOC_ON_FAULT` kbase regions (2 MB growth, first 2 MB committed for CPU
  header writes).
- DXVK creates three VkDevices in NFS:MW (two fail "Cannot create texture"),
  which multiplied the arena cost.

| | beta.10 | beta.11 |
|---|---|---|
| RSS idle/intro | 1.2 GB | 0.75 GB |
| RSS gameplay | 2.0 -> 3.7 -> 4.1 GB+, MemAvailable 0 | 2.02-2.10 GB flat, 10 min |
| DEVICE_LOST | every launch | none |

## FPS (HUD full)

- FEX default preset: race 35 fps. FEX Extreme: intro 86, race 40-44,
  results 55. User field data: real races 15-20 fps (default) -> 30-50 fps
  (Extreme).
- GPU: GED busy 0-11% at 265 MHz (lowest OPP). Main thread pegged; the
  dxvk-submit thread sleeps in `do_sys_poll` because kbase submission is
  synchronous. Async submission is the next step (not in beta.11).

Evidence: `validation/driver-remaining/097-device/`.
