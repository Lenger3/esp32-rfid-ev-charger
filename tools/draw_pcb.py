#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Plaket cizimleri (iki ayri pafta, ayri dosya):
  hardware/pcb_layout.svg   yerlesim: delik izgarasi, header'lar, raylar, bolgeler
  hardware/pcb_wiring.svg   kablolama: ayni yerlesim + net haritasi

    python3 tools/draw_pcb.py
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_LAYOUT = os.path.join(ROOT, "hardware", "pcb_layout.svg")
OUT_WIRING = os.path.join(ROOT, "hardware", "pcb_wiring.svg")

PITCH = 2.54
N = 38
BOARD = 100.0
PX = 18.4                      # px/mm

HDR = {
    "RC522": dict(row=2, c0=3, n=8, title="J2  RC522 (1x8)",
                  pins=["SDA", "SCK", "MOSI", "MISO", "IRQ", "GND", "RST", "3V3"],
                  gpio=["G5", "G18", "G23", "G19", "-", "-", "G27", "-"]),
    "BUTON": dict(row=2, c0=13, n=5, title="J3  BUTON (1x5)",
                  pins=["GND", "UP", "DOWN", "BACK", "OK"],
                  gpio=["-", "G32", "G33", "G26", "G25"]),
    "OLED": dict(row=2, c0=20, n=4, title="J4  OLED i2C (1x4)",
                 pins=["3V3", "GND", "SCL", "SDA"],
                 gpio=["-", "-", "G22", "G21"]),
    "YEDEK": dict(row=2, c0=26, n=4, title="J5  YEDEK (1x4)",
                  pins=["+12V", "GND", "G13", "G14"], gpio=["-", "-", "PWM", "BZR"]),
}

ESP_ROWS = (7, 16)
ESP_C0 = 10
ESP_LEFT = ["EN", "VP36", "VN39", "34", "35", "32", "33", "25", "26",
            "27", "14", "12", "13", "GND", "VIN"]
ESP_RIGHT = ["3V3", "GND", "15", "2", "4", "16", "17", "5", "18",
             "19", "21", "RX3", "TX1", "22", "23"]

RAILS = [dict(name="GND", row=21, color="#111111"),
         dict(name="3V3", row=23, color="#d2691e")]

J1 = dict(row=28.5, col=2.5, w=2.4, h=3, label="J1\n12V")
J2 = dict(row=28.5, col=8.0, w=2.4, h=3, label="J2\n12V")
BUCK = dict(row=27.5, col=20, w=15, h=8, label="BUCK  12 V -> 3.3 V")
CAP = dict(row=24.2, col=13.2, label="100uF")
FUSE = dict(row=24.2, col=10.2, label="1A")

NETS = [
    # I2C (OLED) — 4 tel
    ("ESP32", "22", "OLED", 2, "#1f77b4", "SCL"),
    ("ESP32", "21", "OLED", 3, "#9467bd", "SDA"),
    # SPI (RC522)
    ("ESP32", "18", "RC522", 1, "#2ca02c", "SCLK"),
    ("ESP32", "23", "RC522", 2, "#8c564b", "MOSI"),
    ("ESP32", "19", "RC522", 3, "#e377c2", "MISO"),
    ("ESP32", "5",  "RC522", 0, "#d62728", "CS"),
    ("ESP32", "27", "RC522", 6, "#ff7f0e", "RST"),
    # guc
    ("3V3", None, "OLED", 0, "#d2691e", "3V3"),
    ("3V3", None, "RC522", 7, "#d2691e", "3V3"),
    ("GND", None, "OLED", 1, "#111111", "GND"),
    ("GND", None, "RC522", 5, "#111111", "GND"),
    ("GND", None, "BUTON", 0, "#111111", "GND"),
    # butonlar
    ("ESP32", "32", "BUTON", 1, "#0a8f3c", "UP"),
    ("ESP32", "33", "BUTON", 2, "#0a6b2e", "DOWN"),
    ("ESP32", "26", "BUTON", 3, "#1a5f5f", "BACK"),
    ("ESP32", "25", "BUTON", 4, "#a01020", "OK"),
]


def esc(t):
    return t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def hole_xy(c, r):
    off = (BOARD - N * PITCH) / 2
    return off + c * PITCH, off + r * PITCH


def hdr_pin_xy(name, idx):
    h = HDR[name]
    return hole_xy(h["c0"] + idx, h["row"])


def esp_pin_xy(gpio):
    if gpio in ESP_LEFT:
        return hole_xy(ESP_C0 + ESP_LEFT.index(gpio), ESP_ROWS[0])
    if gpio in ESP_RIGHT:
        return hole_xy(ESP_C0 + ESP_RIGHT.index(gpio), ESP_ROWS[1])
    raise KeyError(gpio)


def rail_xy(name, col):
    r = next(x for x in RAILS if x["name"] == name)
    return hole_xy(col, r["row"])


class Svg:
    def __init__(self, w, h):
        self.w, self.h, self.p = w, h, []

    def add(self, s):
        self.p.append(s)

    def rect(self, x, y, w, h, fill="none", stroke="#000", sw=0.6, rx=0,
             dash=None, op=1):
        d = ' stroke-dasharray="%s"' % dash if dash else ""
        self.add('<rect x="%.2f" y="%.2f" width="%.2f" height="%.2f" rx="%.2f" fill="%s" '
                 'fill-opacity="%.2f" stroke="%s" stroke-width="%.2f"%s/>'
                 % (x, y, w, h, rx, fill, op, stroke, sw, d))

    def circle(self, cx, cy, r, fill="none", stroke="#000", sw=0.5, op=1):
        self.add('<circle cx="%.2f" cy="%.2f" r="%.2f" fill="%s" fill-opacity="%.2f" '
                 'stroke="%s" stroke-width="%.2f"/>' % (cx, cy, r, fill, op, stroke, sw))

    def line(self, x1, y1, x2, y2, stroke="#000", sw=0.5, dash=None, op=1, cap="round"):
        d = ' stroke-dasharray="%s"' % dash if dash else ""
        self.add('<line x1="%.2f" y1="%.2f" x2="%.2f" y2="%.2f" stroke="%s" stroke-width="%.2f" '
                 'stroke-opacity="%.2f" stroke-linecap="%s"%s/>'
                 % (x1, y1, x2, y2, stroke, sw, op, cap, d))

    def poly(self, pts, stroke="#000", sw=1.4, dash=None, op=1, fill="none"):
        d = ' stroke-dasharray="%s"' % dash if dash else ""
        p = " ".join("%.2f,%.2f" % pt for pt in pts)
        self.add('<polyline points="%s" fill="%s" stroke="%s" stroke-width="%.2f" '
                 'stroke-opacity="%.2f" stroke-linejoin="round"%s/>' % (p, fill, stroke, sw, op, d))

    def text(self, x, y, s, size=10, anchor="middle", fill="#111", weight="normal", rot=0):
        tr = ' transform="rotate(%d %.2f %.2f)"' % (rot, x, y) if rot else ""
        out = ['<text x="%.2f" y="%.2f" font-family="Helvetica,Arial,sans-serif" font-size="%.1f" '
               'text-anchor="%s" fill="%s" font-weight="%s"%s>'
               % (x, y, size, anchor, fill, weight, tr)]
        for i, ln in enumerate(s.split("\n")):
            dy = 0 if i == 0 else size * 1.08
            out.append('<tspan x="%.2f" dy="%.2f">%s</tspan>' % (x, dy, esc(ln)))
        out.append("</text>")
        self.add("".join(out))

    def chip(self, cx, cy, label, size=7.4, fg="#102a1a", bg="#ffffff", pad=3.2):
        w = len(label) * size * 0.60 + pad * 2
        h = size * 1.55
        self.rect(cx - w / 2, cy - h / 2, w, h, fill=bg, stroke="#9aa", sw=0.5, rx=2)
        self.text(cx, cy + size * 0.35, label, size, fill=fg, weight="bold")

    def svg(self):
        return ('<?xml version="1.0" encoding="UTF-8"?>\n'
                '<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">\n'
                '<rect width="100%%" height="100%%" fill="#ffffff"/>\n%s\n</svg>\n'
                % (self.w, self.h, self.w, self.h, "\n".join(self.p)))


def draw_board(svg, ox, oy, with_nets):
    M = lambda x, y: (ox + x * PX, oy + y * PX)
    B = BOARD * PX

    # plaket
    svg.rect(*M(0, 0), B, B, fill="#0f6b34", stroke="#06451f", sw=1.6, rx=4)
    svg.rect(ox + 1, oy + 1, B - 2, B - 2, fill="none", stroke="#2e9a5a", sw=0.8, rx=3)

    # delikler
    for c in range(N):
        for r in range(N):
            x, y = M(*hole_xy(c, r))
            svg.circle(x, y, PX * 0.26, fill="#d9b62e", stroke="#8a6d14", sw=0.25)

    # bolge etiketleri (beyaz chip, okunakli)
    zx, zy = M(*hole_xy(19, 0.2))
    svg.chip(zx, zy + 6, "KONNEKTOR SIRASI")
    svg.rect(*M(*hole_xy(2.2, 1.2)), 33.6 * PITCH * PX, 2.8 * PITCH * PX,
             fill="#2f7fd1", op=0.13, stroke="#bfe0ff", sw=0.7, dash="5 4", rx=3)

    sx, sy = M(*hole_xy(ESP_C0 - 1.4, ESP_ROWS[0] - 1.2))
    svg.rect(sx, sy, 17.8 * PITCH * PX, 11.4 * PITCH * PX, fill="#8e6bd1", op=0.14,
             stroke="#d9c9f5", sw=0.7, dash="5 4", rx=3)
    svg.chip(*M(*hole_xy(ESP_C0 + 7, ESP_ROWS[0] - 2.0)), "ESP32-WROOM-32U")

    gx, gy = M(*hole_xy(1.4, 24.6))
    svg.rect(gx, gy, 30.6 * PITCH * PX, 10.4 * PITCH * PX, fill="#d0453a", op=0.12,
             stroke="#ffc9c2", sw=0.7, dash="5 4", rx=3)
    svg.chip(*M(*hole_xy(16.4, 24.4)), "GUC BOLGESI 12V")

    # konnektorler
    for name, h in HDR.items():
        for i in range(h["n"]):
            x, y = M(*hole_xy(h["c0"] + i, h["row"]))
            cell = PITCH * PX
            svg.rect(x - cell / 2, y - cell / 2, cell, cell,
                     fill="#f2f2f2" if name != "YEDEK" else "#e2e2e2",
                     stroke="#1a1a1a", sw=0.6)
            svg.text(x, y + cell * 0.16, h["pins"][i], 6.4, fill="#111")
            if with_nets:
                svg.text(x, y + cell * 0.62, h["gpio"][i], 5.0, fill="#555")
        xa, ya = M(*hole_xy(h["c0"], h["row"]))
        xb, _ = M(*hole_xy(h["c0"] + h["n"] - 1, h["row"]))
        cell = PITCH * PX
        svg.rect(xa - cell / 2 - 2.5, ya - cell / 2 - 2.5,
                 (xb - xa) + cell + 5, cell + 5, stroke="#04130a", sw=1.1, rx=2)
        svg.chip((xa + xb) / 2, ya - cell / 2 - 13, h["title"])
        svg.circle(xa - cell / 2 + 3.5, ya - cell / 2 - 3.5, 2.2, fill="#e33", stroke="#900", sw=0.4)

    # ESP32 soketi
    for row, lst in ((ESP_ROWS[0], ESP_LEFT), (ESP_ROWS[1], ESP_RIGHT)):
        for i, nm in enumerate(lst):
            x, y = M(*hole_xy(ESP_C0 + i, row))
            cell = PITCH * PX
            svg.rect(x - cell / 2, y - cell / 2, cell, cell, fill="#12191f", stroke="#000", sw=0.5)
            if row == ESP_ROWS[0]:
                svg.text(x, y - cell * 0.14, nm, 6.0, fill="#eaf2ff")
            else:
                svg.text(x, y + cell * 0.34, nm, 6.0, fill="#eaf2ff")
    svg.chip(*M(*hole_xy(ESP_C0 + 7, ESP_ROWS[0] + 5.4)), "2 x 15 disi header  (22.86 mm)")

    # raylar
    for r in RAILS:
        xa, ya = M(*hole_xy(4, r["row"]))
        xb, _ = M(*hole_xy(32, r["row"]))
        svg.line(xa, ya, xb, ya, stroke=r["color"], sw=8, op=0.30, cap="butt")
        svg.line(xa, ya, xb, ya, stroke=r["color"], sw=1.6, op=0.95, dash="6 4", cap="butt")
        svg.chip(xa - 26, ya, r["name"], size=7.6, fg="#fff",
                 bg=r["color"] if r["name"] == "GND" else "#b35309")
        # baglanti noktalari
        for c in range(4, 33, 4):
            x, y = M(*hole_xy(c, r["row"]))
            svg.circle(x, y, 2.6, fill=r["color"], op=0.9, stroke="#fff", sw=0.5)

    # guc elemanlari
    def comp(b, fill, stroke, size=6.4):
        x, y = M(*hole_xy(b["col"], b["row"]))
        w, h = b.get("w", 2) * PITCH * PX, b.get("h", 2) * PITCH * PX
        svg.rect(x - w / 2, y - h / 2, w, h, fill=fill, stroke=stroke, sw=0.9, rx=2.5)
        if b.get("label"):
            svg.text(x, y + 2.4, b["label"], size, fill="#0e1a12", weight="bold")
        return x, y

    comp(J1, "#cfe4ff", "#2b6cb0", 6.0)
    comp(J2, "#cfe4ff", "#2b6cb0", 6.0)
    comp(BUCK, "#d8f5dc", "#276b41", 6.6)
    cx, cy = M(*hole_xy(CAP["col"], CAP["row"]))
    svg.circle(cx, cy, 9, fill="#ffe9c7", stroke="#b7791f", sw=0.9)
    svg.text(cx, cy + 2.4, "100uF", 5.4)
    fx, fy = M(*hole_xy(FUSE["col"], FUSE["row"]))
    svg.rect(fx - 8, fy - 4, 16, 8, fill="#ffe0e0", stroke="#a33", sw=0.8, rx=1.5)
    svg.text(fx, fy + 2.2, "1A", 5.4)

    # montaj delikleri (plaket koseleri)
    for (mx, my) in [(1.6, 1.6), (36.4, 1.6), (1.6, 36.4), (36.4, 36.4)]:
        x, y = M(*hole_xy(mx, my))
        svg.circle(x, y, 8.5, fill="#ffffff", stroke="#20321f", sw=1.0)
        svg.circle(x, y, 3.4, fill="#dcdcdc", stroke="#555", sw=0.6)

    if with_nets:
        for idx, (src, sp, dst, di, col, lbl) in enumerate(NETS, start=1):
            a = M(*esp_pin_xy(sp)) if src == "ESP32" else M(*rail_xy(src, 6 + (di % 3) * 3))
            if dst in ("3V3", "GND"):
                b = M(*rail_xy(dst, 9 + (di % 4) * 3))
            else:
                b = M(*hdr_pin_xy(dst, di))
            ay = a[1] - (16 + (idx % 6) * 9)
            svg.poly([a, (a[0], ay), (b[0], ay), b], stroke=col, sw=1.1, op=0.42)
            for pt in (a, b):
                svg.circle(pt[0], pt[1], 6.4, fill="#ffffff", op=1, stroke=col, sw=1.1)
                svg.text(pt[0], pt[1] + 2.6, str(idx), 6.2, fill=col, weight="bold")

    # olcu
    svg.line(ox, oy + B + 16, ox + B, oy + B + 16, stroke="#333", sw=0.7)
    svg.line(ox, oy + B + 11, ox, oy + B + 21, stroke="#333", sw=0.7)
    svg.line(ox + B, oy + B + 11, ox + B, oy + B + 21, stroke="#333", sw=0.7)
    svg.text(ox + B / 2, oy + B + 30, "100 mm   (38 x 38 delik, 2.54 mm adim)",
             9, fill="#333")
    svg.line(ox - 16, oy, ox - 16, oy + B, stroke="#333", sw=0.7)
    svg.text(ox - 24, oy + B / 2, "100 mm", 9, fill="#333", rot=-90)


TBL_ROWS = len(NETS) + 1


def table(svg, x0, y0, w):
    """Sinyal baglanti tablosu: # | sinyal | kaynak | hedef"""
    cols = [38, 150, 250, 250]
    rh = 17.5
    n = len(NETS)
    svg.rect(x0, y0, w, rh * (n + 1) + 4, fill="#ffffff", stroke="#889", sw=0.9, rx=3)
    svg.rect(x0, y0, w, rh, fill="#e8eef6", stroke="#889", sw=0.9, rx=3)
    heads = ["#", "sinyal", "kaynak", "hedef"]
    cx = x0
    for i, h in enumerate(heads):
        svg.text(cx + 8, y0 + rh * 0.7, h, 8.6, anchor="start", weight="bold", fill="#123")
        cx += cols[i]
    y = y0 + rh
    for idx, (src, sp, dst, di, col, lbl) in enumerate(NETS, start=1):
        if idx % 2 == 0:
            svg.rect(x0 + 1, y, w - 2, rh, fill="#f6f8fb", stroke="none", sw=0)
        src_txt = ("GPIO " + sp) if src == "ESP32" else src
        if dst in ("3V3", "GND"):
            dst_txt = dst + " rayi"
        else:
            dst_txt = "%s  pin %d" % (dst, di + 1)
        vals = [str(idx), lbl or "", src_txt, dst_txt]
        cx = x0
        for i, v in enumerate(vals):
            svg.text(cx + 8, y + rh * 0.72, v, 8.2, anchor="start",
                     fill=col if i == 0 else "#222", weight="bold" if i == 0 else "normal")
            cx += cols[i]
        y += rh
    return y


def page(wiring):
    pad_x, pad_top = 74, 96
    W = int(BOARD * PX + pad_x * 2)
    H = int(BOARD * PX + pad_top + (760 if wiring else 210))
    svg = Svg(W, H)
    ttl = ("KABLOLAMA (NET HARITASI)" if wiring else "PLAKET YERLESIMI")
    svg.text(W / 2, 40, "%s — 100 x 100 mm" % ttl, 21, weight="bold")
    sub = ("hatirlatma: SPI kablolari 15 cm'yi gecmesin, GND arada, 12 V demetinden >=5 cm uzak"
           if wiring else
           "ESP32-WROOM-32U + OLED M242 (i2C) + RC522 + 4 buton + buck 12->3.3 V")
    svg.text(W / 2, 62, sub, 10.5, fill="#555")

    draw_board(svg, pad_x, pad_top, with_nets=wiring)

    tbl_end = 0
    if wiring:
        tbl_end = table(svg, 40, pad_top + BOARD * PX + 40, W - 80)

    # notlar
    ny = (tbl_end + 46) if wiring else (pad_top + BOARD * PX + 56)
    svg.line(24, ny - 16, W - 24, ny - 16, stroke="#ddd", sw=1)
    svg.text(28, ny + 4, "NOTLAR", 10, anchor="start", weight="bold")
    notes = [
        "Kirmizi nokta = konnektor pin 1. Tum header'lar 2.54 mm disi; moduller tunik soket ile baglanir.",
        "GND rayi (siyah, satir 21) ve 3V3 rayi (turuncu, satir 23) plaket boyunca kalin lehimle cekilir.",
        "ESP32 iki sirasi arasi 22.86 mm (9 delik). Header'lari once modulun pinlerine tak, sonra lehimle.",
        "12 V bolgesi ESP32'den >=20 mm uzakta; SPI hatlari plaketin ortasindan gecer.",
        "RC522'ye 5 V VERILMEZ. Buck cikisi 3.3 V rayina; USB takarken jumper ile kes.",
    ]
    for i, n in enumerate(notes):
        svg.text(28, ny + 22 + i * 15, "•  " + n, 8.8, anchor="start", fill="#333")
    return svg, W, H


def legend(svg, W, H, wiring):
    y = H - 110
    svg.line(24, y - 18, W - 24, y - 18, stroke="#ddd", sw=1)
    svg.text(28, y, "RENK KODU", 10, anchor="start", weight="bold")
    if wiring:
        items = [("#1f77b4", "OLED SCL  G22"), ("#9467bd", "OLED SDA  G21"),
                 ("#2ca02c", "RC522 SCLK  G18"), ("#8c564b", "RC522 MOSI  G23"),
                 ("#e377c2", "RC522 MISO  G19"), ("#d62728", "RC522 CS  G5"),
                 ("#ff7f0e", "RC522 RST  G27"), ("#d2691e", "3V3"),
                 ("#111111", "GND"), ("#0a8f3c", "buton UP  G32"),
                 ("#0a6b2e", "buton DOWN  G33"), ("#1a5f5f", "buton BACK  G26"),
                 ("#a01020", "buton OK  G25")]
    else:
        items = [("#0f6b34", "plaket"), ("#d9b62e", "delik 2.54"),
                 ("#f2f2f2", "header"), ("#12191f", "ESP32 soketi"),
                 ("#ffffff", "M3 montaj"), ("#2f7fd1", "konnektor sirasi"),
                 ("#8e6bd1", "ESP32 bolgesi"), ("#d0453a", "guc bolgesi")]
    per = 4
    for i, (c, t) in enumerate(items):
        col, row = i % per, i // per
        x = 28 + col * (W - 56) / per
        yy = y + 20 + row * 16
        svg.rect(x, yy - 7, 16, 11, fill=c, stroke="#556", sw=0.5)
        svg.text(x + 21, yy + 2, t, 8.6, anchor="start")


def main():
    os.makedirs(os.path.dirname(OUT_LAYOUT), exist_ok=True)
    svg, W, H = page(False)
    legend(svg, W, H, False)
    open(OUT_LAYOUT, "w", encoding="utf-8").write(svg.svg())
    print("yazildi: %s (%d x %d px)" % (OUT_LAYOUT, W, H))

    svg, W, H = page(True)
    legend(svg, W, H, True)
    open(OUT_WIRING, "w", encoding="utf-8").write(svg.svg())
    print("yazildi: %s (%d x %d px)" % (OUT_WIRING, W, H))
    return 0


if __name__ == "__main__":
    sys.exit(main())
