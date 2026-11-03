/*
 * NAPT (NAT) DESTEK TESTI
 * ESP32 hem router'a baglanir (STA) hem kendi AP'sini acar, aradan paket gecirir.
 *
 * Beklenen seri cikti:
 *   [STA] IP 192.168.1.x
 *   [AP ] SSID=EVCHARGE-SETUP IP 192.168.4.1
 *   [NAPT] sonuc = 0 (ESP_OK)     <-- 0 DEGILSE desteklenmiyor
 */
#include <WiFi.h>
#include <esp_err.h>

const char* STA_SSID = "ROUTER_SSID";
const char* STA_PASS = "ROUTER_PASS";
const char* AP_SSID  = "EVCHARGE-SETUP";
const char* AP_PASS  = "change-me-8chars";

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== NAPT TESTI ===");

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(STA_SSID, STA_PASS);
  Serial.print("[STA] baglaniyor");
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) { delay(500); Serial.print("."); }
  Serial.println(WiFi.status() == WL_CONNECTED ? " OK" : " BASARISIZ");
  if (WiFi.status() == WL_CONNECTED) Serial.printf("[STA] IP %s\n", WiFi.localIP().toString().c_str());

  bool ok = WiFi.softAP(AP_SSID, AP_PASS, 6, 0, 4);
  Serial.printf("[AP ] %s  IP %s  (softAP=%d)\n",
                AP_SSID, WiFi.softAPIP().toString().c_str(), ok);

  delay(300);
  bool napt = WiFi.AP.enableNAPT(true);
  Serial.printf("[NAPT] enableNAPT -> %s\n", napt ? "OK" : "HATA");
  Serial.printf("[NAPT] mac_eth %s\n", WiFi.macAddress().c_str());
  Serial.println("cihazi EVCHARGE-SETUP agina bagla, 192.168.4.2 almali");
  Serial.println("internet testi: cihazdan ping 8.8.8.8");
}

void loop() {
  static uint32_t t = 0;
  if (millis() - t > 5000) {
    t = millis();
    Serial.printf("[durum] STA=%d  AP_istemci=%d  heap=%u\n",
                  WiFi.status(), WiFi.softAPgetStationNum(), ESP.getFreeHeap());
  }
  delay(100);
}
