#!/usr/bin/env python3
"""Run dxdraw.exe on device via dxenv (CLI path), XGetImage the Termux:X11 client, save logs.
usage: run.py <d3d11|d3d9> <outdir> [ENV=VAL ...]  (extra env forwarded to wine)"""
import os, pathlib, subprocess, sys, time, shlex
api, out = sys.argv[1], pathlib.Path(sys.argv[2]); out.mkdir(parents=True, exist_ok=True)
extra = ' '.join(sys.argv[3:])
S = os.environ.get('ADB_SERIAL', '192.168.1.34:40501'); ADB = ['adb', '-s', S]
APP = 'dev.zenithblue.panvklauncher'; C = '/data/user/0/' + APP + '/files/container'
exe = os.environ.get('DXDRAW_EXE', '/var/tmp/panvk/draw/dxdraw-arm64ec.exe')
def run(a): return subprocess.run(a, capture_output=True).stdout
def sh(t): return run(ADB + ['shell', 'run-as', APP, 'sh', '-c', shlex.quote(t)])
run(ADB + ['push', exe, '/data/local/tmp/dxdraw.exe'])
sh('cp /data/local/tmp/dxdraw.exe ' + C + '/.wine/drive_c/dxdraw.exe; rm -f ' + C + '/dd.log ' + C + '/dd-err.log ' + C + '/dd.ppm*')
run(ADB + ['shell', 'input', 'keyevent', 'KEYCODE_WAKEUP'])
run(ADB + ['shell', 'am', 'start', '-n', 'com.termux.x11/.MainActivity'])
time.sleep(1.5)
p = subprocess.Popen(ADB + ['shell', 'run-as', APP, 'sh', '-c', shlex.quote(
    'cd ' + C + '; DXDRAW_HOLD=14000 DXVK_LOG_LEVEL=debug DXVK_LOG_PATH=' + C + '/ddlogs DXVK_HUD= ' + extra +
    ' sh /data/local/tmp/dxenv-synchronized.sh 0 C:\\\\dxdraw.exe ' + api + ' > dd.log 2> dd-err.log; echo exit=$? >> dd.log')],
    stdout=subprocess.PIPE, stderr=subprocess.PIPE)
for _ in range(120):
    if b'Present frame=7' in sh('cat ' + C + '/dd.log'): break
    time.sleep(0.25)
time.sleep(1.5)
(out / 'screenshot.png').write_bytes(run(ADB + ['exec-out', 'screencap', '-p']))
(out / 'xgetimage.log').write_bytes(sh('sh /data/local/tmp/dxenv-synchronized.sh native ' + C + '/xreadback ' + C + '/dd.ppm'))
for r in sh('ls ' + C + '/dd.ppm*').decode().split():
    (out / pathlib.Path(r).name).write_bytes(run(ADB + ['exec-out', 'run-as', APP, 'cat', r]))
p.wait(timeout=40)
(out / 'run.log').write_bytes(sh('cat ' + C + '/dd.log'))
(out / 'stderr.log').write_bytes(sh('cat ' + C + '/dd-err.log'))
for r in sh('ls ' + C + '/ddlogs 2>/dev/null').decode().split():
    (out / ('dxvk-' + r)).write_bytes(sh('cat ' + C + '/ddlogs/' + r))
sh('rm -rf ' + C + '/ddlogs')
print('done', out)
