#!/usr/bin/env python3
"""Idempotent sema gocu — create_all yeni kolon eklemez.

Kullanim (konteyner icinde):
    docker compose exec -T panel python -m app.migrate
"""
import os
import sys

from sqlalchemy import inspect, text

from .db import engine


# tablo -> [(kolon, SQL tipi, varsayilan)]
EXPECTED = {
    "devices": [
        ("local_key", "VARCHAR(32)", "''"),
        ("power_kw", "DOUBLE PRECISION", "0"),
        ("session_kwh", "DOUBLE PRECISION", "0"),
        ("current_a", "INTEGER", "0"),
        ("temp_c", "DOUBLE PRECISION", "0"),
        ("session_min", "INTEGER", "0"),
        ("cmd_ack", "INTEGER", "0"),
    ],
    "sessions": [
        ("client_id", "VARCHAR(48)", "''"),
    ],
    "settings": [
        ("target_kwh", "DOUBLE PRECISION", "0"),
        ("cmd", "VARCHAR(12)", "''"),
        ("cmd_id", "INTEGER", "0"),
    ],
}


def main() -> int:
    changed = 0
    with engine.begin() as conn:
        insp = inspect(conn)

        for table, cols in EXPECTED.items():
            if table not in insp.get_table_names():
                print(f"  {table}: tablo yok, atlandi")
                continue

            have = {c["name"] for c in insp.get_columns(table)}
            for name, sqltype, default in cols:
                if name in have:
                    continue
                stmt = (f'ALTER TABLE {table} ADD COLUMN {name} {sqltype} '
                        f'NOT NULL DEFAULT {default}')
                conn.execute(text(stmt))
                print(f"  + {table}.{name}")
                changed += 1

    print(f"goc tamam ({changed} degisiklik)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
