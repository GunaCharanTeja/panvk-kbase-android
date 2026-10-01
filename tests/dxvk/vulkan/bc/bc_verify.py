#!/usr/bin/env python3
"""Host verification for bc_decode device dumps.

References: Pillow BCn decoder (BC1-3), Mesa CPU BPTC decoder via bc_ref
(BC6H, BC7), and a spec-direct decoder below (BC4/BC5 unorm+snorm).
usage: bc_verify.py <dump_dir> <bc_ref_binary>
"""
import struct
import subprocess
import sys

from PIL import Image

IW, IH, LAYERS = 20, 12, 2
MIPS = [(20, 12, 5, 3), (10, 6, 3, 2)]  # w, h, blocks_w, blocks_h
FMTS = ["BC1_RGB_UNORM", "BC1_RGB_SRGB", "BC1_RGBA_UNORM", "BC1_RGBA_SRGB",
        "BC2_UNORM", "BC2_SRGB", "BC3_UNORM", "BC3_SRGB", "BC4_UNORM",
        "BC4_SNORM", "BC5_UNORM", "BC5_SNORM", "BC6H_UFLOAT", "BC6H_SFLOAT",
        "BC7_UNORM", "BC7_SRGB"]


def block_bytes(name):
    return 8 if name.startswith(("BC1", "BC4")) else 16


def rgtc_channel(b, snorm):
    if snorm:
        e0 = struct.unpack("b", b[0:1])[0]
        e1 = struct.unpack("b", b[1:2])[0]
        f0, f1 = max(e0 / 127.0, -1.0), max(e1 / 127.0, -1.0)
    else:
        e0, e1 = b[0], b[1]
        f0, f1 = e0 / 255.0, e1 / 255.0
    bits = int.from_bytes(b[2:8], "little")
    out = []
    for t in range(16):
        i = (bits >> (3 * t)) & 7
        if i == 0:
            v = f0
        elif i == 1:
            v = f1
        elif e0 > e1:
            v = (f0 * (8 - i) + f1 * (i - 1)) / 7.0
        elif i == 6:
            v = -1.0 if snorm else 0.0
        elif i == 7:
            v = 1.0
        else:
            v = (f0 * (6 - i) + f1 * (i - 1)) / 5.0
        out.append(v)
    return out


def decode_layer(name, data, bw, bh, w, h, bc_ref):
    """Return list of (r,g,b,a) floats (linear for unorm, raw for sRGB
    encoded 0..1 values) for a w x h texel region."""
    bb = block_bytes(name)
    px = [[None] * (bw * 4) for _ in range(bh * 4)]
    if name.startswith(("BC1", "BC2", "BC3")):
        n = int(name[2])
        im = Image.frombytes("RGBA", (bw * 4, bh * 4), data, "bcn", n)
        d = list(im.getdata())
        for y in range(bh * 4):
            for x in range(bw * 4):
                r, g, b, a = d[y * bw * 4 + x]
                px[y][x] = [r / 255.0, g / 255.0, b / 255.0, a / 255.0]
        if name.startswith("BC1_RGB_"):
            for row in px:
                for p in row:
                    p[3] = 1.0
    elif name.startswith(("BC4", "BC5")):
        snorm = name.endswith("SNORM")
        for by in range(bh):
            for bx in range(bw):
                blk = data[(by * bw + bx) * bb:(by * bw + bx + 1) * bb]
                r = rgtc_channel(blk[0:8], snorm)
                g = rgtc_channel(blk[8:16], snorm) if bb == 16 else [0.0] * 16
                for t in range(16):
                    px[by * 4 + t // 4][bx * 4 + t % 4] = [r[t], g[t], 0.0, 1.0]
    else:
        mode = {"BC6H_UFLOAT": "bc6u", "BC6H_SFLOAT": "bc6s"}.get(name, "bc7")
        res = subprocess.run([bc_ref, mode, str(bw), str(bh)], input=data,
                             capture_output=True, check=True).stdout
        f = struct.unpack("%df" % (bw * bh * 64), res)
        for y in range(bh * 4):
            for x in range(bw * 4):
                o = (y * bw * 4 + x) * 4
                px[y][x] = list(f[o:o + 4])
    return [px[y][x] for y in range(h) for x in range(w)]


def srgb_encode(v):
    v = min(max(v, 0.0), 1.0)
    return 12.92 * v if v < 0.0031308 else 1.055 * v ** (1 / 2.4) - 0.055


def compare(name, dev, ref):
    """Max error in the unit the format is judged in."""
    worst = 0.0
    for d, r in zip(dev, ref):
        for c in range(4):
            if name.startswith("BC6H"):
                err = abs(d[c] - r[c]) / max(1.0, abs(r[c]))
            elif name.endswith("SRGB") and c < 3:
                err = abs(srgb_encode(d[c]) - r[c]) * 255.0
            elif name.endswith("SNORM"):
                err = abs(d[c] - r[c]) * 127.0
            else:
                err = abs(d[c] - r[c]) * 255.0
            worst = max(worst, err)
    return worst


def tol(name):
    return 1e-3 if name.startswith("BC6H") else 2.0


def expected(name, blocks, sub, bc_ref):
    bb = block_bytes(name)
    out = []
    off = 0
    per_mip = []
    for (w, h, bw, bh) in MIPS:
        layers = []
        for _ in range(LAYERS):
            layers.append(bytearray(blocks[off:off + bw * bh * bb]))
            off += bw * bh * bb
        per_mip.append(layers)
    if sub is not None:
        lay = per_mip[0][1]
        for sy in range(2):
            for sx in range(2):
                s = sub[(sy * 2 + sx) * bb:(sy * 2 + sx + 1) * bb]
                di = ((1 + sy) * 5 + (2 + sx)) * bb
                lay[di:di + bb] = s
    for mi, (w, h, bw, bh) in enumerate(MIPS):
        for l in range(LAYERS):
            out += decode_layer(name, bytes(per_mip[mi][l]), bw, bh, w, h,
                                bc_ref)
    return out


def load_f4(path):
    raw = open(path, "rb").read()
    f = struct.unpack("%df" % (len(raw) // 4), raw)
    return [list(f[i:i + 4]) for i in range(0, len(f), 4)]


def main():
    d, bc_ref = sys.argv[1], sys.argv[2]
    fails = 0
    for name in FMTS:
        blocks = open("%s/%s.in" % (d, name), "rb").read()
        sub = open("%s/%s.sub" % (d, name), "rb").read()
        e1 = expected(name, blocks, None, bc_ref)
        e2 = expected(name, blocks, sub, bc_ref)
        w1 = compare(name, load_f4("%s/%s.up" % (d, name)), e1)
        w2 = compare(name, load_f4("%s/%s.up2" % (d, name)), e2)
        ok = w1 <= tol(name) and w2 <= tol(name)
        fails += not ok
        print("VERIFY %s upload_maxerr=%.4g subupdate_maxerr=%.4g tol=%g %s"
              % (name, w1, w2, tol(name), "PASS" if ok else "FAIL"))
    print("BC_VERIFY_FAILS=%d" % fails)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
