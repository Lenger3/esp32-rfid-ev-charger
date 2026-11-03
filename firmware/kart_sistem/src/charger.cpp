#include "charger.h"
#include "net.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <LynkTuya.h>

// ===========================================================================
//  22 kW wallbox — Tuya LAN v3.5 surucusu
//
//  Protokol : v3.5 (6699 cerceve, AES-128-GCM, mbedTLS)
//  Kutuphane: LynkTuyaLocal 1.1.0
//
//  DP HARITASI (2026-10-30, cihazda `raw` ile dogrulandi)
//    18 switch                bool     sarj ac/kapa           [YAZ]
//     4 charge_cur_set        int      A                      [YAZ]
//    14 work_mode             enum     charge_now/...
//     3 work_state            enum     charger_free/charging/...
//    10 power_total           int      kW  x1000
//     1 forward_energy_total  int      kWh x100
//    25 charge_energy_once    int      kWh x100
//    24 temp_current          int      C
//    28 charge_time           int      dk x10
// ===========================================================================

#define DP_SWITCH       "18"
#define DP_CURRENT      "4"
#define DP_WORK_STATE   "3"
#define DP_POWER        "10"
#define DP_TOTAL_KWH    "1"
#define DP_SESSION_KWH  "25"
#define DP_TEMP         "24"
#define DP_SINGLE_POWER "9"
#define DP_MODE         "14"

// --------------------------------------------------------------- base64
// Tuya bazi buyuk Integer DP'leri base64 "raw" olarak gonderir.
static int b64val(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}
static int base64Decode(const char* s, int len, uint8_t* out, int outMax) {
  int n = 0, acc = 0, bits = 0;
  for (int i = 0; i < len; i++) {
    if (s[i] == '=' || s[i] == '\n' || s[i] == '\r') continue;
    int v = b64val(s[i]);
    if (v < 0) return 0;
    acc = (acc << 6) | v;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      if (n >= outMax) return 0;
      out[n++] = (uint8_t)((acc >> bits) & 0xFF);
    }
  }
  return n;
}

Charger charger;

// LynkTuya cihazin ittigi DP guncellemelerini buraya birakir.
// Boylece sarj baslar baslamaz ekran/panel aninda guncellenir.
static void onDpsPush(const String& js) {
  charger.applyPush(js);
}

void Charger::applyPush(const String& js) {
  pushes++;
  applyJson(js);
  lastData = millis();
}

bool Charger::begin(bool useSim) {
  sim = useSim;
  t0 = millis();
  if (sim) {
    Serial.println("[charger] SIMULATOR");
    return true;
  }
  Serial.println("[charger] Tuya LAN v3.5 modu");
  return true;
}

void Charger::configure(const String& ip, const char* deviceId, const char* localKey) {
  if (sim) return;
  if (!localKey || strlen(localKey) != 16) {
    Serial.printf("[charger] local_key gecersiz (len=%u) — panelden girin\n",
                  localKey ? (unsigned)strlen(localKey) : 0u);
    return;
  }

  bool keyChanged = (strncmp(devKey, localKey, 16) != 0);
  memcpy(devKey, localKey, 16);
  devKey[16] = 0;
  devId = deviceId ? deviceId : "";
  haveCreds = true;

  if (ip.length() && devIp != ip) {
    devIp = ip;
    if (tuya) { delete (LynkTuyaDevice<TUYA_V35>*)tuya; tuya = nullptr; }
  }
  if (keyChanged || !tuya) {
    if (tuya) { delete (LynkTuyaDevice<TUYA_V35>*)tuya; tuya = nullptr; }
    Serial.printf("[charger] kimlik alindi (id %s) ip=%s\n",
                  devId.c_str(), devIp.length() ? devIp.c_str() : "-");
  }
}

// IP bilinmiyorsa AP'de ara; bulunca nesneyi kur.
bool Charger::bindIp() {
  if (!haveCreds) return false;
  if (!devIp.length()) {
    String found;
    if (!netFindCharger(&found, 2500)) {
      nextTry = millis() + 15000;      // tarama 2.5 sn bloklar, sik denemeyelim
      return false;
    }
    devIp = found;
    Serial.printf("[charger] cihaz bulundu: %s\n", devIp.c_str());
  }
  if (tuya) return true;
  IPAddress a;
  if (!a.fromString(devIp)) {
    Serial.printf("[charger] IP gecersiz: %s\n", devIp.c_str());
    devIp = "";
    return false;
  }
  auto* d = new LynkTuyaDevice<TUYA_V35>(a, devKey);
  d->setTimeout(4000);
  d->onDpsUpdate(onDpsPush);        // cihaz ittiginde aninda oku
  tuya = d;
  return true;
}

void Charger::poll() {
  uint32_t now = millis();

  if (sim) {
    if (now - lastPoll < 1000) return;
    lastPoll = now;
    if (chOn) simKwh += (amps * 0.230f) / 3600.0f;
    return;
  }

  // --- push pompasi: her loop turunda UCUZ bir available() kontrolu ---
  // Cihaz durum degistirdiginde (arac takildi / sarj basladi / bitti)
  // kendisi paket iter; 1 sn beklemeye gerek kalmaz.
  if (tuya) ((LynkTuyaDevice<TUYA_V35>*)tuya)->poll(0);

  if ((int32_t)(now - nextTry) < 0) return;
  if (now - lastPoll < 1000) return;          // saniyede bir yoklama
  lastPoll = now;

  if (!haveCreds) {
    static uint32_t lastWarn = 0;
    if (now - lastWarn > 60000) {
      lastWarn = now;
      Serial.println("[charger] panelden cihaz bilgisi bekleniyor (Cihaz > Tuya ID + local_key)");
    }
    return;
  }

  if (!bindIp()) return;

  String js;
  auto* d = (LynkTuyaDevice<TUYA_V35>*)tuya;

  // autoClose = false  -> TCP + oturum anahtari ACIK kalir.
  // Her yoklamada yeniden el sikismasi 1-2 sn kaybediyordu.
  if (d->getStatus(js, false)) {
    fails = 0;
    nextTry = now + 1000;
    lastData = now;
    applyJson(js);

    static uint32_t lastDump = 0;
    if (now - lastDump > 60000) {
      lastDump = now;
      Serial.printf("[charger] dps %s (push=%lu)\n", js.c_str(), (unsigned long)pushes);
    }
  } else {
    fails++;
    d->closeConnection();        // yarim soket yeniden baglanmayi engelliyordu

    if (fails == 3 || fails % 20 == 0) {
      Serial.printf("[charger] %d hata: IP yeniden aranacak (%s)\n",
                    fails, devIp.c_str());
      devIp = "";
      if (tuya) { delete (LynkTuyaDevice<TUYA_V35>*)tuya; tuya = nullptr; }
    }

    uint32_t back = 2000UL;
    for (int i = 1; i < fails && back < 60000UL; i++) back *= 2;
    if (back > 60000UL) back = 60000UL;
    nextTry = now + back;
    if (fails == 1 || fails % 10 == 0) {
      Serial.printf("[charger] baglanti yok (%s) deneme=%d, %lus sonra\n",
                    devIp.c_str(), fails, (unsigned long)(back / 1000));
    }
    wsText = "offline";
  }
}

// {"dps":{"1":15867,"3":"charger_end","4":16,...}}
void Charger::applyJson(const String& js) {
  JsonDocument doc;
  if (deserializeJson(doc, js)) return;

  JsonVariant dps = doc["dps"];
  if (dps.isNull()) dps = doc["data"]["dps"];
  if (!dps.is<JsonObject>()) return;

  // DP degeri int, string sayi ya da base64 "raw" olarak gelebilir.
  // Ucunu de coz; cozulemezse NAN.
  auto num = [&](const char* k, float scale) -> float {
    JsonVariant v = dps[k];
    if (v.isNull()) return NAN;

    if (v.is<int>())   return v.as<int>() / scale;
    if (v.is<float>()) return v.as<float>() / scale;

    if (v.is<const char*>()) {
      const char* s = v.as<const char*>();
      if (!s || !*s) return NAN;
      bool numeric = true;
      for (const char* p = s; *p; p++) {
        if (!isdigit((unsigned char)*p) && *p != '-' && *p != '.') { numeric = false; break; }
      }
      if (numeric) return atof(s) / scale;
      // base64 -> big endian int
      int len = strlen(s);
      if (len > 0 && len % 4 == 0 && len <= 48) {
        uint8_t buf[36];
        int n = base64Decode(s, len, buf, sizeof(buf));
        if (n > 0 && n <= 8) {
          uint64_t acc = 0;
          for (int i = 0; i < n; i++) acc = (acc << 8) | buf[i];
          return (float)acc / scale;
        }
      }
    }
    return NAN;
  };

  if (dps[DP_SWITCH].is<bool>()) chOn = dps[DP_SWITCH].as<bool>();

  JsonVariant ws = dps[DP_WORK_STATE];
  if (ws.is<const char*>()) {
    wsText = ws.as<const char*>();
  } else if (ws.is<int>()) {
    static const char* names[] = {"charger_free","charger_insert","charger_free_fault",
                                  "charger_wait","charger_charging","charger_pause",
                                  "charger_end","charger_fault"};
    int i = ws.as<int>();
    wsText = (i >= 0 && i < 8) ? names[i] : "charger_free";
  }

  float v;
  bool wasCharging = (wsText == "charger_charging");
  float prevTotal = totalKwhVal;
  if (!isnan(v = num(DP_TOTAL_KWH, 100.0f))) totalKwhVal = v;

  // Oturum kWh'i = omur boyu sayacin farki (DP 25 cogu cihazda
  // sadece sarj bitince yazilir, bu yuzden birincil kaynak DP 1).
  if (kwhBase > 0 && totalKwhVal >= kwhBase) sessKwh = totalKwhVal - kwhBase;
  if (!isnan(v = num(DP_SESSION_KWH, 100.0f)) && v > sessKwh) sessKwh = v;

  // anlik guc: DP 10 (toplam), 0 ise DP 9 (tek faz)
  float p = num(DP_POWER, 1000.0f);
  if (isnan(p) || p <= 0.0f) {
    float p9 = num(DP_SINGLE_POWER, 1000.0f);
    if (!isnan(p9)) p = p9;
  }
  if (!isnan(p)) pKw = p;

  if (!isnan(v = num(DP_TEMP, 1.0f)))    tempVal = v;
  if (!isnan(v = num(DP_CURRENT, 1.0f))) curSet = (uint8_t)v;

  // Sarj basladiysa ve baz yoksa simdiki sayaci referans al
  if (!wasCharging && wsText == "charger_charging" && kwhBase <= 0.0f) {
    kwhBase = prevTotal;              // oturum basi sayaci
    sessKwh = 0;
    t0 = millis();
    Serial.printf("[charger] oturum basi: %.2f kWh\n", (double)kwhBase);
  }
  if (wasCharging && wsText != "charger_charging") {
    // oturum bitti: bir sonraki oturum icin baz temizle
    kwhBase = 0;
  }
}

bool Charger::enable(bool on) {
  if (sim) {
    chOn = on;
    if (on) resetSession();
    Serial.printf("[charger] (sim) enable=%d\n", on);
    return true;
  }
  if (!bindIp()) { Serial.println("[charger] enable: cihaz yok"); return false; }

  auto* d = (LynkTuyaDevice<TUYA_V35>*)tuya;
  bool ok = d->setDps(DP_SWITCH, on, true);
  if (ok) {
    chOn = on;
    if (on) resetSession();
  }
  Serial.printf("[charger] enable=%d -> %s\n", on, ok ? "OK" : "HATA");
  return ok;
}

bool Charger::setWorkMode(const char* mode) {
  if (sim) return true;
  if (!bindIp()) return false;
  auto* d = (LynkTuyaDevice<TUYA_V35>*)tuya;
  bool ok = d->setDps(DP_MODE, mode, true);
  Serial.printf("[charger] work_mode=%s -> %s\n", mode, ok ? "OK" : "HATA");
  return ok;
}

// Kartsiz sarji engelleme: role kapatmak yerine cihazi "zamanlamali" moda
// alip bekletiyoruz. Cihaz kendi kendine SARJ BASLATMAZ (charger_wait),
// boylece 22 kW kontaktor surekli ac/kapa dongusune girmez.
bool Charger::parkLock() {
  if (sim) { chOn = false; parked = true; return true; }
  if (!bindIp()) return false;

  auto* d = (LynkTuyaDevice<TUYA_V35>*)tuya;

  // SIRA ONEMLI: once mode'u degistir, sonra role'yu birak.
  // Kontaktor kapaliyken switch'i dusurmek cihazi "durduruldu" durumuna
  // alir; cihaz tekrar baslamak isterse mode onu tutar.
  d->setDps(DP_SWITCH, false, false);
  setWorkMode("charge_schedule");

  chOn   = false;
  parked = true;
  Serial.println("[charger] PARK: kartsiz sarj engellendi");
  return true;
}

bool Charger::parkLocked() { return parked; }

void Charger::releaseParkLock() {
  if (sim) { parked = false; return; }
  if (!parked) return;
  setWorkMode("charge_now");
  parked = false;
  Serial.println("[charger] PARK kaldirildi (work_mode=charge_now)");
}

bool Charger::setCurrent(uint8_t a) {
  if (a == 0) a = 6;
  if (sim) { amps = a; return true; }
  if (!bindIp()) { Serial.println("[charger] akim: cihaz yok"); return false; }

  auto* d = (LynkTuyaDevice<TUYA_V35>*)tuya;
  bool ok = d->setDps(DP_CURRENT, (int)a, true);
  if (ok) { amps = a; curSet = a; }
  Serial.printf("[charger] akim=%uA -> %s\n", a, ok ? "OK" : "HATA");
  return ok;
}

ChargerState Charger::state() {
  if (!sim && wsText == "offline") return CH_OFFLINE;
  if (sim) return chOn ? CH_CHARGING : CH_FREE;

  if (wsText == "charger_charging") return CH_CHARGING;
  if (wsText == "charger_pause")    return CH_PAUSED;
  if (wsText == "charger_end")      return CH_DONE;
  if (wsText == "charger_insert")   return CH_INSERT;
  if (wsText == "charger_wait")     return CH_WAITING;
  if (wsText == "charger_fault" || wsText == "charger_free_fault") return CH_FAULT;
  return CH_FREE;
}

bool Charger::online() {
  if (sim) return true;
  return haveCreds && wsText != "offline";
}

bool Charger::carConnected() {
  if (sim) return chOn;
  return wsText == "charger_insert" || wsText == "charger_wait" ||
         wsText == "charger_charging" || wsText == "charger_pause" ||
         wsText == "charger_end";
}

uint8_t Charger::currentA() {
  bool on = sim ? chOn : (wsText == "charger_charging");
  return on ? (curSet ? curSet : amps) : 0;
}
float Charger::voltage()    { return currentA() ? 230.0f : 0; }
float Charger::powerKw()    { return sim ? (chOn ? amps * 0.230f : 0) : pKw; }
float Charger::tempC()      { return sim ? (24.0f + (chOn ? 6.0f : 0)) : tempVal; }
float Charger::sessionKwh() { return sim ? simKwh : sessKwh; }
float Charger::totalKwh()   { return sim ? (142.24f + simKwh) : totalKwhVal; }

const char* Charger::workStateText() { return wsText.c_str(); }
int      Charger::linkFails()  { return fails; }
uint32_t Charger::dataAgeSec() {
  if (!lastData) return 0xFFFFFFFFu;
  return (millis() - lastData) / 1000;
}

uint32_t Charger::sessionSec() {
  if (sim) return chOn ? (millis() - t0) / 1000 : 0;
  return (wsText == "charger_charging") ? (millis() - t0) / 1000 : 0;
}

void Charger::resetSession() {
  simKwh = 0;
  // oturum basi: omur boyu sayaci referans al (delta buradan hesaplanir)
  kwhBase = totalKwhVal;
  sessKwh = 0;
  t0 = millis();
}
