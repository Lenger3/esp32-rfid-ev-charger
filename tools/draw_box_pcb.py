#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Plaket kutusu teknik resmi (3 gorunus + olculer) -> hardware/box_pcb_drawing.svg
Saf stdlib; olculer tools/gen_box_pcb.py parametrelerinden gelir.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gen_box_pcb as G  # noqa: E402

P = G.P
g = G.geom()
W, HD = G.W, G.Hd
BH = G.BODY_H
LT = P["lid_t"]
OUT = os.path.join(os.path.dirname(HERE), "hardware", "box_pcb_drawing.svg")

S = 3.0          # px/mm (ekranda okunur)
MARGIN = 40
GAP = 55


def esc(t):
    return t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def rect(x, y, w, h, fill="none", stroke="#000", sw=0.4, rx=0):
    return ('<rect x="%.2f" y="%.2f" width="%.2f" height="%.2f" fill="%s" stroke="%s" '
            'stroke-width="%.2f" rx="%.2f"/>' % (x, y, w, h, fill, stroke, sw, rx))


def circ(cx, cy, r, fill="none", stroke="#000", sw=0.4):
    return '<circle cx="%.2f" cy="%.2f" r="%.2f" fill="%s" stroke="%s" stroke-width="%.2f"/>' % (
        cx, cy, r, fill, stroke, sw)


def line(x1, y1, x2, y2, stroke="#000", sw=0.3, dash=""):
    d = ' stroke-dasharray="%s"' % dash if dash else ""
    return ('<line x1="%.2f" y1="%.2f" x2="%.2f" y2="%.2f" stroke="%s" stroke-width="%.2f"%s/>'
            % (x1, y1, x2, y2, stroke, sw, d))


def txt(x, y, s, size=8, anchor="middle", fill="#000", weight="normal"):
    return ('<text x="%.2f" y="%.2f" font-family="Helvetica,Arial" font-size="%.1f" '
            'text-anchor="%s" fill="%s" font-weight="%s">%s</text>'
            % (x, y, size, anchor, fill, weight, esc(s)))


def dim_h(x1, x2, y, label):
    """yatay olcu cizgisi"""
    return (line(x1, y, x2, y) + line(x1, y - 4, x1, y + 4) + line(x2, y - 4, x2, y + 4)
            + txt((x1 + x2) / 2, y - 4, label, 8))


def dim_v(y1, y2, x, label):
    return (line(x, y1, x, y2) + line(x - 4, y1, x + 4, y1) + line(x - 4, y2, x + 4, y2)
            + ('<text x="%.2f" y="%.2f" font-family="Helvetica,Arial" font-size="8" fill="#000" '
               'text-anchor="middle" transform="rotate(-90 %.2f %.2f)">%s</text>'
               % (x - 4, (y1 + y2) / 2, x - 4, (y1 + y2) / 2, esc(label))))


parts = []

# ---------------------------------------------------------------- UST GORUNUS (XY)
ox, oy = MARGIN + P["tab_d"] * S, MARGIN
X = lambda v: ox + v * S
Y = lambda v: oy + v * S

parts.append(txt(X(W / 2), oy - 22, "UST GORUNUS (kapak cikarilmis)", 10, weight="bold"))
parts.append(rect(X(0), Y(0), W * S, HD * S, fill="#f4f4f4"))
parts.append(rect(X(P["wall"]), Y(P["wall"]), P["interior_w"] * S, P["interior_h"] * S,
                  fill="#ffffff"))
# plaket
parts.append(rect(X(g["pcb_x0"]), Y(g["pcb_y0"]), P["pcb_w"] * S, P["pcb_h"] * S,
                  fill="#e8eef7", stroke="#2b5", sw=0.6))
parts.append(txt(X(W / 2), Y(g["pcb_y0"] + P["pcb_h"] / 2), "PLAKET 100x100", 9, fill="#187"))
# standofflar
for (sx, sy) in g["standoff_xy"]:
    parts.append(circ(X(sx), Y(sy), P["standoff_d"] / 2 * S, fill="#9d9", stroke="#373"))
# govde vida kulaklari
for (bx, by) in g["boss_xy"]:
    parts.append(circ(X(bx), Y(by), P["boss_d"] / 2 * S, fill="#bbb", stroke="#333"))
    parts.append(circ(X(bx), Y(by), P["boss_hole"] / 2 * S, fill="#fff", stroke="#333"))
# kablo cikisi (+Y)
parts.append(rect(X(W / 2 - P["cable_slot_w"] / 2), Y(HD - P["wall"]),
                  P["cable_slot_w"] * S, P["wall"] * S, fill="#fdd", stroke="#c00"))
# usb (-X)
parts.append(rect(X(0), Y(P["usb_open_cy"] - P["usb_open_w"] / 2),
                  P["wall"] * S, P["usb_open_w"] * S, fill="#fdd", stroke="#c00"))
# 12V (+X)
parts.append(circ(X(W - P["wall"] / 2), Y(P["psu_hole_cy"]), P["psu_hole_d"] / 2 * S,
                  fill="#fdd", stroke="#c00"))
# yan havalandirma (-Y)
for i in range(5):
    xv = P["wall"] + P["vent_x_off"] + i * P["vent_gap"]
    parts.append(rect(X(xv - P["vent_w"] / 2), Y(0), P["vent_w"] * S, P["wall"] * S,
                      fill="#def", stroke="#08c"))
# montaj kulaklari (-X / +X)
parts.append(rect(X(-P["tab_d"]), Y((HD - P["tab_w"]) / 2), P["tab_d"] * S, P["tab_w"] * S,
                  fill="#eee", stroke="#666", sw=0.5))
parts.append(circ(X(-P["tab_d"] / 2), Y(HD / 2), P["tab_hole"] / 2 * S, fill="#fff", stroke="#666"))
parts.append(rect(X(W), Y((HD - P["tab_w"]) / 2), P["tab_d"] * S, P["tab_w"] * S,
                  fill="#eee", stroke="#666", sw=0.5))
parts.append(circ(X(W + P["tab_d"] / 2), Y(HD / 2), P["tab_hole"] / 2 * S, fill="#fff", stroke="#666"))

parts.append(dim_h(X(-P["tab_d"]), X(W + P["tab_d"]), Y(HD) + 26,
                   "%g mm (kulaklar dahil)" % (W + 2 * P["tab_d"])))
parts.append(dim_v(Y(0), Y(HD), X(-P["tab_d"]) - 14, "%g mm" % HD))

# ---------------------------------------------------------------- ON GORUNUS (XZ)
ox2 = MARGIN + W * S + GAP
X2 = lambda v: ox2 + v * S
Z2 = lambda v: oy + 20 + (BH + LT - v) * S      # z yukari

parts.append(txt(ox2 + W * S / 2, oy - 22, "ON GORUNUS (kapak takili)", 10, weight="bold"))
parts.append(rect(X2(0), Z2(BH + LT), W * S, (BH + LT) * S, fill="#f4f4f4"))
# ic bosluk
parts.append(rect(X2(P["wall"]), Z2(BH), P["interior_w"] * S, P["interior_d"] * S, fill="#fff"))
# kapak
parts.append(rect(X2(0), Z2(BH + LT), W * S, LT * S, fill="#ddd"))
parts.append(txt(X2(W / 2), Z2(BH + LT / 2) + 3, "KAPAK", 8))
# plaket kesiti
parts.append(rect(X2(g["pcb_x0"]), Z2(g["pcb_top"]), P["pcb_w"] * S, P["pcb_t"] * S,
                  fill="#2b5", sw=0.4))
# standofflar
for (sx, sy) in g["standoff_xy"][:2]:
    parts.append(rect(X2(sx - P["standoff_d"] / 2), Z2(g["pcb_z0"]),
                      P["standoff_d"] * S, P["standoff_h"] * S, fill="#9d9", stroke="#373"))
# kablo cikisi kesiti
parts.append(rect(X2(W / 2 - P["cable_slot_w"] / 2), Z2(P["cable_slot_z0"] + P["cable_slot_h"]),
                  P["cable_slot_w"] * S, P["cable_slot_h"] * S, fill="#fdd", stroke="#c00"))
parts.append(txt(X2(W / 2), Z2(P["cable_slot_z0"] + P["cable_slot_h"] / 2) + 3, "KABLO CIKISI", 7,
                 fill="#c00"))
# 12V deligi
parts.append(rect(X2(W - P["wall"]), Z2(P["psu_hole_cz"] + P["psu_hole_d"] / 2),
                  P["wall"] * S, P["psu_hole_d"] * S, fill="#fdd", stroke="#c00"))
parts.append(dim_v(Z2(BH + LT), Z2(0), X2(W) + 16, "%g mm" % (BH + LT)))
parts.append(dim_h(X2(0), X2(W), Z2(0) + 18, "%g mm" % W))

# ---------------------------------------------------------------- YAN GORUNUS (YZ)
ox3 = ox2 + W * S + GAP
Y3 = lambda v: ox3 + v * S
Z3 = Z2

parts.append(txt(ox3 + HD * S / 2, oy - 22, "YAN GORUNUS (sagdan)", 10, weight="bold"))
parts.append(rect(Y3(0), Z3(BH + LT), HD * S, (BH + LT) * S, fill="#f4f4f4"))
parts.append(rect(Y3(P["wall"]), Z3(BH), P["interior_h"] * S, P["interior_d"] * S, fill="#fff"))
parts.append(rect(Y3(0), Z3(BH + LT), HD * S, LT * S, fill="#ddd"))
# usb gecisi (-X duvardan goruluyor: sagda ya da solda)
parts.append(rect(Y3(P["usb_open_cy"] - P["usb_open_w"] / 2), Z3(P["usb_open_z0"] + P["usb_open_h"]),
                  P["usb_open_w"] * S, P["usb_open_h"] * S, fill="#fdd", stroke="#c00"))
parts.append(txt(Y3(P["usb_open_cy"]), Z3(P["usb_open_z0"] + P["usb_open_h"] / 2) + 3, "USB", 8,
                 fill="#c00"))
# plaket kesiti
parts.append(rect(Y3(g["pcb_y0"]), Z3(g["pcb_top"]), P["pcb_h"] * S, P["pcb_t"] * S,
                  fill="#2b5", sw=0.4))
parts.append(dim_h(Y3(0), Y3(HD), Z3(0) + 18, "%g mm" % HD))

# ---------------------------------------------------------------- LEJANT
lx = MARGIN
ly = oy + (BH + LT) * S + 60
items = [
    ("#2b5", "plaket 100x100x1.6"),
    ("#9d9", "standoff (PCB)"),
    ("#bbb", "M3 govde kulagi"),
    ("#fdd", "kablo / USB / 12V gecisi"),
    ("#def", "havalandirma"),
    ("#eee", "montaj kulagi (buyuk kutuya)"),
]
parts.append(txt(lx, ly, "LEJANT", 9, anchor="start", weight="bold"))
for i, (c, t) in enumerate(items):
    yy = ly + 14 + i * 13
    parts.append(rect(lx, yy - 8, 10, 9, fill=c, stroke="#333", sw=0.3))
    parts.append(txt(lx + 16, yy, t, 8, anchor="start"))

notes = [
    "Dis olcu: %g x %g mm (montaj kulaklari dahil), yukseklik %g mm" % (W + 2*P["tab_d"], HD, BH + LT),
    "Malzeme: PETG, 3 perde, %%25 dolgu, 0.2 mm katman",
    "Kablo cikisi (+Y): %g x %g mm, z %g..%g" % (P["cable_slot_w"], P["cable_slot_h"],
                                                  P["cable_slot_z0"],
                                                  P["cable_slot_z0"] + P["cable_slot_h"]),
    "USB gecisi (-X): %g x %g mm" % (P["usb_open_w"], P["usb_open_h"]),
    "12V delik (+X): O%g mm" % P["psu_hole_d"],
    "Ekran / buton / RC522 BU KUTUDA YOK - buyuk kutuya gider",
]
for i, n in enumerate(notes):
    parts.append(txt(lx, ly + 100 + i * 12, n, 8, anchor="start"))

svg_w = ox3 + HD * S + MARGIN
svg_h = ly + 190

svg = ['<?xml version="1.0" encoding="UTF-8"?>',
       '<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">'
       % (svg_w, svg_h, svg_w, svg_h),
       '<rect width="100%" height="100%" fill="#fff"/>']
svg += parts
svg.append("</svg>")

os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w", encoding="utf-8").write("\n".join(svg))
print("yazildi: %s (%.0f x %.0f px)" % (OUT, svg_w, svg_h))
