#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <string.h>
#include "pins.h"
#include "store.h"
#include "hw.h"
#include "ui.h"
#include "charger.h"
#include "net.h"
#include "config.h"
#include "sync.h"
#include "health.h"

// ===========================================================================
//  DURUM MAKINESI
//
//  KARSILAMA --(kullanici karti)--> KULLANICI --(OK)--> CIHAZ --(OK)--> SARJ
//      |                                |                  |
//      |                                +--(BACK)--> cikis  +--(BACK)--> geri
//      +--(admin karti)---------------> YONETICI MENU
//      +--(OK uzun bas)--> PIN -------->
//
//  Karsilama ekraninda baska hicbir islem yapilamaz.
// ===========================================================================

enum Screen : uint8_t {
  S_WELCOME = 0,
  S_USER_HOME,
  S_USER_DEVICE,
  S_CHARGING,
  S_DENIED,
  S_QUOTA,
  S_SCHED_BLOCK,
  S_PIN,
  S_ADMIN_MENU,
  S_ADMIN_USERS,
  S_ADMIN_USER,
  S_ADMIN_LOGS,
  S_ADMIN_SETTINGS,
  S_ADMIN_SCHEDULE,
  S_ADMIN_DEVICE,
  S_ADMIN_HEALTH,
  S_ADMIN_BTNTEST,
  S_ADMIN_ABOUT,
  S_ENROLL,
  S_CONFIRM,
  S_TOAST,
};

static Screen   scr      = S_WELCOME;

// ekran uykusu: son kullanici etkilesiminden bu yana gecen sure
#define SCREEN_SLEEP_MS   60000UL     // 60 sn
static uint32_t lastActivity = 0;
// Kullanici sarj ekraninda kirmizi ile ekrani elle uyuttuysa: otomatik
// uyandirma devre disi kalir, bir sonraki tus/kart hareketine kadar kapali kalir.
static bool     manualSleep = false;

static void touchActivity() {
  lastActivity = millis();
  manualSleep = false;
}
static int      userIdx  = -1;
static int      sel      = 0;
static int      top      = 0;
static bool     editing  = false;
static uint32_t msgUntil = 0;
static uint32_t lastDraw = 0;
static bool     dirty    = true;
static Screen   afterMsg = S_WELCOME;

// onay ekrani
static Screen   confirmBack = S_ADMIN_USERS;
static int      pendingDel  = -1;
static char     confirmL1[20] = "";
static char     confirmL2[20] = "";

// PIN
static String pinBuf;
static int    pinLen = 0;
static int    pinCur = 0;     // su an secili rakam (0-9)
#define PIN_LEN   4
// Yonetici PIN'i. Uretimde derleme zamaninda degistir:
//   pio run -D ADMIN_PIN='"4826"'
// (Kalici/degistirilebilir PIN henuz yok — "Bilinen sinirlar" bolumune bak.)
#ifndef ADMIN_PIN
#define ADMIN_PIN "0000"
#endif

// oturum
static bool     charging = false;
static uint32_t startTs  = 0;
static float    startKwh = 0;

// gunluk sayac
static float    todayKwh = 0;
static int      todayDay = -1;

static const char* ADMIN_ITEMS[] = {
  "Kullanicilar", "Kayitlar", "Sarj Ayarlari", "Zamanlama",
  "Buton Testi", "Cihaz Bilgisi", "Sistem", "Cikis"
};
static const int ADMIN_N     = 8;
static const int ADMIN_ROWS  = 4;
static const int USER_ROWS   = 4;
static const int LOG_ROWS    = 5;

// ---------------------------------------------------------------- yardimci
static void go(Screen s) {
  scr = s; sel = 0; top = 0; editing = false; dirty = true;
  touchActivity();
  if (uiAsleep()) uiWake();
}

static void clampTop(int& t, int s, int count, int rows) {
  if (s < t) t = s;
  if (s >= t + rows) t = s - rows + 1;
  if (t > count - rows) t = count - rows;
  if (t < 0) t = 0;
}

static void toast(const char* a, const char* b, Screen back = S_WELCOME, uint32_t ms = 1800) {
  afterMsg = back;
  uiMessage(a, b, nullptr);
  scr = S_TOAST;
  msgUntil = millis() + ms;
  dirty = true;
}

static void askConfirm(const char* l1, const char* l2, Screen back) {
  strncpy(confirmL1, l1, sizeof(confirmL1) - 1);
  strncpy(confirmL2, l2, sizeof(confirmL2) - 1);
  confirmBack = back;
  scr = S_CONFIRM;
  dirty = true;
}

static void draw() {
  switch (scr) {
    case S_WELCOME:
      uiWelcome(timeStr(), dateStr(), charger.online(), store.cardCount());
      break;

    case S_USER_HOME: {
      CardRec* c = store.card(userIdx);
      if (!c) { go(S_WELCOME); return; }
      uiUserHome(c, c->used_kwh10 / 10.0f, todayKwh);
      break;
    }
    case S_USER_DEVICE: {
      CardRec* c = store.card(userIdx);
      if (!c) { go(S_WELCOME); return; }
      uiUserDevice(c);
      break;
    }
    case S_CHARGING: {
      CardRec* c = store.card(userIdx);
      if (!c) { go(S_WELCOME); return; }
      uiCharging(c);
      break;
    }
    case S_DENIED: {
      break;   // cizim onCard/startCharging icinde yapildi
    }
    case S_QUOTA: {
      break;
    }
    case S_SCHED_BLOCK: {
      break;
    }
    case S_PIN:   uiPinPad("Sifre girin", PIN_LEN, pinLen, pinCur); break;
    case S_ADMIN_MENU:   uiMenu("YONETICI", ADMIN_ITEMS, ADMIN_N, sel, top); break;
    case S_ADMIN_USERS:  uiAdminUsers(sel, top); break;
    case S_ADMIN_USER: {
      CardRec* c = store.card(sel);
      if (!c) { go(S_ADMIN_USERS); return; }
      uiUserDetail(c);
      break;
    }
    case S_ADMIN_LOGS:     uiLogs(sel, top); break;
    case S_ADMIN_SETTINGS: uiSettings(sel, store.settings(), editing); break;
    case S_ADMIN_SCHEDULE: uiSchedule(store.settings(), sel, editing); break;
    case S_ADMIN_DEVICE:   uiDeviceInfo(store.settings()); break;
    case S_ADMIN_HEALTH:   uiHealth(); break;
    case S_ADMIN_BTNTEST:  uiButtonTest(btnPressedPin()); break;
    case S_ADMIN_ABOUT:    uiAbout(); break;
    case S_ENROLL:         uiWaitingCard("Kart okutun"); break;
    case S_CONFIRM:        uiConfirm(confirmL1, confirmL2); break;
    default: break;
  }
}

// ---------------------------------------------------------------- sarj
static void startCharging() {
  CardRec* c = store.card(userIdx);
  if (!c) return;

  Settings& st = store.settings();

  if (st.schedule_en && !clockInWindow(st.sh, st.sm, st.eh, st.em, st.days)) {
    scr = S_SCHED_BLOCK;
    uiScheduleBlocked(st);
    return;
  }
  if (c->quota_kwh > 0 && (c->used_kwh10 / 10.0f) >= c->quota_kwh) {
    scr = S_QUOTA;
    uiQuotaFull(c);
    return;
  }

  charger.releaseParkLock();       // kart onayladi: cihaz sarj moduna donsun
  charger.setCurrent(st.def_a);
  charger.enable(true);
  charging = true;
  startTs  = nowEpoch();
  startKwh = charger.sessionKwh();

  store.beginOpenSession(startTs, c->uid, c->len);
  go(S_CHARGING);
  Serial.printf("[sarj] basladi: %s %u A\n", c->name, st.def_a);
}

static void stopCharging(bool aborted) {
  if (!charging) return;

  float used = charger.sessionKwh() - startKwh;
  if (used < 0) used = 0;

  charger.enable(false);
  charging = false;

  CardRec* c = store.card(userIdx);
  if (c) {
    c->used_kwh10 += (uint16_t)(used * 10 + 0.5f);
    c->last_ts = startTs ? startTs : nowEpoch();
    store.saveCards();
  }
  todayKwh += used;

  LogRec r;
  memset(&r, 0, sizeof(r));
  r.ts     = startTs ? startTs : nowEpoch();
  r.kwh10  = (uint16_t)(used * 10 + 0.5f);
  r.reason = aborted ? 4 : 0;
  if (c) { memcpy(r.uid, c->uid, c->len); r.len = c->len; }
  store.addLog(r);
  store.clearOpenSession();

  if (c) syncSession(startTs ? startTs : nowEpoch(), c->uid, c->len, used,
                     aborted ? 4 : 0);

  Serial.printf("[sarj] bitti: %.2f kWh (%s)\n", used, aborted ? "iptal" : "normal");
}

// ---------------------------------------------------------------- kart
static void onCard(const CardId& id) {
  int idx = store.findCard(id.uid, id.len);

  // --- sarj surerken: bitirmek icin kart zorunlu ---
  // Ayni kullanici ya da yonetici kartı sarji bitirir. Baska kart yok sayilir.
  if (charging) {
    if (idx < 0) {
      scr = S_DENIED;
      uiAccessDenied(uidToStr(id));
      msgUntil = millis() + 2500;
      return;
    }
    CardRec* cc = store.card(idx);
    bool sameUser = (userIdx >= 0 && idx == userIdx);
    if (!sameUser && !cc->admin) {
      toast("Bu kart sarji bitiremez", cc->name);
      return;
    }
    float used = charger.sessionKwh() - startKwh;
    stopCharging(false);
    char msg[24];
    snprintf(msg, sizeof(msg), "%.2f kWh", used > 0 ? (double)used : 0.0);
    userIdx = sameUser ? idx : -1;
    toast("Sarj bitti", msg, sameUser ? S_USER_HOME : S_WELCOME);
    return;
  }

  // --- kart kaydetme modu ---
  if (scr == S_ENROLL) {
    if (idx >= 0) { toast("Kart zaten kayitli", store.card(idx)->name, S_ADMIN_USERS); return; }
    char nm[16];
    snprintf(nm, sizeof(nm), "KART%d", store.cardCount() + 1);
    store.addCard(id.uid, id.len, nm, false);
    // panele de yaz — yoksa sonraki config cekimi bu karti siler
    syncEnroll(id.uid, id.len, nm);
    toast("Kart eklendi", nm, S_ADMIN_USERS);
    return;
  }

  // --- kayitsiz kart ---
  if (idx < 0) {
    LogRec r;
    memset(&r, 0, sizeof(r));
    r.ts     = nowEpoch();
    r.reason = 5;                      // taninmayan kart
    r.len    = id.len;
    memcpy(r.uid, id.uid, id.len);
    store.addLog(r);
    syncUnknown(id.uid, id.len);

    scr = S_DENIED;
    uiAccessDenied(uidToStr(id));
    msgUntil = millis() + 2500;
    return;
  }

  CardRec* c = store.card(idx);
  if (!c->active) { toast("Kart pasif", c->name); return; }

  userIdx = idx;
  // admin de once kullanici ekranina duser: sarj baslatabilir, kendi
  // kullanimini gorur. Yonetici menusune EVET ile gecer.
  go(S_USER_HOME);
}

// ---------------------------------------------------------------- pin
static void pinReset() { pinBuf = ""; pinLen = 0; pinCur = 0; }

// UP/DOWN secili rakami degistirir, OK onaylayip siradaki haneye gecer
static void pinCycle(int d) {
  pinCur = (pinCur + d + 10) % 10;
  dirty = true;
}

static void pinBack() {
  if (pinLen > 0) { pinBuf.remove(pinLen - 1); pinLen--; dirty = true; }
}

static void pinAdd(int d) {
  if (pinLen >= PIN_LEN) return;
  pinBuf += String(d);
  pinLen++;
  pinCur = 0;
  dirty = true;
}

static void pinConfirm() {
  if (pinBuf == ADMIN_PIN) {
    pinReset();
    userIdx = -1;
    go(S_ADMIN_MENU);
  } else {
    pinReset();
    toast("Hatali sifre", nullptr, S_WELCOME);
  }
}

// ---------------------------------------------------------------- setup
void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n\n=== EVCHARGER / RFID ===");

  // OLED'i ONCE baslat — hata olursa seri portta goruruz
  bool oledOk = uiBegin();
  Serial.printf("[boot] OLED %s\n", oledOk ? "HAZIR" : "YOK (ekran bos kalacak)");

  if (oledOk) {
    // ekran gercekten ciziyor mu? kisa acilis testi
    uiMessage("BASLIYOR", DEVICE_NAME, FW_VERSION);
    delay(900);
  }

  store.begin();
  hwBegin();
  rfidBegin();

  netBegin();                 // AP + STA + NAPT
  charger.begin(CHARGER_SIM == 1);
  clockBegin();
  syncBegin();              // VPS panel senkronu

  // cihazi AP aginda ara
  String ip;
  if (netFindCharger(&ip, 4000)) {
    Serial.printf("[net] sarj cihazi bulundu: %s\n", ip.c_str());
  } else {
    Serial.println("[net] sarj cihazi bulunamadi (AP'ye bagli mi?)");
  }

  // --- elektrik kesintisinden kalan oturum var mi? ---
  OpenSession open;
  if (store.openSession(&open)) {
    float kwh = open.kwh10 / 10.0f;
    int ci = store.findCard(open.uid, open.len);
    if (ci >= 0 && kwh > 0) {
      CardRec* c = store.card(ci);
      c->used_kwh10 += open.kwh10;
      c->last_ts = open.startTs;
      store.saveCards();
      Serial.printf("[kurtarma] %s: %.1f kWh geri yazildi\n", c->name, kwh);
    }
    LogRec r;
    memset(&r, 0, sizeof(r));
    r.ts     = open.startTs;
    r.kwh10  = open.kwh10;
    r.reason = 6;                    // elektrik kesintisi
    r.len    = open.len;
    memcpy(r.uid, open.uid, open.len);
    store.addLog(r);
    store.clearOpenSession();
  }

  Serial.printf("[boot] kart=%d saat=%s surum=%s\n",
                store.cardCount(), clockValid() ? "OK" : "YOK", FW_VERSION);

  // Nobetci EN SON baslar: setup() sirasindaki mesgul bekleme
  // (WiFi, NTP, cihaz taramasi) yanlis restart tetiklemesin.
  healthBegin();

  dirty = true;
}

// ---------------------------------------------------------------- loop
void loop() {
  healthFeed();                // nobetciye "calisiyorum" sinyali
  healthLoop();

  charger.poll();
  netLoop();
  syncLoop();

  // --- kart ---
  CardId cid;
  if (rfidPoll(&cid)) {
    touchActivity();
    if (uiAsleep()) { uiWake(); dirty = true; }
    onCard(cid);
    dirty = true;
  }

  // --- butonlar ---
  Btn b = BTN_NONE;
  BtnEvent ev = btnPoll(&b);
  if (ev != EV_NONE) {
    touchActivity();
    if (uiAsleep()) uiWake();
    dirty = true;
  }

  if (ev == EV_CLICK || ev == EV_REPEAT) {
    bool rep = (ev == EV_REPEAT);

    switch (scr) {
      case S_WELCOME:
        if (b == BTN_OK) { pinReset(); go(S_PIN); }
        break;

      case S_USER_HOME: {
        CardRec* c = store.card(userIdx);
        bool isAdmin = c && c->admin;
        if (b == BTN_OK)   go(S_USER_DEVICE);
        if (b == BTN_UP && isAdmin) go(S_ADMIN_MENU);   // EVET = yonetici
        if (b == BTN_BACK) { userIdx = -1; go(S_WELCOME); }
        break;
      }

      case S_USER_DEVICE:
        if (b == BTN_OK)   startCharging();
        if (b == BTN_BACK) go(S_USER_HOME);
        break;

      case S_CHARGING:
        // Kirmizi (HAYIR/asagi) = ekrani uyut. Sarj DURMAZ, baska ekrana da
        // gecilmez. Ekrani geri acmak icin herhangi bir tusa basmak yeter.
        if (b == BTN_DOWN) {
          manualSleep = true;
          uiSleep();
        }
        // Yesil (EVET/yukari), OK ve GERI: sarj surerken etkisiz.
        break;

      case S_DENIED:
      case S_QUOTA:
      case S_SCHED_BLOCK:
        if (b == BTN_BACK || b == BTN_OK) {
          msgUntil = 0;
          userIdx = -1;
          go(S_WELCOME);
        }
        break;

      case S_PIN:
        if (b == BTN_UP)   pinCycle(+1);
        if (b == BTN_DOWN) pinCycle(-1);
        if (b == BTN_OK) {
          if (pinLen >= PIN_LEN) { pinConfirm(); }
          else { pinAdd(pinCur); if (pinLen >= PIN_LEN) pinConfirm(); }
        }
        if (b == BTN_BACK) {
          if (pinLen > 0) pinBack();
          else { pinReset(); go(S_WELCOME); }
        }
        break;

      case S_ADMIN_MENU:
        if (b == BTN_UP)   { sel = (sel + ADMIN_N - 1) % ADMIN_N; clampTop(top, sel, ADMIN_N, ADMIN_ROWS); }
        if (b == BTN_DOWN) { sel = (sel + 1) % ADMIN_N;           clampTop(top, sel, ADMIN_N, ADMIN_ROWS); }
        if (b == BTN_OK) {
          switch (sel) {
            case 0: go(S_ADMIN_USERS);    break;
            case 1: go(S_ADMIN_LOGS);     break;
            case 2: go(S_ADMIN_SETTINGS); break;
            case 3: go(S_ADMIN_SCHEDULE); break;
            case 4: go(S_ADMIN_BTNTEST);  break;
            case 5: go(S_ADMIN_DEVICE);   break;
            case 6: go(S_ADMIN_HEALTH);   break;
            case 7: userIdx = -1; go(S_WELCOME); break;
          }
        }
        if (b == BTN_BACK) { userIdx = -1; go(S_WELCOME); }
        break;

      case S_ADMIN_USERS: {
        int n = store.cardCount();
        if (b == BTN_UP   && sel > 0)     { sel--; clampTop(top, sel, n, USER_ROWS); }
        if (b == BTN_DOWN && sel < n - 1) { sel++; clampTop(top, sel, n, USER_ROWS); }
        if (b == BTN_OK)   go(S_ADMIN_USER);
        if (b == BTN_UP && !n)  go(S_ENROLL);      // kart yokken EVET = yeni kart
        if (b == BTN_BACK) go(S_ADMIN_MENU);
        break;
      }

      case S_ADMIN_USER: {
        CardRec* c = store.card(sel);
        if (b == BTN_BACK) { go(S_ADMIN_USERS); }
        if (b == BTN_OK && c) {
          c->used_kwh10 = 0; store.saveCards();
          toast("Sifirlandi", c->name, S_ADMIN_USERS);
        }
        if (b == BTN_DOWN && c) {          // HAYIR = sil
          pendingDel = sel;
          askConfirm("Kart silinsin mi?", c->name, S_ADMIN_USER);
        }
        break;
      }

      case S_ADMIN_LOGS: {
        int n = store.logCount();
        if (b == BTN_UP   && sel > 0)     { sel--; clampTop(top, sel, n, LOG_ROWS); }
        if (b == BTN_DOWN && sel < n - 1) { sel++; clampTop(top, sel, n, LOG_ROWS); }
        if (b == BTN_OK && !rep) { store.clearLogs(); sel = 0; top = 0; toast("Kayitlar silindi", nullptr, S_ADMIN_MENU); }
        if (b == BTN_BACK) go(S_ADMIN_MENU);
        break;
      }

      case S_ADMIN_SETTINGS: {
        Settings& st = store.settings();
        int d = (b == BTN_UP) ? +1 : (b == BTN_DOWN ? -1 : 0);

        if (!editing) {
          if (d) { sel = constrain(sel + d, 0, 3); }
          if (b == BTN_OK)   editing = true;
          if (b == BTN_BACK) { go(S_ADMIN_MENU); }
        } else {
          if (d) {
            if (sel == 0) st.max_a = constrain(st.max_a + d, 6, 32);
            if (sel == 1) st.def_a = constrain(st.def_a + d, 6, st.max_a);
            if (sel == 2) { if (!rep) st.require_card = !st.require_card; }
            if (sel == 3) st.max_session_min = constrain((int)st.max_session_min + d * 30, 30, 1440);
            store.markSettingsDirty();
          }
          if (b == BTN_OK)   editing = false;
          if (b == BTN_BACK) editing = false;
        }
        break;
      }

      case S_ADMIN_SCHEDULE: {
        Settings& st = store.settings();
        int d = (b == BTN_UP) ? +1 : (b == BTN_DOWN ? -1 : 0);

        if (!editing) {
          if (d) sel = constrain(sel + d, 0, 2);
          if (b == BTN_OK)   editing = true;
          if (b == BTN_BACK) go(S_ADMIN_MENU);
        } else {
          if (d) {
            if (sel == 0) {
              int m = (st.sh * 60 + st.sm + d * 15 + 1440) % 1440;
              st.sh = m / 60; st.sm = m % 60;
            } else if (sel == 1) {
              int m = (st.eh * 60 + st.em + d * 15 + 1440) % 1440;
              st.eh = m / 60; st.em = m % 60;
            } else if (!rep) {
              st.schedule_en = !st.schedule_en;
            }
            store.markSettingsDirty();
          }
          if (b == BTN_OK)   editing = false;
          if (b == BTN_BACK) editing = false;
        }
        break;
      }

      case S_ADMIN_DEVICE:
        if (b == BTN_BACK || b == BTN_OK) go(S_ADMIN_MENU);
        break;

      case S_ADMIN_BTNTEST:
        if (b == BTN_BACK) go(S_ADMIN_MENU);   // GERI = cik
        break;

      case S_CONFIRM:
        if (b == BTN_UP || b == BTN_OK) {          // EVET
          if (pendingDel >= 0) {
            CardRec* c = store.card(pendingDel);
            char nm[16] = "?";
            if (c) { strncpy(nm, c->name, sizeof(nm) - 1); store.removeCard(pendingDel); }
            pendingDel = -1;
            sel = 0;
            toast("Kart silindi", nm, S_ADMIN_USERS);
          } else {
            go(confirmBack);
          }
        }
        if (b == BTN_DOWN || b == BTN_BACK) {      // HAYIR
          pendingDel = -1;
          go(confirmBack);
        }
        break;

      default: break;
    }
  }

  // --- uzun basis: sadece hizli deger degistirme (menude gerekmiyor) ---
  // EVET/HAYIR kisa basisi zaten yukari/asagi; uzun bas tekrari hizlandirir.

  // --- zaman asimlari / otomatik donus ---
  if (msgUntil && millis() > msgUntil) {
    msgUntil = 0;
    Screen back = afterMsg;
    afterMsg = S_WELCOME;
    // S_CHARGING'de userIdx korunmali: aksi halde kart bilgisi kaybolup
    // ekran karsilamaya dusuyordu.
    bool keepUser = (back == S_USER_HOME || back == S_USER_DEVICE ||
                     back == S_CHARGING);
    userIdx = keepUser ? userIdx : -1;
    go(back);
  }

  // --- YETKISIZ SARJ ENGELLEME (asla baslamasin) ---
  // Cihaz araca takilinca kendi kendine sarj baslatiyor (plug & charge).
  // Kontaktor cekildikten SONRA kapatmak hem gec kalir hem 22 kW role
  // dongusune yol acar. Bu yuzden ARAC TAKILIR TAKILMAZ (work_state =
  // charger_insert, kontaktor henuz kapali degil) cihaz park edilir:
  //   switch = false + work_mode = charge_schedule
  // Boylece cihaz hic enerji vermez. Sadece kart onayli oturum bunu acar.
  {
    static uint32_t lastCheck = 0;
    static uint32_t lastNudge = 0;
    uint32_t gnow = millis();

    if (charging) {
      // kart onayli oturum: bekci pasif
    } else if (gnow - lastCheck >= 1000) {        // 1 sn: hizli tepki
      lastCheck = gnow;

      ChargerState cs = charger.state();
      bool pluggingIn = (cs == CH_INSERT || cs == CH_WAITING || cs == CH_CHARGING);

      if (pluggingIn && !charger.parkLocked()) {
        Serial.printf("[guard] arac takildi (state=%d) -> PARK\n", (int)cs);
        charger.parkLock();               // tek komut, kontaktor kapaliyken
        lastNudge = gnow;

        if (scr == S_WELCOME) {
          toast("Kart okutun", "once yetki gerekli", S_WELCOME);
        }
      } else if (!pluggingIn && charger.parkLocked()) {
        // arac cikti: bir sonraki takilma icin kilidi sifirla
        charger.releaseParkLock();
        Serial.println("[guard] arac cikti, park kilidi kalkti");
      } else if (pluggingIn && charger.parkLocked()
                 && cs == CH_CHARGING && gnow - lastNudge > 15000) {
        // park moduna ragmen sarj ediyorsa: 15 sn'de bir tek hatirlatma
        lastNudge = gnow;
        Serial.println("[guard] park moduna ragmen sarj -> switch off");
        charger.enable(false);
      }
    }
  }

  // --- oturum sinirlari (sure + hedef kWh) ---
  if (charging) {
    Settings& st = store.settings();

    if (st.max_session_min && charger.sessionSec() >= st.max_session_min * 60UL) {
      stopCharging(false);
      toast("Sure siniri doldu", "Sarj durduruldu", S_WELCOME);
    } else if (st.target_kwh10 &&
               (charger.sessionKwh() - startKwh) >= (st.target_kwh10 / 10.0f)) {
      float used = charger.sessionKwh() - startKwh;
      stopCharging(false);
      char msg[24];
      snprintf(msg, sizeof(msg), "%.2f kWh", used > 0 ? (double)used : 0.0);
      toast("Hedef kWh doldu", msg, S_WELCOME);
    }
  }

  // --- sarj cihazi uzun sure cevap vermiyorsa oturumu kapat ---
  // Aksi halde kWh sayaci donar, kullanici "sarj ediyor" sanir.
  if (charging && charger.linkFails() >= 3 && charger.dataAgeSec() > 300) {
    float used = charger.sessionKwh() - startKwh;
    stopCharging(true);
    char msg[24];
    snprintf(msg, sizeof(msg), "%.2f kWh", used > 0 ? (double)used : 0.0);
    toast("Sarj cihazi cevap vermiyor", msg, S_WELCOME);
  }

  // --- panelden uzaktan komut (start / stop) ---
  {
    Settings& st = store.settings();
    // Uygulanan id NVS'de: elektrik kesintisinden sonra eski komut
    // yeniden calismaz (yoksa kartsiz sarj baslardi).
    if (st.cmd && st.cmd_id && st.cmd_id != store.lastCmdId()) {
      uint32_t cid = st.cmd_id;
      uint8_t  cmd = st.cmd;
      store.setLastCmdId(cid);

      if (cmd == 1) {                       // baslat (kartsiz, yonetici)
        if (!charging) {
          charger.releaseParkLock();
          charger.setCurrent(st.def_a);
          if (charger.enable(true)) {
            charging = true;
            userIdx  = -1;
            startTs  = nowEpoch();
            startKwh = charger.sessionKwh();
            go(S_CHARGING);
            Serial.printf("[sarj] panelden baslatildi (%u A, hedef %.1f kWh)\n",
                          st.def_a, st.target_kwh10 / 10.0f);
          } else {
            toast("Sarj baslatilamadi", "cihaz cevap vermiyor", S_WELCOME);
          }
        }
      } else if (cmd == 2) {                // durdur
        if (charging) {
          stopCharging(false);
          toast("Panelden durduruldu", nullptr, S_WELCOME);
          Serial.printf("[sarj] panelden durduruldu (cmd %u)\n", cid);
        } else {
          // ESP32 oturum bilmiyor ama cihaz sarj ediyor olabilir: yine kapat
          charger.enable(false);
          Serial.printf("[sarj] panelden durduruldu (yerel oturum yok, cmd %u)\n", cid);
        }
      }
    }
  }

  // --- yarim kalan oturumu periyodik kaydet (elektrik kesintisine karsi) ---
  uint32_t nowMs = millis();
  if (charging) {
    static uint32_t lastCk = 0;
    static uint16_t lastKwh = 0;
    uint16_t cur = (uint16_t)(charger.sessionKwh() * 10);
    if (nowMs - lastCk > 300000UL || (cur >= lastKwh + 5)) {   // 5 dk veya 0.5 kWh
      lastCk = nowMs;
      lastKwh = cur;
      store.updateOpenSession(cur);
    }
  }

  // --- saat: gecersizse periyodik NTP tekrar (TLS icin sart) ---
  {
    static uint32_t lastNtp = 0;
    static bool ntpDone = false;
    if (!clockValid() && (!ntpDone || millis() - lastNtp > 600000UL)) {
      ntpDone = true;
      lastNtp = millis();
      if (netUplinkOk()) { Serial.println("[ntp] saat yeniden denenecek"); clockBegin(); }
    }
  }

  // --- ertelenen NVS yazimlari ---
  store.flushIfDirty();

  // --- gunluk sayac sifirlama ---
  if (clockValid()) {
    time_t tt = time(nullptr);
    struct tm t;
    localtime_r(&tt, &t);
    if (todayDay != t.tm_mday) {
      if (todayDay != -1) todayKwh = 0;
      todayDay = t.tm_mday;
      dirty = true;
    }
  }

  // --- ekran uykusu ---
  // Sarj surerken ekran acik kalir (kullanici kW/kWh'i izliyor).
  uint32_t now = millis();
  bool mustWake = (charging || scr == S_CHARGING) && !manualSleep;
  if (mustWake && uiAsleep()) { uiWake(); dirty = true; }
  if (!mustWake && !uiAsleep() && (now - lastActivity > SCREEN_SLEEP_MS)) {
    uiSleep();
  }

  // --- ekran ---
  if (!uiAsleep() && (dirty || (now - lastDraw >= 500))) {
    lastDraw = now;
    dirty = false;
    draw();
  }

  delay(15);
}
