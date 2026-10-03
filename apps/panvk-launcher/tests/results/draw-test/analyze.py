#!/usr/bin/env python3
"""Classify each P6 .ppm in a dir: counts of bg(26,51,153), orange, black, other + sample points."""
import sys, pathlib
for p in sorted(pathlib.Path(sys.argv[1]).glob('*.ppm*')):
    d = p.read_bytes()
    try:
        hdr, px = d.split(b'\n255\n', 1); w, h = map(int, hdr.split()[-2:])
    except Exception: continue
    n = w * h; c = {'bg': 0, 'orange': 0, 'black': 0, 'other': 0}
    for i in range(0, n * 3, 3):
        r, g, b = px[i], px[i+1], px[i+2]
        if abs(r-26) < 5 and abs(g-51) < 5 and abs(b-153) < 5: c['bg'] += 1
        elif abs(r-255) < 8 and abs(g-128) < 12 and abs(b-64) < 12: c['orange'] += 1
        elif r+g+b == 0: c['black'] += 1
        else: c['other'] += 1
    def at(x, y): i = (y*w+x)*3; return tuple(px[i:i+3])
    s = {k: at(*v) for k, v in dict(tl=(5,5), tri=(80,130), qa=(215,90), qb=(245,90)).items()} if w >= 250 and h >= 150 else {}
    print(p.name, w, h, c, s)
