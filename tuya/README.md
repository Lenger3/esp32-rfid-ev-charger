# Tuya erişim araçları (CLI)

İki katman var:

- `tuya_cloud.py` — **bulut** (saf stdlib, bağımlılık yok). Sadece `local_key`'i almak, DP haritasını öğrenmek ve cihaza uzaktan komut vermek için.
- `tuya_local.py` — **yerel LAN** (`tinytuya` gerekir). ESP32'nin yapacağı işin aynısı; internet gerekmez, tek doğrulama `local_key` ile AES.

Cihaz: `0123456789abcdef0123` · bölge: `ue` (openapi.tuyaeu.com)

## Kurulum

```bash
cd ~/Documents/WorkSpace/Tuya/tuya
pip3 install tinytuya          # yerel katman için (bulut katmanı stdlib)

cp config.example.json config.json   # zaten mevcut, sadece client_secret gir
```

`config.json` içine Tuya panelinden **Client Secret**'ı yaz (`Cloud → Project → Overview`). `client_id` ve `device_id` hazır.

## 1) local_key'i al

```bash
python3 tuya_cloud.py device        # local_key (maskeli), ip, timezone
python3 tuya_cloud.py save-local    # config.json'a local_key + ip + protokol sürümü yazar
```

`save-local` cihazın IP'sini bulur ve `tinytuya` kuruluysa 3.3 / 3.5 / 3.4 / 3.1 sırayla deneyip çalışan sürümü kaydeder.

> `local_key` cihaz Smart Life'tan silinip yeniden eklenirse değişir — o zaman `save-local` tekrar çalıştır.

## 2) DP numaralarını öğren (yerel kontrol için şart)

Bulut `code` verir (`switch`, `work_mode`...), yerel protokol **sayı** ister (`"1"`, `"2"`...).
**Bu projede bulut uçları `dp_id` döndürmüyor** — o yüzden numaralar cihazın kendisinden okunur:

```bash
python3 tuya_local.py raw        # ham DP listesi (sayısal anahtarlar)
python3 tuya_local.py identify   # bulut değerleriyle eşleştir, config.json'a yaz
```

`identify` hiçbir şey yazmaz; bulutun bildiği değerleri (14224, 1668, 10, 24...) yerel DP
değerleriyle karşılaştırıp **tek aday** olanları otomatik eşler. Belirsiz kalanları elle doğrula.

Örnek `config.json → dp` bloğu:

```json
"dp": {
  "switch": 1,
  "work_mode": 3,
  "charge_cur_set": 2,
  "work_state": 101,
  "forward_energy_total": 1,
  "charge_energy_once": 25,
  "power_total": 105,
  "temp_current": 110,
  "charge_time": 113
}
```

Doğrulama: `status` komutu eşleşmeyenleri `eslesmeyen_dp` altında gösterir.

## 3) Yerel kontrol

```bash
python3 tuya_local.py probe          # hangi protokol sürümü çalışıyor
python3 tuya_local.py status         # çözülmüş durum
python3 tuya_local.py switch off     # şarjı durdur
python3 tuya_local.py switch on
python3 tuya_local.py mode charge_schedule
python3 tuya_local.py current 16     # akım sınırı (A)
python3 tuya_local.py watch 5        # 5 sn'de bir canlı takip
python3 tuya_local.py session start  # sayaç baz değeri
python3 tuya_local.py session stop   # kWh farkı + süre
```

## 4) Bulut kontrolü (uzaktan, internet ile)

```bash
python3 tuya_cloud.py status
python3 tuya_cloud.py switch off
python3 tuya_cloud.py mode charge_now
python3 tuya_cloud.py timers         # cihazın bulut zamanlayıcıları
```

## Bilinenler / kısıtlar

- Yazılabilir DP'ler: **sadece** `switch`, `work_mode`, `charge_cur_set`. Cihazda RFID/`require_authorization` DP'si **yok**.
- Zamanlama saati DP'si de yok; `charge_schedule` modu bulut/timer tarafında anlam kazanıyor. Yani "internet yokken planlı şarj" bu cihazda garanti değil — test edilecek.
- `work_state` 8 değer alır: `charger_free`, `charger_insert`, `charger_free_fault`, `charger_wait`, `charger_charging`, `charger_pause`, `charger_end`, `charger_fault`.
- Ölçekler: `forward_energy_total` ve `charge_energy_once` /100 (kWh), `power_total` /1000 (kW), `charge_time` /10 (dk).
- **Asla** `reset-factory` endpoint'ini çağırma: `local_key` değişir, cihaz Smart Life'tan düşer.

## Güvenlik

- `config.json`, `.token_cache.json`, `.session.json` git'e girmiyor (`.gitignore`).
- `client_secret` hiçbir zaman ağa çıkmaz, sadece HMAC imzasında kullanılır.
- `access_token` 2 saat geçerli, `.token_cache.json` içinde tutulur (chmod 600).

## Donanım

Pin haritası ve doğrulanmış bağlantılar: **`docs/PIN-HARITASI.md`**
Lehim şablonu (1:1): `hardware/LEHIM-SABLONU.pdf`

| Modül | Pinler |
|---|---|
| OLED M242 (I2C) | SCL=G22 · SDA=G21 · RES=3V3 · A0=GND · CS=GND |
| RC522 (SPI) | CS=G5 · SCK=G18 · MOSI=G23 · MISO=G19 · RST=G27 · IRQ=boş |
| Butonlar | UP=G32 · DOWN=G33 · BACK=G26 · OK=G25 |
