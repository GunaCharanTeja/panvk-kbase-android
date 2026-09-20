# DX2 CTS and DXVK Native setup

## Scope

DX2 only. No CTS feature suite, Vulkan workload, DXVK initialization, rendering,
presentation, or driver-feature test was run. Those remain DX3 or later work.

## CTS reconciliation

The existing DX2 CTS tooling and evidence were reused rather than duplicated.
On-device source commits, ELF identity, size, binary SHA-256, case count, and
case-list SHA-256 were reverified against `cts-lock.json`; every value matched.
CTS remains `NOT_RUN`, not PASS or FAIL.

## Stock DXVK Native

Official `doitsujin/dxvk` tag `v3.1.1` was cloned recursively and built unchanged
inside the Poco X6 Pro AArch64 Alpine glibc chroot. SDL2 is the sole enabled WSI
backend. Meson configured a native `aarch64` build; Ninja was limited to two jobs.
Recursive commits are recorded in `source-build-lock.json`.

The first foreground build command's captured output ended before Ninja's final
13 steps. A single bounded incremental `ninja -j2` invocation completed those
steps successfully. This was a build-command/output interruption, not a compiler,
runtime, CTS, Vulkan, PanVK, or missing-feature failure.

## Harness boundary

`tests/dxvk/native/common/dxvk_native_probe.cpp` prepares D3D9 and ordered D3D11
feature-level requests. DX2 does not run it. Same-ICD offscreen, WSI, PanVK
selection, GPU execution evidence, expected baseline rejection, and rendering
remain explicitly `NOT_RUN` for DX3.
