#!/usr/bin/env python3
"""
Gate 088: GS PrimitiveIdIn with tessellation verification test.
Extracts production source body from Mesa worktree:
- panvk_gpu_prerast_lower_gs (src/panfrost/vulkan/panvk_vX_shader.c)
- Dynamic production enum extraction from src/compiler/shader_enums.h

Compiles lightweight NIR-builder emulation harness with GPU memory bounds checking.
Executes baseline:
- Multiple instances (inst=0, 1, 2)
- Chunks including non-zero primitive base
- Invalid triangle and line invocations (j=0, 1 wrapping vin0 underflow)
- Capacity rejected invocations (in_cap and vs_cap)
- Non-tessellation GS ordinal computation with base addition

Runs actual source mutants against the extracted C production code:
- Mutant 1: ignoreoff (omits slot*16 offset)
- Mutant 2: inverseguard comment (comments out if (valid) guard, triggers MMU fault)
- Mutant 3: inst*n deletion (removes instance stride)
- Mutant 4: vin0->lp (substitutes lp for vin[0])
- Mutant 5: baseaddition (erroneously adds prim_id_base to tess patch ID)
"""
import os, sys, subprocess, re

def find_mesa_dir(hint=None):
    candidates = [
        hint,
        "work/mesa-tess-primitive-id",
        "/home/abhaybyte/repos/panvk/work/mesa-tess-primitive-id",
        "../work/mesa-tess-primitive-id",
        "work/mesa-wsi",
        "work/mesa",
    ]
    for c in candidates:
        if c and os.path.isdir(c):
            return c
    raise RuntimeError("Cannot find Mesa source directory")

def extract_production_slot(enums_path):
    with open(enums_path) as f:
        src = f.read()
    pos = src.find("VARYING_SLOT_POS,")
    assert pos != -1, "VARYING_SLOT_POS not found in shader_enums.h"
    val = 0
    slot_prim_id = None
    for token in src[pos:].split(","):
        t = token.strip()
        if "/*" in t:
            t = re.sub(r"/\*.*?\*/", "", t, flags=re.DOTALL).strip()
        if not t:
            continue
        if "VARYING_SLOT_VAR0" in t:
            val = 32
        if "VARYING_SLOT_PRIMITIVE_ID" in t:
            slot_prim_id = val
            break
        val += 1
    assert slot_prim_id is not None, "VARYING_SLOT_PRIMITIVE_ID not found in shader_enums.h"
    return slot_prim_id

def run_c_test(extracted_code, slot_prim_id, tmp_prefix="/tmp/panvk_088"):
    c_src = f"""#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define BITFIELD64_BIT(b) (1ULL << (b))
#define BITFIELD64_MASK(n) (((n) >= 64) ? ~0ULL : ((1ULL << (n)) - 1ULL))

#define VARYING_SLOT_PRIMITIVE_ID {slot_prim_id}
#define VARYING_BIT_PRIMITIVE_ID BITFIELD64_BIT(VARYING_SLOT_PRIMITIVE_ID)

typedef struct {{
   uint64_t v;
}} nir_def;

typedef struct {{
   uint64_t v;
}} nir_variable;

typedef struct nir_builder {{
   bool stack[16];
   int top;
}} nir_builder;

typedef void nir_if;
typedef void nir_function_impl;
typedef void* glsl_type;

static bool builder_active(nir_builder *b) {{
   for (int i = 0; i <= b->top; i++) {{
      if (!b->stack[i]) return false;
   }}
   return true;
}}

static nir_if *nir_push_if(nir_builder *b, nir_def *cond) {{
   assert(b->top < 15);
   b->top++;
   b->stack[b->top] = (cond->v != 0);
   return NULL;
}}

static void nir_push_else(nir_builder *b, nir_if *nif) {{
   assert(b->top >= 1);
   b->stack[b->top] = !b->stack[b->top];
}}

static void nir_pop_if(nir_builder *b, nir_if *nif) {{
   assert(b->top >= 1);
   b->top--;
}}

static nir_def def_pool[256];
static int def_idx = 0;
static nir_def *mk_def(uint64_t v) {{
   assert(def_idx < 256);
   nir_def *d = &def_pool[def_idx++];
   d->v = v;
   return d;
}}

static nir_variable var_pool[32];
static int var_idx = 0;
static nir_variable *nir_local_variable_create(nir_function_impl *impl, glsl_type t, const char *name) {{
   assert(var_idx < 32);
   nir_variable *v = &var_pool[var_idx++];
   v->v = 0;
   return v;
}}

static void nir_store_var(nir_builder *b, nir_variable *var, nir_def *val, unsigned mask) {{
   if (builder_active(b)) {{
      var->v = val->v;
   }}
}}

static nir_def *nir_load_var(nir_builder *b, nir_variable *var) {{
   return mk_def(var->v);
}}

static nir_def *nir_ine_imm(nir_builder *b, nir_def *x, uint64_t imm) {{
   return mk_def(x->v != imm);
}}

static nir_def *nir_inot(nir_builder *b, nir_def *x) {{
   return mk_def(!x->v);
}}

static nir_def *nir_iand(nir_builder *b, nir_def *x, nir_def *y) {{
   return mk_def(x->v & y->v);
}}

static nir_def *nir_imm_int64(nir_builder *b, uint64_t v) {{
   return mk_def(v);
}}

static nir_def *nir_imm_int(nir_builder *b, uint32_t v) {{
   return mk_def(v);
}}

static nir_def *nir_bit_count(nir_builder *b, nir_def *x) {{
   return mk_def(__builtin_popcountll(x->v));
}}

static nir_def *nir_imul(nir_builder *b, nir_def *x, nir_def *y) {{
   return mk_def((uint32_t)(x->v * y->v));
}}

static nir_def *nir_imul_imm(nir_builder *b, nir_def *x, uint32_t imm) {{
   return mk_def((uint32_t)(x->v * imm));
}}

static nir_def *nir_iadd(nir_builder *b, nir_def *x, nir_def *y) {{
   return mk_def((uint32_t)(x->v + y->v));
}}

static nir_def *nir_iadd_imm(nir_builder *b, nir_def *x, int32_t imm) {{
   return mk_def((uint32_t)(x->v + imm));
}}

static nir_def *nir_u2u64(nir_builder *b, nir_def *x) {{
   return mk_def((uint64_t)(uint32_t)x->v);
}}

static uint8_t *gpu_records_mem = NULL;
static size_t gpu_records_size = 0;
static bool mmu_fault_triggered = false;

static nir_def *do_load_global(nir_builder *b, unsigned comps, unsigned bits, nir_def *addr) {{
   if (!builder_active(b)) {{
      return mk_def(0);
   }}
   if (addr->v + 4 > gpu_records_size) {{
      mmu_fault_triggered = true;
      return mk_def(0xdeadbeef);
   }}
   uint32_t val = *(uint32_t *)(gpu_records_mem + addr->v);
   return mk_def(val);
}}
#define nir_load_global(b, comps, bits, addr, ...) do_load_global(b, comps, bits, addr)

static uint32_t param_prim_id_base = 0;
#define GS_PARAM(b, p, bits, field) mk_def(param_##field)
#define glsl_uint_type() NULL
#define nir_push_loop(b) if (0)
#define nir_pop_loop(b, ...) ((void)0)
#define nir_break_if(b, cond) ((void)0)
#define panvk_gs_is_restart(b, restart_en, gen, qq) mk_def(0)
#define panvk_gs_prims_in_run(b, prim, l) mk_def(0)

struct lower_state {{
   nir_def *prim_id;
   nir_def *vs_outputs;
   nir_def *inst;
   nir_def *n;
   nir_def *vin[6];
   nir_def *vs_stride;
   nir_def *vs_records;
}};

static uint32_t run_invocation(uint64_t outputs, uint32_t inst, uint32_t n, uint32_t vin0,
                               uint32_t stride, uint32_t lp_val, bool is_valid, uint32_t base) {{
   def_idx = 0;
   var_idx = 0;
   nir_builder bld = {{ .stack = {{ true }}, .top = 0 }};
   nir_builder *b = &bld;
   nir_function_impl *impl = NULL;

   param_prim_id_base = base;
   nir_def *zero = mk_def(0);
   nir_def *valid = mk_def(is_valid ? 1 : 0);
   nir_def *lp = mk_def(lp_val);
   bool reads_prim_id = true;

   nir_def *restart_en = mk_def(0);
   nir_def *seg = mk_def(0);
   nir_def *gen = mk_def(0);
   nir_def *prim = mk_def(0);
   nir_def *p = mk_def(0);

   struct lower_state s = {{
      .prim_id = lp,
      .vs_outputs = mk_def(outputs),
      .inst = mk_def(inst),
      .n = mk_def(n),
      .vin = {{ mk_def(vin0) }},
      .vs_stride = mk_def(stride),
      .vs_records = mk_def(0),
   }};

   {extracted_code}

   return (uint32_t)s.prim_id->v;
}}

int main() {{
   const uint32_t n = 12;
   const uint32_t instances = 3;
   const uint32_t total_verts = n * instances;
   const uint64_t tess_outputs = BITFIELD64_BIT(0) | BITFIELD64_BIT(VARYING_SLOT_PRIMITIVE_ID) | BITFIELD64_BIT(32);
   const uint32_t nout = __builtin_popcountll(tess_outputs);
   const uint32_t stride = nout * 16;
   gpu_records_size = total_verts * stride;
   gpu_records_mem = calloc(1, gpu_records_size);
   assert(gpu_records_mem);

   /* Populate mock tessellation records across instances and patches */
   for (uint32_t r = 0; r < total_verts; r++) {{
      *(uint32_t *)(gpu_records_mem + r * stride + 0) = 7777; /* POS */
      uint32_t patch = 40 + r / 3; /* chunk with prim_base = 40 */
      *(uint32_t *)(gpu_records_mem + r * stride + 16) = patch; /* PRIMITIVE_ID */
   }}

   /* 1. Invalid triangle invocations j=0, 1 (vin0 = j-2 wraps to 0xfffffffe, 0xffffffff). Must NOT fault! */
   mmu_fault_triggered = false;
   uint32_t j0_res = run_invocation(tess_outputs, 0, n, 0xfffffffe, stride, 0, false, 1000);
   if (mmu_fault_triggered) {{
      fprintf(stderr, "FAIL: MMU fault on j=0 invalid triangle invocation!\\n");
      return 1;
   }}
   if (j0_res != 0) {{
      fprintf(stderr, "FAIL: expected safe 0 on invalid j=0, got %u\\n", j0_res);
      return 1;
   }}

   uint32_t j1_res = run_invocation(tess_outputs, 0, n, 0xffffffff, stride, 0, false, 1000);
   if (mmu_fault_triggered || j1_res != 0) {{
      fprintf(stderr, "FAIL: MMU fault or nonzero on j=1 invalid invocation!\\n");
      return 1;
   }}

   /* 2. Invalid line invocation j=0 (vin0 = j-1 wraps to 0xffffffff). Must NOT fault! */
   uint32_t line_inv = run_invocation(tess_outputs, 0, n, 0xffffffff, stride, 0, false, 1000);
   if (mmu_fault_triggered || line_inv != 0) {{
      fprintf(stderr, "FAIL: MMU fault on invalid line invocation!\\n");
      return 1;
   }}

   /* 3. Valid triangle invocations across multiple instances and chunks with non-zero prim_base */
   /* Instance 0: Triangle 0 (vin0=0, lp=0) -> patch 40 (TD(gs_prim_base)=1000 must NOT be added) */
   uint32_t t0 = run_invocation(tess_outputs, 0, n, 0, stride, 0, true, 1000);
   if (t0 != 40) {{
      fprintf(stderr, "FAIL: expected patch 40 on t0, got %u\\n", t0);
      return 1;
   }}

   /* Instance 0: Triangle 1 (vin0=3, lp=1) -> patch 41 */
   uint32_t t1 = run_invocation(tess_outputs, 0, n, 3, stride, 1, true, 1000);
   if (t1 != 41) {{
      fprintf(stderr, "FAIL: expected patch 41 on t1, got %u\\n", t1);
      return 1;
   }}

   /* Instance 1: Triangle 0 (vin0=0, lp=0, inst=1 -> rec=12) -> patch 44 */
   uint32_t t_inst1 = run_invocation(tess_outputs, 1, n, 0, stride, 0, true, 1000);
   if (t_inst1 != 44) {{
      fprintf(stderr, "FAIL: expected patch 44 on inst1 t0, got %u\\n", t_inst1);
      return 1;
   }}

   /* Instance 2: Triangle 1 (vin0=3, lp=1, inst=2 -> rec=27) -> patch 49 */
   uint32_t t_inst2 = run_invocation(tess_outputs, 2, n, 3, stride, 1, true, 1000);
   if (t_inst2 != 49) {{
      fprintf(stderr, "FAIL: expected patch 49 on inst2 t1, got %u\\n", t_inst2);
      return 1;
   }}

   /* 4. Capacity rejected invocation: valid=false -> load suppressed, returns safe defined 0 */
   uint32_t t_cap = run_invocation(tess_outputs, 1, n, 0, stride, 0, false, 1000);
   if (mmu_fault_triggered || t_cap != 0) {{
      fprintf(stderr, "FAIL: expected safe 0 on capacity-rejected invocation!\\n");
      return 1;
   }}

   /* 5. Non-tessellation GS: outputs do NOT have VARYING_BIT_PRIMITIVE_ID */
   const uint64_t nontess_outputs = BITFIELD64_BIT(0) | BITFIELD64_BIT(32);
   uint32_t nontess = run_invocation(nontess_outputs, 0, n, 0, 32, 7, true, 500);
   if (nontess != 507) {{
      fprintf(stderr, "FAIL: expected 507 on non-tess GS (lp=7 + base=500), got %u\\n", nontess);
      return 1;
   }}

   return 0;
}}
"""
    c_file = f"{tmp_prefix}.c"
    bin_file = f"{tmp_prefix}.bin"
    with open(c_file, "w") as f:
        f.write(c_src)
    subprocess.check_call(["gcc", "-Wall", "-Werror", "-Wno-unused-function", "-Wno-misleading-indentation", "-Wno-unused-variable", c_file, "-o", bin_file])
    res = subprocess.run([bin_file], capture_output=True, text=True)
    return res.returncode, res.stdout, res.stderr

def main():
    mesa_dir = find_mesa_dir(sys.argv[1] if len(sys.argv) > 1 else None)
    print("=== Gate 088 Verification (Python Runner) ===")
    print(f"Mesa source directory: {mesa_dir}")

    enums_path = os.path.join(mesa_dir, "src/compiler/shader_enums.h")
    shader_path = os.path.join(mesa_dir, "src/panfrost/vulkan/panvk_vX_shader.c")

    slot_prim_id = extract_production_slot(enums_path)
    print(f"PASS: Dynamically extracted production VARYING_SLOT_PRIMITIVE_ID = {slot_prim_id}")

    with open(shader_path) as f:
        src = f.read()

    m = re.search(r"(s\.prim_id\s*=\s*lp;.*?s\.prim_id\s*=\s*nir_load_var\(b,\s*prim_var\);\s*\})", src, re.DOTALL)
    assert m, "Extracted GS primitive ID handling block not found in panvk_vX_shader.c"
    code = m.group(1)

    # 1. Baseline Execution
    rc, out, err = run_c_test(code, slot_prim_id, "/tmp/panvk_088_baseline")
    assert rc == 0, f"Baseline failed (rc={rc}): {err}"
    print("PASS: Baseline compiled & executed: instances (0,1,2), invalid triangle/line invocations (j=0,1), capacity-rejected, chunks with prim_base")

    # 2. Mutant 1: ignoreoff (omits slot*16 offset, reads slot 0 POS instead of slot 1 patch ID)
    m1_code = code.replace("nir_imul_imm(b, slot, 16)", "zero")
    rc1, _, _ = run_c_test(m1_code, slot_prim_id, "/tmp/panvk_088_m1")
    assert rc1 != 0, "Mutant 1 (ignoreoff) did not fail!"
    print(f"PASS: Mutant 1 (ignoreoff) caught (exit {rc1})")

    # 3. Mutant 2: inverseguard comment (comments out if (valid) guard, causing MMU fault on j=0)
    m2_code = code.replace("nir_push_if(b, valid);", "// nir_push_if(b, valid);").replace("nir_pop_if(b, NULL);", "// nir_pop_if(b, NULL);", 1)
    rc2, _, _ = run_c_test(m2_code, slot_prim_id, "/tmp/panvk_088_m2")
    assert rc2 != 0, "Mutant 2 (inverseguard comment) did not fail!"
    print(f"PASS: Mutant 2 (inverseguard comment) caught (exit {rc2})")

    # 4. Mutant 3: inst*n deletion (removes instance offset)
    m3_code = code.replace("nir_iadd(b, nir_imul(b, s.inst, s.n), s.vin[0])", "s.vin[0]")
    rc3, _, _ = run_c_test(m3_code, slot_prim_id, "/tmp/panvk_088_m3")
    assert rc3 != 0, "Mutant 3 (inst*n deletion) did not fail!"
    print(f"PASS: Mutant 3 (inst*n deletion) caught (exit {rc3})")

    # 5. Mutant 4: vin0->lp (substitutes lp for vin[0])
    m4_code = code.replace("s.vin[0]", "lp")
    rc4, _, _ = run_c_test(m4_code, slot_prim_id, "/tmp/panvk_088_m4")
    assert rc4 != 0, "Mutant 4 (vin0->lp) did not fail!"
    print(f"PASS: Mutant 4 (vin0->lp) caught (exit {rc4})")

    # 6. Mutant 5: baseaddition (erroneously adds prim_id_base to tess patch ID)
    m5_code = code.replace(
        "nir_store_var(b, prim_var, patch_id, 1);",
        "nir_store_var(b, prim_var, nir_iadd(b, patch_id, GS_PARAM(b, p, 32, prim_id_base)), 1);"
    )
    rc5, _, _ = run_c_test(m5_code, slot_prim_id, "/tmp/panvk_088_m5")
    assert rc5 != 0, "Mutant 5 (baseaddition) did not fail!"
    print(f"PASS: Mutant 5 (baseaddition) caught (exit {rc5})")

    print("ALL CHECKS PASSED: Gate 088 verified.")

if __name__ == "__main__":
    main()
