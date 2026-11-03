#include "hw.h"
#include "pins.h"
#include <Wire.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <time.h>
#include "store.h"
#include "btn.h"

// buton kurulumu btn.cpp'de; hwBegin geriye uyumluluk icin duruyor
void hwBegin() { btnBegin(); }

// ============================================================ RFID
static MFRC522 rfid(PIN_RFID_CS, PIN_RFID_RST);
static uint32_t lastSeen = 0;
static uint8_t  lastUid[7] = {0};
static uint8_t  lastLen    = 0;

bool rfidBegin() {
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_RFID_CS);
  rfid.PCD_Init();
  rfid.PCD_SetRegisterBitMask(MFRC522::TxControlReg, 0x03);
  byte ver = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.printf("[rfid] version=0x%02X\n", ver);
  return ver != 0x00 && ver != 0xFF;
}

bool rfidPoll(CardId* out) {
  if (!rfid.PICC_IsNewCardPresent()) return false;
  if (!rfid.PICC_ReadCardSerial())    return false;

  uint32_t now = millis();
  // ayni kart 1.5 sn icinde tekrar okunmasin
  if (lastLen == rfid.uid.size && memcmp(lastUid, rfid.uid.uidByte, lastLen) == 0
      && (now - lastSeen) < 1500) {
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return false;
  }

  int n = rfid.uid.size > 7 ? 7 : rfid.uid.size;
  memcpy(out->uid, rfid.uid.uidByte, n);
  out->len = n;
  out->ok  = true;
  memcpy(lastUid, out->uid, n);
  lastLen   = n;
  lastSeen  = now;

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  return true;
}

String uidToStr(const CardId& c) {
  char b[24];
  int p = 0;
  for (int i = 0; i < c.len && p < 20; i++) p += snprintf(b + p, sizeof(b) - p, i ? ":%02X" : "%02X", c.uid[i]);
  return String(b);
}

String uidToHex(const CardId& c) {
  char b[16];
  int p = 0;
  for (int i = 0; i < c.len && p < 13; i++) p += snprintf(b + p, sizeof(b) - p, "%02X", c.uid[i]);
  return String(b);
}

// ============================================================ saat
static bool timeOk = false;
static uint32_t lastCheck = 0;

void clockBegin() {
  // WiFi baglantisini net.cpp kurar — burada sadece NTP baslatilir
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ntp] WiFi yok, saat atlandi");
    return;
  }
  configTime(3 * 3600, 0, "tr.pool.ntp.org", "pool.ntp.org");
  Serial.print("[ntp] bekleniyor");
  for (int i = 0; i < 20; i++) {
    if (time(nullptr) > 1700000000) { timeOk = true; break; }
    delay(250);
    Serial.print(".");
  }
  Serial.println(timeOk ? " OK" : " BASARISIZ");
}

bool clockValid() { return timeOk && time(nullptr) > 1700000000; }

uint32_t nowEpoch() { return (uint32_t)time(nullptr); }

String timeStr() {
  if (!clockValid()) return "--:--";
  struct tm t;
  time_t tt = time(nullptr);
  localtime_r(&tt, &t);
  char b[8];
  snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min);
  return String(b);
}

String dateStr() {
  if (!clockValid()) return "--.--";
  struct tm t;
  time_t tt = time(nullptr);
  localtime_r(&tt, &t);
  char b[8];
  snprintf(b, sizeof(b), "%02d.%02d", t.tm_mday, t.tm_mon + 1);
  return String(b);
}

String stampStr(uint32_t ts) {
  if (ts < 1700000000) return "--.-- --:--";
  struct tm t;
  time_t tt = ts;
  localtime_r(&tt, &t);
  char b[16];
  snprintf(b, sizeof(b), "%02d.%02d %02d:%02d", t.tm_mday, t.tm_mon + 1, t.tm_hour, t.tm_min);
  return String(b);
}

bool clockInWindow(uint8_t sh, uint8_t sm, uint8_t eh, uint8_t em, uint8_t days) {
  if (!clockValid()) return true;          // saat yoksa engellemeyelim
  time_t tt = time(nullptr);
  struct tm t;
  localtime_r(&tt, &t);

  uint8_t dow = (t.tm_wday + 6) % 7;      // Pzt=0 .. Paz=6
  if (!(days & (1 << dow))) return false;

  int nowMin = t.tm_hour * 60 + t.tm_min;
  int s = sh * 60 + sm;
  int e = eh * 60 + em;
  if (s <= e) return nowMin >= s && nowMin < e;
  return nowMin >= s || nowMin < e;        // gece yarisini asan
}
