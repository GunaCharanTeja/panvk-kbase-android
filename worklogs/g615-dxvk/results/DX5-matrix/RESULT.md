# DX5 matrix

Status: `MATRIX_FAIL`
UTC: `2026-09-21T15:33:30Z`
ADB: `192.168.1.34:32913` (network)
On-device serial: `Y5WWBMJVOZSK4HU8`
Device: duchamp / 2311DRK48I / arm64-v8a / Mali-G615 6 cores `0xB8A3` / `/dev/mali0`

## Evidence

Identity once: `ro.serialno=Y5WWBMJVOZSK4HU8`. No overlay rebuild. No CreateDevice redo. No DX6.

ICD `f38bee643aec947fe9b97288e0701182c70324149ca1a5cc29a8a6de7f1c40ca`
size 20053320. `ICD_STRINGS=gpu_prerast`. App-local chroot only.

Slice compile: `SLICE_COMPILE=PASS` (`clang`, `/tmp/dx5-slice`).

```text
MATRIX_IDVS_RC=139
MATRIX_PRERAST_RC=139
MATRIX=FAIL
STATUS=MATRIX_FAIL
EXPOSURE geometryShader=0 fillModeNonSolid=0 shaderClipDistance=0 shaderCullDistance=0
ICD device=Mali-G615 MC6 id=0xb8a31030 api=1.4.363
```

Both paths: CreateDevice + enum print, then SIGSEGV before any `CASE` line.
Cases all `NOT_RUN` (crash, not skip):
direct, indexed, instanced, base vertex, first instance, zero counts,
repeated indices, primitive restart, GPU-written indirect, replay,
simultaneous, IDVS before/after.

BUILD_PASS ≠ DEVICE_PASS. Skipped ≠ pass. Host tar
`130ee24f162936a1b0561eff65bdb511724c4216abcb7e015c2e35b301cb2264`.
Log: chroot `/tmp/dx5-matrix.log`.
