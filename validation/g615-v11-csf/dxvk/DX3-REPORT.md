# DX3 glibc runtime and trace setup

## Scope and target

DX3 only. No DX4 feature work, CTS feature suite, driver feature exposure, or
graphics implementation was performed. Every device command targeted serial
`Y5WWBMJVOZSK4HU8`; the single identity check returned `duchamp`, model
`2311DRK48I`, and `arm64-v8a`.

Device interaction comprised nine strictly sequential ADB invocations: seven
bounded `shell` scripts and two small source-file `push` operations. There were
no concurrent calls, retries, server operations, USB resets, sysfs writes,
reboots, root restarts, polling loops, or large transfers. No ADB transport
error, disconnect, or kernel USB symptom occurred.

## Candidate ICD identity

The existing device build was reused; PanVK was not rebuilt.

```text
path: /tmp/build-glibc/src/panfrost/vulkan/libvulkan_panfrost.so
SHA-256: 736f868bf720c95edd9b8bbac4a0a0b650fed67c2a9e46635dc3ee379fb0d2ca
Build ID: 1f8b1362ae206dc1f31dc57f0e1c7da2c7c6206b
ELF: 64-bit AArch64 glibc
Mesa: 26.3.0-devel (git-5a07217f03)
```

This is not the immutable beta.3 glibc ICD (`95a019b...`). The candidate's
build-tree manifest was absent, so DX3 generated a temporary loader manifest
that names the exact absolute library path. Its SHA-256 was
`f9264ebb2b5e2759b6c752c29bcd6d0f515b077f1dbd50b7356a49ef6ec1b4ba`.

Direct ICD queries proved:

```text
deviceName: Mali-G615 MC6
deviceID: 0xb8a31030
vendorID: 0x13b5
driverID: 20 (VK_DRIVER_ID_MESA_PANVK)
driverName: panvk
apiVersion: 1.4.363
```

Enumeration returned exactly one physical device. No system Mali or lavapipe
device was enumerated.

## Independent outcomes

| Gate | Result | Evidence |
| --- | --- | --- |
| Vulkan instance/physical-device creation | PASS | Direct ICD enumerate exited 0; one Mali-G615 MC6 PanVK device |
| Vulkan logical-device creation | PASS | Offscreen harness reached `F-device ready` |
| Offscreen render/submission/readback | PASS | Red triangle over blue clear; `F-submit complete`; exited 0 |
| glibc presentation | BLOCKED | No X11 or Wayland socket; no `DISPLAY`, `WAYLAND_DISPLAY`, or `XDG_RUNTIME_DIR` |
| Stock DXVK device creation | CRASH_BEFORE_REQUIREMENT_GATE | DXVK 3.1.1 loaded `libvulkan.so.1`, selected SDL2 provider, then segfaulted |
| Expected stock DXVK baseline rejection | NOT_CAPTURED | No adapter capability/rejection message was reached |
| DXVK rendering | NOT_RUN | Device creation did not complete |
| DXVK presentation | BLOCKED | No real display session; SDL offscreen explicitly lacks Vulkan window support |

`SDL_VIDEODRIVER=offscreen` was used only to prove that it cannot substitute for
real presentation. SDL rejected Vulkan window creation. It is not counted as a
presentation attempt or PASS. The added `d3d11-headless` probe separates SDL
window creation from D3D11 device creation without changing stock DXVK.

Stock DXVK remained unchanged at commit
`b1a1c99ab52b687cf950d62c88bc2fa316b41663`. The rebuilt probe SHA-256 was
`93a41073373ea294c38858810e3ec9ba7ccf581c15d9850842d34b63eb3e12bf`.

## GPU execution trace

The safe uninstrumented offscreen run produced one
`kbase_queue_wait_current: wait completed successfully` message after a correct
GPU-rendered readback. Counters are defined as:

```text
submitted_jobs: kbase CSF ring jobs emitted by kbase_subqueue_emit_job
completed_jobs: emitted job sequence numbers observed complete by kbase_subqueue_wait_seqno
successful_waits: completed kbase queue waits reported by kbase_queue_wait_current
```

Observed safe counters:

```text
submitted_jobs: unavailable
completed_jobs: unavailable
successful_waits: 1
```

Existing userspace diagnostics are not usable for submission proof on this
candidate. `PANVK_DEBUG=kbase_diag` and `PANVK_DEBUG=trace` both segfaulted before
a successful submission and yielded zero emitted/completed job records. No
kernel tracing, sysfs mutation, perfetto, strace, or dangerous recovery was used.
Therefore GPU rendering is proven by output plus completion, but the required
submission-count trace is `BLOCKED`, not PASS.

## DX3 status

`PARTIAL`: same-candidate ICD creation and offscreen rendering pass. Presentation,
expected stock DXVK baseline rejection, and non-crashing GPU submission trace
remain unresolved. DX4 must not start from this result.

## 2026-09-29 continuation

The later Xvfb-backed DXVK Native run creates D3D9 and D3D11 devices and passes
GPU-rendered D3D9 and D3D11 readback workloads on the integrated G615 driver.
See `DX8-NATIVE-WORKLOAD.md`. The earlier crash was limited to the no-display
SDL2 startup path; physical presentation remains unverified.
