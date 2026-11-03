# Lehim Koordinat Tablosu

Plaket 100 x 100 mm · 38 x 38 delik · adim 2.54 mm
Ilk delik merkezi sol ust koseden **1.74 mm** iceride.

## Header'lar (satir 2)

| Header | Kolon | Delik (x, y) mm | Pin sayisi |
|---|---|---|---|
| J2  RC522 (1x8) | 3–10 | (9.36, 6.82) → (27.14, 6.82) | 8 |
| J3  BUTON (1x5) | 13–17 | (34.76, 6.82) → (44.92, 6.82) | 5 |
| J4  OLED i2C (1x4) | 20–23 | (52.54, 6.82) → (60.16, 6.82) | 4 |
| J5  YEDEK (1x4) | 26–29 | (67.78, 6.82) → (75.40, 6.82) | 4 |

## Pin pin eslesme

### J2  RC522 (1x8)

| Pin | Sinyal | GPIO | Delik (x, y) mm |
|---|---|---|---|
| 1 | SDA | G5 | (9.36, 6.82) |
| 2 | SCK | G18 | (11.90, 6.82) |
| 3 | MOSI | G23 | (14.44, 6.82) |
| 4 | MISO | G19 | (16.98, 6.82) |
| 5 | IRQ | - | (19.52, 6.82) |
| 6 | GND | - | (22.06, 6.82) |
| 7 | RST | G27 | (24.60, 6.82) |
| 8 | 3V3 | - | (27.14, 6.82) |

### J3  BUTON (1x5)

| Pin | Sinyal | GPIO | Delik (x, y) mm |
|---|---|---|---|
| 1 | GND | - | (34.76, 6.82) |
| 2 | UP | G32 | (37.30, 6.82) |
| 3 | DOWN | G33 | (39.84, 6.82) |
| 4 | BACK | G26 | (42.38, 6.82) |
| 5 | OK | G25 | (44.92, 6.82) |

### J4  OLED i2C (1x4)

| Pin | Sinyal | GPIO | Delik (x, y) mm |
|---|---|---|---|
| 1 | 3V3 | - | (52.54, 6.82) |
| 2 | GND | - | (55.08, 6.82) |
| 3 | SCL | G22 | (57.62, 6.82) |
| 4 | SDA | G21 | (60.16, 6.82) |

### J5  YEDEK (1x4)

| Pin | Sinyal | GPIO | Delik (x, y) mm |
|---|---|---|---|
| 1 | +12V | - | (67.78, 6.82) |
| 2 | GND | - | (70.32, 6.82) |
| 3 | G13 | PWM | (72.86, 6.82) |
| 4 | G14 | BZR | (75.40, 6.82) |

## Raylar

- **GND**: satir 21, kolon 4–32 → (11.90, 55.08) → (83.02, 55.08)
- **3V3**: satir 23, kolon 4–32 → (11.90, 60.16) → (83.02, 60.16)

## ESP32 soketi

- satir 7: kolon 10–24 → (27.14, 19.52) → (62.70, 19.52) · 15 pin
- satir 16: kolon 10–24 → (27.14, 42.38) → (62.70, 42.38) · 15 pin

Iki sira arasi: **22.86 mm** (9 delik)

## Guc bolgesi

- J1 12 V: (6.82, 75.40) — 2 pin klemens, dikey
- J2 12 V: (19.52, 75.40) — 2 pin klemens, dikey
- Buck: merkez (52.54, 72.86), ~36 x 18 mm alan
- Montaj delikleri: (5.80, 5.80), (94.20, 5.80), (5.80, 94.20), (94.20, 94.20)

## Malzeme sayimi

| Parca | Adet | Kaynak |
|---|---|---|
| Disi header pini (ESP32 soketi) | 30 | 1x6 disi ×5 (kesilir) |
| Erkek header pini (J2–J5 + raylar) | 37 | 1x40 erkek (kesilir) |
| 2 pin 5.08 klemens | 2 | J1, J2 |

