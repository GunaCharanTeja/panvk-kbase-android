# Boundary found: normal DXVK never emits the orange Vulkan clear

Unchanged beta.8 driver. No driver or DXVK rebuild. Native test modes:
`sample`: offscreen optimal BGRA8 render-pass loadOp=CLEAR/STORE, explicit
COLOR_ATTACHMENT_WRITE -> FRAGMENT_SHADER/SHADER_READ dependency, sampled
fullscreen triangle into a black-cleared swapchain attachment, Present.
`sample-copy`: identical, with image-to-buffer copy between clear and sampling,
COLOR_ATTACHMENT_WRITE -> TRANSFER_READ then -> SHADER_READ dependencies.
No swapchain image copy in either mode. The baseline producer is not CPU-read.
Per-swapchain-image render-finished semaphores; frame submission fence before
reuse; 8 frames then held captures.

Both modes, baseline repeated: **76800/76800 orange client pixels**.
Forced-copy source: **76800/76800 orange**, exact BGRA `40 80 ff ff`.
This proposed native driver fast-clear/sampling reproducer does NOT fail.
SPIR-V validated using `spirv-val`; compile clean with `-Wall -Wextra`.
Khronos validation layer unavailable on device/host. API dump is a tracing
layer, not a Vulkan validation layer; no validation-layer pass claimed.

## Installed DXVK API trace: producer clear absent

Existing `libVkLayer_api_dump.so` records actual Wine Vulkan calls without
inserting copies. New test-only layer manifest; app environment unchanged.
Traced default stays black; traced readback stays orange, captured after frame 7
while alive. Full traces, screenshots, P6 client/root images, commands and
timestamps: `api-trace/default/`, `api-trace/readback/`.

| Installed DX11 trace | Default | Readback |
| --- | ---: | ---: |
| Orange Vulkan clear commands | **0** | **8** |
| Rendering loadOp=CLEAR | **0** | **8** |
| Presenter vkCmdDraw | 8 | 8 |
| vkQueuePresentKHR | 8 | 8 |
| Client orange | 0/76800 | 76800/76800 |

Default only emits initialization clear-to-zero plus eight presenter draws;
all eight rendering attachments use DONT_CARE. No orange clear command reaches
Vulkan. Readback emits eight orange loadOp=CLEAR rendering operations before
copy/readback and presenter draws. This excludes a driver losing an orange
Vulkan clear in the normal sequence: it never received one.

Matching upstream v3.1.1 source:
- `src/dxvk/dxvk_context.cpp:360` `clearRenderTarget`: defers bound-target clear.
- `:204` `beginExternalRendering`: `endCurrentCommands`, `beginCurrentCommands`.
- `:9383` `endCurrentCommands`: calls `endCurrentPass(true)`.
- `:6201` `endRenderPass`: inactive pass + suspend=true does not call
  `prepareShaderReadableImages(false)`; clears remain pending.
- `src/d3d11/d3d11_swapchain.cpp` `PresentImage`: calls
  `beginExternalRendering`, external blitter `present`, submits/presents.
The installed trace establishes the exact application-to-Vulkan causal boundary;
upstream source explains deferred-clear materialization versus external blit.
Installed ARM64EC fork source identity beyond version string remains unverified.

**Not a confirmed driver issue.** Native sampled clear passes. Normal DXVK
producer clear disappears above driver entry. CPU readback only forces that
producer operation; it is not a production fix or acceptance criterion.
Next focused work: installed DXVK deferred-clear/external-rendering integration
or an independently verified runtime option. No normal-pass claim yet.

Documented DXVK options also tested without CPU readback:
`d3d11.reproducibleCommandStream = True`, `dxvk.tilerMode = False`.
Both remain black, zero orange Vulkan clears. Full option traces/captures in
`api-trace/reproducible/`, `api-trace/tiler-off/`. No app config workaround shipped.
Option documentation: https://github.com/doitsujin/dxvk/blob/v3.1.1/dxvk.conf

## Reproduce / check

```sh
sh apps/panvk-launcher/tests/results/final-discrimination/sample-reproduce.sh
python3 apps/panvk-launcher/tests/results/final-discrimination/sample-check.py
```

Requires previously deployed final-discrimination dxsmoke/dxenv/xreadback.
Driver hashes remain in every native `identity.txt`; previous normal bundles
hold DLL/driver identity. Tests only; worklogs untouched; no commit.
