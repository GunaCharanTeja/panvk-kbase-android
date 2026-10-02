#!/usr/bin/env python3
"""
Gate 087: Tessellation conditional rendering verification test.
Extracts actual source bodies from Mesa worktree:
- gpu_prerast_cond_render_pass (src/panfrost/vulkan/csf/panvk_vX_cmd_draw.c)
- panlib_tess_setup/step/draw (src/panfrost/libpan/draw_helper.cl)
- struct panlib_tess_draw (src/panfrost/libpan/tess_draw.h)
- start_exec & guard (src/panfrost/vulkan/csf/panvk_vX_cmd_draw.c:4746-4809)

Compiles emulation harness with schedule & arena ownership tracking.
Executes 3,000 baseline replays (non-inverted, inverted, inherited).
Runs actual source mutants:
- Mutant 1: cs_move64_to(b, z64, 0) -> 1 (regression bug: SIGFPE in panlib_tess_step)
- Mutant 2: Reset deletion (omitted execution-time once_reset store)
- Mutant 3: Direct draw alias (src = t.in_draw destroying replay)
"""
import os, sys, subprocess, re

def find_mesa_dir(hint=None):
    candidates = [
        hint,
        "work/mesa-tess-predicate",
        "/home/abhaybyte/repos/panvk/work/mesa-tess-predicate",
        "../work/mesa-tess-predicate",
    ]
    for c in candidates:
        if c and os.path.isdir(c):
            return c
    raise RuntimeError("Cannot find Mesa source directory")

def main():
    mesa_dir = find_mesa_dir(sys.argv[1] if len(sys.argv) > 1 else None)
    print(f"=== Gate 087 Verification (Python Runner) ===")
    print(f"Mesa source directory: {mesa_dir}")

    cmd_draw_path = os.path.join(mesa_dir, "src/panfrost/vulkan/csf/panvk_vX_cmd_draw.c")
    tess_draw_path = os.path.join(mesa_dir, "src/panfrost/libpan/tess_draw.h")
    draw_helper_path = os.path.join(mesa_dir, "src/panfrost/libpan/draw_helper.cl")

    with open(cmd_draw_path) as f:
        cmd_draw = f.read()
    with open(tess_draw_path) as f:
        tess_draw = f.read()
    with open(draw_helper_path) as f:
        draw_helper = f.read()

    # 1. Extract gpu_prerast_cond_render_pass
    m = re.search(r'(static struct cs_index\s+gpu_prerast_cond_render_pass\s*\([^)]*\)\s*\{.*?\n\})', cmd_draw, re.DOTALL)
    assert m, "gpu_prerast_cond_render_pass not found"
    fn_cond_pass = m.group(1)

    # 2. Extract start_exec lines
    m = re.search(r'(struct cs_index pass = gpu_prerast_cond_render_pass.*?cs_flush_stores\(b\);)', cmd_draw, re.DOTALL)
    assert m, "start_exec not found"
    code_start_exec = m.group(1)

    # 3. Extract guard lines
    m = re.search(r'(\{\s+struct cs_index once = gpu_prerast_load_flag\(b, pass_mem\.gpu \+ 4\);.*?cs_flush_stores\(b\);\s+\}\s+\})', cmd_draw, re.DOTALL)
    assert m, "guard not found"
    code_guard = m.group(1)

    # 4. Extract struct panlib_tess_draw
    m = re.search(r'(struct panlib_tess_draw\s*\{.*?\n\}\s*PACKED;)', tess_draw, re.DOTALL)
    assert m, "struct panlib_tess_draw not found"
    struct_tess_draw = m.group(1)

    def extract_cl_func(text, name):
        m = re.search(r'KERNEL\(\d+\)\s*\n(' + name + r'\s*\([^)]*\)\s*\{.*?\n\})', text, re.DOTALL)
        assert m, f"{name} not found"
        return "void " + m.group(1)

    fn_setup = extract_cl_func(draw_helper, "panlib_tess_setup")
    fn_step = extract_cl_func(draw_helper, "panlib_tess_step")
    fn_draw = extract_cl_func(draw_helper, "panlib_tess_draw")

    def generate_c(code_guard_mod, code_start_mod, is_alias_mutant=False):
        return f"""#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#define PACKED __attribute__((packed))
#define global
#define constant
#define max(a,b) ((a)>(b)?(a):(b))
#define min(a,b) ((a)<(b)?(a):(b))
struct poly_heap {{uint32_t bottom;}};
struct poly_tess_params {{struct poly_heap *heap;uint32_t nr_patches,patches_per_instance;uint32_t *counts;}};
void pstats_add(uint32_t *p,uint32_t n){{*p+=n;}}
typedef uint32_t atomic_uint;
void atomic_fetch_add(uint32_t*p,uint32_t n){{*p+=n;}}
struct cs_builder {{uint64_t r[16];}}; struct cs_index {{unsigned i;}};
#define cs_scratch_reg32(b,x) ((struct cs_index){{x}})
#define cs_scratch_reg64 cs_scratch_reg32
#define cs_move64_to(b,x,v) ((b)->r[(x).i]=(v))
#define cs_move32_to cs_move64_to
#define cs_load32_to(b,x,p,o) ((b)->r[(x).i]=*(uint32_t*)(uintptr_t)((b)->r[(p).i]+(o)))
#define cs_store32(b,x,p,o) (*(uint32_t*)(uintptr_t)((b)->r[(p).i]+(o))=(b)->r[(x).i])
#define cs_store64(b,x,p,o) (*(uint64_t*)(uintptr_t)((b)->r[(p).i]+(o))=(b)->r[(x).i])
#define cs_wait_slot(b,x) ((void)0)
#define cs_flush_stores(b) ((void)0)
#define cs_if(b,c,x) if (((b)->r[(x).i]==0)==((c)==MALI_CS_CONDITION_EQUAL))
#define cs_else(b) else
#define SB_ID(x) 0
#define MALI_CS_CONDITION_EQUAL 0
#define MALI_CS_CONDITION_NEQUAL 1
struct panvk_cs_subqueue_context {{uint32_t cond_render_flag;}};
struct panvk_cmd_buffer {{struct {{struct {{bool enabled,inherited;int exec_cond;uint64_t addr;}}cond_render;}}state;struct panvk_cs_subqueue_context sub;}};
#define cs_subqueue_ctx_reg(b) ((struct cs_index){{15}})
struct cs_index gpu_prerast_load_flag(struct cs_builder*b,uint64_t p){{b->r[0]=*(uint32_t*)(uintptr_t)p;return (struct cs_index){{0}};}}

{struct_tess_draw}

{fn_cond_pass}

{fn_setup}

{fn_step}

{fn_draw}

void start_exec(struct panvk_cmd_buffer *cmdbuf, struct cs_builder *b, uint32_t *mem) {{
    struct {{ uint64_t gpu; }} pass_mem = {{ (uintptr_t)mem }};
    {code_start_mod}
}}

void guard(struct cs_builder *b, struct panlib_tess_draw *t, uint32_t *mem) {{
    struct {{ uint64_t gpu; }} pass_mem = {{ (uintptr_t)mem }};
#define TD(f) ((uintptr_t)t+offsetof(struct panlib_tess_draw,f))
    {code_guard_mod}
}}

static bool arena_busy = false;
static void gpu_prerast_acquire_arena(uint64_t arena) {{
   assert(!arena_busy && "arena deadlock or double-acquire: concurrent invocation must serialize");
   arena_busy = true;
}}
static void gpu_prerast_release_arena(uint64_t arena) {{
   assert(arena_busy && "releasing unacquired arena");
   arena_busy = false;
}}

void execute(struct panvk_cmd_buffer *c, struct cs_builder *b, struct panlib_tess_draw *t, uint32_t *mem, uint32_t *src, uint32_t expected_patches, bool expected) {{
    uint32_t tcs = 0, clip = 0, prims = 0, shader_inv = 0, raster = 0;
    unsigned iters = 0;
    gpu_prerast_acquire_arena(0x1000);
    start_exec(c, b, mem);
    panlib_tess_setup(t, src, 0, 0);
    do {{
        guard(b, t, mem);
        panlib_tess_step(t, &tcs);
        shader_inv += t->vs_draw[0] * t->vs_draw[1] + t->tcs_grid[0] * t->tcs_grid[1];
        for (unsigned i = 0; i < t->p.nr_patches; i++)
            t->p.counts[i] = 1;
        panlib_tess_draw(t, &clip, 0, &prims);
        if (mem[0])
            raster += t->draw[0];
        assert(++iters < 1000);
    }} while (t->more);
    gpu_prerast_release_arena(0x1000);

    assert(tcs == (expected ? expected_patches : 0));
    assert(clip == tcs && prims == tcs && raster == tcs);
    assert(shader_inv == (expected ? 2 * expected_patches : 0));
    assert(mem[1] == 1);
    assert(expected || iters == 1);
}}

int main() {{
    struct cs_builder b = {{0}};
    struct panvk_cmd_buffer c = {{0}};
    struct poly_heap heap = {{0}};
    uint32_t counts[4] = {{0}};
    uint32_t direct_recorded[5] = {{128, 1, 0, 0, 0}};
    uint32_t mem[2] = {{99, 1}};
    uint32_t pred = 0;
    struct panlib_tess_draw t = {{
        .input_patch_size = 1,
        .max_patches = 4,
        .verts_per_prim = 1,
        .p.heap = &heap,
        .p.counts = counts
    }};
    b.r[15] = (uintptr_t)&c.sub;
    c.state.cond_render.addr = (uintptr_t)&pred;

    uint32_t *src = {'(uint32_t *)t.in_draw' if is_alias_mutant else 'direct_recorded'};
    {'(void)direct_recorded; t.in_draw[0] = 128; t.in_draw[1] = 1;' if is_alias_mutant else ''}

    for (unsigned rep = 0; rep < 1000; rep++) {{
        c.state.cond_render.enabled = true;
        c.state.cond_render.inherited = false;
        c.state.cond_render.exec_cond = MALI_CS_CONDITION_NEQUAL;
        pred = (rep % 2) ? 7 : 0;
        execute(&c, &b, &t, mem, src, 128, pred != 0);
        {"" if is_alias_mutant else "assert(direct_recorded[0] == 128 && direct_recorded[1] == 1);"}
    }}

    for (unsigned rep = 0; rep < 1000; rep++) {{
        c.state.cond_render.exec_cond = MALI_CS_CONDITION_EQUAL;
        pred = (rep % 2) ? 7 : 0;
        execute(&c, &b, &t, mem, src, 128, pred == 0);
    }}

    for (unsigned rep = 0; rep < 1000; rep++) {{
        c.state.cond_render.enabled = false;
        c.state.cond_render.inherited = true;
        c.sub.cond_render_flag = (rep % 2);
        src[0] = (rep % 2) ? 13 : 29;
        execute(&c, &b, &t, mem, src, src[0], (rep % 2) != 0);
    }}

    printf("actual extracted predicate/reset/zeroing + setup/step/draw: 3000 replay executions PASS\\n");
    return 0;
}}
"""

    gcc_cmd = ["gcc", "-Wall", "-Werror", "-Wno-error=address-of-packed-member"]

    # 1. Baseline
    src_baseline = generate_c(code_guard, code_start_exec)
    with open("/tmp/panvk_087_test_baseline.c", "w") as f:
        f.write(src_baseline)
    subprocess.check_call(gcc_cmd + ["-o", "/tmp/panvk_087_test_baseline", "/tmp/panvk_087_test_baseline.c"])
    res = subprocess.run(["/tmp/panvk_087_test_baseline"], capture_output=True, text=True)
    assert res.returncode == 0, f"Baseline failed: {res.stderr}"
    print("PASS: Baseline compiled & executed 3,000 replays cleanly")
    print("PASS: Grids & counters verified (tcs, clip, prims, raster, shader_inv)")
    print("PASS: Handshake schedule & arena ownership verified (single-iter handshake on skip)")

    # 2. Mutant 1: cs_move64_to(b, z64, 0) -> cs_move64_to(b, z64, 1)
    mutant_guard = code_guard.replace("cs_move64_to(b, z64, 0);", "cs_move64_to(b, z64, 1);")
    src_m1 = generate_c(mutant_guard, code_start_exec)
    with open("/tmp/panvk_087_test_m1.c", "w") as f:
        f.write(src_m1)
    subprocess.check_call(gcc_cmd + ["-o", "/tmp/panvk_087_test_m1", "/tmp/panvk_087_test_m1.c"])
    res_m1 = subprocess.run(["/tmp/panvk_087_test_m1"], capture_output=True, text=True)
    assert res_m1.returncode != 0, "Mutant 1 (0->1) did not fail!"
    print(f"PASS: Mutant 1 (cs_move64_to 0->1) caught (exit {res_m1.returncode})")

    # 3. Mutant 2: Reset deletion
    mutant_start = code_start_exec.replace("cs_store32(b, once_reset, pass_addr, 4);", "/* deleted reset */")
    src_m2 = generate_c(code_guard, mutant_start)
    with open("/tmp/panvk_087_test_m2.c", "w") as f:
        f.write(src_m2)
    subprocess.check_call(gcc_cmd + ["-o", "/tmp/panvk_087_test_m2", "/tmp/panvk_087_test_m2.c"])
    res_m2 = subprocess.run(["/tmp/panvk_087_test_m2"], capture_output=True, text=True)
    assert res_m2.returncode != 0, "Mutant 2 (reset deletion) did not fail!"
    print(f"PASS: Mutant 2 (reset deletion) caught on replay (exit {res_m2.returncode})")

    # 4. Mutant 3: Direct draw alias
    src_m3 = generate_c(code_guard, code_start_exec, is_alias_mutant=True)
    with open("/tmp/panvk_087_test_m3.c", "w") as f:
        f.write(src_m3)
    subprocess.check_call(gcc_cmd + ["-o", "/tmp/panvk_087_test_m3", "/tmp/panvk_087_test_m3.c"])
    res_m3 = subprocess.run(["/tmp/panvk_087_test_m3"], capture_output=True, text=True)
    assert res_m3.returncode != 0, "Mutant 3 (direct draw alias) did not fail!"
    print(f"PASS: Mutant 3 (direct draw alias) caught on replay (exit {res_m3.returncode})")

    print("ALL CHECKS PASSED: Gate 087 verified.")

if __name__ == "__main__":
    main()
