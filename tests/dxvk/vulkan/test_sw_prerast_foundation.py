#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
PATCH = ROOT / "patches/csf-v11/018-software-prerast-foundation.patch"
text = PATCH.read_text()

for token in (
    "PANVK_PRERAST_HW_IDVS",
    "PANVK_PRERAST_SW",
    "panvk_sw_prerast_select",
    "sw_prerast_draw_info",
    "first_instance",
    "primitive_restart",
    "panvk_cmd_alloc_dev_mem(cmdbuf, desc",
    "libpoly_nir",
    "with_panfrost_vk",
):
    assert token in text, token

physical_device = (ROOT / "work/mesa/src/panfrost/vulkan/panvk_vX_physical_device.c").read_text()
for feature in ("geometryShader", "fillModeNonSolid", "shaderClipDistance", "shaderCullDistance"):
    assert f".{feature} = false" in physical_device, feature

print("PASS: software pre-raster foundation contract; feature exposure unchanged")
