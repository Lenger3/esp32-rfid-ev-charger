#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
STL -> PNG (saf stdlib, painter's algorithm + duz golgeleme).
3 ortografik gorunus yan yana. Gozle kontrol icin.

    python3 tools/render_stl.py out.png a.stl b.stl ...
"""
import struct
import sys
import zlib


def read_stl(path):
    data = open(path, "rb").read()
    tris = []
    if data[:5] == b"solid" and b"vertex" in data[:2000]:
        cur = []
        for line in data.decode("utf-8", "ignore").splitlines():
            s = line.split()
            if s and s[0] == "vertex":
                cur.append(tuple(float(x) for x in s[1:4]))
                if len(cur) == 3:
                    tris.append(tuple(cur))
                    cur = []
        return tris
    n = struct.unpack("<I", data[80:84])[0]
    off = 84
    for _ in range(n):
        d = data[off:off + 50]
        off += 50
        if len(d) < 50:
            break
        v = struct.unpack("<12f", d[:48])
        tris.append(((v[3], v[4], v[5]), (v[6], v[7], v[8]), (v[9], v[10], v[11])))
    return tris


def normal(a, b, c):
    ux, uy, uz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    vx, vy, vz = c[0] - a[0], c[1] - a[1], c[2] - a[2]
    nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
    ln = (nx * nx + ny * ny + nz * nz) ** 0.5 or 1.0
    return nx / ln, ny / ln, nz / ln


def render(tris, size=460, yaw=35.0, pitch=22.0, pad=14):
    import math
    cy, sy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    cp, sp = math.cos(math.radians(pitch)), math.sin(math.radians(pitch))

    pts = [p for t in tris for p in t]
    xs, ys, zs = [p[0] for p in pts], [p[1] for p in pts], [p[2] for p in pts]
    cx, cyy, cz = (max(xs) + min(xs)) / 2, (max(ys) + min(ys)) / 2, (max(zs) + min(zs)) / 2

    def proj(p):
        x, y, z = p[0] - cx, p[1] - cyy, p[2] - cz
        x, z = x * cy + z * sy, -x * sy + z * cy
        y, z = y * cp - z * sp, y * sp + z * cp
        return x, y, z

    pp = [proj(p) for p in pts]
    px, py, pz = [p[0] for p in pp], [p[1] for p in pp], [p[2] for p in pp]
    span = max(max(px) - min(px), max(py) - min(py)) or 1.0
    scale = (size - 2 * pad) / span
    ox = (max(px) + min(px)) / 2
    oy = (max(py) + min(py)) / 2

    W = H = size
    zbuf = [-1e9] * (W * H)
    img = [[(28, 30, 34)] * W for _ in range(H)]

    lx, ly, lz = 0.35, -0.55, 0.76
    lnum = (lx * lx + ly * ly + lz * lz) ** 0.5
    lx, ly, lz = lx / lnum, ly / lnum, lz / lnum

    faces = []
    for t in tris:
        a, b, c = proj(t[0]), proj(t[1]), proj(t[2])
        n = normal(a, b, c)
        depth = (a[2] + b[2] + c[2]) / 3.0
        faces.append((depth, a, b, c, n))
    faces.sort(key=lambda f: f[0])

    for depth, a, b, c, n in faces:
        sh = abs(n[0] * lx + n[1] * ly + n[2] * lz)
        k = 0.30 + 0.70 * sh
        base = (120, 180, 235) if depth > -1e8 else (200, 200, 200)
        col = (int(base[0] * k), int(base[1] * k), int(base[2] * k))
        X = [(p[0] - ox) * scale + W / 2 for p in (a, b, c)]
        Y = [-(p[1] - oy) * scale + H / 2 for p in (a, b, c)]
        Z = [p[2] for p in (a, b, c)]
        minx, maxx = int(max(0, min(X))), int(min(W - 1, max(X) + 1))
        miny, maxy = int(max(0, min(Y))), int(min(H - 1, max(Y) + 1))
        x0, y0, x1, y1 = X[0], Y[0], X[1], Y[1]
        x2, y2 = X[2], Y[2]
        den = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
        if abs(den) < 1e-9:
            continue
        for py_ in range(miny, maxy + 1):
            for px_ in range(minx, maxx + 1):
                w0 = ((y1 - y2) * (px_ - x2) + (x2 - x1) * (py_ - y2)) / den
                w1 = ((y2 - y0) * (px_ - x2) + (x0 - x2) * (py_ - y2)) / den
                w2 = 1.0 - w0 - w1
                if w0 < 0 or w1 < 0 or w2 < 0:
                    continue
                z = w0 * Z[0] + w1 * Z[1] + w2 * Z[2]
                idx = py_ * W + px_
                if z > zbuf[idx]:
                    zbuf[idx] = z
                    img[py_][px_] = col
    return img


def write_png(path, img):
    H = len(img)
    W = len(img[0])
    raw = b"".join(b"\x00" + b"".join(bytes(px) for px in row) for row in img)
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 6))
    png += chunk(b"IEND", b"")
    open(path, "wb").write(png)


def main():
    if len(sys.argv) < 3:
        print("kullanim: render_stl.py out.png a.stl [b.stl ...]")
        return 2
    out = sys.argv[1]
    imgs = []
    for p in sys.argv[2:]:
        tris = read_stl(p)
        if not tris:
            print("bos: %s" % p)
            continue
        imgs.append(render(tris))
        print("%s -> %d ucgen" % (p, len(tris)))
    if not imgs:
        return 1
    total_w = sum(len(i[0]) for i in imgs) + 12 * (len(imgs) - 1)
    H = max(len(i) for i in imgs)
    canvas = [[(18, 18, 20)] * total_w for _ in range(H)]
    xoff = 0
    for im in imgs:
        for y in range(len(im)):
            canvas[y][xoff:xoff + len(im[0])] = im[y]
        xoff += len(im[0]) + 12
    write_png(out, canvas)
    print("yazildi: %s (%dx%d)" % (out, total_w, H))
    return 0


if __name__ == "__main__":
    sys.exit(main())
