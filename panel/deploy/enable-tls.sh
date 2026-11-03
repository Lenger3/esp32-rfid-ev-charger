#!/usr/bin/env bash
# Caddy blogunu ekler ve TLS'i acar.
#   sudo bash deploy/enable-tls.sh
#
# DNS sart: panel.example.com -> bu sunucunun IP'si.
# DNS yoksa script HICBIR SEY yapmaz (Let's Encrypt kota yakmamak icin).
set -euo pipefail

DOMAIN=panel.example.com
CADDYFILE=/etc/caddy/Caddyfile
HERE="$(cd "$(dirname "$0")" && pwd)"
IP="$(curl -fsS https://api.ipify.org || curl -fsS ifconfig.me)"

say() { printf '\n\033[1;36m== %s\033[0m\n' "$*"; }
die() { printf '\n\033[1;31mHATA: %s\033[0m\n' "$*" >&2; exit 1; }

[ "$(id -u)" -eq 0 ] || die "root olarak calistir"

say "1. DNS kontrolu"
RESOLVED="$(dig +short "$DOMAIN" A | tail -1)"
echo "   $DOMAIN -> ${RESOLVED:-<yok>}"
echo "   bu sunucu -> $IP"
[ "$RESOLVED" = "$IP" ] || die "DNS henuz yok/farkli. Once A kaydini ekle, sonra tekrar calistir."

say "2. Zaten ekli mi"
if grep -q "$DOMAIN" "$CADDYFILE"; then
  echo "   evet, blok zaten var — atlandi"
else
  BACKUP="$CADDYFILE.bak.$(date +%s)"
  cp -a "$CADDYFILE" "$BACKUP"
  echo "   yedek: $BACKUP"
  printf '\n' >> "$CADDYFILE"
  cat "$HERE/Caddyfile.snippet" >> "$CADDYFILE"
  echo "   blok eklendi"
fi

# Caddy log dosyasini kendi kullanici adiyla acabilmeli; root sahipliginde
# reload "permission denied" ile patlar ve config yuklenmez.
mkdir -p /var/log/caddy
touch /var/log/caddy/sarj.log
chown caddy:caddy /var/log/caddy/sarj.log 2>/dev/null || true
chmod 600 /var/log/caddy/sarj.log

say "3. Dogrulama"
if ! caddy validate --config "$CADDYFILE" 2>&1 | tail -3; then
  echo "   gecersiz — geri aliniyor"
  cp -a "$BACKUP" "$CADDYFILE"
  die "Caddyfile gecersiz, eski hale donuldu"
fi

say "4. Yeniden yukle"
systemctl reload caddy
sleep 3

say "5. Sertifika bekleniyor"
for i in $(seq 1 30); do
  CODE=$(curl -s -o /dev/null -w '%{http_code}' "https://$DOMAIN/login" || echo 000)
  if [ "$CODE" = "200" ]; then
    echo "   https calisiyor (200)"
    break
  fi
  echo "   ... $CODE"
  sleep 4
done

say "6. Guvenlik basliklari"
curl -sI "https://$DOMAIN/login" | grep -iE 'strict-transport|x-frame|content-security|x-content-type|referrer|server:' | sed 's/^/   /'

say "7. HTTP -> HTTPS yonlendirme"
curl -s -o /dev/null -w '   http kodu: %{http_code} -> %{redirect_url}\n' "http://$DOMAIN/login"

say "8. Mevcut siteler bozulmadi mi"
for h in example.com example.com; do
  c=$(curl -s -o /dev/null -w '%{http_code}' "https://$h/")
  echo "   $h -> $c"
done

say "Bitti"
echo "   https://$DOMAIN"
