# DX0 capability and profile snapshot

## Scope

DX0 only. Branch `feature/g615-dxvk-complete` starts at
`052a5d37315e621082436fc9fa2745d0264347e2`.

This snapshot re-evaluates the existing beta.3 direct-ICD capture. It does not
claim a new device run from the repository base commit. Runtime provenance:

- Driver commit: `fc8a759e7d1b2b8de01c0e96f1fdc5e3950ba1a3`
- Mesa commit: `5a07217f034b3e50d8c7c7794f97a2df1742613b`
- Device: Poco X6 Pro / duchamp, Mali-G615 MC6, GPU ID `0xb8a31030`
- Kbase UAPI: `1.21`
- Vulkan API: `1.4.363`
- Android ICD SHA-256: `576e37de9a3b60dda9a972a6f91c3a50e09238bd31a9888e04215c7b554c803a`

Machine-readable evidence: `dx0-capability-profile-snapshot.json`.

## DXVK status

| Version | COMMON | D3D9 | D3D10 10.1 | D3D11 11.0 | D3D11 11.1 |
| --- | --- | --- | --- | --- | --- |
| 3.1.1 | PASS | FAIL | FAIL | FAIL | FAIL |
| 2.7.1 | PASS | FAIL | FAIL | FAIL | FAIL |
| 1.10.3 | N/A | FAIL | FAIL | FAIL | N/A |

DXVK 3.1.1 D3D9 blockers:

```text
geometryShader
fillModeNonSolid
shaderClipDistance
shaderCullDistance
textureCompressionBC
```

D3D10 10.1 additionally needs `multiViewport`, `VK_EXT_transform_feedback`,
`transformFeedback`, and `geometryStreams`. D3D11 11.0 additionally needs
`tessellationShader`. D3D11 11.1 additionally needs
`vertexPipelineStoresAndAtomics`.

## Truthfulness boundary

- All listed missing capability bits remain false; `VK_EXT_transform_feedback`
  remains absent.
- Vulkan CTS remains `NOT_TESTED/BLOCKED`: no `deqp-vk` evidence exists.
- DXVK Native remains `NOT_TESTED`; stock DXVK smoke remains `BLOCKED`.
- No PanVK source, feature bit, profile, or requirement manifest changed in DX0.
