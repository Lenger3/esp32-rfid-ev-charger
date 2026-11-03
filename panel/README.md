
## Panel (VPS) — panel.example.com

Kart/kota/log yönetimi. ESP32 buraya **sadece outbound HTTPS** ile bağlanır;
VPS'te açılan port yok.

```
ESP32 ──HTTPS/Bearer──► Caddy ──► FastAPI ──► Postgres
   ▲                                          (127.0.0.1:8090)
   └── GET /api/v1/config  (kart listesi + ayarlar)
```

VPS düşerse ESP32 NVS'deki son listeyle **fail-closed** çalışmaya devam eder.

### Dosyalar

```
panel/
  app/
    main.py         panel sayfaları (Jinja2)
    device_api.py   ESP32 uçları
    db.py           SQLAlchemy modelleri
    security.py     oturum + Bearer
    templates/      HTML
    static/style.css
  deploy/
    install.sh          VPS kurulum
    enable-tls.sh       DNS hazir olunca Caddy + TLS
    Caddyfile.snippet
    backup.sh           gunluk pg_dump (cron)
  test_e2e.py       30 testlik uctan uca test
```

### Cihaz API

| Uç | Metot | Ne yapar |
|---|---|---|
| `/api/v1/heartbeat` | POST | canlılık + sürüm/IP |
| `/api/v1/config` | GET | ayarlar + kart listesi + `rev` |
| `/api/v1/session` | POST | biten oturum (kWh, sebep) |
| `/api/v1/unknown` | POST | tanınmayan kart olayı |

Kimlik: `Authorization: Bearer <api_token>` — panelden üretilir, firmware'de
`config.h → API_TOKEN`.

### Kurulum (VPS)

```bash
scp -r panel root@<VPS_IP>:/tmp/
ssh root@<VPS_IP>
sudo cp -r /tmp/panel/* /opt/sarj-panel/
sudo bash /opt/sarj-panel/deploy/install.sh
```

Betik `.env` üretir, şifreyi **bir kez** ekrana yazar, imajı derler, servisi
kaldırır ve cihaz tokenini gösterir.

TLS'i açmak için (DNS A kaydı hazır olduktan sonra):

```bash
ssh root@<VPS_IP> 'bash /opt/sarj-panel/deploy/enable-tls.sh'
```

### Güvenlik durumu

| Önlem | Nerede |
|---|---|
| Panel yalnızca `127.0.0.1:8090` | docker-compose port eşlemesi |
| Postgres dışarı kapalı (port yok) | docker-compose |
| Konteyner `panel` kullanıcısı, tüm cap'ler düşük | Dockerfile + compose |
| `.env` → `chmod 600`, `root:root` | install.sh |
| TLS + HSTS + CSP + X-Frame-Options | Caddyfile |
| Giriş: 5 hatalı deneme → 15 dk kilit (IP başına) | `app/ratelimit.py` |
| Çerez: HttpOnly + Secure + SameSite=Lax, 12 saat | `app/security.py` |
| Günlük DB yedeği, 14 gün saklama | `deploy/backup.sh` + cron |

**Not:** Bu Caddy derlemesinde `rate_limit` modülü **yok**. Caddyfile'a
`rate_limit` yazarsan `caddy validate` hata verir. Hız sınırı uygulama içinde.

**Kilitlendim:** sayaç bellekte; `docker compose restart panel` temizler.

**Caddy log izni:** `/var/log/caddy/sarj.log` `caddy:caddy` olmalı. Root
sahipliğinde reload `permission denied` ile patlar ve yeni config yüklenmez
(eski config çalışmaya devam eder). `enable-tls.sh` bunu kendisi düzeltir.

### Yerel geliştirme

**Python 3.12 veya 3.13 kullan.** 3.15'te SQLAlchemy 2.0.36 `Union[...]`
çözümlemesinde `TypeError: descriptor '__getitem__' requires a 'typing.Union'`
ile patlıyor — panel hiç açılmıyor.

```bash
cd panel
python3.13 -m venv .venv && .venv/bin/pip install -r requirements.txt
ADMIN_PASSWORD=dev-only-pass SESSION_SECRET=dev COOKIE_SECURE=0 \
  DATABASE_URL=sqlite:///./panel.db \
  .venv/bin/uvicorn app.main:app --port 8090
```

`COOKIE_SECURE=0` sadece yerel HTTP içindir. HTTPS arkasında `1` kalmalı,
yoksa tarayıcı çerezi kaydetmez ve giriş sonsuz döngüye girer.

Test: `python3.13 test_e2e.py` (30 kontrol)
