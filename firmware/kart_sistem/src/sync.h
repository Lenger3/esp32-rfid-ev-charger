#pragma once
#include <Arduino.h>

// ===========================================================================
//  ESP32  <->  VPS PANEL senkronu
//
//  Sadece OUTBOUND HTTPS. Router'da port acilmaz.
//  VPS dususse son NVS listesiyle fail-closed calismaya devam eder.
//
//    POST /api/v1/heartbeat   30 sn'de bir
//    GET  /api/v1/config      rev degisince  (kart listesi + ayarlar)
//    POST /api/v1/session     oturum bitince
//    POST /api/v1/unknown     taninmayan kart
// ===========================================================================

void     syncBegin();
void     syncLoop();
bool     syncOnline();          // son basarili istek < 90 sn
uint32_t syncLastOk();          // epoch sn
uint32_t syncRev();             // paneldeki son gorulen rev
int      syncFails();
int      syncQueueDepth();     // gonderilmeyi bekleyen kayit sayisi

// oturum bitti -> panele gonder
void syncSession(uint32_t startTs, const uint8_t* uid, uint8_t len,
                 float kwh, uint8_t reason);
void syncUnknown(const uint8_t* uid, uint8_t len);

// cihazda okutulan yeni karti panele yaz (yoksa sonraki config cekimi siler)
bool syncEnroll(const uint8_t* uid, uint8_t len, const char* name);
