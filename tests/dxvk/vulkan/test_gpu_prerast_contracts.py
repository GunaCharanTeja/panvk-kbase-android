#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[3]
PATCH = ROOT / "patches/csf-v11/018-gpu-prerast-contracts.patch"
text = PATCH.read_text()

required = (
    "PANVK_PRERAST_HW_IDVS",
    "PANVK_PRERAST_GPU_LOWERED",
    "PANVK_GPU_PRERAST_ABI_VERSION",
    "PANVK_GPU_PRERAST_SIDE_EFFECT_SINGLE_EXECUTION",
    "PANVK_GPU_PRERAST_LIFETIME_SUBMISSION",
    "PANVK_GPU_PRERAST_INTERNAL_JOB_SPLIT",
    "PANVK_GPU_PRERAST_ORDER_ORIGINAL_PRIMITIVE",
    "PANVK_GPU_PRERAST_STATE_SAVED_AND_DIRTIED",
    "VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT",
    "VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT",
    "PANVK_GPU_PRERAST_MAX_EXECUTION_BYTES",
    "PANVK_GPU_PRERAST_ARENA_SIZE",
    "PANVK_DEBUG_GPU_PRERAST",
    "PANVK_VS_VARIANT_GPU_PASSTHROUGH",
    "pan_nir_lower_vs_inputs_poly",
    "poly_nir_lower_vs_before_gs",
    "poly_nir_lower_sw_vs",
    "poly_nir_lower_sysvals",
    "requirements && lowered_vs_compiled",
    "nir_intrinsic_load_vertex_param_buffer_poly",
    "panvk_lower_gpu_prerast_sysval",
    "if (!gpu_lower && v != PANVK_VS_VARIANT_GPU_PASSTHROUGH)",
    "bool gpu_lower = false",
    "nir->info.stage = MESA_SHADER_COMPUTE",
    'PANVK_VS_VARIANT_GPU_LOWERED] = "GPU_LOWERED"',
    'PANVK_VS_VARIANT_GPU_PASSTHROUGH] = "GPU_PASSTHROUGH"',
    "gpu_prerast_abi",
    "lowered_vs_compiled = true",
    "passthrough_vs_compiled = true",
    "gpu_prerast_can_lower",
    "gpu_prerast_draw",
    "gpu_prerast_wait_compute",
    "gpu_prerast_release_arena",
    "cs_sync32_wait",
    "cs_sync32_add",
    "nir_load_first_vertex",
    "nir_load_instance_id",
    "primitive_restart",
    "PANVK_GPU_PRERAST_MAX_INVOCATIONS",
    "libpoly_nir",
    "with_panfrost_vk",
)
for token in required:
    assert token in text, token

for forbidden in ("PANVK_PRERAST_SW", "panvk_sw_prerast", "cmd_alloc_dev_mem"):
    assert forbidden not in text, forbidden

assert "gpu_lower = v == PANVK_VS_VARIANT_GPU_LOWERED &&" not in text

physical_device = ROOT / "work/mesa/src/panfrost/vulkan/panvk_vX_physical_device.c"
if physical_device.exists():
    exposed = physical_device.read_text()
    for feature in (
        "geometryShader",
        "fillModeNonSolid",
        "shaderClipDistance",
        "shaderCullDistance",
    ):
        assert f".{feature} = false" in exposed, feature
    assert ".vertexPipelineStoresAndAtomics =" in exposed
    assert "PAN_ARCH >= 13" in exposed

with tempfile.TemporaryDirectory() as directory:
    checkout = Path(directory) / "mesa"
    subprocess.run(
        ["git", "clone", "--no-checkout", str(ROOT / "work/mesa"), str(checkout)],
        check=True,
        stdout=subprocess.DEVNULL,
    )
    subprocess.run(
        ["git", "-C", checkout, "checkout", "-q",
         "5a07217f034b3e50d8c7c7794f97a2df1742613b"],
        check=True,
    )
    subprocess.run(
        [str(ROOT / "scripts/apply-patches.sh"), "--profile", "g615-v11-csf",
         "--mesa", str(checkout)],
        check=True,
        stdout=subprocess.DEVNULL,
    )

print("PASS: GPU prerast VS records to IDVS/FS slice; exposure unchanged")
