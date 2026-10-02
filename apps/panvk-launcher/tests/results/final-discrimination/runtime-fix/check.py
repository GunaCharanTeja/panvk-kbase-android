#!/usr/bin/env python3
import hashlib
import json
import pathlib
import re

root = pathlib.Path(__file__).resolve().parent
for name in ['unpatched-control', 'd3d8', 'd3d9', 'd3d10', 'd3d11', 'ui-d3d8', 'ui-d3d9', 'ui-d3d10', 'ui-d3d11', 'api-trace/default']:
    d = root / name
    for file, sha in json.loads((d / 'manifest.json').read_text()).items():
        assert hashlib.sha256((d / file).read_bytes()).hexdigest() == sha, (name, file)
    black = name == 'unpatched-control'
    clients = []
    for p in d.glob('*.ppm'):
        header, pixels = p.read_bytes().split(b'\n255\n', 1)
        if b'320 240' in header:
            assert pixels == (bytes(3) if black else bytes((255, 128, 64))) * 76800, p
            clients.append(p)
    assert len(clients) == 2, name
    log = (d / ('run.log' if name.startswith('ui-') else ('stdout.log' if name.startswith('api-') else 'd3d11.log'))).read_text()
    assert 'DXSMOKE: FAIL' not in log
    assert not re.search(r'source orange=| Map hr=|GPU event complete|readback=ORANGE_PASS', log)
    if 'd3d11' in name or name.startswith('api-') or black:
        assert 'sync=none staging=0' in log
        assert 'readback=SKIPPED' in log
        assert len(re.findall(r'Present frame=\d hr=0x00000000', log)) == 8
    if name.startswith('ui-'):
        assert 'exit=0' in log
    else:
        assert (d / 'exit.txt').read_text().strip() == '0'
    print(name, 'BLACK control' if black else 'ORANGE normal', '76800/76800')
trace = (root / 'api-trace/default/api-dump.txt').read_text()
assert trace.count('VK_ATTACHMENT_LOAD_OP_CLEAR') == 8
assert len(re.findall(r'vkCmdDraw\(', trace)) == 8
print('fixed normal Vulkan trace: 8 materialized clears, 8 presenter draws')
