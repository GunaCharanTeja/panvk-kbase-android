# G615 Consumer Completion Implementation Plan

## Objective

Finish Mali-G615 MC6 PanVK/Kbase compatibility for the named consumers before
starting any JM, G76, G52, G57, G610, G720, or universal-GPU work.

Priority:

1. Samba S3 / RPCSX.
2. NativeCode AI.
3. Bachata S4 / shadps4-arm64.
4. DXVK 2.7.1 and 3.1.1 (Status 2026-10-02: D3D11 FL11_0 reached; vertexPipelineStoresAndAtomics landed in beta.7; FL11_1 not yet re-checked).
5. vkd3d-proton 2.14.1 and 3.0.1 (Status 2026-10-02: Deferred).
6. Wine 10.0, 10.20, and 11.0.
7. Box64 0.4.4.
8. FEX-2609.

Universal Mali work remains deferred through Phase 24. After Phase 24, this
consumer/profile suite becomes the acceptance suite for later GPUs/backends.

## Frozen Baseline

Treat beta.3 as immutable by project convention. GitHub reports
`immutable=false`; never claim server-enforced immutability and never alter the
tag or release assets.

| Item | Frozen value |
|---|---|
| Repository | `https://github.com/abhay-byte/panvk-kbase-android` |
| Tag | `g615-v11-csf-v0.1.0-beta.3` |
| Tag/PanVK commit | `fc8a759e7d1b2b8de01c0e96f1fdc5e3950ba1a3` |
| Release | `https://github.com/abhay-byte/panvk-kbase-android/releases/tag/g615-v11-csf-v0.1.0-beta.3` |
| Published at | `2026-09-19T18:34:13Z` |
| Mesa | `5a07217f034b3e50d8c7c7794f97a2df1742613b` (`26.3.0-devel`) |
| Patch series | `sha256:c0bbdeef591b206a2f3ae36dc6191c08854399039075f8d69f103e33c0eca2f8` |
| Android package SHA-256 | `e94945ad928881919bdd8ce2273de097f0d385c3ba57e87ef959e4cf40442561` |
| glibc package SHA-256 | `f7f13c5dc7a8c07169151710d18a9b134f6cb8bafb83134fd9f07ef4c2ad36c4` |
| Android ICD SHA-256 | `576e37de9a3b60dda9a972a6f91c3a50e09238bd31a9888e04215c7b554c803a` |
| glibc ICD SHA-256 | `95a019b21d42f9d91bf3ee697a4e84b09485fdd8f0c298dda4494356cb983ec0` |
| Device | Poco X6 Pro / `duchamp`, Dimensity 8300-Ultra |
| GPU | Mali-G615 MC6, ID `0xb8a31030`, Valhall v11 |
| Kernel path | CSF, Kbase UAPI 1.21, `/dev/mali0` |
| Runtime | Vulkan 1.4.363; 13 instance + 181 device = 194 extensions |

Repository-state corrections:

- `README.md`, `docs/RUNTIME-FEATURES.md`, `docs/MESA-MATRIX.md`, package
  metadata, and historical beta.3 validation still describe an unpublished
  candidate. Correct current documentation and future package metadata only.
- Keep historical raw validation records unchanged, including their original
  `NOT_PUBLISHED` state.
- Existing beta.3 gates and native-BC evidence under
  `validation/g615-v11-csf/` are the regression baseline.
- This repository is a patch/build layer. Driver changes belong in qualified
  `patches/<family>/` patches or copied patch-family files, not edits under the
  generated `work/mesa/` tree. Consumer changes belong in their own repositories.
- Keep Android/Bionic and Linux/glibc artifacts separate. Never load a Bionic
  ICD into glibc or mix ABI files in one loader directory.

## Non-Negotiable Rules

Never fake extension enumeration, feature bits, descriptor limits, BC formats,
geometry/tessellation, or sparse support. Never root a target app merely to
pass tests. Never silently select system Mali while reporting PanVK.

Use only `PASS`, `PARTIAL`, `BLOCKED`, `UNSUPPORTED`, or `NOT_TESTED` for
capability and consumer results. `PASS` requires a real workload whenever one
is possible. Preserve the distinction between exposed `VK_EXT_robustness2` and
deliberately unexposed `VK_KHR_robustness2`.

Current false G615/PanVK features remain false until implementation plus
workload proof:

*(Status 2026-10-02: geometryShader, tessellationShader, multiViewport,
shaderClipDistance, shaderCullDistance, fillModeNonSolid, and
textureCompressionBC [GPU decode default on] now have implementation plus device
workload proof. depthBounds and shaderFloat64 remain false).*

```text
shaderFloat64
depthBounds
```

Native BC evidence is conclusive: BC1-BC7 hardware mask `0`, all 16 Vulkan BC
formats unsupported, `vkGetPhysicalDeviceImageFormatProperties2` returns
`VK_ERROR_FORMAT_NOT_SUPPORTED`, and `textureCompressionBC=false`. Native BC
reporting must not change. An emulated path may report BC only after complete,
transparent format semantics pass.

All device work retains repository fault safety: hard timeout, small
allocations/images, single queue first, bounded frames, one change per run;
after a severe fault save logs/kernel evidence, stop, and reboot if needed.

## Repository And Evidence Conventions

- Work on `feature/g615-consumer-completion` from the frozen beta.3 commit.
- Preserve `sources.lock`'s pinned Mesa commit unless a separately reviewed
  rebase is required. Recompute `patchSeriesId` whenever patches change.
- Put focused host/device tests under `tests/consumer-compat/` and executable
  automation under `scripts/`.
- Put generated evidence under `validation/g615-v11-csf/`; never overwrite raw
  beta.3 evidence.
- Record source commit, built binary hash, target/device identity, timestamp,
  command, and status in each new evidence result.
- For external repositories, record exact commit/submodule commits before any
  change. Submit consumer-owned changes there; retain commit/PR references and
  integration evidence here.
- Do not add Wayland testing unless NativeCode actually ships/uses Wayland.
- Do not publish compatibility based on extension count. Report exact profile
  tiers and application behavior.

## Common Verification Commands

Run host checks after each repository phase that changes scripts, schemas,
patches, or docs:

```sh
python3 -m unittest discover -s tests/runtime-features -p 'test_*.py'
python3 tests/bcn/validate_cases.py
python3 tests/validate-package-selfcheck.py
python3 scripts/compute-patch-series-id.py --profile g615-v11-csf
git diff --check
```

Build from a fresh exact Mesa checkout when a driver patch changes:

```sh
./scripts/fetch-mesa.sh
./scripts/apply-patches.sh --profile g615-v11-csf
./scripts/bootstrap-host-tools.sh
./scripts/build-android.sh --profile g615-v11-csf
./scripts/validate-binary.sh --abi android dist/android-g615-v11-csf/libvulkan_panfrost.so
./scripts/build-glibc.sh --profile g615-v11-csf --clean
```

The glibc build may dispatch through the configured device chroot on a
non-AArch64 host. Use existing package commands only for a new version, never
to overwrite beta.3:

```sh
./scripts/package-android-adpkg.sh --profile g615-v11-csf --version <new-version>
./scripts/package-glibc.sh --profile g615-v11-csf --version <new-version>
./scripts/validate-package.sh <new-android-package>
./scripts/validate-package.sh <new-glibc-package>
```

## Phase Sequence

Execute exactly in this order. A phase may record `BLOCKED` or `UNSUPPORTED`
and continue only where later work does not depend on its missing capability.

### Phase 0: Baseline Normalization

Changes:

- Create branch `feature/g615-consumer-completion`.
- Correct stale current-state wording that says beta.3 is unpublished in
  `README.md`, `docs/RUNTIME-FEATURES.md`, `docs/MESA-MATRIX.md`, and any
  current release/package documentation. Do not rewrite historical evidence.
- Add `validation/g615-v11-csf/BETA3-PUBLICATION-ADDENDUM.md` recording tag,
  tag commit, release URL, publication timestamp, package hashes, and ICD
  hashes from the frozen table above.
- Ensure future package metadata does not hard-code `published=false` for a
  published version while preserving old assets unchanged.

Commands:

```sh
git switch -c feature/g615-consumer-completion
git rev-list -n 1 g615-v11-csf-v0.1.0-beta.3
gh release view g615-v11-csf-v0.1.0-beta.3 --json url,publishedAt,isImmutable,assets
sha256sum dist/PanVK-Kbase-Android-g615-v11-csf-v0.1.0-beta.3-5a07217f.adpkg.zip
sha256sum dist/PanVK-Kbase-g615-v11-csf-v0.1.0-beta.3-5a07217f-EMULATOR.zip
```

Exit: baseline is uniquely identifiable; beta.3 tag/assets remain unchanged.

### Phase 1: Exact Capability Capture

Create:

```text
tests/consumer-compat/g615_consumer_caps.c
scripts/capture-consumer-capabilities.py
validation/g615-v11-csf/consumer-capabilities.json
```

The C probe must use `vkGetPhysicalDeviceFeatures2` and
`vkGetPhysicalDeviceProperties2` chains, format queries, and extension
enumeration against the exact PanVK ICD. Capture all Vulkan
1.0/1.1/1.2/1.3/1.4 features/properties plus every advertised extension
feature/property structure. The Python wrapper must preserve raw output,
validate the schema/device/ICD hash, and emit one machine-readable source of
truth.

At minimum explicitly capture:

```text
maxPushConstantsSize, maxPushDescriptors, maxCustomBorderColorSamplers
bufferImageGranularity
robustBufferAccess2, robustImageAccess2, nullDescriptor
customBorderColors, customBorderColorWithoutFormat, depthClipEnable
vertexAttributeInstanceRateDivisor, vertexAttributeInstanceRateZeroDivisor
dynamicRenderingUnusedAttachments
transformFeedback, geometryStreams, transformFeedbackQueries
transformFeedbackRasterizationStreamSelect
transformFeedbackStreamsLinesTriangles, transformFeedbackDraw
all descriptor-indexing bits and all UpdateAfterBind descriptor limits
robustBufferAccessUpdateAfterBind
shaderDrawParameters, samplerMirrorClampToEdge, fragmentStoresAndAtomics
storageBuffer8BitAccess, storageBuffer16BitAccess
shaderInt8, shaderInt16, shaderInt64, scalarBlockLayout
multiview, maxMultiviewViewCount, maxMultiviewInstanceIndex
uniformBufferStandardLayout, subgroupBroadcastDynamicId
imagelessFramebuffer, separateDepthStencilLayouts, hostQueryReset
timelineSemaphore, shaderSubgroupExtendedTypes
vulkanMemoryModel, vulkanMemoryModelDeviceScope, bufferDeviceAddress
robustImageAccess, shaderTerminateInvocation
shaderZeroInitializeWorkgroupMemory, synchronization2
shaderIntegerDotProduct, maintenance4, pipelineCreationCacheControl
subgroupSizeControl, computeFullSubgroups
shaderDemoteToHelperInvocation, inlineUniformBlock, dynamicRendering
geometryShader, tessellationShader, fillModeNonSolid, multiViewport
shaderClipDistance, shaderCullDistance, textureCompressionBC
pipelineStatisticsQuery
sparseBinding, sparseResidencyBuffer, sparseResidencyImage2D
sparseResidencyAliased, shaderResourceResidency, shaderResourceMinLod
all sparseProperties
```

Dump legacy, Properties2/Properties3, and image-format properties for all 16 BC
formats. Add a small parser/schema self-check under
`tests/consumer-compat/`. Exit: one complete, exact G615 capability JSON.

### Phase 2: Official Profile Import And Evaluation

Create:

```text
scripts/import-consumer-profile.py
scripts/evaluate-consumer-profile.py
validation/g615-v11-csf/profiles/dxvk-2.7.1.json
validation/g615-v11-csf/profiles/dxvk-3.1.1.json
validation/g615-v11-csf/profiles/vkd3d-2.14.1.json
validation/g615-v11-csf/profiles/vkd3d-3.0.1.json
```

Pin and fetch the exact tagged files:

```text
DXVK v2.7.1/VP_DXVK_requirements.json
DXVK v3.1.1/VP_DXVK_requirements.json
vkd3d-proton v2.14.1/VP_D3D12_VKD3D_PROTON_profile.json
vkd3d-proton v3.0.1/VP_D3D12_VKD3D_PROTON_profile.json
```

Store upstream URL/tag/SHA-256. If DXVK 2.7.1 fails strict JSON parsing due to
its known trailing comma, normalize syntax only and record original SHA,
normalized SHA, and normalization diff. Never change semantic values.

Compare each profile automatically with Phase 1 JSON. Each output separates
`PASS` requirements, `FAIL` requirements, optional capabilities, and
feature-level capabilities. Manual lists below are summaries, never the source
of truth. Add fixture-based importer/evaluator self-checks.

DXVK 2.7.1 common summary:

```text
VK_KHR_maintenance5; maintenance5=true; maxPushConstantsSize >= 256
shaderSampledImageArrayDynamicIndexing
descriptorIndexing, descriptorBindingSampledImageUpdateAfterBind
descriptorBindingUpdateUnusedWhilePending
descriptorBindingPartiallyBound, runtimeDescriptorArray
```

`VK_KHR_maintenance6` is optional in DXVK 2.7.1.

DXVK 2.7.1 D3D9 additionally includes geometryShader, fillModeNonSolid,
shaderClipDistance, shaderCullDistance, textureCompressionBC, depthClamp,
depthBiasClamp, sampleRateShading, occlusionQueryPrecise, independentBlend,
fullDrawIndexUint32, shaderImageGatherExtended, samplerMirrorClampToEdge,
`VK_EXT_robustness2`, nullDescriptor, and robustBufferAccess2.

DXVK 2.7.1 D3D10/11 additionally includes `VK_EXT_transform_feedback`,
transformFeedback, geometryStreams, multiViewport, geometryShader,
fillModeNonSolid, shaderClipDistance, shaderCullDistance, textureCompressionBC,
dualSrcBlend, and shaderDrawParameters. D3D11 FL11_0 additionally includes
tessellationShader, drawIndirectFirstInstance, fragmentStoresAndAtomics, and
multiDrawIndirect. Never call DXVK 2.7 ready from transform feedback alone.

DXVK 3.1.1 has two independent gates: Vulkan 1.4 driver support policy from
DXVK 3.0 release notes, then the exact tagged profile. Its common summary:

```text
VK_KHR_load_store_op_none, VK_KHR_maintenance5, VK_KHR_maintenance6
VK_EXT_depth_clip_enable, VK_EXT_robustness2
fragmentStoresAndAtomics, samplerAnisotropy, shaderInt16
shaderSampledImageArrayDynamicIndexing, storageBuffer16BitAccess
shaderDrawParameters, storageBuffer8BitAccess, shaderInt8
descriptorIndexing, descriptorBindingSampledImageUpdateAfterBind
descriptorBindingUpdateUnusedWhilePending
descriptorBindingPartiallyBound, runtimeDescriptorArray
maintenance5, maintenance6, nullDescriptor, robustBufferAccess2
depthClipEnable, maxPushConstantsSize >= 256
```

Its D3D profiles still add the major legacy graphics features above. Do not
substitute the shorter driver-support wiki list for the tagged profile.

Exit: reproducible exact profile diffs, with no hand-maintained complete set.

### Phase 3: Samba S3 Loader Integration

External repositories:

```text
https://github.com/JICA98/samba-s3
https://github.com/RPCSX/rpcsx
https://github.com/bylaws/libadrenotools
```

Record Samba commit, RPCSX submodule commit, and libadrenotools submodule
commit. Use the real path:

```text
ADPKG -> GpuDriverHelper -> custom-driver selection
      -> RPCSX.setCustomDriver -> libadrenotools
      -> custom libvulkan_panfrost.so
```

Prove archive and ELF acceptance, exact PanVK SO hash loaded, no system-Mali
selection, G615 enumeration, `VkDevice`, Android surface/swapchain, and RPCSX
Vulkan initialization. Samba code changes stay in Samba; this repository gets
only adapter metadata/tests and evidence.

Exit: real Samba custom-driver routing reaches RPCSX Vulkan initialization.

### Phase 4: RPCSX BC Truth Fix

In RPCSX, remove every generic rule equivalent to `PANVK => BC1-BC3`. Query
actual physical-device format support. Keep `textureCompressionBC=false` while
the physical-device feature is false. This fix is mandatory before emulation.

Consumer-side BC decode is permitted only to diagnose/prove a game. It is not
the final driver-completion architecture. Record the RPCSX commit and test
evidence in this repository.

Exit: RPCSX never infers BC from the PanVK driver name.

### Phase 5: NativeCode PanVK Path

External repository: `https://github.com/abhay-byte/nativecode-ai`. Record its
exact starting commit. Current target is Debian 13 Trixie, native glibc, PRoot,
and chroot, so use the glibc artifact.

Add a narrow G615 mode using `FLUX_GPU=panvk` / `GPU_PANVK`. Select PanVK only
when Mali-G615 is detected and `/dev/mali0` is usable; other Mali devices keep
their existing fallback. Install `libvulkan_panfrost.so` plus the Panfrost ICD
manifest. Prefer `VK_DRIVER_FILES`; use legacy `VK_ICD_FILENAMES` only where
required. Export no Turnip/Freedreno variables in PanVK mode.

Test both PRoot and chroot as non-root application paths. Exit: each selects
the exact glibc PanVK ICD without ABI mixing or vendor fallback.

### Phase 6: NativeCode Linux WSI

This phase gates Wine, Box64, and FEX. Against the exact glibc ICD, run
`vulkaninfo`, create Xlib and XCB surfaces/swapchains, complete 300
acquire/render/present frames, resize, destroy/recreate, and cold restart.
Only after native Vulkan passes, optionally test Zink over PanVK. Add Wayland
only if NativeCode actually uses it.

Exit: PRoot and chroot Linux WSI are useful and repeatable.

### Phase 7: Bachata Baseline Integration

External repositories:

```text
https://github.com/JICA98/Bachata-S4
https://github.com/zenithblue-oss/shadps4-arm64
```

Record exact commits. Prove the real host path selects the exact glibc PanVK
ICD and reaches `VkInstance`, G615 selection, `VkDevice`, allocator, presenter,
UI, and a lightweight title boot. Instrument and record the first unsupported
feature/format request rather than guessing.

Exit: baseline presenter/UI/title behavior is classified with concrete gaps.

### Phase 8: Shared Direct-ICD-Compatible BC

BC is the first major feature implementation because it overlaps Bachata
BC1-BC7, RPCSX, DXVK D3D9/10/11, and vkd3d. Architecture preference:

1. PanVK-internal transparent emulation.
2. Direct-dispatch compatibility component that works for direct ICD callers.
3. Conventional Vulkan layer only for loader-based stacks.

The existing pinned `bcn_layer` is reference code, not the final architecture;
do not ship it unchanged. The solution must work for direct callers of
`vk_icdGetInstanceProcAddr`, not only loader-discovered layers. Prefer a
qualified new patch family/file set if implementing inside PanVK. Do not fake
native BC.

Backing semantics:

```text
BC1/2/3/7 UNORM/SRGB -> matching RGBA backing
BC4 UNORM/SNORM      -> matching single-channel normalized backing
BC5 UNORM/SNORM      -> matching two-channel normalized backing
BC6H UFLOAT/SFLOAT   -> HDR-safe float backing
```

Never convert SNORM to UNORM or BC6H to LDR UNORM. Cover all 16 Vulkan BC
formats and preserve transparent Vulkan image/view/copy/barrier semantics.

Mandatory matrix:

```text
1x1, 2x2, 3x3, 4x4, non-multiple-of-4, large images
mips, arrays, cube, partial copies, non-zero offsets
bufferRowLength, bufferImageHeight, row/slice strides, multiple regions
image views, buffer->image, image->image, copy2 variants
nearest, linear, sRGB, SNORM, HDR, sampling/filtering
barriers, multiple command buffers, WSI coexistence, cold restart
destroy/recreate 1000x
```

Only after the full matrix passes may the emulated path advertise BC support.
Record `nativeHardware=false` and `implementation=emulated`; base native
evidence remains unchanged.

Exit: direct-ICD consumers observe correct BC1-BC7 semantics. Otherwise keep
BC unadvertised and phase status `BLOCKED`/`PARTIAL`.

### Phase 9: Clip And Cull Distance

Research current upstream PanVK compiler behavior, NIR clip/cull lowering,
Panfrost Gallium implementation, and Valhall varying/export behavior. Add the
minimal qualified PanVK patch plus focused tests for positive/negative clip
planes, multiple distances, interpolation, primitive clipping, cull-only, and
combined clip+cull.

Expose `shaderClipDistance` and `shaderCullDistance` only after pixel-correct
results. This work overlaps RPCSX, DXVK, and vkd3d.

Exit: both bits have real rendering proof and all beta.3 gates remain green.

### Phase 10: Fill Mode And Geometry Shader

Implement separately even if delivered together.

For `fillModeNonSolid`, test line/point polygon modes, culling interaction,
depth/stencil, and MSAA. For `geometryShader`, research current upstream PanVK
and Panfrost compiler paths; do not infer driver support from hardware support.
Test points, lines, triangles, adjacency if claimed, multiple emitted vertices,
multiple primitives, and layer/viewport outputs where relevant.

Expose each bit only after its own real workload passes. Re-evaluate the exact
DXVK D3D9 profiles after both complete.

Exit: fill mode and geometry are independently proven or independently marked.

### Phase 11: DXVK D3D9 Milestone

**Status (2026-10-02):** PASS (device-proven: geometryShader, fillModeNonSolid,
clip/cull distance, multiViewport, BC GPU decode).

Run the Phase 2 evaluator first for DXVK 2.7.1 and 3.1.1 D3D9. D3D9 requires
geometry, BC, clip/cull, and fill mode; it does not require transform feedback.
If each exact profile passes, run its D3D9 Vulkan workload. Native Wine host
integration remains Phase 18.

Report separately:

```text
DXVK 2.7.1 D3D9 baseline
DXVK 3.1.1 D3D9 baseline
```

Exit: exact profile and workload status for each version; no broader DXVK
claim.

### Phase 12: MultiViewport And Transform Feedback

**Status (2026-10-02):** PASS (device-proven: multiViewport, transform feedback via patches 065-068, 15,793 XFB CTS pass).

For `multiViewport`, test multiple `VkViewport`s/scissors, different depth
ranges, dynamic state, and viewport-index shader behavior if exposed.

Implement `VK_EXT_transform_feedback` entry points and semantics including:

```text
vkCmdBindTransformFeedbackBuffersEXT
vkCmdBeginTransformFeedbackEXT
vkCmdEndTransformFeedbackEXT
vkCmdDrawIndirectByteCountEXT
indexed query paths required by profile/consumer
counter buffers, resume, multiple buffers, barriers
```

Target `transformFeedback=true`, `geometryStreams=true`, and the
vkd3d-profile-required `transformFeedbackQueries=true`. Implement properties
required by exact consumers only; do not claim unsupported extras.

Exit: multiViewport and required transform-feedback paths pass focused tests,
with exact feature/property reporting.

### Phase 13: DXVK D3D10 / FL10.x

**Status (2026-10-02):** PASS (device-proven: transformFeedback, geometryStreams,
multiViewport).

Re-evaluate exact DXVK 2.7.1 and 3.1.1 D3D10 Level 10.1 baselines. This uses
the D3D11 baseline capability set but not FL11 tessellation. For each passing
profile run a D3D10 minimal workload and, if practical, a D3D11 application
constrained to the supported feature level.

Exit: profile checker and workload status reported independently per DXVK
version.

### Phase 14: Tessellation

**Status (2026-10-02):** PASS (device-proven: tessellationShader via patches 065-068, 526 CTS pass).

Research PanVK compiler TCS/TES support, Valhall hardware path, proprietary
G615 evidence, relevant PanVK forks, NIR tessellation lowering, and CSF draw
setup. Implement real `tessellationShader`, TCS, and TES support.

Test TCS, TES, patch control points, isolines, triangles, quads if applicable,
fractional/equal spacing, winding, tessellation levels, and barriers. No
feature bit until real rendering passes.

Exit: tessellation has real rendering proof for DXVK FL11_0, vkd3d baseline,
and affected Bachata titles.

### Phase 15: DXVK D3D11 FL11_0

**Status (2026-10-02):** PASS (device-proven: tessellationShader; DXVK Native
v3.1.1 FL 11_0 `0xb000` reached). FL11_1 remains in progress pending
`vertexPipelineStoresAndAtomics`.

Run exact DXVK 2.7.1 and 3.1.1 D3D11 Level 11.0 profiles independently. Do not
call DXVK fully compatible from D3D9/D3D10 results. For each passing profile,
test D3D11 triangle, geometry shader, tessellation, stream output, BC texture,
multiViewport, clip/cull, 30-minute presentation, and shader-cache reload.

Optional higher feature levels remain separate. Never report a higher feature
level unless its exact profile passes.

Exit: exact profile and real workload status for each version and FL11_0.

### Phase 16: vkd3d-proton Hard Requirements

**Status (2026-10-02):** DEFERRED (vkd3d-proton deferred; robustImageAccess2=false and sparse impossible on kbase).

Evaluate 2.14.1 and 3.0.1 separately. Official README hard requirements
include Vulkan 1.3, descriptor indexing, at least 1,000,000 UpdateAfterBind
descriptors for required types except UniformBuffer, essentially all required
descriptor-indexing features, samplerMirrorClampToEdge,
shaderDrawParameters, `VK_EXT_robustness2`, and `VK_KHR_push_descriptor`.
Measure descriptor capacity; never infer it.

For v3 also prove `VK_EXT_dynamic_rendering_unused_attachments`'s feature,
despite beta.3 extension enumeration. Evaluate the tagged profile directly.

Report four independent statuses for each version:

```text
HARD_REQUIREMENTS
VULKAN_PROFILE_BASELINE
D3D12_DEVICE
D3D_FEATURE_LEVEL
```

Exit: hard requirements and profile failures are exact and machine-generated.

### Phase 17: vkd3d D3D12 Smoke

**Status (2026-10-02):** DEFERRED (vkd3d-proton deferred).

Only if the corresponding hard gate passes, create D3D12 device, queue, fence,
command list, upload resource, root signature, pipeline, triangle, and
swapchain. Run the tagged Vulkan Profile and report remaining baseline gaps.
Do not require sparse FL12_0 capabilities for this first smoke.

Exit: D3D12 smoke, full profile baseline, and D3D feature level remain
separate results for 2.14.1 and 3.0.1.

### Phase 18: Wine Matrix

Pin and test:

```text
Wine 10.0 stable
Wine 10.20 late Wine-10 development
Wine 11.0 stable
```

DXVK 3.1.1's build requirement is Wine 10.0 or newer, not 10.1+. Wine 11 has
complete new WoW64 and no separate `wine64` loader; invoke `wine`.

For each applicable version run `wineboot`, `winecfg`, minimal winevulkan app,
64-bit app, 32-bit/WoW64 app, reached DXVK milestones, reached vkd3d
milestones, resize, fullscreen/windowed, and relaunch.

Exit: each Wine version and Wine 11 WoW64 have independent statuses.

### Phase 19: Box64 0.4.4

Pin 0.4.4. Test separately:

```text
A. x86_64 Linux Vulkan through Box64
B. Wine 10 x86_64 path
C. Wine 11 new-WoW64 path
```

Box64 documents ordinary 32-bit components through Box86/Box32 and Wine WOW64
in a Box64-only environment as experimental. Do not equate C with A/B; report
it separately.

Required host path:

```text
Box64 -> ARM64 glibc libvulkan.so.1 -> PanVK glibc ICD
```

Never load the Bionic ICD. Log host loader and ICD hashes. Report Vulkan,
Wine10, Wine11 WoW64, DXVK, and vkd3d separately.

### Phase 20: FEX-2609

Pin FEX-2609. Keep Bachata's embedded FEXCore CPU execution separate from
standalone FEX Vulkan thunking. For standalone FEX test guest Vulkan procaddr,
x86_64 vulkaninfo/sample, x86 sample if supported, host loader selection, Wine
10.0/10.20/11.0, reached DXVK milestones, and reached vkd3d milestones. Host
Vulkan must resolve the exact PanVK glibc ICD.

Exit: standalone FEX Vulkan, Wine versions, DXVK, and vkd3d are independently
reported; no claim derived from Bachata's embedded FEXCore.

### Phase 21: Primary-App Real-Game Qualification

Re-run primary apps after shared feature work.

Samba requirements: custom-driver import, correct SO hash, RPCSX `VkDevice`, BC
correctness, clip/cull behavior, real lightweight title, representative title,
10-minute stability, and second launch.

NativeCode requirements: PRoot Vulkan, chroot Vulkan, Xlib/XCB WSI, 300 frames,
restart/cache reuse, and optional Zink only after native Vulkan.

Bachata requirements: PanVK renderer, BC1-BC7 correctness, light title,
Bloodborne boot, Bloodborne in-game or exact remaining non-driver blocker,
10-minute stability, and second launch.

All three must record cache reuse and absence/presence of device loss.

Exit: useful real workloads, second launch, and stability are measured for all
primary consumers.

### Phase 22: Higher D3D/Common Graphics Features

Only after basic DXVK/vkd3d are useful, use measured consumer/profile gaps to
investigate and implement separately:

```text
pipelineStatisticsQuery
depthBounds
logicOp gaps
vertexPipelineStoresAndAtomics
shaderFloat64
higher DXVK feature levels
```

Do not group these automatically with sparse support. Each advertised feature
needs a focused workload and regression run.

Exit: only measured high-value gaps are implemented; exact higher tiers are
reported without overclaiming.

### Phase 23: Sparse / FL12

Do this last. Before coding create `docs/SPARSE-KBASE-FEASIBILITY.md` covering
Kbase VA reservation, partial backing, unback/rebind, page granularity,
residency, page-fault semantics, aliasing, synchronization, sparse queue
behavior, and the CSF queue model.

Assess:

```text
sparseBinding, sparseResidencyAliased, sparseResidencyBuffer
sparseResidencyImage2D, shaderResourceResidency, shaderResourceMinLod
VK_QUEUE_SPARSE_BINDING_BIT
```

Require an explicit go/no-go decision. Implement only after `GO`; otherwise
record a concrete Kbase/hardware blocker. Never spoof FL12.

Exit: reviewed feasibility decision, then real sparse workloads if justified.

### Phase 24: Final G615 Consumer Package

From clean trees run Android and glibc builds, every beta.3 regression gate,
consumer capability capture, all four profile validations, Samba, NativeCode,
Bachata, reached DXVK/vkd3d tiers, Wine, Box64, FEX, and package/provenance
verification.

Preserve these beta.3 regression gates:

```text
Android app-UID hot-load, ICD negotiation, G615 enumeration, vkCreateDevice
compute, offscreen graphics, AHB properties/import, AHB render/readback
binary semaphore, fence, timeline semaphore, synchronization2
dynamic rendering, buffer device address, push descriptors
descriptor-indexing workload, Android surface, swapchain
acquire/render/present, 300/300 presents, cold second process
```

Generate:

```text
validation/g615-v11-csf/CONSUMER-VALIDATION.json
validation/g615-v11-csf/CONSUMER-VALIDATION.md
```

The JSON schema must retain evidence references and the required status enum.
The Markdown report must use the final report fields below. Package under a
new version/tag; do not mutate beta.3. Application-defined results, not
extension count, decide release readiness.

Minimum final criteria:

```text
all beta.3 core gates green
Samba useful
NativeCode Vulkan/WSI useful
Bachata useful, including correct BC path
exact DXVK supported tiers known and tested
exact vkd3d hard/profile/device/feature-level status known and tested
Wine 10.0, 10.20, and 11.0 measured
Box64 0.4.4 host Vulkan route measured
FEX-2609 Vulkan thunk route measured
all remaining failures mapped to concrete capabilities
```

Exit: clean, provenance-complete G615 consumer package and matrix.

## Execution Work Packets

Execute these packets sequentially. A later packet may start only after the
previous packet records its exit status and evidence. `BLOCKED` or
`UNSUPPORTED` permits continuation only where the later packet does not depend
on the missing capability. Each agent owns only the files listed for its
packet: repository driver work stays under qualified `patches/` families,
generated `work/mesa/` is never edited, and consumer source changes stay in the
named external repository. Before editing an external repository whose commit
or internal paths are not frozen here, record its starting commit and exact
owned paths in the packet's `source-manifest.json`; do not guess paths or copy
consumer code into this repository.

### Work Packet 1: Normalize Baseline And Build Capability Truth

- **Phases:** 0, then 1, then 2.
- **Files/ownership:** `README.md`, `docs/RUNTIME-FEATURES.md`,
  `docs/MESA-MATRIX.md`, current package metadata under `scripts/`,
  `validation/g615-v11-csf/BETA3-PUBLICATION-ADDENDUM.md`,
  `tests/consumer-compat/g615_consumer_caps.c`,
  `tests/consumer-compat/test_consumer_caps.py`,
  `tests/consumer-compat/test_profile_tools.py`,
  `tests/consumer-compat/fixtures/`,
  `scripts/capture-consumer-capabilities.py`,
  `scripts/import-consumer-profile.py`,
  `scripts/evaluate-consumer-profile.py`,
  `validation/g615-v11-csf/consumer-capabilities.json`, and the four exact
  profile JSON files listed in Phase 2. Historical beta.3 evidence and release
  assets are read-only.
- **Dependencies:** branch from `fc8a759e7d1b2b8de01c0e96f1fdc5e3950ba1a3`;
  verify the frozen hashes before any change. Phase 2 consumes the Phase 1 JSON.
- **Acceptance criteria:** current docs identify published beta.3 without
  rewriting history; package metadata no longer forces a false unpublished
  state; one schema-validated exact G615 capability capture exists; all four
  tagged upstream profiles retain provenance and produce machine-generated
  PASS/FAIL/optional/feature-level diffs.
- **Validation commands:**

  ```sh
  git rev-list -n 1 g615-v11-csf-v0.1.0-beta.3
  gh release view g615-v11-csf-v0.1.0-beta.3 --json url,publishedAt,isImmutable,assets
  python3 -m unittest discover -s tests/consumer-compat -p 'test_*.py'
  python3 scripts/capture-consumer-capabilities.py --help
  python3 scripts/import-consumer-profile.py --help
  python3 scripts/evaluate-consumer-profile.py --help
  python3 -m unittest discover -s tests/runtime-features -p 'test_*.py'
  python3 tests/bcn/validate_cases.py
  python3 tests/validate-package-selfcheck.py
  python3 scripts/compute-patch-series-id.py --profile g615-v11-csf
  git diff --check
  ```

### Work Packet 2: Complete Samba And RPCSX Truthful Loading

- **Phases:** 3, then 4.
- **Files/ownership:** Samba S3, RPCSX, and libadrenotools files recorded first
  in `validation/g615-v11-csf/samba-rpcsx/source-manifest.json`; only those
  external repositories may receive consumer changes. This repository owns
  `tests/consumer-compat/samba-rpcsx/`,
  `scripts/validate-samba-rpcsx.py`, and
  `validation/g615-v11-csf/samba-rpcsx/`. No PanVK feature reporting changes.
- **Dependencies:** Work Packet 1 capability JSON; Phase 3 loader proof before
  Phase 4 BC inference changes. Record Samba, RPCSX submodule, and
  libadrenotools submodule commits before editing.
- **Acceptance criteria:** ADPKG custom selection reaches
  `RPCSX.setCustomDriver` and libadrenotools; loaded SO hash equals the Android
  PanVK ICD; system Mali is absent; G615 enumeration, `VkDevice`, Android
  surface/swapchain, and RPCSX Vulkan initialization pass; RPCSX contains no
  PanVK-name-to-BC inference and reports/query results from the physical device.
- **Validation commands:**

  ```sh
  python3 scripts/validate-samba-rpcsx.py --manifest validation/g615-v11-csf/samba-rpcsx/source-manifest.json
  python3 -m unittest discover -s tests/consumer-compat/samba-rpcsx -p 'test_*.py'
  python3 scripts/capture-consumer-capabilities.py --output validation/g615-v11-csf/samba-rpcsx/device-capabilities.json
  git diff --check
  ```

### Work Packet 3: Establish NativeCode And Bachata Baselines

- **Phases:** 5, then 6, then 7.
- **Files/ownership:** NativeCode AI, Bachata S4, and shadps4-arm64 paths
  recorded before editing in
  `validation/g615-v11-csf/primary-baselines/source-manifest.json`; consumer
  edits remain in those repositories. This repository owns
  `tests/consumer-compat/linux-wsi/`,
  `scripts/validate-nativecode-panvk.py`,
  `scripts/validate-bachata-panvk.py`, and
  `validation/g615-v11-csf/primary-baselines/`.
- **Dependencies:** Work Packet 1 exact glibc ICD and capability JSON. NativeCode
  Phase 5 selection must pass before Phase 6 WSI. Phase 6 must pass before any
  later Wine, Box64, or FEX packet. Bachata baseline records gaps; it does not
  wait for BC implementation.
- **Acceptance criteria:** non-root PRoot and chroot select only the exact glibc
  PanVK ICD on usable G615 `/dev/mali0`; no Bionic mixing or Turnip variables;
  Xlib and XCB each survive 300 present frames, resize/recreate, and cold
  restart; Bachata reaches device, allocator, presenter, UI, and lightweight
  title or records the first exact unsupported request. Wayland remains absent
  unless the source manifest proves NativeCode uses it.
- **Validation commands:**

  ```sh
  python3 scripts/validate-nativecode-panvk.py --mode proot --frames 300
  python3 scripts/validate-nativecode-panvk.py --mode chroot --frames 300
  python3 scripts/validate-bachata-panvk.py --manifest validation/g615-v11-csf/primary-baselines/source-manifest.json
  python3 -m unittest discover -s tests/consumer-compat/linux-wsi -p 'test_*.py'
  ./scripts/validate-binary.sh --abi glibc dist/glibc-g615-v11-csf/libvulkan_panfrost.so
  git diff --check
  ```

### Work Packet 4: Implement Shared Direct-ICD BC Compatibility

- **Phases:** 8.
- **Files/ownership:** a new qualified family under `patches/bc-emulation/`,
  its copied source files under `patches/bc-emulation/files/`,
  `tests/consumer-compat/bc/`, and
  `validation/g615-v11-csf/bc-emulation/`. Update `sources.lock` only for the
  recomputed `patchSeriesId`; do not edit `work/mesa/` or ship the existing
  `bcn_layer` unchanged.
- **Dependencies:** Work Packets 1-3; direct `vk_icdGetInstanceProcAddr` callers
  are mandatory. Preserve native BC evidence and native
  `textureCompressionBC=false` until the complete emulated matrix passes.
- **Acceptance criteria:** all 16 BC formats preserve the backing, copy, view,
  barrier, sampling, SNORM, sRGB, and BC6H HDR semantics in Phase 8 for loader
  and direct-ICD consumers; 1,000 destroy/recreate cycles and WSI coexistence
  pass. Otherwise BC stays unadvertised and the packet records `PARTIAL` or
  `BLOCKED`.
- **Validation commands:**

  ```sh
  python3 tests/bcn/validate_cases.py
  python3 -m unittest discover -s tests/consumer-compat/bc -p 'test_*.py'
  ./scripts/fetch-mesa.sh
  ./scripts/apply-patches.sh --profile g615-v11-csf
  ./scripts/bootstrap-host-tools.sh
  ./scripts/build-android.sh --profile g615-v11-csf
  ./scripts/validate-binary.sh --abi android dist/android-g615-v11-csf/libvulkan_panfrost.so
  ./scripts/build-glibc.sh --profile g615-v11-csf --clean
  python3 scripts/compute-patch-series-id.py --profile g615-v11-csf
  git diff --check
  ```

### Work Packet 5: Reach DXVK D3D9

- **Status (2026-10-02):** PASS (device-proven: geometryShader, fillModeNonSolid, clip/cull distance, multiViewport, BC GPU decode).
- **Phases:** 9, then 10, then 11.
- **Files/ownership:** qualified new patches under `patches/panvk-graphics/`,
  focused tests under `tests/consumer-compat/panvk-graphics/`,
  `scripts/validate-dxvk-tier.py`, and
  `validation/g615-v11-csf/dxvk/d3d9/`. Only patch files, never generated Mesa
  sources, carry driver changes.
- **Dependencies:** Work Packet 4 BC result. Clip and cull must be independently
  proven before exposure; fill mode and geometry remain independently gated.
  Run the Phase 2 evaluator before either version's workload.
- **Acceptance criteria:** clip/cull pixel tests pass; fill line/point modes and
  geometry workloads pass independently; exact DXVK 2.7.1 and 3.1.1 D3D9
  profile and workload statuses are recorded separately with no broader claim.
- **Validation commands:**

  ```sh
  python3 -m unittest discover -s tests/consumer-compat/panvk-graphics -p 'test_*.py'
  python3 scripts/evaluate-consumer-profile.py --capabilities validation/g615-v11-csf/consumer-capabilities.json --profile validation/g615-v11-csf/profiles/dxvk-2.7.1.json
  python3 scripts/evaluate-consumer-profile.py --capabilities validation/g615-v11-csf/consumer-capabilities.json --profile validation/g615-v11-csf/profiles/dxvk-3.1.1.json
  python3 scripts/validate-dxvk-tier.py --version 2.7.1 --tier d3d9
  python3 scripts/validate-dxvk-tier.py --version 3.1.1 --tier d3d9
  ./scripts/build-android.sh --profile g615-v11-csf
  ./scripts/build-glibc.sh --profile g615-v11-csf --clean
  python3 scripts/compute-patch-series-id.py --profile g615-v11-csf
  git diff --check
  ```

### Work Packet 6: Reach DXVK D3D10 / FL10.x

- **Status (2026-10-02):** PASS (device-proven: transformFeedback, geometryStreams, multiViewport).
- **Phases:** 12, then 13.
- **Files/ownership:** additional qualified patches under
  `patches/panvk-graphics/`, tests under
  `tests/consumer-compat/transform-feedback/`, and evidence under
  `validation/g615-v11-csf/dxvk/d3d10/`. Existing Packet 5 patches may be
  amended only for defects exposed by this packet, not unrelated refactoring.
- **Dependencies:** Work Packet 5. `multiViewport` and every required transform
  feedback command, property, query, counter, resume, barrier, and indirect
  path must pass before profile reevaluation.
- **Acceptance criteria:** exact feature/property reporting matches tested
  behavior; both DXVK versions receive independent D3D10 Level 10.1 profile and
  workload status; no FL11 claim.
- **Validation commands:**

  ```sh
  python3 -m unittest discover -s tests/consumer-compat/transform-feedback -p 'test_*.py'
  python3 scripts/validate-dxvk-tier.py --version 2.7.1 --tier d3d10_10_1
  python3 scripts/validate-dxvk-tier.py --version 3.1.1 --tier d3d10_10_1
  ./scripts/fetch-mesa.sh
  ./scripts/apply-patches.sh --profile g615-v11-csf
  ./scripts/build-android.sh --profile g615-v11-csf
  ./scripts/build-glibc.sh --profile g615-v11-csf --clean
  python3 scripts/compute-patch-series-id.py --profile g615-v11-csf
  git diff --check
  ```

### Work Packet 7: Reach DXVK D3D11 FL11_0

- **Status (2026-10-02):** PASS for FL11_0 (device-proven: tessellationShader; DXVK Native v3.1.1 FL 11_0 reached; vertexPipelineStoresAndAtomics landed in beta.7; FL11_1 not yet re-checked).
- **Phases:** 14, then 15.
- **Files/ownership:** qualified tessellation patches under
  `patches/panvk-graphics/`, tests under
  `tests/consumer-compat/tessellation/`, and evidence under
  `validation/g615-v11-csf/dxvk/d3d11-fl11_0/`.
- **Dependencies:** Work Packet 6. TCS/TES rendering proof precedes feature
  exposure; exact profile success precedes each tagged DXVK workload.
- **Acceptance criteria:** required TCS/TES topology, spacing, winding, level,
  control-point, and barrier tests pass; each DXVK version records independent
  FL11_0 profile, triangle, geometry, tessellation, stream output, BC,
  multiViewport, clip/cull, 30-minute presentation, and cache-reload results.
- **Validation commands:**

  ```sh
  python3 -m unittest discover -s tests/consumer-compat/tessellation -p 'test_*.py'
  python3 scripts/validate-dxvk-tier.py --version 2.7.1 --tier d3d11_11_0 --minutes 30
  python3 scripts/validate-dxvk-tier.py --version 3.1.1 --tier d3d11_11_0 --minutes 30
  ./scripts/fetch-mesa.sh
  ./scripts/apply-patches.sh --profile g615-v11-csf
  ./scripts/build-android.sh --profile g615-v11-csf
  ./scripts/build-glibc.sh --profile g615-v11-csf --clean
  python3 scripts/compute-patch-series-id.py --profile g615-v11-csf
  git diff --check
  ```

### Work Packet 8: Measure vkd3d And Run Permitted D3D12 Smoke

- **Status (2026-10-02):** DEFERRED (vkd3d-proton deferred; robustImageAccess2=false and sparse impossible on kbase).
- **Phases:** 16, then 17.
- **Files/ownership:** `tests/consumer-compat/vkd3d/`,
  `scripts/validate-vkd3d-tier.py`, and
  `validation/g615-v11-csf/vkd3d/`. Driver changes are out of scope unless a
  separately measured defect routes work back to the owning capability packet.
- **Dependencies:** Work Packet 7 capability capture. Measure descriptor limits
  and all profile requirements; run a version's D3D12 smoke only after that
  version's hard gate passes.
- **Acceptance criteria:** vkd3d-proton 2.14.1 and 3.0.1 each report
  `HARD_REQUIREMENTS`, `VULKAN_PROFILE_BASELINE`, `D3D12_DEVICE`, and
  `D3D_FEATURE_LEVEL`; v3 explicitly tests dynamic-rendering-unused-attachments;
  allowed smoke covers device through swapchain without conflating sparse FL12.
- **Validation commands:**

  ```sh
  python3 -m unittest discover -s tests/consumer-compat/vkd3d -p 'test_*.py'
  python3 scripts/evaluate-consumer-profile.py --capabilities validation/g615-v11-csf/consumer-capabilities.json --profile validation/g615-v11-csf/profiles/vkd3d-2.14.1.json
  python3 scripts/evaluate-consumer-profile.py --capabilities validation/g615-v11-csf/consumer-capabilities.json --profile validation/g615-v11-csf/profiles/vkd3d-3.0.1.json
  python3 scripts/validate-vkd3d-tier.py --version 2.14.1
  python3 scripts/validate-vkd3d-tier.py --version 3.0.1
  git diff --check
  ```

### Work Packet 9: Execute Wine, Box64, And FEX Matrices

- **Phases:** 18, then 19, then 20.
- **Files/ownership:** exact external revisions and any consumer-owned paths in
  `validation/g615-v11-csf/host-stacks/source-manifest.json`; this repository
  owns `tests/consumer-compat/host-stacks/`,
  `scripts/validate-host-stack.py`, and
  `validation/g615-v11-csf/host-stacks/`. No Wine, Box64, or FEX source change
  is allowed without adding its exact path to the manifest first.
- **Dependencies:** Work Packet 3 Linux WSI gate plus reached Work Packets 5-8
  tiers. Test Wine in listed order, then Box64, then standalone FEX. Keep
  Bachata's embedded FEXCore outside standalone FEX results.
- **Acceptance criteria:** Wine 10.0, 10.20, 11.0, and Wine 11 new-WoW64 results
  are separate; Box64 A/B/C paths are separate and use ARM64 glibc loader plus
  exact PanVK glibc ICD; standalone FEX guest Vulkan, host resolution, Wine,
  DXVK, and vkd3d results are separate; every loader and ICD hash is recorded.
- **Validation commands:**

  ```sh
  python3 -m unittest discover -s tests/consumer-compat/host-stacks -p 'test_*.py'
  python3 scripts/validate-host-stack.py --stack wine --version 10.0
  python3 scripts/validate-host-stack.py --stack wine --version 10.20
  python3 scripts/validate-host-stack.py --stack wine --version 11.0
  python3 scripts/validate-host-stack.py --stack box64 --version 0.4.4
  python3 scripts/validate-host-stack.py --stack fex --version 2609
  ./scripts/validate-binary.sh --abi glibc dist/glibc-g615-v11-csf/libvulkan_panfrost.so
  git diff --check
  ```

### Work Packet 10: Requalify Primary Applications

- **Phases:** 21.
- **Files/ownership:** validation automation already owned by Work Packets 2-4
  and evidence under `validation/g615-v11-csf/primary-qualification/`.
  Consumer-source edits are defect fixes only in their original external
  owner, with commit/path additions to the relevant source manifest.
- **Dependencies:** Work Packets 4-9. Rebuild and reinstall exact artifacts
  before rerunning; do not substitute extension counts for app behavior.
- **Acceptance criteria:** Samba, NativeCode, and Bachata satisfy every Phase 21
  workload, stability, cache reuse, second-launch, and device-loss field, or
  record an exact non-driver/capability blocker with evidence.
- **Validation commands:**

  ```sh
  python3 scripts/validate-samba-rpcsx.py --qualification
  python3 scripts/validate-nativecode-panvk.py --mode proot --frames 300 --qualification
  python3 scripts/validate-nativecode-panvk.py --mode chroot --frames 300 --qualification
  python3 scripts/validate-bachata-panvk.py --qualification
  python3 -m unittest discover -s tests/consumer-compat -p 'test_*.py'
  git diff --check
  ```

### Work Packet 11: Resolve Measured Higher-Tier Gaps And Sparse Feasibility

- **Phases:** 22, then 23.
- **Files/ownership:** focused additions under `patches/panvk-graphics/` only
  for measured Phase 22 gaps, corresponding tests under
  `tests/consumer-compat/higher-tiers/`,
  `docs/SPARSE-KBASE-FEASIBILITY.md`, optional qualified sparse patches under
  `patches/panvk-sparse/`, tests under `tests/consumer-compat/sparse/`, and
  evidence under `validation/g615-v11-csf/higher-tiers/` and
  `validation/g615-v11-csf/sparse/`.
- **Dependencies:** Work Packet 10. Implement only measured high-value Phase 22
  gaps, one feature and proof at a time. Complete and review the sparse
  feasibility document after Phase 22; sparse code is forbidden until its
  explicit decision is `GO`.
- **Acceptance criteria:** each exposed higher-tier feature has an isolated
  workload and regression result; sparse has a concrete Kbase/CSF `GO` or
  `NO-GO`. On `GO`, all claimed sparse bits, queue behavior, residency,
  aliasing, rebinding, and synchronization pass real workloads; on `NO-GO`, no
  sparse bit is added and the blocker is concrete.
- **Validation commands:**

  ```sh
  python3 -m unittest discover -s tests/consumer-compat/higher-tiers -p 'test_*.py'
  python3 -m unittest discover -s tests/consumer-compat/sparse -p 'test_*.py'
  ./scripts/fetch-mesa.sh
  ./scripts/apply-patches.sh --profile g615-v11-csf
  ./scripts/build-android.sh --profile g615-v11-csf
  ./scripts/build-glibc.sh --profile g615-v11-csf --clean
  python3 scripts/compute-patch-series-id.py --profile g615-v11-csf
  git diff --check
  ```

  Skip the sparse test command when the reviewed decision is `NO-GO`; record
  that command as `NOT_TESTED` with the decision evidence rather than treating
  the absent implementation as a test failure.

### Work Packet 12: Build And Validate The Final G615 Package

- **Phases:** 24.
- **Files/ownership:** release/package metadata under `scripts/`, a new version
  under `dist/`, `validation/g615-v11-csf/CONSUMER-VALIDATION.json`, and
  `validation/g615-v11-csf/CONSUMER-VALIDATION.md`. Beta.3 tag, assets, packages,
  hashes, and raw evidence remain read-only.
- **Dependencies:** Work Packets 1-11 completed or explicitly classified. Start
  from clean Mesa and consumer trees; use a new version/tag only after every
  required result is populated with the allowed status enum.
- **Acceptance criteria:** both ABI packages validate; all beta.3 regression
  gates remain green; the full consumer/profile/host matrix is rerun; JSON
  retains evidence references and valid statuses; Markdown contains every Final
  Report Contract field; every residual failure maps to a concrete capability;
  release readiness follows application results, not extension count.
- **Validation commands:**

  ```sh
  ./scripts/fetch-mesa.sh
  ./scripts/apply-patches.sh --profile g615-v11-csf
  ./scripts/bootstrap-host-tools.sh
  ./scripts/build-android.sh --profile g615-v11-csf
  ./scripts/validate-binary.sh --abi android dist/android-g615-v11-csf/libvulkan_panfrost.so
  ./scripts/build-glibc.sh --profile g615-v11-csf --clean
  ./scripts/package-android-adpkg.sh --profile g615-v11-csf --version <new-version>
  ./scripts/package-glibc.sh --profile g615-v11-csf --version <new-version>
  ./scripts/validate-package.sh <new-android-package>
  ./scripts/validate-package.sh <new-glibc-package>
  python3 -m unittest discover -s tests/runtime-features -p 'test_*.py'
  python3 -m unittest discover -s tests/consumer-compat -p 'test_*.py'
  python3 tests/bcn/validate_cases.py
  python3 tests/validate-package-selfcheck.py
  python3 scripts/compute-patch-series-id.py --profile g615-v11-csf
  git diff --check
  ```

## Final Report Contract

Populate every field with the status enum or exact value; use `NOT_TESTED`
rather than omission.

```text
BASELINE:
TAG=
PANVK_COMMIT=
MESA_COMMIT=
PATCH_SERIES=
ANDROID_ICD_SHA=
GLIBC_ICD_SHA=

CAPTURE:
API=
MAX_PUSH_CONSTANTS=
MAX_PUSH_DESCRIPTORS=
MAX_CUSTOM_BORDER_SAMPLERS=
ROBUST_BUFFER_ACCESS2=
ROBUST_IMAGE_ACCESS2=
NULL_DESCRIPTOR=
DESCRIPTOR_INDEXING=
UAB_STORAGE_BUFFER=
UAB_SAMPLED_IMAGE=
UAB_STORAGE_IMAGE=
GEOMETRY_SHADER=
TESSELLATION_SHADER=
FILL_MODE_NON_SOLID=
MULTI_VIEWPORT=
CLIP_DISTANCE=
CULL_DISTANCE=
TRANSFORM_FEEDBACK=
GEOMETRY_STREAMS=
BC=
SPARSE_BINDING=

PROFILES:
DXVK_2_7_1_D3D9=
DXVK_2_7_1_D3D10_10_1=
DXVK_2_7_1_D3D11_11_0=
DXVK_3_1_1_D3D9=
DXVK_3_1_1_D3D10_10_1=
DXVK_3_1_1_D3D11_11_0=
VKD3D_2_14_1_HARD=
VKD3D_2_14_1_BASELINE=
VKD3D_3_0_1_HARD=
VKD3D_3_0_1_BASELINE=

SAMBA:
IMPORT=
CUSTOM_LOAD=
DEVICE=
BC=
GAME1=
GAME2=
STABILITY=

NATIVECODE:
PROOT=
CHROOT=
XLIB=
XCB=
WSI_300=
ZINK=

BACHATA:
DEVICE=
PRESENT=
BC1=
BC2=
BC3=
BC4=
BC5=
BC6H=
BC7=
LIGHT_GAME=
BLOODBORNE=

WINE:
10_0=
10_20=
11_0=
11_0_WOW64=

BOX64:
0_4_4_VULKAN=
WINE10=
WINE11_WOW64=
DXVK=
VKD3D=

FEX:
2609_VULKAN=
WINE10=
WINE11=
DXVK=
VKD3D=

REGRESSIONS:
-

BLOCKERS:
-

FILES_CHANGED:
-

PATCHES:
-

NEXT_SINGLE_STEP=
```

## Definition Of G615 Completion

G615 is complete for this project only when beta.3 gates remain green; Samba
and Bachata run useful real-game workloads; NativeCode runs real glibc Vulkan
and Linux WSI; the shared BC path is correct; official DXVK profiles establish
practical D3D9/D3D10/D3D11 tiers without spoofing; vkd3d hard requirements and
baseline are measured and basic D3D12 runs where capability permits; all three
Wine versions, Box64 host Vulkan, and FEX Vulkan thunking are measured; and
every remaining failure maps to a concrete capability.

Only then resume universal-GPU work.
