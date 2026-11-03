#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Lehim sablonu — 1:1 olcekli, yazdirilabilir SVG.
   hardware/LEHIM-SABLONU.svg    A4, 100% baski = gercek olcu
   hardware/lehim-koordinat.md   delik koordinat tablosu + lehim sirasi

    python3 tools/gen_solder_template.py
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import draw_pcb as D  # noqa: E402

ROOT = os.path.dirname(HERE)
OUT_SVG = os.path.join(ROOT, "hardware", "LEHIM-SABLONU.svg")
OUT_MD = os.path.join(ROOT, "hardware", "lehim-koordinat.md")

PITCH = D.PITCH
N = D.N
BOARD = D.BOARD
OFF = (BOARD - N * PITCH) / 2          # 1.74 mm

# A4 yatay
A4W, A4H = 297.0, 210.0


def esc(t):
    return str(t).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def hole(c, r):
    return OFF + c * PITCH, OFF + r * PITCH


class Sheet:
    def __init__(self, w, h):
        self.w, self.h, self.p = w, h, []

    def add(self, s):
        self.p.append(s)

    def rect(self, x, y, w, h, fill="none", stroke="#000", sw=0.2, dash=None, op=1):
        d = ' stroke-dasharray="%s"' % dash if dash else ""
        self.add('<rect x="%.3f" y="%.3f" width="%.3f" height="%.3f" fill="%s" '
                 'fill-opacity="%.2f" stroke="%s" stroke-width="%.3f"%s/>'
                 % (x, y, w, h, fill, op, stroke, sw, d))

    def circle(self, cx, cy, r, fill="none", stroke="#000", sw=0.15, op=1):
        self.add('<circle cx="%.3f" cy="%.3f" r="%.3f" fill="%s" fill-opacity="%.2f" '
                 'stroke="%s" stroke-width="%.3f"/>' % (cx, cy, r, fill, op, stroke, sw))

    def line(self, x1, y1, x2, y2, stroke="#000", sw=0.2, dash=None, op=1):
        d = ' stroke-dasharray="%s"' % dash if dash else ""
        self.add('<line x1="%.3f" y1="%.3f" x2="%.3f" y2="%.3f" stroke="%s" stroke-width="%.3f" '
                 'stroke-opacity="%.2f"%s/>' % (x1, y1, x2, y2, stroke, sw, op, d))

    def text(self, x, y, s, size=2.2, anchor="middle", fill="#000", weight="normal", rot=0):
        tr = ' transform="rotate(%g %.3f %.3f)"' % (rot, x, y) if rot else ""
        self.add('<text x="%.3f" y="%.3f" font-family="Helvetica,Arial" font-size="%.2f" '
                 'text-anchor="%s" fill="%s" font-weight="%s"%s>%s</text>'
                 % (x, y, size, anchor, fill, weight, tr, esc(s)))

    def svg(self):
        return ('<?xml version="1.0" encoding="UTF-8"?>\n'
                '<svg xmlns="http://www.w3.org/2000/svg" width="%.1fmm" height="%.1fmm" '
                'viewBox="0 0 %.1f %.1f">\n'
                '<rect width="100%%" height="100%%" fill="#ffffff"/>\n%s\n</svg>\n'
                % (self.w, self.h, self.w, self.h, "\n".join(self.p)))


def draw_board(sheet, ox, oy, scale=1.0, labels=True):
    """plaket 1:1 cizimi; ox,oy = plaketin sol ust kosesi (mm)"""
    M = lambda x, y: (ox + x * scale, oy + y * scale)

    # plaket cevresi
    sheet.rect(ox, oy, BOARD * scale, BOARD * scale, fill="#f7faf7", stroke="#111", sw=0.35)

    # tum delikler
    for c in range(N):
        for r in range(N):
            x, y = M(*hole(c, r))
            sheet.circle(x, y, 0.55 * scale, fill="#c9a227", stroke="#8a6d14", sw=0.1)

    # 5'lik referans cizgileri (kenar)
    for c in range(0, N, 5):
        x, _ = M(*hole(c, 0))
        sheet.text(x, oy - 1.2, str(c), 1.9, fill="#666")
    for r in range(0, N, 5):
        _, y = M(*hole(0, r))
        sheet.text(ox - 2.6, y + 0.6, str(r), 1.9, fill="#666")

    # header'lar
    for name, h in D.HDR.items():
        c0, n = h["c0"], h["n"]
        xa, ya = M(*hole(c0, h["row"]))
        xb, _ = M(*hole(c0 + n - 1, h["row"]))
        pad = 1.27 * scale
        sheet.rect(xa - pad, ya - pad, (xb - xa) + 2 * pad, 2 * pad,
                   fill="#dbe7f5", stroke="#123", sw=0.3)
        # pin 1 kirmizi kare
        sheet.rect(xa - pad, ya - pad, 2 * pad, 2 * pad, fill="none", stroke="#d00", sw=0.45)
        for i in range(n):
            x, y = M(*hole(c0 + i, h["row"]))
            sheet.circle(x, y, 0.85 * scale, fill="none", stroke="#123", sw=0.25)
            if labels:
                sheet.text(x, ya - pad - 0.9, str(i + 1), 1.6,
                           fill="#d00" if i == 0 else "#444")
                # dikey pin adi: 2.54 mm aralikta yatay metin sigmaz
                sheet.text(x, ya + pad + 1.4, h["pins"][i], 1.55, fill="#003", rot=-90)
        cx = (xa + xb) / 2
        sheet.text(cx, ya - pad - 3.6, h["title"], 2.6, weight="bold")

    # raylar
    for r_ in D.RAILS:
        xa, ya = M(*hole(4, r_["row"]))
        xb, _ = M(*hole(32, r_["row"]))
        sheet.line(xa, ya, xb, ya, stroke="#000" if r_["name"] == "GND" else "#b35309",
                   sw=1.1, op=0.55)
        sheet.text(xa - 5, ya + 0.7, r_["name"], 2.0, anchor="end",
                   fill="#000" if r_["name"] == "GND" else "#b35309", weight="bold")
        for c in range(4, 33):
            x, y = M(*hole(c, r_["row"]))
            sheet.circle(x, y, 0.7 * scale, fill="none", stroke="#666", sw=0.15)

    # ESP32 soketi
    for row, lst in ((D.ESP_ROWS[0], D.ESP_LEFT), (D.ESP_ROWS[1], D.ESP_RIGHT)):
        for i, nm in enumerate(lst):
            x, y = M(*hole(D.ESP_C0 + i, row))
            sheet.rect(x - 1.27 * scale, y - 1.27 * scale, 2.54 * scale, 2.54 * scale,
                       fill="#20262c", stroke="#000", sw=0.2)
            sheet.text(x, y + 1.15, nm, 1.45, fill="#ffffff", rot=-90)
    # ESP32 govde silueti
    ex, ey = M(*hole(D.ESP_C0 - 0.5, D.ESP_ROWS[0] - 0.5))
    sheet.rect(ex, ey, 16 * PITCH * scale, 10 * PITCH * scale, fill="none",
               stroke="#5a3a8f", sw=0.3, dash="1.2 0.8")
    sheet.text(ex + 8 * PITCH * scale, ey + 10 * PITCH * scale - 0.6,
               "ESP32-WROOM-32U  (govde 26 x 52 mm)", 1.8, fill="#5a3a8f")

    # guc bolgesi
    for key, col in (("J1", "#2b6cb0"), ("J2", "#2b6cb0")):
        pass
    jx, jy = M(*hole(2, 29))
    sheet.rect(jx - 2, jy - 3, 4, 6, fill="#cfe4ff", stroke="#2b6cb0", sw=0.3)
    sheet.text(jx, jy + 0.7, "J1", 1.9, weight="bold")
    jx2, jy2 = M(*hole(7, 29))
    sheet.rect(jx2 - 2, jy2 - 3, 4, 6, fill="#cfe4ff", stroke="#2b6cb0", sw=0.3)
    sheet.text(jx2, jy2 + 0.7, "J2", 1.9, weight="bold")

    bx, by = M(*hole(20, 28))
    sheet.rect(bx - 18, by - 9, 36, 18, fill="#dff5e3", stroke="#276b41", sw=0.3, dash="1.2 0.8")
    sheet.text(bx, by, "BUCK 12->3.3 V", 2.2, weight="bold")
    # montaj delikleri
    for (mc, mr) in [(1.6, 1.6), (36.4, 1.6), (1.6, 36.4), (36.4, 36.4)]:
        x, y = M(*hole(mc, mr))
        sheet.circle(x, y, 1.6, fill="#ffffff", stroke="#333", sw=0.3)
        sheet.circle(x, y, 0.75, fill="#cccccc", stroke="#333", sw=0.2)


def main():
    os.makedirs(os.path.dirname(OUT_SVG), exist_ok=True)
    sh = Sheet(A4W, A4H)

    # cerceve + baslik
    sh.rect(8, 8, A4W - 16, A4H - 16, stroke="#bbb", sw=0.2)
    sh.text(A4W / 2, 15, "LEHIM SABLONU — 100 x 100 mm  ·  1:1 OLCek", 5.2, weight="bold")
    sh.text(A4W / 2, 20.5,
            "Yazdirirken 'Actual size / %100' secin. Cetvel kontrolu: alttaki 50 mm olcegi olcun.",
            2.6, fill="#444")

    # plaket 1:1
    draw_board(sh, 20, 36, scale=1.0)

    # olcek kontrol cubugu
    sx, sy = 20, 160
    sh.line(sx, sy, sx + 50, sy, stroke="#000", sw=0.4)
    for i in range(0, 51, 10):
        sh.line(sx + i, sy - 1.5, sx + i, sy + 1.5, stroke="#000", sw=0.3)
        sh.text(sx + i, sy + 4.5, str(i), 2.0)
    sh.text(sx + 25, sy - 3, "OLCEK KONTROLU: bu cizgi tam 50 mm olmali", 2.2, weight="bold")

    # sag panel: lehim sirasi
    px = 132
    sh.text(px, 30, "LEHIM SIRASI", 3.4, anchor="start", weight="bold")
    steps = [
        ("0", "Olcek kontrolu: 50 mm cizgisi tam 50 mm mi?"),
        ("1", "GND rayi (satir 21, kolon 4-32) — 8 pin"),
        ("2", "3V3 rayi (satir 23, kolon 4-32) — 8 pin"),
        ("3", "ESP32 2x15 soket — once modulle hizala"),
        ("4", "J2 RC522 (satir 2, kolon 3-10)"),
        ("5", "J3 BUTON (satir 2, kolon 13-17)"),
        ("6", "J4 OLED i2C (satir 2, kolon 20-23)"),
        ("7", "J5 YEDEK (satir 2, kolon 26-29)"),
        ("8", "J1 + J2 12V klemens (satir 29)"),
        ("9", "Buck cikisi raylara + 100uF"),
        ("10", "Guc verme testi: 3V3 rayinda 3.3 V"),
        ("11", "Modulleri tak, firmware yukle"),
    ]
    y = 36
    for code, txt_ in steps:
        sh.circle(px + 2.5, y - 0.8, 2.0, fill="#eef3fa", stroke="#345", sw=0.2)
        sh.text(px + 2.5, y + 0.1, code, 1.9, weight="bold")
        sh.text(px + 6.5, y + 0.1, txt_, 1.9, anchor="start")
        y += 6.0

    # sag alt: pin 1 uyarilari
    y += 4
    sh.text(px, y, "KRITIK", 3.0, anchor="start", weight="bold", fill="#b00")
    warns = [
        "Kirmizi kare = pin 1. Tum header'larda ayni yon.",
        "RC522'ye 5 V VERME — aninda olur.",
        "Raylar birbirine degmemeli: 2 delik (5.08 mm) ara var.",
        "ESP32 sirasi 22.86 mm = 9 delik. Kayarsa modul girmez.",
        "OLED'e giden 4 tel: 3V3, GND, SCL(G22), SDA(G21)",
        "RES/A0/CS modul tarafinda: RES->3V3, A0->GND, CS->GND",
    ]
    for w in warns:
        y += 5.2
        sh.text(px + 1, y, "•", 2.2, anchor="start", fill="#b00")
        sh.text(px + 4, y, w, 1.85, anchor="start")

    open(OUT_SVG, "w", encoding="utf-8").write(sh.svg())
    print("yazildi: %s" % OUT_SVG)

    # ---------------- koordinat tablosu ----------------
    md = ["# Lehim Koordinat Tablosu", "",
          "Plaket 100 x 100 mm · 38 x 38 delik · adim 2.54 mm",
          "Ilk delik merkezi sol ust koseden **%.2f mm** iceride." % OFF, "",
          "## Header'lar (satir 2)", "",
          "| Header | Kolon | Delik (x, y) mm | Pin sayisi |",
          "|---|---|---|---|"]
    for name, h in D.HDR.items():
        x0, y0 = hole(h["c0"], h["row"])
        x1, _ = hole(h["c0"] + h["n"] - 1, h["row"])
        md.append("| %s | %d–%d | (%.2f, %.2f) → (%.2f, %.2f) | %d |"
                  % (h["title"], h["c0"], h["c0"] + h["n"] - 1, x0, y0, x1, y0, h["n"]))
    md += ["", "## Pin pin eslesme", ""]
    for name, h in D.HDR.items():
        md.append("### %s" % h["title"])
        md.append("")
        md.append("| Pin | Sinyal | GPIO | Delik (x, y) mm |")
        md.append("|---|---|---|---|")
        for i in range(h["n"]):
            x, y = hole(h["c0"] + i, h["row"])
            md.append("| %d | %s | %s | (%.2f, %.2f) |" % (i + 1, h["pins"][i], h["gpio"][i], x, y))
        md.append("")

    md += ["## Raylar", ""]
    for r_ in D.RAILS:
        x0, y0 = hole(4, r_["row"])
        x1, _ = hole(32, r_["row"])
        md.append("- **%s**: satir %d, kolon 4–32 → (%.2f, %.2f) → (%.2f, %.2f)"
                  % (r_["name"], r_["row"], x0, y0, x1, y0))
    md += ["", "## ESP32 soketi", ""]
    for row, lst in ((D.ESP_ROWS[0], D.ESP_LEFT), (D.ESP_ROWS[1], D.ESP_RIGHT)):
        x0, y0 = hole(D.ESP_C0, row)
        x1, _ = hole(D.ESP_C0 + 14, row)
        md.append("- satir %d: kolon %d–%d → (%.2f, %.2f) → (%.2f, %.2f) · %d pin"
                  % (row, D.ESP_C0, D.ESP_C0 + 14, x0, y0, x1, y0, len(lst)))
    md += ["", "Iki sira arasi: **%.2f mm** (9 delik)" % (9 * PITCH), "",
           "## Guc bolgesi", "",
           "- J1 12 V: (%.2f, %.2f) — 2 pin klemens, dikey" % hole(2, 29),
           "- J2 12 V: (%.2f, %.2f) — 2 pin klemens, dikey" % hole(7, 29),
           "- Buck: merkez (%.2f, %.2f), ~36 x 18 mm alan" % hole(20, 28),
           "- Montaj delikleri: %s" % ", ".join("(%.2f, %.2f)" % hole(a, b)
                                                for a, b in [(1.6, 1.6), (36.4, 1.6),
                                                             (1.6, 36.4), (36.4, 36.4)]),
           "", "## Malzeme sayimi", "",
           "| Parca | Adet | Kaynak |", "|---|---|---|",
           "| Disi header pini (ESP32 soketi) | 30 | 1x6 disi ×5 (kesilir) |",
           "| Erkek header pini (J2–J5 + raylar) | %d | 1x40 erkek (kesilir) |"
           % (8 + 5 + 4 + 4 + 8 + 8),
           "| 2 pin 5.08 klemens | 2 | J1, J2 |", ""]
    open(OUT_MD, "w", encoding="utf-8").write("\n".join(md) + "\n")
    print("yazildi: %s" % OUT_MD)
    return 0


if __name__ == "__main__":
    sys.exit(main())
