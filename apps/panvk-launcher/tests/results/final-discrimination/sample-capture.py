#!/usr/bin/env python3
"""Capture native sampled clear without touching driver binaries."""
import datetime
import hashlib
import json
import os
import pathlib
import shlex
import subprocess
import time

root = pathlib.Path(__file__).resolve().parent / 'native-sample'
adb = ['adb', '-s', os.environ.get('ADB_SERIAL', '192.168.1.34:40501')]
app = 'dev.zenithblue.panvklauncher'
c = '/data/user/0/' + app + '/files/container'
commands = []

def cmd(args):
    commands.append(shlex.join(args))
    return subprocess.check_output(args)

def shell(text):
    return cmd(adb + ['shell', 'run-as', app, 'sh', '-c', shlex.quote(text)])

for mode in ('sample', 'sample-copy', 'sample-repeat'):
    out = root / mode
    out.mkdir(parents=True, exist_ok=True)
    commands.clear()
    events = []
    def stamp(event):
        events.append({'event': event, 'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
                       'monotonic_ns': time.monotonic_ns()})
    actual = 'sample' if mode == 'sample-repeat' else mode
    (out / 'identity.txt').write_bytes(shell('sha256sum ' + c + '/nclear ' + c + '/sample-clear.*.spv ' +
        '/data/user/0/' + app + '/files/m4wsi/libvulkan_panfrost.so'))
    shell('rm -f ' + c + '/sample.log ' + c + '/sample.err ' + c + '/sample.exit ' + c + '/native-xread.ppm*')
    launch = adb + ['shell', 'run-as', app, 'sh', '-c', shlex.quote('cd ' + c +
        '; sh /data/local/tmp/dxenv-synchronized.sh native ./nclear ' + actual +
        ' 8 > sample.log 2> sample.err; echo $? > sample.exit')]
    commands.append(shlex.join(launch))
    stamp('launch')
    p = subprocess.Popen(launch, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    deadline = time.monotonic() + 30
    while True:
        log = shell('cat ' + c + '/sample.log 2>/dev/null || true')
        if b'final mode=' in log:
            (out / 'observed.log').write_bytes(log)
            stamp('final_present_and_client_observed')
            break
        assert p.poll() is None and time.monotonic() < deadline, log.decode(errors='replace')
        time.sleep(0.1)
    stamp('screenshot_start')
    (out / 'screenshot.png').write_bytes(cmd(adb + ['exec-out', 'screencap', '-p']))
    stamp('screenshot_end')
    (out / 'xgetimage.log').write_bytes(shell('sh /data/local/tmp/dxenv-synchronized.sh native ' +
        c + '/xreadback ' + c + '/native-xread.ppm'))
    stamp('xgetimage_end')
    assert p.poll() is None
    stamp('captures_complete_process_alive')
    p.communicate(timeout=30)
    stamp('exit_observed')
    for remote, local in [('sample.log', 'stdout.log'), ('sample.err', 'stderr.log'), ('sample.exit', 'exit.txt'),
                          ('nclear-' + actual + '.raw', 'client.raw')]:
        (out / local).write_bytes(cmd(adb + ['exec-out', 'run-as', app, 'cat', c + '/' + remote]))
    if actual == 'sample-copy':
        (out / 'source.raw').write_bytes(cmd(adb + ['exec-out', 'run-as', app, 'cat', c + '/nclear-sample-copy-src.raw']))
    for name in shell('ls ' + c + '/native-xread.ppm*').decode().splitlines():
        (out / pathlib.Path(name).name).write_bytes(cmd(adb + ['exec-out', 'run-as', app, 'cat', name]))
    (out / 'timestamps.json').write_text(json.dumps(events, indent=2) + '\n')
    (out / 'commands.txt').write_text('\n'.join(commands) + '\n')
    (out / 'manifest.json').write_text(json.dumps({f.name: hashlib.sha256(f.read_bytes()).hexdigest()
        for f in out.iterdir() if f.is_file() and f.name != 'manifest.json'}, indent=2, sort_keys=True) + '\n')
    print(mode, (out / 'exit.txt').read_text().strip())
    print('\n'.join(l for l in (out / 'stdout.log').read_text().splitlines() if l.startswith(('final', 'src ', 'FAIL'))))
