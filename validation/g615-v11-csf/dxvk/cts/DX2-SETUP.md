# DX2 Vulkan CTS setup

## Pins

- VK-GL-CTS: `vulkan-cts-1.4.6.2`, commit `f6a29701220f34dd1407513bfe80d74ca7b392ce`
- Vulkan-Headers source: Vulkan-Docs commit `4abe0260bbd8e59930786eed645481808a3fe6f1`, generated header version `1.4.353`
- Build ABI: native Linux `aarch64`, Alpine chroot on Poco X6 Pro / duchamp, Release, `vulkan_headless`
- Compiler: Alpaquita Clang `22.1.8`

Every script rejects targets other than `duchamp`, requires `arm64-v8a` and `/dev/mali0`, then performs source setup, compilation, or execution inside the device's native AArch64 Alpine chroot. No host-built binary is accepted. `fetch-vk-gl-cts.sh` verifies both source commits. `build-vk-cts.sh` builds the standalone Vulkan-headless executable on-device. This executable is suitable for focused development subsets, not WSI tests or conformance submissions.

`run-vk-cts-subset.sh TEST_LIST OUTPUT_DIR` stores `console.txt`, `results.qpa`, and `summary.json`. The summary records pass, fail, skip, not-supported, crash, and device-lost counts plus the exact test-list SHA-256.

## DX2 status

- Source fetch: PASS
- Device identity: PASS (`Y5WWBMJVOZSK4HU8`, Xiaomi `2311DRK48I`, `duchamp`, `MT6897Z_A/ZA`, `arm64-v8a`, `/dev/mali0`)
- Device-native AArch64 build: PASS
- Binary: ARM64 ELF, 89,657,384 bytes
- Binary SHA-256: `5515cea5213851879c1b8f690aadae5ae6f50b117514b2d28e36f3539f2caac7`
- Standard test list: 7,702,260 cases
- Standard test-list SHA-256: `3d046479d4f23f574d9bc93511d028831468184e508f5e4c981d6643450fffc0`
- CTS execution: NOT RUN; DX2 installs tooling only

An untrusted pre-existing host Android build was detected and deliberately not used. The host is limited to source, scripts, and captured validation records.

The first foreground build lost its ADB session under unconstrained Ninja load. The resumed device-native build used two jobs and completed successfully. No compiler error, device loss, or CTS failure occurred.

No Vulkan conformance claim is made.
