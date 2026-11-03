# 100×100 mm Plaket Yerleşimi + Kutu Tasarım Girdisi

Plaket: **100 × 100 mm**, 2.54 mm adım → **38 × 38 delik** (kolon 0–37, satır 0–37).
Delik (kolon, satır) olarak veriliyor; 1 delik = 2.54 mm.

---

## 1. Bölge haritası

```
satır  kol→  0    5    10   15   20   25   30   35  37
  0   ┌────────────────────────────────────────────────┐
  2   │   [RC522 1×8] [BTN 1×5] [OLED 1×4] [YEDEK 1×4] │  ← C: konnektör sırası
  5   │                                                │
  6   │        ┌──────────────────────┐                │
      │        │                      │                │
 11   │        │      ESP32 2×15      │                │  ← B: kontrolcü
      │        │       (soket)        │                │
 17   │        └──────────────────────┘                │
 21   │   ●━━━━━━━━━━ GND barı ━━━━━━━━━━●             │  ← D: raylar
 23   │   ●━━━━━━━━━━ 3V3 rayı ━━━━━━━━━━●             │
      │                                                │
 26   │  [J1]  [J2]        ┌──────────────────┐        │
      │  12V   12V         │   BUCK 12→3.3V   │        │  ← E: güç
 34   │  klemens klemens   └──────────────────┘        │
 37   └────────────────────────────────────────────────┘
```

| Bölge | Satır | Kolon | İçerik |
|---|---|---|---|
| **C** Konnektör sırası | 2 | 2–35 | RC522, butonlar, OLED, yedek |
| **B** ESP32 | 6–17 | 7–27 | 2×15 dişi soket (pin sıraları: satır 7 ve 16) |
| **D** Raylar | 21, 23 | 4–32 | GND barı ve 3V3 rayı, paralel |
| **E** Güç | 26–34 | 2–30 | 12 V klemensler + buck |
| **A** Giriş | 26–34 | 0–10 | Kablolar kutuya buradan çıkar |

**Ayrım:** C sırası ile B arasında 4 delik (10 mm), B ile E arasında 9 delik (23 mm).
12 V bölgesi ile ESP32 arası **23 mm** — SPI'den yeterince uzak.

---

## 2. Delik koordinatları (lehim planı)

### 2.1 Üst konnektör sırası — satır 2

| Header | Kolonlar | Pin sayısı | Görev |
|---|---|---|---|
| 1×8 erkek | 3–10 | 8 | RC522 |
| 1×5 erkek | 13–17 | 5 | Butonlar (GND + 4 sinyal) |
| 1×4 erkek | 20–23 | 4 | OLED i2C (3V3, GND, SCL, SDA) |
| 1×4 erkek | 26–29 | 4 | Yedek (+12V, GND, G13, G14) |

Aralar: 2 delik (5.08 mm) → konnektörler birbirine değmez, kablo soketi rahat takılır.

**Pin eşlemesi (kolon sırası):**

```
RC522  (kol 3..10)      BTN (kol 13..17)
 3 → SDA  = GPIO5       13 → GND (ortak)
 4 → SCK  = GPIO18      14 → UP    GPIO32
 5 → MOSI = GPIO23      15 → DOWN  GPIO33
 6 → MISO = GPIO19      16 → BACK  GPIO26
 7 → IRQ  (kes)         17 → OK    GPIO25
 8 → GND
 9 → RST  = GPIO27
10 → 3.3V

OLED i2C (kol 20..23)   YEDEK (kol 26..29)
20 → 3V3                26 → +12V (fan)
21 → GND                27 → GND
22 → SCL  = GPIO22      28 → GPIO13 (fan PWM)
23 → SDA  = GPIO21      29 → GPIO14 (buzzer)

OLED modul tarafinda birlesen 3 tel (plakete GITMEZ):
  RES -> 3V3 · A0 -> GND · CS -> GND
```

> **I2C'ye geçiş notu:** OLED SPI iken GPIO 18/23 paylaşılıyordu ve RC522 CS GPIO 22'deydi.
> Şimdi I2C SCL/SDA 22/21 olduğu için **RC522 CS → GPIO 5**'e taşındı (eski OLED CS).
> Kazanç: **GPIO 4 ve 5 boşaldı**, GPIO 18/23 artık sadece RC522'ye ait.

### 2.2 ESP32 soketi

- **Satır 7:** kolon 10–24 (15 pin)
- **Satır 16:** kolon 10–24 (15 pin)
- Satırlar arası 9 delik = **22.86 mm** (modül genişliği ile birebir)

Modül gövdesi: satır 6–17, kolon 7–27 (26 × 52 mm). USB ucu **kolon 7 tarafına** (sol) bakar →
kutu tasarımında sol yan yüzde USB kesiti.

**Lehim sırası:** header'ları önce ESP32'nin pinlerine tak → modülle plakete oturt → iki köşe
lehimle → modülü çıkar → hizayı kontrol et → kalanı lehimle.

### 2.3 Raylar

| Ray | Satır | Kolon | Header |
|---|---|---|---|
| GND barı | 21 | 4–32 | 1×8 dişi (kesip kullan veya 29 delik boyunca kalın lehim) |
| 3V3 rayı | 23 | 4–32 | 1×8 dişi + kalın lehim |

İki ray arası **2 delik (5.08 mm)** — kısa devre riski yok.
Her iki rayın altından **kalın lehim şeridi** çek (0.75 mm² kablo köprüsü), akım taşısın.

### 2.4 Güç bölgesi

| Parça | Satır | Kolon | Not |
|---|---|---|---|
| J1: 2 pin 5.08 klemens | 28–30 | 2 ve 4 | 12 V giriş |
| J2: 2 pin 5.08 klemens | 28–30 | 8 ve 10 | 12 V çıkış (fan / yedek) |
| Buck 12→3.3 V | 26–34 | 14–30 | modül ayak izni ~40×20 mm |
| 100 µF | 25 | 13 | buck çıkışına yakın |
| Sigorta (varsa) | 27 | 12 | J1 ile buck arası |

Buck modülü plakete **lehimlenmez**: çift taraflı bant + 2 vida veya 4 pinli dişi header.
Giriş/çıkış kabloları raylara gider.

---

## 3. Bağlantı şeması (özet)

```
   12V+ ──[sigorta]──┬── Buck IN+          Buck OUT+ ──► 3V3 rayı ──► ESP32 3V3
                     │                     Buck OUT− ──► GND barı
   GND ──────────────┴── Buck IN−                              │
                                                                ├──► OLED VCC
   ESP32 GPIO22 ───── OLED SCL  (I2C)
   ESP32 GPIO21 ───── OLED SDA  (I2C)
   ESP32 GPIO18 ───── RC522 SCK
   ESP32 GPIO23 ───── RC522 MOSI
   ESP32 GPIO19 ───── RC522 MISO
   ESP32 GPIO5  ───── RC522 SDA (CS)
   ESP32 GPIO27 ───── RC522 RST
   (RC522 IRQ bağlanmıyor — polling)
   (OLED RES->3V3, A0->GND, CS->GND : modül tarafında)
   ESP32 GPIO32 ───── Buton UP
   ESP32 GPIO33 ───── Buton DOWN
   ESP32 GPIO26 ───── Buton BACK
   ESP32 GPIO25 ───── Buton OK
   Butonlar ortak ─── GND barı
```

**Kritik kablolar:**
- SPI (18/23/19) → 15 cm'yi geçmeyecek, GND arada, 12 V demetinden ≥5 cm uzak
- RC522 GND ve 3.3V **doğrudan raylardan**, ESP32 pininden değil
- Buck çıkışı ile ESP32 3V3 arasına **jumper** koy (USB takarken çıkar)

---

## 4. Kablo uzunlukları (yeni yerleşime göre)

| Kablo | Uzunluk |
|---|---|
| Plaket → OLED | 20 cm |
| Plaket → RC522 | 20 cm |
| Plaket → butonlar | 35 cm |
| Plaket → 12 V PSU | 25 cm |
| Plaket → fan | 30 cm |
| **Servis payı** | +5 cm her birine |

---

## 5. Kutu tasarım girdisi (3D için)

### 5.1 Dış ölçüler

| Ölçü | Değer |
|---|---|
| Genişlik (W) | **135 mm** |
| Yükseklik (H) | **130 mm** |
| Derinlik (D) | **55 mm** |
| Et kalınlığı | 3 mm |
| İç hacim | 129 × 124 × 49 mm |

Gerekçe: plaket 100×100, üst kenarda 18 mm kablo dönüş payı, altta 6 mm, yanlarda 14.5 mm.

### 5.2 Plaket montajı

- Plaket **arka iç yüzeye** paralel, **8 mm standoff** ile
- Standoff konumları (arka iç yüzey, sol-alt köşeden mm):
  `(12, 26) (112, 26) (12, 118) (112, 118)` → M3 × 8 mm, 4 adet
- Plaket üst kenarı arka yüzeyin 24 mm altında → konnektör kabloları yukarı döner

### 5.3 Ön yüz (kullanıcı arayüzü)

| Öğe | Konum (sol-üstten, mm) | Ölçü |
|---|---|---|
| OLED pencere | (32, 18) merkez | **58 × 30 mm** (aktif alan 55×27.5) |
| Buton 1 — BACK | (40, 78) merkez | Ø12.2 |
| Buton 2 — DOWN | (58, 78) | Ø12.2 |
| Buton 3 — OK (kırmızı) | (76, 78) | Ø12.2 |
| Buton 4 — UP | (94, 78) | Ø12.2 |
| RC522 kart bölgesi | (67, 108) merkez | **45 × 60 mm**, et kalınlığı **1.2 mm** |
| Kart işareti | üstüne lazer/etiket | "KARTI BURAYA" |

> **RC522 duvar kalınlığı 1.2 mm olmalı.** 3 mm duvar menzili 1 cm'nin altına düşürür.
> 3D baskıda o bölgeyi 1.2 mm'ye düşür (veya pencere aç + 1 mm styrene kapak yapıştır).

### 5.4 Yan yüzler

| Yüz | İçerik |
|---|---|
| **Sol** | USB kesiti: 12 × 8 mm, plaket USB ucuna denk (arka yüzeyden 30 mm yükseklikte) |
| **Sağ** | KTS102 ana şalter için Ø12.5 mm delik, arka yüzeyden 100 mm yükseklikte |
| **Üst** | Havalandırma: 8 × 3 mm yarık × 6 adet (buck üstüne denk) |
| **Alt** | 3 × M16 kablo rakoru deliği (Ø16.2): 12 V giriş, fan çıkış, yedek |

### 5.5 Kutu içi bölmeler

- **230 V / 12 V bölmesi:** kutunun alt 30 mm'si. 1.5 mm bölme sacı, 12 V PSU burada
- **Plaket bölmesi:** üst 94 mm
- Bölme sacında 2 × Ø8 mm kablo geçişi (12 V çıkış + GND)

### 5.6 Baskı notları

| Parametre | Değer |
|---|---|
| Malzeme | PETG (ABS'den daha az çeker, ısıya dayanıklı) |
| Duvar | 3 mm (3 perde × 1.0) |
| Dolgu | %25 gyroid |
| RC522 bölgesi | 1.2 mm (yerel olarak azalt) |
| Destek | Sadece üst havalandırma yarıklarında |
| Kapak | Ayrı parça, 4 × M3 vidalı + 1 conta kanalı |

---

## 6. Montaj sırası

1. Plaketi standoff'lara sabitle (kutuya girmeden önce kabloları lehimle).
2. Üst konnektör sırasını lehimle (satır 2).
3. ESP32 soketini lehimle, modülü tak, USB ile firmware at, **sonra** güç bağla.
4. Rayları lehimle, buck'ı sabitle, 3.3 V'u ölç (3.25–3.35 V).
5. Jumper'ı çıkar → plaket güç ver → ESP32 açılıyor mu.
6. OLED, RC522, butonları kutuya monte et, kabloları plakete bağla.
7. Kutuya yerleştir, servis paylarını ayarla, kablo bağlarını kes.
8. Kapak kapat, ana şalterle aç, ekranda "KART OKUTUN" görünene kadar.

---

## 7. Çizimler

| Dosya | İçerik |
|---|---|
| `hardware/pcb_layout.svg` / `.png` | yerleşim: 38×38 delik ızgarası, header'lar, raylar, bölgeler, montaj delikleri |
| `hardware/pcb_wiring.svg` / `.png` | kablolama: aynı yerleşim + numaralı net hatları + 19 satırlık bağlantı tablosu |
| `hardware/box_pcb_drawing.svg` | kutu teknik resmi (3 görünüş) |

Yeniden üret: `python3 tools/draw_pcb.py`

Okuma sırası: önce **numaralı hatlar** (1–19), sonra alttaki tablodan aynı numaranın
kaynak/hedef satırı. Renkler sinyali ayırt eder, numara ise tek anlamlı referanstır.

## 8. Kontrol listesi

```
[ ] 38×38 delik grid, 100×100 mm — ölçüldü mü
[ ] ESP32 pin sıraları arası 22.86 mm (9 delik)
[ ] 12 V bölgesi ile ESP32 arası ≥20 mm
[ ] GND barı ile 3V3 rayı arası 2 delik, kısa devre yok
[ ] Buck çıkışı 3.3 V (±0.15 V), yükte 3.2 V üstü
[ ] USB takılırken buck 3.3 V hattı jumper ile kesiliyor
[ ] Konnektör pin 1'leri işaretli
[ ] Tüm kablolar iki uçtan etiketli
[ ] Servis payı 5 cm
```
