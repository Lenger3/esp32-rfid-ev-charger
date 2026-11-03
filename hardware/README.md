# Plaket Kutusu (v2) — sadece 100×100 plaket için

**Dış ölçü: 124 × 124 × 48 mm** (montaj kulaklarıyla genişlik 144 mm)
PETG · 3 perde · %25 dolgu

> **v2 nedir:** Ekran, butonlar ve RC522 bu kutuda **yok** — onlar büyük şarj kutusuna gider.
> Bu kutu sadece plaketi taşır, büyük kutunun içine vidalanır. 12 V adaptör de büyük kutuda.
>
> Eski v1 (140×170×62, ön panelde OLED+buton+RC522) `box.scad` olarak duruyor, **kullanılmıyor**.

## Dosyalar

```
hardware/box_pcb.scad            kutu modeli (OpenSCAD)
hardware/box_pcb_params.json     kutu ölçüleri
hardware/body_pcb.stl            kutu gövde (baskıya hazır)
hardware/lid_pcb.stl             kutu kapak (baskıya hazır)
hardware/preview.png             kutu 3D önizleme
hardware/box_pcb_drawing.svg     kutu teknik resim

hardware/LEHIM-SABLONU.svg       ★ 1:1 lehim şablonu (A4 yatay, 297×210 mm)
hardware/LEHIM-SABLONU.pdf       ★ aynı şablon, 1:1 PDF (baskıya hazır)
hardware/LEHIM-SABLONU.png       önizleme
hardware/lehim-koordinat.md      delik koordinatları + pin tablosu
hardware/pcb_layout.svg/.png     plaket yerleşimi (renkli, ekranda okumak için)
hardware/pcb_wiring.svg/.png     kablolama + 16 satırlık net tablosu
```

Üretici: `tools/gen_box_pcb.py`, `tools/gen_solder_template.py`, `tools/draw_pcb.py`,
`tools/render_stl.py` · Doğrulama: `tools/check_stl.py`, `tools/verify_solid.py`,
`tools/verify_print_scale.py`

## Lehim şablonu — 1:1 baskı

1. `LEHIM-SABLONU.pdf` aç, yazdır → **"Actual size / %100"**, ölçekleme KAPALI, A4 yatay.
2. Sayfadaki **50 mm ölçek çubuğunu cetvelle ölç** — tam 50 mm olmalı. Değilse yazıcı
   ölçeklemiştir, ayarı düzelt.
3. Şablonu masaya koy, 100×100 plaketi üzerine hizala (köşe montaj delikleri çakışır).
4. Deliklerden iğne/uçla işaretle veya doğrudan üstüne lehimle.
5. Header kutularının kırmızı karesi = **pin 1**, hepsinde aynı yön.

Doğrulama: `python3 tools/verify_print_scale.py hardware/_pdf.png 297`
→ `dikey cerceve: ... genislik 100.19 mm · SONUC: GECTI`

## Yeniden üret

```bash
cd "$(git rev-parse --show-toplevel)"
python3 tools/gen_box_pcb.py                  # kontrol + scad
python3 tools/draw_box_pcb.py                 # teknik resim
cd hardware
docker run --rm -v "$PWD:/src" openscad/openscad:latest \
  openscad -o /src/body_pcb.stl -D 'part="body"' /src/box_pcb.scad
docker run --rm -v "$PWD:/src" openscad/openscad:latest \
  openscad -o /src/lid_pcb.stl  -D 'part="lid"'  /src/box_pcb.scad
cd .. && python3 tools/verify_solid.py hardware/body_pcb.stl hardware/lid_pcb.stl
python3 tools/render_stl.py hardware/preview.png hardware/body_pcb.stl hardware/lid_pcb.stl
```

Not: `brew install --cask openscad` bu makinede çalışmıyor (cask 2026-09'da devre dışı).
Docker ile render en pratik yol.

## Açıklıklar

| Yüz | Açıklık | Ölçü | Ne için |
|---|---|---|---|
| +Y duvar | kablo çıkışı | 38 × 14 (z 14–28) | OLED 7 + RC522 8 + buton 5 tel |
| −X duvar | USB geçişi | 19 × 13 (z 13.5–26.5) | panel USB uzatması, firmware |
| +X duvar | 12 V giriş | Ø10 (z 21) | adaptörden gelen 12 V |
| −Y duvar | havalandırma | 5 × (3 × 26) | buck soğutma |
| Kapak | havalandırma | 5 × (32 × 3) | konveksiyon |
| Taban | montaj kulakları | 2 × (10 × 30 × 4), Ø4.4 | büyük kutuya sabitleme |
| İç | kablo bağlama tırnağı | 2 × Ø5 h14 | gerinim azaltma |

## İç yerleşim

```
42 mm iç yükseklik
┌──────────────────────────────────┐
│ kapak 5 kanal havalandırma       │
│ ┌──────────────────────────────┐ │
│ │ tunik soket + kablo dönüşü   │ │ 12.9 mm
│ │ header 8.5 mm                │ │
│ ├──────────────────────────────┤ │
│ │ PLAKET 100×100×1.6           │ │
│ ├──────────────────────────────┤ │
│ │ standoff 8 mm (4× M3)        │ │
│ └──────────────────────────────┘ │
│  taban                           │
└──────────────────────────────────┘
```

- Plaket 100 × 100, 4 × M3 × 8 standoff, kenar boşluğu 9 mm
- Plaket üstü yığın 21.1 mm, üstte 12.9 mm boşluk
- İç yükseklik 42 = standoff 8 + plaket 1.6 + yığın 21.1 + kablo dönüşü 11.3

## Vida

- Kapak: 4 × M3 × 12 (havşa önerilir)
- Plaket: 4 × M3 × 8 dişi-dişi standoff + M3 × 6 vida

## Baskı notları

- Gövdeyi **ön yüzü yukarı** bas (destek gerekmez)
- Kapak: dış yüzey tablaya, dudak yukarı (destek gerekmez)
- USB geçişi 19 × 13: 13 mm köprü, sorun çıkarırsa yavaşlat veya geçici destek
- Montaj kulakları tabanla aynı düzlemde, brim önerilir

## Doğrulama durumu

| Test | Sonuç |
|---|---|
| `gen_box_pcb.py` geometri kontrolleri | 31/31 PASS |
| `check_stl.py` su sızdırmazlık | gövde 1 parça, kapak 1 parça, açık kenar 0 |
| `verify_solid.py` iç nokta testleri | 34/34 PASS |
| Görsel önizleme (`preview.png`) | taban, duvarlar, açıklıklar, dudak doğrulandı |

Bulunan ve düzeltilen hatalar:
1. Taban kesiliyordu — iç boşluk z=−1'den başlıyordu, kutu tabansız çıkıyordu
2. Kapak havalandırması 5 kanal yerine 1 kanal basılıyordu
3. Vida kulakları standoff ve plaketle çakışıyordu
4. Montaj kulakları kablo çıkışını kapatıyordu (Y → X duvarına taşındı)
