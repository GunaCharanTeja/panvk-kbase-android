# DX7 clip/cull distance

Status: exposed by default on v10+ (`shaderClipDistance=1`, `shaderCullDistance=1`,
`maxClipDistances=maxCullDistances=maxCombinedClipAndCullDistances=8`).
Patch: `patches/csf-v11/023-clip-cull-distance-fs-discard.patch`.

## Mechanism (GPU only)

Valhall has no clip/cull distance hardware.
- VS (preprocess): the compact `ClipDistance`/`CullDistance` arrays (separate,
  combined, or cull packed after clip at `CLIP_DIST1`) become temporaries.
  New vec4 outputs: `CLIP_DIST0/1` = clip distances (unused lanes +1.0),
  `CULL_DIST0/1` = per-vertex flag `cull[i] < 0 ? 1.0 : 0.0` (unused lanes 0).
- FS: discard if any interpolated clip distance < 0 (same as
  `nir_lower_clip_fs`), or any interpolated cull flag >= 1 - 2^-16. Barycentric
  weights are convex, so the flag reaches 1.0 only if every vertex of the
  primitive had that cull distance negative: whole-primitive cull, not a
  per-pixel test. Linked FS reads only slots the VS writes; missing slots read
  constant 0 (no discard).
- Link fix: varying attribute emission used `location < VAR0` for "hardware
  slot"; CLIP/CULL_DIST are generic slots below VAR0 and got offset 0. Now keyed
  on the slot section.
- Fork 187662d (`nir_lower_clip_cull_distance_to_vec4s` + `nir_lower_clip_fs`)
  not taken: treats cull like clip (per-pixel), wrong for partially negative
  primitives, and hit the link bug above.

## Device proof (2026-09-29)

ADB `192.168.1.34:41369`, patches 001-024 (`OK applied=25` on fresh pin
`5a07217f`, `src/` identical to device tree). ICD sha256
`985f71f1ffa3dedae3ffcfec001357ec26f8249cd1dc2ed1dde900d7c26a6d9d`.
Test: `tests/dxvk/vulkan/clip-cull/clip_cull.c` (64x64 RGBA8, every pixel
compared with the expected image; GLSL in the same directory,
`build_spv.sh` regenerates `clip_cull_spv.h`). `cull*` right-half triangle has
cull = -1, -1, +1: a per-pixel discard would lose most of it.

```text
ICD device=Mali-G615 MC6 geometryShader=0 fillModeNonSolid=0 multiViewport=1 shaderClipDistance=1 shaderCullDistance=1 maxViewports=16 maxClip=8 maxCull=8 maxCombined=8
CASE none red=4096 mismatch=0 PASS
CASE none_cullgeo red=4096 mismatch=0 PASS
CASE clip1_x red=2048 mismatch=0 PASS
CASE clip2_xy red=1024 mismatch=0 PASS
CASE clip6_x5 red=2048 mismatch=0 PASS
CASE clip8_y7 red=2048 mismatch=0 PASS
CASE clip1_fs_read red=2048 mismatch=0 PASS
CASE cull1 red=2048 mismatch=0 PASS
CASE cull5_i4 red=2048 mismatch=0 PASS
CASE cull8_i7 red=2048 mismatch=0 PASS
CASE clip4y3_cull4i2 red=1024 mismatch=0 PASS
CASE clip1_x_lines red=64 mismatch=0 PASS
CASE cull1_lines red=64 mismatch=0 PASS
CLIP_CULL_FAILS=0
```

Identical 13/13 with `PANVK_DEBUG=gpu_prerast`. Regression: `gpu_prerast_slice`
IDVS 13/13 + gpu_prerast 13/13 (`MATRIX_FAILS=0` both; its exposure check now
only rejects `geometryShader`), DX6 `BC_DEVICE_FAILS=0`. dmesg: no fault.

## Limits / not proven

- FS `CullDistance` inputs read 0 (only the flag is forwarded).
- Cull edge band: pixels within 2^-16 barycentric of the edge opposite the only
  non-negative vertex can be dropped.
- FS compiled without its VS (GPL/shader objects) always reads the 4 slots and
  always has discard (early-ZS loss); pipelines link VS+FS and avoid it.
- Points not tested. CTS `dEQP-VK.clipping.user_defined.*` not run.
