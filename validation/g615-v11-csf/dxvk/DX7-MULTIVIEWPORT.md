# DX7 multiViewport

Status: exposed on v10+ (`multiViewport=1`, `maxViewports=16`).
Patch: `patches/csf-v11/024-multi-viewport-index0.patch`.

No exposed stage can write `ViewportIndex` (`geometryShader=0`,
`shaderOutputViewportIndex=0`), so every primitive uses viewport/scissor 0.
That is the spec behaviour; the driver already emits `viewports[0]` /
`scissors[0]`. Arrays up to 16 (static and dynamic, incl. `firstViewport`)
are accepted.

When GS or `shaderOutputViewportIndex` is exposed, per-primitive selection is
required. Candidate: v10/v11 DCD "Scissor array enable" (genxml) + per-viewport
transform in the VS (panvk does the viewport transform in the VS). Not done.

## Device proof (2026-09-29)

Same ICD/tree as DX7-CLIP-CULL.md. Test:
`tests/dxvk/vulkan/multi-viewport/multi_viewport.c`.

```text
CASE static_vp16_vp0_left red=2048 mismatch=0 PASS
CASE static_sc16_sc0_top red=2048 mismatch=0 PASS
CASE dynamic_vp4_vp0_bottom_right red=1024 mismatch=0 PASS
CASE dynamic_first1_keeps_vp0 red=1024 mismatch=0 PASS
CASE dynamic_sc4_sc0_center red=1024 mismatch=0 PASS
MULTI_VIEWPORT_FAILS=0
```

Identical with `PANVK_DEBUG=gpu_prerast`. CTS multi-viewport tests need GS or
`shaderOutputViewportIndex` and are not run.
