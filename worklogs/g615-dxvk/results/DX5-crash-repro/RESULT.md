# DX5 crash-repro

Status: `DIAGNOSED` (not PASS)
UTC: `2026-09-21T15:42:00Z`
ADB: `192.168.1.34:32913` serial `Y5WWBMJVOZSK4HU8`
Device: duchamp / Mali-G615 MC6 `0xb8a31030` `/dev/mali0`

ICD `f38bee643aec947fe9b97288e0701182c70324149ca1a5cc29a8a6de7f1c40ca`
size 20053320 BuildID `f2fea46665b9d9001cb281b83a5a638126b81bfc`. No overlay rebuild.

## Fault

`MATRIX_IDVS_RC=139` `MATRIX_PRERAST_RC=139`. CreateDevice prints. First `vkCreateGraphicsPipelines` (`STEP GfxPipe`) SIGSEGV before `CASE`.

```
CRASH sig=11 code=1 addr=0x18 pc=ICD+0x4df660 lr=same
gather_func_info
  bl nir_get_io_offset_src
  ldr x0, [x0, #24]   ; nir_src.ssa ; NULL+0x18
```

`nir_gather_info.c` groups `nir_intrinsic_load_attr_pan` with `load_input` and calls `nir_get_io_offset_src`. That helper has no `load_attr_pan` case → NULL.

`load("attr_pan", [1, 1, 1], …)` srcs `{ vertex_id, instance_id, handle }`. Offset is src 2.

## Exact fix (not applied)

`src/compiler/nir/nir_lower_io.c` `nir_get_io_offset_src_number`:
`case nir_intrinsic_load_attr_pan: return 2;`

Then rebuild ICD and re-run DX5-matrix. BUILD_PASS ≠ DEVICE_TEST.
