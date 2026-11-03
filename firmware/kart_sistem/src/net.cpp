#include "net.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <lwip/ip_addr.h>

static bool     naptOk        = false;
static bool     apUp          = false;
static bool     dnsSet        = false;
static uint32_t lastReconnect = 0;
static uint32_t lastApCheck   = 0;
static uint32_t lastDnsCheck  = 0;
static bool     internetOk    = false;
static String   chargerIp     = "";
static uint32_t staAttempts   = 0;
static uint32_t reconnects    = 0;
static uint32_t downSince     = 0;
static bool     wasUp         = false;

// ---------------------------------------------------------------- DNS
// KRITIK: AP'nin DHCP'si istemcilere 192.168.4.1'i DNS olarak verir, ama
// orada DNS sunucusu YOKTUR. NAPT yalnizca paket yonlendirir; DNS'i cevaplamaz.
// Bu yuzden DHCP'nin verdigi DNS'i dogrudan upstream sunuculara ceviriyoruz.
// Yoksa cihaz "bagli ama internet yok" der.
static void ip4(esp_netif_dns_info_t* d, uint8_t a, uint8_t b, uint8_t c, uint8_t e) {
  memset(d, 0, sizeof(*d));
  d->ip.type = ESP_IPADDR_TYPE_V4;
  d->ip.u_addr.ip4.addr = ((uint32_t)a) | ((uint32_t)b << 8) |
                          ((uint32_t)c << 16) | ((uint32_t)e << 24);
}

static void apSetDns() {
  esp_netif_t* ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
  if (!ap) { Serial.println("[net] AP netif bulunamadi"); return; }

  // ONEMLI: ESP_NETIF_DOMAIN_NAME_SERVER bir DNS ADRESI degil, "DNS sun"
  // bayragi (dhcps_offer_t = uint8) bekler. Adres esp_netif_set_dns_info()
  // ile AP netif'ine yazilir. Ikisi de DHCP sunucusu DURMUŞKEN cagrilmali.
  esp_netif_dns_info_t d;
  bool haveUp = false;

  esp_netif_t* sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (sta) {
    if (esp_netif_get_dns_info(sta, ESP_NETIF_DNS_MAIN, &d) == ESP_OK &&
        d.ip.u_addr.ip4.addr != 0) {
      haveUp = true;
    }
  }
  if (!haveUp) ip4(&d, 1, 1, 1, 1);

  esp_netif_dhcps_stop(ap);

  uint8_t offer = 1;                                   // OFFER_DNS
  esp_err_t e1 = esp_netif_dhcps_option(ap, ESP_NETIF_OP_SET,
                                        ESP_NETIF_DOMAIN_NAME_SERVER,
                                        &offer, sizeof(offer));
  esp_err_t e2 = esp_netif_set_dns_info(ap, ESP_NETIF_DNS_MAIN, &d);

  if (e2 != ESP_OK && haveUp) {                        // upstream reddedildi
    ip4(&d, 1, 1, 1, 1);
    e2 = esp_netif_set_dns_info(ap, ESP_NETIF_DNS_MAIN, &d);
    haveUp = false;
  }

  esp_netif_dhcps_start(ap);

  dnsSet = (e1 == ESP_OK && e2 == ESP_OK);
  Serial.printf("[net] DHCP DNS -> %s (%s) opt=0x%x dns=0x%x\n",
                dnsSet ? "ayarlandi" : "HATA",
                haveUp ? "upstream" : "1.1.1.1",
                (unsigned)e1, (unsigned)e2);
}

// ---------------------------------------------------------------- baslat
void netBegin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);

  // ---------------- 1) STA: once internet ----------------
  WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASS);
  Serial.printf("[net] STA %s baglaniyor", WIFI_STA_SSID);
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
    delay(400);
    Serial.print(".");
  }
  bool sta = (WiFi.status() == WL_CONNECTED);
  Serial.println(sta ? " OK" : " BASARISIZ");
  if (sta) {
    Serial.printf("[net] STA ip=%s  rssi=%d\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  }

  // ---------------- 2) AP ----------------
  apUp = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, WIFI_AP_CHANNEL, 0, 4);
  Serial.printf("[net] AP %s -> %s  ip=%s\n",
                WIFI_AP_SSID, apUp ? "ACIK" : "HATA",
                WiFi.softAPIP().toString().c_str());

  delay(300);

  // ---------------- 3) NAPT (STA IP'si hazir olmali) ----------------
  if (apUp) {
    naptOk = WiFi.AP.enableNAPT(true);
    Serial.printf("[net] NAPT -> %s\n", naptOk ? "AKTIF" : "HATA");
    if (!naptOk) {
      Serial.println("[net] HATA: platform 8.0.0+ gerekli (CONFIG_LWIP_IPV4_NAPT)");
    }
  }

  // ---------------- 4) DHCP DNS ----------------
  if (apUp) apSetDns();
}

// ---------------------------------------------------------------- dongu
void netLoop() {
  uint32_t now = millis();

  // ---------------- STA kurtarma ----------------
  bool up = (WiFi.status() == WL_CONNECTED);

  if (up && !wasUp) {
    // yeniden baglandi: NAPT ve DNS'i tazele
    wasUp = true;
    downSince = 0;
    if (reconnects || staAttempts) {
      Serial.printf("[net] STA geri geldi (ip=%s)\n", WiFi.localIP().toString().c_str());
    }
    if (apUp) {
      naptOk = WiFi.AP.enableNAPT(true);
      dnsSet = false;                 // bir sonraki turda yeniden uygulanir
    }
    internetOk = false;
  } else if (!up && wasUp) {
    wasUp = false;
    downSince = now ? now : 1;
    Serial.println("[net] STA koptu");
  }

  if (!up) {
    // ussel geri cekilme: 5,10,20,30,60 sn
    uint32_t waitMs = 5000UL << (staAttempts > 3 ? 3 : staAttempts);
    if (waitMs > 60000UL) waitMs = 60000UL;

    if (now - lastReconnect > waitMs) {
      lastReconnect = now;
      staAttempts++;
      reconnects++;
      Serial.printf("[net] STA yeniden baglaniyor (deneme %lu)\n",
                    (unsigned long)staAttempts);
      if (staAttempts < 3) {
        WiFi.reconnect();                        // hafif yol
      } else {
        WiFi.disconnect(true, false);            // ag ayarlarini koru
        WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASS);
        if (staAttempts % 5 == 0) {
          // inatci durum: radyo modunu tazele
          WiFi.mode(WIFI_OFF);
          delay(200);
          WiFi.mode(WIFI_AP_STA);
          WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, WIFI_AP_CHANNEL, 0, 4);
          WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASS);
          apUp = true;
        }
      }
    }
  } else {
    staAttempts = 0;
  }

  // AP dustuyse kaldir + NAPT ve DNS'i tekrar ac
  if (apUp && now - lastApCheck > 60000) {
    lastApCheck = now;
    if (!naptOk) {
      naptOk = WiFi.AP.enableNAPT(true);
      if (naptOk) Serial.println("[net] NAPT yeniden acildi");
    }
    dnsSet = false;                       // periyodik olarak yeniden uygula
  }

  // DNS'i 60 sn'de bir tazele (STA DNS'i sonradan ogrenilebilir)
  if (apUp && !dnsSet && now - lastDnsCheck > 10000) {
    lastDnsCheck = now;
    apSetDns();
  }

  // internet gercekten calisiyor mu? (60 sn'de bir DNS cozumu)
  static uint32_t lastInet = 0;
  if (now - lastInet > 60000) {
    lastInet = now;
    IPAddress ip;
    internetOk = netUplinkOk() && WiFi.hostByName("one.one.one.one", ip);
    if (!internetOk) Serial.println("[net] internet DNS cozulemedi");
  }
}

bool   netUplinkOk()   { return WiFi.status() == WL_CONNECTED; }
int    netApClients()  { return WiFi.softAPgetStationNum(); }
bool   netNaptOk()     { return naptOk; }
String netStaIp()      { return WiFi.localIP().toString(); }
String netApIp()       { return WiFi.softAPIP().toString(); }
int    netRssi()       { return netUplinkOk() ? WiFi.RSSI() : 0; }
bool   netHasInternet(){ return internetOk; }
uint32_t netReconnects(){ return reconnects; }
uint32_t netDownSec()   { return netUplinkOk() ? 0 : (downSince ? (millis() - downSince) / 1000 : 0); }
String netChargerIp()  { return chargerIp; }

// ---------------------------------------------------------------- kesif
// Tuya cihazi UDP 6666/6667'de yayin dinler. Broadcast atip cevabindaki ip'yi al.
bool netFindCharger(String* ipOut, uint16_t timeoutMs) {
  if (!apUp) return false;

  WiFiUDP udp;
  IPAddress bcast = WiFi.softAPIP();
  bcast[3] = 255;                       // 192.168.4.255

  char payload[128];
  uint32_t t = millis() / 1000;
  snprintf(payload, sizeof(payload),
           "{\"gwId\":\"%s\",\"devId\":\"%s\",\"uid\":\"%s\",\"t\":\"%u\"}",
           TUYA_DEVICE_ID, TUYA_DEVICE_ID, TUYA_DEVICE_ID, (unsigned)t);

  bool found = false;
  for (uint16_t port : {(uint16_t)6666, (uint16_t)6667}) {
    if (!udp.begin(port)) continue;
    udp.beginPacket(bcast, port);
    udp.write((const uint8_t*)payload, strlen(payload));
    udp.endPacket();
    delay(80);
    udp.stop();
  }

  // cevap 6667'de gelir
  if (!udp.begin(6667)) return false;
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    int sz = udp.parsePacket();
    if (sz > 0) {
      char buf[512];
      int n = udp.read(buf, sizeof(buf) - 1);
      if (n > 0) {
        buf[n] = 0;
        Serial.printf("[net] cevap: %s\n", buf);
        const char* k = strstr(buf, "\"ip\"");
        if (k) {
          const char* q = strchr(k, ':');
          if (q) {
            const char* a = strchr(q, '"');
            if (a) {
              const char* b = strchr(a + 1, '"');
              if (b && b > a + 1) {
                *ipOut = String(a + 1).substring(0, b - a - 1);
                found = true;
                break;
              }
            }
          }
        }
        *ipOut = udp.remoteIP().toString();
        found = true;
        break;
      }
    }
    delay(20);
  }
  udp.stop();

  if (found) chargerIp = *ipOut;
  return found;
}
