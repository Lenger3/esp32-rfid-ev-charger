// ===========================================================================
//  HOST SIMULATOR — firmware'in GERCEK ui.cpp dosyasini Mac'te derler,
//  sanal bir SSD1309'a cizer ve ekran goruntulerini diske yazar.
//
//  Bu, cihazdaki piksel ciktisinin aynisidir (ayni font, ayni yerlesim).
// ===========================================================================
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "u8g2.h"
#include "U8g2lib.h"

#include "WiFi.h"
#include "store.h"
#include "charger.h"
#include "hw.h"
#include "ui.h"

// net stub (simulasyonda ag yok)
bool netUplinkOk() { return true; }
bool netHasInternet() { return true; }
int  netRssi() { return -55; }
String netChargerIp() { return String("192.168.4.2"); }
bool syncOnline() { return true; }
int  syncFails() { return 0; }
uint32_t syncRev() { return 15; }
int  syncQueueDepth() { return 0; }
uint32_t netReconnects() { return 2; }
uint32_t netDownSec() { return 0; }

// health.cpp yerine
uint32_t healthUptimeSec() { return 3 * 86400 + 7 * 3600; }
uint32_t healthBootCount() { return 1; }
uint32_t healthMinHeap() { return 120000; }
int      healthRestarts() { return 0; }
const char* healthLastReason() { return "wifi yok"; }
bool netNaptOk()   { return true; }
int  netApClients(){ return 1; }
String netStaIp()  { return String("192.168.1.42"); }
String netApIp()   { return String("192.168.4.1"); }

SerialShim Serial;
uint32_t gFakeMillis = 500;
uint8_t  gPinState[64];
uint8_t  gPinMode[64];
WiFiShim WiFi;

// ------------------------------------------------------------- sanal panel
static uint8_t gddram[8][128];
static int curPage = 0, curCol = 0;
static int colStart = 0, colEnd = 127;
static int pageStart = 0, pageEnd = 7;
static int addrMode = 2;          // 0=yatay 1=dikey 2=sayfa (SSD1309 varsayilan)
static int cmdPhase = -1;         // 0x21/0x22 arguman sayaci
static int cmdArgs[2], cmdN = 0;

static void clearGddram() {
  memset(gddram, 0, sizeof(gddram));
  curPage = 0; curCol = 0;
  colStart = 0; colEnd = 127;
  pageStart = 0; pageEnd = 7;
  addrMode = 2; cmdPhase = -1; cmdN = 0;
}

static void advancePtr() {
  switch (addrMode) {
    case 0:                                   // yatay
      curCol++;
      if (curCol > colEnd) { curCol = colStart; curPage++; if (curPage > pageEnd) curPage = pageStart; }
      break;
    case 1:                                   // dikey
      curPage++;
      if (curPage > pageEnd) { curPage = pageStart; curCol++; if (curCol > colEnd) curCol = colStart; }
      break;
    default:                                  // sayfa
      curCol++;
      if (curCol > 127) curCol = 0;
      break;
  }
}

static uint8_t byteCb(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
  (void)u8x8;
  switch (msg) {
    case U8X8_MSG_BYTE_SEND: {
      uint8_t* p = (uint8_t*)arg_ptr;
      int i = 0;
      bool isData = false;
      if (arg_int > 0 && (p[0] == 0x00 || p[0] == 0x40)) {
        isData = (p[0] == 0x40);
        i = 1;
      }
      for (; i < arg_int; i++) {
        uint8_t b = p[i];
        if (isData) {
          gddram[curPage & 7][curCol & 127] = b;
          advancePtr();
        } else {
          if (cmdPhase >= 0) {                 // 0x21 / 0x22 argumanlari
            cmdArgs[cmdN++] = b;
            if (cmdPhase == 0x20 && cmdN == 1) { addrMode = cmdArgs[0] & 0x03; cmdPhase = -1; cmdN = 0; continue; }
            if (cmdN == 2) {
              if (cmdPhase == 0x21) { colStart = cmdArgs[0]; colEnd = cmdArgs[1]; curCol = colStart; }
              else                  { pageStart = cmdArgs[0] & 7; pageEnd = cmdArgs[1] & 7; curPage = pageStart; }
              cmdPhase = -1; cmdN = 0;
            }
            continue;
          }
          if      (b <= 0x0F) curCol = (curCol & 0xF0) | (b & 0x0F);
          else if (b <= 0x1F) curCol = (curCol & 0x0F) | ((b & 0x0F) << 4);
          else if (b >= 0xB0 && b <= 0xB7) curPage = b & 0x07;
          else if (b == 0x21) { cmdPhase = 0x21; cmdN = 0; }
          else if (b == 0x22) { cmdPhase = 0x22; cmdN = 0; }
          else if (b == 0x20) { cmdPhase = 0x20; cmdN = 0; }
        }
      }
      break;
    }
    default:
      break;
  }
  return 1;
}

static uint8_t gpioCb(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
  (void)u8x8; (void)arg_int; (void)arg_ptr;
  if (msg == U8X8_MSG_GPIO_AND_DELAY_INIT) return 1;
  if (msg == U8X8_MSG_DELAY_MILLI) return 1;
  return 1;
}

static U8G2* makeDisplay() {
  U8G2* g = new U8G2();
  u8g2_Setup_ssd1309_i2c_128x64_noname0_f(g->getU8g2(), U8G2_R0, byteCb, gpioCb);
  g->begin();
  g->setContrast(200);
  return g;
}

static U8G2* gDisp = nullptr;

static void saveRaw(const char* name) {
  char path[256];
  snprintf(path, sizeof(path), "out/%s.raw", name);
  FILE* f = fopen(path, "wb");
  if (!f) { printf("yazilamadi: %s\n", path); return; }
  // u8g2'nin kendi framebuffer'i — sanal panel emulasyonuna gerek yok
  uint8_t* buf = u8g2_GetBufferPtr(gDisp->getU8g2());
  fwrite(buf, 1, 1024, f);
  fclose(f);
  printf("  %-14s -> %s\n", name, path);
}

// ------------------------------------------------------------- sahte veri
static CardRec fakeCards[3];
static LogRec  fakeLogs[6];
static Settings fakeSet;

// Store metodlari (sahte)
Store store;

void Store::begin() {}
int Store::cardCount() { return 3; }
CardRec* Store::card(int i) { return (i >= 0 && i < 3) ? &fakeCards[i] : nullptr; }
int Store::findCard(const uint8_t*, uint8_t) { return 0; }
Settings& Store::settings() { return fakeSet; }
int Store::logCount() { return 6; }
LogRec* Store::log(int i) { return (i >= 0 && i < 6) ? &fakeLogs[i] : nullptr; }

// Charger
Charger charger;
bool  Charger::begin(bool) { return true; }
void  Charger::configure(const String&, const char*, const char*) {}
void  Charger::poll() {}
bool  Charger::enable(bool) { return true; }
bool  Charger::setCurrent(uint8_t) { return true; }
ChargerState Charger::state() { return CH_CHARGING; }
bool  Charger::online() { return true; }
bool  Charger::carConnected() { return true; }
uint8_t Charger::currentA() { return 16; }
float Charger::voltage() { return 232.0f; }
float Charger::powerKw() { return 3.68f; }
float Charger::tempC() { return 31.0f; }
float Charger::sessionKwh() { return 6.42f; }
uint32_t Charger::sessionSec() { return 37 * 60 + 12; }
float  Charger::totalKwh() { return 148.66f; }
const char* Charger::workStateText() { return "charger_charging"; }
void  Charger::applyPush(const String&) {}
bool  Charger::parkLock() { return true; }
bool  Charger::parkLocked() { return false; }
void  Charger::releaseParkLock() {}
void  Charger::resetSession() {}

// hw.cpp yerine
int hwPressedPin() { return 33; }        // ekran goruntusu icin DOWN basili
int btnPin(Btn b) { return (int)b; }
bool clockValid() { return true; }
uint32_t nowEpoch() { return 1793000000; }
String timeStr() { return String("20:41"); }
String dateStr() { return String("28.10 Sal"); }
String stampStr(uint32_t) { return String("28.10 20:41"); }
bool clockInWindow(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t) { return true; }

// ------------------------------------------------------------- main
int main() {
  memset(fakeCards, 0, sizeof(fakeCards));

  strcpy(fakeCards[0].name, "AYSE");
  fakeCards[0].len = 4; fakeCards[0].uid[0] = 0xD3;
  fakeCards[0].quota_kwh = 100; fakeCards[0].used_kwh10 = 421;
  fakeCards[0].active = true;  fakeCards[0].last_ts = 1792990000;

  strcpy(fakeCards[1].name, "AYSE");
  fakeCards[1].len = 4; fakeCards[1].quota_kwh = 60; fakeCards[1].used_kwh10 = 128;
  fakeCards[1].active = true;

  strcpy(fakeCards[2].name, "MISAFIR");
  fakeCards[2].len = 4; fakeCards[2].quota_kwh = 0; fakeCards[2].used_kwh10 = 55;
  fakeCards[2].active = true;

  uint32_t ts[6] = {1792999000, 1792991000, 1792980000, 1792970000, 1792960000, 1792950000};
  uint16_t kwh[6] = {64, 128, 42, 210, 75, 33};
  for (int i = 0; i < 6; i++) {
    memset(&fakeLogs[i], 0, sizeof(LogRec));
    fakeLogs[i].ts = ts[i];
    fakeLogs[i].kwh10 = kwh[i];
    fakeLogs[i].len = 4;
    fakeLogs[i].reason = 0;
  }

  fakeSet.max_a = 16; fakeSet.def_a = 10;
  fakeSet.schedule_en = true;
  fakeSet.sh = 22; fakeSet.sm = 0; fakeSet.eh = 6; fakeSet.em = 0;
  fakeSet.days = 0x7F; fakeSet.max_session_min = 480; fakeSet.require_card = true;

  U8G2* g = makeDisplay();
  gDisp = g;
  uiAttach(g);

  struct Item { const char* name; void (*fn)(); };
  clearGddram();

  // 1 karsilama
  clearGddram();
  uiWelcome(timeStr(), dateStr(), true, 3);
  g->sendBuffer();
  saveRaw("01_karsilama");

  // 1b ilk kurulum (hic kart yok)
  clearGddram();
  uiWelcome(timeStr(), dateStr(), true, 0);
  g->sendBuffer();
  saveRaw("01b_ilk_kurulum");

  // 2 kullanici ana
  clearGddram();
  uiUserHome(&fakeCards[0], 42.1f, 6.4f);
  g->sendBuffer();
  saveRaw("02_kullanici");

  // 3 cihaz
  clearGddram();
  uiUserDevice(&fakeCards[0]);
  g->sendBuffer();
  saveRaw("03_cihaz");

  // 4 sarj
  clearGddram();
  uiCharging(&fakeCards[0]);
  g->sendBuffer();
  saveRaw("04_sarj");

  // 5 admin menu
  const char* items[] = {"Kullanicilar", "Kayitlar", "Sarj Ayarlari", "Zamanlama", "Cihaz Bilgisi", "Cikis"};
  const char* items7[] = {"Kullanicilar", "Kayitlar", "Sarj Ayarlari", "Zamanlama",
                          "Buton Testi", "Cihaz Bilgisi", "Cikis"};
  clearGddram();
  uiMenu("YONETICI", items, 6, 0, 0);
  g->sendBuffer();
  saveRaw("05_admin_menu");

  // 6 kullanicilar
  clearGddram();
  uiAdminUsers(0, 0);
  g->sendBuffer();
  saveRaw("06_kullanicilar");

  // 7 kayitlar
  clearGddram();
  uiLogs(0, 0);
  g->sendBuffer();
  saveRaw("07_kayitlar");

  // 8 ayarlar
  clearGddram();
  uiSettings(0, fakeSet, false);
  g->sendBuffer();
  saveRaw("08_ayarlar");

  // 9 zamanlama
  clearGddram();
  uiSchedule(fakeSet, 0, false);
  g->sendBuffer();
  saveRaw("09_zamanlama");

  // 10 pin
  clearGddram();
  uiPinPad("Sifre girin", 4, 2, 7);
  g->sendBuffer();
  saveRaw("10_pin");

  // 11 taninmayan kart
  clearGddram();
  uiAccessDenied(String("A7:51:2B:64"));
  g->sendBuffer();
  saveRaw("11_red");

  // 12 cihaz bilgisi
  clearGddram();
  uiDeviceInfo(fakeSet);
  g->sendBuffer();
  saveRaw("12_cihazbilgi");

  // 13 buton testi
  clearGddram();
  uiButtonTest(33);
  g->sendBuffer();
  saveRaw("13_butontest");

  // 14 admin menu (7 oge, secili olan buton testi)
  clearGddram();
  uiMenu("YONETICI", items7, 7, 4, 2);
  g->sendBuffer();
  saveRaw("14_menu_kaydirma");

  // 15 onay ekrani
  clearGddram();
  uiConfirm("Kart silinsin mi?", "AYSE");
  g->sendBuffer();
  saveRaw("15_onay");

  // 16 admin karti -> kullanici ekrani (sarj edebilir, EVET=menu)
  fakeCards[0].admin = true;
  clearGddram();
  uiUserHome(&fakeCards[0], 42.1f, 6.4f);
  g->sendBuffer();
  saveRaw("16_admin_kullanici");

  // 17 normal kart ayni ekran (karsilastirma)
  fakeCards[0].admin = false;
  clearGddram();
  uiUserHome(&fakeCards[0], 42.1f, 6.4f);
  g->sendBuffer();
  saveRaw("17_kullanici_kart");

  printf("toplam 17 ekran yazildi\n");
  return 0;
}
