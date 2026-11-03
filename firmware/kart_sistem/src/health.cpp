#include "health.h"
#include "net.h"
#include "config.h"

#include <WiFi.h>
#include <Preferences.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

// ---------------------------------------------------------------------------
//  Neden ayri bir gorev?
//  loop() bloke olursa (TLS, DNS, I2C) ayni gorev icinde nobetci calisamaz.
//  Bu yuzden ayri bir FreeRTOS gorevi loop'un attigi adimi izler.
// ---------------------------------------------------------------------------

static volatile uint32_t beat = 0;
static volatile uint32_t beatSeen = 0;

static uint32_t bootMs = 0;
static uint32_t minHeap = 0xFFFFFFFF;
static int      restarts = 0;
static char     lastReason[28] = "-";
static uint32_t bootCount = 0;

static Preferences prefs;

// DIKKAT: kontrol penceresi, loop'un mesru blok suresinden uzun OLMALI.
// TLS istegi tek basina 8 sn bloke edebiliyor; 5 sn'lik pencere yuzunden
// her yavas istek "kilitlenme" sanilip cihaz resetleniyordu.
#define WD_STALL_MS      180000UL   // 180 sn ilerleme yok -> gercek kilit
#define WD_CHECK_MS      5000UL
#define WIFI_DOWN_LIMIT  600000UL   // 10 dk Wi-Fi yok -> restart
#define HEAP_FLOOR       9000UL

// ------------------------------------------------------------------ gorev
static void healthTask(void* arg) {
  (void)arg;
  uint32_t downSince  = 0;
  uint32_t lastProgress = millis() ? millis() : 1;
  int      heapLow = 0;

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(WD_CHECK_MS));

    uint32_t now = millis();
    uint32_t b = beat;

    // 1) loop ilerliyor mu?
    // Ilerleme VARSA zaman damgasini tazele; YOKSA sayaci bekle.
    if (b != beatSeen) {
      beatSeen = b;
      lastProgress = now ? now : 1;
    } else if (lastProgress && (now - lastProgress) > WD_STALL_MS) {
      Serial.printf("[health] LOOP %lus ilerlemedi -> restart\n",
                    (unsigned long)((now - lastProgress) / 1000));
      snprintf(lastReason, sizeof(lastReason), "loop kilit");
      prefs.putString("lastReason", lastReason);
      prefs.putUInt("boots", prefs.getUInt("boots", 0) + 1);
      delay(120);
      esp_restart();
    }

    // 2) Wi-Fi uzun sure yok
    if (WiFi.status() != WL_CONNECTED) {
      if (!downSince) downSince = now ? now : 1;
      else if (now - downSince > WIFI_DOWN_LIMIT) {
        Serial.println("[health] Wi-Fi 10 dk yok -> restart");
        snprintf(lastReason, sizeof(lastReason), "wifi yok");
        prefs.putString("lastReason", lastReason);
        prefs.putUInt("boots", prefs.getUInt("boots", 0) + 1);
        delay(120);
        esp_restart();
      }
    } else {
      downSince = 0;
    }

    // 3) bellek
    // TLS el sikismasi sirasinda heap gecici olarak 15-20 KB'a duser;
    // bu NORMAL. Tek okumada restart etmek yanlisti -> 3 ardisik okuma.
    uint32_t hp = ESP.getFreeHeap();
    if (hp < minHeap) minHeap = hp;
    if (hp < HEAP_FLOOR) heapLow++; else heapLow = 0;
    if (heapLow >= 3) {
      Serial.printf("[health] heap kritik (%u) -> restart\n", hp);
      snprintf(lastReason, sizeof(lastReason), "heap %u", hp);
      prefs.putString("lastReason", lastReason);
      prefs.putUInt("boots", prefs.getUInt("boots", 0) + 1);
      delay(120);
      esp_restart();
    }
  }
}

// ------------------------------------------------------------------ API
void healthBegin() {
  prefs.begin("evcharge", false);
  bootCount = prefs.getUInt("boots", 0);
  bootMs = millis();
  minHeap = ESP.getFreeHeap();

  const char* r = prefs.getString("lastReason", "-").c_str();
  strncpy(lastReason, r, sizeof(lastReason) - 1);

  xTaskCreatePinnedToCore(healthTask, "health", 3072, nullptr, 1, nullptr, 0);
  Serial.printf("[health] nobetci aktif (restart sayisi=%u, son neden=%s)\n",
                bootCount, lastReason);
}

void healthFeed() { beat++; }

void healthLoop() {
  static uint32_t lastLog = 0;
  uint32_t now = millis();
  if (now - lastLog < 60000) return;
  lastLog = now;

  Serial.printf("[health] up=%lus heap=%u min=%u wifi=%s rssi=%d ap=%d "
                "sync_fail=%d q=%d chr_fail=%d\n",
                (unsigned long)(now / 1000), ESP.getFreeHeap(),
                (unsigned)minHeap, netUplinkOk() ? "OK" : "YOK",
                netRssi(), netApClients(), 0, 0, 0);
}

uint32_t healthUptimeSec() { return (millis() - bootMs) / 1000; }
uint32_t healthBootCount() { return bootCount; }
uint32_t healthMinHeap()   { return minHeap; }
int      healthRestarts()  { return restarts; }
const char* healthLastReason() { return lastReason; }
