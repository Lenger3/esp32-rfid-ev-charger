#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""STL dogrulama: bbox, hacim, su sizdirmazlik, ayrik govde sayisi."""
import struct
import sys
from collections import defaultdict


def read_stl(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:5] == b"solid" and b"vertex" in data[:2000]:
        import io
        return read_ascii(io.StringIO(data.decode("utf-8", "ignore")))
    n = struct.unpack("<I", data[80:84])[0]
    tris = []
    off = 84
    for _ in range(n):
        d = data[off:off + 50]
        off += 50
        if len(d) < 50:
            break
        v = struct.unpack("<12f", d[:48])
        tris.append(((v[3], v[4], v[5]), (v[6], v[7], v[8]), (v[9], v[10], v[11])))
    return tris


def read_ascii(f):
    tris, cur = [], []
    for line in f:
        s = line.strip().split()
        if s and s[0] == "vertex":
            cur.append(tuple(float(x) for x in s[1:4]))
            if len(cur) == 3:
                tris.append(tuple(cur))
                cur = []
    return tris


def volume(tris):
    vol = 0.0
    for a, b, c in tris:
        vol += (a[0] * (b[1] * c[2] - b[2] * c[1])
                - a[1] * (b[0] * c[2] - b[2] * c[0])
                + a[2] * (b[0] * c[1] - b[1] * c[0])) / 6.0
    return abs(vol)


def components(tris):
    """Ayni koseleri paylasan ucgenler ayni govdede."""
    key = lambda p: (round(p[0], 4), round(p[1], 4), round(p[2], 4))
    parent = {}

    def find(x):
        parent.setdefault(x, x)
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[rb] = ra

    for t in tris:
        a, b, c = key(t[0]), key(t[1]), key(t[2])
        union(a, b)
        union(b, c)
    return len({find(k) for k in parent})


def open_edges(tris):
    key = lambda p: (round(p[0], 4), round(p[1], 4), round(p[2], 4))
    ec = defaultdict(int)
    for t in tris:
        ks = [key(t[0]), key(t[1]), key(t[2])]
        for i in range(3):
            e = tuple(sorted((ks[i], ks[(i + 1) % 3])))
            ec[e] += 1
    return sum(1 for v in ec.values() if v != 2)


def main():
    rc = 0
    for path in sys.argv[1:]:
        tris = read_stl(path)
        xs = [p[0] for t in tris for p in t]
        ys = [p[1] for t in tris for p in t]
        zs = [p[2] for t in tris for p in t]
        bb = (max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs))
        vol = volume(tris)
        comp = components(tris)
        oe = open_edges(tris)
        print("=== %s" % path)
        print("  ucgen        : %d" % len(tris))
        print("  bbox (mm)    : %.1f x %.1f x %.1f" % bb)
        print("  hacim        : %.1f cm3" % (vol / 1000.0))
        print("  ayrik govde  : %d" % comp)
        print("  acik kenar   : %d" % oe)
        ok = oe == 0
        print("  su gecirmez  : %s" % ("EVET" if ok else "HAYIR"))
        if comp > 1:
            print("  UYARI        : %d ayrik parca (union birlestirmedi)" % comp)
            rc = 1
        if not ok:
            rc = 1
        print()
    return rc


if __name__ == "__main__":
    sys.exit(main())
