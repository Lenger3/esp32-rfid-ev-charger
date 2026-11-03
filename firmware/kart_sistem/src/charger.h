#pragma once
#include <Arduino.h>

enum ChargerState : uint8_t {
  CH_FREE = 0,      // bos, arac yok
  CH_INSERT,        // arac takildi, baslatma bekliyor
  CH_WAITING,       // kart/kota kontrolu
  CH_CHARGING,
  CH_PAUSED,
  CH_DONE,
  CH_FAULT,
  CH_OFFLINE,
};

// Sarj cihazi soyutlamasi.
//   sim=true   -> sahte veri (donanim baglanmadan UI testi)
//   sim=false  -> Tuya LAN v3.5 (LynkTuya kutuphanesi)
class Charger {
 public:
  bool          begin(bool useSim);

  // Panelden gelen cihaz bilgileri (IP + local_key + device_id).
  // local_key SIR — sadece RAM'de tutulur, log'a yazilmaz.
  void          configure(const String& ip, const char* deviceId, const char* localKey);

  void          poll();                  // her loop'ta cagrilabilir

  bool          enable(bool on);         // sarji baslat/durdur  -> DP 18
  bool          setCurrent(uint8_t amps);// akim limiti          -> DP 4

  ChargerState  state();
  bool          online();
  bool          carConnected();
  uint8_t       currentA();
  float         voltage();
  float         powerKw();
  float         tempC();
  float         sessionKwh();
  uint32_t      sessionSec();
  float         totalKwh();              // cihaz omur boyu sayac
  const char*   workStateText();

  // Cihazin kendi ittigi DP guncellemesi (push, cmd 0x08)
  void          applyPush(const String& js);

  // Kartsiz sarji engelleme kilidi. true donerse cihaz "park" edildi
  // (work_mode = charge_schedule) — role donguye girmez.
  bool          parkLock();
  bool          parkLocked();
  void          releaseParkLock();       // kart onayli oturum baslarken
  int           linkFails();             // ardisik basarisiz yoklama
  uint32_t      dataAgeSec();            // son basarili okumadan beri

  void          resetSession();

 private:
  void          applyJson(const String& json);
  bool          setWorkMode(const char* mode);
  bool          bindIp();

  bool     sim = true;
  bool     chOn = false;
  uint8_t  amps = 0;
  uint32_t t0 = 0;
  float    kwhBase = 0;
  float    simKwh = 0;
  uint32_t lastPoll = 0;
  uint32_t nextTry  = 0;
  uint32_t lastData = 0;
  int      fails    = 0;
  bool     parked   = false;     // work_mode = charge_schedule ile kilitli
  uint32_t pushes   = 0;         // alinan push sayisi (teshis)

  // gercek mod
  String   devIp;
  String   devId;
  char     devKey[17] = {0};
  bool     haveCreds = false;
  void*    tuya = nullptr;              // LynkTuyaDevice<TUYA_V35>*

  // son okunan DP'ler
  String   wsText = "charger_free";
  float    pKw = 0;
  float    sessKwh = 0;
  float    totalKwhVal = 0;
  float    tempVal = 0;
  uint8_t  curSet = 0;
};

extern Charger charger;
