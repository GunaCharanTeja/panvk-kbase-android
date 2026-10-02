#!/usr/bin/env python3
"""Run from the repo root. Builds test tools only; leaves the driver untouched."""
import concurrent.futures
import datetime
import hashlib
import json
import os
import pathlib
import re
import shlex
import subprocess
import time
import tempfile

TOOLS = pathlib.Path(__file__).resolve().parent
OUT = pathlib.Path(os.environ.get('PROOF_OUT', str(TOOLS))).resolve()
OUT.mkdir(parents=True, exist_ok=True)
API = os.environ.get('DXSMOKE_API', 'd3d11')
ARCH = os.environ.get('DXSMOKE_ARCH', 'i686')
STAGING = os.environ.get('DXSMOKE_STAGING', '1')
SYNC = os.environ.get('DXSMOKE_SYNC', '')
assert STAGING in ('0', '1') and SYNC in ('', 'flush', 'event')
assert API in ('d3d8', 'd3d9', 'd3d10', 'd3d11')
assert ARCH in ('i686', 'arm64ec')
ADB = ['adb'] + (['-s', os.environ['ADB_SERIAL']] if os.environ.get('ADB_SERIAL') else [])
APP = 'dev.zenithblue.panvklauncher'
C = '/data/user/0/' + APP + '/files/container'
events = []
transcript = []


def stamp(event):
    events.append({'event': event, 'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
                   'monotonic_ns': time.monotonic_ns()})


def command(args, check=True):
    transcript.append(shlex.join(args))
    p = subprocess.run(args, capture_output=True, check=check)
    return p.stdout


def shell(text):
    return command(ADB + ['shell', 'run-as', APP, 'sh', '-c', shlex.quote(text)])


build = tempfile.TemporaryDirectory(prefix='panvk-proof-')
helper = build.name + '/xreadback'
exe = build.name + '/dxsmoke-' + ARCH + '.exe'
command([os.environ.get('ANDROID_CC', 'aarch64-linux-android35-clang'), '-O2', '-Wall', '-Wextra',
         str(TOOLS / 'xreadback.c'), '-L' + os.environ['X11_LIBDIR'], '-lX11',
         '-o', helper])
command([os.environ.get('MINGW_CC', ARCH + '-w64-mingw32-clang'), '-O2', '-Wall', '-Wextra',
         '-DWINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP',
         'apps/panvk-launcher/tests/dxsmoke.c', 'apps/panvk-launcher/tests/dxsmoke_d3d8.c',
         '-o', exe, '-luser32', '-lgdi32'])
for source, dest in [(helper, 'xreadback'),
                     (exe, 'dxsmoke-synchronized.exe'),
                      (str(TOOLS / 'dxenv.sh'), 'dxenv-synchronized.sh')]:
    command(ADB + ['push', source, '/data/local/tmp/' + dest])
command(ADB + ['shell', 'chmod', '755', '/data/local/tmp/xreadback'])
shell('cp /data/local/tmp/dxsmoke-synchronized.exe ' + C + '/.wine/drive_c/dxsmoke-synchronized.exe')
shell('cp /data/local/tmp/xreadback ' + C + '/xreadback; chmod 755 ' + C + '/xreadback')
identity = shell('sha256sum ' + C + '/.wine/drive_c/windows/' +
                 ('syswow64' if ARCH == 'i686' else 'system32') +
                 '/{d3d8,d3d9,d3d10core,d3d11,dxgi}.dll; '
                 'sha256sum /data/user/0/' + APP + '/files/m4wsi/libvulkan_panfrost.so; '
                 'cat /data/user/0/' + APP + '/files/m4wsi/icd.json')
(OUT / 'identity.txt').write_bytes(identity)
(OUT / 'build-identity.json').write_text(json.dumps({
    'xreadback': hashlib.sha256(pathlib.Path(helper).read_bytes()).hexdigest(),
    'architecture': ARCH, 'api': API, 'staging': STAGING, 'sync': SYNC or 'none',
    'dxsmoke-' + ARCH + '.exe': hashlib.sha256(pathlib.Path(exe).read_bytes()).hexdigest(),
}, indent=2) + '\n')
shell('rm -f ' + C + '/synchronized-exit.txt ' + C + '/synchronized-d3d11.log ' +
      C + '/synchronized-xread.ppm*')
for stale in OUT.glob('synchronized-xread.ppm*'):
    stale.unlink()
environment = shell('DXSMOKE_HOLD=12000 sh /data/local/tmp/dxenv-synchronized.sh 0 env')
(OUT / 'environment.txt').write_bytes(environment)
command(ADB + ['shell', 'input', 'keyevent', 'KEYCODE_WAKEUP'])
command(ADB + ['shell', 'wm', 'dismiss-keyguard'])
launch = ADB + ['shell', 'run-as', APP, 'sh', '-c', shlex.quote(
    'cd ' + C + '; DXSMOKE_STAGING=' + STAGING + ' ' +
    ('DXSMOKE_SYNC=' + SYNC + ' ' if SYNC else '') +
    'DXSMOKE_HOLD=12000 sh /data/local/tmp/dxenv-synchronized.sh 0 '
    'C:\\\\dxsmoke-synchronized.exe ' + API + ' > synchronized-d3d11.log 2> synchronized-d3d11-stderr.log; '
    'echo $? > synchronized-exit.txt')]
transcript.append(shlex.join(launch))
stamp('launch')
process = subprocess.Popen(launch, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
deadline = time.monotonic() + 25
while True:
    log = shell('cat ' + C + '/synchronized-d3d11.log 2>/dev/null || true')
    present_pattern = rb' Present frame=7' if API == 'd3d11' else rb' Present'
    if re.search(rb'^DXSMOKE: api=' + API.encode() + present_pattern + rb'.*hr=0x00000000', log, re.M):
        assert b'DXSMOKE: PASS' not in log and b'outcome API=PASS' not in log, 'Present observed only after exit'
        stamp('present_success_observed')
        (OUT / 'present-observed.log').write_bytes(log)
        break
    assert process.poll() is None and time.monotonic() < deadline, log.decode(errors='replace')
    time.sleep(0.1)


def screenshot():
    stamp('screenshot_start')
    (OUT / 'screenshot.png').write_bytes(command(ADB + ['exec-out', 'screencap', '-p']))
    stamp('screenshot_end')


def readback():
    stamp('xgetimage_start')
    data = shell('sh /data/local/tmp/dxenv-synchronized.sh native ' +
                 C + '/xreadback ' + C + '/synchronized-xread.ppm')
    (OUT / 'xgetimage.log').write_bytes(data)
    stamp('xgetimage_end')


with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    futures = [pool.submit(screenshot), pool.submit(readback)]
    for future in futures:
        future.result()
assert process.poll() is None, 'Process exited before captures finished'
stamp('captures_complete_process_alive')
process.communicate(timeout=30)
stamp('process_exit_observed')
(OUT / 'd3d11.log').write_bytes(shell('cat ' + C + '/synchronized-d3d11.log'))
(OUT / 'd3d11-stderr.log').write_bytes(shell('cat ' + C + '/synchronized-d3d11-stderr.log'))
(OUT / 'exit.txt').write_bytes(shell('cat ' + C + '/synchronized-exit.txt'))
for name in shell('ls ' + C + '/synchronized-xread.ppm*').decode().splitlines():
    (OUT / pathlib.Path(name).name).write_bytes(
        command(ADB + ['exec-out', 'run-as', APP, 'cat', name]))
(OUT / 'timestamps.json').write_text(json.dumps(events, indent=2) + '\n')
(OUT / 'commands.txt').write_text('\n'.join(transcript) + '\n')
manifest = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in OUT.iterdir() if p.is_file() and p.name != 'manifest.json'}
(OUT / 'manifest.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
assert (OUT / 'exit.txt').read_text().strip() == '0'
print(json.dumps(events, indent=2))
print((OUT / 'xgetimage.log').read_text())
