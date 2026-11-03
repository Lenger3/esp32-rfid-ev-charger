#!/usr/bin/env bash
# Postgres yedegi — gunde bir, 14 gun saklar.
#   /opt/sarj-panel/deploy/backup.sh
set -euo pipefail
cd /opt/sarj-panel
mkdir -p backups
STAMP=$(date +%Y%m%d-%H%M)
OUT="backups/sarj-$STAMP.sql.gz"

docker compose exec -T db pg_dump -U sarj -d sarj | gzip > "$OUT"
chmod 600 "$OUT"
chown root:root "$OUT"

# 14 gunden eski yedekleri sil
find backups -name 'sarj-*.sql.gz' -mtime +14 -delete

echo "$(date -Is) $OUT $(du -h "$OUT" | cut -f1)"
