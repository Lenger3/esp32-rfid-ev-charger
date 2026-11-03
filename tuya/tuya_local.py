#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
wallbox / Tuya EV sarj cihazi - YEREL (LAN) kontrol istemcisi

Gereksinim:  pip install tinytuya
Gereken config alanlari: device_id, ip, local_key, local_version

Kullanim:
    python3 tuya_local.py status        # cozulmus anlik durum
    python3 tuya_local.py raw           # ham DP JSON (eslesme ogrenmek icin)
    python3 tuya_local.py switch on|off
    python3 tuya_local.py mode charge_now|charge_schedule|charge_pct|charge_energy
    python3 tuya_local.py current 16
    python3 tuya_local.py watch 5       # 5 sn'de bir durum
    python3 tuya_local.py session start # sayac baz degeri al
    python3 tuya_local.py session stop  # kWh farki + sure
    python3 tuya_local.py probe         # protokol surumunu test et (3.3/3.5/3.4/3.1)

Bulut gerekmez. Tek dogrulama: local_key ile AES.
"""
import argparse
import json
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
CONFIG_PATH = os.path.join(HERE, "config.json")
SESSION_PATH = os.path.join(HERE, ".session.json")

WORK_STATES = {
    "charger_free": "bos / arac yok",
    "charger_insert": "arac takili, baslamayi bekliyor",
    "charger_free_fault": "ariza (arac yok)",
    "charger_wait": "bekliyor (planli / onay bekliyor)",
    "charger_charging": "SARJ EDILIYOR",
    "charger_pause": "duraklatildi",
    "charger_end": "sarj bitti",
    "charger_fault": "ARIZA",
}


def load_config():
    if not os.path.exists(CONFIG_PATH):
        sys.exit("config.json bulunamadi.")
    with open(CONFIG_PATH, "r", encoding="utf-8") as f:
        cfg = json.load(f)
    for k in ("device_id", "local_key"):
        if not cfg.get(k):
            sys.exit("config.json icinde %s yok. Once: python3 tuya_cloud.py save-local" % k)
    return cfg


def connect(cfg, version=None):
    try:
        import tinytuya
    except ImportError:
        sys.exit("tinytuya kurulu degil:  pip install tinytuya")
    v = version or cfg.get("local_version") or 3.3
    ip = cfg.get("ip")
    if not ip:
        sys.exit("config.json icinde ip yok. Once: python3 tuya_cloud.py save-local")
    d = tinytuya.Device(cfg["device_id"], ip, cfg["local_key"], version=float(v))
    d.set_socketTimeout(5)
    return d


def get_dps(d):
    st = d.status()
    if not isinstance(st, dict) or "dps" not in st:
        sys.exit("Cihazdan cevap alinamadi: %s\n(IP/protokol surumu/local_key hatali olabilir)" % st)
    return st["dps"]


def _b64_int(v):
    """Bazi DP'ler Integer yerine Raw (base64) geliyor: 'AAA=' -> 0"""
    if isinstance(v, str):
        try:
            import base64
            b = base64.b64decode(v)
            return int.from_bytes(b, "big") if b else 0
        except Exception:
            return v
    return v


def decode(dps, dp):
    """config['dp'] eslesmesine gore okunabilir ozet uretir."""
    out = {}
    if not dp:
        return {"raw": dps}

    def val(code, scale=1, suffix=""):
        i = dp.get(code)
        if i is None:
            return None
        v = _b64_int(dps.get(str(i)))
        if v is None:
            return None
        return round(float(v) / scale, 3) if scale != 1 else v

    ws = val("work_state")
    out["durum"] = "%s (%s)" % (WORK_STATES.get(ws, ws), ws) if ws else "-"
    out["arac_takili"] = ws in ("charger_insert", "charger_wait", "charger_charging",
                                "charger_pause", "charger_end")
    out["sarj_ediyor"] = ws == "charger_charging"
    out["switch"] = val("switch")
    out["work_mode"] = val("work_mode")
    out["akim_limiti_A"] = val("charge_cur_set")
    out["anlik_guc_kW"] = val("power_total", 1000)
    out["oturum_kWh"] = val("charge_energy_once", 100)
    out["toplam_kWh"] = val("forward_energy_total", 100)
    out["sicaklik_C"] = val("temp_current")
    out["sarj_suresi_dk"] = val("charge_time", 10)
    unknown = dict((k, v) for k, v in dps.items() if str(k) not in set(str(x) for x in dp.values()))
    if unknown:
        out["eslesmeyen_dp"] = unknown
    return out


def dp_id(cfg, code):
    dp = cfg.get("dp") or {}
    if code not in dp:
        sys.exit("config.json -> dp.%s tanimli degil. 'python3 tuya_local.py identify' calistir." % code)
    return dp[code]


def identify(cfg):
    """Bulutun bildigi DEGERLER ile yerel sayisal DP'leri eslestirir.
    Hicbir yazma yapmaz: sadece okur ve config.json'a tahmini haritayi yazar."""
    sys.path.insert(0, HERE)
    try:
        import tuya_cloud
    except Exception as e:
        sys.exit("tuya_cloud.py import edilemedi: %s" % e)
    cloud = tuya_cloud.Cloud(cfg)
    status = cloud.status()
    cvals = dict((x["code"], x["value"]) for x in status)

    d = connect(cfg)
    dps = get_dps(d)
    norm = lambda v: (str(v).lower() if isinstance(v, bool) else str(v))

    print("yerel DP'ler: %s\n" % json.dumps(dps, ensure_ascii=False))
    print("%-22s %-10s %-22s %s" % ("code", "bulut", "eslesen dp_id", "guven"))
    print("-" * 78)
    mapping = {}
    for code, cv in sorted(cvals.items()):
        hits = [k for k, v in dps.items() if norm(v) == norm(cv)]
        if len(hits) == 1:
            mapping[code] = int(hits[0])
            print("%-22s %-10s %-22s TEK" % (code, str(cv)[:9], hits[0]))
        elif hits:
            print("%-22s %-10s %-22s BELIRSIZ %s" % (code, str(cv)[:9], hits[0], hits))
        else:
            print("%-22s %-10s %-22s yok (cihaz degeri degismis olabilir)" % (code, str(cv)[:9], "-"))

    if mapping:
        cur = dict(cfg.get("dp") or {})
        cur.update(mapping)
        cfg["dp"] = cur
        with open(CONFIG_PATH, "w", encoding="utf-8") as f:
            json.dump(cfg, f, indent=2, ensure_ascii=False)
        print("\n%d eslesme config.json -> dp bloguna yazildi: %s" % (len(mapping), mapping))
        miss = [c for c in ("switch", "work_mode", "charge_cur_set") if c not in cur]
        if miss:
            print("eksik kalanlar (elle dogrulanmali): %s" % ", ".join(miss))
    else:
        print("\nHicbir eslesme bulunamadi. Bulut degerleri cihazda degismis olabilir; "
              "status komutunu buluttan tekrar cekip identify'i tekrar calistir.")


def main():
    p = argparse.ArgumentParser(description="Yerel Tuya EV sarj kontrolu")
    p.add_argument("command", choices=["status", "raw", "switch", "mode", "current",
                                       "watch", "session", "probe", "identify"])
    p.add_argument("arg", nargs="?")
    a = p.parse_args()
    cfg = load_config()
    dp = cfg.get("dp") or {}

    if a.command == "identify":
        identify(cfg)
        return

    if a.command == "probe":
        for v in [3.3, 3.5, 3.4, 3.1]:
            try:
                d = connect(cfg, v)
                st = d.status()
                if isinstance(st, dict) and "dps" in st:
                    print("protokol %s -> CALISIYOR (dps: %s)" % (v, list(st["dps"].keys())[:8]))
                    cfg["local_version"] = v
                    with open(CONFIG_PATH, "w", encoding="utf-8") as f:
                        json.dump(cfg, f, indent=2, ensure_ascii=False)
                    return
                print("protokol %s -> cevap yok: %s" % (v, st))
            except Exception as e:
                print("protokol %s -> hata: %s" % (v, e))
        sys.exit("Hicbir surumde baglanti kurulamadi.")

    d = connect(cfg)

    if a.command == "raw":
        print(json.dumps(get_dps(d), indent=2, ensure_ascii=False))

    elif a.command == "status":
        print(json.dumps(decode(get_dps(d), dp), indent=2, ensure_ascii=False))

    elif a.command == "switch":
        if a.arg not in ("on", "off"):
            sys.exit("kullanim: switch on|off")
        i = dp_id(cfg, "switch")
        print(d.set_value(i, a.arg == "on"))
        print("switch (DP %s) -> %s" % (i, a.arg))

    elif a.command == "mode":
        modes = ["charge_now", "charge_pct", "charge_energy", "charge_schedule"]
        if a.arg not in modes:
            sys.exit("kullanim: mode <%s>" % "|".join(modes))
        i = dp_id(cfg, "work_mode")
        print(d.set_value(i, a.arg))
        print("work_mode (DP %s) -> %s" % (i, a.arg))

    elif a.command == "current":
        if not a.arg:
            sys.exit("kullanim: current <amper>")
        i = dp_id(cfg, "charge_cur_set")
        print(d.set_value(i, int(a.arg)))

    elif a.command == "watch":
        period = float(a.arg or 5)
        try:
            while True:
                s = decode(get_dps(d), dp)
                print("%s | %s | %.2f kW | %.2f kWh | %s C" % (
                    time.strftime("%H:%M:%S"),
                    s.get("durum"),
                    s.get("anlik_guc_kW") or 0.0,
                    s.get("oturum_kWh") or 0.0,
                    s.get("sicaklik_C"),
                ))
                time.sleep(period)
        except KeyboardInterrupt:
            print("\ndurduruldu")

    elif a.command == "session":
        code = "forward_energy_total"
        i = dp_id(cfg, code)
        v = get_dps(d).get(str(i))
        if v is None:
            sys.exit("Enerji sayaci DP'si (%s) okunamadi" % i)
        if a.arg == "start":
            data = {"start_ts": time.time(), "start_kwh": float(v) / 100.0}
            with open(SESSION_PATH, "w", encoding="utf-8") as f:
                json.dump(data, f)
            print("oturum basladi: %.2f kWh" % data["start_kwh"])
        elif a.arg == "stop":
            if not os.path.exists(SESSION_PATH):
                sys.exit("acik oturum yok")
            with open(SESSION_PATH, "r", encoding="utf-8") as f:
                s = json.load(f)
            end = float(v) / 100.0
            print(json.dumps({
                "sure_dk": round((time.time() - s["start_ts"]) / 60.0, 1),
                "baslangic_kWh": s["start_kwh"],
                "bitis_kWh": end,
                "tuketim_kWh": round(end - s["start_kwh"], 2),
            }, indent=2, ensure_ascii=False))
            os.remove(SESSION_PATH)
        else:
            sys.exit("kullanim: session start|stop")


if __name__ == "__main__":
    main()
