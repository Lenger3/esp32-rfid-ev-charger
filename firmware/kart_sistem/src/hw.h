#pragma once
#include <Arduino.h>

#include "btn.h"

// ============================================================ butonlar
void     hwBegin();                  // btnBegin() + RFID/saat kurulumu

// ============================================================ RFID
struct CardId {
  uint8_t uid[7];
  uint8_t len;
  bool    ok = false;
};

bool     rfidPoll(CardId* out);      // yeni kart varsa true
String   uidToStr(const CardId& c);  // "D3:BF:4E:07"
String   uidToHex(const CardId& c);  // "D3BF4E07"

// ============================================================ saat
void     clockBegin();
bool     clockValid();
uint32_t nowEpoch();
String   timeStr();                  // "20:41"
String   dateStr();                  // "28.10"
String   stampStr(uint32_t ts);      // "28.10 20:41"
bool     clockInWindow(uint8_t sh, uint8_t sm, uint8_t eh, uint8_t em, uint8_t days);
bool     rfidBegin();
