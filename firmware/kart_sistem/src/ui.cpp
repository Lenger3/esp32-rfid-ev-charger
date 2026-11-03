#include "ui.h"
#include "pins.h"
#ifndef HOST_SIM
#include <Wire.h>
#endif
#include "hw.h"
#include "net.h"
#include "sync.h"
#include "health.h"
#include <WiFi.h>
#include <stdio.h>

bool uiOk = false;

#ifdef HOST_SIM
U8G2* oled = nullptr;
void uiAttach(U8G2* d) { oled = d; }
#else
static U8G2_SSD1309_128X64_NONAME0_F_HW_I2C oledDev(U8G2_R0, U8X8_PIN_NONE);
U8G2* oled = &oledDev;
#endif

// Buton etiketleri — kullaniciya GPIO degil islev gosterilir
#define LBL_OK     "OK"
#define LBL_YES    "EVET"
#define LBL_NO     "HAYIR"
#define LBL_BACK   "GERI"

#define F_SM u8g2_font_5x7_tf
#define F_MD u8g2_font_6x10_tf
#define F_LG u8g2_font_helvB12_tf

#ifdef HOST_SIM
bool  uiBegin() { uiOk = true; return true; }
#else
// ---------------------------------------------------------------------------
//  OLED bring-up — CALISAN testin ayni mantigi (oled_i2c_test/scanBothOrders)
//
//  SSD1309 ilk Wire.begin() sonrasi "stuck bus" davranisi gosteriyor:
//  ilk islem NAK donuyor. Her basarisiz I2C islemi hatta clock darbesi uretir;
//  TAM tarama (1..127) yeterli darbe uretip hatti acar. Sadece 0x3C/0x3D
//  denemek YETMIYOR — bu yuzden onceki surumlerde ekran bos kaliyordu.
// ---------------------------------------------------------------------------
static int i2cScanFull(byte* oledAddr) {
  int found = 0;
  for (byte a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      found++;
      Serial.printf("[i2c] 0x%02X cevap verdi\n", a);
      if ((a == 0x3C || a == 0x3D) && !*oledAddr) *oledAddr = a;
    }
  }
  return found;
}

bool uiBegin() {
  byte addr = 0;

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
  delay(30);

  int n = i2cScanFull(&addr);
  Serial.printf("[i2c] tarama 1: %d cihaz, oled=%s\n", n, addr ? "var" : "yok");

  if (!addr) {
    Wire.end();
    delay(20);
    Wire.begin(PIN_SCL, PIN_SDA);          // SDA/SCL takas
    Wire.setClock(100000);
    delay(30);
    n = i2cScanFull(&addr);
    Serial.printf("[i2c] tarama 2 (takas): %d cihaz, oled=%s\n", n, addr ? "var" : "yok");
    Wire.end();
    delay(20);
    Wire.begin(PIN_SDA, PIN_SCL);          // dogru sirada kal
    Wire.setClock(100000);
    delay(30);
    if (!addr) i2cScanFull(&addr);         // 3. tam tarama (darbeler hatti acar)
  }

  if (!addr) {
    Serial.println("[i2c] OLED BULUNAMADI");
    Serial.println("      kontrol: VCC=3V3 GND SCL=G22 SDA=G21");
    Serial.println("               RES->3V3  A0->GND  CS->GND  (R4+R9 takili, R5 sokulu)");
    // yine de dene — bazi moduller adres vermeden ACK doner
    addr = 0x3C;
  }

  oled->setI2CAddress(addr << 1);
  oled->begin();
  oled->setBusClock(400000);
  oled->setContrast(200);
  oled->setPowerSave(0);

  // begin sonrasi hat canli mi?
  Wire.beginTransmission(addr);
  bool alive = (Wire.endTransmission() == 0);
  Serial.printf("[i2c] OLED 0x%02X begin sonrasi: %s\n", addr, alive ? "ACK" : "NAK");

  // NAK ise tek kez daha dene (bazen ilk begin yarim kalir)
  if (!alive) {
    delay(60);
    oled->begin();
    oled->setBusClock(400000);
    Wire.beginTransmission(addr);
    alive = (Wire.endTransmission() == 0);
    Serial.printf("[i2c] tekrar deneme: %s\n", alive ? "ACK" : "NAK");
  }

  uiOk = alive;
  return alive;
}
#endif
U8G2& disp()    { return *oled; }

const char* tr(const char* s) { return s; }

void fmtKwh(char* out, size_t n, float kwh) {
  snprintf(out, n, "%.1f", kwh);
}

static void center(const char* s, int y) {
  int w = oled->getStrWidth(s);
  oled->drawStr((128 - w) / 2, y, s);
}

// ============================================================ ok ikonlari
// U8g2 5x7 fontunda ok glifi yok — ucgen olarak ciziyoruz.
// Y ekseni asagi dogru artar: "yukari ok"un sivri ucu kucuk y'de.

static void triUp(int x, int y, int w, int h) {
  oled->drawTriangle(x, y + h, x + w, y + h, x + w / 2, y);
}

static void triDown(int x, int y, int w, int h) {
  oled->drawTriangle(x, y, x + w, y, x + w / 2, y + h);
}

// footer metninde 0x01 = yukari ok, 0x02 = asagi ok
#define ICO_UP   "\x01"
#define ICO_DOWN "\x02"

// bir footer'i verilen font ve ikon genisligiyle olcup cizer; genislik doner
static int footerDraw(const char* txt, const uint8_t* font,
                      int charAdv, int iconW, int iconAdv, int y) {
  oled->setFont(font);
  int x = 2;
  const uint8_t* p = (const uint8_t*)txt;

  while (*p) {
    if (*p == 0x01) {
      triUp(x, y - 5, iconW, 5);
      x += iconAdv;
    } else if (*p == 0x02) {
      triDown(x, y - 5, iconW, 5);
      x += iconAdv;
    } else {
      char b[2] = {(char)*p, 0};
      oled->drawStr(x, y, b);
      x += charAdv;
    }
    p++;
  }
  return x;
}

static int footerWidth(const char* txt, int charAdv, int iconAdv) {
  int w = 0;
  for (const uint8_t* p = (const uint8_t*)txt; *p; p++) {
    w += (*p == 0x01 || *p == 0x02) ? iconAdv : charAdv;
  }
  return w + 4;
}

// ============================================================ durum ikonlari
// Ust barda uc sembol: WiFi (router), sarj cihazi (fis), backend (bulut).
//   dolu   = calisiyor
//   yanip  = kismi (bagli ama internet yok / backend cevap vermiyor)
//   carpi  = hata

static const char* BMP_WIFI[7] = {
  "...###...",
  ".##...##.",
  "#.......#",
  "..#####..",
  ".#.....#.",
  "...###...",
  "....#....",
};
static const char* BMP_PLUG[7] = {
  "..#...#..",
  "..#...#..",
  ".#######.",
  ".#######.",
  ".#######.",
  "..#####..",
  "....#....",
};
static const char* BMP_CLOUD[7] = {
  "...###...",
  "..#####..",
  ".#######.",
  "#########",
  "#########",
  ".#######.",
  "..#####..",
};

static void bmpDraw(const char* const* rows, int x, int y, int w, int h) {
  for (int r = 0; r < h; r++) {
    const char* p = rows[r];
    for (int c = 0; c < w && p[c]; c++) {
      if (p[c] == '#') oled->drawPixel(x + c, y + r);
    }
  }
}

static void icoCross(int x, int y, int w, int h) {
  oled->drawLine(x, y, x + w - 1, y + h - 1);
  oled->drawLine(x + w - 1, y, x, y + h - 1);
}

// durum: 0 = hata (carpi), 1 = kismi (yanip soner), 2 = tamam
static void icoState(const char* const* rows, int x, int y, int w, int h, int st) {
  bool drawIt = true;
  if (st == 1) drawIt = ((millis() / 500) % 2);    // kismi: yanip soner

  if (drawIt) bmpDraw(rows, x, y, w, h);

  if (st == 0) {
    // cerceve + carpi: bos kutu, icinde X
    oled->drawFrame(x - 1, y - 1, w + 2, h + 2);
    oled->setDrawColor(0);
    oled->drawBox(x, y, w, h);                     // simgeyi sil
    oled->setDrawColor(1);
    icoCross(x + 1, y + 1, w - 2, h - 2);
  }
}

// sagdan sola: [wifi] [fis] [bulut]
void uiStatusBar(int rightX, int y) {
  int wifiSt = !netUplinkOk() ? 0 : (netHasInternet() ? 2 : 1);
  int plugSt = charger.online() ? 2 : 0;
  int srvSt  = syncOnline() ? 2 : (syncFails() > 0 ? 0 : 1);

  int x3 = rightX - 8;          // bulut
  int x2 = x3 - 12;             // fis
  int x1 = x2 - 12;             // wifi

  icoState(BMP_WIFI,  x1, y, 9, 7, wifiSt);
  icoState(BMP_PLUG,  x2, y, 9, 7, plugSt);
  icoState(BMP_CLOUD, x3, y, 9, 7, srvSt);
}

// ---------------------------------------------------------------- parcalar
// ---------------------------------------------------------------- uyku
static bool asleep = false;

void uiSleep() {
  if (asleep || !oled) return;
  oled->setPowerSave(1);          // SSD1309: panel kapali, RAM korunur
  asleep = true;
  Serial.println("[ui] ekran uyudu");
}

void uiWake() {
  if (!asleep || !oled) return;
  oled->setPowerSave(0);
  asleep = false;
  Serial.println("[ui] ekran uyandi");
}

bool uiAsleep() { return asleep; }

void uiHeader(const char* title, const char* right) {
  oled->setFont(F_MD);
  oled->drawBox(0, 0, 128, 11);
  oled->setDrawColor(0);
  oled->drawStr(3, 9, title);
  if (right) {
    int w = oled->getStrWidth(right);
    oled->drawStr(125 - w, 9, right);
  }
  oled->setDrawColor(1);
}

void uiFooter(const char* txt) {
  oled->drawHLine(0, 54, 128);

  // 5x7 sigmazsa 4x6'ya dus (128 px sinir)
  if (footerWidth(txt, 6, 8) <= 128) {
    footerDraw(txt, u8g2_font_5x7_tf, 6, 6, 8, 63);
  } else {
    footerDraw(txt, u8g2_font_4x6_tf, 5, 5, 7, 63);
  }
}

// ---------------------------------------------------------------- karşılama
void uiWelcome(const String& clock, const String& date, bool online, int userCount) {
  oled->clearBuffer();

  // ust bar: cihaz adi + durum ikonlari
  oled->setFont(F_SM);
  oled->drawStr(2, 8, DEVICE_NAME);
  uiStatusBar(126, 1);
  oled->drawHLine(0, 11, 128);

  // saat
  bool blink = (millis() / 600) % 2;
  oled->setFont(u8g2_font_helvB18_tn);
  const char* t = clock.c_str();
  int w = oled->getStrWidth(t);
  oled->drawStr((128 - w) / 2, 33, t);

  // tarih
  oled->setFont(F_SM);
  center(date.c_str(), 44);

  // ana mesaj
  oled->setFont(F_MD);
  if (blink) center(">> KART OKUTUN <<", 55);
  else       center("  KART OKUTUN  ", 55);

  // ikincil ipucu: yonetici girisi (kart olmadan da girilebilsin)
  oled->setFont(F_SM);
  center(userCount == 0 ? "OK: kurulum yap" : "OK: yonetici", 63);

  oled->sendBuffer();
}

void uiWaitingCard(const String& msg) {
  oled->clearBuffer();
  uiHeader("BEKLENIYOR", nullptr);
  oled->setFont(F_MD);
  center(msg.c_str(), 30);
  oled->setFont(F_SM);
  center("GERI: iptal", 46);
  oled->sendBuffer();
}

// ---------------------------------------------------------------- kullanici
void uiUserHome(const CardRec* c, float monthKwh, float todayKwh) {
  oled->clearBuffer();

  char right[16];
  snprintf(right, sizeof(right), "%s", c->admin ? "ADMIN" : "KART");
  uiHeader(c->name, right);

  char b[32];
  oled->setFont(F_MD);

  oled->drawStr(3, 24, "Bu ay");
  snprintf(b, sizeof(b), "%5.1f kWh", monthKwh);
  oled->drawStr(126 - oled->getStrWidth(b), 24, b);

  oled->drawStr(3, 36, "Bugun");
  snprintf(b, sizeof(b), "%5.1f kWh", todayKwh);
  oled->drawStr(126 - oled->getStrWidth(b), 36, b);

  oled->drawStr(3, 48, "Kalan");
  if (c->quota_kwh == 0) snprintf(b, sizeof(b), "sinirsiz");
  else                   snprintf(b, sizeof(b), "%5.1f kWh", c->quota_kwh - monthKwh);
  oled->drawStr(126 - oled->getStrWidth(b), 48, b);

  // admin kartinda ek bir kisa yol var: EVET = yonetici menusu
  uiFooter(c->admin ? "OK:cihaz  " ICO_UP ":menu" : "OK:cihaz  GERI:cikis");
  oled->sendBuffer();
}

void uiUserDevice(const CardRec* c) {
  oled->clearBuffer();
  uiHeader("CIHAZ", c->name);

  const char* st = "BOS";
  switch (charger.state()) {
    case CH_CHARGING: st = "SARJ EDILIYOR"; break;
    case CH_FREE:     st = "ARAC YOK";      break;
    case CH_INSERT:   st = "ARAC TAKILDI";  break;
    case CH_PAUSED:   st = "DURAKLATILDI";  break;
    case CH_DONE:     st = "BITTI";         break;
    case CH_FAULT:    st = "ARIZA";         break;
    case CH_OFFLINE:  st = "BAGLANTI YOK";  break;
    default: break;
  }
  oled->setFont(F_SM);
  oled->drawStr(3, 20, "Durum");
  oled->setFont(F_MD);
  oled->drawStr(38, 20, st);

  oled->drawHLine(0, 23, 128);

  char v[24];
  // sol hucre 0..63, sag hucre 64..127
  oled->setFont(F_SM);
  oled->drawStr(3, 33, "Akim");
  oled->drawStr(3, 44, "Guc");
  oled->drawStr(3, 55, "Oturum");
  oled->drawStr(66, 33, "Sicak");
  oled->drawStr(66, 44, "Sure");

  oled->setFont(F_MD);
  snprintf(v, sizeof(v), "%.0f A", (double)charger.currentA());
  oled->drawStr(62 - oled->getStrWidth(v), 33, v);
  snprintf(v, sizeof(v), "%.2f kW", (double)charger.powerKw());
  oled->drawStr(62 - oled->getStrWidth(v), 44, v);
  snprintf(v, sizeof(v), "%.1f kWh", (double)charger.sessionKwh());
  oled->drawStr(126 - oled->getStrWidth(v), 55, v);

  snprintf(v, sizeof(v), "%.0fC", (double)charger.tempC());
  oled->drawStr(126 - oled->getStrWidth(v), 33, v);

  uint32_t sec = charger.sessionSec();
  snprintf(v, sizeof(v), "%lu:%02lu", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
  oled->drawStr(126 - oled->getStrWidth(v), 44, v);

  uiFooter("OK:baslat  GERI:geri");
  oled->sendBuffer();
}

void uiCharging(const CardRec* c) {
  oled->clearBuffer();
  uiHeader("SARJ EDILIYOR", c->name ? c->name : nullptr);

  char b[32];

  // --- buyuk: anlik sarj gucu ---
  float kw = charger.powerKw();
  oled->setFont(u8g2_font_helvB18_tn);
  snprintf(b, sizeof(b), "%.1f", (double)kw);
  int w = oled->getStrWidth(b);
  oled->drawStr(5, 34, b);
  oled->setFont(F_MD);
  oled->drawStr(5 + w + 2, 34, "kW");

  // --- sag ust: akim ---
  snprintf(b, sizeof(b), "%u A", (unsigned)charger.currentA());
  int wa = oled->getStrWidth(b);
  oled->drawStr(123 - wa, 22, b);
  // --- sag alt: sicaklik ---
  snprintf(b, sizeof(b), "%.0f C", (double)charger.tempC());
  int wc = oled->getStrWidth(b);
  oled->drawStr(123 - wc, 33, b);

  oled->drawHLine(0, 39, 128);

  // --- sol alt: oturum kWh ---
  oled->setFont(F_LG);
  snprintf(b, sizeof(b), "%.2f", (double)charger.sessionKwh());
  oled->drawStr(3, 52, b);
  oled->setFont(F_SM);
  oled->drawStr(3 + oled->getStrWidth(b) + 1, 52, "kWh");

  // --- sag alt: sure ---
  uint32_t s = charger.sessionSec();
  oled->setFont(F_MD);
  snprintf(b, sizeof(b), "%02lu:%02lu",
           (unsigned long)(s / 3600), (unsigned long)((s / 60) % 60));
  int wt = oled->getStrWidth(b);
  oled->drawStr(123 - wt, 52, b);

  uiFooter("KART OKUT: bitir   GERI:menu");
  oled->sendBuffer();
}

void uiAccessDenied(const String& uid) {
  oled->clearBuffer();
  uiHeader("TANINMAYAN KART", nullptr);
  oled->setFont(F_MD);
  center("Bu kart kayitli degil", 30);
  oled->setFont(F_SM);
  center(uid.c_str(), 42);
  uiFooter("GERI:geri");
  oled->sendBuffer();
}

void uiQuotaFull(const CardRec* c) {
  oled->clearBuffer();
  uiHeader("KOTA DOLU", c->name);
  oled->setFont(F_MD);
  center("Aylik kotaniz doldu", 30);
  char b[32];
  snprintf(b, sizeof(b), "%.1f / %u kWh", (double)c->quota_kwh, c->quota_kwh);
  oled->setFont(F_SM);
  center(b, 44);
  uiFooter("GERI:geri");
  oled->sendBuffer();
}

void uiScheduleBlocked(const Settings& st) {
  oled->clearBuffer();
  uiHeader("ZAMANLAMA", nullptr);
  oled->setFont(F_MD);
  center("Su an sarj penceresi", 26);
  center("disinda", 38);
  char b[32];
  snprintf(b, sizeof(b), "%02u:%02u - %02u:%02u", st.sh, st.sm, st.eh, st.em);
  oled->setFont(F_SM);
  center(b, 50);
  uiFooter("GERI:geri");
  oled->sendBuffer();
}

// ---------------------------------------------------------------- menu
void uiMenu(const char* title, const char* const* items, int n, int sel, int top) {
  oled->clearBuffer();
  uiHeader(title, nullptr);

  oled->setFont(F_MD);
  int rows = 4;
  for (int i = 0; i < rows; i++) {
    int idx = top + i;
    if (idx >= n) break;
    int y = 22 + i * 10;
    if (idx == sel) {
      oled->drawBox(0, y - 8, 128, 10);
      oled->setDrawColor(0);
      oled->drawStr(4, y, items[idx]);
      oled->setDrawColor(1);
    } else {
      oled->drawStr(4, y, items[idx]);
    }
  }

  // scroll cubugu
  if (n > rows) {
    int h = 42 * rows / n;
    int y = 13 + (42 - h) * top / (n - rows);
    oled->drawFrame(124, 13, 3, 42);
    oled->drawBox(125, y, 1, h);
  }

  uiFooter(ICO_UP ICO_DOWN ":gez   OK:sec");
  oled->sendBuffer();
}

void uiLogs(int sel, int top) {
  oled->clearBuffer();
  uiHeader("KAYITLAR", nullptr);

  int rows = 5;
  oled->setFont(F_SM);
  int n = store.logCount();
  if (n == 0) {
    center("kayit yok", 34);
  }
  for (int i = 0; i < rows; i++) {
    int idx = top + i;
    if (idx >= n) break;
    LogRec* r = store.log(idx);
    if (!r) break;
    char b[40];
    int ci = store.findCard(r->uid, r->len);
    const char* nm = (ci >= 0) ? store.card(ci)->name : "?";
    snprintf(b, sizeof(b), "%s %-8.8s %4.1f", stampStr(r->ts).c_str(), nm, r->kwh10 / 10.0);
    oled->drawStr(2, 20 + i * 8, b);
  }
  uiFooter("OK:sil  GERI:geri");
  oled->sendBuffer();
}

void uiAdminUsers(int sel, int top) {
  oled->clearBuffer();
  uiHeader("KULLANICILAR", nullptr);

  int n = store.cardCount();
  if (n == 0) { center("kart yok", 34); }
  int rows = 4;
  oled->setFont(F_MD);
  for (int i = 0; i < rows; i++) {
    int idx = top + i;
    if (idx >= n) break;
    CardRec* c = store.card(idx);
    int y = 22 + i * 10;
    if (idx == sel) {
      oled->drawBox(0, y - 8, 128, 10);
      oled->setDrawColor(0);
    }
    oled->drawStr(4, y, c->name);
    char b[16];
    snprintf(b, sizeof(b), "%s%.1f", c->admin ? "*" : " ", c->used_kwh10 / 10.0);
    oled->drawStr(126 - oled->getStrWidth(b), y, b);
    oled->setDrawColor(1);
  }
  uiFooter("OK:detay " ICO_UP ":yeni");
  oled->sendBuffer();
}

void uiUserDetail(const CardRec* c) {
  oled->clearBuffer();
  uiHeader(c->name, c->admin ? "ADMIN" : nullptr);

  char b[32];
  oled->setFont(F_SM);

  oled->drawStr(3, 22, "Kota");
  snprintf(b, sizeof(b), "%u kWh", c->quota_kwh);
  oled->setFont(F_MD);
  oled->drawStr(50, 22, b);

  oled->setFont(F_SM);
  oled->drawStr(3, 34, "Kullanim");
  snprintf(b, sizeof(b), "%.1f kWh", c->used_kwh10 / 10.0);
  oled->setFont(F_MD);
  oled->drawStr(50, 34, b);

  oled->setFont(F_SM);
  oled->drawStr(3, 46, "Son");
  oled->setFont(F_MD);
  oled->drawStr(50, 46, stampStr(c->last_ts).c_str());

  uiFooter("OK:sifirla " ICO_DOWN ":sil");
  oled->sendBuffer();
}

void uiSettings(int sel, const Settings& st, bool editing) {
  oled->clearBuffer();
  uiHeader("SARJ AYARLARI", editing ? "DEGISTIR" : nullptr);

  const char* labels[] = {"Maks akim", "Varsayilan", "Kart zorunlu", "Max sure"};
  char vals[4][16];
  snprintf(vals[0], 16, "%u A", st.max_a);
  snprintf(vals[1], 16, "%u A", st.def_a);
  snprintf(vals[2], 16, "%s", st.require_card ? "EVET" : "HAYIR");
  snprintf(vals[3], 16, "%u dk", st.max_session_min);

  oled->setFont(F_MD);
  for (int i = 0; i < 4; i++) {
    int y = 22 + i * 10;
    bool marked = (i == sel);
    if (marked) {
      oled->drawBox(0, y - 8, 128, 10);
      oled->setDrawColor(0);
    }
    oled->drawStr(4, y, labels[i]);
    oled->drawStr(126 - oled->getStrWidth(vals[i]), y, vals[i]);
    if (marked && editing) {
      oled->drawStr(2, y, ">");
    }
    oled->setDrawColor(1);
  }

  uiFooter(editing ? ICO_UP ICO_DOWN ":degis  OK:bitir" : ICO_UP ICO_DOWN ":gez  OK:sec");
  oled->sendBuffer();
}

void uiSchedule(const Settings& st, int sel, bool editing) {
  oled->clearBuffer();
  uiHeader("ZAMANLAMA", st.schedule_en ? "ACIK" : "KAPALI");

  const char* labels[3] = {"Baslangic", "Bitis", "Sarj"};
  char vals[3][16];
  snprintf(vals[0], 16, "%02u:%02u", st.sh, st.sm);
  snprintf(vals[1], 16, "%02u:%02u", st.eh, st.em);
  snprintf(vals[2], 16, "%s", st.schedule_en ? "ACIK" : "KAPALI");

  oled->setFont(F_MD);
  for (int i = 0; i < 3; i++) {
    int y = 22 + i * 10;
    bool marked = (i == sel);
    if (marked) {
      oled->drawBox(0, y - 8, 128, 10);
      oled->setDrawColor(0);
    }
    oled->drawStr(4, y, labels[i]);
    oled->drawStr(126 - oled->getStrWidth(vals[i]), y, vals[i]);
    if (marked && editing) oled->drawStr(2, y, ">");
    oled->setDrawColor(1);
  }

  // gun noktalari  P S C P C C P
  const char* g[] = {"P","S","C","P","C","C","P"};
  oled->setFont(F_SM);
  for (int i = 0; i < 7; i++) {
    int x = 4 + i * 13;
    oled->drawStr(x + 1, 52, g[i]);
    if (st.days & (1 << i)) oled->drawBox(x, 55, 8, 6);
    else                    oled->drawFrame(x, 55, 8, 6);
  }
  oled->setFont(F_SM);
  oled->drawStr(100, 61, editing ? "OK:bitir" : "OK:sec");
  oled->sendBuffer();
}

void uiDeviceInfo(const Settings& st) {
  (void)st;
  oled->clearBuffer();
  uiHeader("AG DURUMU", nullptr);
  uiStatusBar(126, 1);

  char b[44];
  oled->setFont(F_SM);

  // STA
  if (netUplinkOk()) {
    snprintf(b, sizeof(b), "WiFi  %s %ddBm", netStaIp().c_str(), netRssi());
  } else {
    snprintf(b, sizeof(b), "WiFi  BAGLI DEGIL");
  }
  oled->drawStr(3, 20, b);

  // AP + NAPT
  snprintf(b, sizeof(b), "AP    %s %s", netApIp().c_str(),
           netNaptOk() ? "NAT" : "NAT-YOK");
  oled->drawStr(3, 30, b);

  // bagli istemci + internet
  snprintf(b, sizeof(b), "Bagli %d   NET %s", netApClients(),
           netHasInternet() ? "var" : "yok");
  oled->drawStr(3, 40, b);

  // sarj cihazi
  String cip = netChargerIp();
  snprintf(b, sizeof(b), "Cihaz %s", cip.length() ? cip.c_str() : "-");
  oled->drawStr(3, 50, b);

  // backend
  snprintf(b, sizeof(b), "Panel %s  rev %lu",
           syncOnline() ? "OK" : "yok", (unsigned long)syncRev());
  oled->drawStr(3, 60, b);

  oled->sendBuffer();
}

void uiHealth() {
  oled->clearBuffer();
  uiHeader("SISTEM", nullptr);

  char b[40];
  oled->setFont(F_SM);

  uint32_t up = healthUptimeSec();
#ifdef HOST_SIM
  unsigned heapK = 120;
#else
  unsigned heapK = ESP.getFreeHeap() / 1024;
#endif
  snprintf(b, sizeof(b), "Acik  %lud %02luh  heap %uk",
           (unsigned long)(up / 86400), (unsigned long)((up / 3600) % 24), heapK);
  oled->drawStr(3, 19, b);

#ifdef HOST_SIM
  const char* wifiTxt = "OK";
#else
  const char* wifiTxt = (WiFi.status() == WL_CONNECTED) ? "OK" : "YOK";
#endif
  snprintf(b, sizeof(b), "WiFi  %s  kopma %lu",
           wifiTxt, (unsigned long)netReconnects());
  oled->drawStr(3, 29, b);

  snprintf(b, sizeof(b), "Panel %s  hata %d",
           syncOnline() ? "OK" : "YOK", syncFails());
  oled->drawStr(3, 39, b);

  snprintf(b, sizeof(b), "Kuyruk %d   Cihaz %s",
           syncQueueDepth(), charger.online() ? "OK" : "YOK");
  oled->drawStr(3, 49, b);

  snprintf(b, sizeof(b), "Restart %lu  %s",
           (unsigned long)healthBootCount(), healthLastReason());
  oled->drawStr(3, 59, b);

  oled->sendBuffer();
}

void uiAbout() {
  oled->clearBuffer();
  uiHeader("HAKKINDA", nullptr);
  oled->setFont(F_MD);
  center(DEVICE_NAME, 26);
  oled->setFont(F_SM);
  center("Kartli sarj sistemi", 38);
  char b[32];
  snprintf(b, sizeof(b), "v%s", FW_VERSION);
  center(b, 50);
  center("Back: geri", 62);
  oled->sendBuffer();
}

// Buton eslemesini bulmak icin: hangi GPIO basilirsa burada yanar.
void uiButtonTest(int pressedPin) {
  oled->clearBuffer();
  uiHeader("BUTON TESTI", nullptr);

  char b[32];
  oled->setFont(F_SM);

  struct { const char* ad; int pin; } rows[4] = {
    {"",     PIN_BTN_UP},     // yukari ok cizilecek
    {"",     PIN_BTN_DOWN},   // asagi ok cizilecek
    {"GERI ", PIN_BTN_BACK},
    {"OK   ", PIN_BTN_OK},
  };

  for (int i = 0; i < 4; i++) {
    int y = 22 + i * 9;
    bool on = (pressedPin == rows[i].pin);
    if (on) {
      oled->drawBox(0, y - 7, 128, 9);
      oled->setDrawColor(0);
    }
    if (i == 0)      triUp(6, y - 6, 7, 6);
    else if (i == 1) triDown(6, y - 6, 7, 6);
    else             oled->drawStr(4, y, rows[i].ad);

    snprintf(b, sizeof(b), "GPIO %2d", rows[i].pin);
    oled->drawStr(20, y, b);
    if (on) oled->drawStr(96, y, "BASILI");
    oled->setDrawColor(1);
  }

  oled->setFont(F_SM);
  if (pressedPin < 0) {
    oled->drawStr(3, 61, "bir tusa bas...  GERI:cik");
  } else {
    snprintf(b, sizeof(b), "okunan: GPIO %d", pressedPin);
    oled->drawStr(3, 61, b);
  }
  oled->sendBuffer();
}

void uiPinPad(const char* prompt, int digits, int entered, int cur) {
  oled->clearBuffer();
  uiHeader("YONETICI GIRISI", nullptr);

  oled->setFont(F_MD);
  center(prompt, 24);

  int boxW = 14, gap = 4;
  int total = digits * boxW + (digits - 1) * gap;
  int x0 = (128 - total) / 2;

  for (int i = 0; i < digits; i++) {
    int x = x0 + i * (boxW + gap);
    oled->drawFrame(x, 32, boxW, 15);

    if (i < entered) {
      // girilmis hane: dolu daire
      oled->drawDisc(x + boxW / 2, 39, 3);
    } else if (i == entered && cur >= 0) {
      // siradaki hane: secili rakam (yanip sonen)
      if ((millis() / 400) % 2) {
        char d[2] = {(char)('0' + cur), 0};
        oled->setFont(F_SM);
        oled->drawStr(x + boxW / 2 - 2, 42, d);
      }
      oled->drawFrame(x - 1, 31, boxW + 2, 17);   // vurgu cercevesi
    }
  }

  oled->setFont(F_SM);
  center(entered == 0 ? ICO_UP ICO_DOWN ":rakam" : "OK:onay  GERI:sil", 56);
  oled->sendBuffer();
}

// EVET / HAYIR onayi — yikici islemler icin
void uiConfirm(const char* l1, const char* l2) {
  oled->clearBuffer();
  uiHeader("ONAY", nullptr);

  oled->setFont(F_MD);
  center(l1, 24);
  oled->setFont(F_SM);
  if (l2 && *l2) center(l2, 36);

  // iki buyuk buton: yesil=yukari ok, kirmizi=asagi ok
  oled->drawRBox(4, 42, 56, 18, 3);
  oled->setDrawColor(0);
  triUp(22, 46, 10, 9);
  oled->setFont(F_SM);
  oled->drawStr(37, 55, "EVET");
  oled->setDrawColor(1);

  oled->drawRFrame(68, 42, 56, 18, 3);
  triDown(110, 46, 10, 9);
  oled->setFont(F_SM);
  oled->drawStr(78, 55, "HAYIR");

  oled->sendBuffer();
}

void uiMessage(const char* l1, const char* l2, const char* l3) {
  oled->clearBuffer();
  if (l1) { oled->setFont(F_MD); center(l1, 24); }
  if (l2) { oled->setFont(F_SM); center(l2, 38); }
  if (l3) { oled->setFont(F_SM); center(l3, 52); }
  oled->sendBuffer();
}
