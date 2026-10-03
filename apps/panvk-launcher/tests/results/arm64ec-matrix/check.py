#!/usr/bin/env python3
"""Validate artifact hashes, ordering, source counts and raw X11 pixels."""
import hashlib
import json
import pathlib
import re

root = pathlib.Path(__file__).resolve().parent
for api in ('d3d8', 'd3d9', 'd3d10', 'd3d11'):
    out = root / api
    for name, digest in json.loads((out / 'manifest.json').read_text()).items():
        assert hashlib.sha256((out / name).read_bytes()).hexdigest() == digest, name
    events = [e['event'] for e in json.loads((out / 'timestamps.json').read_text())]
    assert events.index('present_success_observed') < events.index('captures_complete_process_alive') < events.index('process_exit_observed')
    log = (out / 'd3d11.log').read_text()
    assert (out / 'exit.txt').read_text().strip() == '0'
    assert ('readback=ORANGE_PASS (orange=614400 zero=0 other=0)' in log if api == 'd3d11'
            else 'source orange=76800 zero=0 other=0 need=76800' in log)
    clients = []
    for ppm in out.glob('*.ppm'):
        data = ppm.read_bytes()
        header = re.match(rb'P6\s+(\d+)\s+(\d+)\s+255\s', data)
        assert header, ppm
        w, h = map(int, header.groups())
        if (w, h) != (320, 240):
            continue
        pixels = data[header.end():]
        assert len(pixels) == w * h * 3
        orange = sum(pixels[i:i+3] == bytes((255, 128, 64)) for i in range(0, len(pixels), 3))
        assert orange == 76800, (ppm, orange)
        clients.append(ppm.name)
    assert len(clients) >= 2, api
    print(api, 'source=PASS X11=76800/76800', clients)
