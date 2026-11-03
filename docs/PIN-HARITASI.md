# Donanım Pin Haritası — DOĞRULANMIŞ

> **Bu dosya tek doğruluk kaynağıdır.** Kart üzerinde test edilip onaylanmıştır.
> Tarih: 2026-10-28 · ESP32-WROOM-32U, kart 0123456789abcdef0123
>
> Değiştiren herkes: aynı anda `docs/WIRING.md`, `docs/PLAKET-100x100.md`,
> `tools/draw_pcb.py` ve `hardware/lehim-koordinat.md` dosyalarını da güncelle.

## Özet tablo

| Modül | Sinyal | ESP32 | Durum |
|---|---|---|---|
| **OLED M242-12864** (I2C) | SCL | **GPIO 22** | ✅ test edildi |
| | SDA | **GPIO 21** | ✅ |
| | RES | **3V3** (modül tarafında) | plakete gitmez |
| | A0 (DC) | **GND** (adres 0x3C) | modül tarafında |
| | CS | **GND** | modül tarafında |
| | VCC | 3V3 | |
| **RC522** (SPI) | SDA/CS | **GPIO 5** | ✅ 0x82 klon |
| | SCK | **GPIO 18** | ✅ |
| | MOSI | **GPIO 23** | ✅ |
| | MISO | **GPIO 19** | ✅ |
| | RST | **GPIO 27** | ✅ |
| | IRQ | **bağlanmıyor** | polling kullanılıyor |
| | 3.3V | 3V3 | **5 V YASAK** |
| **Butonlar** | EVET (yeşil) | **GPIO 26** | ✅ **`board.h`** |
| | HAYIR (siyah) | **GPIO 33** | ✅ **`board.h`** |
| | GERİ (siyah) | **GPIO 32** | ✅ **`board.h`** |
| | OK (kırmızı) | **GPIO 25** | ✅ **`board.h`** |
| | ortak | GND | INPUT_PULLUP, harici direnç YOK |
| **Güç** | 12 V giriş | J1/J2 klemens | |
| | Buck çıkışı | 3V3 rayı | ≥1.5 A |
| **Boş pinler** | | 4, 13, 14, 16 | fan/buzzer/yedek |

## Bağlantı test sonuçları (gerçek çıktı)

```
OLED  : I2C tarama -> 0x3C, 5 sayfa çizildi          ✅
RC522 : VersionRegister = 0x82 (klon çip), anten AÇIK ✅
Buton : 4/4 basıldı, hepsi boşta HIGH                 ✅
Kart  : UID okundu (D3:BF:4E:07, MIFARE 1KB)          ✅
Soak  : 60 sn, heap 346752 sabit (sızıntı yok)        ✅
```

## Buton yerleşimi değişebilir

Buton pinleri artık **`firmware/kart_sistem/src/board.h`** içinde:

```c
#define BTN_UP_PIN     26      // ekranda EVET
#define BTN_DOWN_PIN   33      // ekranda HAYIR
#define BTN_BACK_PIN   32      // ekranda GERI
#define BTN_OK_PIN     25      // ekranda OK
#define BTN_ACTIVE_LOW 1       // 0 = buton 3V3'e çekiyor (INPUT_PULLDOWN)
```

Ekranda GPIO değil **işlev** yazılır: `EVET`, `HAYIR`, `GERI`, `OK`.
Kullanıcı "hangi pin" değil "hangi tuş" görür.

Fiziksel butonlar karışık bağlandıysa **sadece bu dosya** değişir; başka yerde
pin numarası yok.

**Doğrulanmış harita (cihazda test edildi):**

| Buton | GPIO | Kod adı |
|---|---|---|
| EVET (yeşil) | 26 | `BTN_UP` |
| HAYIR (siyah) | 33 | `BTN_DOWN` |
| GERİ (siyah) | 32 | `BTN_BACK` |
| OK (kırmızı) | 25 | `BTN_OK` |

**Hangi butonun hangi GPIO'da olduğunu bulmak:**
Cihazda `Yönetici → Buton Testi`. Ekranda dört satır listelenir, bastığın
butonun GPIO'su vurgulanır ve altta `okunan: GPIO xx` yazar. Çıkmak için
**uzun bas**. Test sırasında kısa basışlar menüyü gezmez.

Aynı GPIO'ya iki buton atanırsa (veya buton OLED/RC522 hattına denk gelirse)
derleme `#error` ile durur — yanlış haritayla firmware yüklenmez.

## Buton kodunda dikkat — enum kayması

`Btn` enum'u `BTN_NONE = 0`'dan başlar (`BTN_UP=1 … BTN_OK=4`), ama `btns[]`
dizisi 0..3 indekslidir. `(Btn)i` dönüşümü **yanlıştır** — her butonu bir
ötekine kaydırır ve `BTN_OK` hiç üretilmez:

| Basılan | GPIO | `(Btn)i` sonucu | Doğru |
|---|---|---|---|
| EVET | 26 | `BTN_NONE` (ölü) | `BTN_UP` |
| HAYIR | 33 | `BTN_UP` | `BTN_DOWN` |
| GERİ | 32 | `BTN_DOWN` | `BTN_BACK` |
| OK | 25 | `BTN_BACK` | `BTN_OK` |

Çözüm: açık eşleme tablosu `kBtnOf[]` (`src/btn.cpp`). Birim testi
`sim/test_buttons.cpp` bu hatayı yakalar.

## Kritik notlar

1. **OLED ilk taramada bulunmayabilir.** SSD1309 ilk `Wire.begin()` sonrası stuck-bus
   davranışı gösteriyor: ilk tarama (126 adres) boş döner, ikinci tarama bulur.
   Her başarısız I2C işlemi hat üzerinde clock darbesi üretir ve hattı açar.
   **Çözüm:** en az iki kez tam tarama yap; tek denemeyle bırakma.
   Doğrulanmış kod: `firmware/test/oled_i2c_test/oled_i2c_test.ino` (scanBothOrders)

2. **"Takas" sonucu yanıltıcı.** İkinci taramada bulunması kabloların ters olduğu
   anlamına GELMEZ — sadece daha fazla darbeye ihtiyaç olduğunu gösterir.
   Begin sonrası `Wire.endTransmission()` ile ACK kontrolü yap, ona göre karar ver.

3. **RC522 klon çip.** VersionRegister `0x82` döner (Fudan FM17522). UID okuma için
   sorun değil; kart üzerine yazma/Crypto1 gerektiren işlerde sorun çıkarabilir.

4. **400 kHz çalışıyor** ama 100 kHz daha güvenli. Uzun kablo/parazit varsa 100 kHz'e düş.

5. **IRQ bağlanmıyor.** RC522 polling ile okunuyor (`PICC_IsNewCardPresent`).
   Kesme kullanmak istersen GPIO 4 boş.

## WiFi (harici anten)

ESP32-WROOM-32U'da dahili anten **yok**. IPEX/U.FL 2.4 GHz anten takılmadan
Wi-Fi çekmez. Anten kablosu güç kablolarından uzak, min bükülme yarıçapı 5 mm.
