# DX5 load_attr_pan io offset

Status: `PARTIAL` (SIGSEGV gone; matrix CASE FAIL)
UTC: `2026-09-21T16:12:21Z`
ADB: `192.168.1.34:32913` serial `Y5WWBMJVOZSK4HU8`
Device: duchamp / Mali-G615 MC6 `0xb8a31030` `/dev/mali0`

ICD `55e2be9f0c767e9b90914fd606f82e9e4c30d599e74b9421f8a8e3be3605424d`
size 20053320 BuildID `c1f11d860c2f4540709defca3ebf0609abf373be`.
Stale `f38bee64…` replaced. App-local Alpine chroot only.

## Change

`patches/common/019-nir-load-attr-pan-io-offset.patch`:
`nir_get_io_offset_src_number` `case nir_intrinsic_load_attr_pan: return 2;`
(src `{vertex_id, instance_id, handle}`). Overlay + wipe `*nir_lower_io*.o`.

## Device

```
NINJA PASS
CREATEDEVICE_IDVS=PASS r=0
CREATEDEVICE_PRERAST=PASS r=0
EXPOSURE all 0
MATRIX_IDVS_RC=1  PASS: idvs_before, zero_count, repeated_indices; FAIL 10
MATRIX_PRERAST_RC=1  CASE idvs_before RGBA=0 0 255 255 FAIL; Wait r=-4
```

No SIGSEGV 139. BUILD_PASS ≠ DEVICE_TEST. No DX6.
