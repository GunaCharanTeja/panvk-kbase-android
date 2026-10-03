#!/usr/bin/env python3
"""Validate differential evidence, not acceptance of normal rendering."""
import hashlib
import json
import pathlib
import re

root = pathlib.Path(__file__).resolve().parent
identities = set()
executables = set()
for mode in ('default', 'flush', 'event', 'readback', 'default-repeat'):
    d = root / mode
    for name, sha in json.loads((d / 'manifest.json').read_text()).items():
        assert hashlib.sha256((d / name).read_bytes()).hexdigest() == sha, (mode, name)
    identities.add((d / 'identity.txt').read_bytes())
    build = json.loads((d / 'build-identity.json').read_text())
    executables.add(build['dxsmoke-arm64ec.exe'])
    log = (d / 'd3d11.log').read_text()
    assert (d / 'exit.txt').read_text().strip() == '0'
    assert len(re.findall(r'Present frame=\d hr=0x00000000', log)) == 8
    observed = (d / 'present-observed.log').read_text()
    assert 'Present frame=7 hr=0x00000000' in observed
    assert 'outcome API=PASS' not in observed
    if mode == 'event':
        assert len(re.findall(r'GPU event complete frame=\d', observed)) == 8
    assert ('readback=ORANGE_PASS' if mode == 'readback' else 'readback=SKIPPED') in log
    events = {e['event']: e['monotonic_ns'] for e in json.loads((d / 'timestamps.json').read_text())}
    assert events['present_success_observed'] < events['xgetimage_start']
    assert events['captures_complete_process_alive'] < events['process_exit_observed']
    clients = []
    for p in d.glob('*.ppm'):
        header, pixels = p.read_bytes().split(b'\n255\n', 1)
        if b'320 240' not in header:
            continue
        assert len(pixels) == 320 * 240 * 3
        expected = bytes((255, 128, 64)) if mode == 'readback' else bytes(3)
        assert pixels == expected * (320 * 240), (mode, p.name)
        clients.append(p)
    assert len(clients) == 2, mode
    print(mode, 'orange' if mode == 'readback' else 'black', '76800/76800')
assert len(identities) == len(executables) == 1
