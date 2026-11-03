#include "store.h"
#include <string.h>

Store store;

void Store::begin() {
  prefs.begin("evcharge", false);

  // --- kartlar ---
  size_t n = prefs.getBytesLength("cards");
  if (n >= sizeof(CardRec)) {
    nCards = prefs.getBytes("cards", cards, sizeof(cards)) / sizeof(CardRec);
    if (nCards > MAX_CARDS) nCards = MAX_CARDS;
  } else {
    nCards = 0;
  }

  // --- ayarlar ---
  if (prefs.getBytesLength("set") == sizeof(Settings)) {
    prefs.getBytes("set", &st, sizeof(st));
  } else {
    resetSettings();
    prefs.putBytes("set", &st, sizeof(st));
  }

  // --- loglar ---
  if (prefs.getBytesLength("logs") == sizeof(logs)) {
    prefs.getBytes("logs", logs, sizeof(logs));
    logHead = prefs.getInt("loghead", 0);
    nLogs   = prefs.getInt("logn", 0);
    if (logHead < 0 || logHead >= MAX_LOGS) logHead = 0;
    if (nLogs < 0 || nLogs > MAX_LOGS) nLogs = 0;
  }

  Serial.printf("[store] kart=%d log=%d kota_ay=%u\n", nCards, nLogs, st.max_session_min);
}

bool Store::isFirstBoot() { return nCards == 0; }

// ------------------------------------------------------------ sunucu senkronu
uint32_t Store::configRev() {
  if (rev == 0) rev = (uint32_t)prefs.getUInt("rev", 0);
  return rev;
}

void Store::applyServer(const CardRec* list, int n, const Settings& srv, uint32_t newRev) {
  if (n < 0) n = 0;
  if (n > MAX_CARDS) n = MAX_CARDS;

  // kartlari kopyala (yerel kullanim kWh'sini koru: sunucu zaten dogru degeri gonderir)
  memset(cards, 0, sizeof(cards));
  for (int i = 0; i < n; i++) cards[i] = list[i];
  nCards = n;
  saveCards();

  st = srv;
  saveSettings();

  rev = newRev;
  prefs.putUInt("rev", rev);
}

uint32_t Store::lastCmdId() {
  return prefs.getUInt("cmdId", 0);
}

void Store::setLastCmdId(uint32_t v) {
  prefs.putUInt("cmdId", v);
}

// ------------------------------------------------------------ kartlar
int Store::cardCount() { return nCards; }

CardRec* Store::card(int i) {
  if (i < 0 || i >= nCards) return nullptr;
  return &cards[i];
}

int Store::findCard(const uint8_t* uid, uint8_t len) {
  for (int i = 0; i < nCards; i++) {
    if (cards[i].len != len) continue;
    if (memcmp(cards[i].uid, uid, len) == 0) return i;
  }
  return -1;
}

bool Store::addCard(const uint8_t* uid, uint8_t len, const char* name, bool admin) {
  if (nCards >= MAX_CARDS || len > 7) return false;
  if (findCard(uid, len) >= 0) return false;
  CardRec& c = cards[nCards++];
  memset(&c, 0, sizeof(c));
  memcpy(c.uid, uid, len);
  c.len     = len;
  c.admin   = admin;
  c.active  = true;
  strncpy(c.name, name, sizeof(c.name) - 1);
  saveCards();
  return true;
}

bool Store::removeCard(int i) {
  if (i < 0 || i >= nCards) return false;
  for (int k = i; k < nCards - 1; k++) cards[k] = cards[k + 1];
  nCards--;
  saveCards();
  return true;
}

void Store::saveCards() {
  prefs.putBytes("cards", cards, nCards * sizeof(CardRec));
  cardsDirty = false;
}

// --- yazmayi ertele: 140 ms'de bir tekrar eden butonda flash'i yakma ---
void Store::markSettingsDirty() { settingsDirty = true; lastChange = millis(); }
void Store::markCardsDirty()    { cardsDirty    = true; lastChange = millis(); }

void Store::flushIfDirty(uint32_t quietMs) {
  if ((!settingsDirty && !cardsDirty) || (millis() - lastChange) < quietMs) return;
  if (settingsDirty) { saveSettings(); }
  if (cardsDirty)    { saveCards(); }
}

// ------------------------------------------------------------ acik oturum
bool Store::openSession(OpenSession* out) {
  if (prefs.getBytesLength("open") != sizeof(OpenSession)) return false;
  OpenSession o;
  prefs.getBytes("open", &o, sizeof(o));
  if (o.magic != 0x45564331 || !o.valid) return false;
  *out = o;
  return true;
}

void Store::beginOpenSession(uint32_t startTs, const uint8_t* uid, uint8_t len) {
  OpenSession o;
  memset(&o, 0, sizeof(o));
  o.magic   = 0x45564331;
  o.startTs = startTs;
  o.len     = len > 7 ? 7 : len;
  if (uid) memcpy(o.uid, uid, o.len);
  o.kwh10   = 0;
  o.valid   = 1;
  prefs.putBytes("open", &o, sizeof(o));
}

void Store::updateOpenSession(uint16_t kwh10) {
  OpenSession o;
  if (!openSession(&o)) return;
  if (o.kwh10 == kwh10) return;
  o.kwh10 = kwh10;
  prefs.putBytes("open", &o, sizeof(o));
}

void Store::clearOpenSession() {
  OpenSession o;
  memset(&o, 0, sizeof(o));
  prefs.putBytes("open", &o, sizeof(o));
}

// ------------------------------------------------------------ ayarlar
Settings& Store::settings() { return st; }

void Store::saveSettings() {
  prefs.putBytes("set", &st, sizeof(st));
  settingsDirty = false;
}

void Store::resetSettings() {
  memset(&st, 0, sizeof(st));
  st.max_a           = 16;
  st.def_a           = 10;
  st.schedule_en     = false;
  st.sh = 22; st.sm = 0;
  st.eh = 6;  st.em = 0;
  st.days            = 0x7F;          // her gun
  st.max_session_min = 480;           // 8 saat
  st.require_card    = true;
}

// ------------------------------------------------------------ loglar
int Store::logCount() { return nLogs; }

LogRec* Store::log(int i) {
  if (i < 0 || i >= nLogs) return nullptr;
  int idx = (logHead - 1 - i + MAX_LOGS * 2) % MAX_LOGS;
  return &logs[idx];
}

void Store::addLog(const LogRec& r) {
  logs[logHead] = r;
  logHead = (logHead + 1) % MAX_LOGS;
  if (nLogs < MAX_LOGS) nLogs++;
  prefs.putBytes("logs", logs, sizeof(logs));
  prefs.putInt("loghead", logHead);
  prefs.putInt("logn", nLogs);
}

void Store::clearLogs() {
  nLogs = 0;
  logHead = 0;
  prefs.putBytes("logs", logs, sizeof(logs));
  prefs.putInt("loghead", 0);
  prefs.putInt("logn", 0);
}
