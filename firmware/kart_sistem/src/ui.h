#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "store.h"
#include "charger.h"

// 128x64 ekran, 4 sayfa. Cizim fonksiyonlari sadece okur, karar vermez.

extern bool uiOk;      // OLED hatti calisiyor mu
bool uiBegin();      // OLED'i bulur, baslatir; ACK yoksa false
#ifdef HOST_SIM
void uiAttach(U8G2* d);
#endif
U8G2& disp();

// ekran uykusu: OLED panelini kapatir, cizim dongusu durur.
// Arka plan islemleri (sarj, senkron, kart okuma) etkilenmez.
void uiSleep();
void uiWake();
bool uiAsleep();

// --- ortak parcalar ---
void uiHeader(const char* title, const char* right);
void uiStatusBar(int rightX, int y);   // wifi + sarj cihazi + backend
void uiFooter(const char* txt);

// --- ekranlar ---
void uiWelcome(const String& clock, const String& date, bool online, int userCount);
void uiWaitingCard(const String& msg);
void uiUserHome(const CardRec* c, float monthKwh, float todayKwh);
void uiUserDevice(const CardRec* c);
void uiAccessDenied(const String& uid);
void uiQuotaFull(const CardRec* c);
void uiScheduleBlocked(const Settings& st);
void uiCharging(const CardRec* c);

void uiMenu(const char* title, const char* const* items, int n, int sel, int top);
void uiLogs(int sel, int top);
void uiAdminUsers(int sel, int top);
void uiUserDetail(const CardRec* c);
void uiSettings(int sel, const Settings& st, bool editing);
void uiSchedule(const Settings& st, int sel, bool editing);
void uiDeviceInfo(const Settings& st);
void uiHealth();                        // sistem sagligi: uptime/heap/kuyruk
void uiAbout();
void uiButtonTest(int pressedPin);
void uiPinPad(const char* prompt, int digits, int entered, int cur = -1);
void uiMessage(const char* l1, const char* l2, const char* l3);
void uiConfirm(const char* l1, const char* l2);

// --- yardimci ---
const char* tr(const char* s);        // "Sicaklik" gibi ASCII'ye cevirir
void        fmtKwh(char* out, size_t n, float kwh);
