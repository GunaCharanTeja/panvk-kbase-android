# Mali GPU architectures and PanVK support

This document explains which Arm Mali GPUs exist, how Arm's marketing
generation names relate to Mesa's `PAN_ARCH` numbers, and which of them this
repository supports. Research date: 2026-10-03.

Short version: this repository supports and validates exactly one GPU, the
**Mali-G615 MC6** (Mesa `PAN_ARCH` **v11**, CSF frontend, Arm "4th gen Valhall").
Everything else in the tables below is background information, not a support
claim.

## Confidence markers

Every fact in the tables carries a marker:

- **[H]** verified in first-party material: Mesa source (`pan_model.c`,
  `pan_model.h`, `src/panfrost/vulkan/`), Mesa docs, Arm pages/newsroom, or a
  Linux kernel patch.
- **[M]** from Wikipedia or reputable press (Phoronix, Android Authority,
  Collabora). One source, plausible, not cross-checked against Arm.
- **[L]** community or inferred, a single weak source, or not found in any
  source. Treat as a lead only.

Where sources disagree this is said explicitly in
[Where sources disagree](#where-sources-disagree).

## 1. What this repository supports

| Item | Value |
|---|---|
| Supported GPU | Mali-G615 (Poco X6 Pro, MediaTek Dimensity 8300-Ultra, MT6897) |
| Configuration seen | Mali-G615 MC6, GPU ID `0xb8a31030`, r1p3 |
| Mesa `PAN_ARCH` | **11** (`PAN_PROD_ID(11, 8, 3)`, model `G615`) [H] |
| Arm generation | 4th generation Valhall (Arm TCS22, June 2022) [H] |
| Frontend | CSF (Command Stream Frontend) |
| Kernel interface | Android vendor `mali_kbase` via `/dev/mali0`, CSF uAPI 1.21 |
| Mesa base | `26.3.0-devel`, commit `5a07217f` (see `sources.lock`) |
| Profile | `profiles/g615-v11-csf.json` (status: primary bring-up target) |
| Vulkan | API 1.4.363, 194 extensions, not Khronos conformant |

What is proven on the device (details in `README.md`, `docs/RUNTIME-FEATURES.md`
and `validation/g615-v11-csf/`):

- Android loader and WSI, glibc ICD path, and the 13 Phase 5 feature workloads.
- DXVK Native creating a D3D11 device at feature level 11_0 and running D3D11
  and D3D9 draw workloads; many Vulkan CTS groups with zero failures.
- Not working: vkd3d-proton / D3D12 (missing `robustImageAccess2`, no sparse on
  Kbase), and there is no BC texture hardware on G615 (BC is decoded in
  compute).

### Other GPUs: not supported, not validated

The repository contains scaffolding for other architectures, but none of it has
been validated on hardware:

| Profile / patch family | Status |
|---|---|
| `profiles/g610-v10-csf.json`, `patches/csf-v10/` | planned, not validated |
| `profiles/g720-v12-csf.json`, `patches/csf-v12/` | planned, not validated |
| `patches/csf-v13/`, `patches/csf-v14/` | arch-scoped quirks only, no profile, no device |
| `profiles/g52-v7-jm.json`, `patches/jm-v7/` | planned (JM, Bifrost), not validated |
| `profiles/g57-v9-jm.json`, `patches/jm-v9/` | planned (JM, Valhall v9); upstream PanVK has no v9 backend, so this is a real port |

Why another GPU needs its own profile and validation, not just a different
device:

1. **The Mesa model table must know the GPU.** PanVK refuses to start on a
   `gpu_id`/variant that is not in `pan_model_list` ("Unknown gpu_id").
2. **Arch gates decide features.** Many Vulkan features and limits are
   `PAN_ARCH >= N` conditionals (descriptor indexing at v9; the
   `VK_EXT_robustness2` extension, `nullDescriptor`, draw-indirect-count and
   conditional rendering at v10; `robustBufferAccess2`, conservative
   rasterization and cooperative matrix at v11; 64 KiB color tilebuffers at
   v12). A different arch exposes a
   different feature set that has never been tested here.
3. **Frontend differs.** v10 and newer use CSF (queues, groups, tiler heap
   chunks); v9 and older use the Job Manager (job chains). The kbase submission
   path, sync (KCPU queues), and tiler handling are completely different code.
4. **Vendor kernels differ.** Each phone ships a vendor `mali_kbase` with its
   own uAPI version, core mask, SELinux label and quirks. The profile key is
   `pan_arch + gpu_id + frontend + kbase_uapi`, never the Android release.
5. **Upstream gating.** Upstream PanVK only treats v10 (G610 class) as
   conformant, and still hides v6, v7, v11 and v14 behind
   `PAN_I_WANT_A_BROKEN_VULKAN_DRIVER=1` (v11 on the DRM path only; this
   repository's Kbase path lets v10 to v13 through without the variable).

Adding a GPU therefore means: model-table entry, profile, arch patch family,
kbase probe on the real phone, then the vulkan-smoke, compute, WSI and CTS
validation ladder. See `docs/PORTABILITY.md` and `docs/KBASE-PROFILES.md`.

## 2. Naming: Arm generations versus Mesa `PAN_ARCH`

Arm marketing generation names and Mesa architecture numbers are two different
numbering schemes:

- **Arm names** (marketing): Utgard, Midgard (1st to 4th gen), Bifrost (1st to
  3rd gen), Valhall (1st to 4th gen), then "5th Gen Arm GPU Architecture" (Arm
  stopped using a code name and a "Valhall" label for it), and now G2 NX.
- **Mesa `PAN_ARCH`**: the architecture major number found in the top bits of
  the hardware `GPU_ID`. For Bifrost and newer, `pan_arch()` simply returns that
  major. For older Midgard it uses a lookup table (v4 and v5). Mesa labels its
  model macros `MIDGARD_MODEL`, `BIFROST_MODEL`, `VALHALL_MODEL`,
  `FIFTHGEN_MODEL`; the Mesa docs table names the groups "Valhall (v9, v10,
  v11)" and "5th Gen (v12, v13, v14)". [H]
- The two do not line up one to one. For example Arm's "Bifrost 1st gen" holds
  G31, G51 and G71, but Mesa puts G71 in v6 and G31/G51 in v7. One Mesa arch
  number usually covers several Arm products (v11 = G615 and G715 and
  Immortalis-G715; v14 = G1-Pro, G1-Premium and G1-Ultra).
- **Immortalis** is a brand tier, not a different architecture: it marks the
  flagship configuration with 10 or more shader cores and hardware ray tracing
  (Immortalis-G715 onward). It shares the arch number of its Mali siblings, and
  the Linux kernel tells them apart only by core count and a ray-tracing
  capability bit. The G1 generation (2025) dropped the Immortalis name; the
  flagship is simply "Mali G1-Ultra".
- **Frontend**: JM (Job Manager, job chains) up to and including v9; CSF
  (Command Stream Frontend, firmware-driven queues) from v10, i.e. G310, G510,
  G610 and G710 onward. In Mesa `src/panfrost/vulkan/meson.build`:
  `jm_archs = [6, 7]`, `csf_archs = [10, 11, 12, 13, 14]`; v9 has no PanVK
  per-arch library. [H]
- **User statement "G615 is v11, Valhall gen 4"**: confirmed correct. Mesa
  `pan_model.c` has `VALHALL_MODEL(PAN_PROD_ID(11, 8, 3), ... "G615" ...)` and the
  Mesa docs list "G615, G715: Valhall (v11)". Arm's own announcement and press
  coverage call G615/G715 the "4th generation of Valhall". 5th Gen starts one
  step later, at v12 (G620/G720). [H]

## 3. Complete chronological list

Years are the Arm announcement or launch quarter as given by Wikipedia unless
noted. "Frontend" is JM or CSF. "Mesa" gives the open-source driver and its
status in current Mesa main (and in the pinned `26.3.0-devel`). Example SoCs are
representative, mostly from Wikipedia's implementation list.

Open-source driver summary used below:

- **Lima**: Gallium GLES 2.0 driver for Utgard (Mali-400, Mali-450).
- **Panfrost**: Gallium OpenGL ES 3.1 / OpenGL 3.1 driver for Midgard v4/v5,
  Bifrost v6/v7, Valhall v9 to v11 and 5th Gen v12 to v14. Kernel side: Panfrost
  DRM driver for JM GPUs, Panthor DRM driver for CSF GPUs (v10+), Tyr (Rust)
  upcoming.
- **PanVK**: Vulkan driver in Mesa. Per-arch backends for v6, v7, v10, v11,
  v12, v13, v14. No v9 backend. Only v10 reports Vulkan conformance.

### 3.1 Pre-Utgard and Utgard (2005 to 2015), no Mesa arch number

| Family / Arm gen | GPUs | Announced | PAN_ARCH | Frontend | Mesa support | Example SoCs |
|---|---|---|---|---|---|---|
| Fixed-function, pre-Utgard [M] | Mali-55, Mali-110 | 2005 [M] | n/a | n/a | none | rare early handset SoCs [L] |
| Utgard [M] | Mali-200 | 2007 [M] | n/a | fixed (pre-JM) | no driver upstream (Lima list has only 400/450/470) [H] | Rockchip RK2818, Telechips TCC8900, NetLogic Au1380 [M] |
| Utgard [M] | Mali-400 MP (1 to 4 cores) | 2008 [M] | n/a | n/a | **Lima**, supported [H] | Allwinner A10/A20/A64, Exynos 4210/4412, Rockchip RK3066/RK3188, MediaTek MT6582 [M] |
| Utgard [M] | Mali-300 | 2010 [M] | n/a | n/a | no upstream driver [M] | ELVEES 1892VM14Ya [M] |
| Utgard [M] | Mali-450 MP (1 to 8 cores) | 2012 [M] | n/a | n/a | **Lima**, supported [H] | Amlogic S905/S905X, Allwinner H5, Kirin 620/910, Rockchip RK3328, MediaTek MT6592 [M] |
| Utgard [M] | Mali-470 MP | 2015 [M] | n/a | n/a | Lima lists it as **unsupported** [H] | Realtek RTD1395, MediaTek MSD6683 [M] |

### 3.2 Midgard (2010 to 2016), Mesa v4 and v5, Job Manager

Midgard introduced the unified shader model and a SIMD ISA. Wikipedia numbers it
1st to 4th gen; Mesa folds it into v4 and v5.

| Arm gen | GPUs | Announced | PAN_ARCH | Frontend | Mesa support | Example SoCs |
|---|---|---|---|---|---|---|
| Midgard 1st gen [M] | Mali-T604 | Nov 2010 [M] | **v4** (Mesa model `T600`, id 0x600) [H] | JM | Panfrost GLES 2.0, no Vulkan [H] | Exynos 5250 (T604 MP4) [M] |
| Midgard 1st gen [M] | Mali-T658 | Nov 2011 [M] | not listed in Mesa model table [H]; v4 expected [L] | JM | not listed [H] | none found [L] |
| Midgard 2nd gen [M] | Mali-T622, T624, T628, T678 | 2012 to 2013 [M] | **v4** via model `T620` (id 0x620) covers the T62x family [M]; T678 not listed [H] | JM | Panfrost GLES 2.0 (T62x), no Vulkan [H] | Exynos 5260 (T624), Exynos 5420/5430 (T628), Kirin 920/930 (T628), Baikal-M (T628 MP8) [M] |
| Midgard 3rd gen [M] | Mali-T720 | Oct 2013 [M] | **v4** (model `T720`, id 0x720) [H] | JM | Panfrost GLES 2.0, no Vulkan [H] | MediaTek MT6735/MT6753, Exynos 7570/7580, Allwinner H6, Kirin (Hi3798) [M] |
| Midgard 3rd gen [M] | Mali-T760 | Oct 2013 to 2014 [M] | **v5** (id 0x750) [H] | JM | Panfrost GLES 3.1 [H] | Exynos 7420, Rockchip RK3288, MediaTek MT6752 [M] |
| Midgard 4th gen [M] | Mali-T820, T830 | Q4 2015 [M] | **v5** (ids 0x820, 0x830) [H] | JM | Panfrost GLES 3.1 [H] | Exynos 7870/7880, Amlogic S912/T966, Realtek RTD1295, Kirin 650 [M] |
| Midgard 4th gen [M] | Mali-T860, T880 | Q4 2015 / Q2 2016 [M] | **v5** (ids 0x860, 0x880) [H] | JM | Panfrost GLES 3.1 [H] | Rockchip RK3399, Helio P10/P20/X20, Kirin 950/955, Exynos 8890 [M] |

### 3.3 Bifrost (2016 to 2018), Mesa v6 and v7, Job Manager

Bifrost adds a quad-based execution engine and unified memory. Mesa support for
v6 and v7 exists in Panfrost (GLES 3.1) and as **experimental** PanVK (Vulkan
1.3, hidden behind `PAN_I_WANT_A_BROKEN_VULKAN_DRIVER=1`). [H]

| Arm gen | GPUs | Announced | PAN_ARCH | Frontend | Mesa support | Example SoCs |
|---|---|---|---|---|---|---|
| Bifrost 1st gen [M] | Mali-G71 | Q2 2016 [M] | **v6** (`PAN_PROD_ID(6, 0, 0)`) [H] | JM | Panfrost model entry exists, but the Mesa docs say G71 is "not yet supported" (see disagreements) [H] | Kirin 960, Exynos 8895, Helio P30 [M] |
| Bifrost 2nd gen [M] | Mali-G72 | Q2 2017 [M] | **v6** (`PAN_PROD_ID(6, 2, 1)`) [H] | JM | Panfrost GLES 3.1; PanVK 1.3 experimental [H] | Kirin 970, Exynos 9810, Helio P60/P70 [M] |
| Bifrost 1st gen [M] | Mali-G51 | Q4 2016 [M] | **v7** (`PAN_PROD_ID(7, 0, 0)`) [H] | JM | Panfrost GLES 3.1; PanVK 1.3 experimental [H] | Kirin 710 [M] |
| Bifrost 1st gen [M] | Mali-G31 | Q1 2018 per Wikipedia (see disagreements) [L] | **v7** (`PAN_PROD_ID(7, 0, 3)`) [H] | JM | Panfrost GLES 3.1; PanVK 1.3 experimental [H] | Amlogic S905X2/X3, Allwinner H616/H618, Rockchip RK3326 [M] |
| Bifrost 2nd gen [M] | Mali-G52 | Q1 2018 [M] | **v7** (`PAN_PROD_ID(7, 2, 2)`, plus `G52 r1` 7.4.2) [H] | JM | Panfrost **conformant GLES** (Khronos); PanVK 1.3 experimental [H] | Kirin 810, Helio G80/G85, Rockchip RK3566/RK3568/RK3576, Amlogic S922X/A311D [M] |
| Bifrost 3rd gen [M] | Mali-G76 | Q2 2018 [M] | **v7** (`PAN_PROD_ID(7, 2, 1)`) [H] | JM | Panfrost GLES 3.1; PanVK 1.3 experimental [H] | Kirin 980/990, Exynos 9820, Helio G90, ARM Morello [M] |

### 3.4 Valhall 1st to 4th gen (2019 to 2022), Mesa v9, v10, v11

Valhall is a scalar, superscalar-engine redesign with a new ISA. v9 is still the
Job Manager; v10 introduced the Command Stream Frontend.

| Arm gen | GPUs | Announced | PAN_ARCH | Frontend | Mesa support | Example SoCs |
|---|---|---|---|---|---|---|
| Valhall 1st gen [M] | Mali-G57 | Q2 2019 (Wikipedia; Arm said late 2019) [M] | **v9** (`PAN_PROD_ID(9, 0, 1)` and `(9, 0, 3)`) [H] | JM | Panfrost **conformant GLES**; no PanVK v9 backend [H] | Kirin 820, Helio G95/G96/G99, Dimensity 700/800/820, Kompanio 820/1200 [M] |
| Valhall 1st gen [M] | Mali-G77 | May 2019 [M] | **v9** [H] (shares the G57 id family; counters named `G77`) | JM | Panfrost GLES; no separate model entry beyond the G57 ids [H] | Kirin 985, Exynos 990, Dimensity 1000/1100/1200 [M] |
| Valhall 2nd gen [M] | Mali-G68 | Q2 2020 [M] | **v9** (`PAN_PROD_ID(9, 2, 4)`) [H] | JM | Panfrost GLES 3.1; no PanVK v9 [H] | Exynos 1280/1330/1380, Dimensity 900/1080/7050 [M] |
| Valhall 2nd gen [M] | Mali-G78 | May 2020 [M] | **v9** [H] (counters named `G78`) | JM | Panfrost; no separate model entry [H] | Exynos 1080/2100, Google Tensor (G1), Kirin 9000 [M] |
| Valhall 2nd gen, automotive [M] | Mali-G78AE | Nov 2020 [M] | v9 [L] (not found in Mesa) | JM [L] | none [L] | automotive SoCs [L] |
| Valhall 3rd gen [M] | Mali-G310 | May 2021 [M] | **v10** (`PAN_PROD_ID(10, 12, 4)`, variants v1 to v5) [H] | CSF | Panfrost + PanVK (Vulkan 1.4, same arch as the conformant G610) [H]; kernel Panthor [M] | Amlogic S905X5, NXP i.MX95 [M] |
| Valhall 3rd gen [M] | Mali-G510 | May 2021 [M] | v10 [M] (sbcwiki, kernel Panthor list) | CSF | not in Mesa model table [H]; Panthor lists it [M] | none found [L] |
| Valhall 3rd gen [M] | Mali-G610 | May 2021 [M] | **v10** (`PAN_PROD_ID(10, 8, 7)`) [H] | CSF | Panfrost conformant GLES; **PanVK Vulkan 1.4 conformant** (the only conformant PanVK GPU) [H] | Rockchip RK3588, Dimensity 7200, Dimensity 8000/8100/8200 [M] |
| Valhall 3rd gen [M] | Mali-G710 | May 2021 [M] | v10 [H] (G610 entry uses `G710` counters) | CSF | not a separate Mesa entry; Panthor lists it [M] | Dimensity 9000/9000+, Google Tensor G2 [M] |
| Valhall 4th gen [H] | **Mali-G615** | Jun 2022 [H] | **v11** (`PAN_PROD_ID(11, 8, 3)`) [H] | CSF | Panfrost; PanVK experimental upstream (env var), Vulkan 1.4 on paper; **this repository's target** [H] | Dimensity 7300 (G615 MC2), **Dimensity 8300 / 8350 (G615 MC6)** [M] |
| Valhall 4th gen [H] | Mali-G715 | Jun 2022 [H] | **v11** (`PAN_PROD_ID(11, 8, 2)`) [H] | CSF | same as G615 [H] | Google Tensor G3 and G4 (7 cores) [M] |
| Valhall 4th gen [H] | Immortalis-G715 (10+ cores, ray tracing) | Jun 2022 [H] | v11 [M] (same arch major as G715; no separate Mesa entry) | CSF | as G715 [M] | Dimensity 9200 / 9200+ [M] |

### 3.5 5th Gen Arm GPU Architecture (2023 to 2025), Mesa v12, v13, v14

Arm calls this "5th Gen Arm GPU Architecture". It introduced deferred vertex
shading (DVS). Mesa uses the `FIFTHGEN_MODEL` macro for it. In the kernel, the
Panthor driver identifies these by `GPU_PROD_ID_MAKE(12, x)` (G720, G620),
`(13, x)` (G725, G625, G925) and recognises Immortalis by shader core count and
ray-tracing capability. [H]

| Arm gen | GPUs | Announced | PAN_ARCH | Frontend | Mesa support | Example SoCs |
|---|---|---|---|---|---|---|
| 5th Gen [H] | Mali-G720 (6 to 9 cores) | May 2023 [H] | **v12** (`PAN_PROD_ID(12, 8, 0)`) [H] | CSF | Panfrost + PanVK, no env var required upstream, not conformant [H] | Dimensity 8400 / 8450 (G720 MC7) [M] |
| 5th Gen [H] | Mali-G620 (1 to 5 cores) | May 2023 [H] | v12 [M] (Panthor `GPU_PROD_ID_MAKE(12, 1)`) | CSF | no Mesa model entry in pinned or main table [H] | none found [L] |
| 5th Gen [H] | Immortalis-G720 (10+ cores) | Nov 2023 [M] | v12 [M] | CSF | as G720 [M] | Dimensity 9300 (MP12) [M] |
| 5th Gen [H] | Mali-G725 (6 to 9 cores) | May 2024 [M] | **v13** (`PAN_PROD_ID(13, 8, 0)`) [H] | CSF | Panfrost + PanVK, no env var upstream, not conformant [H] | none found [L] |
| 5th Gen [H] | Mali-G625 (1 to 5 cores) | May 2024 [M] | v13 [M] (Panthor `GPU_PROD_ID_MAKE(13, 1)`) | CSF | no Mesa model entry [H] | none found [L] |
| 5th Gen [H] | Immortalis-G925 (10 to 24 cores) | May 2024 [M] | v13 [M] (Panthor `GPU_PROD_ID_MAKE(13, 0)` with 10+ cores and ray tracing) | CSF | not a separate Mesa entry; the G725 model matches the same product id [M] | Dimensity 9400 (MP12), Xiaomi XRING O1 (MP16) [M] |
| 5th Gen, G1 series [H] | Mali G1-Pro (1 to 5 cores) | Sep 2025 [H] | **v14** (`PAN_PROD_ID(14, 8, 3)`, variants 1 and 4) [H] | CSF | Panfrost + PanVK, **experimental** (env var); G1-Pro bring-up merged for Mesa 26.2 [H] | not found [L] |
| 5th Gen, G1 series [H] | Mali G1-Premium (6 to 9 cores) | Sep 2025 [H] | **v14** (`PAN_PROD_ID(14, 8, 1)`) [H] | CSF | as G1-Pro; model entry present in current main and pinned tree [H] | not found [L] |
| 5th Gen, G1 series [H] | Mali G1-Ultra (10 to 24 cores, no Immortalis name) | Sep 2025 [H] | **v14** (`PAN_PROD_ID(14, 8, 0)`) [H] | CSF | as G1-Pro; model entry present [H] | **Dimensity 9500 (MC12)** [M] |

The G1 series is described by Arm as a continuation of the 5th Gen DVS
architecture with core-level improvements (dual-stack shader cores, more
fast-access uniform registers, Image Region Dependencies, second-generation ray
tracing unit); it ships as part of the Arm Lumex CSS platform. [H]

### 3.6 Newest: G2 (announced 2026-09-08)

| Arm gen | GPUs | Announced | PAN_ARCH | Frontend | Mesa support | Example SoCs |
|---|---|---|---|---|---|---|
| Next architecture, "largest execution engine redesign in seven generations" per Arm [H]; "6th Gen / Magni" per community only [L] | Mali G2-Ultra NX (Arm name); G2-Premium-NX, G2-Pro-NX, "TMAX/TMEX" rumoured [L] | Sep 8, 2026 [H] | v15 [L] (community, from kernel/Android 17 Panthor SDK strings via sbcwiki) | CSF [L] | none: not in `pan_model_list` [H] | none shipping yet [H]; part of Arm CSS for Mobile 2 |

G2-Ultra NX adds dedicated neural accelerators inside the shader cores, a
redesigned execution engine and a third-generation ray tracing unit, and Arm
quotes up to 14 percent higher sustained gaming performance and up to 24
percent higher peak benchmark performance than G1-Ultra. [H]

## 4. Upstream driver support at a glance

| Arch | GPUs | Frontend | Panfrost GLES / GL | PanVK Vulkan upstream | Kernel driver |
|---|---|---|---|---|---|
| Utgard | Mali-400/450 | n/a | Lima, GLES 2.0 / GL 2.1 | none | Lima DRM |
| v4 | T600, T620, T720 | JM | 2.0 / 2.1 | none | Panfrost DRM |
| v5 | T760, T820, T830, T860, T880 | JM | 3.1 / 3.1 | none | Panfrost DRM |
| v6 | G71 (not supported), G72 | JM | 3.1 / 3.1 | 1.3, experimental | Panfrost DRM |
| v7 | G31, G51, G52, G76 | JM | 3.1 / 3.1 | 1.3, experimental | Panfrost DRM |
| v9 | G57, G68 (G77, G78 by id family) | JM | 3.1 / 3.1 | none (no backend) | Panfrost DRM |
| v10 | G310, G610 (G510, G710) | CSF | 3.1 / 3.1 | 1.4, **conformant on G610**; no env var | Panthor DRM |
| v11 | G615, G715 | CSF | 3.1 / 3.1 | 1.4, experimental (env var) | Panthor DRM |
| v12 | G720 | CSF | 3.1 / 3.1 | 1.4, not conformant | Panthor DRM |
| v13 | G725 | CSF | 3.1 / 3.1 | 1.4, not conformant | Panthor DRM |
| v14 | G1-Pro, G1-Premium, G1-Ultra | CSF | 3.1 / 3.1 | 1.4, experimental (env var) | Panthor DRM (patches posted Oct 2025) |
| v15 [L] | G2-Ultra NX (G2-Premium-NX, G2-Pro-NX rumoured) | CSF [L] | none | none (not in `pan_model_list`) | none yet |

Panfrost OpenGL ES is Khronos-conformant on Mali-G52, G57 and G610 only. PanVK is
conformant on G610 only (Vulkan 1.1, 1.2 and now 1.4 submissions, Collabora and
Khronos). In code, `panvk_physical_device.c` reports a non-conformant
implementation for every arch except v10. All upstream support above uses the
DRM kernel drivers (Panfrost, Panthor). Android phones ship the proprietary
`mali_kbase` kernel driver instead, which upstream Mesa has no backend for; the
`kbase` kmod backend is what this repository adds.

## 5. Where sources disagree

1. **Task statement versus Mesa for v6/v7/v10.** An earlier assumption listed
   "v6 = G31/G51/G71/G52/G72/G76" and "v7 = G57/G68/G77/G78" and put G615/G715
   at v10. Mesa source says otherwise: G71 and G72 are v6; G31, G51, G52 and G76
   are v7; G57, G68, G77 and G78 are v9; G310 and G610 (and G510, G710) are v10;
   G615 and G715 are v11. The statement "G615 is v11, Valhall gen 4" is correct.
2. **Arm generation numbers versus Mesa arch.** Wikipedia counts Bifrost as 1st
   gen (G31, G51, G71), 2nd gen (G52, G72), 3rd gen (G76), which does not match
   Mesa v6/v7 (G71 and G72 are v6; the others v7).
3. **Where 5th Gen belongs.** Wikipedia nests the 5th generation under the
   "Valhall" heading; Arm, Mesa (`FIFTHGEN_MODEL`, "5th Gen (v12)") and the
   Panthor kernel code treat it as a separate architecture that follows
   Valhall. This document follows Arm and Mesa.
4. **G71 in Mesa.** `pan_model.c` has a G71 entry, but the Mesa Panfrost docs
   say "Other Midgard and Bifrost chips (e.g. G71) are not yet supported".
5. **PanVK v11 status.** The Mesa docs table lists G615/G715 with Vulkan 1.4,
   while `panvk_physical_device.c` still requires
   `PAN_I_WANT_A_BROKEN_VULKAN_DRIVER=1` for v11 on the DRM path. The code is
   authoritative for what actually runs; the docs table says what exists.
6. **G2 naming and generation.** Arm's pages say "Mali G2-Ultra NX" and describe
   the execution engine as the largest redesign in seven generations, without
   (in the pages read) calling it "6th Gen". Wikipedia lists "G2-Ultra" with a
   Q3 2026 date; sbcwiki lists "6th Gen / Magni (v15)" and sibling parts from a
   game vendor's Vulkan allow-list and Panthor SDK strings. Treat v15 and the
   sibling names as unconfirmed.
7. **G31 and G57 announcement dates.** Wikipedia gives Q1 2018 (G31) and Q2 2019
   (G57); other Arm material places both earlier or later by a few months. Dates
   here are approximate quarters.
8. **G77/G78 in Mesa.** Mesa's table has the G57 (9.0.1, 9.0.3) and G68 (9.2.4)
   ids and uses `G77` and `G78` only as counter-set names; separate G77/G78
   ids are not in the table.

## 6. Sources

Mesa and kernel (first-party, [H]):

- Mesa Panfrost docs: <https://docs.mesa3d.org/drivers/panfrost.html>
- Mesa Lima docs: <https://docs.mesa3d.org/drivers/lima.html>
- `pan_model.c`: <https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/panfrost/model/pan_model.c>
- `pan_model.h` (`pan_arch()`): <https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/panfrost/model/pan_model.h>
- PanVK per-arch lists: <https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/panfrost/vulkan/meson.build>
- PanVK gating and conformance: <https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/panfrost/vulkan/panvk_physical_device.c> and `panvk_vX_physical_device.c`
- Panthor G720/G725 patch (kernel product ids): <https://lists.freedesktop.org/archives/dri-devel/2025-June/508735.html>
- This repo's pinned tree: `work/mesa-cube/src/panfrost/model/pan_model.c` and `panvk_physical_device.c` (Mesa `5a07217f`, 26.3.0-devel)

Arm:

- Mali G1-Ultra announcement (Sep 10, 2025): <https://newsroom.arm.com/blog/arm-mali-g1-ultra-gpu-gaming-ai>
- Mali G1-Premium product page: <https://www.arm.com/products/silicon-ip-multimedia/gpu/mali-g1-premium>
- Mali G2-Ultra NX (Sep 8, 2026): <https://newsroom.arm.com/blog/arm-mali-g2-ultra-nx-ai-native-mobile-graphics> and <https://www.arm.com/products/silicon-ip-multimedia/gpu/mali-g2-ultra-nx>

Secondary ([M]/[L]):

- Wikipedia, Mali (processor): <https://en.wikipedia.org/wiki/Mali_(processor)>
- Android Authority, Immortalis-G715 / 4th-gen Valhall: <https://www.androidauthority.com/arm-mali-g715-immortalis-3179061/>
- Android Authority, G710/G610/G510/G310: <https://www.androidauthority.com/arm-mali-g710-g610-g510-g310-1225934/>
- Phoronix, Lumex and G1-Ultra: <https://www.phoronix.com/news/Arm-Lumex-Platform-C1>
- Phoronix, G1 Panthor patches: <https://www.phoronix.com/news/Arm-Open-Source-Mali-G1>
- Phoronix, Mesa 25.1 5th Gen: <https://www.phoronix.com/news/Mesa-25.1-Newer-Mali-5th-Gen>
- Phoronix, G1-Pro in Mesa 26.2: <https://www.phoronix.com/news/Arm-Mali-G1-Pro-Mesa-26.2>
- Collabora, PanVK Vulkan 1.4: <https://www.collabora.com/news-and-blog/news-and-events/panvk-now-supports-vulkan-1.4.html>
- Collabora, PanVK v10 support: <https://www.collabora.com/news-and-blog/news-and-events/panvk-v10-support.html>
- Khronos conformant products: <https://www.khronos.org/conformance/adopters/conformant-products/vulkan>
- SBC wiki, Mali generations (community, source of v15 / "Magni" claims): <https://sbcwiki.com/docs/soc-manufacturers/arm/mali-gpu/>
