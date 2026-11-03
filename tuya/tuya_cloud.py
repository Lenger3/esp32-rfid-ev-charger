#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Tuya Bulut API istemcisi (saf stdlib - bagimlilik yok)

Kullanim:
    python3 tuya_cloud.py device      # cihaz bilgisi: local_key, IP, timezone
    python3 tuya_cloud.py spec        # instruction set (code bazli)
    python3 tuya_cloud.py dps         # code -> DP numarasi eslesmesi (yerel kontrol icin SART)
    python3 tuya_cloud.py status      # anlik DP degerleri
    python3 tuya_cloud.py switch on   # calistir
    python3 tuya_cloud.py switch off  # durdur
    python3 tuya_cloud.py mode charge_schedule
    python3 tuya_cloud.py current 16  # akim siniri (A)
    python3 tuya_cloud.py timers      # cihaz zamanlayicilari
    python3 tuya_cloud.py save-local  # local_key + IP + protokol surumunu config.json'a yaz
    python3 tuya_cloud.py raw /v1.0/iot-03/devices

Not: client_secret config.json icinde durur, koda gomulmez, repoya girmez.
"""
import argparse
import hashlib
import hmac
import json
import os
import sys
import time
import urllib.error
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
CONFIG_PATH = os.path.join(HERE, "config.json")
TOKEN_CACHE = os.path.join(HERE, ".token_cache.json")

REGIONS = {
    "eu": "https://openapi.tuyaeu.com",        # Central Europe (Frankfurt)
    "ue": "https://openapi.tuyaeu.com",        # ayni bolge, farkli isim
    "we": "https://openapi-weaz.tuyaeu.com",   # Western Europe
    "us": "https://openapi.tuyaus.com",
    "cn": "https://openapi.tuyacn.com",
    "in": "https://openapi.tuyain.com",
}


def load_config():
    if not os.path.exists(CONFIG_PATH):
        sys.exit("config.json bulunamadi. config.example.json dosyasini kopyalayip doldur.")
    with open(CONFIG_PATH, "r", encoding="utf-8") as f:
        return json.load(f)


class Cloud(object):
    def __init__(self, cfg):
        self.client_id = cfg["client_id"]
        self.client_secret = cfg["client_secret"]
        region = cfg.get("region", "ue").lower()
        if region not in REGIONS:
            sys.exit("Bilinmeyen region: %s (secenekler: %s)" % (region, ", ".join(REGIONS)))
        self.base = REGIONS[region]
        self.device_id = cfg.get("device_id", "")
        self._token = None

    # --- imza -------------------------------------------------------------
    # Resmi SDK algoritmasi (tuya-iot-python-sdk / tinytuya new_sign_algorithm):
    #   strToSign = METHOD \n sha256hex(body) \n \n path(+query)
    #   msg       = client_id + access_token + t + strToSign
    def _sign(self, method, path, token="", body=b""):
        t = str(int(time.time() * 1000))
        content_hash = hashlib.sha256(body).hexdigest()
        sts = method + "\n" + content_hash + "\n" + "\n" + path
        msg = self.client_id + (token or "") + t + sts
        sign = hmac.new(
            self.client_secret.encode("utf-8"),
            msg.encode("utf-8"),
            hashlib.sha256,
        ).hexdigest().upper()
        headers = {
            "client_id": self.client_id,
            "sign": sign,
            "sign_method": "HMAC-SHA256",
            "t": t,
            "Content-Type": "application/json",
        }
        if token:
            headers["access_token"] = token
        return headers

    # --- token ------------------------------------------------------------
    def token(self):
        if self._token:
            return self._token
        if os.path.exists(TOKEN_CACHE):
            try:
                with open(TOKEN_CACHE, "r", encoding="utf-8") as f:
                    c = json.load(f)
                if c.get("client_id") == self.client_id and c.get("expire", 0) > time.time() + 60:
                    self._token = c["access_token"]
                    return self._token
            except Exception:
                pass
        path = "/v1.0/token?grant_type=1"
        headers = self._sign("GET", path)
        body = self._request("GET", path, headers, None)
        if not body.get("success"):
            sys.exit("Token alinamadi: %s %s (client_id/secret ve region dogru mu?)"
                     % (body.get("code"), body.get("msg")))
        res = body["result"]
        self._token = res["access_token"]
        with open(TOKEN_CACHE, "w", encoding="utf-8") as f:
            json.dump({
                "client_id": self.client_id,
                "access_token": self._token,
                "expire": time.time() + int(res.get("expire_time", 7200)),
            }, f)
        try:
            os.chmod(TOKEN_CACHE, 0o600)
        except Exception:
            pass
        return self._token

    # --- istek ------------------------------------------------------------
    def _request(self, method, path, headers, body_bytes):
        req = urllib.request.Request(self.base + path, data=body_bytes, headers=headers, method=method)
        try:
            with urllib.request.urlopen(req, timeout=20) as r:
                return json.loads(r.read().decode("utf-8"))
        except urllib.error.HTTPError as e:
            raw = e.read().decode("utf-8", "ignore")
            sys.exit("HTTP %s: %s" % (e.code, raw))
        except urllib.error.URLError as e:
            sys.exit("Baglanti hatasi: %s" % e)

    def api(self, method, path, payload=None):
        body_bytes = b""
        if payload is not None:
            body_bytes = json.dumps(payload, separators=(",", ":")).encode("utf-8")
        out = None
        for attempt in (0, 1):
            headers = self._sign(method, path, self.token(), body_bytes)
            if body_bytes:
                headers["Content-Type"] = "application/json"
            out = self._request(method, path, headers, body_bytes if payload is not None else None)
            if out.get("success", False):
                return out.get("result")
            # 1010 / 1011 / 1004 = token gecersiz -> bir kez yenileyip tekrar dene
            if attempt == 0 and out.get("code") in (1010, 1011):
                self._invalidate_token()
                continue
            break
        print(json.dumps(out, indent=2, ensure_ascii=False))
        sys.exit("Istek basarisiz: %s %s" % (out.get("code"), out.get("msg")))

    def _invalidate_token(self):
        self._token = None
        try:
            if os.path.exists(TOKEN_CACHE):
                os.remove(TOKEN_CACHE)
        except Exception:
            pass

    # --- kisayollar -------------------------------------------------------
    def device(self):
        """/v1.0/iot-03/devices/{id} projede yetkili degil (1106); legacy ucu kullan."""
        ok, code, msg, res = self.try_api("GET", "/v1.0/devices/%s" % self.device_id)
        if not ok:
            sys.exit("Cihaz bilgisi alinamadi: %s %s" % (code, msg))
        return res

    def spec(self):
        return self.api("GET", "/v1.0/iot-03/devices/%s/specification" % self.device_id)

    def dp_map(self):
        """code <-> dp_id eslesmesi. Eski ama hala calisan endpoint."""
        return self.api("GET", "/v1.0/devices/%s/specifications" % self.device_id)

    def status(self):
        return self.api("GET", "/v1.0/iot-03/devices/%s/status" % self.device_id)

    def command(self, code, value):
        payload = {"commands": [{"code": code, "value": value}]}
        for path in ("/v1.0/iot-03/devices/%s/commands" % self.device_id,
                     "/v1.0/devices/%s/commands" % self.device_id):
            body = json.dumps(payload, separators=(",", ":")).encode("utf-8")
            headers = self._sign("POST", path, self.token(), body)
            out = self._request("POST", path, headers, body)
            if out.get("success"):
                return out.get("result")
            last = out
        print(json.dumps(last, indent=2, ensure_ascii=False))
        sys.exit("Komut gonderilemedi: %s %s" % (last.get("code"), last.get("msg")))

    def timers(self):
        return self.api("GET", "/v1.0/devices/%s/timers" % self.device_id)

    def try_api(self, method, path):
        """Hata firlatmadan sonucu dondurur: (ok, code, msg, result)"""
        headers = self._sign(method, path, self.token(), b"")
        try:
            out = self._request(method, path, headers, None)
        except SystemExit:
            return False, "-", "istisna", None
        if out.get("success"):
            return True, 0, "", out.get("result")
        return False, out.get("code"), out.get("msg"), None


def detect_local_version(dev_id, ip, key, cfg):
    """Yerel protokol surumunu dener: 3.3 -> 3.5 -> 3.4 -> 3.1"""
    try:
        import tinytuya  # noqa
    except ImportError:
        return cfg.get("local_version", 3.3), False
    for v in [3.3, 3.5, 3.4, 3.1]:
        try:
            d = tinytuya.Device(dev_id, ip, key, version=v)
            d.set_socketTimeout(3)
            st = d.status()
            if isinstance(st, dict) and "dps" in st:
                return v, True
        except Exception:
            continue
    return cfg.get("local_version", 3.3), False


def is_private_ip(ip):
    try:
        p = [int(x) for x in ip.split(".")]
    except Exception:
        return False
    if len(p) != 4:
        return False
    return (p[0] == 10
            or (p[0] == 192 and p[1] == 168)
            or (p[0] == 172 and 16 <= p[1] <= 31))


def cmd_save_local(cfg):
    c = Cloud(cfg)
    info = c.device()
    key = info.get("local_key")
    ip = info.get("ip")
    if not key:
        sys.exit("local_key gelmedi. Smart Home Basic Service yetkisi ekli mi?")
    print("device_id : %s" % info.get("id"))
    print("local_key : %s" % ("*" * (len(key) - 4) + key[-4:]))
    print("ip        : %s" % (ip or "(cihaz kapali / IP yok)"))
    print("time_zone : %s" % info.get("time_zone"))
    print("online    : %s" % info.get("online"))

    cfg["device_id"] = info.get("id", cfg.get("device_id"))
    cfg["local_key"] = key
    if ip:
        if not is_private_ip(ip):
            print("\n!! UYARI: bulut '%s' dondurdu, bu WAN (public) IP." % ip)
            print("   Yerel kontrol LAN IP'si ister. Ayri komutla bul:")
            print("     python3 -m tinytuya scan")
            print("   veya router'in DHCP istemci listesinden cihazin 192.168.x.x adresini al.")
            print("   Bulunca config.json -> ip alanina yaz.\n")
        else:
            cfg["ip"] = ip
            v, ok = detect_local_version(cfg["device_id"], ip, key, cfg)
            cfg["local_version"] = v
            print("yerel protokol: %s%s" % (v, "" if ok else " (dogrulanamadi - tinytuya yok veya cihaz kapali)"))


    with open(CONFIG_PATH, "w", encoding="utf-8") as f:
        json.dump(cfg, f, indent=2, ensure_ascii=False)
    try:
        os.chmod(CONFIG_PATH, 0o600)
    except Exception:
        pass
    print("\nconfig.json guncellendi.")


def main():
    p = argparse.ArgumentParser(description="Tuya bulut istemcisi")
    p.add_argument("command", choices=[
        "device", "spec", "dps", "status", "switch", "mode", "current",
        "timers", "save-local", "raw", "diag",
    ])
    p.add_argument("arg", nargs="?", help="switch on|off, mode <enum>, current <A>, raw <path>")
    a = p.parse_args()

    cfg = load_config()
    if not cfg.get("device_id") and a.command != "raw":
        sys.exit("config.json icinde device_id yok.")

    if a.command == "save-local":
        cmd_save_local(cfg)
        return

    c = Cloud(cfg)

    if a.command == "diag":
        did = cfg["device_id"]
        paths = [
            "/v1.0/iot-03/devices?page_no=1&page_size=50",
            "/v1.0/iot-03/devices/%s" % did,
            "/v1.0/iot-03/devices/%s/specification" % did,
            "/v1.0/iot-03/devices/%s/status" % did,
            "/v1.0/devices/%s" % did,
            "/v1.0/devices/%s/specifications" % did,
            "/v1.0/devices/%s/status" % did,
            "/v1.0/devices/%s/timers" % did,
            "/v1.0/iot-03/devices/%s/functions" % did,
        ]
        for p_ in paths:
            ok, code, msg, res = c.try_api("GET", p_)
            flag = "OK  " if ok else "FAIL"
            print("%s %-55s %s %s" % (flag, p_, code, msg))
            if ok and "/specifications" in p_:
                print("     -> dp_id haritasi alindi (%d function)" % len(res.get("functions", [])))
            if ok and p_.endswith("devices?page_no=1&page_size=50"):
                print("     -> bagli hesapta %d cihaz" % len(res.get("devices", [])))
        return

    if a.command == "device":
        r = c.device()
        r = dict(r)
        if r.get("local_key"):
            k = r["local_key"]
            r["local_key"] = "*" * (len(k) - 4) + k[-4:]
        print(json.dumps(r, indent=2, ensure_ascii=False))

    elif a.command == "spec":
        print(json.dumps(c.spec(), indent=2, ensure_ascii=False))

    elif a.command == "dps":
        r = c.dp_map()
        print("%-22s %-6s %-9s %s" % ("code", "dp_id", "type", "values"))
        print("-" * 78)
        for f in r.get("functions", []):
            print("%-22s %-6s %-9s %s" % (f.get("code"), f.get("dp_id"), f.get("type"), f.get("values")))
        for f in r.get("status", []):
            if any(x.get("code") == f.get("code") for x in r.get("functions", [])):
                continue
            print("%-22s %-6s %-9s %s" % (f.get("code"), f.get("dp_id"), f.get("type"), f.get("values")))
        if not any(f.get("dp_id") for f in r.get("functions", [])):
            print("\nNOT: Bu projede bulut uclari dp_id dondurmuyor (sadece 'code').")
            print("     Yerel kontrol icin DP numaralarini bulut yerine cihazdan ogren:")
            print("       python3 tuya_local.py raw")
            print("       python3 tuya_local.py identify")

    elif a.command == "status":
        r = c.status()
        d = dict((x["code"], x["value"]) for x in r)
        for k in sorted(d):
            print("%-22s %s" % (k, d[k]))
        if "forward_energy_total" in d:
            print("\ntoplam enerji: %.2f kWh" % (float(d["forward_energy_total"]) / 100.0))
        if "temp_current" in d:
            print("sicaklik      : %s C" % d["temp_current"])

    elif a.command == "switch":
        if a.arg not in ("on", "off"):
            sys.exit("kullanim: switch on | switch off")
        print(c.command("switch", a.arg == "on"))
        print("switch -> %s" % a.arg)

    elif a.command == "mode":
        modes = ["charge_now", "charge_pct", "charge_energy", "charge_schedule"]
        if a.arg not in modes:
            sys.exit("kullanim: mode <%s>" % "|".join(modes))
        print(c.command("work_mode", a.arg))
        print("work_mode -> %s" % a.arg)

    elif a.command == "current":
        if not a.arg:
            sys.exit("kullanim: current <amper>")
        print(c.command("charge_cur_set", int(a.arg)))

    elif a.command == "timers":
        print(json.dumps(c.timers(), indent=2, ensure_ascii=False))

    elif a.command == "raw":
        if not a.arg:
            sys.exit("kullanim: raw </v1.0/...>")
        print(json.dumps(c.api("GET", a.arg), indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
