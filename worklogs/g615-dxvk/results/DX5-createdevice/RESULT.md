# DX5 CreateDevice

Status: `CREATEDEVICE_OK`
UTC: `2026-09-21T15:28:04Z`
ADB: `192.168.1.34:32913` (network)
On-device serial: `Y5WWBMJVOZSK4HU8`
Device: duchamp / 2311DRK48I / arm64-v8a / Mali-G615 6 cores `0xB8A3` / `/dev/mali0`

## Evidence

Identity once: `ro.serialno=Y5WWBMJVOZSK4HU8`. No overlay rebuild. No matrix.

ICD `f38bee643aec947fe9b97288e0701182c70324149ca1a5cc29a8a6de7f1c40ca`
size 20053320 BuildID `f2fea46665b9d9001cb281b83a5a638126b81bfc`.

```text
PROBE_COMPILE=PASS
CREATEDEVICE_IDVS=PASS r=0
CREATEDEVICE_PRERAST=PASS r=0 PANVK_DEBUG=gpu_prerast
EXPOSURE geometryShader=0 fillModeNonSolid=0 shaderClipDistance=0 shaderCullDistance=0 tessellationShader=0
ICD device=Mali-G615 MC6 id=0xb8a31030 api=1.4.363
MATRIX=NOT_RUN
STATUS=CREATEDEVICE_OK
```

Prior SIGSEGV 139 on 64 MiB arena ICD is gone on this 256 KiB gated ICD.
BUILD_PASS from overlay ≠ this DEVICE_PASS. App-local chroot only.

## Next bounded microtask

`DX5-matrix`: same ADB/serial. Identity once. Vertical-slice matrix on
default IDVS and `PANVK_DEBUG=gpu_prerast` against the same ICD.
Stop after matrix. No overlay rebuild, no DX6.
