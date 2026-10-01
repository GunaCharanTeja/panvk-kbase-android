import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def mesa_root():
    """Mesa checkout for source checks. Default is the integrated tree."""
    return Path(os.environ.get("PANVK_MESA", ROOT / "work" / "mesa-dxint"))
