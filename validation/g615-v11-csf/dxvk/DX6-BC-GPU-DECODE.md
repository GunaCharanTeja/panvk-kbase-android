# DX6 BC1-BC7 GPU decode

Status: `GPU_LOWERED` / `DEV_CANDIDATE` (exposed only with `PANVK_DEBUG=bc_emul`).
Default exposure unchanged: `textureCompressionBC=0`, 16 BC formats unsupported.

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

## Not yet proven (blocks default exposure, DX8)

- CTS `dEQP-VK.texture.compressed.*bc*`, `api.copy_and_blit.*bc*`, format
  query tests; 3D BC images; cube views; `vkCmdCopyImage` uncompressed->BC.
- Host-mapped writes to BC optimal images are undefined by spec; not handled.
- BC6H UF16<->SF16 mutable views decode with the image format.
- Memory: decoded plane adds 4-8x the raw size per BC image.
