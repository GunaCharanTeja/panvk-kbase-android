# PanVK Kbase Android — Master Roadmap

**Roadmap revision:** 2026-09-20  
**Repository:** `abhay-byte/panvk-kbase-android`  
**Owner-directed sequence:** DXVK → vkd3d-proton → Wine/consumer testing → universal Mali loading/support → beta-to-RC qualification → stable  
**Active implementation phase:** M1 — DXVK on the G615 reference target  
**Execution constraint:** GPU-only graphics implementation; no CPU graphics fallback.

## 1. Project mission and document hierarchy

Deliver a GPU-executed Vulkan driver project that first supports DXVK, then vkd3d-proton, then expands into a universally loadable Mali driver distribution, and finally graduates from beta through release candidate to stable.

“DXVK ONLY” in a phase worker is a temporary execution boundary. It does not remove vkd3d-proton, other Mali families, loader portability, or stable releases from the project mission.

Use two levels of planning:

- **This master roadmap** owns project scope, phase order, cross-phase constraints, and release qualification.
- **A phase-specific WORKER.md** owns the implementation, tests, and evidence needed for its current milestone. The companion `PANVK_G615_DXVK_GPU_ONLY_REVISED_WORKER.md` remains the detailed M1 worker. This roadmap does not replace its rendering-correctness requirements.

Advance the primary implementation effort in the owner's requested order. Reusable interfaces, regression tests, evidence capture, and clean packaging should be developed from the beginning, but are not permission to start several full feature programs at once.

This document records the requested future plan. It does not claim any new driver implementation, hardware test, repository commit, or release promotion.

## 2. Non-negotiable execution and evidence rules

Production graphics operations must execute on the GPU, using native hardware functionality when available and correct GPU shader/compute lowering when necessary. The restriction applies to BC decoding, geometry processing, tessellation, transform feedback, raster-related lowering, and future D3D12-required implementations.

Do not introduce CPU texture decoding, CPU shader execution, CPU geometry/tessellation, software rasterizers, or a CPU readback-and-rebuild loop for generated draws. Ordinary CPU shader compilation, command recording, memory management, and synchronization bookkeeping are allowed. Host-side test oracles must remain outside the production execution path.

Classify implementation paths separately as `HARDWARE_NATIVE`, `GPU_LOWERED`, or `UNSUPPORTED`. Keep execution mode independent from test status: `PASS`, `FAIL`, `NOT_RUN`, or `BLOCKED`.

Do not spoof Vulkan features, extension support, limits, GPU identity, or D3D feature levels. Do not count the vendor system driver as a passing custom-PanVK result. Do not bypass stock consumer checks to manufacture compatibility.

A safety test proving an unsupported feature remains disabled is valuable, but is not an implementation-completion result. All claims must identify the exact source revision, patch series, binary hashes, device, kernel/UAPI, loader, and test run.

## 3. Milestone M1 — Finish DXVK first

**Focus:** the existing G615 reference target and the revised GPU-only DXVK worker.

Retain the already selected DXVK 3.1.1 baseline ladder from the source plan [S1]:

1. D3D9.
2. D3D10 / D3D11 feature-level 10.1 operation.
3. D3D11 feature level 11.0.
4. D3D11 feature level 11.1.

The current worker remains responsible for the evaluator correction, GPU BC path, shared pre-raster implementation, clip/cull, non-solid modes, geometry, multiple viewports, transform feedback, tessellation, vertex-stage memory semantics, and their required tests.

Set up the actual DXVK Native build and presentation environment early. Each tier must progress beyond enumeration and device creation to verified rendering/semantic workloads. Preserve the existing ordinary graphics fast path and regression suite.

**M1 exit evidence:** selected baseline profiles pass with a corrected evaluator; corresponding real DXVK Native workloads pass; relevant Vulkan/CTS tests execute; stability, memory, cache, and presentation checks pass; older selected DXVK requirement checks are recorded; optional and unsupported functionality remains separately listed.

Do not let optional extension accumulation delay the agreed baseline. Conversely, do not remove a mandatory requirement merely to advance the milestone. Native validation establishes driver/translation-layer evidence, not universal Windows-game compatibility.

## 4. Milestone M2 — vkd3d-proton / D3D12

**Entry:** M1 has completed its agreed baseline with evidence. Freeze that evidence and keep DXVK regressions mandatory.

Pin the chosen vkd3d-proton revision and extract its README hard requirements, profile requirements, source-level device-creation checks, and D3D12 feature-level requirements separately. Do not infer the current release from an old plan.

The upstream project implements D3D12 over Vulkan and documents native development builds, including AArch64 test-build instructions [S2]. Use a verified native test route where suitable, without confusing it with a full end-user game integration result.

Bring forward the work deliberately deferred by M1: descriptor-limit stress, image robustness, query/statistics behavior, transform-feedback query/counter behavior, and other requirements of the selected revision and feature levels. The original plan explicitly deferred the 1M-descriptor proof and `robustImageAccess2` to this phase [S1]. Re-evaluate each requirement rather than assuming every deferred item is an unconditional blocker for every tier.

Use staged targets: verified D3D12 device creation, then real rendering/compute/descriptor/synchronization workloads, then each selected D3D12 feature level and its required tests. Keep optional capability tiers distinct.

**M2 exit evidence:** an explicit target/feature-level matrix; mandatory requirements and associated workloads pass for every claimed tier; no DXVK regressions; actual GPU execution and relevant CTS evidence; clear limits, unsupported tiers, and unresolved kernel/UAPI dependencies.

Preserve existing sparse-feasibility evidence as input, not a permanent universal verdict. New evidence may justify a new design. A mandatory unresolved capability blocks the corresponding tier; it must not be relabelled PASS or quietly dropped.

## 5. Milestone M3 — Universal Mali loading and GPU-family coverage

**Detailed plan:** `docs/plans/PANVK_UNIVERSAL_MALI_PLAN.md` (2026-09-29). It covers:

- the family matrix;
- wave order (v9 JM moved ahead of Bifrost);
- the patch-layout migration;
- the adrenotools/Winlator packaging plus wrapper matrix;
- the device-proof gates;
- the FristOneRR adopt/avoid list.

The 2026-09-25 bundle is archived in `docs/plans/universal-mali-bundle/`.

**Ordering:** M3 starts only after **DXVK (M1) → vkd3d-proton (M2) → Wine/consumer testing** all pass on G615 with evidence. Until then, only documents, pins and passive source study are allowed.

**Entry:** the agreed M2 milestone and the Wine testing milestone are verified. Universal Mali support becomes the primary implementation effort.

The owner's target remains **any Mali GPU**. Do not silently redefine that to the handful of devices already tested. Maintain an explicit family/model inventory with `SUPPORTED`, `IN_DEVELOPMENT`, `BLOCKED_KERNEL_OR_FIRMWARE`, `UNSUPPORTED_WITH_EVIDENCE`, or `NOT_INVESTIGATED` dispositions.

The proposed distribution architecture is one shared driver project and loader contract, with architecture-aware backends and appropriate ABI packages. Do not require one identical `.so` or an identical feature set on every device.

Mesa's published hardware table differentiates Vulkan support across Mali architectures; older Utgard GPUs belong to Lima rather than Panfrost [S3]. Thus, “Mali” branding alone is not proof that the present PanVK path can load or provide the same APIs. Treat legacy coverage as explicit architecture work, not a GPU-name whitelist edit.

### M3 implementation order

1. Define the profile and loader contract: GPU identity/revision, architecture, command frontend, kernel interface/UAPI, host ABI, and supported loading/presentation route.
2. Qualify closely related targets first where code reuse is demonstrated; then broaden to distinct architectures/frontends, including JM and legacy families as separately investigated work.
3. Produce Android/Bionic and ARM64 glibc packages for their intended consumer paths. Record any additional ABI as a separate deliverable.
4. Validate direct-ICD and standard Vulkan-loader routes where applicable, plus Android and desktop-style presentation routes actually claimed by each package.
5. Test app permissions, actual kernel access, vendor differences, synchronization, buffer import/export, lifecycle, and fresh-process behavior. An unsupported route must fail clearly rather than select another driver silently.

Keep these claims separate for every matrix entry:

`package selectable → library loadable → device created → GPU compute/render correct → presentation correct → DXVK tier → vkd3d tier → release qualification`

A successful load is not a DXVK or D3D12 pass. A Vulkan rendering pass is not proof that all advertised higher-level tiers work.

**M3 exit evidence:** a tested common distribution/loading mechanism; a published coverage matrix with exact qualification evidence for every claimed profile; GPU-only execution preserved; both consumer test suites rerun wherever their tiers are claimed; documented exclusions and their technical basis.

Any unresolved family remains an explicit gap in the “any Mali” objective. A scoped release can accurately name its tested matrix, but must not be described as universal completion while coverage gaps remain. Scope changes require owner approval, not a worker's silent reinterpretation.

## 6. Milestone M4 — Beta to release candidate

**Entry:** the universal-loading/coverage milestone is qualified for the explicitly declared release matrix. Keep the wider universal objective visible if any targets remain unresolved.

Beta is the feature-development and broader-test stage. Functional, multi-device, and long-running testing should accumulate before the RC decision; do not postpone all quality work until feature implementation ends.

Before RC promotion:

- Freeze the release support matrix, per-profile consumer tiers, source/dependency pins, and performance/memory acceptance thresholds.
- Build and verify immutable packages for each promised ABI/loading route, with hashes and reproducible provenance.
- Require relevant semantic, CTS, DXVK, vkd3d, loader/presentation, lifecycle, memory-pressure, and multi-device regressions to pass for the claimed scope.
- Resolve release-blocking hangs, device losses, memory-safety failures, corruption, silent fallback, and correctness failures. Quarantined tests remain visible and cannot silently count as passed mandatory coverage.
- Publish installation, diagnostics, known-limitations, upgrade/rollback, and bug-report instructions.

RC means a specific candidate is ready for final qualification. It is not simply the next filename after beta. New risky feature work stays off the candidate branch; fixes require appropriate reruns and a new candidate identity.

## 7. Milestone M5 — RC to stable

Promote the exact candidate only after it has passed the predefined release matrix, extended workload/soak tests, repeated lifecycle and cache tests, memory-pressure checks, and representative device/consumer validation.

Thresholds and soak criteria must be selected before qualification and recorded with the candidate. Do not shorten them afterward to turn a failing run into a pass. A material fix produces a new candidate and reruns the affected gates plus required regressions.

Stable requires no unresolved release-blocking issues within the published scope, a rollback path, complete package provenance, and a clear separation between supported tiers, known limitations, and experimental profiles.

A product stability label is not a substitute for formal conformance certification. Do not claim certification based only on selected CTS results.

Do not promote the whole Mali distribution because one G615 build passes. Per-profile qualification may be recorded, but the global release claim must match the coverage actually tested.

## 8. Fastest execution policy without sacrificing correctness

The speed objective is to reach verified consumer milestones with minimum rework, not maximize feature counts or audit-document volume.

Use one primary phase at a time. Within that phase, parallelize independent work with explicit interfaces and ownership: for example, GPU BC image work, pre-raster infrastructure, and test/evaluator work can be separated without beginning full vkd3d or universal-backend implementation early.

Use a short implementation loop: smallest real workload → code → focused tests → focused CTS → consumer regression → mergeable evidence. Reserve broad sweeps for integration points and release candidates. Reuse the existing probes and audits rather than rewriting them.

Keep hardware available for testing. Record disconnects and infrastructure failures as `NOT_RUN`/`BLOCKED`, not rendering results. During a device outage, continue independent build, unit-test, or implementation tasks; do not fabricate hardware evidence or declare the milestone complete.

Pin dependencies and upgrade deliberately. Prepare shared interfaces now so later phases can reuse code, but avoid speculative architecture intended to solve every Mali generation before G615 DXVK works.

Every task should name a concrete result: implemented behavior, regression fixed, executed test, or released qualified artifact. Research-only blockers must include the next falsifiable experiment or implementation step.

## 9. Progress and handoff contract

Recommended persistent project files:

```text
docs/plans/PANVK-MASTER-ROADMAP.md
docs/plans/DXVK-WORKER.md
docs/plans/VKD3D-WORKER.md
docs/plans/UNIVERSAL-MALI-WORKER.md
docs/plans/RELEASE-QUALIFICATION.md
validation/roadmap-status.json
validation/device-support-matrix.json
```

These are proposed repository destinations; creating this handoff file does not create or modify those repository files.

Every phase report should state:

```text
ACTIVE_PHASE=
EXACT_CODE_AND_ARTIFACT_IDENTITIES=
BEHAVIOR_IMPLEMENTED=
TESTS_EXECUTED=
CONSUMER_TIERS_VERIFIED=
GPU_EXECUTION_EVIDENCE=
REGRESSIONS=
UNTESTED_OR_BLOCKED=
NEXT_CRITICAL_PATH_TASK=
PHASE_EXIT_GATE_STATUS=
NEXT_PHASE_ENTRY_STATUS=
```

Worker handoff instruction:

> Read the master roadmap first, then execute only the active phase worker. The project sequence is DXVK, vkd3d-proton, universal Mali loading/support, RC qualification, then stable. Preserve GPU-only graphics execution and truthful capability reporting throughout. Keep later goals architecturally possible without diverting from the active milestone. Report code, hardware evidence, consumer behavior, and release readiness separately.

## 10. Source basis

[S1] User-provided `PANVK_G615_DXVK_ONLY_NEXT_WORKER.md`: Sections 0–5 for the reference target and DXVK ladder; Section 2 for deferred vkd3d-related work; Sections 26 and 34 for the source plan's sparse limitation and phase boundaries. Its technical corrections remain in the companion revised GPU-only worker. These are source-plan statements, not newly executed tests.

[S2] Official vkd3d-proton README, consulted 2026-09-20: project purpose, driver requirements, native development builds, AArch64 build/test route. `https://github.com/HansKristian-Work/vkd3d-proton`

[S3] Official Mesa Panfrost documentation, consulted 2026-09-20: differing architecture/API coverage and the Utgard/Lima distinction. Upstream coverage is not evidence that this Kbase fork supports a target. `https://docs.mesa3d.org/drivers/panfrost.html`

The phase ordering and GPU-only requirement are owner instructions from this conversation. Distribution architecture, tracking structure, work sequencing, and release gates above are proposed planning additions, not claims of already implemented functionality.
