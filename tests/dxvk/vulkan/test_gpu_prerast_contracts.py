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
    "VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT",
    "PANVK_GPU_PRERAST_MAX_EXECUTION_BYTES",
    "poly_nir_lower_vs_before_gs",
    "poly_nir_lower_sw_vs",
    "poly_nir_lower_sysvals",
    "info->nir->num_inputs == 0",
    "nir_intrinsic_load_vertex_param_buffer_poly",
    "panvk_lower_gpu_prerast_sysval",
    "nir->info.stage = MESA_SHADER_COMPUTE",
    'PANVK_VS_VARIANT_GPU_LOWERED] = "GPU_LOWERED"',
    "gpu_prerast_abi",
    "lowered_vs_compiled = true",
    "libpoly_nir",
    "with_panfrost_vk",
)
for token in required:
    assert token in text, token

for forbidden in ("PANVK_PRERAST_SW", "panvk_sw_prerast", "cmd_alloc_dev_mem"):
    assert forbidden not in text, forbidden

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
    checkout.mkdir()
    archive = subprocess.Popen(
        ["git", "-C", ROOT / "work/mesa", "archive", "5a07217f034b3e50d8c7c7794f97a2df1742613b"],
        stdout=subprocess.PIPE,
    )
    subprocess.run(["tar", "-x", "-C", checkout], stdin=archive.stdout, check=True)
    assert archive.wait() == 0
    subprocess.run(["git", "apply", "--recount", "--check", PATCH], cwd=checkout, check=True)

print("PASS: GPU-lowered pre-raster contracts; first VS compute variant; exposure unchanged")
