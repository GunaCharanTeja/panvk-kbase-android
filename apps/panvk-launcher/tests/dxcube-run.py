#!/usr/bin/env python3
"""Run dxcube.exe on device via dxenv (like results/draw-test/run.py), screencap + XGetImage readback.
usage: dxcube-run.py <d3d11|d3d9> <outdir> [ENV=VAL ...]
env: DXCUBE_EXE (host path, default /var/tmp/panvk/draw/dxcube-arm64ec.exe), ADB_SERIAL, SHOT_AT (frame to wait for, default 5)"""
import os, pathlib, subprocess, sys, time, shlex
api, out = sys.argv[1], pathlib.Path(sys.argv[2]); out.mkdir(parents=True, exist_ok=True)
extra = ' '.join(sys.argv[3:])
S = os.environ.get('ADB_SERIAL', '192.168.1.34:40501'); ADB = ['adb', '-s', S]
APP = 'dev.zenithblue.panvklauncher'; C = '/data/user/0/' + APP + '/files/container'
exe = os.environ.get('DXCUBE_EXE', '/var/tmp/panvk/draw/dxcube-arm64ec.exe')
at = os.environ.get('SHOT_AT', '5')
def run(a): return subprocess.run(a, capture_output=True).stdout
def sh(t): return run(ADB + ['shell', 'run-as', APP, 'sh', '-c', shlex.quote(t)])
run(ADB + ['push', exe, '/data/local/tmp/dxcube.exe'])
sh('cp /data/local/tmp/dxcube.exe ' + C + '/.wine/drive_c/dxcube.exe; rm -f ' + C + '/dc.log ' + C + '/dc-err.log ' + C + '/dc.ppm*')
run(ADB + ['shell', 'input', 'keyevent', 'KEYCODE_WAKEUP'])
run(ADB + ['shell', 'am', 'start', '-n', 'com.termux.x11/.MainActivity'])
time.sleep(1.5)
p = subprocess.Popen(ADB + ['shell', 'run-as', APP, 'sh', '-c', shlex.quote(
    'cd ' + C + '; DXCUBE_SECS=25 ' + extra +
    ' sh /data/local/tmp/dxenv-synchronized.sh 0 C:\\\\dxcube.exe ' + api + ' > dc.log 2> dc-err.log; echo exit=$? >> dc.log')],
    stdout=subprocess.PIPE, stderr=subprocess.PIPE)
for _ in range(160):
    if ('Present frame=' + at).encode() in sh('cat ' + C + '/dc.log'): break
    time.sleep(0.25)
time.sleep(1.0)
(out / 'screenshot.png').write_bytes(run(ADB + ['exec-out', 'screencap', '-p']))
(out / 'xgetimage.log').write_bytes(sh('sh /data/local/tmp/dxenv-synchronized.sh native ' + C + '/xreadback ' + C + '/dc.ppm'))
for r in sh('ls ' + C + '/dc.ppm*').decode().split():
    (out / pathlib.Path(r).name).write_bytes(run(ADB + ['exec-out', 'run-as', APP, 'cat', r]))
p.wait(timeout=60)
(out / 'run.log').write_bytes(sh('cat ' + C + '/dc.log'))
(out / 'stderr.log').write_bytes(sh('cat ' + C + '/dc-err.log'))
print('done', out)
