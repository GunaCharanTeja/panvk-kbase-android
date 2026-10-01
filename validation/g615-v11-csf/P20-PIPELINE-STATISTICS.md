# P20 pipelineStatisticsQuery and remaining PROFILE BASELINE extras

Historical baseline audit: this `BLOCKED_SAFE_FALSE` result describes the
original Mesa pin. The later GPU implementation and G615 results are recorded
in [DX8-PIPELINE-STATS.md](dxvk/DX8-PIPELINE-STATS.md).

## Result

`BLOCKED_SAFE_FALSE`

No complete PanVK `VK_QUERY_TYPE_PIPELINE_STATISTICS` implementation exists
on Valhall v11 / G615. The G615 runtime remains:

```text
pipelineStatisticsQuery            false
```

`vkCreateDevice` with `pipelineStatisticsQuery=true` returns
`VK_ERROR_FEATURE_NOT_PRESENT`.

No PanVK patch or feature-reporting change was made. P21 and P22 were not
started.

## Source Audit

- Pinned Mesa: `5a07217f034b3e50d8c7c7794f97a2df1742613b`.
- Audited Mesa `origin/main`: `e1f3f372c4a661cd0e71a0cdafc4d469d56ecf35`.
- Post-pin Panfrost commits: `1a63ca3af98`, `a78c40578d6`, `2092558f77d`.
  None implement pipeline statistics.
- Pinned and current upstream PanVK both report
  `pipelineStatisticsQuery=false` in
  `src/panfrost/vulkan/panvk_vX_physical_device.c`.
- Query infrastructure covers only:
  - `VK_QUERY_TYPE_OCCLUSION` (hardware occlusion counters)
  - `VK_QUERY_TYPE_TIMESTAMP` (CSF timestamp, PAN_ARCH >= 10)
  - `VK_QUERY_TYPE_PRIMITIVES_GENERATED_EXT` (software CSF accounting)
- `panvk_vX_query_pool.c` `CreateQueryPool` / `GetQueryPoolResults` switch
  hits `UNREACHABLE("Unsupported query type")` for any other type, including
  `VK_QUERY_TYPE_PIPELINE_STATISTICS`.
- CSF `panvk_vX_cmd_query.c` begin/end/reset/copy switches have the same
  three types. JM query code is occlusion-only.
- `panvk_cmd_query.h` stores occlusion and primitives-generated state only.
  There is no pipeline-stat pointer, flag mask, or per-counter report layout.
- `VK_EXT_primitives_generated_query` is implemented in CSF draw accounting
  (`update_prims_generated_query`) but is **not advertised** in
  `get_device_extensions()`. It is not a substitute for
  `VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT`.
- Gallium Panfrost has no `PIPE_QUERY_PIPELINE_STATISTICS` path.
- `src/panfrost/perf` Mali performance counters are a profiling database.
  They are not wired into PanVK query pools and do not map 1:1 onto Vulkan
  pipeline statistic bits (IA vertices, VS invocations, clip invocations,
  FS invocations, CS invocations).
- Mesa git history has no PanVK pipeline-statistics series. The only
  `pipelineStatisticsQuery` hit outside other vendors is
  `anv: Don't advertise pipelineStatisticsQuery`.
- Asahi `hk_query_pool.c` implements software pipeline statistics. That is
  a semantic reference only. Cargo-culting it into PanVK would be a new
  implementation, not an upstream PanVK backport.

## Semantic Blocker

Vulkan `pipelineStatisticsQuery=true` requires
`VK_QUERY_TYPE_PIPELINE_STATISTICS` pools whose advertised counters match
the spec for every enabled bit. vkd3d `VP_D3D12_FL_11_0_baseline` requires
the feature bit. DXVK stock profiles do not.

Required counters:

```text
VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT
VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT
VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT
VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT
VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT
VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT
VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT
VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT
VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT
VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT
VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT
```

Software accounting is allowed only if correct.

G615 currently has none of:

- a query-pool report layout for N statistic bits
- begin/end/reset/copy for `VK_QUERY_TYPE_PIPELINE_STATISTICS`
- per-draw IA vertex/primitive accounting that matches restart, indexed,
  indirect, instanced, and multiview
- VS invocation counts (IDVS vertex threads are not Vulkan VS invocations)
- clip invocation/primitive counts
- FS invocation counts (helper invocations, sample-rate, early-Z)
- CS invocation counts (workgroup size × dispatched groups, including
  indirect)
- GS/TCS/TES counters: those stages are disabled (P12, P17). Spec still
  requires the bits to read as zero when the corresponding feature is
  disabled and no such shaders are bound. That still needs a real query
  object that returns zeros, plus tests.

Flipping `pipelineStatisticsQuery` would advertise pools that currently
`UNREACHABLE`.

Existing primitives-generated software math counts decomposed primitives
for the active draw. It does not count IA vertices, VS/FS/CS invocations,
or clipping. It is not advertised. Reusing it as
`INPUT_ASSEMBLY_PRIMITIVES` without the rest of the bitmask would be a
partial feature.

## Required Design

A future complete implementation must finish one coherent path before
exposure.

### Path A — hardware Mali counters

Map each Vulkan statistic onto a Mali counter that has identical
semantics, including restart, instancing, multiview, mesh-less IDVS, and
helper invocations. Current `pan_perf` data does not provide that mapping
for Valhall v11. Do not expose a best-effort counter.

### Path B — software accounting (acceptable if correct)

1. Allocate one `uint64_t` report per enabled pipeline-statistic bit.
2. On graphics draws, add:
   - IA vertices from vertex count × instance count, with indexed and
     indirect sources
   - IA primitives from decomposed topology, matching primitive restart
   - VS invocations: Vulkan vertex-shader invocations, not Mali IDVS
     position/varying thread counts unless proven equal
   - clipping invocations/primitives after clip volume and user clip
     (P10 clip/cull is also false)
   - FS invocations after rasterization, including helper invocations
     when required
3. On compute dispatches, add CS invocations = `x * y * z * workgroupSize`.
4. When `geometryShader` / `tessellationShader` are false, GS/TCS/TES
   counters stay zero. When those features later become true, the same
   query path must count real invocations.
5. Honor availability, `WAIT`, `PARTIAL`, 32/64-bit results, host reset,
   `vkCmdCopyQueryPoolResults`, and simultaneous occlusion/timestamp
   queries.
6. Run the counter matrix and CTS before setting
   `pipelineStatisticsQuery=true`.

Do not expose a subset (compute-only, primitives-only, zeros-for-everything).

## Remaining PROFILE BASELINE extras

P19 `VP_D3D12_FL_11_0_baseline` still fails 15 leaves on both vkd3d 2.14.1
and 3.0.1. P20 owns only `pipelineStatisticsQuery`. The rest stay with
their original owners. P21/P22 sparse is out of scope.

| Extra | Gate | Owner | Status | Action |
|---|---|---|---|---|
| `pipelineStatisticsQuery` | PROFILE_BASELINE | P20 | `NOT_IMPLEMENTED` / `BLOCKED_SAFE_FALSE` | Keep false |
| `robustImageAccess2` | PROFILE_BASELINE + DEVICE_CREATE | P6 | `UNSUPPORTED` | Keep false |
| `textureCompressionBC` | PROFILE_BASELINE | P9 | `UNSUPPORTED_NATIVE` | Keep false |
| `shaderClipDistance` | PROFILE_BASELINE | P10 | `BLOCKED_SAFE_FALSE` | Keep false |
| `shaderCullDistance` | PROFILE_BASELINE | P10 | `BLOCKED_SAFE_FALSE` | Keep false |
| `fillModeNonSolid` | PROFILE_BASELINE | P11 | `BLOCKED_SAFE_FALSE` | Keep false |
| `geometryShader` | PROFILE_BASELINE | P12 | `BLOCKED_SAFE_FALSE` | Keep false |
| `multiViewport` | PROFILE_BASELINE | P14 | `BLOCKED_SAFE_FALSE` | Keep false |
| `VK_EXT_transform_feedback` | PROFILE_BASELINE | P15 | `BLOCKED_SAFE_FALSE` | Keep absent |
| `transformFeedback` | PROFILE_BASELINE | P15 | `BLOCKED_SAFE_FALSE` | Keep false |
| `geometryStreams` | PROFILE_BASELINE | P15 | `BLOCKED_SAFE_FALSE` | Keep false |
| `transformFeedbackQueries` | PROFILE_BASELINE | P15 | `BLOCKED_SAFE_FALSE` | Keep false |
| `tessellationShader` | PROFILE_BASELINE | P17 | `BLOCKED_SAFE_FALSE` | Keep false |
| `bufferImageGranularity` | evaluator | n/a | `EVALUATOR_DIRECTION` | No driver change. Actual 64 is finer than required 65536 |
| `subgroupSupportedOperations[0]` | evaluator | n/a | `EVALUATOR_ARRAY` | No driver change. Bitmask 1791 already includes BASIC |
| sparse / tiled resources | higher FL | P22 | not started | Out of P20 scope |

DXVK D3D9/D3D10/D3D11 remaining fails are the same P9–P17 graphics bits.
DXVK stock profiles do not require `pipelineStatisticsQuery`. P20 does
not change DXVK gates.

Machine inventory: `validation/g615-v11-csf/p20-remaining-baseline-extras.json`.

## Required Test Matrix

Counter tests are invalid while `pipelineStatisticsQuery` is disabled.
These cases remain blocked:

```text
IA vertices
IA primitives
VS invocations
GS invocations
GS primitives
clipping invocations
clipping primitives
FS invocations
TCS patches
TES invocations
CS invocations
```

`tests/dxvk-vkd3d/test_p20_pipeline_statistics_safe_false.py` guards
against accidental exposure and detects a future PanVK query, compiler,
Gallium, or Mali-perf-wired implementation that requires this assessment
to be repeated.

`tests/dxvk-vkd3d/test_p20_remaining_baseline_extras.py` checks the
inventory: P20 owns only pipeline statistics, prior-phase extras stay
false, evaluator artifacts are not driver bugs, sparse stays P22.

`tests/dxvk-vkd3d/p20-pipeline-statistics.c` is the G615 runtime probe:
it asserts the feature stays false and enabling it is rejected. It is
not a substitute for counter semantics or CTS.

The G615 identity check in this session returned `duchamp` / `2311DRK48I`.
The staged Android ICD SHA-256 is
`576e37de9a3b60dda9a972a6f91c3a50e09238bd31a9888e04215c7b554c803a`, matching
the P10–P19 ICD. Serial `Y5WWBMJVOZSK4HU8` stayed connected. The live probe
reported `pipelineStatisticsQuery=NO`,
`VK_EXT_primitives_generated_query=NO`, and `vkCreateDevice` with
`pipelineStatisticsQuery=true` as `VK_ERROR_FEATURE_NOT_PRESENT`. Result
`BLOCKED_SAFE_FALSE`.

Vulkan CTS is `BLOCKED`: no `deqp-vk` binary or matching CTS test list is
installed on the host or G615. CTS version, test-list hash, and
pass/fail/skip/crash/device-lost counts are `NOT_TESTED`.

## Exit

P20 exits `BLOCKED_SAFE_FALSE`. `pipelineStatisticsQuery` stays false.
Remaining vkd3d PROFILE BASELINE extras besides P21/P22 sparse are
documented with truthful status. Next engineering step: a complete
pipeline-statistics query path whose advertised counters match Vulkan
semantics, then CTS. Do not flip the bit first.

P21 D3D12 feature/profile analysis and P22 sparse/Kbase were not started.
