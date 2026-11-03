#pragma once
#include <Arduino.h>
#include <Preferences.h>

#define MAX_CARDS   24
#define MAX_LOGS    40

struct CardRec {
  uint8_t  uid[7];
  uint8_t  len;
  char     name[16];
  bool     admin;
  bool     active;
  uint16_t quota_kwh;     // aylik kota, 0 = sinirsiz
  uint16_t used_kwh10;    // bu ay kullanim x10 (42.1 kWh -> 421)
  uint32_t last_ts;       // son kullanim
};

struct LogRec {
  uint32_t ts;
  uint8_t  uid[7];
  uint8_t  len;
  uint16_t kwh10;
  uint8_t  reason;        // 0 kart, 1 kota, 2 sure, 3 admin, 4 iptal
};

// Acik oturum kaydi — elektrik kesilirse kurtarmak icin
struct OpenSession {
  uint32_t magic;        // 0x45564331
  uint32_t startTs;
  uint8_t  uid[7];
  uint8_t  len;
  uint16_t kwh10;
  uint8_t  valid;
};

struct Settings {
  uint8_t  max_a;             // donanim ust sinir
  uint8_t  def_a;             // varsayilan akim
  bool     schedule_en;       // zamanlamali sarj
  uint8_t  sh, sm, eh, em;    // baslangic / bitis
  uint8_t  days;              // bit0=Pzt .. bit6=Paz
  uint16_t max_session_min;
  bool     require_card;      // kart zorunlu
  uint16_t target_kwh10;      // oturum hedefi x10, 0 = sinirsiz
  uint8_t  cmd;               // 0 yok, 1 start, 2 stop
  uint32_t cmd_id;            // ayni komutu iki kez uygulamamak icin
};

class Store {
 public:
  void begin();

  // --- kartlar ---
  int      cardCount();
  CardRec* card(int i);
  int      findCard(const uint8_t* uid, uint8_t len);
  bool     addCard(const uint8_t* uid, uint8_t len, const char* name, bool admin);
  bool     removeCard(int i);
  void     saveCards();

  // --- ayarlar ---
  Settings& settings();
  void      saveSettings();
  void      resetSettings();

  // Flash omru korumasi: sik degisen ayarlar buraya, sessizlikte tek yazim
  void      markSettingsDirty();
  void      markCardsDirty();
  void      flushIfDirty(uint32_t quietMs = 2000);

  // Oturum kurtarma
  bool      openSession(OpenSession* out);
  void      beginOpenSession(uint32_t startTs, const uint8_t* uid, uint8_t len);
  void      updateOpenSession(uint16_t kwh10);
  void      clearOpenSession();

  // --- loglar (0 = en yeni) ---
  int      logCount();
  LogRec*  log(int i);
  void     addLog(const LogRec& r);
  void     clearLogs();

  bool     isFirstBoot();

  // --- sunucu senkronu ---
  uint32_t configRev();                       // NVS'deki son uygulanan rev

  // Uygulanan son uzaktan komut id'si. NVS'de tutulur ki elektrik
  // kesintisinden sonra eski "start"/"stop" yeniden calismasin.
  uint32_t lastCmdId();
  void     setLastCmdId(uint32_t v);
  void     applyServer(const CardRec* list, int n, const Settings& srv, uint32_t rev);
  // sunucudan gelen kart listesiyle yerel listeyi degistirir (fail-closed onbellek)

 private:
  Preferences prefs;
  CardRec  cards[MAX_CARDS];
  int      nCards = 0;
  LogRec   logs[MAX_LOGS];
  int      nLogs = 0;
  int      logHead = 0;      // en yeni kaydin yeri
  uint32_t rev           = 0;
  bool     settingsDirty = false;
  bool     cardsDirty    = false;
  uint32_t lastChange    = 0;
  Settings st;
};

extern Store store;
