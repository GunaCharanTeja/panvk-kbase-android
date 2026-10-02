#!/usr/bin/env python3
import hashlib
import json
import pathlib
import re

root = pathlib.Path(__file__).resolve().parent
fixed = dict((pathlib.Path(path).name, sha) for sha, path in
             (line.split() for line in (root.parent / 'fixed-identity.txt').read_text().splitlines()))
component = (root / 'component-before.txt').read_text()
for arch in ('system32', 'syswow64'):
    hashes = {}
    for sha, path in re.findall(r'^([0-9a-f]{64})  (.*)$', component, re.M):
        if '/' + arch + '/' in path:
            hashes.setdefault(pathlib.Path(path).name, set()).add(sha)
    assert len(hashes) == 5 and all(len(values) == 1 for values in hashes.values()), arch
print('sole installed DXVK: 3.1.1-clearfix-arm64ec; source/prefix hashes match both architectures')
for api in ('d3d8', 'd3d9', 'd3d10', 'd3d11'):
    d = root / api
    for file, sha in json.loads((d / 'manifest.json').read_text()).items():
        assert hashlib.sha256((d / file).read_bytes()).hexdigest() == sha, (api, file)
    for sha, path in re.findall(r'^([0-9a-f]{64})  (.*\.dll)$', (d / 'identity.txt').read_text(), re.M):
        assert fixed[pathlib.Path(path).name] == sha
    clients = []
    for p in d.glob('*.ppm'):
        header, pixels = p.read_bytes().split(b'\n255\n', 1)
        if b'320 240' in header:
            assert pixels == bytes((255, 128, 64)) * 76800, p
            clients.append(p)
    assert len(clients) == 2, api
    log = (d / 'd3d11.log').read_text()
    assert 'DXSMOKE: FAIL' not in log
    assert not re.search(r'source orange=| Map hr=|GPU event complete|readback=ORANGE_PASS', log)
    assert (d / 'exit.txt').read_text().strip() == '0'
    env = (d / 'environment.txt').read_text()
    assert 'VK_LOADER_LAYERS_DISABLE=~all~' in env
    assert 'VK_INSTANCE_LAYERS=' not in env
    if api == 'd3d11':
        assert 'sync=none staging=0' in log and 'readback=SKIPPED' in log
        assert len(re.findall(r'Present frame=\d hr=0x00000000', log)) == 8
    events = {e['event']: e['monotonic_ns'] for e in json.loads((d / 'timestamps.json').read_text())}
    assert events['present_success_observed'] < events['captures_complete_process_alive'] < events['process_exit_observed']
    elapsed = lambda end, start: round((events[end] - events[start]) / 1e6, 1)
    print(api, 'ORANGE 76800/76800 x2; exit=0;',
          'present_ms=', elapsed('present_success_observed', 'launch'),
          'capture_ms=', elapsed('captures_complete_process_alive', 'present_success_observed'))
