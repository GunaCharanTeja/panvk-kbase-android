#!/usr/bin/env python3
"""Host helper for the PanVK Launcher game shortcuts (see docs/GAME-SHORTCUTS.md).

  launch-shortcut.py --list
  launch-shortcut.py --add /path/on/device/game.exe [--name N] [--args "-w"] [--env K=V]... [--res 1280x720] [--driver ID] [--fex MODE] [--arch i386|x86_64|arm64ec]
  launch-shortcut.py --launch NAME_OR_ID [--wait 20] [--screenshot out.png] [--logs [DIR]] [--force-stop]
  launch-shortcut.py --delete NAME_OR_ID

Serial: -s SERIAL or $ADB_SERIAL (default 192.168.1.34:40501). Needs a debug APK (run-as + intent gate).
"""
import argparse, json, os, pathlib, shlex, struct, subprocess, sys, time, uuid

APP = 'dev.zenithblue.panvklauncher'
EXTRA = 'dev.zenithblue.panvklauncher.LAUNCH_SHORTCUT'
MACHINES = {0x14c: 'i386', 0x8664: 'x86_64', 0xA641: 'arm64ec', 0xAA64: 'arm64'}


def adb(*a, input=None, timeout=60):
    return subprocess.run(['adb', '-s', SERIAL, *a], input=input, capture_output=True, timeout=timeout)


def runas(script, input=None):
    return adb('shell', 'run-as', APP, 'sh', '-c', shlex.quote(script), input=input).stdout


def list_shortcuts():
    out = runas('cd files/shortcuts 2>/dev/null && for f in *.json; do echo "@@$f"; cat "$f"; echo; done').decode(errors='replace')
    res = []
    for chunk in out.split('@@')[1:]:
        _, _, body = chunk.partition('\n')
        try:
            res.append(json.loads(body))
        except ValueError:
            pass
    return sorted(res, key=lambda s: s.get('name', '').lower())


def find(key):
    al = list_shortcuts()
    for pred in (lambda s: s['id'] == key, lambda s: s.get('name', '').lower() == key.lower(),
                 lambda s: s.get('name', '').lower().startswith(key.lower())):
        m = [s for s in al if pred(s)]
        if len(m) == 1:
            return m[0]
        if len(m) > 1:
            sys.exit('ambiguous: ' + ', '.join(s['name'] for s in m))
    sys.exit('no such shortcut: %s (try --list)' % key)


def detect_arch(exe):
    hdr = adb('exec-out', 'head', '-c', '4096', exe).stdout
    try:
        off = struct.unpack_from('<I', hdr, 0x3C)[0]
        if hdr[off:off + 4] != b'PE\0\0':
            return 'auto'
        machine = struct.unpack_from('<H', hdr, off + 4)[0]
        if machine != 0x8664:
            return MACHINES.get(machine, 'auto')
        # AMD64 machine id is shared by x86_64 and ARM64EC; ARM64EC has a CHPE metadata ptr (load config +200).
        nsec, optsz = struct.unpack_from('<H', hdr, off + 6)[0], struct.unpack_from('<H', hdr, off + 20)[0]
        lc_rva = struct.unpack_from('<I', hdr, off + 24 + 112 + 80)[0]
        sec = off + 24 + optsz
        for i in range(nsec):
            vsz, va, rsz, raw = struct.unpack_from('<IIII', hdr, sec + i * 40 + 8)
            if lc_rva and va <= lc_rva < va + max(vsz, rsz):
                fo = raw + lc_rva - va
                lc = adb('exec-out', 'tail -c +%d %s | head -c 208' % (fo + 1, shlex.quote(exe))).stdout
                if len(lc) >= 208 and struct.unpack_from('<I', lc, 0)[0] >= 208 and struct.unpack_from('<Q', lc, 200)[0]:
                    return 'arm64ec'
        return 'x86_64'
    except struct.error:
        pass
    return 'auto'


def write_json(sc):
    runas('mkdir -p files/shortcuts; cat > files/shortcuts/%s.json' % sc['id'], input=json.dumps(sc, indent=2).encode())


def cmd_add(a):
    name = a.name or pathlib.PurePosixPath(a.add.replace('\\', '/')).stem
    env = dict(e.split('=', 1) for e in a.env) if a.env else {'DXVK_HUD': 'full'}
    old = [s for s in list_shortcuts() if s.get('name', '').lower() == name.lower()]
    sc = old[0] if old else {'id': 'g' + uuid.uuid4().hex[:8], 'created': int(time.time() * 1000), 'lastPlayed': 0}
    sc.update(name=name, exe=a.add, args=a.args or '', env=env, arch=a.arch or detect_arch(a.add),
              resolution=a.res or '', driver=a.driver or '', fex=a.fex or '', icon='auto')  # icon filled in by the app
    write_json(sc)
    print('%s %s  %s  arch=%s' % ('updated' if old else 'added', sc['id'], name, sc['arch']))


def cmd_launch(a):
    sc = find(a.launch)
    if a.force_stop:
        adb('shell', 'am', 'force-stop', APP)
        time.sleep(1)
    adb('shell', 'logcat', '-c')
    r = adb('shell', 'am', 'start', '-n', APP + '/.MainActivity', '--es', EXTRA, sc['id'])
    print((r.stdout + r.stderr).decode(errors='replace').strip().splitlines()[-1:] or r.returncode)
    print('launched %s (%s)' % (sc['name'], sc['id']))
    if a.wait:
        time.sleep(a.wait)
    if a.screenshot:
        png = adb('exec-out', 'screencap', '-p').stdout
        pathlib.Path(a.screenshot).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(a.screenshot).write_bytes(png)
        print('screenshot %s (%d bytes)' % (a.screenshot, len(png)))
    if a.logs is not None:
        pull_logs(sc, pathlib.Path(a.logs))


def pull_logs(sc, out):
    out.mkdir(parents=True, exist_ok=True)
    name = runas('ls -t files/logs/run-*.log 2>/dev/null | head -1').decode().strip()
    if name:
        data = runas('cat %s' % shlex.quote(name))
        (out / 'wine-run.log').write_bytes(data)
        print('wine log -> %s/wine-run.log (%d bytes), tail:' % (out, len(data)))
        print('\n'.join(data.decode(errors='replace').splitlines()[-12:]))
    # DXVK writes <exe>_<dll>.log into the exe's working dir; app-private paths need run-as, others plain shell.
    d = os.path.dirname(sc['exe'].replace('\\', '/'))
    if d:
        for f in runas('ls -t %s/*_d3d*.log %s/*_dxgi.log 2>/dev/null | head -4' % (shlex.quote(d), shlex.quote(d))).decode().splitlines():
            data = runas('cat %s' % shlex.quote(f)) or adb('exec-out', 'cat ' + shlex.quote(f)).stdout
            (out / os.path.basename(f)).write_bytes(data)
            print('dxvk log -> %s/%s (%d bytes)' % (out, os.path.basename(f), len(data)))
    pid = adb('shell', 'pidof', APP).stdout.decode().split()
    lc = adb('shell', 'logcat', '-d', '-t', '2000', *(['--pid=' + pid[0]] if pid else [])).stdout
    (out / 'logcat-launcher.txt').write_bytes(lc)


def main():
    global SERIAL
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('-s', '--serial', default=os.environ.get('ADB_SERIAL', '192.168.1.34:40501'))
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument('--list', action='store_true')
    g.add_argument('--add', metavar='EXE')
    g.add_argument('--launch', metavar='NAME|ID')
    g.add_argument('--delete', metavar='NAME|ID')
    p.add_argument('--name'); p.add_argument('--args'); p.add_argument('--env', action='append')
    p.add_argument('--res'); p.add_argument('--driver'); p.add_argument('--arch')
    p.add_argument('--fex', help='FEX preset: Stability|Compatibility|Intermediate|Performance|Extreme|Denuvo')
    p.add_argument('--wait', type=int, default=0, help='seconds to wait after launch')
    p.add_argument('--screenshot', metavar='PNG')
    p.add_argument('--logs', nargs='?', const='logs-out', metavar='DIR', help='pull wine + DXVK logs after launch')
    p.add_argument('--force-stop', action='store_true', help='am force-stop the app first (kills a running game)')
    a = p.parse_args()
    SERIAL = a.serial
    if a.list:
        for s in list_shortcuts():
            print('%s  %-28s %-8s %-9s %s %s' % (s['id'], s['name'], s.get('arch', ''), s.get('resolution') or '-', s['exe'], s.get('args', '')))
    elif a.add:
        cmd_add(a)
    elif a.launch:
        cmd_launch(a)
    elif a.delete:
        sc = find(a.delete)
        runas('rm -f files/shortcuts/%s.json files/shortcuts/%s.png' % (sc['id'], sc['id']))
        print('deleted', sc['id'], sc['name'])


if __name__ == '__main__':
    main()
