#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
PATCH = ROOT / "patches/csf-v11/019-crc-invalidate-undefined-clear.patch"
PIN = "5a07217f034b3e50d8c7c7794f97a2df1742613b"
text = PATCH.read_text()
assert "invalidate_initial_attachment_crcs" in text
assert "VK_ATTACHMENT_LOAD_OP_CLEAR" in text
assert "if (!host_preinit && !discard)" in text
mesa = ROOT / "work/mesa"
with tempfile.TemporaryDirectory() as d:
    checkout = Path(d) / "mesa"
    subprocess.run(["git", "-C", str(mesa), "worktree", "add", "--detach",
                    str(checkout), PIN], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        subprocess.run([str(ROOT / "scripts/apply-patches.sh"),
                        "--profile", "g615-v11-csf", "--mesa", str(checkout)],
                       check=True, stdout=subprocess.DEVNULL)
        lowered = (checkout / "src/panfrost/vulkan/csf/panvk_vX_cmd_draw.c").read_text()
        assert "if (!host_preinit && !discard)" in lowered
        assert "VK_ATTACHMENT_LOAD_OP_CLEAR" in lowered
    finally:
        subprocess.run(["git", "-C", str(mesa), "worktree", "remove", "--force",
                        str(checkout)], stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)
print("PASS")
