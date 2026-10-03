#!/usr/bin/env python3
"""Run every Windows 3D test (dxsmoke / dxdraw / dxcube) on the device through the
launcher container (dxenv-synchronized.sh), screencap the Termux:X11 screen, crop it to
the game window, check pixels, and write results/3d-suite/<arch>-<api>-<test>.png +
results.json.

usage: 3d-suite.py [--only test,arch,api filters e.g. cube,i686] [--out DIR]
env: ADB_SERIAL, DRAW_DIR (default /var/tmp/panvk/draw), CUBE_SECS (default 20)
"""
import io, json, os, pathlib, re, shlex, subprocess, sys, time
from PIL import Image

S = os.environ.get('ADB_SERIAL', '192.168.1.34:40501'); ADB = ['adb', '-s', S]
APP = 'dev.zenithblue.panvklauncher'; C = '/data/user/0/' + APP + '/files/container'
HERE = pathlib.Path(__file__).resolve().parent
DRAW = pathlib.Path(os.environ.get('DRAW_DIR', '/var/tmp/panvk/draw'))
OUT = HERE / 'results' / '3d-suite'
CUBE_SECS = int(os.environ.get('CUBE_SECS', '20'))
TRIPLE = {'i686': 'i686-w64-mingw32', 'x86_64': 'x86_64-w64-mingw32', 'arm64ec': 'arm64ec-w64-mingw32'}

args = sys.argv[1:]
filt = []
while args:
    a = args.pop(0)
    if a == '--out': OUT = pathlib.Path(args.pop(0))
    elif a == '--only': filt = args.pop(0).split(',')
only = set(filt)
import warnings; warnings.filterwarnings('ignore')


def run(a, t=60):
    try:
        return subprocess.run(a, capture_output=True, timeout=t).stdout
    except subprocess.TimeoutExpired:
        return b''


def sh(t, tmo=60): return run(ADB + ['shell', 'run-as', APP, 'sh', '-c', shlex.quote(t)], tmo)


def kill_stale():
    out = run(ADB + ['shell', 'ps -A | grep -E "dxcube|dxdraw|dxsmoke" | grep -v " Z "']).decode()
    for l in out.splitlines():
        p = l.split()
        if len(p) > 1 and p[1].isdigit(): run(ADB + ['shell', 'run-as', APP, 'kill', '-9', p[1]])


def cases():
    for arch in ('arm64ec', 'x86_64', 'i686'):
        for api in ('d3d9', 'd3d10', 'd3d11'):
            if api != 'd3d10':
                yield dict(test='draw', arch=arch, api=api, exe=DRAW / ('dxdraw-%s.exe' % arch),
                           marker='Present frame=115', env='DXDRAW_FRAMES=120 DXDRAW_W=1280 DXDRAW_H=720 DXDRAW_HOLD=20000', tmo=60)
            yield dict(test='cube', arch=arch, api=api, exe=DRAW / ('dxcube-%s.exe' % arch),
                       marker='Present frame=5', env='DXCUBE_SECS=%d' % CUBE_SECS, tmo=CUBE_SECS + 60)
    for s in SDK:
        yield dict(test='dx8sdk-' + s, arch='i686', api='d3d8', exe=SDK_DIR / (s + '.exe'), sdk=s, marker='\0', env='', tmo=40)


SDK = ['cubemap', 'billboard', 'pointsprites', 'shadowvolume', 'dolphinvs', 'water', 'stencilmirror', 'vertexblend']
SDK_DIR = pathlib.Path('/var/tmp/panvk/dxtests/directx8/samples/multimedia/direct3d/bin')


def window_rect(xlog):
    best = None
    for m in re.finditer(r'WIN depth=\d+ id=\S+ map=1 (\d+)x(\d+)\+(-?\d+)\+(-?\d+)', xlog):
        w, h, x, y = map(int, m.groups())
        if w >= 1000 and h >= 1000 and w * h >= 1920 * 1080 - 1: continue  # root
        if w < 64 or h < 64: continue
        if best is None or w * h > best[0] * best[1]: best = (w, h, x, y)
    return best


def analyze(img):
    px = list(img.convert('RGB').getdata())
    n = len(px); nb = sum(1 for p in px if max(p) > 24)
    cols = {}
    for p in px[::7]:
        k = (p[0] >> 5, p[1] >> 5, p[2] >> 5); cols[k] = cols.get(k, 0) + 1
    return nb / n, len(cols)


def run_case(c):
    name = '%s-%s-%s' % (c['arch'], c['api'], c['test'])
    logs = OUT / 'logs' / name; logs.mkdir(parents=True, exist_ok=True)
    res = dict(name=name, test=c['test'], arch=c['arch'], api=c['api'], result='FAIL', fps=None, note='')
    if not c['exe'].exists():
        res['note'] = 'exe missing: %s' % c['exe']; return res
    kill_stale()
    base = c['exe'].name
    sdk = c.get('sdk')
    if not sdk:
        run(ADB + ['push', str(c['exe']), '/data/local/tmp/' + base])
        sh('cp /data/local/tmp/%s %s/.wine/drive_c/3dt.exe' % (base, C))
    sh('rm -f %s/s3.log %s/s3-err.log %s/s3.ppm*' % (C, C, C))
    run(ADB + ['shell', 'input', 'keyevent', 'KEYCODE_WAKEUP'])
    run(ADB + ['shell', 'am', 'start', '-n', 'com.termux.x11/.MainActivity']); time.sleep(3)
    api_arg = c['api']
    target = ('start /d C:\\\\dx8m\\\\direct3d\\\\bin %s.exe' % sdk) if sdk else ('C:\\\\3dt.exe ' + api_arg)
    p = subprocess.Popen(ADB + ['shell', 'run-as', APP, 'sh', '-c', shlex.quote(
        'cd %s; %s DXVK_HUD=full sh /data/local/tmp/dxenv-synchronized.sh 0 %s > s3.log 2> s3-err.log; echo exit=$? >> s3.log'
        % (C, c['env'], target))], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    t0 = time.time(); got = False
    if sdk: time.sleep(16)
    while not sdk and time.time() - t0 < 50:
        lg = sh('cat %s/s3.log' % C).decode(errors='replace')
        if c['marker'] in lg or 'FAIL' in lg or 'exit=' in lg: got = True; break
        time.sleep(0.5)
    time.sleep(2.0)
    for _ in range(4):
        shot = run(ADB + ['exec-out', 'screencap', '-p'])
        if Image.open(io.BytesIO(shot)).size[0] >= 1500: break
        run(ADB + ['shell', 'am', 'start', '-n', 'com.termux.x11/.MainActivity']); time.sleep(2.5)
    xlog = sh('sh /data/local/tmp/dxenv-synchronized.sh native %s/xreadback %s/s3.ppm' % (C, C)).decode(errors='replace')
    if sdk:
        for l in run(ADB + ['shell', 'ps -A | grep -i %s | grep -v " Z "' % sdk]).decode().splitlines():
            if l.split()[1].isdigit(): run(ADB + ['shell', 'run-as', APP, 'kill', '-9', l.split()[1]])
    try: p.wait(timeout=c['tmo'])
    except subprocess.TimeoutExpired: p.kill(); kill_stale()
    lg = sh('cat %s/s3.log' % C).decode(errors='replace')
    err = sh('cat %s/s3-err.log' % C).decode(errors='replace')
    (logs / 'run.log').write_text(lg); (logs / 'xgetimage.log').write_text(xlog)
    (logs / 'stderr.log').write_text('\n'.join(l for l in err.splitlines() if '_r_debug' not in l and 'libc:' not in l))
    (logs / 'screencap-full.png').write_bytes(shot)
    ok = True if sdk else (('PASS' in lg) and 'exit=0' in lg)
    res['errors'] = len(re.findall(r'MESA: error|VK_ERROR|ERROR', err))
    m = re.search(r'frames=(\d+)', lg)
    if m and c['test'] == 'cube': res['fps'] = round(int(m.group(1)) / CUBE_SECS, 1)
    try:
        img = Image.open(io.BytesIO(shot)).convert('RGB')
        r = window_rect(xlog)
        crop = img.crop((r[2], r[3], r[2] + r[0], r[3] + r[1])) if r else img
        frac, ncol = analyze(crop); src = 'screencap'
        # XGetImage of the client window (authoritative pixels; the screencap can lag for static tests)
        best = None
        for m in re.finditer(r'WIN depth=\d+ id=(\S+) map=1 (\d+)x(\d+)\+', xlog):
            w, h = int(m.group(2)), int(m.group(3))
            if w * h >= 1920 * 1080 - 1 or w < 64 or h < 64: continue
            if best is None or w * h >= best[1]: best = (m.group(1), w * h)
        xi = None
        if best:
            raw = run(ADB + ['exec-out', 'run-as', APP, 'cat', '%s/s3.ppm-%s.ppm' % (C, best[0])])
            try: xi = Image.open(io.BytesIO(raw)).convert('RGB'); xi.save(logs / 'xgetimage-client.png')
            except Exception: xi = None
        xf, xc = analyze(xi) if xi else (0, 0)
        if (frac < 0.05 or ncol < 2) and xi and xf >= 0.05 and xc >= 1:
            crop, frac, ncol, src = xi, xf, xc, 'XGetImage'
        res['nonblack'] = round(frac, 3); res['colors'] = ncol; res['window'] = list(r) if r else None; res['image'] = src
        res['xgetimage_nonblack'] = round(xf, 3)
        crop.save(OUT / (name + '.png'))
        if ok and (frac < 0.05 or ncol < (8 if sdk else 2)): ok = False; res['note'] = 'black/blank'
    except Exception as e:
        res['note'] = 'screenshot: %r' % e; ok = False
    res['result'] = 'PASS' if ok else 'FAIL'
    if not ok and not res['note']: res['note'] = (lg.strip().splitlines() or ['no log'])[-2:].__repr__()[:200]
    return res


OUT.mkdir(parents=True, exist_ok=True)
results = []
for c in cases():
    if only and not only <= {c['test'], c['arch'], c['api']}: continue
    r = run_case(c); results.append(r)
    print(r['name'], r['result'], r.get('fps'), r.get('nonblack'), r['note'], flush=True)
rf = OUT / 'results.json'
old = {}
if rf.exists(): old = {x['name']: x for x in json.loads(rf.read_text())}
for r in results: old[r['name']] = r
rf.write_text(json.dumps(sorted(old.values(), key=lambda x: x['name']), indent=1))
