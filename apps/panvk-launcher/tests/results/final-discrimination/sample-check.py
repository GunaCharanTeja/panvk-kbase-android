#!/usr/bin/env python3
import hashlib
import json
import pathlib
import re

root = pathlib.Path(__file__).resolve().parent
for group in ('native-sample', 'api-trace'):
    for d in (root / group).iterdir():
        for name, sha in json.loads((d / 'manifest.json').read_text()).items():
            assert hashlib.sha256((d / name).read_bytes()).hexdigest() == sha, (d, name)
        assert (d / 'exit.txt').read_text().strip() == '0'
        orange = group == 'native-sample' or d.name == 'readback'
        clients = 0
        for p in d.glob('*.ppm'):
            header, pixels = p.read_bytes().split(b'\n255\n', 1)
            if b'320 240' in header:
                assert pixels == (bytes((255, 128, 64)) if orange else bytes(3)) * 76800, p
                clients += 1
        assert clients >= 1
        events = {e['event']: e['monotonic_ns'] for e in json.loads((d / 'timestamps.json').read_text())}
        assert events['captures_complete_process_alive'] < events['exit_observed']
        if group == 'native-sample':
            if (d / 'source.raw').exists():
                assert (d / 'source.raw').read_bytes() == bytes((64, 128, 255, 255)) * 76800
            assert 'drawable=ORANGE_PASS' in (d / 'stdout.log').read_text()
        else:
            trace = (d / 'api-dump.txt').read_text()
            blocks = re.split(r'\nThread ', trace)
            cmds = [b for b in blocks if len(b.splitlines()) > 1 and b.splitlines()[1].startswith('vkCmd')]
            clears = [b for b in cmds if 'float = 0.5' in b and 'float = 0.25' in b]
            assert len(clears) == (8 if orange else 0)
            assert trace.count('VK_ATTACHMENT_LOAD_OP_CLEAR') == (8 if orange else 0)
            assert len(re.findall(r'vkCmdDraw\(', trace)) == 8
            assert len(re.findall(r'vkQueuePresentKHR\(', trace)) == 8
            print(d.name, 'orange Vulkan clear commands', len(clears))
        print(group, d.name, 'orange' if orange else 'black', 'verified')
