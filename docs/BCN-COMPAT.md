# BC1-BC7 compatibility

## Current status (1.1.3-FC and later): in-driver transcoding

BC1-BC7 are supported inside PanVK itself (patches 124-130, 172-174), not
through the layer described below. On upload the driver converts each BC
block on the GPU: BC1 and BC7 to ASTC 4x4 LDR (sRGB kept), BC2/BC3 to a 12 bpp
AFRC-compressed RGBA8 shadow on GPUs with AFRC (ASTC otherwise, or with
PANVK_BC_AFRC=0), BC4/BC5 UNORM and SNORM exactly to R8/R8G8, BC6H
UFLOAT/SFLOAT to ASTC 4x4 HDR (negative SFLOAT values read as 0, the only
lossy semantic). ASTC and AFRC are lossy: measured PSNR is in CHANGELOG
1.1.2-FC. The rest of this file is the historical evaluation of the external
layer.

Since 1.1.3-FC the decoded plane is U-interleaved (patch 176, faster
sampling, same size) and images filled only by uploads and BC copies no
longer keep the raw BC blocks (patch 180, PANVK_BC_KEEP_RAW=1 keeps them).

## Status

Experimental layer build/load only. Nothing is packaged or enabled by default.
Native and emulated PanVK BC remain false; `textureCompressionBC=false`.

Reference: `https://github.com/leegao/bcn_layer` pinned at
`50993a2d51772567de9c36de4d523652773f0899`, MIT. The source checkout remains
untracked under `work/bcn_layer`; provenance is in `sources.lock`.

## Intended mode

Correct uncompressed backing is the only acceptable default:

- BC1/2/3/7 UNORM and SRGB: RGBA8 with matching color-space semantics.
- BC4/5 UNORM and SNORM: channel-preserving normalized backing.
- BC6H UFLOAT and SFLOAT: HDR-safe float backing.

ASTC or ETC2 recompression is optional and lossy. It is forbidden for BC4/5
SNORM and BC6H until numeric semantics are proven. It is not enabled here.

## Build result

The pin tracks `src/analyze_astc_parameters.slang`, not the generated `.comp`.
It has no LFS objects, uses the pinned `libGPUCounters` submodule at
`9fe9f4b3e9ee7d9ddb0274991ecf7ae9679be542`, and has no option to omit the ASTC
sources. `FORCE_GLSL=ON` is not usable because that mode expects the absent
generated fallback. The pinned Android CI installs Slang `2026.10.2`; its run
for this exact commit succeeded. Upstream releases are older experimental
snapshots, not artifacts with commit-level provenance, so none were imported.

`scripts/build-bcn-layer-android.sh` downloads that exact Slang release with a
pinned SHA-256, generates shaders from the pinned `.slang` files, applies only
the documented Android enumeration patch, and builds with NDK r30. Result:
Android API 26 AArch64 ELF `libbcn_layer.so`, SHA-256
`08c49c398cf1fcb746593dbd07565fe51a5bb79a20af500656dd3285774c44a5`.

On the Poco X6 Pro, Android 16 discovered and loaded the APK layer. All 16 BC
format queries returned optimal-tiling features `0x9401`. This proves Android
layer discovery/interception against the system Mali ICD only. Android's system
loader did not select the staged PanVK ICD; direct PanVK loading bypasses the
loader and layer. Device creation then failed, so no known-block correctness
workload ran. See `validation/g615-v11-csf/beta3-phase6-bcn-loader.txt`.

## Correctness blockers

- Copying from a decoded BC image is explicitly unimplemented for both
  `vkCmdCopyImage` and `vkCmdCopyImage2`.
- Buffer-size calculation ignores non-default row and image-height strides.
- ASTC/ETC2 paths map BC4/5 SNORM and BC6H to UNORM, losing required semantics.
- No device evidence exists for known blocks, SRGB, SNORM, HDR, mip, array,
  cube, partial copy, filtering, synchronization, or resource stress.
- No tested Android path currently composes the loader, this layer, and the
  staged PanVK ICD in one process.

Consequently the layer cannot truthfully expose the complete Vulkan
`textureCompressionBC` contract in this release.

P9 re-audited the pinned component against current upstream; the pin is still
upstream HEAD and no complete replacement exists. The expanded contract ledger
is `validation/g615-v11-csf/p9-bcn.json`; detailed blockers and the safe in-ICD
completion design are in `validation/g615-v11-csf/P9-BC-COMPATIBILITY.md`.
Exclude `libbcn_layer.so` from Android and glibc packages. No shipping,
emulation, or default-enablement claim is made.
