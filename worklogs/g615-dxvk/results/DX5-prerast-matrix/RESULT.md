# DX5 MATRIX_PRERAST only

Status: `MATRIX_PRERAST_FAIL`
UTC: `2026-09-21T16:46:17Z`
ADB: `192.168.1.34:32913` serial `Y5WWBMJVOZSK4HU8`
Device: duchamp / Mali-G615 MC6 `0xb8a31030` `/dev/mali0`

ICD `23fee3c1cc9f55ca59d42abd55f14f5595bed602dbdb0c08cd771860ed2621b0`
size 20053312 BuildID `a0aa104ea3371601c119895ebaf8452231100396`.
Matches prior IDVS ICD. No rebuild. No overlay. No IDVS redo. App-local chroot only.

```
PANVK_DEBUG=gpu_prerast
CASE idvs_before RGBA=0 0 0 0 FAIL
MESA: error: kbase: CS error 0x1 on subqueue 0
FAIL Wait r=-4 line=584
MATRIX_PRERAST_RC=1
EXPOSURE all 0
```

First case only. `vkWaitForFences` returns `VK_ERROR_DEVICE_LOST` (-4).
Clear is blue `(0,0,1,1)`; readback is black `(0,0,0,0)` — not the prior
blue leftover `(0,0,255,255)`. Remaining 12 cases not run (device lost).
BUILD_PASS ≠ DEVICE_TEST. No DX6.
