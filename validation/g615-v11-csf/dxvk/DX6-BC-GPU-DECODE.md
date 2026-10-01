# DX6 BC1-BC7 GPU decode

Status: `GPU_LOWERED`, default on since `csf-v11/039` (2026-09-29).
`textureCompressionBC=1` by default when the GPU has no BC texturing;
`PANVK_DEBUG=no_bc_emul` opts out (`textureCompressionBC=0`, fmt_fail=16).
`bc_emul` = vk_meta compute SPIR-V decode into a `bc_decoded` plane. No CPU decode.
See "Default exposure proof" at the end; earlier sections are the 022 dev-gated history.

## Hardware

`TEXTURE_FEATURES_0=0xc7fe001e`, BC mask `0x0001ff80` -> `BC_SUPPORTED_MASK=0x00000000`
(`validation/g615-v11-csf/beta3-final-kbase-texture-features-2026-09-19.txt`).
ETC2/ASTC native; no BC texturing. Native path impossible -> GPU compute decode.

## Mechanism (patches/csf-v11/022-bc-gpu-decode-emulation.patch)

- BC image: `planes[0]` raw blocks (LINEAR) + `bc_decoded` plane (LINEAR) in one
  allocation. Decoded formats: BC1/2/3/7 -> RGBA8 (UNORM/SRGB per view),
  BC4/BC5 -> RG16 UNORM/SNORM, BC6H -> RGBA16F.
- Copy buffer/memory->image and image->image into a BC image: raw copy, full
  barrier, then vk_meta compute dispatch (SPIR-V, `src/panfrost/vulkan/bc/`)
  per destination region. Decoder sources derived from Granite/bcn_layer (MIT).
- Sampled BC views -> decoded plane; BC1_RGB forces A=1 by swizzle.
  Image->buffer reads raw blocks. Blit samples decoded plane.
- Features (optimal only): SAMPLED, FILTER_LINEAR, FILTER_MINMAX, BLIT_SRC,
  TRANSFER_SRC, TRANSFER_DST. Rejected: linear, DRM modifier, host image copy,
  storage, attachments, sparse, disjoint, 1D.
- No CPU decode anywhere in the driver. CPU only records and submits.

Fix found on the way (`patches/csf-v11/021-gpu-prerast-variant-gate-passthrough-base.patch`):
any `vkCmdBlitImage` SIGSEGV'd in `pan_nir_lower_vs_outputs` because 018 built
GPU_LOWERED/PASSTHROUGH VS variants for every VS and the passthrough used the
varying location as layout index. Variants now gated on `PANVK_DEBUG=gpu_prerast`;
passthrough base = HW layout slot index.

## Device proof (2026-09-29)

ADB `192.168.1.34:41369`, chroot `/tmp/build-glibc`, patches 001-022 (applied=23).
ICD sha256 `9603547a2998e2b9849702c232133a2020b71ed40017c3bd5edfc4737818e9d3`.
Test: `tests/dxvk/vulkan/bc/bc_decode.c` (20x12, 2 mips incl. partial blocks,
2 layers, random blocks; upload, image copy, raw readback, blit->RGBA32F,
texelFetch sampling, block-aligned sub-update at (8,4) 8x8 on layer 1).
Host reference: `bc_verify.py` (Pillow BC1-3, Mesa CPU BPTC via `bc_ref` for
BC6H/BC7, spec BC4/BC5). Reference runs on host only.

Default (no flag):

```text
ICD device=Mali-G615 MC6 textureCompressionBC=0
BC_SUMMARY FAIL feature/format (fmt_fail=16)
```

`PANVK_DEBUG=bc_emul`:

```text
ICD device=Mali-G615 MC6 textureCompressionBC=1
FORMAT BC1_RGB_UNORM optimal=0x1d401 linear=0x0 ifp=0 PASS   (all 16 identical)
CASE BC1_RGB_UNORM raw=PASS copy=PASS blit=PASS nonzero=2150
CASE BC1_RGB_SRGB raw=PASS copy=PASS blit=PASS nonzero=2143
CASE BC1_RGBA_UNORM raw=PASS copy=PASS blit=PASS nonzero=2097
CASE BC1_RGBA_SRGB raw=PASS copy=PASS blit=PASS nonzero=2029
CASE BC2_UNORM raw=PASS copy=PASS blit=PASS nonzero=2336
CASE BC2_SRGB raw=PASS copy=PASS blit=PASS nonzero=2338
CASE BC3_UNORM raw=PASS copy=PASS blit=PASS nonzero=2357
CASE BC3_SRGB raw=PASS copy=PASS blit=PASS nonzero=2360
CASE BC4_UNORM raw=PASS copy=PASS blit=PASS nonzero=1167
CASE BC4_SNORM raw=PASS copy=PASS blit=PASS nonzero=1200
CASE BC5_UNORM raw=PASS copy=PASS blit=PASS nonzero=1733
CASE BC5_SNORM raw=PASS copy=PASS blit=PASS nonzero=1796
CASE BC6H_UFLOAT raw=PASS copy=PASS blit=PASS nonzero=2160
CASE BC6H_SFLOAT raw=PASS copy=PASS blit=PASS nonzero=2208
CASE BC7_UNORM raw=PASS copy=PASS blit=PASS nonzero=2389
CASE BC7_SRGB raw=PASS copy=PASS blit=PASS nonzero=2323
BC_DEVICE_FAILS=0
VERIFY BC1_RGB_UNORM upload_maxerr=1 subupdate_maxerr=1 tol=2 PASS
VERIFY BC1_RGB_SRGB upload_maxerr=1.078 subupdate_maxerr=1.08 tol=2 PASS
VERIFY BC1_RGBA_UNORM upload_maxerr=1 subupdate_maxerr=1 tol=2 PASS
VERIFY BC1_RGBA_SRGB upload_maxerr=1.269 subupdate_maxerr=1.269 tol=2 PASS
VERIFY BC2_UNORM upload_maxerr=1 subupdate_maxerr=1 tol=2 PASS
VERIFY BC2_SRGB upload_maxerr=1.241 subupdate_maxerr=1.241 tol=2 PASS
VERIFY BC3_UNORM upload_maxerr=1 subupdate_maxerr=1 tol=2 PASS
VERIFY BC3_SRGB upload_maxerr=1.224 subupdate_maxerr=1.171 tol=2 PASS
VERIFY BC4_UNORM upload_maxerr=0.001674 subupdate_maxerr=0.001674 tol=2 PASS
VERIFY BC4_SNORM upload_maxerr=0.001922 subupdate_maxerr=0.001922 tol=2 PASS
VERIFY BC5_UNORM upload_maxerr=0.001675 subupdate_maxerr=0.001675 tol=2 PASS
VERIFY BC5_SNORM upload_maxerr=0.001935 subupdate_maxerr=0.001935 tol=2 PASS
VERIFY BC6H_UFLOAT upload_maxerr=0 subupdate_maxerr=0 tol=0.001 PASS
VERIFY BC6H_SFLOAT upload_maxerr=0 subupdate_maxerr=0 tol=0.001 PASS
VERIFY BC7_UNORM upload_maxerr=0 subupdate_maxerr=0 tol=2 PASS
VERIFY BC7_SRGB upload_maxerr=0.3455 subupdate_maxerr=0.3455 tol=2 PASS
BC_VERIFY_FAILS=0
```

Error units: 8-bit LSB (UNORM, sRGB-encoded), 1/127 (SNORM), relative (BC6H).
BC1-3 max 1 LSB = float lerp vs Pillow integer lerp.

Regression: `gpu_prerast_slice` IDVS 13/13, `PANVK_DEBUG=gpu_prerast` 13/13,
`MATRIX_FAILS=0` both. dmesg: no CS_FAULT/device loss. Blit probe
(`BLIT_PROBE=1|2`, default and gpu_prerast) records OK (was SIGSEGV).

Fresh pin `5a07217f` + `scripts/apply-patches.sh --profile g615-v11-csf`:
`OK applied=23`; tree identical to device-validated `work/mesa`.

## Default exposure proof (csf-v11/038-040, 2026-09-29)

Patches: `038-crc-init-kmod-munmap` (CRC state init unmapped the kbase SAME_VA
BO with `os_munmap`, tearing down the GPU mapping: CSF fault 0xc3 on the next
render target, mmap ENOMEM on realloc; now `pan_kmod_bo_munmap`),
`039-bc-gpu-decode-default`, `040-bc-zero-initialized-decode` (barrier out of
`VK_IMAGE_LAYOUT_ZERO_INITIALIZED_EXT` GPU-decodes the zero blocks).
Fresh pin + apply: `OK applied=41`, tree identical to `work/mesa` 89274dbd817.
ICD sha256 `6731e2ec70b8763dbefbf0f8c70ba5279d34e65691aa95b0769f269e2b26aa32`, PAN_ARCH 11.

```text
ICD device=Mali-G615 MC6 textureCompressionBC=1
FORMAT <all 16> optimal=0x1d401 linear=0x0 ifp=0 PASS
CASE <all 16> raw=PASS copy=PASS blit=PASS
BC_DEVICE_FAILS=0
BC_VERIFY_FAILS=0            (host reference, all 16)
PANVK_DEBUG=no_bc_emul: textureCompressionBC=0 fmt_fail=16
```

CTS (`deqp-vk`, BC subset of the case list, same ICD):

| group | Pass | Fail | NotSupported |
|---|---|---|---|
| texture.compressed (2D) + compressed_3D, image.texel_view_compatible, pipeline.monolithic, image.extended_usage_bit_compatibility, api.info (6348) | 1398 | 0 | 4950 |
| api.copy_and_blit BC, every 20th (1138) | 465 | 0 | 673 |

Main NotSupported reasons: storage/attachment usage on BC ("Operation not
supported with this image format"), linear/DRM-modifier BC, video queues, FSR.
One GPU queue timeout in the long batch at
`texel_view_compatible.graphic.basic.3d_image.texture_read.bc5_unorm_block.r32g32b32a32_uint`;
the case passes alone and the remaining 3144 cases resumed from it pass (0 fail).
Not root-caused; watch for recurrence.

`memory.zero_initialize_device_memory.image_transition`: BC1 1x1 pass after
040. Larger sizes fail the same way non-BC formats do (whole group: Pass 160,
Fail 248, NS 24 incl. r8/rg8/rgba8), so this is a general panvk
zeroInitializeDeviceMemory issue, not BC. Open.

Regression on the same ICD: dx5 IDVS/gpu_prerast 13/13, clip-cull 13/13,
multi-viewport 5/5, fill-mode 17/17 (all `*_FAILS=0`).

## 2026-09-29 re-test

Mesa `work/mesa` `dx6-dx7-base` HEAD `bdd0c26cc5e` (GS behind `PANVK_DEBUG=gs`)
on `efce7cc4382` ("panfrost/kmod: kbase: zero BO pages at allocation") on
`89274dbd817` (= `csf-v11/040`). Uncommitted TMP debug code stashed first
(`stash@{0}` "TMP debug BC5 3D timeout"); tree clean.
ADB `192.168.1.61:41161` (duchamp, Mali-G615 MC6), chroot
`/data/local/tmp/chrootAlpine`. Device `/tmp/mesa` synced to HEAD (sha256 of all
75 files changed since pin `5a07217f` identical), `/tmp/bld.sh`
(`ninja -j4 -C /tmp/build-glibc src/panfrost/vulkan/libvulkan_panfrost.so`),
`NINJA_RC=0`. ICD sha256 `3a10ad4adda689c66d7dd104d649e8be6bb7222cfc0cfd76c52ca923d05d25f0`,
`VK_ICD_FILENAMES=/tmp/bp-icd.json`.

`memory.zero_initialize_device_memory.image_transition` (432 cases, `/tmp/cts-zi.sh`):

| build | Pass | Fail | NotSupported |
|---|---|---|---|
| 040 (baseline) | 160 | 248 | 24 |
| `bdd0c26cc5e` (kbase zeroes BO pages) | **408** | **0** | 24 |

NotSupported: "Format not supported for the target usage" (16 + 8). The general
zeroInitializeDeviceMemory bug is fixed by `efce7cc4382`.

Full `api.copy_and_blit` BC subset, no sampling (22764 cases, `/tmp/cab.sh
'api.copy_and_blit' cab3`: `deqp-vk --deqp-caselist-file`, resumes after an
aborted case):

| run | Pass | Fail | NotSupported | DeviceLost |
|---|---|---|---|---|
| 1/20 sample (040) | 465 | 0 | 673 | 0 |
| full (`bdd0c26cc5e`) | 9619 | 0 | 13144 | 1 |

Runtime 83 s, 2 deqp processes (one resume). The DeviceLost:

```text
dEQP-VK.api.copy_and_blit.core.blit_image.all_formats.color.2d.bc2_srgb_block.r8g8b8a8_srgb.optimal_optimal_linear
vk.waitForFences(...): VK_ERROR_DEVICE_LOST at vkCmdUtil.cpp:296
MESA: error: kbase: timeout on subqueue 0: seqno 33910, ... target 33911, insert 6555712, extract 6555632
MESA: error: kbase_queue_wait_current: failed to wait for subqueue 0 seqno 33911
dmesg: mali 13000000.mali: Timeout waiting for dump completion; Terminate ctx ..., kbase_csf_ctx_handle_fault
```

It is position-dependent, not case-dependent: the case passes alone, the 45-case
window before it passes, and all 12 `bc2_srgb -> r8g8b8a8_srgb` blits pass
(8 Pass, 4 cubic NotSupported). An earlier full run on a pre-`efce7cc4382` build
(`/tmp/cts-cabfull`) hung at the same case and the same subqueue-0 seqno 33911,
then aborted (17274 results). The same class as the earlier one-off BC5 3D
`texel_view_compatible` timeout in a long batch. Likely a per-process resource
that is exhausted after about 34k submissions (for example, ring/syncobj/BO
growth in the kbase queue path), not BC decode. Not root-caused.

## Still open

- `vkCmdCopyImage` uncompressed->BC and cube-view specific checks beyond CTS sample.
- ~~Long-batch queue timeout at subqueue-0 seqno 33911~~ fixed by `9add58a66ee`
  (exported as `csf-v11/043`):
  the kbase tiler heap was never renewed (`tiler_work_estimate` never set).
  Full copy_and_blit is now 9620/0/13144 with 0 DeviceLost. See `SUBMIT-EXHAUSTION.md`.
- Host-mapped writes to BC optimal images are undefined by spec; not handled.
- BC6H UF16<->SF16 mutable views decode with the image format.
- Memory: decoded plane adds 4-8x the raw size per BC image.
