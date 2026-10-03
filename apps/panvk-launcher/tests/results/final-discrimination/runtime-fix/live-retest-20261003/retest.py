#!/usr/bin/env python3
"""Live retest: launcher UI Run button -> Termux:X11 client pixels (XGetImage) + screenshot."""
import json, os, pathlib, re, subprocess, sys, time, xml.etree.ElementTree as ET
out0 = pathlib.Path(__file__).resolve().parent
S = os.environ.get('ADB_SERIAL', '192.168.1.34:40501')
adb = ['adb', '-s', S]
app = 'dev.zenithblue.panvklauncher'
f = '/data/user/0/' + app + '/files'
c = f + '/container'
def run(a): return subprocess.check_output(a)
def sh(t): return run(adb + ['shell', 'run-as', app, 'sh', '-c', "'" + t + "'"])
def tap(x, y): run(adb + ['shell', 'input', 'tap', str(x), str(y)])
def swipe(a, b): run(adb + ['shell', 'input', 'swipe', '540', str(a), '540', str(b), '350'])
def dump():
    run(adb + ['shell', 'uiautomator', 'dump', '/sdcard/ui.xml'])
    return ET.fromstring(run(adb + ['exec-out', 'cat', '/sdcard/ui.xml']))
def bounds(n):
    m = re.findall(r'\d+', n.get('bounds')); return list(map(int, m))
def find_run(name):
    root = dump(); nodes = list(root.iter('node'))
    t = [bounds(n) for n in nodes if n.get('text') == name or n.get('content-desc') == name]
    r = [bounds(n) for n in nodes if (n.get('text') == 'Run')]
    if not t or not r: return None
    ty = (t[0][1] + t[0][3]) // 2
    # Run button in same row: nearest in y within card height
    best = min(r, key=lambda b: abs((b[1] + b[3]) // 2 - (ty + 100)))
    if abs((best[1] + best[3]) // 2 - ty) > 400: return None
    return (best[0] + best[2]) // 2, (best[1] + best[3]) // 2
def orange_frac_ppm(p):
    h, px = p.read_bytes().split(b'\n255\n', 1)
    n = len(px) // 3; o = sum(1 for i in range(0, len(px), 3) if abs(px[i]-255) < 8 and abs(px[i+1]-128) < 12 and abs(px[i+2]-64) < 12)
    return h.split(b'\n')[-1].decode(), o, n
def orange_frac_png(p):
    from PIL import Image
    im = Image.open(p).convert('RGB'); w, h = im.size
    top = 130  # exclude status bar/battery overlay band
    px = im.load(); o = t = 0
    for y in range(top, h - 140, 2):
        for x in range(0, w, 2):
            r, g, b = px[x, y]; t += 1
            if abs(r-255) < 12 and abs(g-128) < 16 and abs(b-64) < 16: o += 1
    return o, t
SEQ = {'d3d11': ([(400,1400)], (886,993)), 'd3d10': ([(400,1400)], (886,525)),
  'd3d9': ([(400,1400),(400,1000)], (886,713)), 'd3d8': ([(400,1400),(400,1000),(400,1000)], (886,713))}
apis = sys.argv[1:] or ['d3d8', 'd3d9', 'd3d10', 'd3d11']
summary = {}
for api in apis:
    d = out0 / (api + os.environ.get('SUF','')); d.mkdir(exist_ok=True)
    name = f'dxsmoke-{api}-arm64ec-w64-mingw32.exe'
    # screen already positioned by operator (verified screenshot); tap given coords
    pos = (int(os.environ['TX']), int(os.environ['TY']))
    (d / 'pre-tap.png').write_bytes(run(adb + ['exec-out', 'screencap', '-p']))
    old = set(sh('ls ' + f + '/logs/run-*.log').decode().split())
    t0 = time.time(); tap(*pos)
    log = b''; nm = None
    while time.time() - t0 < 25:
        new = set(sh('ls ' + f + '/logs/run-*.log').decode().split()) - old
        if new:
            nm = sorted(new)[-1]; log = sh('cat ' + nm)
            if b'Present' in log: break
        time.sleep(0.2)
    time.sleep(1.5)
    run(adb + ['shell', 'am', 'start', '-n', 'com.termux.x11/.MainActivity']); time.sleep(1.5)
    (d / 'screenshot.png').write_bytes(run(adb + ['exec-out', 'screencap', '-p']))
    sh('rm -f ' + c + '/lr.ppm*')
    xl = sh('sh /data/local/tmp/dxenv-synchronized.sh native ' + c + '/xreadback ' + c + '/lr.ppm')
    (d / 'xgetimage.log').write_bytes(xl)
    res = []
    for remote in sh('ls ' + c + '/lr.ppm*').decode().split():
        p = d / pathlib.Path(remote).name
        p.write_bytes(run(adb + ['exec-out', 'run-as', app, 'cat', remote]))
        res.append((p.name,) + orange_frac_ppm(p))
    time.sleep(14)
    log = sh('cat ' + nm) if nm else log
    (d / 'run.log').write_bytes(log)
    try: so, st = orange_frac_png(d / 'screenshot.png')
    except Exception as e: so, st = 0, str(e)
    if name.encode() not in log and api.encode() not in log: summary[api+'_warn'] = 'run log lacks api name'
    ex = re.findall(rb'exit=(\d+)', log)
    summary[api] = {'ppm': res, 'screenshot_orange_sampled': [so, st], 'exit': ex[-1].decode() if ex else None,
                    'present': re.findall(rb'Present[^\n]*', log)[:3] and [x.decode() for x in re.findall(rb'Present[^\n]*', log)[:2]]}
    print(api, json.dumps(summary[api]))
(out0 / ('summary-' + '-'.join(apis) + os.environ.get('SUF','') + '.json')).write_text(json.dumps(summary, indent=1))
