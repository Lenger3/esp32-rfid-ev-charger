#!/usr/bin/env bash
# VPS kurulum — panel.example.com
#   sudo bash deploy/install.sh
#
# Yaptigi: .env uretir (sifreler rastgele), imaji derler, servisi kaldirir,
#          saglik kontrolu yapar, cihaz tokenini gosterir.
# YAPMADIGI: Caddyfile'a dokunmaz (mevcut siteleri bozmamak icin) — blogu basilir.
set -euo pipefail

APP_DIR="${APP_DIR:-/opt/sarj-panel}"
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$(cd "$HERE/.." && pwd)"

say() { printf '\n\033[1;36m== %s\033[0m\n' "$*"; }
die() { printf '\n\033[1;31mHATA: %s\033[0m\n' "$*" >&2; exit 1; }

[ "$(id -u)" -eq 0 ] || die "root olarak calistir (sudo)"

say "1. On kosullar"
command -v docker >/dev/null || die "docker yok"
docker compose version >/dev/null || die "docker compose v2 yok"
command -v rsync >/dev/null || die "rsync yok (apt install rsync)"

say "2. Dosyalar -> $APP_DIR"
mkdir -p "$APP_DIR"
rsync -a --delete \
  --exclude '.venv' --exclude 'panel.db' --exclude 'data' --exclude '.env' \
  --exclude '__pycache__' --exclude 'screens' --exclude 'test_e2e.py' \
  "$SRC"/ "$APP_DIR"/
chmod -R go-w "$APP_DIR"
cd "$APP_DIR"

say "3. .env"
if [ -f .env ]; then
  echo "   zaten var, DOKUNULMADI (degistirmek icin once yedekle)"
else
  ADMIN_PW=$(openssl rand -base64 24 | tr -dc 'A-Za-z0-9' | cut -c1-20)
  SESS=$(openssl rand -hex 32)
  DBP=$(openssl rand -hex 24)
  TOK=$(openssl rand -hex 24)
  cat > .env <<EOF
ADMIN_PASSWORD=$ADMIN_PW
SESSION_SECRET=$SESS
DB_PASS=$DBP
TUYA_DEVICE_ID=0123456789abcdef0123
DEVICE_TOKEN=$TOK
COOKIE_SECURE=1
TZ_NAME=Europe/Istanbul
EOF
  chmod 600 .env
  chown root:root .env
  echo
  echo "   +--------------------------------------------------+"
  echo "   |  PANEL SIFRESI:  $ADMIN_PW"
  echo "   +--------------------------------------------------+"
  echo "   Bu sifre bir daha gosterilmeyecek. Simdi kaydet."
  echo
  echo "   Cihaz tokeni: $TOK"
fi

say "4. Imaj derleniyor"
docker compose build --quiet

say "5. Servisler"
docker compose up -d --remove-orphans

say "6. Saglik kontrolu"
ok=0
for _ in $(seq 1 45); do
  if curl -fsS http://127.0.0.1:8090/health >/dev/null 2>&1; then ok=1; break; fi
  sleep 2
done
if [ "$ok" = 1 ]; then echo "   panel yanit veriyor"; else
  docker compose logs --tail 40 panel
  die "panel acilmadi"
fi

say "7. Cihaz tokeni (veritabanindan)"
docker compose exec -T db psql -U sarj -d sarj -tAc \
  "select api_token from devices limit 1" 2>/dev/null | sed 's/^/   /' || true

say "8. Portlar"
echo "   dinlenen 8090:"
ss -ltnp 2>/dev/null | grep ':8090' | sed 's/^/     /' || echo "     (yok)"
echo "   beklenen: SADECE 127.0.0.1:8090 — 0.0.0.0 gorursen dur."

say "9. Caddy blogu"
D="panel.example.com"
if grep -q "$D" /etc/caddy/Caddyfile 2>/dev/null; then
  echo "   $D zaten Caddyfile'da — atlandi"
else
  echo "   /etc/caddy/Caddyfile dosyasinin SONUNA ekle:"
  echo "   ------------------------------------------------"
  sed 's/^/   /' "$HERE/Caddyfile.snippet"
  echo "   ------------------------------------------------"
  echo "   sonra:"
  echo "     sudo caddy validate --config /etc/caddy/Caddyfile"
  echo "     sudo systemctl reload caddy"
fi

say "Bitti"
echo "   https://$D"
