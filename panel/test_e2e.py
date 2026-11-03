#!/usr/bin/env python3
"""Panel uctan uca test: giris -> kart ekle -> cihaz API -> panelde dogrula."""
import http.cookiejar
import json
import sqlite3
import sys
import urllib.error
import urllib.parse
import urllib.request

BASE = "http://127.0.0.1:8090"
DB = "panel.db"

ok = 0
fail = 0


def check(cond, msg):
    global ok, fail
    if cond:
        ok += 1
        print(f"  [OK  ] {msg}")
    else:
        fail += 1
        print(f"  [FAIL] {msg}")


jar = http.cookiejar.CookieJar()
opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar))


def get(path, headers=None):
    req = urllib.request.Request(BASE + path, headers=headers or {})
    try:
        r = opener.open(req)
        return r.status, r.read().decode()
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode()


def post(path, data, headers=None):
    body = urllib.parse.urlencode(data, doseq=True).encode()
    req = urllib.request.Request(BASE + path, data=body, headers=headers or {})
    try:
        r = opener.open(req)
        return r.status, r.read().decode()
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode()


def get_json(path, token):
    req = urllib.request.Request(BASE + path,
                                 headers={"Authorization": "Bearer " + token})
    try:
        r = opener.open(req)
        return r.status, json.loads(r.read().decode())
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode()


def post_json(path, payload, token):
    body = json.dumps(payload).encode()
    req = urllib.request.Request(
        BASE + path, data=body,
        headers={"Content-Type": "application/json",
                 "Authorization": "Bearer " + token})
    try:
        r = opener.open(req)
        return r.status, json.loads(r.read().decode())
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode()


print("\n--- 1. kimlik dogrulama ---")
st, _ = get("/")
check(st == 200 and "/login" in _.lower() or "Şifre" in _, "giris yapmadan panele erisilemiyor")

st, _ = post("/login", {"sifre": "yanlis"})
check(st == 401, "yanlis sifre reddedildi (401)")

st, _ = post("/login", {"sifre": "dev-only-pass"})
check(st == 200, "dogru sifre kabul edildi")

st, html = get("/")
check("Kullanıcılar" in html and "Bu ay" in html, "panel acildi")

print("\n--- 2. kart ekleme ---")
st, _ = post("/users/add", {"uid": "D3:BF:4E:07", "name": "AYSE",
                            "quota_kwh": 100, "is_admin": "true"})
check(st == 200, "kart eklendi (iki nokta temizlendi)")

st, html = get("/users")
check("AYSE" in html and "D3BF4E07" in html, "kart listede, UID normalize edildi")

st, _ = post("/users/add", {"uid": "D3BF4E07", "name": "DUP", "quota_kwh": 0})
st, html = get("/users")
check(html.count('small">D3BF4E07</td>') == 1 and "DUP" not in html, "ayni UID ikinci kez eklenmiyor")

st, _ = post("/users/add", {"uid": "ZZZ", "name": "BOZUK", "quota_kwh": 0})
st, html = get("/users")
check("BOZUK" not in html, "gecersiz UID reddedildi")

print("\n--- 3. cihaz API ---")
con = sqlite3.connect(DB)
token = con.execute("select api_token from devices limit 1").fetchone()[0]
con.close()
check(len(token) > 20, f"cihaz token uretildi ({len(token)} karakter)")

st, r = post_json("/api/v1/heartbeat",
                  {"firmware": "0.1.0", "ip": "192.168.4.2",
                   "state": "charger_free"}, "yanlis-token")
check(st == 401, "gecersiz token reddedildi")

st, r = post_json("/api/v1/heartbeat",
                  {"firmware": "0.1.0", "ip": "192.168.4.2",
                   "state": "charger_free"}, token)
check(st == 200 and r.get("ok"), "heartbeat kabul edildi")

st, r = get_json("/api/v1/config", token)
check(st == 200, "config alindi")
check(r["settings"]["max_a"] == 16, "ayarlar geldi")
check(len(r["cards"]) == 1 and r["cards"][0]["name"] == "AYSE", "kart listesi geldi")
check(r["cards"][0]["admin"] is True, "admin bayragi dogru")

st, r = post_json("/api/v1/session",
                  {"uid": "D3BF4E07", "kwh": 12.4, "reason": 0}, token)
check(st == 200 and r.get("ok"), "oturum kaydedildi")

st, r = post_json("/api/v1/session",
                  {"uid": "A7512B64", "kwh": 3.1, "reason": 6}, token)
check(st == 200, "taninmayan kart oturumu kaydedildi")

st, r = post_json("/api/v1/unknown", {"uid": "A7512B64"}, token)
check(st == 200, "taninmayan kart olayi kaydedildi")

print("\n--- 4. panelde dogrulama ---")
st, html = get("/")
check("Bağlı" in html and "Çevrimdışı" not in html, "cihaz cevrimici gorunuyor")
check("12.4" in html, "oturum kWh panelde")

st, html = get("/sessions")
check("AYSE" in html and "elektrik kesintisi" in html, "kayitlar + sebep etiketi")

st, html = get("/users")
check("12.4" in html, "kullanim karta islendi")

print("\n--- 5. ayar degisikligi ---")
_, r0 = get_json("/api/v1/config", token)
rev0 = r0["rev"]
st, _ = post("/settings", {"max_a": "20", "def_a": "14",
                           "require_card": "true", "schedule_en": "true",
                           "schedule_start": "22:30", "schedule_end": "06:15",
                           "max_session_min": "240",
                           "days": ["0", "1", "2", "3", "4"]})
check(st == 200, f"ayarlar kaydedildi (http {st})")

st, r = get_json("/api/v1/config", token)
check(r["settings"]["max_a"] == 20, "yeni max_a cihaza yansidi")
check(r["settings"]["schedule_start"] == "22:30", "zamanlama baslangici")
check(r["settings"]["schedule_days"] == 31, "gun maskesi (Pzt-Cum = 31)")
check(r["rev"] == rev0 + 1, "rev artti: %d -> %d" % (rev0, r["rev"]))

print("\n--- 6. kart silme ---")
con = sqlite3.connect(DB)
cid = con.execute("select id from cards where uid='D3BF4E07'").fetchone()[0]
con.close()
st, _ = post(f"/users/{cid}/delete", {})
st, html = get("/users")
check("AYSE" not in html, "kart silindi")

st, r = get_json("/api/v1/config", token)
check(len(r["cards"]) == 0, "silme cihaz config'ine yansidi")
check(r["rev"] == rev0 + 2, "rev silmede de artti: %d" % r["rev"])

print(f"\n=== SONUC: {ok} gecti, {fail} kaldi ===")
sys.exit(0 if fail == 0 else 1)
