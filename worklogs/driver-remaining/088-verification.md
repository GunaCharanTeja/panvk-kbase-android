# Gate 088 Verification Evidence: TES PrimitiveIdIn to Geometry Shaders

## 1. Scope and Verification Boundary
- **Verification Level**: Host C source extraction, lightweight NIR-builder block emulation, and NDK compilation.
- **Physical GPU Proof**: NOT_RUN (no physical device proof claimed; hardware NOT_RUN; active launcher/X11 container sessions preserved on test device `192.168.1.34:40501`).
- **Clean Base Commit**: `1451e2d99ccb144a35fe958b24eaf121fad4288b` (`work/mesa`, clean git commit)
- **Patch Path**: `patches/csf-v11/088-use-tes-primitive-id-in-tess-fed-geometry-shaders.patch`
- **Patch SHA256**: `2d32306c7218fd0ebb05212825abbb1594ec1899ff1dfd4744167fdedae55eb6`
- **Target Worktree**: `work/mesa-tess-primitive-id` (isolated worktree on branch `dx-tess-primitive-id`)
- **Blob Transition**: `86c7ab7847e` -> `8a827377566` (`src/panfrost/vulkan/panvk_vX_shader.c`)
- **Emulation & Hardware Constraints**:
  - Verification uses host lightweight NIR-builder block emulation over production C body.
  - Harness operates on supplied topology and capacity inputs (`vin0 = j - 2`, `vin0 = j - 1`, `inst * n`, `is_valid`, `stride`).
  - Primitive restart loops in non-tessellation path remain untested in host harness (`nir_push_loop` stubbed).
  - No full NIR compiler pipeline or GPU execution evaluated in test harness (no full NIR/GPU).
  - Hardware execution is NOT_RUN.

---

## 2. Review P1 Defect & Resolution

### Defect
In primitive assembly for triangle lists (`MESA_PRIM_TRIANGLES`), invocations `j = 0, 1` calculate vertex index `s.vin[0] = j - 2`. In 32-bit unsigned math, `j - 2` underflows to `0xfffffffe` / `0xffffffff`. While the original GS shader body was deferred inside `nir_push_if(b, valid)`, the tess-fed GS primitive ID load unconditionally ran before that guard. Invocations `j = 0, 1` computed `rec = inst * n + 0xfffffffe`, computing an invalid global address and executing an out-of-bounds `nir_load_global` from `vs_records`.

### Minimal Resolution
1. **Defined Safe Initialization**: `prim_var` is initialized to `zero` prior to `if (is_tess)` (`nir_store_var(b, prim_var, zero, 1)`).
2. **Execution Guard with Validity & Capacity**: Inside `if (is_tess)`, the global record load is enclosed in `nir_push_if(b, valid)`. Invalid invocations (e.g. `j = 0, 1` for triangles) and invocations exceeding capacity (`in_cap` and `vs_cap`, which are already combined into `valid`) skip the global memory load entirely.
3. **Preserved Non-Tess Ordinal Path**: Non-tess draws execute the `else` branch, preserving restart segment counting, loop scanning, and `prim_id_base` offset addition.
4. **Dynamic Production Enum Consumption**: Extracted dynamically from `src/compiler/shader_enums.h`: `VARYING_SLOT_PRIMITIVE_ID` (= 21, not copied placeholder 38). Slot index calculation uses `BITFIELD64_MASK(VARYING_SLOT_PRIMITIVE_ID)`.

---

## 3. Test Verification (`tests/gs-tess-primitive-id.py`)

1. **Source-Extracted Execution via Lightweight NIR-Builder Stubs**:
   - Production block extracted verbatim from `src/panfrost/vulkan/panvk_vX_shader.c`.
   - `nir_builder` emulation maintains execution activation stack (`stack[top]`). C execution of memory loads and variable stores is suppressed for inactive builder branches.
   - Mock GPU memory buffer bounds-checks global addresses (`addr + 4 > gpu_records_size`) to detect MMU faults from invalid address wrapping.
   - Emulation operates purely on supplied topology and capacity inputs; non-tess restart loops are untested (stubbed out); no full NIR or GPU execution.
2. **Baseline Coverage**:
   - Multiple instances: `inst = 0, 1, 2` with instance stride.
   - Invalid triangle invocations: `j = 0, 1` (where `vin0 = j - 2` wraps to `0xfffffffe`, `0xffffffff`) cleanly skip memory loads and return safe defined `0`.
   - Invalid line invocations: `j = 0` (where `vin0 = j - 1` wraps).
   - Capacity rejected invocations: `valid = false` skips loads and returns `0`.
   - Tessellation chunks with non-zero `prim_base`: patch IDs read from `vs_records` without adding `prim_id_base`.
   - Non-tessellation GS: computes `lp + prim_id_base`.
3. **Concrete Mutant Suite (all non-zero exit)**:
   - **Mutant 1 (ignoreoff)**: Omits `slot * 16` offset, reading slot 0 (POS) instead of slot 1 (patch ID). Caught (exit 1).
   - **Mutant 2 (inverseguard comment)**: Comments out `nir_push_if(b, valid);`, executing load on invalid `j = 0` invocation and triggering MMU fault on wrapped `0xfffffffe`. Caught (exit 1).
   - **Mutant 3 (inst*n deletion)**: Removes instance offset `inst * n`. Instance 1 reads instance 0 record. Caught (exit 1).
   - **Mutant 4 (vin0->lp)**: Substitutes `lp` for `vin[0]`. Triangle 1 evaluates record 1 instead of record 3. Caught (exit 1).
   - **Mutant 5 (baseaddition)**: Adds `prim_id_base` to tess patch ID. Caught (exit 1).

---

## 4. Independent Android Object Build Verification
- **Toolchain**: NDK r30 (`30.0.14904198`), `aarch64-linux-android35-clang`
- **Target Arch**: ARM64 Bionic Android API 35 (`PAN_ARCH=11`, Mali-G615 CSF)
- **Worktree**: `work/mesa-tess-primitive-id`

| Object / Binary File | Exit Code | SHA256 Hash | Notes |
|---|---|---|---|
| `libpanvk_v11.a.p/panvk_vX_shader.c.o` | 0 | `5f876997bd3cce799149f887c488702cbf140e8c77b5fff031e65c10b2e839cf` | CSF v11 shader compiler object |
| `libvulkan_panfrost.so` | 0 | `0ae44dff3bab875ad9630c563c0069616f9d0e83ae80c12597a2414960f92fa0` | Full ICD library (dist: `c0010b87a3bdc7ff...`) |

- **Static Binary Validation**:
  `STATIC-VALIDATION: PASS (liblog.so, libnativewindow.so, libsync.so, libm.so, libz.so, libdl.so, libc.so; exports vk_icdGetInstanceProcAddr, vk_icdNegotiateLoaderICDInterfaceVersion)`

---

## 5. Verification Execution Log
```
$ python3 tests/gs-tess-primitive-id.py
=== Gate 088 Verification (Python Runner) ===
Mesa source directory: work/mesa-tess-primitive-id
PASS: Dynamically extracted production VARYING_SLOT_PRIMITIVE_ID = 21
PASS: Baseline compiled & executed: instances (0,1,2), invalid triangle/line invocations (j=0,1), capacity-rejected, chunks with prim_base
PASS: Mutant 1 (ignoreoff) caught (exit 1)
PASS: Mutant 2 (inverseguard comment) caught (exit 1)
PASS: Mutant 3 (inst*n deletion) caught (exit 1)
PASS: Mutant 4 (vin0->lp) caught (exit 1)
PASS: Mutant 5 (baseaddition) caught (exit 1)
ALL CHECKS PASSED: Gate 088 verified.
```
