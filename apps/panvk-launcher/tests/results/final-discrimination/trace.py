#!/usr/bin/env python3
import hashlib
import json
import os
import pathlib
import shlex
import subprocess
import datetime
import time

root = pathlib.Path(__file__).resolve().parent
adb = ['adb', '-s', os.environ.get('ADB_SERIAL', '192.168.1.34:40501')]
app = 'dev.zenithblue.panvklauncher'
f = '/data/user/0/' + app + '/files'
c = f + '/container'
commands = []
def cmd(args):
    commands.append(shlex.join(args))
    return subprocess.check_output(args)
def shell(text):
    return cmd(adb + ['shell', 'run-as', app, 'sh', '-c', shlex.quote(text)])
cmd(adb + ['push', str(root / 'api-dump.json'), '/data/local/tmp/sample-api-dump.json'])
shell('mkdir -p ' + c + '/sample-trace-layer; cp /data/local/tmp/sample-api-dump.json ' + c + '/sample-trace-layer/')
options = {
    'default': '',
    'readback': '',
    'reproducible': 'd3d11.reproducibleCommandStream = True',
    'tiler-off': 'dxvk.tilerMode = False',
}
for mode, option in options.items():
    if os.environ.get('TRACE_DEFAULT_ONLY') and mode != 'default':
        continue
    out = pathlib.Path(os.environ.get('TRACE_OUT', str(root / 'api-trace'))) / mode
    out.mkdir(parents=True, exist_ok=True)
    (out / 'identity.txt').write_bytes(shell('sha256sum ' + c + '/.wine/drive_c/dxsmoke-synchronized.exe ' +
        c + '/.wine/drive_c/windows/system32/{d3d11,dxgi}.dll ' + f + '/m4wsi/libvulkan_panfrost.so ' +
        f + '/contents/imagefs/bionic/usr/lib/libVkLayer_api_dump.so'))
    inner = ('unset VK_LOADER_LAYERS_DISABLE; export VK_LAYER_PATH=' + c + '/sample-trace-layer '
             'VK_INSTANCE_LAYERS=VK_LAYER_LUNARG_api_dump VK_APIDUMP_LOG_FILENAME=' + c + '/api-dump.txt '
             'VK_APIDUMP_DETAILED=true VK_APIDUMP_FLUSH=true DXSMOKE_HOLD=12000 DXSMOKE_STAGING=' +
             ('1' if mode == 'readback' else '0') + '; unset DXSMOKE_SYNC; ' +
             ('export DXVK_CONFIG=' + shlex.quote(option) + '; ' if option else 'unset DXVK_CONFIG; ') + 'exec ' +
             f + '/contents/Proton/11.0-2-arm64ec/lib/wine/aarch64-unix/wine C:\\\\dxsmoke-synchronized.exe d3d11')
    shell('rm -f ' + c + '/api-dump.txt ' + c + '/trace.log ' + c + '/trace-xread.ppm*')
    events = []
    def stamp(event):
        events.append({'event': event, 'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
                       'monotonic_ns': time.monotonic_ns()})
    launch = adb + ['shell', 'run-as', app, 'sh', '-c', shlex.quote('cd ' + c +
          '; sh /data/local/tmp/dxenv-synchronized.sh native sh -c ' + shlex.quote(inner) +
          ' > trace.log 2> trace.err; echo $? > trace.exit')]
    commands.append(shlex.join(launch))
    stamp('launch')
    p = subprocess.Popen(launch, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    deadline = time.monotonic() + 30
    while True:
        log = shell('cat ' + c + '/trace.log 2>/dev/null || true')
        if b'Present frame=7 hr=0x00000000' in log:
            (out / 'observed.log').write_bytes(log)
            stamp('frame7_observed')
            break
        assert p.poll() is None and time.monotonic() < deadline, log.decode(errors='replace')
        time.sleep(0.1)
    (out / 'screenshot.png').write_bytes(cmd(adb + ['exec-out', 'screencap', '-p']))
    (out / 'xgetimage.log').write_bytes(shell('sh /data/local/tmp/dxenv-synchronized.sh native ' +
        c + '/xreadback ' + c + '/trace-xread.ppm'))
    assert p.poll() is None
    stamp('captures_complete_process_alive')
    p.communicate(timeout=30)
    stamp('exit_observed')
    for name in shell('ls ' + c + '/trace-xread.ppm*').decode().splitlines():
        (out / pathlib.Path(name).name).write_bytes(cmd(adb + ['exec-out', 'run-as', app, 'cat', name]))
    (out / 'timestamps.json').write_text(json.dumps(events, indent=2) + '\n')
    for remote, local in [('api-dump.txt', 'api-dump.txt'), ('trace.log', 'stdout.log'),
                          ('trace.err', 'stderr.log'), ('trace.exit', 'exit.txt')]:
        (out / local).write_bytes(cmd(adb + ['exec-out', 'run-as', app, 'cat', c + '/' + remote]))
    (out / 'commands.txt').write_text('\n'.join(commands) + '\n')
    (out / 'manifest.json').write_text(json.dumps({p.name: hashlib.sha256(p.read_bytes()).hexdigest()
        for p in out.iterdir() if p.is_file() and p.name != 'manifest.json'}, indent=2) + '\n')
    print(mode, (out / 'exit.txt').read_text().strip(), (out / 'api-dump.txt').stat().st_size)
