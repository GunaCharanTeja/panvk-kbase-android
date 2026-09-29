# DX5 IDVS black-clear CRC

Status: `IDVS_MATRIX_PASS`
UTC: `2026-09-21T16:42:55Z`
ADB: `192.168.1.34:32913` serial `Y5WWBMJVOZSK4HU8`
Device: duchamp / Mali-G615 MC6 `0xb8a31030` `/dev/mali0`

ICD `23fee3c1cc9f55ca59d42abd55f14f5595bed602dbdb0c08cd771860ed2621b0`
size 20053312 BuildID `a0aa104ea3371601c119895ebaf8452231100396`.
Prior ICD `55e2be9f…`. App-local Alpine chroot only.

## Change

`patches/csf-v11/019-crc-invalidate-undefined-clear.patch`:
`invalidate_initial_attachment_crcs` now invalidates CRC on
`LOAD_OP_CLEAR` / `DONT_CARE` as well as `PREINITIALIZED`.
Host `tests/dxvk/vulkan/test_crc_invalidate_undefined_clear.py` PASS.

## Device

Same ICD rebuilt after overlay+mesa hunk. `ninja -j2`.
`PANVK_DEBUG` unset (not `no_crc`).

```
NINJA PASS
MATRIX_IDVS 13/13 PASS  MATRIX_FAILS=0  SLICE_RC=0
EXPOSURE all 0
```

`no_crc` probe on old ICD was 13/13 PASS (CRC skip diagnosed).
MATRIX_PRERAST not re-run. BUILD_PASS ≠ DEVICE_TEST. No DX6.
