# Universal Mali roadmap review and M3 addendum

**Reviewed:** 2026-09-25. **Result:** keep the owner's roadmap; replace the universal phase's high-level outline with the accompanying detailed worker. No driver code or remote repository was changed.

## 1. Which source was actually reviewed

The requested `feature/g615-consumer-completion` branch resolved to `052a5d37315e621082436fc9fa2745d0264347e2`, dated 2026-09-20. Its `docs/plans` directory listing contained `g615-consumer-completion.md`. The previously supplied master roadmap was reviewed separately from the local/conversation file. Do not present that older branch as the user's newer local DXVK progress. [R1, R8]

The consumer plan was written to finish named products before universal work, with universal work deferred through Phase 24. That is understandable for that plan's original scope, but is not the owner's latest priority. Its preserved tests and failure evidence remain useful; its ordering does not govern the new execution. [R1]

The master roadmap already specifies the correct DXVK → vkd3d → universal → RC → stable progression. Its universal section is a scope outline, not a finished implementation specification. The attached worker adds that missing engineering detail rather than replacing the wider objective.

## 2. Changes needed before running universal work

| Finding | Change in the new plan |
|---|---|
| Older consumer-first ordering conflicts with current owner order. | Universal starts only after the agreed, evidenced DXVK and vkd3d targets. Keep product-specific regression tests, but do not restart the old product-first sequence. |
| The linked branch is not a completed M2 base. | Branch from the actual future integrated DXVK/vkd3d result, not from the old snapshot or G720 fork. |
| “Universal loadable” can conceal several unrelated failures. | Separate package selection, ABI loading, kernel access, GPU execution, display, consumer tiers, and release maturity. |
| Profile-specific patch selection is not a universal build. | Compose a tested architecture set with runtime-scoped backend/quirk selection and deterministic patch dependencies. |
| Existing Android package metadata falls back to G615 values. | Fail on incomplete/unknown metadata and bind test matrices to the exact profile/binary. |
| Current Android build can lose failure status through output filtering. | Preserve tool exit codes and reject stale binaries; add an injected-failure test before expanding packaging. |
| API 35 packages do not automatically cover older Android targets. | Build/test the actual minimum OS/API and libc dependencies; do not lower metadata alone. |
| No fully specified any-Mali completion criterion. | Retain family/model gaps explicitly; require actual graphics support, not safe rejection, and explicit authorization for any narrower release scope. |
| RC/stable labels could be confused with feature milestones. | Freeze the supported matrix and exact candidate; qualify through device tests without equating a release checkbox or tag name with stability. |

These code findings are from the linked snapshot. Recheck the relevant files once on the future integration base; do not duplicate fixes already made by the current DXVK/vkd3d workers. [R5–R7]

## 3. Requested G720 reference is now included

Use the exact release **`0.1.0-beta.2`** at commit **`f1d7bed571766c49e5dd464f92d1fda264612311`**. It was published at `2026-09-24T22:42:17Z`, or **25 September 2026, 04:12:17 IST**. The published ZIP's GitHub-reported SHA-256 is `fc1d69647c071ca3fe30ae2fb450e95c91c08e90779eb32c865fa384dff5aaca`. The tag mapping was verified; the artifact bytes were not downloaded and hashed in this review. [D1, D2]

The original `sources.lock` already references this repository at `d9cbb91e5df720f0a62c4b5010d96c327aec315f`. Therefore this is a **new pinned checkpoint of an existing donor**, not a reason to replace the project's Mesa base. Preserve both identities and import only needed changes. [R2]

The release describes an experimental G720 Android driver, reports graphics/compute and GS/tessellation work, and leaves Winlator/DXVK/games in validation. Its source README provides more focused GPU tessellation evidence but lists continuing coverage work. Do not count these as complete DXVK, vkd3d, or cross-GPU results. [D1, D3]

The most useful donor inputs are property-driven CSF initialization, bounded internal GPU waits, allocator/display transport, and concrete GPU pre-raster/tessellation fixes. These are **candidates for reuse**, not promises that they apply unchanged to G615, other CSF revisions, or JM.

One important trap: CPU/KCPU handling of synchronization is not CPU graphics emulation. Another: a copying MIT-SHM presentation path is not the same result as a raw-dma-buf route. The new worker keeps both distinctions visible instead of disabling all host synchronization or treating every present as zero-copy. [D4]

The donor's general Kbase documentation also describes other devices and distro packages. Those broader inherited notes are not proof that the exact Android beta.2 ZIP was tested on every named device or ABI. Match each claim to its source revision, platform and binary.

## 4. Architecture decision

Keep a **single shared driver project** with ABI-specific artifacts and real architecture/frontend support. Prefer a composed multi-arch PanVK library when possible; permit explicit compatible variants when needed. The selector chooses a backend or artifact before initialization. It does not virtualize absent hardware features, rewrite GPU identity or bypass host requirements.

Keep Android in-process custom loading separate from the platform HAL. Keep glibc JSON-ICD loading separate from Bionic. A universal package cannot force arbitrary Android applications to use a custom driver; compatible applications or runtimes need an integration route. [R3, E2, E4, E5]

G615 is the regression anchor. G720 is the first proposed new target because the user supplied a recent pinned implementation reference. Expand other CSF targets next, then implement G52/Bifrost-JM and G57/Valhall-v9-JM with their different job/descriptor contracts. Legacy Midgard/Utgard stays a real implementation/investigation lane, not omitted coverage. Upstream API coverage and Lima GLES support are not this fork's Vulkan proof. [J1, J2, E1]

## 5. Revised master-roadmap insertion

Insert this under M3 of the existing master roadmap, preserving M1/M2 unchanged:

> Execute `PANVK_UNIVERSAL_MALI_GPU_ONLY_WORKER.md` only after verified DXVK and vkd3d completion for the agreed target tiers. Preserve their integrated code, tests and source pins as the universal base. Use the G720 `0.1.0-beta.2` release at `f1d7bed…` as a separately pinned donor, not as a replacement source tree. Implement portable loading, architecture/UAPI selection, memory/presentation paths and per-target GPU consumer behavior in small device-tested increments. Expand CSF, Bifrost/JM, Valhall-v9/JM, legacy families and required ABIs with explicit evidence. Retain every unresolved any-Mali gap. Advance to RC/stable only for an explicitly approved, fully qualified support matrix, without describing a scoped release as universal completion.

M4/M5 now reference U17–U19 and Section 11 of the worker for candidate qualification. The proposed soak/count thresholds must be frozen before testing; they are not claims of completed qualification.

## 6. Execution stays simple

One main-thread dispatcher; one fresh built-in `general` subagent at a time; one small code/build/device-test objective; no review agents and no custom configuration. Reuse scripts, retain full logs on disk, return compact status. A macro-phase can contain many such subphases; it must not be assigned wholesale to one child.

The 200k requirement is a ceiling, not a session-filling target. Use conservative early handoffs and actual runtime accounting where available. A prompt cannot certify exact usage in an unmetered runtime.

## 7. What was not established

This review did not build PanVK, run G615/G720/JM hardware, verify the downloaded donor ZIP, read every Mesa source file, certify Vulkan conformance, confirm DXVK/vkd3d completion on another branch, or publish changes. Architecture additions, phase sequence details, microtasks and qualification thresholds are proposed work. Repo-reported test results remain attributed to their original sources.

## Sources

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
