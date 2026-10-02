# Gate 087 Verification Evidence: Tessellation Conditional Rendering

## 1. Source and Patch Provenance
- **Clean Base Commit**: `1451e2d99ccb144a35fe958b24eaf121fad4288b` (`work/mesa`, clean git commit)
- **Patch Path**: `patches/csf-v11/087-honour-conditional-rendering-in-the-tessellation-compute-loop.patch`
- **Patch SHA256**: `8a3972d65f8aa398ed1c72b9b7e7a6b413d73e84ced0d77906eae4e1031c9617`
- **Blob Transition**: `7b40ea4fd06` -> `d6bcf58eb02` (`src/panfrost/vulkan/csf/panvk_vX_cmd_draw.c`)
- **Source Worktree**: `work/mesa-tess-predicate`
- **Validation of Local Changes**:
  - **Lines 4594-4601 (Immutable direct source)**: `direct_draw` dev mem allocated (5 words: count, instance count, offset/base). GPU reads from `direct_draw.gpu` fresh on every execution, preventing first-iteration zeroing of `td->in_draw` from destroying recorded draw parameters on command buffer replay or simultaneous use.
  - **Lines 4748-4757 (Execution-time flag reset)**: CS instructions emit store of predicate `pass` to `pass_mem.gpu + 0` and reset `once_reset = 0` to `pass_mem.gpu + 4` before `panlib_tess_setup()`. Guarantees every submission/replay evaluates the once-flag fresh instead of retaining stale `once = 1`.

---

## 2. Test Verification (`tests/tess-cond-render-predicate.py`)

1. **Source-Body Extraction & Serial Synchronous Emulation (No Handwritten Replicas)**:
   - Extracted verbatim from actual Mesa sources:
     - `gpu_prerast_cond_render_pass`: from `src/panfrost/vulkan/csf/panvk_vX_cmd_draw.c`
     - `struct panlib_tess_draw`: from `src/panfrost/libpan/tess_draw.h`
     - `panlib_tess_setup`, `panlib_tess_step`, `panlib_tess_draw`: from `src/panfrost/libpan/draw_helper.cl`
     - `start_exec` (predicate evaluation + `once_reset` store): from `panvk_vX_cmd_draw.c:4746-4757`
     - `guard` (compute first-iteration zeroing): from `panvk_vX_cmd_draw.c:4771-4809`
   - **Emulation Scope**: Explicitly serial synchronous sourcebody emulation on host. No ready/free simulation, concurrent execution, or physical device proof.
2. **Modeled Tessellator Counters & Grid Verification**:
   - Asserts TCS invocations (`tcs == (expected ? patches : 0)`).
   - Asserts pipeline statistics & generated queries (`clip == tcs && prims == tcs && raster == tcs`) using modeled tessellator counters.
   - Asserts total shader invocations (`shader_inv == (expected ? 2 * patches : 0)` from `vs_draw[0]*vs_draw[1] + tcs_grid[0]*tcs_grid[1]`).
   - Asserts zero grids when skipped (`vs_draw == 0`, `tcs_grid == 0`, `nr_patches == 0`).
3. **Synchronous Schedule & Arena Tracking**:
   - Asserts `gpu_prerast_acquire_arena` and `gpu_prerast_release_arena` synchronous acquire/release pairing.
   - Asserts skipped draws execute exactly 1 empty-handshake iteration (`iters == 1`), cleanly releasing the arena and synchronization tokens without overclaiming hardware.
4. **Repetition Test Suite**:
   - 1,000 non-inverted replay executions (alternating predicate 7 and 0)
   - 1,000 inverted replay executions (`MALI_CS_CONDITION_EQUAL`: 0 executes, non-zero skips)
   - 1,000 inherited secondary executions (`cond_render_flag`)
   - 3,000 total executions on shared allocations, verifying direct-draw source immutability.
5. **Concrete Mutant Suite**:
   - **Mutant 1 (Source mutation `cs_move64_to(b, z64, 0)` -> `1`)**: Setting `total_patches = 1` while `patches_per_instance = 0` causes `panlib_tess_step` division by zero (`next_patch / ppi`). Caught via SIGFPE (status 136 / exit -8).
   - **Mutant 2 (Reset deletion)**: Omission of `cs_store32(b, once_reset, pass_addr, 4)` leaves stale `once = 1` on replay. Skipped draw erroneously runs. Caught on replay (status 134 / exit -6).
   - **Mutant 3 (Direct draw alias `src = t.in_draw`)**: First skipped draw destroys recorded parameters. Replay draw lost. Caught on replay (status 134 / exit -6).

---

## 3. Independent Android Object Build Verification
- **Toolchain**: NDK r30 (`30.0.14904198`), `aarch64-linux-android35-clang`
- **Build Output Directory**: `/tmp/panvk-087`
- **Target Arch**: ARM64 Bionic Android API 35 (`PAN_ARCH=11`)
- **Missing `c99_compat.h` Resolution**: `/tmp/panvk-087/clc.log` resolved by including Mesa `include/` path with generated `genxml` headers to `mesa_clc` and `panfrost_compile`.

| Object File | Exit Code | SHA256 Hash | Symbol / Header Evidence |
|---|---|---|---|
| `csf_panvk_vX_cmd_draw.c.o` | 0 | `2da03567b5a3f3e1790391021751e037510245d6dd2a291d526fa24eb9e27d5d` | `<gpu_prerast_cond_render_pass>` emitted, direct_draw & once_reset compiled |
| `libpan_shaders_v11.h` | 0 | `15df998076cc1c7afabc171cabade0fd12afb0581d93c1244a58902ef9fb1d4d` | Generated via `mesa_clc` + `panfrost_compile` |

---

## 4. Device Inspection and Blockers
- **Hardware**: NOT_RUN (no disruptive CTS run)
- **Device**: `192.168.1.34:40501` (`2311DRK48I`, Dimensity 8300 Ultra `mt6897`, Mali-G615 MC6)
- **Active Processes**: `dev.zenithblue.panvklauncher` (PID 11731), `dev.zenithblue.panvktest` (PID 15719), `termux-x11` (PID 19323), `com.termux.x11` (PID 28471)
- **Blocker**: Active launcher and X11 container sessions running. Nondisruptive inspection preserved launcher and running workloads untouched.

---

## 5. Verification Execution Log

```
$ python3 tests/tess-cond-render-predicate.py
=== Gate 087 Verification (Python Runner) ===
Mesa source directory: work/mesa-tess-predicate
PASS: Baseline compiled & executed 3,000 replays cleanly
PASS: Grids & counters verified (tcs, clip, prims, raster, shader_inv)
PASS: Handshake schedule & arena ownership verified (single-iter handshake on skip)
PASS: Mutant 1 (cs_move64_to 0->1) caught (exit -8)
PASS: Mutant 2 (reset deletion) caught on replay (exit -6)
PASS: Mutant 3 (direct draw alias) caught on replay (exit -6)
ALL CHECKS PASSED: Gate 087 verified.
```
