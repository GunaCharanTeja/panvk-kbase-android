# DX8 pipeline statistics queries (2026-09-29)

Status: `GPU_IMPLEMENTED`, with full supported-case query CTS passing on the
Mali-G615 MC6. Mesa `work/mesa` commit `d33a343037f` includes the query
implementation and ordering fixes; tracked patches `csf-v11/050` through
`054` reproduce it after patch `049`. This does not establish overall Vulkan
or DXVK conformance.

The query pool stores counters in GPU-visible memory. CSF and GPU helper
kernels update draw, dispatch, geometry, and clipping counts, and a GPU copy
kernel handles `vkCmdCopyQueryPoolResults`. Host reads invalidate every counter
in the report. GPU queue ordering now covers indirect/restart counter kernels
and copy followed by reset; `vkGetQueryPoolResults` waits through kbase instead
of spinning on a CPU core. Five counters are exact in this implementation;
vertex shader, clipping, and fragment shader values are approximations where
hardware execution details do not map exactly to Vulkan invocations. Further
semantic work may be needed outside the tested cases.

Device: duchamp, Mali-G615 MC6, ADB `192.168.1.61:41161`, kbase CSF UAPI
1.21, Alpine chroot `/data/local/tmp/chrootAlpine`. Mesa built with
`/tmp/bld.sh` (`NINJA_RC=0`). ICD SHA-256:
`95dd86770f9762283ae869b1d88eb6eac4879b566c7a7052c34bd3f9f4d0e42e`.

## Focused device test

Source: `tests/dxvk/vulkan/pipeline_stats/`. The harness runs direct, indexed,
instanced, restart, indirect, geometry shader, and compute cases. It verifies
all returned counters and query availability; the CPU only checks the result.

```text
chroot /data/local/tmp/chrootAlpine /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /tmp/pst /tmp/build-glibc/src/panfrost/vulkan/libvulkan_panfrost.so
```

Result: 12 `CASE ... PASS` lines, `PIPELINE_STATS_FAILS=0`, process exit 0,
without `PANVK_DEBUG` flags.

## Full query CTS

The isolated device run used `deqp-vk` from
`vulkan-cts-1.4.6.2-0-gf6a29701220f34dd1407513bfe80d74ca7b392ce`:

```text
/tmp/ctsg.sh pstatsall3 '^dEQP-VK.query_pool.statistics_query'
```

| Outcome | Cases |
|---|---:|
| Pass | 14,098 |
| Fail | 0 |
| NotSupported | 3,671 |
| Total | 17,769 |

Unsupported reasons were inherited queries (2,158), tessellation (1,431),
no matching queue family (78), and `VK_KHR_device_address_commands` (4).
The log and QPA remain on the device at `/tmp/cts-pstatsall3.log` and
`/tmp/cts-pstatsall3.qpa` inside the chroot. No device loss occurred in this
isolated run.

Earlier full runs found the ordering bugs: a run concurrent with other GPU
tests lost the device once, and the next isolated run had eight
`reset_after_copy.compute_shader_invocations.*cmdcopyquerypoolresults*`
failures. A targeted rerun after the copy/reset barrier passed all eight;
the complete isolated rerun above passed. The concurrent-run device loss has
not been separately root-caused.

Tessellation and inherited queries still block those unsupported CTS cases.
DXVK Native D3D11 remains at feature level 10_1 until GPU tessellation is
implemented and validated. Physical display presentation remains unverified.
