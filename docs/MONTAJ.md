# Montaj ve Kablolama Planı

Mevcut parçalarla. Plaket: 13×23 (cm varsayımı — **delik** ise Bölüm 9'a bak).
Kutuya giden: 4 buton, OLED, RC522, 12 V PSU. Plakete giden: ESP32 + konnektörler + raylar.

---

## 1. Bölge mantığı (neden böyle)

Plaketi 4 bölgeye ayır. Amaç: **12 V ile SPI hiçbir yerde yan yana gelmesin**, kablo çıkışları
tek kenarda toplansın, arıza aramada "hangi bölge" sorusu tek bakışta cevaplansın.

```
        ┌──────────────────────────────────────────────┐ 13 cm
   2 cm │  A: GÜÇ GİRİŞİ                               │
        │  [J1 12V klemens] [KTS102] [sigorta] [J2 fan]│
        ├──────────────────────────────────────────────┤
        │                                              │
        │  B: KONTROLCÜ        C: KONNEKTÖR KENARI     │
        │  ┌────────────┐      ┌─────────────────┐     │
        │  │  ESP32     │      │ 1×8 → RC522     │     │
        │  │  2×15 dişi │      ├─────────────────┤     │
        │  │  soket     │      │ 1×8 → butonlar  │     │
        │  │            │      ├─────────────────┤     │
        │  └────────────┘      │ 1×4 → OLED i2C  │     │
        │                      ├─────────────────┤     │
        │  D: RAYLAR           │ 1×4 yedek       │     │
        │  [3V3 rayı]          └─────────────────┘     │
        │  [GND barı]                                  │
        ├──────────────────────────────────────────────┤
        │  E: BUCK / YEDEK                             │
        │  [buck 12→3.3]  [100µF]  [boş alan]          │
        └──────────────────────────────────────────────┘
                          23 cm
```

| Bölge | İçerik | Kural |
|---|---|---|
| A | 12 V giriş klemensi, toggle, sigorta, fan çıkışı | Plaketin **kısa kenarında**, kablolar kutuya buradan girer |
| B | ESP32 soketi | Bölge A'dan **≥4 cm** uzak |
| C | Tüm sinyal konnektörleri | Plaketin **uzun kenarında**, tek hizada |
| D | 3V3 rayı + GND barı | İkisi **paralel iki şerit**, araları ≥2 delik |
| E | Buck, kondansatör, yedek | Bölge A'ya yakın (12 V buraya gelir) |

---

## 2. Plakete ne monte edilecek

| # | Parça | Montaj | Konum |
|---|---|---|---|
| 1 | ESP32 2×15 dişi soket | lehim | B bölgesi |
| 2 | 1×8 erkek header → RC522 | lehim + modül üstüne | C, en üst |
| 3 | 1×5 erkek header → butonlar | lehim | C, 2. sıra |
| 4 | 1×4 erkek header → OLED i2C | lehim | C, 3. sıra |
| 5 | 1×4 erkek header (yedek) | lehim | C, en alt |
| 6 | J1: 2 pin 5.08 klemens (12 V giriş) | lehim | A, sol |
| 7 | J2: 2 pin 5.08 klemens (12 V çıkış → PSU/fan) | lehim | A, sağ |
| 8 | KTS102 toggle | **plaket dışı**, kutu yüzeyine | A'nın yanı |
| 9 | Buck 12→3.3 V | çift taraflı bant + 2 vida veya 4 pinli header | E |
| 10 | 3V3 rayı | tek sıra erkek header (1×6, 1×4) | D |
| 11 | GND barı | tek sıra erkek header (1×8) | D |

**Plakete monte EDİLMEYECEK:** OLED, RC522, butonlar, 12 V PSU. Hepsi kutuya.

---

## 3. Kutuya ne monte edilecek

| Parça | Nereye | Detay |
|---|---|---|
| **OLED** | Kapak iç yüzü | 4 vida veya M3 standoff + pencere aç (ekran camı kapağa yapışır) |
| **RC522** | Kapak iç yüzü, OLED'in yanı | 2 vida, bobin tarafı kapağa baksın; **ferrit** arkaya |
| **4 buton** | Kapak veya ön panel | 4× **12 mm delik**, DC180 vida somunu ile |
| **12 V PSU** | Kutu tabanı, güç bölmesi | 4 vida veya DIN rayı; 230 V tarafı ayrı bölmede |
| **Plaket** | Taban, 4× M3 standoff (10 mm) | ESP32 USB ucu kutu kenarına bakacak |
| **KTS102** | Kutu dış yüzeyi | Ana şalter, erişilebilir olsun |

**Buton yerleşimi (kapakta, soldan sağa):**
```
[ BACK(siyah) ] [ DOWN(siyah) ] [ OK(kırmızı) ] [ UP(yeşil) ]
```
Kırmızı **OK** ortada-alt: en çok basılan tuş, başparmakla bulunur.

---

## 4. Kablolama — sistematik sıra

### 4.1 Kablo kesim tablosu (önce kes, etiketle, sonra lehimle)

| Kablo | Uç 1 | Uç 2 | Uzunluk | Kesit |
|---|---|---|---|---|
| K1 | J1 klemens + | Buck IN+ | 12 cm | 0.5 mm² |
| K2 | J1 klemens − | Buck IN− / GND barı | 12 cm | 0.5 mm² |
| K3 | Buck OUT+ 3V3 | 3V3 rayı | 10 cm | 0.22 mm² |
| K4 | Buck OUT− | GND barı | 10 cm | 0.22 mm² |
| K5 | 3V3 rayı | ESP32 3V3 | 8 cm | 0.22 mm² |
| K6 | GND barı | ESP32 GND | 8 cm | 0.22 mm² |
| K7–K9 | ESP32 18/23/19 | SPI header (RC522) | 10 cm | 0.22 mm² |
| K10–K11 | ESP32 4/5/16/22/27 | OLED + RC522 kontrol | 10 cm | 0.22 mm² |
| K12 | 8 pin dişi | RC522 modülü | 20 cm | 0.22 mm² |
| K13 | 7 pin dişi | OLED modülü | 20 cm | 0.22 mm² |
| K14 | 8 pin dişi | 4 buton + GND | 50 cm | 0.22 mm² |
| K15 | J2 klemens | Fan (varsa) | 30 cm | 0.5 mm² |

**Kural:** her kabloyu **iki ucu da etiketle** (`K7`, `SPI-CLK`). Etiket olmadan 3. gün çözemezsin.

### 4.2 Demetler

**Demet 1 — Güç (12 V):** K1 + K2 birlikte, **burulmuş** (10 cm'de 2 tur).
Plaketin A bölgesinden E bölgesine, **sol kenardan** gider.

**Demet 2 — 3V3/GND:** K3 + K4 birlikte burulmuş, E → D arası, kısa.

**Demet 3 — SPI:** K7/K8/K9 düz, **GND arada** olacak şekilde: `SCLK · GND · MOSI`.
B bölgesinden C bölgesine, **plaketin ortasından** geçer, 12 V demetine ≥5 cm uzak.

**Demet 4 — Sinyal:** K10–K14, tek demet hâlinde C kenarından kutu içine çıkar.
RC522 ve OLED kabloları **15 cm'yi geçmesin**. Butonlar 50 cm'ye kadar serbest.

**Demet 5 — Anten (varsa):** tek başına, hiçbir demetle birleşmez.

### 4.3 Kablo bağı disiplini

- Her demeti **5 cm'de bir** bağla, gergin değil hafif boşluklu (titreşimde kesilmesin).
- Kablo bağının keskin ucu **içeri baksın**, dışarıda kalmasın (kapak kapanınca sıkışır).
- Demetleri plakete **yapıştırma**; plaket deliklerinden geçir veya plaket kenarındaki
  bağ deliklerinden kullan.

### 4.4 Gerinim azaltma (strain relief)

- Tüm tunik konnektörlerde kablo–soket birleşimine **1 damla sıcak silikon** veya ısı büzüşmeli
  makaron. Dupont soketleri zamanla gevşer.
- Kutu girişinde kablolar **M16 rakor** veya kablo bağı ile sabitlenmeli; çekilince lehim kopmasın.
- Her kabloda **servis payı** bırak: 3–5 cm fazla, kapak açıldığında gerginlik olmasın.

---

## 5. Plaket üzeri lehim planı (sırayla)

**Adım 1 — Raylar (önce bunlar):**
1. GND barı: 1×8 dişi header, plaketin D bölgesine yatay.
2. 3V3 rayı: 1×6 dişi header, GND'nin 2 delik üstüne (paralel).
   İkisinin altından **kalın lehim köprüsü** çek (raylar birbirine değmesin).

**Adım 2 — ESP32 soketi:**
3. İki 1×15 hattı hazırla (1×6 ×2 + 1×3 = 15).
4. Header'ları **önce ESP32'nin pinlerine** tak, modülle birlikte plakete oturt.
5. İki köşe bacağı lehimle → modülü çıkar → **hiza kontrolü** (22.86 mm) → kalanı lehimle.
   *(Bu sıra atlanırsa 0.5 mm kayma olur ve modül zorlanır.)*

**Adım 3 — Konnektör kenarı:**
6. 1×8 (RC522), 1×5 (buton), 1×4 (OLED i2C), 1×4 (yedek) — dikey hizada, 2 delik ara.
7. Her header'ın 1. bacağını plaket altından **işaretle** (pin 1 = kırmızı nokta).

**Adım 4 — Güç bölgesi:**
8. J1 ve J2 klemensleri A bölgesine, 2 delik ara.
9. 12 V girişe **1 A sigorta** (varsa) veya jumper.

**Adım 5 — Buck:**
10. Buck'ı E bölgesine çift taraflı bantla sabitle, çıkış kablolarını raylara lehimle.
11. Buck çıkışına **100 µF** (kutupsuz ters bağlamayın: şeritli bacak +).

**Adım 6 — Test (lehim bittikten sonra, modüller takılmadan):**
12. 12 V ver → 3V3 rayında **3.25–3.35 V** ölç.
13. GND barı ile 3V3 rayı arası **kısa devre yok** (multimetre süreklilik → ötmemeli).
14. KTS102 OFF → 0 V, ON → 3.3 V. Toggle doğru bağlanmış mı.

---

## 6. Kutu içi kablo güzergâhı

```
     ┌─────────────────────────────────────────┐
     │  [OLED]        [RC522]      [buton×4]   │  ← kapak
     │     │             │             │       │
     │     └──────┬──────┴──────┬──────┘       │
     │            │  DEMET 4    │              │
     │        ┌───▼─────────────▼───┐          │
     │        │   C: konnektörler   │          │
     │        │   B: ESP32          │          │
     │        │   D: raylar         │          │
     │        │   E: buck           │          │
     │        │   A: 12V [KTS102]   │          │
     │        └─────────┬───────────┘          │
     │                  │ DEMET 1              │
     │        ┌─────────▼─────────┐            │
     │        │  12 V PSU         │            │
     │        │  ── bölme sacı ── │            │
     │        │  230 V giriş      │            │
     │        └───────────────────┘            │
     └─────────────────────────────────────────┘
```

- **230 V bölmesi en altta**, ayrı kapak veya bölme sacı ile.
- Demet 4 (sinyal) **sağ kenardan** yukarı, Demet 1 (güç) **sol kenardan** aşağı. Kesişmesinler.
- Kablolar **kapak menteşe hattından** geçmesin — kapağı 20 kez açınca kopar.
- Servis payı: kapak açıldığında demetlerde 5 cm boşluk.

---

## 7. Etiketleme düzeni

| Yer | Etiket |
|---|---|
| Her kablo iki uçta | `K7 / SPI-CLK`, `K14 / BTN-GND` |
| Her header pin 1 | kırmızı nokta (marker) |
| Konnektör gövdesi | `OLED`, `RC522`, `BTN`, `PWR` |
| Kutu içi demet | `SIG`, `PWR`, `ANT` |
| Plaket köşesi | yapım tarihi + firmware versiyonu |

---

## 8. Nihai kontrol listesi (kapak kapanmadan)

```
[ ] 3V3 rayında 3.3 V (±0.15 V), yük altında 3.2 V üstü
[ ] GND barı ile 3V3 arası kısa devre yok
[ ] KTS102 OFF → sistem kapalı
[ ] ESP32 seri port 115200 açılıyor
[ ] OLED "KART OKUT" gösteriyor (I2C 0x3C)
[ ] RC522 dumpVersion() → 0x91/0x92
[ ] Her buton tek olay veriyor (4/4)
[ ] Butonlar ters çalışmıyor (basınca LOW)
[ ] Tüm konnektörlerde silikon/makaron var
[ ] Kablo bağları kesilmiş, keskin uç yok
[ ] Servis payı 5 cm, gergin kablo yok
[ ] Anten (varsa) güç kablosundan uzak
[ ] Etiketler okunuyor
```

---

## 9. Plaket "13×23" delik ise

Delik sayısı ise (33 × 58 mm), ESP32 tek başına 26 × 52 mm — **hiçbir şey sığmaz**.
O durumda: sadece ESP32 soketi plakete, tüm konnektörler **kablo üzerinde** (tunik dişi doğrudan
ESP32 pinine). Plaket sadece taşıyıcı olur. Yeni plaket gerekir.
