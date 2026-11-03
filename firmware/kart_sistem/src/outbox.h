#pragma once
#include <Arduino.h>

// ===========================================================================
//  Cevrimdisi kuyruk (outbox)
//
//  Internet yokken biten oturumlar kaybolmaz: NVS'ye yazilir, baglanti
//  gelince sirayla gonderilir. Her kaydin tekil `id`'si vardir; panel
//  ayni id'yi iki kez yazmaz (mukerrer koruma).
// ===========================================================================

#define OUTBOX_MAX  32

struct OutRec {
  uint32_t id;          // tekil sira numarasi (NVS'de artarak)
  uint32_t ts;          // oturum baslangici (epoch sn)
  uint8_t  uid[7];
  uint8_t  len;
  uint16_t kwh10;
  uint8_t  reason;
  uint8_t  kind;        // 0 = oturum, 1 = taninmayan kart
};

void     outboxBegin();
int      outboxCount();
bool     outboxPush(const OutRec& r);       // dolu ise en eskiyi atar
OutRec*  outboxAt(int i);
void     outboxDrop(int i);                 // gonderildi -> sil
void     outboxSave();
uint32_t outboxNextId();
