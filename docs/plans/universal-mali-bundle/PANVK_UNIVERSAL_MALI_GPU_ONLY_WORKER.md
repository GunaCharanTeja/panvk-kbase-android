# WORKER — Universal loadable Mali PanVK, GPU-only

**Revision:** 2026-09-25  
**Project:** `abhay-byte/panvk-kbase-android`  
**Position in roadmap:** M3, after verified M1 DXVK and M2 vkd3d-proton  
**Execution state of this document:** PLANNED; no implementation or new hardware tests performed by this review.

## 0. Authority, entry gate, and practical objective

The owner-directed order is:

**DXVK → vkd3d-proton → universal Mali loading/support → beta qualification → RC → stable.**

This worker expands M3 of `PANVK_MASTER_ROADMAP.md`; it does not replace the revised DXVK worker, change the agreed vkd3d target, or authorize switching away from an unfinished prerequisite. The linked `feature/g615-consumer-completion` branch resolved to `052a5d37315e621082436fc9fa2745d0264347e2` when inspected. That snapshot is a reference, **not the future integration base**. Its consumer-first priority and Phase 24 boundary are older than the owner's current order. [R1, R8]

### Entry evidence, before changing universal-driver code

Use the actual completed DXVK/vkd3d development checkout. Read the existing final reports and their referenced test results. Record their exact source commits, Mesa/patch pins, binary hashes, test-list revisions, device identity, and the vkd3d feature levels that were agreed before implementation. All mandatory gates for those targets must pass; merely creating a report is insufficient.

If either prerequisite is incomplete, record `DEFERRED_WAITING_FOR_DXVK_VKD3D` and the exact failed/unrun gate. Do not lower the target, turn a capability blocker into a PASS, or restart the older branch. The documents and donor pins may be prepared now; this implementation begins after the gate passes.

Create/resume `feature/universal-mali-loadable` from the **verified M2 integration commit**, preserving current work. If prerequisite work is on different branches, integrate it deliberately with its regression evidence before establishing this base. Never reset to beta.3, `052a5d3`, or the donor release to get a convenient clean checkout.

### Deliverable

A common, maintainable driver distribution that selects a compatible GPU/backend and load route, executes graphics on the real Mali GPU, and exposes only implemented capabilities. It must include functioning app-local Android and native glibc routes for the claimed targets, repeatable device tests, clear failures on unsupported combinations, and an exact support matrix.

The goal remains **any Mali GPU**. An early multi-family release is useful, but is not universal completion. Model/family coverage, specific board/kernel coverage, app integration, graphics features, and consumer compatibility are separate dimensions. Unknown or untested devices remain visible gaps, not implied successes. A safe rejection is good loader behavior, **not support for rendering on the rejected GPU**.

## 1. Execution rules inherited from the owner

Graphics work stays on the GPU: `HARDWARE_NATIVE` or `GPU_LOWERED`. No CPU texture decoding, shader interpretation, geometry/tessellation/clipping/rasterization, software renderer, or CPU rebuilding of follow-up draws from downloaded GPU-generated results. No silent vendor-Mali renderer fallback. Ordinary compilation, command recording, memory management, and host/KCPU synchronization remain allowed. Test-side reference calculations and final output readback remain outside the production rendering path.

Use one fresh built-in OpenCode `general` child for one bounded implementation/build/device-test task. The parent only dispatches. No custom-agent setup, reviewer agents, approval phases, nested child trees, or orchestration framework. Necessary code inspection and debugging belong to the implementation task. Tests are not optional.

Target substantially below 200,000 tokens per child/execution subphase: aim for 50,000 reported cumulative input+output, checkpoint around 80,000, hand off by 100,000, respecting any smaller model limit. Count repeated inputs, and cached/reasoning usage where reported. Without runtime metering, use at most eight working tool calls plus two checkpoint calls and bounded excerpts; mark the budget unmetered. These are conservative execution rules, not a claim that a prompt enforces a numerical ceiling. [E6 for the built-in agent mechanism]

Use one sequential checkout and reuse existing scripts. Save multi-command work in scripts and invoke short commands. Save full logs; return at most 80 words per child. One compact `worklogs/universal-mali/NEXT.md` is sufficient. Keep the support matrix and test evidence because the driver needs them, not to create an agent-management system.

## 2. What the reviewed sources establish

The base project already separates Android/Bionic and ARM64 glibc artifacts, requires kernel/app-UID probing, and has profile-selected patch families. Preserve that structure. A profile selects code and quirks; it does not create hardware support. [R3–R5]

The requested new donor is **G720 `0.1.0-beta.2`**, resolving directly to commit `f1d7bed571766c49e5dd464f92d1fda264612311`. Its release is experimental, with no universal-compatibility or conformance claim. The existing project lock already has an older pin for the same repository; keep that ancestry and add this release as a distinct reference. [R2, D1, D2]

The donor README documents focused GPU tessellation execution and a restricted internal GPU-wait path, while retaining broader coverage gaps. Its older and newer checkpoint descriptions must not be collapsed into one tested feature list. [D3]

G52 is the existing Bifrost/JM reference; its chronological notes progress beyond early enumeration to GPU compute, offscreen graphics and limited X11 presentation. G57 is the separate Valhall-v9/JM reference and explicitly lacks WSI/presentation at the pinned status. Do not copy early historical status or assume G52 descriptors implement G57. [J1, J2]

Everything below is the **proposed engineering plan**. Source-reported results are inputs for reproducing behavior, not new results from this project.

## 3. Distribution and runtime architecture

```text
Supported application or managed runtime
    → ABI-appropriate loading adapter / Vulkan loader
    → common PanVK Vulkan frontend + shared GPU feature implementations
    → architecture/compiler/descriptor implementation selected from real GPU identity
    → Kbase CSF or Kbase JM transport selected from verified kernel interface
    → installed kernel driver / firmware
    → physical Mali GPU
```

### 3.1 One project, not one incompatible binary

Prefer one coherent Mesa source tree and patch stack with runtime architecture dispatch. Build a multi-architecture library per ABI where the implementations compose safely. If a real toolchain or architecture incompatibility requires several binaries, ship them as declared alternatives in one distribution and select one before Vulkan initialization. Do not fork all shared BC, pre-raster or consumer code per GPU.

Keep at least these packages distinct:

- `android-aarch64-bionic`: in-process custom driver, Android-native dependencies and WSI.
- `linux-aarch64-glibc`: native ICD and its JSON manifest, the tested glibc baseline and WSI dependencies.

Do not load Bionic code into glibc or treat musl as glibc. Native 32-bit processes, including older Android devices, need an explicitly built/tested ABI; do not claim them because an arm64 package exists. CPU-ISA translation and its Vulkan thunking remain external integration concerns, not a CPU graphics fallback and not evidence of a 32-bit-native driver.

### 3.2 Loading is an application contract

For Android, preserve the existing explicit in-process ICD route and test its real entrypoints and window support. For glibc, use the normal Vulkan loader and an unambiguous manifest path. Android's platform loader/HAL and native WSI ownership are different from desktop JSON-ICD loading. A desktop environment variable does not make every Android app select this library. [R3, E2, E4]

Do not build a new Vulkan forwarding wrapper unless a specific host interface demands it and the existing adapter cannot meet that contract. Prefer direct loading with correct dispatch over wrapping every Vulkan function. Never mix handles or proc-address tables from vendor and custom drivers.

AdrenoTools-style ZIP compatibility is a **separate host-adapter target**. An Adreno-oriented loader is not automatically a Mali loader, and applications must integrate a custom-loading mechanism. Preserve `libvulkan_panfrost.so` as the canonical name; do not rename it to Freedreno and claim integration. [E5]

“Loadable” means safely selected and initialized at process startup. It does not mean swapping a library while its VkDevices, queues, callbacks, or surfaces are alive. Hold the library for its required lifetime; switch packages by a controlled process restart.

### 3.3 No global installation requirement

Use app-private or managed-runtime directories with explicit package selection. Do not overwrite `/vendor` libraries, flash kernels, chmod device nodes, disable SELinux, alter device security policies, or require privileged app execution. A route needing an OEM/kernel change is recorded separately; an ordinary ZIP cannot supply that kernel access.

## 4. Profile and capability contract

Extend existing profile data instead of adding a competing registry. Separate reusable **hardware/backend facts** from individual **validation records** and build metadata.

| Domain | Fields to preserve or add |
|---|---|
| GPU identity | Raw ID and source, product/revision/variant, architecture, core mask; explicit integer encoding. |
| Kernel transport | Kbase versus another kernel interface; CSF versus JM; UAPI family/major/minor; DDK and firmware facts where readable. |
| Transport limits | CSF register/stream/group limits or verified JM atom layout/stride; coherency, VA widths and supported memory operations. |
| Host | Process CPU ABI, libc, minimum OS API or glibc symbol requirement, process page size. |
| Presentation | Android native-window route or exact X11/Wayland transport, allocator/mapper contract, required node access. |
| Selection | Compiled architectures, implemented transport, exact matching rules, tested revisions and scoped quirks. |
| Evidence | Driver/library hashes, device build fingerprint, app UID/domain, loader version, test identities and results. |

Do not compare CSF UAPI `1.x` and JM UAPI `11.x` as versions of the same interchangeable ABI. Negotiate the correct interface and verify structure sizes; a higher minor number is not permission to assume every layout. Unknown layouts must not receive guessed ioctls.

Do not identify a GPU from marketing strings or a Vulkan ID printed by some other loaded driver. Resolve architecture using the actual raw properties and the selected Mesa model table. Preserve distinctions between raw GPU IDs, products, revision bits and Vulkan `deviceID`.

Profiles may select quirks and known-safe paths. Runtime features must still be derived from the actual compiled implementation, GPU/kernel capabilities and validated configuration. A profile must not replace real features with a copied G615 capability dump.

### 4.1 Status dimensions

Use independent fields, not a single “supported=true”:

```text
coverage: NOT_INVESTIGATED | IN_DEVELOPMENT | SUPPORTED | BLOCKED | UNSUPPORTED_WITH_EVIDENCE
execution: HARDWARE_NATIVE | GPU_LOWERED | UNSUPPORTED
loading/rendering/presentation/tests: PASS | FAIL | NOT_RUN | BLOCKED
consumer tiers: exact DXVK/vkd3d revision + profile/feature level + result
release maturity: dev | alpha | beta | rc | stable
```

Keep existing enums where needed and define an explicit mapping; never silently rename historical results. `UNSUPPORTED_WITH_EVIDENCE` needs a concrete reason and the next condition that could reopen it. It does not mean every unimplemented feature is impossible.

### 4.2 Detection failure behavior

Probe only documented/verified low-risk discovery operations first. Validate property lengths and counts before accessing arrays. Open the real device under the target app/runtime identity. Do not require readable sysfs DDK strings when the necessary UAPI facts are available through a supported probe.

Separate errors such as node missing, access denied, unknown GPU, unsupported UAPI, wrong ABI, OS too old, dependency missing, WSI transport unavailable, and device lost. Never collapse them into “Mali not supported.” Do not repeatedly try random GPU profiles on the same live context. New logical-device context setup must follow the kernel's actual lifetime and handshake rules.

## 5. Build and package correctness before expanding models

The reviewed Android build pipes Meson/Ninja output into `tail` under `/bin/sh`; the package script has G615 fallback metadata and can choose a generic feature matrix. These are concrete risks to recheck on the future integration base, because newer work may already fix them. [R6, R7]

Required changes/tests when still present:

1. Preserve the real build exit code while writing full logs and a short summary. An old `.so` must never be copied after a failed build. Test this with an injected failing build command and a pre-existing stale artifact.
2. Key mutable build/output locations by ABI, target API/libc baseline, architecture set, toolchain and source/patch identity. Reuse incremental builds only when that identity matches.
3. Distinguish source commit, Mesa base commit, patch-stack identity, build configuration and binary hash. A Mesa SHA alone is not the complete driver identity.
4. Remove missing-profile defaults to G615; unknown/incomplete metadata must fail packaging. Generate name, supported profiles and minimum API from the actual artifact configuration.
5. Associate capability/test records with the exact binary and device profile. Do not attach a global or G615 capture to a different GPU package.
6. Validate portable shell/Python behavior. The current packager's Bash-style substring expansion under `/bin/sh` is one compatibility case to test, not a reason to rewrite all automation.
7. Compose the required arch patch families deterministically. Applying only one profile's patches is not automatically a multi-architecture build. Introduce an explicit union build set with conflicts rejected; keep driver quirks runtime-scoped.
8. Keep license/source attribution with imported code. Verify downloaded donor asset digests before using them as comparison artifacts; a release title is not provenance.

No speculative Mesa upgrade is required. Preserve the completed M2 base unless a specific donor dependency or defect requires a deliberate update with regressions. Do not overwrite the whole Mesa tree with the donor fork.

## 6. G720 beta.2 integration contract

### 6.1 Exact reference

```text
repository: https://github.com/wonderkast02/panvk-g720-kbase-csf
tag: 0.1.0-beta.2
commit: f1d7bed571766c49e5dd464f92d1fda264612311
release published: 2026-09-24T22:42:17Z
                  2026-09-25 04:12:17 Asia/Kolkata
asset: PanVK-G720-0.1.0-beta.2.zip
GitHub-reported asset SHA-256:
fc1d69647c071ca3fe30ae2fb450e95c91c08e90779eb32c865fa384dff5aaca
```

The tag-to-commit mapping and release metadata were retrieved. Asset bytes and their manifest were not successfully downloaded in this review; the digest above is **reported metadata, not a local checksum verification**. The release's GitHub `prerelease=false` flag does not override its explicit beta/experimental description. [D1, D2]

The tagged README's reference G720 is MT6899 / MC8 / raw ID `0xc8700010`, Kbase r49p1, UAPI 1.30. Use that as the initial donor comparison target, not a universal mapping for all G720 devices. [D3]

### 6.2 Selective reuse order

First compare the common Kbase changes already imported at the older `d9cbb91…` pin against the new release. Produce a short list of relevant commits/files and their dependencies. This is a bounded implementation-preparation task, not another full-source audit report.

Port in this order, only where missing from the completed M2 base:

- Device/CSF property handling and architecture-specific initialization.
- Allocation/import/coherency and queue-lifetime correctness needed for basic GPU execution.
- Presentation transport needed by the chosen host.
- Eligible internal GPU-wait optimization, after correct synchronization works.
- Useful compiler/pre-raster/tessellation fixes not already implemented in the common driver.

For each imported change, record upstream commit/path, local patch, dependency and focused device result. Preserve both G615 and G720 regression behavior. Reuse semantic GPU algorithms, not command/descriptor encodings from another architecture.

### 6.3 Synchronization boundary

The donor's WAIT64 optimization is deliberately restricted to certain internal binary payloads, with separate handling for other payload types. Preserve that eligibility model until expanded behavior has its own tests; do not replace all fence/semaphore paths with one GPU wait. Ordinary CPU/KCPU synchronization handling is not CPU rendering. [D3, D4]

Tests must cover producer-before-consumer and consumer-before-producer submission where valid, same/different queues, binary reuse, imported sync files, timeline host waits/signals, mixed payloads, wait-only submits, timeout/error paths and device destruction. Prove waits do not hold resources needed by their producers. Measure benefit; do not enable an optimization merely because it exists in the donor.

### 6.4 Donor graphics is not automatic consumer completion

Use focused donor tessellation tests as regression inputs, then exercise the full contracts inherited from M1/M2. Geometry/tessellation advertisement or one rendering result does not establish BC, clip/cull, multiple viewports, XFB, queries, synchronization, or the selected D3D feature levels. Never substitute the donor's feature table for the completed common driver.

## 7. Memory, allocation, and presentation portability

### 7.1 Host page size and GPU/UAPI units

Remove accidental 4 KiB assumptions from host allocation, mapping and ELF packaging, but do not mechanically replace every 4096 with the host page size. Kernel ioctl units, GPU pages, granules and host pages may be distinct. Derive each from its own contract. Validate overflow, alignment, mapping lengths, executable-memory rules, cache maintenance and cleanup. Test real 4 KiB and 16 KiB host-page environments for every claimed route; ELF alignment alone is not device execution proof. [E3]

Keep VA sizes and heap reservations within the actual kernel/device limits. Do not propagate a G615 or G720 address-space reservation into older JM hardware. Measure memory behavior instead of allocating a maximum-sized speculative heap.

### 7.2 Allocator and mapper adapters

Do not assume `/dev/dma_heap/system` exists or is accessible to every app. Use a validated allocator appropriate to the route: Android hardware buffers, a supported dma-heap or another verified legacy allocator. Any legacy ION path needs the device's actual interface and focused tests; it is not a fallback to arbitrary ioctls.

For imported images validate fd ownership, allocation length, offset, stride, format, modifier/compression, planes, usage, CPU/GPU coherency and fence ownership. Do not blindly choose the first fd from a native handle. Vendor metadata/mapper differences need scoped adapters rather than guessed global rules.

Cover import failure rollback, duplicated handles, double close, process exit, image destruction while work is outstanding, and pressure/reuse. Hardware-buffer and external-memory extension bits must match the operations actually implemented.

### 7.3 Android presentation

Test under a normal app UID and real surface lifecycle, not only an adb/root harness. Keep direct-ICD Android WSI separate from the platform-HAL path. Exercise acquire/release fences, recreation, resize, rotation, background/foreground, zero-sized or lost surfaces, repeated launch, and window destruction with queued work. [E2 for the platform interface distinction]

Minimum Android API is artifact-specific. The reviewed packager and requested donor release say API 35; older targets require a real lower-API build and dependency test, not changing `minApi` in JSON. In particular, a profile for the G52 donor's API 34 device cannot assume it can load an API 35 binary. [R7, D1, J1]

### 7.4 glibc presentation

Implement/test the transport actually used by the managed runtime. A compiled X11 or Wayland option is not presentation proof. The donor documents a private raw-dma-buf X11 route requiring a compatible server, distinct from a wrapper/AHardwareBuffer protocol; MIT-SHM is a different, copying route. Negotiate or explicitly qualify that exact server contract. [D4]

Prefer GPU-rendered exportable images with no mandatory full-frame CPU copy. A CPU wait for an external fence is allowed bookkeeping; it is not equivalent to a CPU rasterizer. A SHM pixel-copy fallback is also not itself CPU rasterization, but must be explicit, measured and **must not count as passing the fast zero-copy route**. Default to a clear route failure rather than silently degrading a qualified package.

Keep Wayland as a separate claim when needed by a real host. Do not substitute ordinary DRM/panthor behavior on a Kbase fd, fabricate a DRM device, or assume Linux DRI3 semantics match a private Android X server extension.

## 8. Coverage waves and honest scope

Use the following proposed order. Device availability may change the order within a wave, but not erase validation requirements.

| Wave | Target | Engineering purpose and boundary |
|---|---|---|
| A | Completed G615 reference, then requested G720 target | Prove the common package preserves M1/M2 and supports a second CSF architecture/device. |
| B | Other available CSF targets: G610/G710, other G615 boards, then G725/newer supported models | Establish real revision/UAPI/vendor/allocator variation. Each model gets a measured result, not inherited support. |
| C | G52 Bifrost/JM, then G76 and other relevant Bifrost variants | Implement/qualify atom submission, completion and asynchronous synchronization using JM-specific contracts. |
| D | G57/G68 Valhall-v9/JM where available | Complete the separate v9 descriptor/compiler/job path and its missing WSI; reuse tests, not Bifrost layouts. |
| E | Midgard and Utgard/older Mali inventory | Attempt the missing architecture/backend work from concrete evidence; retain explicit gaps until actual GPU rendering/API requirements pass. |
| F | Additional ABIs and board/kernel combinations needed by that inventory | Native 32-bit and legacy platforms are separate builds/tests, not implied by arm64 loading. |

Mesa's upstream table has architecture-dependent API coverage, and Utgard belongs to Lima rather than Panfrost. Neither that table nor Lima GLES support proves a Vulkan implementation for this Kbase project. Do not present a packaging exercise as completing an absent Vulkan compiler/backend. [E1]

For every unsupported legacy family, identify the first absent contract—compiler, memory model, command submission, shader stage, descriptor/format semantics, kernel access or host ABI—and perform a bounded test or implementation increment. Keep a specific next action. Do not conclude impossibility from lack of an existing patch, but do not count a model-table entry or an unsupported-device dialog as GPU support either.

To release a useful subset while some of the any-Mali goal remains unfinished, obtain an explicit owner decision on the scoped support matrix. Label the release accordingly; do not silently convert partial coverage into “universal complete.”

## 9. Small implementation phases

Every numbered item below is a **milestone**, not one subagent assignment. The lettered tasks are starting microtasks and may be split further. Implement and test in the same child when practical. No separate review agents or approval loop.

### U0 — Open the universal work only after M1/M2

**Input:** completed DXVK/vkd3d evidence and actual integration checkout.

- **U0.a:** verify prerequisite results/identities and save the minimal resume pointer; otherwise return the exact blocked entry gate.
- **U0.b:** establish the universal development branch from that integration and capture one existing G615 smoke baseline without changing behavior.

**Exit:** known integration base, mandatory target tiers recorded, G615 candidate identity and one real baseline run. No restart of completed work.

### U1 — Stop stale or misidentified artifacts

**Files:** existing Android/glibc build and package scripts, package selfchecks.

- **U1.a:** reproduce/fix hidden build exit codes and stale-artifact copying; add injected-failure tests.
- **U1.b:** isolate or identity-check build configuration and replace G615/global-matrix metadata defaults with verified inputs.
- **U1.c:** build/package G615 once, deploy it, prove the new plumbing did not change its driver selection or smoke result.

**Exit:** a failed build cannot emit a success package, metadata matches the delivered binary, incremental reuse has an explicit identity check.

### U2 — Extend the real profile probe

**Files:** `profiles/`, `tests/kbase-probe/`, existing capability capture.

- **U2.a:** extend the existing safe probe with host ABI/page size, frontend/UAPI and presentation-access facts.
- **U2.b:** test parsers/matching on valid and malformed fixtures; unknown GPU/UAPI and missing app access must fail clearly.
- **U2.c:** run the probe as the target G615 app/runtime UID and compare with its existing profile.

**Exit:** machine-readable identity and reasoned failure results, with no fake capability values or privileged-only success.

### U3 — Prove portable selection on the already-working G615

- **U3.a:** make Android package selection deterministic with existing direct-ICD loader paths and library-lifetime rules.
- **U3.b:** make glibc manifest paths relocatable or install them deterministically; verify the real loaded library, not just an environment variable.
- **U3.c:** run cold launch, wrong-ABI, missing-dependency and rollback tests; run compute/offscreen/present on each claimed route.

**Exit:** package selection through a functioning host adapter and confirmed candidate GPU execution. No system replacement.

### U4 — Compose the multi-architecture source/build set

- **U4.a:** define the smallest common patch/build set for G615 plus G720; detect duplicated/conflicting patch dependencies.
- **U4.b:** add architecture/transport dispatch tests while keeping unknown combinations rejected.
- **U4.c:** reconstruct from pinned Mesa plus tracked patches, build and rerun G615 smoke before introducing new-GPU feature changes.

**Exit:** reproducible composed source, not two unrelated driver trees or metadata claiming uncompiled architectures.

### U5 — Prepare the exact G720 donor

- **U5.a:** verify `0.1.0-beta.2` still resolves to the recorded commit; fetch source and release metadata with checksums and local provenance.
- **U5.b:** compare only relevant changes since the prior donor pin; select the first missing property/init/transport increment.
- **U5.c:** add a G720 profile from an actual device probe, not by copying G615 or assuming every G720 has the donor's UAPI.

**Exit:** pinned reusable changes and actual target facts. If no G720 is accessible, mark device tasks NOT_RUN and continue only independent work; do not invent its result.

### U6 — Make G720 execute before chasing its features

- **U6.a:** implement initialization/register-limit differences and build the target candidate.
- **U6.b:** run a tiny compute workload with nontrivial input/output and a negative control, then a partial-triangle offscreen workload.
- **U6.c:** fix the first observed device failure; establish correct completion/lifetime behavior before optimization.

**Exit:** confirmed PanVK→Kbase→G720 execution with exact output and candidate identity. Enumeration and empty completion events are insufficient.

### U7 — G720 memory and display path

- **U7.a:** implement the selected allocator/import contract and validate memory/fence ownership.
- **U7.b:** implement one actual Android or glibc display route and run bounded presents/recreation; then qualify the second promised ABI/route.
- **U7.c:** test process restart and pressure/reuse; record any copying/bridge route separately.

**Exit:** working presentation under the normal host identity, not only offscreen GPU success.

### U8 — Qualify inherited DXVK/vkd3d behavior on G720

- **U8.a:** run the exact inherited feature/profile tests and find one real semantic gap at a time.
- **U8.b:** port/implement needed shared GPU-lowered stages with per-architecture resource/command handling; run focused Vulkan tests and executed CTS.
- **U8.c:** run actual Native DXVK and vkd3d workloads for each claimed tier; fix, retest and retain unsupported tiers explicitly.

**Exit:** the declared G720 consumer targets have device+render+presentation evidence. A lower temporary tier is progress, not completion of the higher selected target.

### U9 — Add donor optimizations only on a correct base

- **U9.a:** port eligible GPU-internal waits if they solve a measured host-wait cost; test eligibility and ordering.
- **U9.b:** run mixed/internal/external synchronization and lifetime cases, plus G615 regressions.
- **U9.c:** measure frame time, host wait time and memory before/after; keep the simpler correct path if the optimization is not proven beneficial/safe.

**Exit:** no new synchronization failures or hidden CPU-render fallback. This optimization must not delay fixing a correctness blocker.

### U10 — Expand CSF board and architecture coverage

Repeat for one target at a time:

- **U10.a:** probe an available G610/G710/other G615/newer CSF target and classify differences.
- **U10.b:** implement one scoped quirk or arch change and run compute/offscreen/presentation immediately.
- **U10.c:** run claimed consumer tiers and regression cases; add only that validated tuple to support data.

**Exit:** multiple measured CSF targets with exact UAPI, OS, ABI and WSI coverage. Do not turn one successful G720 into support for all CSF chips.

### U11 — Bring up Bifrost/JM using G52

- **U11.a:** use the pinned G52 reference to implement/verify JM context, atom layout, memory and completion handling.
- **U11.b:** run GPU compute and partial-triangle tests before enabling display; distinguish a dependency-only atom from a real hardware job.
- **U11.c:** wire output semaphore/fence behavior and test repeated submissions, errors and resource destruction; do not leave a sync-object type mismatch hidden in release builds.

**Exit:** real JM GPU execution and correct synchronization. Source-derived G52 fixes must be scoped, not generalized to every Kbase version.

### U12 — JM WSI, async behavior and consumer features

- **U12.a:** build the correct API/ABI artifact for G52 and qualify its actual allocator/display route.
- **U12.b:** implement required asynchronous/dependency behavior and port shared GPU feature paths with JM barriers/jobs.
- **U12.c:** run focused CTS, inherited consumer workloads and repeated present; then repeat the validated sequence on G76/other selected Bifrost hardware.

**Exit:** the claimed JM packages/tier results work without requiring a CSF queue implementation or copying G615 assumptions.

### U13 — Complete Valhall-v9/JM as a separate backend path

- **U13.a:** reproduce pinned G57 compute/offscreen evidence in the integrated tree and reconcile resource/FAU/descriptor layouts.
- **U13.b:** implement the missing WSI and synchronization pieces and run actual display tests.
- **U13.c:** port the required shared GPU feature implementations, then run claimed DXVK/vkd3d tiers; repeat on another v9 model when it is part of the promised scope.

**Exit:** positive v9 rendering/presentation/consumer evidence. Adding 9 to an arch list is not this implementation. [J2]

### U14 — Legacy-family and additional-ABI completion work

- **U14.a:** populate the remaining Midgard/Utgard/other models using actual compiler/kernel/API evidence and identify the first absent contract per family.
- **U14.b:** implement/test the smallest real GPU path for the selected legacy target; add a missing Vulkan backend/compiler path where required rather than substituting GLES success.
- **U14.c:** create needed native 32-bit or lower-OS packages and run them in those processes/devices; maintain explicit unresolved kernel/hardware/API limitations.

**Exit:** each promised family is either actually implemented and validated, or an explicit unfinished gap. This phase cannot be marked universally complete by classifying every difficult target as unsupported.

### U15 — Multi-profile package and host integration

- **U15.a:** package the integrated architecture set with generated manifests, exact hashes, ABI/API requirements and safe install paths.
- **U15.b:** exercise real custom-driver import hosts sequentially: the existing app-local adapter, managed glibc loader, then requested Winlator/GameHub-style host adapters with source/version-specific contracts.
- **U15.c:** test paths with spaces, corrupt/partial packages, missing dependencies, unknown profiles, upgrades and rollback; keep all driver-owned handles within one selected ICD.

**Exit:** one coherent distribution mechanism across the promised hosts; an app that lacks custom loading remains an explicit integration gap, not an implicit global installation target.

### U16 — Cross-family regression and universal scope result

- **U16.a:** run the full promised ABI/GPU/UAPI/WSI matrix on exact package builds; compare inherited M1/M2 results.
- **U16.b:** run combined CTS subsets, memory pressure, pipeline cache/replay, sync and lifecycle workloads across representative paths.
- **U16.c:** generate the completion report and support matrix from actual evidence. Preserve NOT_RUN, failures and copying routes.

**Exit:** M3 COMPLETE only for the explicitly satisfied objective. If broad all-Mali coverage is unfinished, report PARTIAL; obtain owner approval before promoting a narrower matrix into the release track. Do not manufacture success from extension counts.

### U17 — Beta qualification for the declared release matrix

- **U17.a:** freeze the promised profiles/ABIs/routes/consumer tiers and their acceptance criteria.
- **U17.b:** run the beta gate in Section 11; fix actual problems and rerun affected cases plus regressions.
- **U17.c:** produce installation/diagnostic/rollback instructions and one reproducible candidate package set.

**Exit:** an evidence-qualified beta candidate, not a public release action. New unqualified profiles remain experimental and excluded from the stable selection set.

### U18 — RC qualification

- **U18.a:** reconstruct the candidate from tracked sources and test the delivered defaults, not a debug-only configuration.
- **U18.b:** complete RC lifecycle/soak/cross-host gates and resolve mandatory failures.
- **U18.c:** freeze the exact RC identity and known limitations. Any material fix creates a new candidate with appropriate reruns.

**Exit:** ready-for-RC evidence. Publishing/tagging needs explicit authorization; GitHub's prerelease checkbox is not the maturity gate.

### U19 — Stable qualification and handoff

- **U19.a:** run extended stability/memory/consumer tests on the exact RC packages and validate the rollback package.
- **U19.b:** verify every stable claim has matching device evidence; keep remaining universal-coverage gaps visible.
- **U19.c:** produce the stable-readiness report with exact artifacts, tests, known limits and next unsupported target.

**Exit:** stable-ready only for the approved scope and actual passing candidate. Do not claim formal Vulkan certification from selected CTS execution.

## 10. Test ladder used by each new target

Reuse the existing tests rather than copying a complete suite for every GPU. Parameterize target/ABI/loader inputs and preserve independently meaningful checks.

| Gate | Required evidence |
|---|---|
| G0 — access and selection | Target process identity, node/allocator access, actual GPU/UAPI, expected artifact/ABI; clear negative cases. |
| G1 — initialization | Exact loaded library and Vulkan driver identity; instance/device/queue creation and clean failure/destruction. |
| G2 — GPU compute | Nonconstant inputs, deterministic outputs, bounds and synchronization checks; job execution evidence. |
| G3 — GPU graphics | Partial triangle plus clear background, textures, depth/stencil/blend/MSAA and ordinary fast-path checks. |
| G4 — display | Actual surface, repeated presents, fences, resize/recreation, cold launch and selected transport evidence. |
| G5 — required Vulkan semantics | Every newly exposed feature/format/limit, cross-feature interactions and applicable executed CTS. |
| G6 — consumer tiers | Correct evaluator plus requested/reported feature level, actual rendering and presentation under exact DXVK/vkd3d revisions. |
| G7 — release behavior | Memory/lifetime/soak/cache/rollback/performance results on delivered package configurations. |

A new target advances through the ladder even when the source has another device's PASS. Tests may reuse algorithms and expected results, but not another GPU's actual results.

Record each result with source/patch/binary identity, device/build/UAPI, ABI/loader/WSI, test and CTS revisions, list hash, status and raw log paths. Record pass/fail/skip/not-supported/crash/timeout/device-lost counts. A candidate feature hidden during every test cannot get a positive feature gate.

For GPU-only proof, combine the code path selected by the candidate, loaded-library identity, real dispatch/submission evidence and result checks. A constant “fallback count = 0” alone is not evidence. Do not demand unavailable privileged counters; use the existing unprivileged traces where possible and state the remaining proof gap.

## 11. Proposed release thresholds — freeze before qualification

These are **new project acceptance proposals**, not source-reported completed tests or performance promises. Keep the existing stricter requirements where applicable. Set workload-specific performance/memory tolerances before running qualification and never relax them afterward just to pass.

| Stage | Minimum proposed execution |
|---|---|
| Development microtask | Small relevant workload; bounded initial submit/allocation; focused regression after a driver change. |
| Target beta qualification | At least 300 verified presents; a 10-minute relevant consumer loop; 100 surface-recreation cycles; 10 cold launches; corresponding semantic/CTS gates. |
| RC qualification | At least two independently captured successful runs per promised profile/ABI/WSI tuple; 60-minute mixed workload on each promised tuple; 1,000 lifecycle/recreation cycles per distinct WSI implementation; representative memory pressure and async synchronization. |
| Stable qualification | An 8-hour mixed-workload soak on a representative of every distinct frontend/UAPI-implementation/WSI combination; the full declared matrix still passes its RC requirements; 30 cold launches on each promised target; install/upgrade/rollback and persistent-cache checks. |

Require zero attributable device losses, hangs, memory corruption, validation-failing mandatory cases or silent fallback in acceptance runs. Distinguish unrelated infrastructure failures and rerun them; do not mask driver failures as infrastructure. Capture thermal conditions, clocks when readable, memory, host work and frame-time distributions. No universal FPS target is invented.

Fault injection is separate from normal qualification: deliberate failure tests may generate expected errors but must demonstrate bounded cleanup. Start new hardware with small workloads and hard timeouts. After a severe fault save available evidence and stop repeated risky submissions; do not flash or modify a kernel as an automatic recovery step.

## 12. Minimum deliverables and script policy

Reuse these existing paths where still applicable:

```text
profiles/
patches/common/  patches/kbase-common/  patches/csf/  patches/csf-v*/  patches/jm-v*/
patches/android/  patches/app-loader/  patches/wsi/
sources.lock
scripts/apply-patches.sh
scripts/build-android.sh
scripts/build-glibc.sh
scripts/package-android-adpkg.sh
scripts/package-glibc.sh
scripts/validate-package.sh
tests/kbase-probe/  tests/vulkan-smoke/  tests/compute/  tests/offscreen/
tests/android-loader-app/  tests/ahb/  tests/android-surface/  tests/sync/
```

Reconfirm actual names on the future M2 base; do not create duplicates when newer scripts already solve the problem. The existing DXVK/vkd3d tests remain the consumer source of truth.

Add only the missing parameterized tests/helpers and one support report:

```text
tests/universal/                    profile/ABI/package/negative cases
validation/universal/<profile>/     new evidence; preserve historical G615 files
validation/universal/support-matrix.json
validation/universal/UNIVERSAL-COMPLETION.md
worklogs/universal-mali/NEXT.md
```

The delivered reference JSON accompanying this plan is a **planning/provenance input**, not a replacement `sources.lock`. Import it into the existing lock schema only when implementing U5 and recompute affected identities properly.

When no equivalent runner exists, add one small `scripts/universal/run.py` wrapper around existing scripts. Proposed short invocations after implementation:

```sh
python3 scripts/universal/run.py probe --profile g615-v11-csf
python3 scripts/universal/run.py smoke --profile g720-v12-csf --abi android-aarch64-bionic
python3 scripts/universal/run.py qualify --profile g720-v12-csf --stage beta
```

These are proposed interfaces, **not commands asserted to exist today**. Require an explicit device serial when multiple devices are attached. Save job IDs to avoid duplicate builds; collect exit codes without frequent polling. Return a few status/count/path lines. Never hide failures using `|| true`, suppress stderr, or report the exit code of an output filter as a build/test result.

## 13. Completion and next handoff

A universal phase report must make these independent conclusions explicit:

```text
PREREQUISITE_DXVK=
PREREQUISITE_VKD3D_AND_TARGET_TIERS=
INTEGRATION_COMMIT=
MESA_AND_PATCH_IDENTITY=
PACKAGES_AND_BINARY_HASHES=
G720_DONOR_TAG_AND_COMMIT=
G720_DONOR_ASSET_BYTES_VERIFIED=
SUPPORTED_GPU_KERNEL_ABI_WSI_TUPLES=
GPU_EXECUTION_PATHS=
DXVK_TIERS_PER_TUPLE=
VKD3D_TIERS_PER_TUPLE=
UNTESTED_OR_BLOCKED_MODELS=
ALL_MALI_OBJECTIVE=COMPLETE|PARTIAL
SCOPED_RELEASE_AUTHORIZATION=
BETA_READY=
RC_READY=
STABLE_READY=
ACTUAL_TEST_COUNTS_AND_LOGS=
NEXT_SMALL_TASK=
```

Loadability, true graphics execution, consumer compatibility and maturity must never be merged into one Boolean. Keep the future all-Mali objective visible after a scoped release. Continue implementing the next concrete missing target, rather than repeatedly writing unsupported-feature reports.

## Sources and review boundary

The referenced branch, named documents, donor tag metadata/README/WSI notes, profile/build/package scripts and existing JM reference documentation were inspected. This was **not** a full audit of all Mesa code, a driver build, a donor-binary verification, or a new device test. Moving documentation below was consulted on 2026-09-25; pin exact revisions during implementation when it becomes a build or test input.

The companion `PANVK_UNIVERSAL_ROADMAP_REVIEW.md` explains differences from the older plan. The owner-provided `PANVK_MASTER_ROADMAP.md`, especially M3–M5, remains the source for the project sequence and wider goal. This worker supplies the detailed implementation and acceptance proposal.

- **[R1] Requested branch snapshot and consumer plan:** https://github.com/abhay-byte/panvk-kbase-android/blob/052a5d37315e621082436fc9fa2745d0264347e2/docs/plans/g615-consumer-completion.md
- **[R2] Existing source and donor pins:** https://github.com/abhay-byte/panvk-kbase-android/blob/052a5d37315e621082436fc9fa2745d0264347e2/sources.lock
- **[R3] Existing portability contract:** https://github.com/abhay-byte/panvk-kbase-android/blob/052a5d37315e621082436fc9fa2745d0264347e2/docs/PORTABILITY.md
- **[R4] Existing Kbase profile/probe contract:** https://github.com/abhay-byte/panvk-kbase-android/blob/052a5d37315e621082436fc9fa2745d0264347e2/docs/KBASE-PROFILES.md
- **[R5] Current profile-specific patch application:** https://github.com/abhay-byte/panvk-kbase-android/blob/052a5d37315e621082436fc9fa2745d0264347e2/scripts/apply-patches.sh
- **[R6] Current Android build implementation:** https://github.com/abhay-byte/panvk-kbase-android/blob/052a5d37315e621082436fc9fa2745d0264347e2/scripts/build-android.sh
- **[R7] Current Android package implementation:** https://github.com/abhay-byte/panvk-kbase-android/blob/052a5d37315e621082436fc9fa2745d0264347e2/scripts/package-android-adpkg.sh
- **[R8] Recorded driver/consumer status:** https://github.com/abhay-byte/panvk-kbase-android/blob/052a5d37315e621082436fc9fa2745d0264347e2/README.md
- **[D1] Requested G720 release:** https://github.com/wonderkast02/panvk-g720-kbase-csf/releases/tag/0.1.0-beta.2
- **[D2] Exact release tag reference:** https://api.github.com/repos/wonderkast02/panvk-g720-kbase-csf/git/ref/tags/0.1.0-beta.2
- **[D3] G720 source README at release commit:** https://github.com/wonderkast02/panvk-g720-kbase-csf/blob/f1d7bed571766c49e5dd464f92d1fda264612311/README.rst
- **[D4] G720 Kbase/WSI documentation at release commit:** https://github.com/wonderkast02/panvk-g720-kbase-csf/blob/f1d7bed571766c49e5dd464f92d1fda264612311/docs/panvk-kbase.md
- **[J1] Existing G52 donor pin and chronological hardware reports:** https://github.com/LukeValen/panvk-mali-g52/blob/dd2d0ee7f2dcd518d9ab6291afc0b1518c128b21/README.md
- **[J2] Existing G57/v9 donor pin and labelled status:** https://github.com/Noysz/panvk-g99-jm/blob/0a4f0e248df7ed990d219f30d78d796b4983b6f7/README.md
- **[E1] Mesa Panfrost hardware/API coverage:** https://docs.mesa3d.org/drivers/panfrost.html
- **[E2] AOSP Vulkan driver loading and WSI:** https://source.android.com/docs/core/graphics/implement-vulkan
- **[E3] Android native-library page-size guidance:** https://developer.android.com/guide/practices/page-sizes
- **[E4] Khronos Vulkan platform distinctions:** https://docs.vulkan.org/guide/latest/platforms.html
- **[E5] AdrenoTools integration scope:** https://github.com/bylaws/libadrenotools
- **[E6] OpenCode built-in subagents:** https://opencode.ai/docs/agents/
