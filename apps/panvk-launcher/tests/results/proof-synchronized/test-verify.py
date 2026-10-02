#!/usr/bin/env python3
"""Negative proof checks; mutations rehashed to test semantic checks too."""
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile

root = pathlib.Path(__file__).resolve().parent
subprocess.run([sys.executable, str(root / 'verify.py')], check=True)
for case in ['mismatched-log', 'missing-screenshot', 'failed-readback', 'stale-ppm', 'wrong-mask', 'split-present-line', 'present-failed', 'bad-hash']:
    with tempfile.TemporaryDirectory() as tmp:
        copy = pathlib.Path(tmp) / 'proof'
        shutil.copytree(root, copy)
        if case == 'mismatched-log':
            p = copy / 'present-observed.log'
            p.write_bytes(b'mismatched\n' + p.read_bytes())
        elif case == 'missing-screenshot':
            (copy / 'screenshot.png').unlink()
        elif case == 'failed-readback':
            (copy / 'xgetimage.log').write_text('GRAB fail id=0xc0002d\n')
        elif case == 'stale-ppm':
            (copy / 'synchronized-xread.ppm-0xdead.ppm').write_bytes(b'P6\n1 1\n255\n\0\0\0')
        elif case == 'wrong-mask':
            p = copy / 'xgetimage.log'
            p.write_text(p.read_text().replace('masks=ff0000/ff00/ff', 'masks=ff/ff00/ff0000'))
        elif case == 'split-present-line':
            p = copy / 'present-observed.log'
            p.write_text('DXSMOKE: api=d3d11 Present frame=0\nhr=0x00000000\nclient=320x240 hold_ms=12000\n')
            (copy / 'd3d11.log').write_text(p.read_text() + 'DXSMOKE: outcome API=PASS readback=SKIPPED (orange=0 zero=0 other=0) frames=8\n')
        elif case == 'present-failed':
            p = copy / 'present-observed.log'
            p.write_text('DXSMOKE: api=d3d11 Present frame=0 hr=0x887a0005 (FAILED)\nclient=320x240 hold_ms=12000\n')
            (copy / 'd3d11.log').write_text(p.read_text() + 'DXSMOKE: outcome API=FAIL frames=8\n')
        else:
            (copy / 'identity.txt').write_text('changed\n')
        if case != 'bad-hash':
            manifest = json.loads((copy / 'manifest.json').read_text())
            for name in manifest:
                if (copy / name).exists():
                    manifest[name] = hashlib.sha256((copy / name).read_bytes()).hexdigest()
            (copy / 'manifest.json').write_text(json.dumps(manifest))
        result = subprocess.run([sys.executable, str(root / 'verify.py'), str(copy)], capture_output=True)
        assert result.returncode != 0, case
        print('Rejected:', case)
