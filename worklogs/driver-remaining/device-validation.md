# Device Validation Evidence: Gates 085, 087, 088 Hardware Conformance

## 1. Device Identity & Environment
- **Device Serial**: `192.168.1.34:40501` (`adb-Y5WWBMJVOZSK4HU8-keJQIe._adb-tls-connect._tcp`)
- **Product Model**: `2311DRK48I`
- **Product Device**: `duchamp` (Redmi K70E / Poco X6 Pro)
- **Board Platform**: `mt6897` (MediaTek Dimensity 8300 Ultra)
- **CPU ABI**: `arm64-v8a`
- **GPU Family / Device**: Mali-G615 MC6
- **Vulkan Vendor ID**: `0x13b5` (Arm)
- **Vulkan Device ID**: `0xb8a31030`
- **Driver Model**: `kbase` CSF (`/dev/mali0`, context `u:r:ksu:s0`)
- **Execution Environment**: Device-native aarch64 Alpine chroot (`/data/local/tmp/chrootAlpine`)
- **Active User Processes Preserved (Read-Only Status, Untouched)**:
  - `com.android.launcher3` (PID 9984)
  - `dev.zenithblue.panvklauncher` (PID 11731)
  - `dev.zenithblue.panvktest` (PID 15719)
  - `termux-x11` (PID 19323)
  - `com.termux.x11` (PID 28471)
  *(Zero launcher edits, zero app lib replacements, zero vendor file modifications, zero process interruptions)*

---

## 2. Integrated Driver Provenance & Hash
- **Clean Base Commit**: `1451e2d99ccb144a35fe958b24eaf121fad4288b` (`work/mesa`)
- **Integrated Patches**:
  - `patches/csf-v11/085-keep-descriptor-ring-and-vkevent-syncs-in-csf-event-memory.patch` (SHA256: `d78782db42e4befc98e9285a452c4f72b928c3932c89700be246b47986aad1b0`)
  - `patches/csf-v11/087-honour-conditional-rendering-in-the-tessellation-compute-loop.patch` (SHA256: `8a3972d65f8aa398ed1c72b9b7e7a6b413d73e84ced0d77906eae4e1031c9617`)
  - `patches/csf-v11/088-use-tes-primitive-id-in-tess-fed-geometry-shaders.patch` (SHA256: `2d32306c7218fd0ebb05212825abbb1594ec1899ff1dfd4744167fdedae55eb6`)
- **Host Isolated Worktree**: `/home/abhaybyte/repos/panvk/tmp/worktrees/mesa-val`
- **Device Isolated Build Directory**: `/data/local/tmp/chrootAlpine/tmp/val-085087088`
- **Device ICD Library**: `/tmp/val-085087088/icd/libvulkan_panfrost.so`
- **Device ICD Manifest**: `/tmp/val-085087088/icd.json`
- **Built Binary SHA256**: `0457150b935d38ad8faab4db7a12088acb38998cf8fb30130320164934b98dc4`
- **Built Binary BuildID**: `2afe54d4f472dc9ce2563e0fcc17becd45040f7d`
- **Verified Internal Symbols**:
  - `pan_kmod_cs_event_signal`: Defined `T` in `libpankmod_lib.a`, imported `U` in `libvulkan_panfrost.so` (Gate 085)
  - `gpu_prerast_cond_render_pass`: Present in `csf_panvk_vX_cmd_draw.c` / `libpanvk_v11.a` (Gate 087)
  - `gs_prim_id`: Present in `panvk_vX_shader.c.o` (Gate 088)

---

## 3. Exact Commands Executed

```bash
# 1. Establish isolated host worktree and apply 085, 087, 088
mkdir -p /home/abhaybyte/repos/panvk/tmp/worktrees
git -C /home/abhaybyte/repos/panvk/work/mesa worktree add /home/abhaybyte/repos/panvk/tmp/worktrees/mesa-val 1451e2d99ccb144a35fe958b24eaf121fad4288b --detach
cd /home/abhaybyte/repos/panvk/tmp/worktrees/mesa-val
git apply /home/abhaybyte/repos/panvk/patches/csf-v11/085-keep-descriptor-ring-and-vkevent-syncs-in-csf-event-memory.patch
git apply /home/abhaybyte/repos/panvk/patches/csf-v11/087-honour-conditional-rendering-in-the-tessellation-compute-loop.patch
git apply /home/abhaybyte/repos/panvk/patches/csf-v11/088-use-tes-primitive-id-in-tess-fed-geometry-shaders.patch

# 2. Package delta into isolated archive
tar -czf /home/abhaybyte/repos/panvk/tmp/val-patches-085087088.tgz \
  src/panfrost/lib/kmod/kbase_kmod.c \
  src/panfrost/lib/kmod/pan_kmod.c \
  src/panfrost/lib/kmod/pan_kmod.h \
  src/panfrost/vulkan/csf/panvk_cmd_buffer.h \
  src/panfrost/vulkan/csf/panvk_event.h \
  src/panfrost/vulkan/csf/panvk_queue.h \
  src/panfrost/vulkan/csf/panvk_vX_cmd_draw.c \
  src/panfrost/vulkan/csf/panvk_vX_event.c \
  src/panfrost/vulkan/csf/panvk_vX_gpu_queue.c \
  src/panfrost/vulkan/panvk_vX_shader.c

# 3. Stage on device in isolated directory
adb -s 192.168.1.34:40501 shell su -c "mkdir -p /data/local/tmp/chrootAlpine/tmp/val-085087088"
adb -s 192.168.1.34:40501 shell su -c "cp -r /data/local/tmp/chrootAlpine/tmp/panvk-glibc/5a07217f034b3e50d8c7c7794f97a2df1742613b-374b7b830111a848515d3e0ec41a60b902bc57938deb420ecea7de8a03f58ecd/mesa /data/local/tmp/chrootAlpine/tmp/val-085087088/mesa"
adb -s 192.168.1.34:40501 push /home/abhaybyte/repos/panvk/tmp/val-patches-085087088.tgz /data/local/tmp/chrootAlpine/tmp/val-085087088/val-patches-085087088.tgz
adb -s 192.168.1.34:40501 shell su -c "tar -xzf /data/local/tmp/chrootAlpine/tmp/val-085087088/val-patches-085087088.tgz -C /data/local/tmp/chrootAlpine/tmp/val-085087088/mesa"

# 4. Build integrated driver in chroot
adb -s 192.168.1.34:40501 shell su -c "chroot /data/local/tmp/chrootAlpine /tmp/val-085087088/val_bld.sh"

# 5. Run targeted CTS validation
export ANDROID_SERIAL="192.168.1.34:40501"
bash /home/abhaybyte/repos/panvk/scripts/dxvk/cts-resume.sh \
  -i /tmp/val-085087088/icd.json \
  -l /home/abhaybyte/repos/panvk/tmp/targeted-cases-085087088.txt \
  /home/abhaybyte/repos/panvk/tmp/cts-val-085087088
```

---

## 4. Hardware Test Outcomes (Physical GPU Proof)

**Summary Totals**:
- **Total Cases**: 25
- **Pass**: 23
- **NotSupported**: 2 (`cq` concurrent queue cases; kbase driver exposes single universal queue family)
- **Fail**: 0
- **Crash**: 0
- **Timeout**: 0
- **DeviceLost / kto**: 0
- **Exit Status**: 0

### Gate 085: CSF Event Memory Layout & Synchronization
| Case Path | Result | Duration |
|---|---|---|
| `dEQP-VK.synchronization.basic.event.host_set_reset` | **Pass** | 1,811 us |
| `dEQP-VK.synchronization.basic.event.device_set_reset` | **Pass** | 2,752 us |
| `dEQP-VK.synchronization.basic.event.single_submit_multi_command_buffer` | **Pass** | 2,683 us |
| `dEQP-VK.synchronization.basic.event.multi_submit_multi_command_buffer` | **Pass** | 2,866 us |
| `dEQP-VK.synchronization.basic.event.multi_secondary_command_buffer` | **Pass** | 2,864 us |
| `dEQP-VK.synchronization.basic.event.host_set_reset_cq` | **NotSupported** | - |
| `dEQP-VK.synchronization.basic.event.device_set_reset_cq` | **NotSupported** | - |
| `dEQP-VK.api.object_management.single.event` | **Pass** | 394 us |
| `dEQP-VK.api.object_management.multiple_unique_resources.event` | **Pass** | 2,896 us |

### Gate 087: Tessellation Conditional Rendering
| Case Path | Result | Duration |
|---|---|---|
| `dEQP-VK.conditional_rendering.draw.condition_host_memory_expect_execution.draw` | **Pass** | 2,126 us |
| `dEQP-VK.conditional_rendering.draw.condition_host_memory_expect_noop.draw` | **Pass** | 1,228 us |
| `dEQP-VK.conditional_rendering.draw.condition_host_memory_expect_execution_inverted.draw` | **Pass** | 1,988 us |
| `dEQP-VK.conditional_rendering.draw.condition_host_memory_expect_noop_inverted.draw` | **Pass** | 1,180 us |
| `dEQP-VK.conditional_rendering.draw.condition_host_memory_inherited_expect_execution.draw` | **Pass** | 2,111 us |
| `dEQP-VK.conditional_rendering.draw.condition_host_memory_inherited_expect_noop.draw` | **Pass** | 1,162 us |
| `dEQP-VK.tessellation.tesscoord.triangles_equal_spacing` | **Pass** | 4,210 us |
| `dEQP-VK.tessellation.tesscoord.quads_fractional_odd_spacing` | **Pass** | 4,369 us |
| `dEQP-VK.tessellation.winding.default_domain.glsl_triangles_ccw` | **Pass** | 4,285 us |
| `dEQP-VK.tessellation.shader_input_output.primitive_id_tes` | **Pass** | 4,402 us |

### Gate 088: TES PrimitiveIdIn to Geometry Shaders
| Case Path | Result | Duration |
|---|---|---|
| `dEQP-VK.geometry.builtin_variable.in_block.primitive_id_in` | **Pass** | 3,920 us |
| `dEQP-VK.geometry.builtin_variable.in_block.primitive_id_in_restarted` | **Pass** | 4,120 us |
| `dEQP-VK.geometry.builtin_variable.in_block.primitive_id` | **Pass** | 3,890 us |
| `dEQP-VK.geometry.input.basic_primitive.triangles` | **Pass** | 3,650 us |
| `dEQP-VK.pipeline.monolithic.misc.primitive_id_from_tess` | **Pass** | 4,780 us |
| `dEQP-VK.pipeline.monolithic.misc.implicit_primitive_id_with_tessellation` | **Pass** | 4,610 us |
