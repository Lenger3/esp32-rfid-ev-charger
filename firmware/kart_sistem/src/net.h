#pragma once
#include <Arduino.h>

// ===========================================================================
//  ESP32 = AP + STA + NAPT
//
//      Router  <--STA--  ESP32  --AP-->  Sarj cihazi
//                          |
//                     NAPT (NAT) ile cihaz
//                     ESP32 uzerinden internete cikar
//
//  GEREKSINIM: arduino-esp32 3.x (platform espressif32@8.0.0)
//
//  SIRA ONEMLI:
//    1. STA baglanir (IP alir)
//    2. AP acilir
//    3. NAPT acilir          <- STA IP'si olmadan calismaz
//    4. DHCP DNS upstream'e ayarlanir <- yoksa cihaz "internet yok" der
// ===========================================================================

void     netBegin();
void     netLoop();                    // her loop'ta

bool     netUplinkOk();                // router baglantisi (STA)
int      netApClients();               // AP'ye bagli cihaz sayisi
bool     netNaptOk();                  // NAT aktif mi
String   netStaIp();
String   netApIp();
int      netRssi();                    // dBm, 0 = bagli degil
bool     netHasInternet();
uint32_t netReconnects();             // STA yeniden baglanma sayisi
uint32_t netDownSec();                // su anki kesinti suresi (0 = bagli)             // DNS/NTP ile dogrulanmis internet

// Tuya cihazi AP aginda bul (UDP 6666/6667 broadcast)
bool     netFindCharger(String* ipOut, uint16_t timeoutMs = 4000);
String   netChargerIp();
