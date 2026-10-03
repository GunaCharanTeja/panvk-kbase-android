# PanVK Universal Mali Plan (M3)

**Revision:** 2026-09-29
**Roadmap slot:** M3 of `PANVK_MASTER_ROADMAP.md`
**Status:** PLANNED. This document adds no driver code and no new device results.
**Supersedes:** the high-level M3 outline. It updates the 2026-09-25 bundle, archived verbatim in `docs/plans/universal-mali-bundle/`. The bundle's worker (`PANVK_UNIVERSAL_MALI_GPU_ONLY_WORKER.md`) is still the detailed rulebook for sections 3–7 and 10–13. Where the two disagree, this file wins.

## 0. Entry gate and ordering

The owner-directed sequence is:

**M1 DXVK → M2 vkd3d-proton → Wine/consumer testing → M3 universal Mali → M4 beta→RC → M5 stable**

M3 code work may start only when every one of these holds:

1. The M1 DXVK agreed ladder (D3D9 → D3D11 FL11.1) passes on G615, with evidence as defined in `worklogs/g615-dxvk/CONTRACT.md`.
2. The M2 vkd3d-proton agreed feature-level matrix passes on G615.
3. The Wine testing milestone passes on G615. This means real Wine/Winlator-class runs (box64/FEX, DXVK, vkd3d) with this driver loaded, recorded per host (see §5.3). A Native-only DXVK/vkd3d pass does not meet this gate.

If any gate fails or was not run, record `DEFERRED_WAITING_FOR_<DXVK|VKD3D|WINE>` with the exact gate and do not start universal code.

**Allowed before the gate:** documents, donor pins, and passive source study.
**Not allowed before the gate:**

- patch reshuffles that change the G615 series;
- new-GPU profiles that affect the build.

Base the work on the verified integration commit that comes out of the Wine milestone. Never base it on `052a5d3` (the older consumer-completion branch) or on a donor tree.

## 1. Changes from the 2026-09-25 bundle

| # | Bundle | This plan |
|---|---|---|
| 1 | Entry gate after DXVK and vkd3d | Adds the Wine gate (§0) and host adapters as first-class test routes (§5). |
| 2 | OpenCode execution rules: one `general` child, fixed token caps | Dropped. Use this repo's coordinator/worker pattern (`worklogs/*/CONTRACT.md`, `NEXT.md`). Keep "one small implement→build→device-test task per worker". |
| 3 | Wave order: CSF (G615/G720/others) → Bifrost JM → v9 JM → legacy | Reordered by reuse and demand (§3). v9 JM (G57/G68/G77/G78) moves up to Wave B: it has the largest community user base and two working public donors. |
| 4 | Assumed per-profile patch dirs could be composed | Current repo fact: all 41 G615 patches, including arch-generic ones (backports 026–037, BC decode 022/039/040, gpu_prerast, clip/cull, fill mode), live in `patches/csf-v11/`. U4 must first move arch-generic patches into `common/`/`kbase-common/`/`csf/`. See §4.1. |
| 5 | G720 donor = beta.2 `f1d7bed` | Local refs are older: `work/ref-g720` = `d9cbb91` (docs/license layout only, no Mesa source); `work/ref-g720-beta` = `3549264` (`0.1.0-beta.1.9.4` source). U5 must fetch `f1d7bed` separately. |
| 6 | Vendor kernels were not a dimension | Adds an explicit vendor-kbase axis (MediaTek, Samsung Exynos, Google Tensor, Unisoc, Rockchip). A matching GPU ID does not mean the vendor kbase is compatible (§2, §6). |
| 7 | No external community donors beyond G720/G52/G57 | Adds FristOneRR (MIT, Mesa-derived, JM v9 + G52) as a candidate donor, with provenance rules (§7). |
| 8 | Consumer gaps on JM were generic | States the key value on offer: FristOneRR G57 users must run DXVK 1.10.3 for D3D10/11 because v9 lacks GS/tess/XFB. Our M1 GPU-lowered pre-raster work (`gpu_prerast`, libpoly GS/tess, XFB) is what can lift v9/Bifrost to DXVK 2.x/3.x, provided it is ported per arch and not spoofed. |
| 9 | Build-hygiene risks were "to recheck" | Recheck done: `scripts/build-android.sh:43-44` still pipes meson/ninja into `tail`, so a failed build can report success (U1.a still needed). `package-android-adpkg.sh` still hardcodes `"name":"PanVK Kbase G615"` and `minApi 35` (U1.b still needed). |

Every bundle rule not listed above is kept unchanged. That includes: GPU-only execution, no spoofing, independent status dimensions, the probe/failure taxonomy, page-size handling, allocator adapters, the synchronization boundary, the test ladder G0–G7, and the release thresholds.

## 2. Per-GPU-family matrix

Status reflects the repo as of 2026-09-29. "Donor" means a public tree with reported results; those results are not ours.

| Wave | Family / arch | GPUs (examples) | Frontend | Kbase UAPI seen | Typical SoCs / vendor kbase | Our profile | Our status | Best donor evidence |
|---|---|---|---|---|---|---|---|---|
| A0 | Valhall 4th-gen v11 | G615 | CSF | 1.21 | MTK D8300 (duchamp) | `g615-v11-csf` | **Anchor**: M1 in progress, 41 patches | — |
| A1 | 5th-gen v12 | G720, G625/G725 | CSF | 1.30 (r49p1, MT6899) | MTK D8400/D9300, D7400 | `g720-v12-csf` (stub) | NOT_INVESTIGATED; `patches/csf-v12` empty | wonderkast02 beta.2 `f1d7bed` (G720 MC8): graphics, compute, AHB, tess |
| B | Valhall v9 | G57, G68, G77, G78, G78AE | JM | 11.x (r32p1–r54p1) | MTK G99/G100/D6080/D1080, Kompanio 1300T; Unisoc T6xx/T8xx; Exynos 1280/1380 | `g57-v9-jm` (stub) | NOT_INVESTIGATED; `jm-v9` empty | FristOneRR beta 1.1.0 (G57 MC2 widely reported working; G68/G77 partial; Exynos 1380 G68 fails to load); Noysz `0a4f0e2` (no WSI) |
| C | Bifrost v7 | G52, G76, G51 | JM | 11.38 (Redmi 13C), r49.1 (A38) | MTK G85/G88/G95, Helio P-series; Unisoc T6xx; Exynos 9xx | `g52-v7-jm` (stub) | NOT_INVESTIGATED; `jm-v7` empty | LukeValen `dd2d0ee` + v0.0.1-alpha (Winlator VKCube, DXVK D3D9–11, but advertises unimplemented features, so not reusable as-is); FristOneRR (A38 G52 r1, 56-byte atom stride); G76 (G95) reportedly freezes |
| D | Valhall v10 + other CSF | G610, G710, G715, G925/Immortalis | CSF | 1.x (various) | Rockchip RK3588 (G610), Tensor G2/G3 (G710/G715), Exynos 2200+/Xclipse (not Mali; excluded), MTK D9200+ | `g610-v10-csf` (stub) | NOT_INVESTIGATED; `csf-v10/13/14` empty | funnymdzz base targeted Pixel 7 G710 on kbase; RK3588 also has upstream panthor (a separate route) |
| E | Bifrost v6 | G71, G72, G31 | JM | 11.x (old r1x–r2x possible) | Exynos 8890/9810, Helio P60/P70, older Unisoc | none | NOT_INVESTIGATED | none; FristOneRR "untested" |
| F | Midgard v4/v5 | T6xx/T7xx/T8xx | JM | old | Exynos 7xxx, Helio X, RK3288/3399 | none | NOT_INVESTIGATED; upstream PanVK has no Midgard Vulkan | none; needs a real backend decision |
| F | Utgard | Mali-400/450/470 | (Lima) | n/a | legacy | none | UNSUPPORTED_WITH_EVIDENCE candidate: no Vulkan-capable hardware model; Lima is GLES2 only | Record it; no Vulkan work planned unless the owner decides otherwise |

Vendor-kbase axis (a separate column per validation record):

- **MediaTek:** has most of the demand; JM r32–r54 and CSF r4x.
- **Samsung Exynos:** modified kbase. FristOneRR fails to load on Exynos 1380. Samsung phones with MTK SoCs work.
- **Google Tensor:** CSF, Pixel kernel. The funnymdzz base notes UMM mapping quirks.
- **Unisoc:** JM G57/G52 at lower budgets.
- **Rockchip:** BSP kbase or upstream panthor, which are different routes.

For each vendor, record: node path(s) `/dev/mali0..7`, SELinux label and app-UID access, VERSION_CHECK result, atom stride/struct sizes, and MEM_IMPORT/UMM behavior.

## 3. Wave order and rationale

1. **A0 → A1 (G615 → G720).**
   - Same frontend, adjacent arch.
   - Proves that the composed build (§4) and a second CSF target work without G615 regressions.
2. **B (v9 JM).**
   - Largest user demand: r/EmulationMediatekMali is dominated by G57/G99 and G720 users.
   - Two MIT/Mesa donors already show JM v9 submission working through Winlator.
   - This wave adds the JM transport, atom submission, and the JM completion model, reused by Wave C.
3. **C (Bifrost v7 JM).**
   - Reuses the JM transport from B.
   - Must implement the Bifrost descriptor/compiler paths and deal with the 48/56/64-byte atom stride variation.
4. **D (other CSF).**
   - Each board or vendor kernel is its own tuple.
   - Tensor and Rockchip bring vendor-kernel differences.
5. **E / F (legacy).**
   - For each family, identify the first missing contract (compiler, descriptor, submission) and do one bounded increment.
   - Never count GLES/Lima as Vulkan.

A family is never marked SUPPORTED on donor evidence. It must pass our ladder (§6) on real hardware of that family.

## 4. Build and patch architecture

### 4.1 Patch layout migration (U4, first code task after the gate)

The current `apply-patches.sh` series is `common kbase-common [android app-loader wsi …] csf csf-v$ARCH | jm-v$ARCH`, selected from a single profile.

Needed changes:

1. **Classify each `csf-v11/*` patch** into one of three groups:
   - arch-generic (Vulkan runtime/NIR/backports, BC decode compute, gpu_prerast framework, clip/cull lowering, polygon kernel);
   - CSF-generic (cmd-stream emission, CRC/`pan_kmod_bo_munmap` fix);
   - true v11 quirks.

   Then move them into `common/`, `kbase-common/`, `csf/` and `csf-v11/` respectively. The note in `NEXT.md` ("backports sit in csf-v11 because common/ placement breaks kbase-common/006 and csf-v11/022") is the known ordering conflict. Resolve it by rebasing those dependents, not by keeping the patches arch-scoped.
2. **Multi-arch union build:** `apply-patches.sh --profiles g615-v11-csf,g720-v12-csf,g57-v9-jm,…`
   - Takes the deduplicated union of series, in a deterministic order.
   - Rejects conflicts.
   - Builds `-Dvulkan-drivers=panfrost` once. PanVK already compiles `panvk_vX_*` per `PAN_ARCH`, so one `.so` carries every enabled arch plus both kbase transports. Note: upstream PanVK has no v9 in its arch list. Adding it is the v9 JM port itself (Noysz/FristOneRR did this), not a flag.
3. **Proof of the move:** the G615 single-profile apply still yields a byte-identical `src/`, and the ICD hash is identical (or explained) after the move.
4. **Runtime quirks:** keyed on `(gpu_id, arch, frontend, uapi, vendor)`, never on build-time profile.

### 4.2 Transport selection

- **VERSION_CHECK:** probe CSF (nr 52, UAPI 1.x) first, then JM (nr 0, UAPI 11.x). This is already the pattern in the funnymdzz/FristOneRR `kbase_kmod.c`, and must be confirmed against ours.
- **Struct layouts:** negotiate from the reported version with `STATIC_ASSERT`ed sizes (e.g. `cs_queue_group_create` 1_6 / 1_18 / current).
- **Unknown versions:** fail with a typed error. Never guess ioctls.

### 4.3 Build hygiene (U1, unchanged from bundle §5, now confirmed present)

- Remove pipe-to-`tail` exit masking and use an explicit log file plus the real exit code.
- Generate package metadata from the built profile set. Unknown profile → fail.
- Key build dirs on ABI, API, arch set, and patch-series id.

## 5. Loading and packaging

### 5.1 Artifacts

| Artifact | Contents | Consumers |
|---|---|---|
| `android-aarch64-bionic` `.adpkg.zip` | `libvulkan_panfrost.so` (bionic, multi-arch), `meta.json` (adrenotools-style: `schemaVersion`, `name`, `libraryName`, `minApi`, …), MANIFEST/SHA256SUMS/SOURCE/VALIDATION json | Winlator forks (Winlator Mali, Ludashi, Bannerlator, BannerHub), GameHub, Yuzu/Eden/Lemon-style Android emulators that support custom driver ZIPs, our `tests/android-loader-app` |
| `linux-aarch64-glibc` tarball | `libvulkan_panfrost.so` (glibc) + `panfrost_icd.aarch64.json` | glibc chroots, Wine-in-glibc runtimes, desktop loader tests |
| (later) `android-arm32`, lower-API builds | separate builds | only when a target device needs them and passes tests |

Packaging rules:

- Keep `libvulkan_panfrost.so` as the canonical name. Never rename it to `libvulkan_freedreno.so`.
- `minApi` must match the real build. Today it is 35. A lower API needs a real API-29/30/33 build plus a device test, because G52/G57 users are often on Android 11–14.

### 5.2 Host-adapter matrix (U15)

The Winlator-family hosts load the bionic `.so` in-process through an adrenotools-style hook. Presentation then goes through a **wrapper** layer:

- leegao `bionic-vulkan-wrapper`;
- the host's built-in "Wrapper original";
- other wrappers.

FristOneRR's field data shows the wrapper choice changes results:

- DXVK 2.x + leegao crashes on the first frame;
- D3D11 fails with "Wrapper original".

So each validation record must name the exact host app and version, wrapper and version, box64/FEX version, DXVK/vkd3d version, and driver package hash. Test the wrapper routes that users actually pick:

1. Wrapper original: D3D9/10/11/12.
2. leegao wrapper: D3D9/10/11/12.
3. Bannerlator default.

A wrapper-specific crash is a driver bug until proven otherwise. The wrapper is not patched to hide it.

### 5.3 Wine milestone carry-over

The Wine gate (§0) produces the first host-adapter records on G615. M3 reuses the same harness on every new tuple instead of building a new one.

### 5.4 Non-goals

- No system HAL replacement, root, SELinux change, or chmod of `/dev/mali*`.
- Apps without a custom-driver loader stay an integration gap.
- AdrenoTools compatibility means ZIP-format and loader-hook compatibility only. It is not a Qualcomm feature claim.

## 6. Gates (device proof required)

This extends bundle §10. A gate is PASS only with these artifacts:

- the exact package hash;
- `getprop ro.build.fingerprint`;
- raw GPU ID and UAPI version from our probe, run as the app UID;
- the loaded-library proof (`/proc/<pid>/maps` shows our `.so`, and `VkPhysicalDeviceDriverProperties` shows PanVK);
- the test log path.

| Gate | Evidence |
|---|---|
| G0 access | app-UID open of `/dev/mali*` + VERSION_CHECK + props; negative cases (wrong ABI, unknown UAPI) produce typed errors |
| G1 init | instance/device/queue create + destroy ×10, no leaked fds/BOs |
| G2 compute | nonconstant SSBO compute + negative control; JM: a real job atom, not a dependency-only atom |
| G3 graphics | partial triangle + clear, textures, depth/stencil/blend/MSAA offscreen, pixel-exact |
| G4 present | ≥300 presents via the claimed route (Android surface **and** each claimed wrapper), resize/recreate, cold launch |
| G5 semantics | focused CTS subsets for every exposed feature; zero hidden-feature positives |
| G6 consumer | Wine host + DXVK tiers + vkd3d tiers actually rendering (screenshot/hash + FPS log) |
| G7 release | M4/M5 thresholds (bundle §11) |

Rules:

- **Missing hardware:** a tuple with no hardware stays `NOT_RUN`. Community reports (reddit, issues) can open an `IN_DEVELOPMENT` entry and set priority. They never count as PASS.
- **Community test kit:** before accepting outside testers, ship a probe APK or script that outputs the G0 record (GPU ID, UAPI, vendor kbase, node access). Their reports then map onto the matrix without guessing.

## 7. FristOneRR evaluation (adopt / avoid)

Sources:

- https://github.com/FristOneRR/FristOneRR-Panvk-Driver: release repo, MIT © 2026 FristOneRR.
- https://github.com/FristOneRR-Admin/FristOneRR-Panvk-Source: single squashed commit `2a2e5f0` (2026-09-29). It is a full Mesa 26.3.0-devel fork based on `funnymdzz/mesa@6598829019c0746aa8e473b4ae1c980cbfa6ea4b`, built with `-Dplatforms=android -Dpanfrost-kmds=kbase -Dplatform-sdk-version=35`. Mesa per-file licenses apply: `kbase_kmod.c` is `SPDX MIT`.

Provenance caveat, from the author's own README:

- Parts of the tree came from `mexicanbr0auth/mesa-panvk-g57`, `Noysz/panvk-g99-jm`, and `0x8055/panvk-g52-oppo-a38` without tracked history.
- `tools/fristonerr/code_origin.py` measures overlap.

Rules for reuse:

- Take only hunk-level ideas or code with per-hunk attribution in the patch header (`Source/Reference:` url + commit + original project if matched).
- Re-check the licenses of those three upstream repos before copying their lines.
- Prefer re-deriving from Arm kbase UAPI headers and Mesa upstream.

### Adopt (candidates for Waves B/C, each through our gates)

| Item | Where | Why |
|---|---|---|
| JM atom-stride autodetect (48/56/64) | `src/panfrost/lib/kmod/kbase_kmod.c` `kbase_probe_atom_stride()` / `kbase_atom_stride()`, `PANVK_ATOM_STRIDE` override | Real vendor variance: r49 on 6.6 kernels uses 56 bytes (Oppo A38 G52). Prefer deriving the stride from UAPI version first; use the dependency-only-atom probe as a documented fallback. |
| Dual-flavour VERSION_CHECK (CSF nr 52 then JM nr 0), JM UAPI ≥11.0 check | same file, ~L1270–1290 | Matches §4.2 |
| Versioned CSF `CS_QUEUE_GROUP_CREATE` (current → 1_18 → 1_6 fallback) with size asserts | same file ~L615–680 | Needed for the CSF UAPI spread in Wave D |
| `VK_EXT_external_memory_host` via `KBASE_IOCTL_MEM_IMPORT` (`kbase_kmod_bo_import_host`) | same file ~L1593; `panvk_vX_physical_device.c` | Wine WoW64 / FEX host-memory import. Relevant to the Wine milestone on every tuple. Needs our own import-rollback and lifetime tests. |
| Heap sizing: tiler heap env `PANVK_TILER_HEAP_MB` (default 512), libpoly heap default reduced 128→16 MB (`PANVK_POLY_HEAP_MB`) | `src/panfrost/vulkan/panvk_vX_device.c` ~L600–660 | Saves about 112 MB per VkDevice on 4–6 GB phones. Adopt as measured, per-profile defaults, not a blind 16 MB, because GS/tess from M1 use libpoly. |
| Model-table names: G57 variants, G68 (9.0.4/9.2.4), G77, G78, G78AE, G52 r1 (`0x7402…`) | `src/panfrost/model/pan_model.c` | Identity only. Upstream-first where possible. |
| Kbase tiler-heap renewal notes (renew every 8 graphics submits; kernel kills the group on `frag_end > vt_end` or all-zero counters) | inherited `CLAUDE.md` + `docs/kbase-minecraft-fault-analysis.md` (funnymdzz base) | Cross-check against our CSF heap handling. This is a known fault class. |
| Field compatibility table + report template (device/SoC/GPU/kbase, host, wrapper, DXVK, box64/FEX, logs) | driver README | Seeds the §2 matrix priorities and the §6 community kit |
| Provenance script idea (`tools/fristonerr/code_origin.py`) | source repo | Useful for our own donor-import audits |

### Avoid

- **Silent job drop:** `kbase_atom_alloc()` prints `[ATOMTABLE] … job dropped` and returns NULL after 10000 tries. Must become `VK_ERROR_DEVICE_LOST` or back-pressure, never a dropped job.
- **CPU-synchronous model:** submissions are polled on seqno and semaphores are resolved on the CPU (`kbase_cpu_sync_type`). This is allowed bookkeeping, but it is not the async/KCPU/sync-fd design we need for Wine and present pacing. Our transport keeps real fences.
- **Tracing in common Vulkan code:** `PANVK_TRACE` `fprintf` gates were patched into common `wsi_common*.c` and `vk_sync.c`. Use `mesa_log`/`PANVK_DEBUG` instead.
- **Wholesale tree import:** no history, and a different Mesa base. Cherry-pick ideas only.
- **Feature exposure:** follow their feature exposure only as a status marker. Their v9 path targets DXVK 1.10.3 because GS/tess/XFB are missing. We close that with GPU-lowered stages, never by advertising. LukeValen's G52 alpha explicitly advertises unimplemented features, which we must not copy.
- **Samsung/Exynos:** a ported "fix" from them is not evidence for Exynos. Probe Exynos kbase ourselves.

## 8. Phases (maps to bundle U0–U19)

| Phase | Bundle | Delta |
|---|---|---|
| U0 | U0 | Gate adds Wine (§0) |
| U1 | U1 | Confirmed defects (§1 row 9) |
| U2 | U2 | Add vendor-kbase fields, atom stride, MEM_IMPORT support to the probe; ship the community probe kit |
| U3 | U3 | Add the Winlator-family host adapter on G615 (reuse Wine milestone harness) |
| U4 | U4 | §4.1 patch migration + union build first |
| U5–U9 | U5–U9 | G720: fetch `f1d7bed` (not local yet) |
| U10' | U13 (moved up) | **v9 JM (G57 first; G68/G77/G78 next)**: JM transport + atom stride + v9 draw path; port M1 GPU-lowered GS/tess/XFB/BC so DXVK 2.x/3.x D3D10/11 work; Wine hosts + wrappers |
| U11–U12 | U11–U12 | Bifrost v7 JM (G52 → G76), reusing U10' transport |
| U13' | U10 | Other CSF / vendor kernels (G610 RK3588, G710/G715 Tensor, other MTK G615/G720/G925 boards) |
| U14 | U14 | Bifrost v6, Midgard, Utgard disposition; arm32 / low-API packages |
| U15–U19 | U15–U19 | Unchanged, plus the §5.2 wrapper matrix |

Checkpoint file: `worklogs/universal-mali/NEXT.md`. Evidence goes in `validation/universal/<profile>/` and `validation/universal/support-matrix.json`.

## 9. Risks

| Risk | Mitigation |
|---|---|
| Patch migration (§4.1) regresses G615 | byte-identical `src/` + ICD hash + G615 smoke before any new-GPU change |
| Vendor kbase divergence (Exynos, Tensor UMM, r49 atom stride) | vendor axis in profiles; typed probe failures; per-vendor quirks, never global |
| Low RAM (4–6 GB) devices OOM with M1 heaps | per-profile measured heap defaults; memory gate in G7 |
| No hardware for many tuples | `NOT_RUN`; community probe kit; never promote on donor/community claims |
| Donor provenance/licensing (FristOneRR mixed origin) | hunk-level import with attribution; re-derive from UAPI headers; check upstream repo licenses |
| Wrapper/host churn (Winlator forks, GameHub) | pin host+wrapper versions per record; test the popular 2–3 routes |
| Pressure to spoof features for DXVK 2.x on JM | contract forbids it; GPU-lowered ports are the only path |
| Old Android API on budget JM devices | real lower-API build + device test before changing `minApi` |
| `ALL_MALI_OBJECTIVE` quietly shrunk | scoped releases need owner approval; gaps stay listed |

## 10. Community signal (r/EmulationMediatekMali, RSS read 2026-09-29)

- **Activity:** very active the week of 2026-09-27/29.
  - FristOneRR "Panvk-Mali-G57 beta 1.0.0/1.1.0" release threads.
  - LukeValen "PanVK Mali-G52 MC2 v0.0.1-alpha": Redmi 13C, Android 14, kernel 4.19, kbase UAPI 11.38, Winlator/Ludashi VKCube + D3D9–11. The fixes it lists are the `DRM_FORMAT_MOD_INVALID` swapchain and sync-fd import/export.
  - A G720 (Poco X7 Pro, D8400) Winlator game-compatibility list; users there are currently on the vendor driver.
- **Failure patterns:** Samsung/Exynos kbase incompatibility; G76 (G95) freezes; high RAM use; wrapper-dependent crashes; artifacts in some D3D games; D3D12 draws nothing.
- **Demand:** G57/G99 by far the largest, then G720/G615-class D8xxx, then G52 budget phones. Hosts: Winlator Mali, Ludashi, Bannerlator/BannerHub, GameHub.
- **Access note:** reddit JSON endpoints returned 403; one RSS feed read; later requests were rate-limited (429). This is a sample, not a census.

## Sources

- The bundle's R/D/J/E sources are listed in `docs/plans/universal-mali-bundle/PANVK_UNIVERSAL_MALI_GPU_ONLY_WORKER.md`.
- F1: https://github.com/FristOneRR/FristOneRR-Panvk-Driver (README incl. Wrappers, 2026-09-29)
- F2: https://github.com/FristOneRR-Admin/FristOneRR-Panvk-Source @ `2a2e5f0ade073732eb65e30715cb19288cf38d34`
- F3: https://www.reddit.com/r/EmulationMediatekMali/ (RSS, 2026-09-29)
