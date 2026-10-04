# NOTICE

This repository mixes several license regimes. Do not treat the tree as
single-license MIT. Do not relicense upstream code.

## 1. MIT repository scaffolding

Build/patch/test/packaging authored for `panvk-kbase-android` is MIT-licensed
unless a file header states otherwise. See `LICENSES/MIT.txt`.

This covers scripts, profiles, compatibility data, CI workflows, tests, and
docs written in this repository.

## 2. Mesa upstream (not vendored as a git subtree)

All Mesa source fetched by `scripts/fetch-mesa.sh` remains under Mesa's
upstream licenses (predominantly MIT/X11 with Khronos SGI/X11 portions — see
the Mesa checkout's `LICENSES/` after fetch).

This repository does not vendor the Mesa tree. `sources.lock` pins the exact
upstream commit; release manifests (`SOURCE.json`) record the Mesa SHA, patch
series hash, compiler, NDK, and build host for every binary.

Patches under `patches/**/*.patch` are derived modifications of Mesa and keep
Mesa's upstream license. They are not a grant of a new license.

## 3. Linux / Arm kbase UAPI headers — VENDORED

The following committed files are Arm Mali kbase kernel UAPI / register
headers, SPDX `GPL-2.0 WITH Linux-syscall-note`:

- `patches/kbase-common/files/include/drm-uapi/mali_kbase_ioctl.h`
- `patches/kbase-common/files/src/panfrost/lib/kmod/kbase_uapi.h`
- `patches/kbase-common/files/src/panfrost/lib/kmod/kbase_csf_uapi.h`
- `patches/kbase-common/files/src/panfrost/lib/kmod/mali_kbase_csf_registers.h`
- `patches/kbase-common/files/src/panfrost/lib/kmod/mali_base_kernel.h`

These **are vendored** into this tree (copied into the Mesa build via
`patches/*/files/`). They are **not** "studied, not vendored".

License texts:

- `LICENSES/GPL-2.0.txt`
- `LICENSES/Linux-syscall-note.txt`

The Linux-syscall-note exception is the kernel UAPI exception: including these
headers from userspace via normal ioctl/syscall use does not make the
userspace program a derived work of the kernel. The headers themselves remain
GPL-2.0 WITH Linux-syscall-note. This project does not relicense them.

Copyright on those headers remains with ARM Limited (and other upstream
authors named in the file headers).

## 4. Userspace kmod backend (this project's / Mesa-style MIT)

- `patches/kbase-common/files/src/panfrost/lib/kmod/kbase_kmod.c`
- `patches/kbase-common/files/src/panfrost/lib/kmod/kbase_kmod.h`

File SPDX is MIT. These implement a pan_kmod backend against the UAPI above.
They are not kernel code and are not the UAPI headers.

## 5. Reference Kbase work studied (not copied as a project)

The following repositories were studied as implementation references. Their
trees are **not** vendored here. Individual patches that learned from them
must still carry provenance in the patch header.

- https://github.com/wonderkast02/panvk-g720-kbase-csf (CSF primary)
- https://github.com/nangitagamer777-art/PanVK-kbase (history)
- https://github.com/nangitagamer777-art/Panvk_Kmod (secondary)
- https://github.com/leegao/mesa-funnymdzz (secondary)
- https://github.com/LukeValen/panvk-mali-g52 (JM/Bifrost primary)
- https://github.com/Noysz/panvk-g99-jm (JM/v9 primary)
- https://github.com/FristOneRR-Admin/FristOneRR-Panvk-Source (FristOneRR-Panvk-Source, commit efd07ba; Mesa-derived, MIT per-file headers kept; used for the JM kbase submission and v9 JM backend in patches/jm-v9)

Each patch under `patches/` must carry `purpose / source-reference /
tested-GPU / tested-Kbase-UAPI / Mesa-base-range / dependencies /
validation-test` in its header. Never ship a binary without exact source
provenance (`SOURCE.json` + `VALIDATION.json` + `SHA256SUMS.txt`).

## 6. BCn compatibility reference (not vendored)

`https://github.com/leegao/bcn_layer` at commit
`50993a2d51772567de9c36de4d523652773f0899` was inspected and built as the
optional BC1-BC7 Vulkan-layer reference. It is MIT-licensed, copyright (c)
2025 Pipetto-crypto. The exact notice is preserved in
`LICENSES/bcn_layer-MIT.txt`.

No `bcn_layer` source or binary is vendored or packaged by this repository.
If a later change copies or derives source, that file must retain this notice
and identify the pinned commit.

## 7. Winlator X server — VENDORED (LGPL-2.1)

The launcher's built-in X server is copied and modified from
`https://github.com/brunodev85/winlator` at commit
`db6d7aa446b5607b48ac2ee36dc4333799ff3dc7` (brunodev85 and Winlator
contributors). It is licensed **LGPL-2.1**; the license text is
`apps/panvk-launcher/third_party/winlator/LICENSE`.

Vendored locations:

- `apps/panvk-launcher/app/src/main/java/com/winlator/`
- `apps/panvk-launcher/app/src/main/cpp/winlator/`
- `apps/panvk-launcher/third_party/winlator/` (license and README with the file
  list and modifications; changes are marked `panvk-launcher` in the sources)

These files stay LGPL-2.1 and are not relicensed. Full source is in this
repository so recipients can modify the code and rebuild the launcher APK.

## 8. FristOneRR-Panvk-Source (Mesa-derived, MIT)

`https://github.com/FristOneRR-Admin/FristOneRR-Panvk-Source` at commit `efd07ba`:
Mesa-derived, MIT per-file headers kept. Used for the JM kbase submission and
v9 JM backend in `patches/jm-v9`.
