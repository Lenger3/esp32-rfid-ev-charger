#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
1:1 baski dogrulamasi: PDF/SVG raster goruntusunde plaket cercevesini olcup
gercek mm cinsinden genisligi hesaplar.

    python3 tools/verify_print_scale.py hardware/LEHIM_sablon.png 297
"""
import struct
import sys
import zlib


def read_png(path):
    d = open(path, "rb").read()
    assert d[:8] == b"\x89PNG\r\n\x1a\n", "PNG degil"
    pos = 8
    w = h = bitdepth = ctype = None
    idat = b""
    while pos < len(d):
        ln = struct.unpack(">I", d[pos:pos + 4])[0]
        tag = d[pos + 4:pos + 8]
        data = d[pos + 8:pos + 8 + ln]
        if tag == b"IHDR":
            w, h, bitdepth, ctype = struct.unpack(">IIBB", data[:10])
        elif tag == b"IDAT":
            idat += data
        elif tag == b"IEND":
            break
        pos += 12 + ln
    raw = zlib.decompress(idat)
    ch = {0: 1, 2: 3, 4: 2, 6: 4}[ctype]
    stride = w * ch * (bitdepth // 8)
    out = []
    prev = bytearray(stride)
    i = 0
    bpp = ch * (bitdepth // 8)
    for _ in range(h):
        f = raw[i]
        i += 1
        line = bytearray(raw[i:i + stride])
        i += stride
        if f == 1:
            for x in range(bpp, stride):
                line[x] = (line[x] + line[x - bpp]) & 0xFF
        elif f == 2:
            for x in range(stride):
                line[x] = (line[x] + prev[x]) & 0xFF
        elif f == 3:
            for x in range(stride):
                a = line[x - bpp] if x >= bpp else 0
                line[x] = (line[x] + ((a + prev[x]) >> 1)) & 0xFF
        elif f == 4:
            for x in range(stride):
                a = line[x - bpp] if x >= bpp else 0
                b = prev[x]
                c = prev[x - bpp] if x >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 0xFF
        out.append(bytes(line))
        prev = line
    return w, h, ch, out


def main():
    if len(sys.argv) < 3:
        print("kullanim: verify_print_scale.py img.png sayfa_genislik_mm")
        return 2
    path, page_w = sys.argv[1], float(sys.argv[2])
    w, h, ch, rows = read_png(path)
    mm_per_px = page_w / w

    # koyu pikselleri bul (cerceve + metin + header kutulari)
    cols, rws = [], []
    for y, row in enumerate(rows):
        for x in range(0, w, 1):
            v = row[x * ch]
            if v < 90:
                cols.append(x)
                rws.append(y)
    if not cols:
        print("koyu piksel yok — goruntu bos")
        return 1

    print("goruntu     : %d x %d px" % (w, h))
    print("olcek       : %.4f mm/px" % mm_per_px)
    print("koyu icerik : x %.1f..%.1f mm   y %.1f..%.1f mm"
          % (min(cols) * mm_per_px, max(cols) * mm_per_px,
             min(rws) * mm_per_px, max(rws) * mm_per_px))

    # plaket cercevesi: en uzun dikey koyu cizgiyi ara (sol kenar x=20 mm bekleniyor)
    target_x = int(20.0 / mm_per_px)
    col_counts = {}
    for y, row in enumerate(rows):
        for x in range(w):
            if row[x * ch] < 90:
                col_counts[x] = col_counts.get(x, 0) + 1
    tall = [(x, c) for x, c in col_counts.items() if c > h * 0.35]
    tall.sort()
    if len(tall) >= 2:
        left, right = tall[0][0], tall[-1][0]
        span_mm = (right - left) * mm_per_px
        print("dikey cerceve: sol %.2f mm, sag %.2f mm → genislik %.2f mm"
              % (left * mm_per_px, right * mm_per_px, span_mm))
        ok = abs(span_mm - 100.0) < 1.5
        print("SONUC: %s (beklenen 100.0 mm ± 1.5)" % ("GECTI" if ok else "BASARISIZ"))
        return 0 if ok else 1
    print("dikey cerceve bulunamadi (agirlikli koyu kolon yok)")
    return 1


if __name__ == "__main__":
    sys.exit(main())
