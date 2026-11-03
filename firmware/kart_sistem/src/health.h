#pragma once
#include <Arduino.h>

// ===========================================================================
//  Saglik denetimi / nobetci (watchdog)
//
//  Cihaz 7/24 acik kalacagi icin üc sey garanti edilir:
//    1. loop() kilitlenirse cihaz kendini yeniden baslatir (yazilim nobetcisi)
//    2. Wi-Fi uzun sure gelmezse (10 dk) yeniden baslatilir
//    3. Bellek tukenirse (heap < 15 KB) yeniden baslatilir
//
//  Ayrica panel/ekran icin sayaclar tutar.
// ===========================================================================

void     healthBegin();          // loop'tan once bir kez
void     healthFeed();           // her loop turunda cagrilmali
void     healthLoop();           // 1 sn'de bir: kontroller + seri ozet
uint32_t healthUptimeSec();
uint32_t healthBootCount();      // kac kez yeniden basladi (NVS)
uint32_t healthMinHeap();
int      healthRestarts();       // bu oturumdaki otomatik restart sayaci
const char* healthLastReason();  // son restart nedeni
