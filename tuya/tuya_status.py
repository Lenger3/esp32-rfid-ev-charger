#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Tuya bulut API — tek dosyalik durum sorgusu (saf stdlib, bagimlilik yok).

Kimlik bilgileri KODA GOMULMEZ; `tuya/config.json` dosyasindan okunur:

    {"client_id": "...", "client_secret": "...", "region": "ue",
     "device_id": "...", "local_key": "", "ip": "", "local_version": 3.3}

Kullanim:
    python3 tuya_status.py        # config.json'daki cihazin durumu + DP'leri

access_token her cagrida yeniden imzalanip alinir; repoda ya da diskte
kalici bir token tutulmaz.
"""
import hashlib
import hmac
import json
import os
import sys
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
CONFIG_PATH = os.path.join(HERE, "config.json")

REGIONS = {
    "eu": "https://openapi.tuyaeu.com",
    "ue": "https://openapi.tuyaeu.com",
    "we": "https://openapi-weaz.tuyaeu.com",
    "us": "https://openapi.tuyaus.com",
    "cn": "https://openapi.tuyacn.com",
    "in": "https://openapi.tuyain.com",
}


def load_config():
    if not os.path.exists(CONFIG_PATH):
        sys.exit("config.json bulunamadi. config.example.json'u kopyalayip doldur.")
    with open(CONFIG_PATH, "r", encoding="utf-8") as f:
        return json.load(f)


def sign(client_id, client_secret, token, t, method, path):
    """Tuya imza algoritmasi: HMAC-SHA256, buyuk harf hex."""
    content_hash = hashlib.sha256(b"").hexdigest()
    string_to_sign = "%s\n%s\n\n%s" % (method, content_hash, path)
    msg = "%s%s%s%s" % (client_id, token or "", t, string_to_sign)
    return hmac.new(
        client_secret.encode("utf-8"), msg.encode("utf-8"), hashlib.sha256
    ).hexdigest().upper()


def _call(base, client_id, client_secret, token, path):
    t = str(int(time.time() * 1000))
    headers = {
        "client_id": client_id,
        "sign": sign(client_id, client_secret, token, t, "GET", path),
        "t": t,
        "sign_method": "HMAC-SHA256",
        "Content-Type": "application/json",
    }
    if token:
        headers["access_token"] = token
    req = urllib.request.Request(base + path, headers=headers, method="GET")
    with urllib.request.urlopen(req, timeout=15) as r:
        return json.loads(r.read().decode())


def access_token(base, client_id, client_secret):
    r = _call(base, client_id, client_secret, "", "/v1.0/token?grant_type=1")
    if not r.get("success"):
        sys.exit("token alinamadi: %s" % r.get("msg"))
    return r["result"]["access_token"]


def get_tuya_device_status(client_id, client_secret, token, device_id, base):
    return _call(base, client_id, client_secret, token,
                 "/v1.0/devices/%s" % device_id)


if __name__ == "__main__":
    cfg = load_config()
    base = REGIONS.get(str(cfg.get("region", "ue")).lower())
    if not base:
        sys.exit("Bilinmeyen region: %s" % cfg.get("region"))
    if not cfg.get("device_id"):
        sys.exit("config.json icinde device_id bos")

    tok = access_token(base, cfg["client_id"], cfg["client_secret"])
    result = get_tuya_device_status(
        cfg["client_id"], cfg["client_secret"], tok, cfg["device_id"], base
    )
    print(json.dumps(result, indent=2, ensure_ascii=False))
