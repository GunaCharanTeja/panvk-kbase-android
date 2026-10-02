#!/usr/bin/env python3
"""Offline, stdlib-only checks of synchronization and client pixels."""
import json
import hashlib
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(__file__).resolve().parent
manifest = json.loads((root / 'manifest.json').read_text())
for name, digest in manifest.items():
    assert pathlib.Path(name).name == name
    assert hashlib.sha256((root / name).read_bytes()).hexdigest() == digest, name
assert (root / 'screenshot.png').read_bytes().startswith(b'\x89PNG\r\n\x1a\n')
assert {'identity.txt', 'build-identity.json', 'environment.txt', 'timestamps.json',
        'd3d11.log', 'present-observed.log', 'xgetimage.log', 'screenshot.png', 'exit.txt'} <= manifest.keys()
environment = dict(line.split('=', 1) for line in (root / 'environment.txt').read_text().splitlines())
assert environment['VK_LOADER_LAYERS_DISABLE'] == '~all~'
assert environment['DXSMOKE_HOLD'] == '12000'
assert not {'VK_INSTANCE_LAYERS', 'VK_LAYER_PATH', 'VK_ADD_LAYER_PATH',
            'VK_LOADER_LAYERS_ENABLE', 'VK_LOADER_LAYERS_ALLOW', 'PANVK_FB_PATH'} & environment.keys()
events = {e['event']: e['monotonic_ns'] for e in json.loads((root / 'timestamps.json').read_text())}
for kind in ['screenshot', 'xgetimage']:
    assert events['present_success_observed'] < events[kind + '_start']
    assert events[kind + '_start'] < events[kind + '_end']
    assert events[kind + '_end'] < events['captures_complete_process_alive']
assert events['captures_complete_process_alive'] < events['process_exit_observed']
assert max(events['screenshot_start'], events['xgetimage_start']) < min(
    events['screenshot_end'], events['xgetimage_end'])
log = (root / 'present-observed.log').read_text()
assert (root / 'd3d11.log').read_bytes().startswith((root / 'present-observed.log').read_bytes())
assert re.search(r'^DXSMOKE: api=d3d11 Present.*hr=0x00000000', log, re.M) and 'DXSMOKE: PASS' not in log and 'outcome API=PASS' not in log
assert 'client=320x240 hold_ms=12000' in log
assert (root / 'exit.txt').read_text().strip() == '0'
d3d11_text = (root / 'd3d11.log').read_text()
assert 'DXSMOKE: PASS api=d3d11' in d3d11_text or 'outcome API=PASS' in d3d11_text
tree = (root / 'xgetimage.log').read_text()
assert 'GRAB fail' not in tree and 'failed' not in tree
writes = re.findall(r'^WROTE (\S+) (\d+)x(\d+) bpp=32 masks=ff0000/ff00/ff orange=(\d+)/(\d+)$', tree, re.M)
assert writes
assert len(writes) == len(re.findall(r'^WROTE ', tree, re.M))
assert any(path.endswith('/synchronized-xread.ppm') for path, *_ in writes)
expected = set()
for path, width, height, orange, count in writes:
    assert path.startswith('/data/user/0/dev.zenithblue.panvklauncher/files/container/synchronized-xread.ppm')
    name = pathlib.Path(path).name
    assert name not in expected
    expected.add(name)
    assert int(count) == int(width) * int(height)
    assert int(orange) <= int(count)
    ppm = (root / name).read_bytes()
    header = f'P6\n{width} {height}\n255\n'.encode()
    assert ppm.startswith(header) and len(ppm) == len(header) + int(count) * 3
assert expected == {p.name for p in root.glob('synchronized-xread.ppm*')}
assert expected <= manifest.keys()
clients = re.findall(r'WIN depth=\d+ id=(0x[0-9a-f]+) map=1 320x240', tree)
assert clients
for client in clients:
    assert 'synchronized-xread.ppm-' + client + '.ppm' in expected
    ppm = (root / ('synchronized-xread.ppm-' + client + '.ppm')).read_bytes()
    header = b'P6\n320 240\n255\n'
    assert ppm.startswith(header)
    pixels = ppm[len(header):]
    assert len(pixels) == 320 * 240 * 3
    orange = sum(r > 200 and 80 < g < 180 and b < 100
                 for r, g, b in zip(pixels[0::3], pixels[1::3], pixels[2::3]))
    assert not any(pixels), 'Evidence changed: client no longer entirely black'
    print(f'{client}: black=76800/76800 orange={orange}/76800')
print('Synchronization verified. API PASS; visual FAIL (black client).')
