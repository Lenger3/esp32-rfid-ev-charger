# Bağlantı Şeması — Mevcut Parçalarla

> ## ⚠ ARŞİV — KULLANMA
> Bu dosya OLED **SPI** iken yazıldı ve güncellenmedi.
> Güncel ve doğrulanmış bağlantı: **`docs/PIN-HARITASI.md`** ve `docs/WIRING.md`
> Tek fark özeti: OLED artık I2C (SCL=G22, SDA=G21, RES=3V3, A0=GND, CS=GND),
> RC522 CS = **GPIO 5** (22 değil), RC522 IRQ bağlanmıyor.


> **GUNCELLIK NOTU:** Bu dosya OLED SPI iken yazildi. Guncel baglanti icin
> `docs/WIRING.md` ve `hardware/LEHIM-SABLONU.svg` kullanin. Buradaki tek fark:
> OLED artik I2C (G22/G21), RC522 CS = GPIO 5.

Bu dosya **sadece elindeki parçalarla** çizildi. Listeye ekleme yapılmadı.
Eksik olan kritik kalemler en sonda "Kritik" başlığı altında.

---

## 1. Elindeki parçalar ve roller

| Parça | Adet | Rol |
|---|---|---|
| ESP32-WROOM-32U | 1 | kontrol kartı (harici anten **yok**) |
| OLED SSD1309 2.42" 128×64 7 pin | 1 | ekran |
| RC522 RFID | 1 | kart okuyucu |
| 13.56 MHz RFID kart | 2 | kullanıcı kartı |
| DC180 buton yeşil / siyah ×2 / kırmızı | 4 | UP / DOWN / BACK / OK |
| KTS102 toggle 3 ayak | 1 | **ana güç anahtarı** (12 V hattı) |
| 13×23 delikli plaket | 1 | taşıyıcı |
| 2 pin 5.08 mm yeşil klemens | 2 | 12 V giriş + 12 V çıkış |
| 1×40 erkek header 23 mm | 1 | plaket üstü pinler (kesilerek) |
| 1×6 dişi header | 10 | ESP32 soketi (kesilerek) |
| 1×4 dişi header | 10 | yedek / küçük soket |
| 8 pin tunik erkek + dişi | 2+2 | RC522 ve buton demeti |
| 4 pin tunik erkek + dişi | 4+4 | güç / yedek |
| 3 pin tunik erkek + dişi | 6+6 | fan / buzzer / yedek |
| 2 pin tunik erkek + dişi | 4+4 | güç |
| 2.54 tunik pim (krimp) | 100 | konnektör pinleri |
| Kablo bağı 2.5×100 | 100 | kablo toplama |

> **Not:** "tunik konnektör" = dupont tipi soket. Modülün erkek pinleri → dişi tunik yuva →
> kablo → plaketteki erkek header. Sistem bu şekilde kurulur, modüller plakete doğrudan oturmaz
> (ESP32 hariç).

---

## 2. Pin haritası

| GPIO | Görev | Kablo rengi önerisi |
|---|---|---|
| 4 | OLED DC | yeşil |
| 5 | OLED CS | yeşil |
| 16 | OLED RES | yeşil |
| 18 | SPI SCLK → OLED D0 + RC522 SCK | beyaz |
| 19 | SPI MISO ← RC522 MISO | beyaz |
| 22 | RC522 SDA/SS (CS) | yeşil |
| 23 | SPI MOSI → OLED D1 + RC522 MOSI | beyaz |
| 27 | RC522 RST | yeşil |
| 25 | Buton OK (kırmızı) | mavi |
| 26 | Buton BACK (siyah) | gri |
| 32 | Buton UP (yeşil) | mavi |
| 33 | Buton DOWN (siyah) | gri |
| 3V3 | OLED VCC + RC522 3.3V | sarı |
| GND | ortak | siyah |
| VIN | 5 V besleme (buck/USB) | kırmızı |
| 13 | (boş — fan) | — |
| 14 | (boş — buzzer) | — |

**Yasak pinler:** 6–11 (flash), 0/2/12/15 (boot strap), 34–39 (sadece giriş).

---

## 3. OLED SSD1309 7 pin → ESP32

| OLED | ESP32 | Tunik |
|---|---|---|
| VCC | 3V3 | 8 pin dişi, pin 1 |
| GND | GND | pin 2 |
| D0 (SCLK) | GPIO 18 | pin 3 |
| D1 (MOSI) | GPIO 23 | pin 4 |
| RES | GPIO 16 | pin 5 |
| DC | GPIO 4 | pin 6 |
| CS | GPIO 5 | pin 7 |
| — | (boş) | pin 8 |

Modül üzerindeki **BS0/BS1 jumper'ı SPI konumunda** olmalı. I2C'de kalırsa ekran hiç açılmaz.

---

## 4. RC522 8 pin → ESP32

| RC522 | ESP32 | Tunik (8 pin) |
|---|---|---|
| SDA (SS) | GPIO 22 | pin 1 |
| SCK | GPIO 18 | pin 2 |
| MOSI | GPIO 23 | pin 3 |
| MISO | GPIO 19 | pin 4 |
| IRQ | — | pin 5 (kesilir) |
| GND | GND | pin 6 |
| RST | GPIO 27 | pin 7 |
| 3.3V | 3V3 | pin 8 |

**3.3 V zorunlu. 5 V verilirse modül anında ölür.**

---

## 5. Butonlar (4 adet DC180)

Hepsi **momentary (anlık)**, NO kontak. Kullanılmayan bacağa dokunma.

| Buton | Renk | GPIO | Tunik (8 pin) |
|---|---|---|---|
| UP | yeşil | GPIO 32 | pin 2 |
| DOWN | siyah | GPIO 33 | pin 3 |
| BACK | siyah | GPIO 26 | pin 4 |
| OK | kırmızı | GPIO 25 | pin 5 |
| ortak GND | — | GND | pin 1 |

Bağlantı: bir bacak **GND**, diğer bacak **GPIO** (aktif LOW).
Kodda `pinMode(pin, INPUT_PULLUP)` + 40 ms yazılım debounce. **Harici direnç yok, gerek de yok.**

---

## 6. KTS102 toggle anahtar (ana güç)

3 ayaklı: **orta ayak** giriş, **dış ayaklar** çıkış (ON konumunda orta–üst birleşir).

```
12V+ ──► [orta ayak]  KTS102  [üst ayak] ──► buck girişi + fan (varsa)
GND ──────────────────────────────────────► ortak GND barı
```

Alt ayağı boş bırak (kullanılmıyor) veya "OFF konumunda LED" için kullan.

---

## 7. Plaket yerleşimi (13×23)

```
┌─────────────────────────────────────────────────┐
│ [J1 12V giriş]  [KTS102 switch]                 │
│  5.08 klemens      (plaket dışına, kutuya)      │
│                                                 │
│  ┌──────────────┐         ┌──────────────────┐  │
│  │   ESP32      │         │ 1×8 erkek header │  │
│  │  2×15 dişi   │         │   → RC522        │  │
│  │  soket       │         ├──────────────────┤  │
│  │              │         │ 1×8 erkek header │  │
│  │              │         │   → butonlar     │  │
│  └──────────────┘         ├──────────────────┤  │
│                           │ 1×7 erkek header │  │
│  [3V3 bar]  [GND bar]     │   → OLED         │  │
│                           └──────────────────┘  │
│                                                 │
│  (buck modülü için boş alan — kutu içine)       │
└─────────────────────────────────────────────────┘
```

**ESP32 soketi nasıl yapılır (elindeki 1×6 dişi header'larla):**
Her sıra 15 pin. Sırayı **6 + 6 + 3** olarak böl:
- 4 adet 1×6 header tam kullanılır
- 1 adet 1×6 header ortadan kesilir → iki adet 1×3

Yerleştirme: iki 1×15 sıra arası **22.86 mm** (9 delik).
Püf nokta: dişi header'ları önce ESP32'nin pinlerine tak, modülle birlikte plakete oturt,
iki köşeyi lehimle, modülü çıkar, hizayı kontrol et, sonra kalanı lehimle.

**GND barı:** plaketin ortasında 5–6 delikli tek şerit. ESP32 GND, OLED GND, RC522 GND,
buton ortak GND, 12 V GND — hepsi buraya. Yıldız topoloji, döngü yok.

---

## 8. Kablo demetleri (elindeki tunik konnektörlerle)

| Demet | Konnektör | İçerik |
|---|---|---|
| OLED | 8 pin dişi→erkek | 7 kablo + 1 boş |
| RC522 | 8 pin dişi→erkek | 8 kablo |
| Butonlar | 8 pin dişi→erkek | GND + 4 sinyal + 3 boş |
| 12 V giriş | 2 pin 5.08 klemens | +12 V / GND |
| Yedek | 4 pin, 3 pin, 2 pin | fan, buzzer, RTC |

**Kablolama kuralı:** her buton sinyalini kendi GND'siyle birlikte bur (10 cm'de 2 tur).
SPI kablolarını 15 cm'yi geçirme, 12 V kablosundan ≥5 cm uzak tut.

---

## 9. Kritik (bunlar olmadan çalışmaz)

1. **12 → 3.3 V buck YOK listede.** ESP32'yi 12 V'tan besleyemezsin. Buck **≥1.5 A** olmalı
   (Wi-Fi tepe 500 mA + OLED 40 + RC522 30). Çıkış ESP32'nin **3V3** pinine gider, VIN'e değil.
   USB'yi takarken 3.3 V hattını kes (devkit regülatörüyle çakışır).
2. **Anten YOK.** ESP32-WROOM-**32U**'da dahili anten bulunmaz; IPEX/U.FL 2.4 GHz anten olmadan
   Wi-Fi hiç çekmez.
3. **Kablo YOK** (listede sadece kablo bağı var). Tunik konnektörlere krimp yapmak için de
   **krimp pensesi** gerekir; yoksa pinleri lehimlemek gerekir.
4. **Plaket ölçüsü teyit:** "13×23" **cm** ise bol bol yeter. **Delik** ise (33×58 mm) ESP32 tek
   başına 26×52 mm olduğu için neredeyse hiç yer kalmaz.
5. **RC522 çok yakın çalışır (1–2 cm, kapalı kutuda 0.5–1 cm).** Kartı kapağın dışına işaretli
   noktaya tutturmak zorundasın; menzil yetmezse PN532'ye geçilir.
6. **Fan / MOSFET yok.** Sistem çalışır ama 22 kW kutusunda sıcaklık `temp_current` DP'sinden
   izlenmeli; 60 °C üzerinde akım düşürülmeli.
7. **RFID kart 2 adet.** 3 kişi planı için 1 kart eksik.