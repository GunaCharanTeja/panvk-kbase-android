#!/usr/bin/env python3
"""Actual launcher Recent Executables Run button, no Wine CLI launch."""
import datetime
import hashlib
import json
import os
import pathlib
import shlex
import subprocess
import sys
import time

api, x, y = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
assert api in ('d3d8', 'd3d9', 'd3d10', 'd3d11')
out = pathlib.Path(__file__).resolve().parent / 'runtime-fix' / ('ui-' + api)
out.mkdir(parents=True, exist_ok=True)
adb = ['adb', '-s', os.environ.get('ADB_SERIAL', '192.168.1.34:40501')]
app = 'dev.zenithblue.panvklauncher'
f = '/data/user/0/' + app + '/files'
c = f + '/container'
commands, events = [], []
def cmd(args):
    commands.append(shlex.join(args))
    return subprocess.check_output(args)
def shell(text):
    return cmd(adb + ['shell', 'run-as', app, 'sh', '-c', shlex.quote(text)])
def stamp(event):
    events.append({'event': event, 'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
                   'monotonic_ns': time.monotonic_ns()})
old = set(shell('ls ' + f + '/logs/run-*.log').decode().splitlines())
(out / 'identity.txt').write_bytes(shell('sha256sum ' + c + '/.wine/drive_c/windows/system32/{d3d8,d3d9,d3d10core,d3d11,dxgi}.dll ' +
    f + '/m4wsi/libvulkan_panfrost.so ' + f + '/drivers/PanVK_Kbase_G615/libvulkan_panfrost.so ' +
    c + '/.wine/drive_c/dxsmoke-' + api + '-arm64ec-w64-mingw32.exe'))
stamp('ui_run_tap')
cmd(adb + ['shell', 'input', 'tap', str(x), str(y)])
deadline = time.monotonic() + 20
while True:
    logs = set(shell('ls ' + f + '/logs/run-*.log').decode().splitlines()) - old
    for name in sorted(logs):
        log = shell('cat ' + name)
        match = b'Present frame=7 hr=0x00000000' if api == 'd3d11' else b'Present hr=0x00000000'
        if match in log or (api == 'd3d8' and b'DXSMOKE: api=d3d8 Present hr=' in log):
            assert b'outcome API=PASS' not in log and b'DXSMOKE: PASS' not in log
            break
    else:
        assert time.monotonic() < deadline, 'No new successful launcher Present'
        time.sleep(0.1)
        continue
    break
(out / 'observed.log').write_bytes(log)
stamp('present_observed')
# Bring existing X11 activity forward; process was launched by app Run above.
cmd(adb + ['shell', 'am', 'start', '-n', 'com.termux.x11/.MainActivity'])
(out / 'screenshot.png').write_bytes(cmd(adb + ['exec-out', 'screencap', '-p']))
shell('rm -f ' + c + '/ui-fixed.ppm*')
(out / 'xgetimage.log').write_bytes(shell('sh /data/local/tmp/dxenv-synchronized.sh native ' + c + '/xreadback ' + c + '/ui-fixed.ppm'))
stamp('captures_complete')
assert b'outcome API=PASS' not in shell('cat ' + name) and b'DXSMOKE: PASS' not in shell('cat ' + name)
for remote in shell('ls ' + c + '/ui-fixed.ppm*').decode().splitlines():
    (out / pathlib.Path(remote).name).write_bytes(cmd(adb + ['exec-out', 'run-as', app, 'cat', remote]))
time.sleep(13)
(out / 'run.log').write_bytes(shell('cat ' + name))
stamp('final_log_collected')
(out / 'timestamps.json').write_text(json.dumps(events, indent=2) + '\n')
(out / 'commands.txt').write_text('\n'.join(commands) + '\n')
(out / 'manifest.json').write_text(json.dumps({p.name: hashlib.sha256(p.read_bytes()).hexdigest()
    for p in out.iterdir() if p.is_file() and p.name != 'manifest.json'}, indent=2) + '\n')
print(api, (out / 'xgetimage.log').read_text())
