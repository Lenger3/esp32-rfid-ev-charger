# Tam Bağlantı Şeması — Konnektör, Kablo, Ekran

22 kW wallbox (Tuya) · ESP32-WROOM-32U · OLED SSD1309 2.42" · RC522 · 4 buton · fan
Bu dosya **uygulama** dosyasıdır: lehim masasına bu kâğıtla otur.

---

## 0. Ekran kararı — elindeki OLED'i kullan

| | **SSD1309 OLED 2.42"** (elinde) | 1.44" ST7735 128×128 | 2.4" ILI9341 |
|---|---|---|---|
| Kontrast | Mükemmel | Orta (backlight sızması) | Orta |
| Görüş açısı | ~160° | ~120° | ~140° |
| Isı | **Yok** | Backlight ısıtır | Backlight ısıtır |
| Bellek | 1 KB buffer | 32 KB sprite | ESP32'de imkânsız |
| Pin | 7 | 8 | 14 |

Kapalı ve ısınan bir kutuda OLED kazanır: backlight yok → ekstra ısı yok, geniş açı, flex kablo.
**Tek bedeli 128×64 mono** — arayüz 4 sayfa × 4 satır olarak tasarlanır (Bölüm 11).

> Güneş altında okunmuyorsa ekranı değiştirme: kapağa **3 cm koyu renk güneşlik/tünel** yap veya
> ekranı gölgede kalan yüze al.
> Modül siyah ekranda kalırsa U8g2 constructor'ını `NONAME0` → **`NONAME2`** yap (2.42" modüllerin
> çoğu NONAME0'dır, ikisi de denenir).

---

## 1. Ana şema (mantıksal)

```
 230V AC ──[ F1 1A ]──┬──────────────────────────────────────────┐
                      │                                          │
                 ┌────┴─────────────┐                            │
                 │  12V DC PSU ≥2A  │                            │
                 └────┬────────┬────┘                            │
                   +12V│        │GND ◄──────── ortak GND barı ────┤
                      │        │                                 │
        ┌─────────────┼────────┼──────────────────┐              │
        │      ┌──────┴────────┴─────┐            │              │
        │      │ DC-DC 12V→5V 3A     │            │              │
        │      └──────┬────────┬─────┘            │              │
        │          +5V│        │GND                │              │
        │      ┌──────┴────────┴──────┐     ┌─────┴─────┐        │
        │      │  ESP32-WROOM-32U     │     │  FAN 12V  │        │
        │      │  VIN   GND   3V3     │     │  (MOSFET) │        │
        │      └──┬───┬───┬───┬───┬───┘     └─────┬─────┘        │
        │         │   │   │   │   └─ GPIO13 ──[MOSFET]───────────┘
        │    SPI  │   │   │   └───────────────┐
        │    ┌────┴───┴┐  │                   │
        │    │  OLED    │  │   ┌───────────┐   │
        │    │ SSD1309  │  └───┤  RC522    │   │
        │    └──────────┘      └───────────┘   │
        │    ┌─────────────────────────────────┴─┐
        └────┤  4× BUTON (UP / DOWN / OK / BACK)  │
             └───────────────────────────────────┘
```

Üç gerilim bölgesi, **tek ortak GND noktası** (yıldız topoloji):

| Bölge | Kaynak | Tüketici |
|---|---|---|
| 12 V | PSU | fan, buzzer (12 V ise) |
| 5 V | buck | ESP32 VIN |
| 3.3 V | ESP32 dahili regülatör | OLED, RC522 |

**Kural:** ESP32'yi şarj cihazının iç beslemesinden **beslemeyin** — ayrı PSU, ayrı sigorta.

---

## 2. Konnektör listesi (ne alacaksın)

| Kod | Nerede | Tip | Pin | Neden bu |
|---|---|---|---|---|
| **J1** | 12 V giriş | **Vidalı klemens 5.08 mm, 2 pin** | 2 | saha kablosu sökülebilir, pabuç gerekmez |
| **J2** | 12 V → buck | Vidalı klemens 5.08 mm, 2 pin | 2 | ayrılabilir |
| **J3** | buck 5 V → ESP32 | **JST-XH 2.54, 2 pin** (dişi) | 2 | ters takılamaz, titreşimde çıkmaz |
| **J4** | ESP32 → OLED | **JST-XH 2.54, 7 pin** | 7 | 7 kablo tek sokette |
| **J5** | ESP32 → RC522 | **JST-XH 2.54, 8 pin** | 8 | modülün kendi header'ına oturur |
| **J6** | ESP32 → butonlar | **JST-XH 2.54, 5 pin** | 5 | 1×GND + 4×sinyal |
| **J7** | ESP32 → fan MOSFET | JST-XH 2.54, 3 pin | 3 | GND / +12 V / gate |
| **J8** | ESP32 → buzzer | JST-XH 2.54, 2 pin | 2 | polariteli |
| **J9** | anten | **SMA dişi bulkhead** (kutuya) | 1 | IPEX→SMA pigtail |
| **J10** | buton arkası | **2.8×0.5 mm tam izole faston** | 2/buton | lehimsiz, sökülebilir |
| **J11** | (ops) DS3231 | JST-XH 4 pin | 4 | VCC/GND/SDA/SCL |
| **J12** | 12 V dallanma | **WAGO 221-412** | 2 | fan + buzzer, lehimsiz |

**Ne ALMA:** kalıcı montajda dupont jumper. Temas direnci zamanla artar, titreşimde gevşer.
Prototipte kullan, kutuya girmeden JST-XH'ye çevir.

Yedek: 2× JST-XH 7 pin, 2× 8 pin, 5× 2 pin, 1 set krimpli pin yuvası + krimp pensesi.

---

## 3. Kablo seçimi

| Hat | Kesit | Renk | Uzunluk |
|---|---|---|---|
| 12 V (+/−) | **0.75 mm² (18 AWG)** | kırmızı / siyah | ≤30 cm |
| 5 V (+/−) | 0.5 mm² (20 AWG) | turuncu / siyah | ≤20 cm |
| 3.3 V | 0.22 mm² (24 AWG) | sarı | ≤15 cm |
| SPI | 0.22 mm² | beyaz / yeşil | ≤15 cm |
| Buton sinyali | 0.22–0.5 mm² | mavi/yeşil/beyaz/gri | ≤80 cm |
| Fan | 0.5 mm² | kırmızı / siyah | ≤30 cm |
| Anten | RG178/RG316 pigtail | — | 10–15 cm |
| Toprak barı | 1.0 mm² | siyah | — |

**Renk standardı (karışmasın):**
kırmızı = +12 V · turuncu = +5 V · sarı = +3.3 V · siyah = GND ·
mavi = buton sinyali · beyaz = SPI · yeşil = CS/DC/RES · gri = sensör

---

## 4. Hangi kablolar "yapışık" (birlikte) olacak

Altın kural: **gidiş ve dönüş aynı demette, burulmuş.**
Böylece akım döngüsü alanı küçülür ve 22 kW'nin anahtarlama gürültüsü kuplaj yapamaz.

### Demet A — Güç (burgulu ikili)
```
[Kırmızı +12V] ═╦═ [Siyah GND]      ← birlikte bur (10 cm'de 2 tur)
[Turuncu +5V ] ═╦═ [Siyah GND]      ← ayrı ikili, ayrı bur
```

### Demet B — SPI (GND arada, düz)
```
[SCLK] [GND] [MOSI] [GND] [CS] [DC] [RES]
        ↑            ↑
   GND her zaman iki hızlı sinyalin arasında
```
- SCLK ve MOSI yan yana gelirse kapasitif kuplaj olur → aralarına GND koy.
- Ribbon kullanıyorsan **GND'li ribbon** (9 pin, 2. ve 5. hat GND) al.
- **MISO tek başına** — sadece RC522'den gelir, OLED yazma-only.

### Demet C — Butonlar (her sinyal kendi GND'siyle)
```
Buton 1: [Mavi  sinyal] + [Siyah GND]
Buton 2: [Yeşil sinyal] + [Siyah GND]
Buton 3: [Beyaz sinyal] + [Siyah GND]
Buton 4: [Gri   sinyal] + [Siyah GND]
```
- Her butonu kendi GND'siyle bur, sonra 4 demeti birlikte topla.
- 80 cm'yi geçme. Uzun gerekirse **4 çekirdekli burgulu kablo** (telefon tipi) kullan.
- **100 nF kondansatörü ESP32 ucuna koy**, buton ucuna değil.

### Demet D — Anten (yalnız)
```
[IPEX → SMA pigtail] ═══════════► kutu dışı
```
- **Asla güç kablosuyla birlikte gitmez, asla bükülmez.** Min bükülme yarıçapı 5 mm.
- Anten ile ESP32 arası ≤15 cm.

### Ayrım kuralları
| Kural | Neden |
|---|---|
| Buton/SPI kabloları 12 V ve fan kablosundan **≥5 cm** uzak | anahtarlama gürültüsü |
| Buton kabloları 230 V girişinden **≥15 cm** uzak | güvenlik + 50 Hz kuplaj |
| SPI kablosu RC522 bobininden geçmesin | 13.56 MHz girişim |
| Anten kablosu metal yüzeye paralel gitmesin | sönümleme |
| Tüm GND'ler tek noktada (yıldız) | toprak döngüsü yok |

---

## 5. Modül bağlantı tabloları

### 5.1 OLED M242-12864-7-V2.0 → ESP32 (**J4, I2C modu**)

Modül I2C'ye alındı (R4 + R9 = 0R takılı, **R5 sökülü**).

| # | Modül pini | ESP32 | Renk | Not |
|---|---|---|---|---|
| 1 | GND | GND | siyah | |
| 2 | VCC | 3V3 | sarı | 3.3 V (modülde boost var, 5 V da olur) |
| 3 | SCL | **GPIO 22** | mavi | donanım I2C |
| 4 | SDA | **GPIO 21** | mavi | donanım I2C |
| 5 | RES | GPIO 16 | yeşil | istemezsen 3V3'a bağla |
| 6 | DC → **A0** | **GND** | siyah | adres seçimi: **GND = 0x3C**, 3V3 = 0x3D |
| 7 | CS | **GND** | siyah | R8 0R takılıysa boş bırakılabilir |

**I2C moduna geçince değişenler:**
- **GPIO 18 ve 23 artık OLED için kullanılmıyor** → SPI hattına geri dönmeye gerek yok
- **GPIO 4 ve GPIO 5 boş kaldı** → fan (G13 yerine), buzzer veya yedek için serbest
- **GPIO 16 da boş kaldı** (RES artık modül tarafında 3V3'a bağlı) → yedek olarak serbest
- I2C hattı 21/22 → DS3231 RTC eklerseniz bu iki pini paylaşırsınız, OLED ile aynı hatta
  paralel çalışır (adres farklı: OLED 0x3C, DS3231 0x68) — çakışma olmaz

**Modül üzerinde 4.7 kΩ pull-up (R6/R7) hazır** — harici pull-up gerekmez.

> **Ekran bir kere çizip donuyorsa** (ör. sadece çerçeve görünüyor): I2C hattı birkaç
> yazımdan sonra kilitleniyor olabilir. `firmware/test/oled_diag` bunu ölçer — her yazımdan
> sonra ACK kontrolü yapar, kilitlenirse bus recovery dener ve hangi adımda koptuğunu söyler.
> Çözüm sırası: kabloyu 15 cm altına indir → **100 kHz**'de kal → SDA/SCL'ye harici 4.7 kΩ
> → kabloyu 12 V demetinden uzaklaştır.

Pin numaraları (yukarıdan aşağıya): `1 GND · 2 VCC · 3 SCL · 4 SDA · 5 RES · 6 A0 · 7 CS`

### 5.2 RC522 8 pin → ESP32 (**J5**)

| RC522 | ESP32 | Renk | Not |
|---|---|---|---|
| SDA (SS) | **GPIO 5** | yeşil | CS — *I2C'ye geçişte 22'den buraya taşındı* |
| SCK | GPIO 18 | beyaz | SPI SCLK |
| MOSI | GPIO 23 | beyaz | SPI MOSI |
| MISO | GPIO 19 | beyaz | SPI MISO |
| IRQ | — | — | kullanılmıyor (polling) |
| GND | GND | siyah | |
| RST | GPIO 27 | yeşil | |
| 3.3V | 3V3 | sarı | **5 V YASAK — anında ölür** |

> **Neden CS = GPIO 5?** OLED I2C'ye geçtiği için GPIO 5 (eski OLED CS) boşaldı. GPIO 22
> artık I2C SCL olduğu için RC522 CS orada olamazdı. GPIO 5 boot sırasında yukarı çekili
> olduğundan CS'in "pasif = HIGH" durumuyla da uyumlu.

Ek: bobin tarafına bakacak şekilde arkasına **ferrit levha**.

### 5.3 Butonlar (**J6**, 5 pin tek soket)

| Pin | Bağlantı | Renk |
|---|---|---|
| 1 | GND (ortak) | siyah |
| 2 | GPIO 32 — UP (yeşil buton) | mavi |
| 3 | GPIO 33 — DOWN (siyah buton) | yeşil |
| 4 | GPIO 25 — OK (kırmızı buton) | beyaz |
| 5 | GPIO 26 — BACK (yeşil buton) | gri |

Her buton: bir bacak GND, diğer bacak GPIO (aktif LOW).
GPIO–3.3 V arası 10 kΩ, GPIO–GND arası 100 nF (kondansatör ESP32 ucunda).

### 5.4 Fan + MOSFET (**J7**)

```
   +12V ────────────────┬──────────┐
                        │          │
                     [FAN +]       │
                     [FAN −]───┐   │
                               │   │
                         ┌─────┴───┴──┐
                         │  D      S  │  IRLZ44N
  GPIO13 ──[1kΩ]── G ────┤            │
             │           └─────┬──────┘
          [10kΩ]                │
             │                  │
            GND ────────────────┘
```
- `D` = fan eksi, `S` = GND, `G` = GPIO13 (1 kΩ seri)
- Fan endüktif → **1N4007 paralel** (katot +12 V, anot D). Yoksa MOSFET ölür.
- 10 kΩ gate–GND: ESP32 açılmadan fan kapalı kalsın.

### 5.5 Anten (**J9**)

`ESP32-32U IPEX` → `IPEX/U.FL → SMA dişi pigtail 10 cm` → `SMA bulkhead` → `2.4 GHz 3–5 dBi anten`
Pigtail kutu içinde serbest, gergin olmasın.

### 5.6 (Opsiyonel) DS3231 (**J11**)

| DS3231 | ESP32 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 21 (OLED ile aynı hat, adres farklı: 0x68) |
| SCL | GPIO 22 (OLED ile aynı hat) |

> Önerim: RTC'yi hiç eklememek. Saati VPS damgalasın — elektrik kesintisi hiçbir şeyi bozmaz.

---

## 6. Pin özet tablosu (masaya as)

| GPIO | Görev |
|---|---|
| 4 | **boş** (fan / buzzer için serbest) |
| 5 | RC522 CS |
| 13 | Fan PWM (MOSFET) |
| 14 | Buzzer |
| 16 | OLED RES |
| 18 | SPI SCLK (RC522) |
| 19 | SPI MISO (RC522) |
| 21 | **I2C SDA (OLED)** |
| 22 | **I2C SCL (OLED)** |
| 23 | SPI MOSI (RC522) |
| 25 | Buton OK |
| 26 | Buton BACK |
| 27 | RC522 RST |
| 32 | Buton UP |
| 33 | Buton DOWN |
| VIN | +5 V (buck) |
| 3V3 | OLED + RC522 |
| GND | ortak bar |

**Yasak:** 6–11 (flash), 0/2/12/15 (boot strap), 34–39 (sadece giriş, dahili pull-up yok).

---

## 7. Güç bütçesi

| Yük | Tipik | Tepe |
|---|---|---|
| ESP32 (Wi-Fi) | 120 mA | **500 mA** |
| OLED | 40 mA | 50 mA |
| RC522 | 26 mA | 30 mA |
| **5 V toplam** | ~190 mA | **~580 mA** |
| Fan 12 V | 150 mA | 250 mA |
| **12 V toplam** | ~0.4 A | ~0.9 A |

3 A buck ve 2 A PSU rahat. **Kapasitör:** 100 µF buck çıkışı, 100 µF + 100 nF ESP32 VIN,
100 nF RC522 3.3 V, 100 nF her buton.

---

## 8. Kutu içi yerleşim

```
┌──────────────────────────────────────────┐
│  [OLED 2.42"]        [RC522 + ferrit]    │ ← kapak iç yüzü / ön panel
│   (pencere aç)        (KARTI BURAYA)     │
│                                          │
│  ─── düşük gerilim bölmesi (5 V) ──────  │
│  [ESP32-32U + anten]  [buck 12→5]        │
│  [buton kabloları]    [sigorta]          │
│  ═════ bölme sacı (varsa) ══════════════ │
│  ─── güç bölmesi (12 V / 230 V) ───────  │
│  [12 V PSU]  [fan]  [klemensler]         │
│  [22 kW wallbox]                         │
└──────────────────────────────────────────┘
```
- RC522 ile güç bölmesi arası **≥10 cm**, arkasında ferrit
- Anten metalden ve güç kablolarından uzak, kutu dışına yakın
- Fan **egzoz** yönünde, giriş menfezi altta
- Kart noktası kapağa etiketlenir — RC522 menzili 0.5–1 cm

---

## 9. Montaj sırası

1. **Buck tek başına:** 12 V ver, çıkış 4.9–5.1 V (yüksüz).
2. **ESP32:** VIN'den besle (USB'yi takma), seri port 115200 açılıyor mu.
3. **OLED:** "MERHABA" yaz. Görünmüyorsa `NONAME2` varyantını dene.
   **Bilinen davranış:** ilk I2C taraması boş dönebilir (stuck-bus). İkinci tam tarama
   bulur — her başarısız işlem hatta clock darbesi üretip hattı açar. En az iki kez
   tam tarama yap. Doğrulanmış kod: `firmware/test/oled_i2c_test` (`scanBothOrders`).
4. **RC522:** `dumpVersion()` → `0x91`/`0x92`. Kart okut → seri portta UID.
5. **Butonlar:** her birine bas → **tek** olay. Debounce 40 ms.
6. **Fan:** PWM %0 duruyor, %50 yarı, %100 tam. Gate boştayken durmalı.
7. **Tuya TCP:** `status()` oku (LAN IP gerekli, evde).
8. **Hepsi:** 10 dk çalıştır, brownout var mı (ESP32 reset).
9. **Kutu:** JST'leri tak, kablo bağlarını kes, etiketle, kapak kapat.

---

## 10. Plan dosyasına etkisi (delta)

`firmware/platformio.ini`:
- `TFT_eSPI` → **`olikraus/U8g2@^2.35`**
- `Adafruit PN532` → **`miguelbalboa/MFRC522@^1.4.11`**
- Ekran define'ları silinir, yerine U8g2 constructor sabiti
- `TFT_eSprite` / 32 KB heap derdi yok (U8g2 buffer 1 KB)
- `evcore` mantığı değişmez; `ui.cpp` 128×64 mono için sıfırdan yazılır

---

## 11. Ekran düzeni (128×64 · 6×10 font · 21 karakter)

```
SAYFA 0 BEKLEME           SAYFA 1 ŞARJ
┌───────────────────┐    ┌───────────────────┐
│WALLBOX-22kW   *   │    │AYSE         16 A  │
│                   │    │    6.42 kWh       │
│   KART OKUTUN     │    │10.9 kW    41 C    │
│                   │    │00:37   [OK]durdur │
│Bugun 12.4 kWh     │    │████████░░  64%    │
└───────────────────┘    └───────────────────┘

SAYFA 2 KULLANICI         SAYFA 3 SISTEM
┌───────────────────┐    ┌───────────────────┐
│AYSE               │    │192.168.1.77  -58  │
│Kart ****A3F2      │    │Tuya OK  proto 3.3 │
│Bu ay  42.1 kWh    │    │FW 0.1.0  up 10:37 │
│Kalan  57.9 kWh    │    │Hata: yok          │
│Tutar 109.46 TL    │    │Son sync 20:31     │
└───────────────────┘    └───────────────────┘
```
- Türkçe glif yok → `ı→i ş→s ğ→g ü→u ö→o ç→c İ→I Ç→C Ş→S` (`ui_text_tr()`)
- 21 karakteri geçen isim `AYSE…` gibi kısaltılır
- Sadece değişen satır çizilir, tam refresh yapılmaz
- 60 sn dokunulmazsa `setContrast(30)`, butona basınca 200

---

## 12. Sipariş listesi (bu şemaya özel)

```
[ ] 2.4 GHz IPEX/U.FL anten 3-5 dBi                        ×1
[ ] IPEX → SMA dişi bulkhead pigtail 10 cm                 ×1
[ ] DC-DC 12V→5V 3A (XL4015 veya MP1584)                   ×1
[ ] 12V DC PSU ≥2A                                         ×1
[ ] Vidalı klemens 5.08 mm 2 pin                           ×3
[ ] JST-XH 2.54 dişi soket seti (2/3/4/5/7/8 pin)          ×1 set
[ ] Krimp pensesi + yedek pin yuvaları                     ×1
[ ] Ferrit levha 30×30 yapışkanlı                          ×1
[ ] 10 kΩ 0.25W direnç                                     ×4
[ ] 100 nF seramik 50V                                     ×6
[ ] 100 µF 25V elektrolitik                                ×2
[ ] IRLZ44N + 1N4007 + 1 kΩ + 10 kΩ                        ×1 set
[ ] 12V 80 mm fan + ızgara                                 ×1
[ ] Aktif buzzer 3.3V                                      ×1
[ ] DS3231 RTC + CR2032 (opsiyonel)                        ×1
[ ] 0.75 mm² kırmızı/siyah 1 m                             ×1'er
[ ] 0.5 mm² turuncu 1 m                                    ×1
[ ] 0.22 mm² çok renkli 2 m                                ×1
[ ] 4 çekirdekli burgulu kablo 2 m (buton demeti)          ×1
[ ] M16 kablo rakoru                                       ×4
[ ] M3 standoff + vida seti                                ×1
[ ] Delikli pertinaks 5×7 cm                               ×2
[ ] Isı büzüşmeli makaron + pabuç seti                     ×1
[ ] 1A cam sigorta + yuva                                  ×1
```
