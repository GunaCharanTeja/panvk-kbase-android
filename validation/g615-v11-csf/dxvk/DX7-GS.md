# DX7 geometry shaders (gpu_prerast)

Status: `GPU_LOWERED`. Base patch `csf-v11/042` is dev-gated; integrated Mesa
commit `cf5aa478003` enables geometry shaders by default on v10+. The integrated
driver passes the 23-case device smoke without a debug flag. Six cube-layer CTS
failures remain, so full geometry conformance is open.
Mesa base: `bdd0c26cc5e` on `efce7cc4382` on `89274dbd817` (= `csf-v11/040`).

Mechanism (all GPU):
- The VS runs as the gpu_prerast lowered VS (compute) and writes post-VS records.
- The GS is IO-lowered and turned into a compute kernel: one invocation per
  (input vertex position, instance, GS invocation). Positions that close a
  primitive run the GS body. Each invocation owns `max_vertices` output records
  and `index_stride` indices, prefilled with the restart index, so output order
  matches API order without a prefix sum.
- An IDVS VS reads the GS output records and draws indexed with restart.
- No CPU readback, no CPU geometry.

Limits reported with `PANVK_DEBUG=gs`: `maxGeometryShaderInvocations=32`,
input components 64, output components 128, `maxGeometryOutputVertices=256`,
total output components 1024.

## Device proof (2026-09-29)

ADB `192.168.1.61:41161` (duchamp, Mali-G615 MC6, kbase CSF v11), Alpine chroot
`/data/local/tmp/chrootAlpine`. Device tree `/tmp/mesa` synced to `bdd0c26cc5e`
(sha256 of all 75 files changed since pin `5a07217f` identical to host HEAD),
built with `/tmp/bld.sh` (`ninja -j4 -C /tmp/build-glibc src/panfrost/vulkan/libvulkan_panfrost.so`).
ICD sha256 `3a10ad4adda689c66d7dd104d649e8be6bb7222cfc0cfd76c52ca923d05d25f0`.

Test: `tests/dxvk/vulkan/geometry/` (`build_spv.sh` + `geometry.c`, harness
`tests/dxvk/vulkan/dx7_harness.h`). Each case draws VS+GS+FS and compares the
64x64 RGBA readback pixel-exact against a GPU reference drawn without a GS (the
primitives the GS is specified to emit, as TRIANGLE_LIST/LINE_LIST via IDVS).
Pushed to chroot `/tmp/dx7-h/`, run with `/tmp/gst.sh gs`:

```text
sh /tmp/dx7-h/geometry/build_spv.sh
gcc -O2 -Wall -o /tmp/gst /tmp/dx7-h/geometry/geometry.c -ldl
PANVK_DEBUG=gs /tmp/gst /tmp/build-glibc/src/panfrost/vulkan/libvulkan_panfrost.so
```

```text
ICD device=Mali-G615 MC6 geometryShader=1 fillModeNonSolid=1 multiViewport=1 shaderClipDistance=1 shaderCullDistance=1 maxViewports=16 maxClip=8 maxCull=8 maxCombined=8
limits invocations=32 in=64 out=128 outv=256 total=1024
CASE tri_list_pass outv=9 ref_red=1009 red=1009 mismatch=0 PASS
CASE tri_strip_pass outv=9 ref_red=598 red=598 mismatch=0 PASS
CASE tri_fan_pass outv=15 ref_red=903 red=903 mismatch=0 PASS
CASE tri_strip_restart_pass outv=12 ref_red=771 red=771 mismatch=0 PASS
CASE tri_list_indirect outv=9 ref_red=1009 red=1009 mismatch=0 PASS
CASE tri_strip_restart_indexed_indirect outv=12 ref_red=771 red=771 mismatch=0 PASS
CASE tri_list_instanced3 outv=18 ref_red=439 red=439 mismatch=0 PASS
CASE tri_list_instanced3_indirect outv=18 ref_red=439 red=439 mismatch=0 PASS
CASE line_list_pass outv=8 ref_red=176 red=176 mismatch=0 PASS
CASE line_strip_pass outv=10 ref_red=208 red=208 mismatch=0 PASS
CASE point_to_quad outv=30 ref_red=180 red=180 mismatch=0 PASS
CASE tri_to_lines_endprim outv=18 ref_red=267 red=267 mismatch=0 PASS
CASE two_tris_endprim outv=18 ref_red=1439 red=1439 mismatch=0 PASS
CASE strip_out4 outv=18 ref_red=1283 red=1283 mismatch=0 PASS
CASE invocations3 outv=18 ref_red=431 red=431 mismatch=0 PASS
CASE primid_list outv=6 ref_red=684 red=684 mismatch=0 PASS
CASE primid_strip_restart outv=9 ref_red=382 red=382 mismatch=0 PASS
CASE primid_instanced outv=6 ref_red=182 red=182 mismatch=0 PASS
CASE adj_tri_list outv=6 ref_red=619 red=619 mismatch=0 PASS
CASE adj_tri_strip outv=9 ref_red=598 red=598 mismatch=0 PASS
CASE adj_line_list outv=4 ref_red=87 red=87 mismatch=0 PASS
CASE adj_line_strip outv=6 ref_red=123 red=123 mismatch=0 PASS
CASE layer_write outv=9 ref_red=1009 red=1009 mismatch=0 PASS
GEOMETRY_FAILS=0
```

Per mode (11 modes, 23 cases, all PASS):

| mode | cases | result |
|---|---|---|
| PASS_TRI | 8 (list/strip/fan/restart, indirect, indexed-indirect, instanced x3 direct+indirect) | 8/8 |
| PASS_LINE | 2 (list, strip) | 2/2 |
| POINT_QUAD | 1 | 1/1 |
| TRI_WIRE (EndPrimitive) | 1 | 1/1 |
| TWO_TRIS (EndPrimitive) | 1 | 1/1 |
| STRIP4 | 1 | 1/1 |
| INVOC3 (`invocations=3`) | 1 | 1/1 |
| PRIMID | 3 (list, strip+restart, instanced) | 3/3 |
| ADJ_TRI | 2 (list, strip) | 2/2 |
| ADJ_LINE | 2 (list, strip) | 2/2 |
| LAYER (`gl_Layer`) | 1 | 1/1 |

Default (no `PANVK_DEBUG`): `geometryShader=0`, test stops at
`FAIL CreateDevice r=-8` (feature not present), as expected.

## CTS (`deqp-vk`, all of `dEQP-VK.geometry.*`, 199 cases)

```text
/tmp/ctsg.sh geomgs '^dEQP-VK.geometry.' PANVK_DEBUG=gs
```

| group | Pass | Fail | NotSupported |
|---|---|---|---|
| basic | 12 | 0 | 2 |
| builtin_variable | 4 | 0 | 1 |
| emit | 23 | 0 | 0 |
| input | 28 | 0 | 0 |
| instanced | 16 | 0 | 8 |
| layered | 94 | 6 | 0 |
| varying | 5 | 0 | 0 |
| **total** | **182** | **6** | **11** |

No crash, no device loss, no queue timeout.

NotSupported: `vertexPipelineStoresAndAtomics` (2),
`shaderTessellationAndGeometryPointSize` (1), `maxGeometryShaderInvocations`
< 64 / < 127 (8).

Fails (deterministic; identical with `PANVK_DEBUG=gs,noafbc`, `gs,no_crc`, `gs,sync`):

```text
dEQP-VK.geometry.layered.cube.64_64_6.readback                  Color Depth Stencil
dEQP-VK.geometry.layered.cube.36_36_6.readback                  Color Depth Stencil
dEQP-VK.geometry.layered.cube_array.64_64_12.readback           Color Depth Stencil
dEQP-VK.geometry.layered.cube_array.36_36_12.readback           Color Depth Stencil
dEQP-VK.geometry.layered.cube_array.64_64_12.secondary_cmd_buffer  Rendered images are incorrect
dEQP-VK.geometry.layered.cube_array.36_36_12.secondary_cmd_buffer  Rendered images are incorrect
```

`readback` passes for 1d_array, 2d_array (incl. 6 layers) and 3d; it fails only
for CUBE/CUBE_ARRAY views. Face 0 is valid; faces 1..N-1 are wrong for color,
depth and stencil. The GS loops over all layers and emits one quad per layer
with increasing Z, two passes with `LOAD_OP_LOAD` and a D/S cube attachment.
A valid face 0 with the other faces wrong fits every primitive being routed to
layer 0 (face 0 wins the depth test) when the attachments are cube views with
D/S. The other cube tests (`render_to_all`, `invocation_per_layer`, ...) are
color-only and pass. `secondary_cmd_buffer` fails only for cube_array (FS
`imageStore` into an `imageCubeArray` storage view). Not root-caused. Suspects:
FB/ZS layer addressing for cube D/S attachment views, and cube-array storage
coordinates (face + 6 * layer) in descriptor lowering.

## Integrated device smoke (2026-09-29)

Mesa `dx6-dx7-base` at `be230148967`, including default exposure, the tiler heap
fix, and pipeline statistics. The device build returned `NINJA_RC=0` and ICD
SHA-256 `29fbd359ad072e8587329d8d758fb4c46f7cf22791e07b26143db5983d0125f8`.
Running `/tmp/gst /tmp/build-glibc/src/panfrost/vulkan/libvulkan_panfrost.so`
with no `PANVK_DEBUG` reported `geometryShader=1`, all 23 cases `PASS`, and
`GEOMETRY_FAILS=0`.

## Still open

- 6 layered cube/cube_array fails above.
- `maxGeometryShaderInvocations=32` (CTS wants 64/127 for 8 instanced cases).
- CTS coverage for the default-on integrated driver, including indirect-count,
  oversized, and non-fill polygon draws.
