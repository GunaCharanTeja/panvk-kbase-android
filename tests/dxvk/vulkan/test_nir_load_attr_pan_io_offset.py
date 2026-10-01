#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
PATCH = ROOT / "patches/common/019-nir-load-attr-pan-io-offset.patch"
PIN = "5a07217f034b3e50d8c7c7794f97a2df1742613b"
text = PATCH.read_text()
assert "case nir_intrinsic_load_attr_pan:" in text
assert "return 2;" in text
assert "nir_get_io_offset_src_number" in text
mesa = ROOT / "work/mesa"
with tempfile.TemporaryDirectory() as d:
    checkout = Path(d) / "mesa"
    subprocess.run(["git", "clone", "--no-checkout", str(mesa), str(checkout)],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["git", "-C", str(checkout), "checkout", "-q", PIN], check=True)
    subprocess.run(["git", "-C", str(checkout), "apply", "--check", str(PATCH)], check=True)
    subprocess.run(["git", "-C", str(checkout), "apply", str(PATCH)], check=True)
    lowered = (checkout / "src/compiler/nir/nir_lower_io.c").read_text()
    assert "case nir_intrinsic_load_attr_pan:" in lowered
print("PASS")
