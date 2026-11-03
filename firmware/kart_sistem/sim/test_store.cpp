// ===========================================================================
//  store.cpp DAVRANIS TESTI (Mac'te, gercek kod uzerinde)
//
//  Dogrulananlar:
//   1. Kart ekle / bul / sil — NVS'ye yaziliyor mu
//   2. Ayar tekrarli degisimde flash yazimi sinirli mi (deferred save)
//   3. Elektrik kesintisi: acik oturum kaydi geri okunuyor mu
//   4. Log dongusu (40 kayit) dogru sirada mi
// ===========================================================================
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "Arduino.h"
#include "../src/store.h"

SerialShim Serial;
PrefStore gPrefs;
uint32_t gFakeMillis = 1000;
uint8_t  gPinState[64];
uint8_t  gPinMode[64];

static int  gFail = 0;
static int  gPass = 0;

#define CHECK(cond, msg) do { \
  if (cond) { gPass++; printf("  [OK  ] %s\n", msg); } \
  else      { gFail++; printf("  [FAIL] %s\n", msg); } \
} while (0)

static void testCards() {
  printf("\n--- 1. kart CRUD ---\n");
  store.begin();

  uint8_t a[4] = {0xD3, 0xBF, 0x4E, 0x07};
  uint8_t b[4] = {0xA7, 0x51, 0x2B, 0x64};

  CHECK(store.addCard(a, 4, "AYSE", true),  "kart eklendi");
  CHECK(store.addCard(b, 4, "AYSE", false), "ikinci kart eklendi");
  CHECK(store.cardCount() == 2,             "kart sayisi 2");
  CHECK(!store.addCard(a, 4, "DUP", false), "ayni kart tekrar eklenmiyor");

  int i = store.findCard(a, 4);
  CHECK(i >= 0 && strcmp(store.card(i)->name, "AYSE") == 0, "UID ile bulundu");
  CHECK(store.card(i)->admin == true, "admin bayragi dogru");

  // yeniden baslat: NVS'den okunuyor mu
  store.begin();
  CHECK(store.cardCount() == 2, "yeniden acilista kartlar kalici");

  CHECK(store.removeCard(0), "kart silindi");
  CHECK(store.cardCount() == 1, "silme sonrasi sayi 1");
  CHECK(store.findCard(a, 4) < 0, "silinen kart bulunamıyor");
}

static void testDeferredSave() {
  printf("\n--- 2. flash omru (ertelenen yazim) ---\n");
  Settings& st = store.settings();

  gPrefs.puts = 0;
  for (int k = 0; k < 50; k++) {          // 50 tus tekrari = 50 degisiklik
    st.def_a = 6 + (k % 20);
    store.markSettingsDirty();
    store.flushIfDirty(2000);             // sessizlik yok -> yazmaz
  }
  CHECK(gPrefs.puts == 0, "50 degisiklikte 0 flash yazimi");

  store.flushIfDirty(0);                  // sessizlik bitti -> tek yazim
  CHECK(gPrefs.puts == 1, "tek yazim yapildi");

  st.def_a = 12;
  store.markSettingsDirty();
  store.flushIfDirty(0);
  store.begin();
  CHECK(store.settings().def_a == 12, "ayar kalici");
}

static void testOpenSession() {
  printf("\n--- 3. elektrik kesintisi kurtarma ---\n");
  uint8_t uid[4] = {0xD3, 0xBF, 0x4E, 0x07};

  OpenSession o;
  CHECK(!store.openSession(&o), "baslangicta acik oturum yok");

  store.beginOpenSession(1793000000, uid, 4);
  CHECK(store.openSession(&o), "oturum baslatildi");
  CHECK(o.startTs == 1793000000, "baslangic zamani dogru");
  CHECK(o.kwh10 == 0, "kWh 0 ile basladi");

  store.updateOpenSession(37);            // 3.7 kWh
  store.begin();                          // <<< ELEKTRIK KESINTISI SIMULASYONU
  OpenSession r;
  CHECK(store.openSession(&r), "kesinti sonrasi oturum hala orada");
  CHECK(r.kwh10 == 37, "3.7 kWh korundu");
  CHECK(memcmp(r.uid, uid, 4) == 0, "kart UID korundu");

  store.clearOpenSession();
  CHECK(!store.openSession(&o), "oturum kapatildi");
}

static void testLogRing() {
  printf("\n--- 4. log dongusu ---\n");
  store.clearLogs();

  uint8_t uid[4] = {0xD3, 0xBF, 0x4E, 0x07};
  for (int k = 0; k < 45; k++) {
    LogRec r;
    memset(&r, 0, sizeof(r));
    r.ts     = 1793000000 + k * 3600;
    r.kwh10  = (uint16_t)(k * 10);
    r.len    = 4;
    memcpy(r.uid, uid, 4);
    store.addLog(r);
  }

  CHECK(store.logCount() == MAX_LOGS, "log sayisi 40'ta sabit");
  LogRec* newest = store.log(0);
  CHECK(newest && newest->kwh10 == 440, "en yeni kayit 0. indekste (44.0 kWh)");
  LogRec* oldest = store.log(MAX_LOGS - 1);
  CHECK(oldest && oldest->kwh10 == 50, "en eski kayit 5. (5.0 kWh)");

  store.begin();
  CHECK(store.logCount() == MAX_LOGS, "loglar yeniden acilista kalici");
  CHECK(store.log(0)->kwh10 == 440, "sira korundu");
}

int main() {
  printf("=== STORE TESTLERI (gercek store.cpp) ===\n");
  testCards();
  testDeferredSave();
  testOpenSession();
  testLogRing();

  printf("\n=== SONUC ===\n");
  printf("  gecti: %d   kaldi: %d\n", gPass, gFail);
  printf("  toplam flash yazimi: %d\n", gPrefs.puts);
  return gFail == 0 ? 0 : 1;
}
