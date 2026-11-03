# Kartlı Şarj Sistemi — Firmware

ESP32-WROOM-32U · M242-12864 OLED (I2C) · RC522 · 4 buton

## Ne yapıyor

| Ekran | Ne gösterir | Nasıl girilir |
|---|---|---|
| **Karşılama** | saat, tarih, `KART OKUTUN` | açılış / çıkış |
| **Kullanıcı** | bu ay, bugün, kalan kota | kullanıcı kartı |
| **Cihaz** | durum, akım, güç, sıcaklık, süre, oturum kWh | kullanıcı ekranında OK |
| **Şarj** | büyük kWh, süre, akım/güç | cihaz ekranında OK |
| **Yönetici menüsü** | kullanıcılar, kayıtlar, ayarlar, zamanlama, cihaz | admin kartı veya karşılamada OK uzun bas |
| **PIN** | 4 haneli yönetici şifresi | karşılamada OK uzun bas |

Karşılama ekranında **kart okutmak dışında hiçbir işlem yapılamaz** (kısa basışlar yok sayılır).

## Dosya yapısı

```
src/
  board.h     buton/modül pinleri — DEĞİŞTİRİLECEK TEK YER
  pins.h      board.h'yi okur + çakışma denetimi
  btn.h/cpp   buton durum makinesi (debounce, uzun bas)
  store.h/cpp NVS: kartlar, ayarlar, loglar
  hw.h/cpp    buton debounce, RC522, NTP saat
  ui.h/cpp    tüm çizim — donanımdan bağımsız
  charger.h/cpp  şarj cihazı katmanı — Tuya LAN v3.5 sürücüsü
  main.cpp    durum makinesi
sim/          Mac'te ekran görüntüsü üreten simülatör
```

## Derleme / yükleme

```bash
cd firmware/kart_sistem
pio run -t upload && pio device monitor
```

## Testler (donanım gerekmez)

```bash
cd firmware/kart_sistem/sim
./test.sh          # buton makinesi (21) + depolama (26)
```

## Ekranları Mac'te görmek

```bash
cd firmware/kart_sistem/sim
./build.sh          # out/*.png + out/ekranlar.png
```

`build.sh` **gerçek `src/ui.cpp`** dosyasını derler; yani PNG'ler cihazdaki
piksel çıktısının aynısıdır (aynı font, aynı koordinatlar).

## Ağ mimarisi — ESP32 AP + NAT

```
İnternet
   │
 Router  ◄──STA──  ESP32  ──AP──►  Şarj cihazı (192.168.4.x)
                      │
                 NAPT (NAT)
       cihazın bulut trafiği ESP32 üzerinden geçer
```

Cihaz bizim AP'mize bağlanır ama **internete ESP32 üzerinden** çıkar (NAT).
Böylece cihazın hangi ağda olduğunu biz belirleriz.

### Zorunlu: platform 8.0.0

`WiFi.AP.enableNAPT()` sadece arduino-esp32 **3.3.12+** ile var.
Platform 6.13.0 (arduino 2.0.17) `CONFIG_LWIP_IPV4_NAPT` içermez — derleme hata verir.

### Ayarlar

`src/config.h` (git'e girmez):

```c
#define WIFI_STA_SSID   "EV_SSID"          // router
#define WIFI_STA_PASS   "EV_SIFRE"
#define WIFI_AP_SSID    "EVCHARGE-SETUP"   // cihazın bağlanacağı AP
#define WIFI_AP_PASS    "change-me-8chars"
```

### Dikkat — sıralama önemli

1. ESP32'yi router'a bağla, `[net] NAPT -> AKTIF` yazdığını doğrula
2. **Sonra** şarj cihazını Smart Life'ta `EVCHARGE-SETUP` ağına taşı
   (Cihaz → Ayarlar → Wi-Fi değiştir)
3. Yeniden eşleştirme **`local_key`'i değiştirir** — yeni anahtarı tekrar okumak gerekir

Cihaz önce ESP32 AP'sine bağlanırsa ve STA o an ayakta değilse **eşleştirme
tamamlanmaz** (Tuya bulut kaydı internet ister).

### Sınırlar

- ESP32 NAT ~ birkaç Mbps — şarj için sorun değil, bulut trafiği için yeter
- softAP maksimum 4 istemci (kodda 4)
- ESP32 çökerse cihaz buluta çıkamaz **ama şarj fiziksel olarak devam eder**
- Flash kullanımı %81 → OTA için daha büyük partition şeması gerekecek

## AP + internet (NAPT) — sıra kritik

```
1. STA bağlanır (router'dan IP alır)
2. AP açılır
3. NAPT açılır            <- STA IP'si olmadan çalışmaz
4. DHCP DNS upstream'e ayarlanır  <- yoksa "bağlı ama internet yok" olur
```

**4. adım en sık atlanan:** AP'nin DHCP'si istemcilere `192.168.4.1`'i DNS
olarak verir, ama orada DNS sunucusu yoktur. NAPT yalnızca paket yönlendirir,
DNS'i cevaplamaz. Bu yüzden DHCP DNS'i doğrudan upstream'e (STA'nın öğrendiği
DNS, yoksa 1.1.1.1 / 1.0.0.1 / 8.8.8.8) çeviriyoruz.

Seri çıktıda beklenen:
```
[net] STA ip=192.168.x.x  rssi=-55
[net] AP EVCHARGE-SETUP -> ACIK  ip=192.168.4.1
[net] NAPT -> AKTIF
[net] DHCP DNS -> ayarlandi (upstream)
```

## Durum ikonları

Üst barda üç sembol — telefonda alıştığın gibi:

| İkon | Dolu | Yanıp sönen | Çarpı |
|---|---|---|---|
| WiFi | router + internet tamam | bağlı, internet yok | bağlı değil |
| Fiş | şarj cihazı erişilebilir | — | ulaşılamıyor |
| Bulut | panel bağlı | henüz denenmedi | istek hatası |

Ayrıntı için: **Yönetici → Cihaz Bilgisi** (`AG DURUMU`): IP, RSSI, NAPT,
bağlı istemci, internet, cihaz IP, panel rev.

## 7/24 dayanıklılık (bozulma senaryoları)

Cihaz sürekli açık kalacağı için her kopma türü ayrı ayrı ele alındı.

### 1. Ekran uykusu
Son kullanıcı etkileşiminden **60 sn** sonra OLED paneli kapanır
(`u8g2 setPowerSave`). Arka plan durmaz: şarj, kart okuma, panel senkronu,
saat hepsi çalışmaya devam eder. Herhangi bir buton ya da kart okutma
ekranı anında geri açar. **Şarj sürerken ekran kapanmaz** (kW/kWh izlenir).

### 2. Wi-Fi koptu
STA bağlantısı düşerse üstel geri çekilmeyle yeniden denenir:

```
deneme 1-2 : WiFi.reconnect()            5 sn, 10 sn
deneme 3+  : disconnect(true)+begin()    20, 30, 60 sn
deneme 5/10/15... : radyo modu sıfırlanır (AP yeniden açılır)
```

Bağlantı geri gelince **NAPT ve DHCP DNS otomatik tazelenir** (yoksa
"bağlı ama internet yok" durumu kalırdı).

**10 dakika** boyunca hiç bağlanamazsa cihaz kendini yeniden başlatır.

### 3. Şarj cihazı (Tuya) cevap vermiyor
- Her başarısız yoklamada bekleme artar: 5 → 10 → 20 → 40 → 60 sn.
  Bu olmadan her turda 4 sn timeout yiyip butonlar donuyordu.
- 3 başarısız yoklama + 5 dakika veri yoksa oturum **kapatılır**
  (sebep: "şarj cihazı cevap vermiyor"), kWh son bilinen değerle yazılır.
- IP kaybolursa AP ağında UDP ile yeniden aranır.

### 4. İnternet koptu ama şarj yapıldı
Oturum kayıtları **NVS kuyruğuna** yazılır (`outbox`, 32 kayıt).
- İnternet gelince 20 sn'de bir sırayla gönderilir.
- Her kaydın tekil `client_id`'si var; panel aynı kaydı iki kez yazmaz
  (`{"dup":true}` döner). Böylece "tekrar dene" güvenli.
- Kuyruk dolarsa en eski kayıt atılır (yenisi daha değerli).

### 5. Yazılım kilitlenmesi
`health.cpp` ayrı bir FreeRTOS görevi çalıştırır (loop'un içinde değil,
çünkü kilitlenen loop kendi kendini denetleyemez):

| Koşul | Eylem |
|---|---|
| 90 sn boyunca loop ilerlemedi | `esp_restart()` |
| 10 dk Wi-Fi yok | `esp_restart()` |
| boş heap < 15 KB | `esp_restart()` |

Yeniden başlatma sayısı ve nedeni NVS'de tutulur, **Yönetici → Sistem**
ekranında görünür.

### 6. Saat
NTP bir kez ayarlanır; saat geçersizse panel senkronu durur.
Açılışta saat yoksa tekrar denenir (TLS için saat şart).

### 7. Sistem ekranı (Yönetici → Sistem)
```
Acik  3d 07h  heap 120k
WiFi  OK  kopma 2
Panel OK  hata 0
Kuyruk 0   Cihaz OK
Restart 1  wifi yok
```

## Şu an eksik / sıradaki iş

1. **Kart imzası** — şu an yalnızca UID okunuyor, klonlanabilir. Kritik kurulumda
   kartın bir sektörüne (UID + sayaç) üzerinden HMAC imzası yazılmalı; bu da
   kuruluma özel anahtar yönetimi gerektiriyor.
2. **OTA** — partition şeması `huge_app.csv`; imzalı A/B OTA için şema değişmeli.
   Bugün firmware güncellemesi USB kabloyla yapılıyor.
3. **Wi-Fi ve yönetici PIN'i** — derleme zamanında sabit
   (`config.h` ve `-D ADMIN_PIN=...`). Ayarlar ekranından / panelden
   değiştirilebilir olmalı.
4. **Tuya bulut bağımlılığı** — `local_key` bir kez buluttan alınıyor; cihaz
   yeniden eşleştirilirse anahtar değişir ve panelden güncellenmesi gerekir.
5. **Mifare sektör okuma** — kart kaydetme akışı UID ile çalışıyor; imza
   eklendiğinde kayıt akışının da sektörü yazması gerekecek.

## Butonlar

| Buton | GPIO | Kısa bas | Uzun bas |
|---|---|---|---|
| UP | 32 | yukarı / + | tekrar |
| DOWN | 33 | aşağı / − | tekrar |
| BACK | 26 | geri / çık | — |
| OK | 25 | seç / onayla | yönetici girişi, silme, yeni kart |

## Log kaydı

Her şarj oturumu NVS'ye yazılır: zaman, kart UID, kWh, sebep
(0 normal, 4 iptal, 5 tanınmayan kart). 40 kayıt döner tampon.
