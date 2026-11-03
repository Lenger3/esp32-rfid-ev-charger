/*
 * RC522 (MFRC522) — PIN DOGRULAMA TESTI
 * ESP32-WROOM-32U  ·  SADECE RC522 bagli, diger moduller TAKILI DEGIL
 *
 * ---------------------------------------------------------------------------
 * BEKLENEN KABLOLAMA
 * ---------------------------------------------------------------------------
 *   RC522 SDA (SS) -> GPIO  5      CS
 *   RC522 SCK      -> GPIO 18      SPI clock
 *   RC522 MOSI     -> GPIO 23      ESP32 -> RC522
 *   RC522 MISO     -> GPIO 19      RC522 -> ESP32
 *   RC522 IRQ      -> BOS          (polling kullaniyoruz)
 *   RC522 GND      -> GND
 *   RC522 RST      -> GPIO 27
 *   RC522 3.3V     -> 3V3          !! 5 V YASAK !!
 *
 * ---------------------------------------------------------------------------
 * NE TEST EDER
 * ---------------------------------------------------------------------------
 *  1) RST piniyle donanim reset atip cevap veriyor mu
 *  2) VersionRegister (0x37) -> 0x91 veya 0x92 olmali. Bu, SPI hattinin
 *     (SCK/MOSI/MISO/CS) dogru oldugunun KANITIDIR.
 *  3) Anten acilip kapaniyor mu (TxControlReg)
 *  4) Surekli kart tarama: UID + kart tipi
 *
 * ---------------------------------------------------------------------------
 * SERI CIKTI YORUMU
 * ---------------------------------------------------------------------------
 *   version = 0x92 / 0x91        -> TUM PINLER DOGRU
 *   version = 0x00 / 0xFF        -> MISO veya CS yanlis, ya modul beslenmiyor
 *   version = 0x12 / 0x91 degil  -> SCK/MOSI karisik veya SPI hizi yuksek
 *   "RST cevap vermiyor"         -> RST kablosu / GPIO 27 kontrol
 *   "Anten acilmadi"             -> 3.3 V zayif veya modul arizali
 */

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>

#define PIN_CS    5
#define PIN_RST   27
#define PIN_SCK   18
#define PIN_MOSI  23
#define PIN_MISO  19
#define SPI_HZ    4000000UL     // RC522 max 10 MHz; 4 MHz guvenli

MFRC522 rfid(PIN_CS, PIN_RST);

byte lastUid[10];
byte lastLen = 0;
unsigned long lastCardMs = 0;

const char* cardType(byte sak) {
  switch ((sak >> 1) & 0x03) {
    case 0: return "MIFARE Classic 1K/4K";
    case 1: return "MIFARE Plus";
    case 2: return "MIFARE Ultralight / NTAG";
    default: return "ISO14443-4 (DESFire vs.)";
  }
}

void dumpRegister(const char* name, MFRC522::PCD_Register reg) {
  byte v = rfid.PCD_ReadRegister(reg);
  Serial.printf("  %-22s 0x%02X\n", name, v);
}

bool hardwareReset() {
  // RST pini ile manuel reset: RC522 cevap vermezse kurtarma yolu
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, LOW);
  delay(20);
  digitalWrite(PIN_RST, HIGH);
  delay(60);          // datasheet: ~50 ms sonra hazir
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("==================================================");
  Serial.println(" RC522 PIN DOGRULAMA");
  Serial.println("==================================================");
  Serial.printf("  CS=%d  SCK=%d  MOSI=%d  MISO=%d  RST=%d\n",
                PIN_CS, PIN_SCK, PIN_MOSI, PIN_MISO, PIN_RST);
  Serial.printf("  SPI = %lu Hz\n", (unsigned long)SPI_HZ);
  Serial.println("  IRQ baglanmiyor (polling)");
  Serial.println();

  // 1) RST ile donanim reset
  hardwareReset();

  // 2) SPI baslat
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
  rfid.PCD_Init();
  rfid.PCD_SetRegisterBitMask(MFRC522::TxControlReg, 0x03);  // antenleri ac

  // 3) SURUM KAYDI — asil kanit
  byte ver = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print("  VersionRegister (0x37) = 0x");
  Serial.println(ver, HEX);

  Serial.println();
  if (ver == 0x92 || ver == 0x91) {
    Serial.println("  >>> PINLER DOGRU. RC522 konusuyor. <<<");
  } else if (ver == 0x00 || ver == 0xFF) {
    Serial.println("  >>> HATA: cevap yok. Kontrol sirasi:");
    Serial.println("      1) 3.3 V ve GND gercekten bagli mi (5 V YASAK)");
    Serial.println("      2) MISO (GPIO 19) dogru mu — en sik hata");
    Serial.println("      3) CS (GPIO 5) dogru mu");
    Serial.println("      4) SCK/MOSI yer degistirmis olabilir");
    Serial.println("      5) Lehim soguk / dupont gevsek");
  } else {
    Serial.println("  >>> HATA: bozuk veri. SCK/MOSI karisik veya SPI hizi yuksek.");
    Serial.println("      SPI_HZ degerini 1000000 yapip tekrar dene.");
  }

  Serial.println();
  Serial.println("  --- kayit dokumu (referans) ---");
  dumpRegister("VersionReg", MFRC522::VersionReg);
  dumpRegister("TxControlReg", MFRC522::TxControlReg);
  dumpRegister("ModeReg", MFRC522::ModeReg);
  dumpRegister("Status2Reg", MFRC522::Status2Reg);
  dumpRegister("CommandReg", MFRC522::CommandReg);
  dumpRegister("TModeReg", MFRC522::TModeReg);

  byte tx = rfid.PCD_ReadRegister(MFRC522::TxControlReg);
  Serial.println();
  if ((tx & 0x03) == 0x03) {
    Serial.println("  Anten: ACIK");
    Serial.println();
    Serial.println("  Karti okuyucuya yaklastir (1-2 cm)...");
  } else {
    Serial.println("  Anten: KAPALI — TxControlReg yazilamadi, modul arizali olabilir");
  }
  Serial.println("==================================================");
  Serial.println();
}

void loop() {
  static unsigned long lastStatus = 0;

  if (!rfid.PICC_IsNewCardPresent()) {
    if (millis() - lastStatus > 5000) {     // 5 sn'de bir "bekliyorum" satiri
      lastStatus = millis();
      Serial.printf("[%6.1fs] kart bekleniyor... (version=0x%02X)\n",
                    millis() / 1000.0, rfid.PCD_ReadRegister(MFRC522::VersionReg));
    }
    return;
  }

  if (!rfid.PICC_ReadCardSerial()) {
    Serial.println("  kart goruldu ama UID okunamadi (temas zayif?)");
    return;
  }

  // ayni kart 1.5 sn icinde tekrar sayilmasin
  bool same = (rfid.uid.size == lastLen) && lastLen > 0;
  if (same) {
    for (byte i = 0; i < rfid.uid.size; i++) {
      if (rfid.uid.uidByte[i] != lastUid[i]) { same = false; break; }
    }
  }
  if (same && millis() - lastCardMs < 1500) {
    rfid.PICC_HaltA();
    return;
  }

  lastCardMs = millis();
  lastLen = rfid.uid.size;
  memcpy(lastUid, rfid.uid.uidByte, rfid.uid.size);

  Serial.println();
  Serial.print("  KART OKUNDU  UID = ");
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) Serial.print("0");
    Serial.print(rfid.uid.uidByte[i], HEX);
    if (i + 1 < rfid.uid.size) Serial.print(":");
  }
  Serial.println();

  MFRC522::PICC_Type t = rfid.PICC_GetType(rfid.uid.sak);
  Serial.printf("  tip              = %s (SAK=0x%02X)\n",
                rfid.PICC_GetTypeName(t), rfid.uid.sak);
  Serial.printf("  UID uzunlugu     = %d bayt\n", rfid.uid.size);

  // kart UID'sini sistemde kullanilacak sekilde yazdir
  Serial.print("  uid (hash girdisi) = ");
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) Serial.print("0");
    Serial.print(rfid.uid.uidByte[i], HEX);
  }
  Serial.println();
  Serial.print("  son 4 hane         = ");
  for (byte i = (rfid.uid.size > 2 ? rfid.uid.size - 2 : 0); i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) Serial.print("0");
    Serial.print(rfid.uid.uidByte[i], HEX);
  }
  Serial.println();
  Serial.println();

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}
