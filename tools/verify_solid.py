#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
STL ic nokta testleri: modelin gercekten dogru katı oldugunu dogrular.
Ray casting (Moller-Trumbore) ile "nokta katı icinde mi" sorusu.

    python3 tools/verify_solid.py hardware/body_pcb.stl hardware/lid_pcb.stl
"""
import struct
import sys


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


def _ray_hits(tris, o, d):
    """Moller-Trumbore, tek yon, kesisim sayisi."""
    ox, oy, oz = o
    dx, dy, dz = d
    hits = 0
    for a, b, c in tris:
        e1 = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
        e2 = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
        h = (dy * e2[2] - dz * e2[1], dz * e2[0] - dx * e2[2], dx * e2[1] - dy * e2[0])
        det = e1[0] * h[0] + e1[1] * h[1] + e1[2] * h[2]
        if abs(det) < 1e-9:
            continue
        inv = 1.0 / det
        s_ = (ox - a[0], oy - a[1], oz - a[2])
        u = (s_[0] * h[0] + s_[1] * h[1] + s_[2] * h[2]) * inv
        if u < 1e-7 or u > 1 - 1e-7:
            continue
        q = (s_[1] * e1[2] - s_[2] * e1[1], s_[2] * e1[0] - s_[0] * e1[2],
             s_[0] * e1[1] - s_[1] * e1[0])
        v = (dx * q[0] + dy * q[1] + dz * q[2]) * inv
        if v < 1e-7 or u + v > 1 - 1e-7:
            continue
        t = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv
        if t > 1e-6:
            hits += 1
    return hits


# Kenar/kose temasinda yanlis saymamak icin 5 farkli yonde isin + cogunluk
DIRS = [(1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0),
        (0.5773, 0.5774, 0.5775), (-0.7071, 0.7072, 0.0001)]


def inside(tris, p):
    votes = sum(1 for d in DIRS if _ray_hits(tris, p, d) % 2 == 1)
    return votes >= 3


class T:
    def __init__(self):
        self.ok = 0
        self.bad = []

    def expect(self, name, got, want):
        if bool(got) == bool(want):
            self.ok += 1
            print("  [PASS] %-52s %s" % (name, "dolu" if got else "bos"))
        else:
            self.bad.append(name)
            print("  [FAIL] %-52s %s (beklenen %s)" % (name, "dolu" if got else "bos",
                                                       "dolu" if want else "bos"))


def main():
    args = sys.argv[1:]
    if len(args) < 2:
        print("kullanim: verify_solid.py body.stl lid.stl")
        return 2
    body = read_stl(args[0])
    lid = read_stl(args[1])

    t = T()

    print("GOVDE (%s)" % args[0])
    # taban
    t.expect("taban ortasi z=1.5 dolu", inside(body, (62, 62, 1.5)), True)
    t.expect("taban kenari z=1.5 dolu", inside(body, (12, 12, 1.5)), True)
    # ic bosluk
    t.expect("ic hacim ortasi z=25 bos", inside(body, (62, 62, 25)), False)
    t.expect("ic hacim z=40 bos (header ustu)", inside(body, (62, 62, 40)), False)
    # duvarlar
    t.expect("sol duvar z=25 dolu", inside(body, (1.5, 62, 25)), True)
    t.expect("sag duvar z=25 dolu", inside(body, (122.5, 62, 25)), True)
    t.expect("on duvar (-Y) z=25 dolu", inside(body, (62, 1.5, 25)), True)
    # standoff (pcb koseleri 16,16)
    t.expect("standoff halkasi z=6 dolu", inside(body, (18.2, 16, 6)), True)
    t.expect("standoff vida deligi bos", inside(body, (16, 16, 6)), False)
    t.expect("standoff disi z=6 bos", inside(body, (30, 30, 6)), False)
    # kablo cikisi (+Y duvar, x=62, z=20)
    t.expect("kablo cikisi acik (duvar icinde)", inside(body, (62, 121.5, 20)), False)
    t.expect("kablo cikisi ustu kapali", inside(body, (62, 121.5, 36)), True)
    # usb gecisi (-X duvar, y=38, z=20)
    t.expect("usb gecisi acik", inside(body, (1.5, 38, 20)), False)
    t.expect("usb gecisi disi kapali", inside(body, (1.5, 80, 20)), True)
    # 12V delik (+X duvar, y=54, z=21)
    t.expect("12V deligi acik", inside(body, (122.5, 54, 21)), False)
    # montaj kulagi (-X, x=-5)
    t.expect("montaj kulagi dolu", inside(body, (-8, 52, 2)), True)
    t.expect("montaj kulagi deligi acik", inside(body, (-5, 62, 2)), False)
    # yan havalandirma (-Y duvar, x=19, z=20)
    t.expect("yan havalandirma acik", inside(body, (19, 1.5, 20)), False)
    # govde vidalari deligi (6.75, 6.75)
    t.expect("vida deligi acik (z=30)", inside(body, (6.75, 6.75, 30)), False)
    t.expect("vida kulagi dolu (z=30, yakin)", inside(body, (6.75, 9.6, 30)), True)

    print("\nKAPAK (%s)" % args[1])
    t.expect("kapak tabani dolu (z=46.5)", inside(lid, (62, 62, 46.5)), True)
    t.expect("kapak dudagi dolu (z=43.5, dis)", inside(lid, (4.5, 62, 43.5)), True)
    t.expect("kapak dudagi ici bos (z=43.5, orta)", inside(lid, (62, 62, 43.5)), False)
    t.expect("kapak vida deligi acik", inside(lid, (6.75, 6.75, 46.5)), False)
    # kapak havalandirma kanallari: 5 kanal acik, aralari dolu
    vents_y = [19.5, 37.5, 55.5, 73.5, 91.5]
    open_n = 0
    for yv in vents_y:
        ok = not inside(lid, (62, yv + 1.5, 46.5))
        open_n += 1 if ok else 0
        t.expect("kapak kanali acik y=%.0f" % (yv + 1.5), not ok, False)
    t.expect("kapak kanal sayisi 5", open_n == 5, True)
    for yv in [28.5, 46.5, 64.5, 82.5]:
        t.expect("kanal arasi dolu y=%.0f" % yv, inside(lid, (62, yv + 1.5, 46.5)), True)

    print("\nSONUC: %d gecti, %d basarisiz" % (t.ok, len(t.bad)))
    if t.bad:
        print("basarisiz: %s" % ", ".join(t.bad))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
