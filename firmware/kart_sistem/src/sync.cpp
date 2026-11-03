#include "sync.h"
#include "config.h"
#include "store.h"
#include "hw.h"
#include "pins.h"
#include "charger.h"
#include "net.h"
#include "outbox.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// Let's Encrypt ISRG Root X1 — Caddy'nin kullandigi kok sertifika.
// Sertifika degisirse guncelle. TLS dogrulamasi kapali degil.
static const char* ROOT_CA = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

static WiFiClientSecure tls;
static bool     started   = false;
static uint32_t lastBeat  = 0;
static uint32_t lastCfg   = 0;
static uint32_t lastOk    = 0;
static uint32_t lastFlush = 0;
static uint32_t srvRev    = 0;
static int      fails     = 0;

#define BEAT_MS   10000UL   // panel canli veriyi 10 sn'de bir gorur
#define CFG_MS    30000UL

// --------------------------------------------------------------- yardimci
static String uidHex(const uint8_t* uid, uint8_t len) {
  char b[20];
  int p = 0;
  for (int i = 0; i < len && p < 16; i++) p += snprintf(b + p, sizeof(b) - p, "%02X", uid[i]);
  return String(b);
}

static int hexToBytes(const char* hex, uint8_t* out, int maxLen) {
  int n = strlen(hex) / 2;
  if (n > maxLen) n = maxLen;
  for (int i = 0; i < n; i++) {
    char s[3] = {hex[i * 2], hex[i * 2 + 1], 0};
    out[i] = (uint8_t)strtol(s, nullptr, 16);
  }
  return n;
}

static String isoNow(uint32_t ts) {
  if (ts < 1700000000) return String("");
  time_t t = ts;
  struct tm g;
  gmtime_r(&t, &g);
  char b[24];
  snprintf(b, sizeof(b), "%04d-%02d-%02dT%02d:%02d:%02dZ",
           g.tm_year + 1900, g.tm_mon + 1, g.tm_mday,
           g.tm_hour, g.tm_min, g.tm_sec);
  return String(b);
}

static void hmToPair(const char* hm, uint8_t* h, uint8_t* m) {
  *h = 0; *m = 0;
  if (!hm || strlen(hm) < 4) return;
  *h = (uint8_t)atoi(hm);
  const char* c = strchr(hm, ':');
  if (c) *m = (uint8_t)atoi(c + 1);
}

// --------------------------------------------------------------- HTTP
// Kalici TLS baglantisi (keep-alive): her istekte yeniden el sikisma
// ESP32'de 1-2 sn yiyor ve loop'u kilitliyordu. Baglanti koparsa
// seffaf sekilde yeniden kurulur.
//
// Butce (budget) ZORUNLU: TLS + baslik + govde beklemeleri toplanip
// 40 sn'ye cikiyor, butonlar donuyordu.
#define HTTP_BUDGET_MS  8000UL

static bool tlsReady = false;
static String tlsHost;

static void tlsDrop() {
  tls.stop();
  tlsReady = false;
}

static bool tlsEnsure(const String& host, uint32_t budgetMs) {
  if (tlsReady && tls.connected()) return true;
  tlsDrop();
  tls.setCACert(ROOT_CA);
  tls.setTimeout(4);
  if (!tls.connect(host.c_str(), 443)) {
    Serial.printf("[sync] TLS baglanti hatasi (%s)\n", host.c_str());
    return false;
  }
  tlsReady = true;
  tlsHost = host;
  (void)budgetMs;
  return true;
}

static bool httpReq(const char* method, const char* path, const String& body,
                    String* resp, uint32_t budgetMs = HTTP_BUDGET_MS) {
  if (WiFi.status() != WL_CONNECTED) { tlsDrop(); return false; }

  uint32_t start = millis();
  auto kalan = [&]() -> int32_t {
    return (int32_t)budgetMs - (int32_t)(millis() - start);
  };

  String url = String(PANEL_URL);
  String host = url.substring(8);
  int slash = host.indexOf('/');
  if (slash > 0) host = host.substring(0, slash);

  // 2 deneme: kalici baglanti bayatsa bir kez yeniden kur
  for (int attempt = 0; attempt < 2; attempt++) {
    if (!tlsEnsure(host, budgetMs)) return false;

    String req = String(method) + " " + path + " HTTP/1.1\r\n";
    req += "Host: " + host + "\r\n";
    req += "Authorization: Bearer " + String(API_TOKEN) + "\r\n";
    req += "User-Agent: ESP32-EVCHARGER/" FW_VERSION "\r\n";
    req += "Connection: keep-alive\r\n";
    if (body.length()) {
      req += "Content-Type: application/json\r\n";
      req += "Content-Length: " + String(body.length()) + "\r\n";
    }
    req += "\r\n";
    req += body;

    tls.print(req);

    // --- durum satiri ---
    String status;
    while (kalan() > 0) {
      if (tls.available()) { status = tls.readStringUntil('\n'); break; }
      if (!tls.connected()) break;
      delay(2);
    }
    if (!status.length()) { tlsDrop(); continue; }   // bayat soket: tekrar dene

    int code = 0;
    int sp = status.indexOf(' ');
    if (sp > 0) code = status.substring(sp + 1).toInt();

    // --- basliklar ---
    int contentLen = -1;
    bool chunked = false, closeConn = false;
    while (kalan() > 0) {
      if (!tls.available() && !tls.connected()) break;
      String line = tls.readStringUntil('\n');
      if (line == "\r" || line.length() == 0) break;
      String low = line;
      low.toLowerCase();
      if (low.startsWith("content-length:")) {
        contentLen = low.substring(15).toInt();
      } else if (low.indexOf("transfer-encoding:") == 0 && low.indexOf("chunked") > 0) {
        chunked = true;
      } else if (low.startsWith("connection:") && low.indexOf("close") > 0) {
        closeConn = true;
      }
    }

    // --- govde ---
    String out;
    if (contentLen >= 0) {
      int got = 0;
      while (got < contentLen && kalan() > 0) {
        while (tls.available() && got < contentLen) { out += (char)tls.read(); got++; }
        if (!tls.connected() && !tls.available()) break;
        delay(2);
      }
      if (got < contentLen) { tlsDrop(); continue; }
    } else {
      // uzunluk yok (chunked): baglanti kapanana kadar oku, sonra kapat
      while (kalan() > 0) {
        while (tls.available()) out += (char)tls.read();
        if (!tls.connected() && !tls.available()) break;
        delay(2);
      }
      tlsDrop();
    }

    if (closeConn || chunked) tlsDrop();

    if (resp) *resp = out;
    Serial.printf("[sync] %s %s -> %d (%lums)\n", method, path, code,
                  (unsigned long)(millis() - start));
    return code >= 200 && code < 300;
  }
  return false;
}

// --------------------------------------------------------------- istekler
// panelde gosterilecek kisa durum metni
static const char* chargerStateText() {
  switch (charger.state()) {
    case CH_CHARGING: return "charging";
    case CH_PAUSED:   return "paused";
    case CH_DONE:     return "done";
    case CH_FAULT:    return "fault";
    case CH_INSERT:   return "plugged";
    case CH_OFFLINE:  return "offline";
    default:          return "idle";
  }
}

// heartbeat yanitindan gelen komutu ANINDA uygula (config turunu bekleme)
static void applyInline(const String& resp) {
  if (!resp.length()) return;
  JsonDocument d;
  if (deserializeJson(d, resp)) return;

  uint32_t rev    = d["rev"] | 0;
  const char* cmd = d["cmd"] | "";
  uint32_t cid    = d["cmd_id"] | 0;

  Settings& st = store.settings();
  if (cid) {
    st.cmd_id = cid;
    st.cmd = (!strcmp(cmd, "start")) ? 1 : (!strcmp(cmd, "stop")) ? 2 : 0;
  }

  // panelde rev ilerlediyse kart listesi/ayarlar hemen cekilir
  if (rev && rev != srvRev) {
    Serial.printf("[sync] rev %u -> %u (hemen config)\n", srvRev, rev);
  }
}

static bool sendHeartbeat() {
  JsonDocument d;
  d["firmware"]  = FW_VERSION;
  d["ip"]        = WiFi.localIP().toString();
  d["state"]     = chargerStateText();
  d["uptime_s"]  = millis() / 1000;
  d["free_heap"] = ESP.getFreeHeap();
  d["card_count"] = store.cardCount();
  // canli sarj verisi (panelde gosterilir)
  d["power_kw"]    = serialized(String(charger.powerKw(), 2));
  d["session_kwh"] = serialized(String(charger.sessionKwh(), 2));
  d["current_a"]   = charger.currentA();
  d["temp_c"]      = serialized(String(charger.tempC(), 1));
  d["session_min"] = charger.sessionSec() / 60;
  d["cmd_id"]      = store.lastCmdId();   // UYGULANAN komut (ack)

  String body, resp;
  serializeJson(d, body);
  bool ok = httpReq("POST", "/api/v1/heartbeat", body, &resp);
  if (ok) applyInline(resp);
  return ok;
}

// paneldeki kart listesini ve ayarlari NVS'ye yaz
static bool pullConfig() {
  String resp;
  if (!httpReq("GET", "/api/v1/config", "", &resp)) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, resp);
  if (err) {
    Serial.printf("[sync] JSON hatasi: %s\n", err.c_str());
    return false;
  }

  uint32_t rev = doc["rev"] | 0;
  JsonObject s = doc["settings"];

  Settings st;
  st.max_a           = s["max_a"] | 16;
  st.def_a           = s["def_a"] | 10;
  st.require_card    = s["require_card"] | true;
  st.schedule_en     = s["schedule_en"] | false;
  hmToPair(s["schedule_start"] | "22:00", &st.sh, &st.sm);
  hmToPair(s["schedule_end"]   | "06:00", &st.eh, &st.em);
  st.days            = s["schedule_days"] | 0x7F;
  st.max_session_min = s["max_session_min"] | 480;
  st.target_kwh10    = (uint16_t)((s["target_kwh"] | 0.0f) * 10.0f + 0.5f);
  const char* cmdStr = s["cmd"] | "";
  st.cmd             = (!strcmp(cmdStr, "start")) ? 1
                     : (!strcmp(cmdStr, "stop"))  ? 2 : 0;
  st.cmd_id          = s["cmd_id"] | 0;

  // --- kartlar ---
  static CardRec buf[MAX_CARDS];
  memset(buf, 0, sizeof(buf));
  int n = 0;

  for (JsonObject c : doc["cards"].as<JsonArray>()) {
    if (n >= MAX_CARDS) break;
    const char* uidHexStr = c["uid"] | "";
    if (strlen(uidHexStr) < 4) continue;

    CardRec& r = buf[n];
    r.len = hexToBytes(uidHexStr, r.uid, 7);
    if (r.len < 2) continue;

    const char* nm = c["name"] | "KART";
    strncpy(r.name, nm, sizeof(r.name) - 1);
    r.admin     = c["admin"] | false;
    r.active    = c["active"] | true;
    r.quota_kwh = c["quota_kwh"] | 0;

    float used  = c["used_kwh"] | 0.0f;
    r.used_kwh10 = (uint16_t)(used * 10.0f + 0.5f);
    r.last_ts   = 0;
    n++;
  }

  store.applyServer(buf, n, st, rev);
  srvRev = rev;

  // --- sarj cihazi kimligi (panelden) ---
  // local_key sadece bu Bearer token'li HTTPS cagrisinda doner; RAM'de kalir.
  const char* tid  = doc["device"]["tuya_id"] | "";
  const char* lkey = doc["device"]["local_key"] | "";
  if (strlen(lkey) == 16) {
    String ip = netChargerIp();
    if (!ip.length()) {
      String found;
      if (netFindCharger(&found, 3000)) ip = found;
    }
    if (ip.length()) charger.configure(ip, tid, lkey);
  } else {
    Serial.println("[sync] panelde local_key yok (Cihaz > local_key girin)");
  }

  Serial.printf("[sync] config uygulandi: rev=%u kart=%d maxA=%u\n",
                rev, n, st.max_a);
  return true;
}

static String clientId(uint32_t id) {
  return String(TUYA_DEVICE_ID) + "-" + String(id);
}

// Tek kaydi gonder. true = panel kabul etti.
static bool postRecord(const OutRec& r) {
  JsonDocument d;
  d["uid"]  = uidHex(r.uid, r.len);
  d["kwh"]  = r.kwh10 / 10.0f;
  d["reason"] = r.reason;
  d["client_id"] = clientId(r.id);
  String st = isoNow(r.ts);
  if (st.length()) d["started_at"] = st;
  String en = isoNow(nowEpoch());
  if (en.length()) d["ended_at"] = en;

  String body;
  serializeJson(d, body);
  return httpReq("POST", "/api/v1/session", body, nullptr);
}

// Kuyrugu bosalt: sirayla gonder, ilk hatada dur (sira korunur).
static void flushOutbox() {
  int guard = 0;
  while (outboxCount() > 0 && guard++ < 8) {
    OutRec* r = outboxAt(0);
    if (!r) break;

    bool ok;
    if (r->kind == 1) {                 // taninmayan kart
      JsonDocument d;
      d["uid"] = uidHex(r->uid, r->len);
      String body;
      serializeJson(d, body);
      ok = httpReq("POST", "/api/v1/unknown", body, nullptr);
    } else {
      ok = postRecord(*r);
    }

    if (!ok) break;
    Serial.printf("[outbox] gonderildi id=%u (kalan %d)\n", r->id, outboxCount() - 1);
    outboxDrop(0);
  }
}

// --------------------------------------------------------------- API
void syncBegin() {
  outboxBegin();
  srvRev = store.configRev();
  started = true;
  Serial.printf("[sync] panel: %s (yerel rev=%u)\n", PANEL_URL, srvRev);
}

bool     syncOnline()  { return lastOk && (millis() - lastOk) < 90000UL; }
uint32_t syncLastOk()  { return lastOk / 1000; }
uint32_t syncRev()     { return srvRev; }
int      syncFails()   { return fails; }
int      syncQueueDepth() { return outboxCount(); }

void syncSession(uint32_t startTs, const uint8_t* uid, uint8_t len,
                 float kwh, uint8_t reason) {
  if (!started) return;

  OutRec r;
  memset(&r, 0, sizeof(r));
  r.id     = outboxNextId();
  r.ts     = startTs ? startTs : nowEpoch();
  r.len    = len > 7 ? 7 : len;
  memcpy(r.uid, uid, r.len);
  r.kwh10  = (uint16_t)(kwh * 10.0f + 0.5f);
  r.reason = reason;
  r.kind   = 0;

  outboxPush(r);
  flushOutbox();                       // internet varsa hemen gonder
}

void syncUnknown(const uint8_t* uid, uint8_t len) {
  if (!started) return;

  OutRec r;
  memset(&r, 0, sizeof(r));
  r.id   = outboxNextId();
  r.ts   = nowEpoch();
  r.len  = len > 7 ? 7 : len;
  memcpy(r.uid, uid, r.len);
  r.kind = 1;

  outboxPush(r);
  flushOutbox();
}

bool syncEnroll(const uint8_t* uid, uint8_t len, const char* name) {
  if (!started) return false;
  JsonDocument d;
  d["uid"]  = uidHex(uid, len);
  d["name"] = name;
  String body;
  serializeJson(d, body);
  bool ok = httpReq("POST", "/api/v1/card", body, nullptr);
  if (ok) Serial.println("[sync] kart panele kaydedildi");
  else    Serial.println("[sync] kart panele YAZILAMADI (yerelde kaldi)");
  return ok;
}

void syncLoop() {
  if (!started) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (!clockValid()) return;          // TLS icin saat sart

  uint32_t now = millis();

  // --- heartbeat ---
  if (now - lastBeat >= BEAT_MS) {
    lastBeat = now;
    if (sendHeartbeat()) { lastOk = now ? now : 1; fails = 0; }
    else                 { fails++; }
  }

  // --- cevrimdisi kuyruk ---
  if (outboxCount() > 0 && now - lastFlush >= 20000UL) {
    lastFlush = now;
    flushOutbox();
  }

  // --- config (rev degistiyse ya da hic cekilmediyse) ---
  static bool once = false;
  if (!once || now - lastCfg >= CFG_MS) {
    lastCfg = now;
    pullConfig();                     // rev farkliysa uygular, ayniysa da yazar
    once = true;
  }
}
