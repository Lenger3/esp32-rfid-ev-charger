/*
 * ===========================================================================
 *  KART OKU -> EKRANA YAZ
 *  ESP32-WROOM-32U · M242-12864 OLED (I2C) + RC522
 * ===========================================================================
 *
 *  ONEMLI: OLED baslatma kodu, CALISAN testin (oled_i2c_test) BIREBIR aynisi.
 *          Ayni sira, ayni hiz (400 kHz), ayni tarama, ayni begin zamani.
 *          Sadece ekrana cizilen icerik degisti (sayfalar yerine kart bilgisi).
 *
 *  KABLOLAMA
 *    OLED  SCL=22  SDA=21  VCC=3V3  GND   (RES=3V3, A0=GND, CS=GND)
 *    RC522 CS=5  SCK=18  MOSI=23  MISO=19  RST=27  3V3
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <MFRC522.h>

#define PIN_SDA      21
#define PIN_SCL      22
#define PIN_RFID_CS   5
#define PIN_RFID_RST 27
#define PIN_SCK      18
#define PIN_MOSI     23
#define PIN_MISO     19

U8G2_SSD1309_128X64_NONAME0_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
MFRC522 rfid(PIN_RFID_CS, PIN_RFID_RST);

bool i2cSwapped = false;   // true = SDA/SCL fiziksel olarak ters bagli
int  okumaSayisi = 0;
char sonUid[20]  = "--";
char sonTip[20]  = "--";

// ======================================================= OLED (test koduyla AYNI)
byte i2cScan(const char* tag) {
  Serial.printf("=== I2C TARAMASI (%s) ===\n", tag);
  byte found = 0, oledA = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  bulundu: 0x%02X", addr);
      if (addr == 0x3C || addr == 0x3D) {
        Serial.print("   <-- SSD1309");
        if (!oledA) oledA = addr;
      }
      Serial.println();
      found++;
    }
  }
  if (found == 0) Serial.println("  hicbir cihaz cevap vermedi");
  else Serial.printf("  toplam %d cihaz\n", found);
  Serial.println("================================");
  return oledA;
}

byte scanBothOrders() {
  byte oledA = 0;

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
  delay(30);
  oledA = i2cScan("SDA=21 SCL=22");

  if (!oledA) {
    Serial.println();
    Serial.println("  >> Cevap yok. SDA/SCL yer degistirmis olabilir, takas deneniyor...");
    Wire.end();
    delay(20);
    Wire.begin(PIN_SCL, PIN_SDA);      // TAKAS
    Wire.setClock(100000);
    delay(30);
    oledA = i2cScan("SDA=22 SCL=21 (takas)");
    if (oledA) {
      Serial.println();
      Serial.println("  >>> IKINCI taramada bulundu (ilk tarama stuck-bus).");
      Serial.println("      Kablolar ters olmayabilir; begin sonrasi test edilecek.");
      Serial.println();
      Wire.end();
      delay(20);
      Wire.begin(PIN_SDA, PIN_SCL);    // normale don, dogrulama asagida
      Wire.setClock(100000);
      delay(30);
    } else {
      Wire.end();
      delay(20);
      Wire.begin(PIN_SDA, PIN_SCL);
    }
  }
  return oledA;
}

// u8g2'nin kullanacagi sirayi uygula (tarama sonrasi)
void applyI2C(bool swapped) {
  Wire.end();
  delay(10);
  if (swapped) Wire.begin(PIN_SCL, PIN_SDA);
  else         Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
  delay(30);
}

// ======================================================= EKRAN
void ekranCiz() {
  oled.clearBuffer();

  oled.setFont(u8g2_font_6x10_tf);
  oled.drawBox(0, 0, 128, 12);
  oled.setDrawColor(0);
  oled.drawStr(3, 9, okumaSayisi ? "KART OKUNDU" : "KART OKUT");
  oled.setDrawColor(1);

  if (okumaSayisi == 0) {
    oled.setFont(u8g2_font_6x10_tf);
    oled.drawStr(18, 34, "karti yaklastir");
    oled.drawStr(40, 50, "1-2 cm");
  } else {
    oled.setFont(u8g2_font_helvB12_tf);
    int w = oled.getStrWidth(sonUid);
    oled.drawStr((128 - w) / 2, 34, sonUid);

    oled.setFont(u8g2_font_6x10_tf);
    int w2 = oled.getStrWidth(sonTip);
    oled.drawStr((128 - w2) / 2, 48, sonTip);

    char b[16];
    snprintf(b, sizeof(b), "okuma: %d", okumaSayisi);
    oled.drawStr(3, 62, b);
  }
  oled.sendBuffer();
}

// ======================================================= KART
void kartOku() {
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  snprintf(sonUid, sizeof(sonUid), "%02X:%02X:%02X:%02X",
           rfid.uid.size > 0 ? rfid.uid.uidByte[0] : 0,
           rfid.uid.size > 1 ? rfid.uid.uidByte[1] : 0,
           rfid.uid.size > 2 ? rfid.uid.uidByte[2] : 0,
           rfid.uid.size > 3 ? rfid.uid.uidByte[3] : 0);

  String tip = rfid.PICC_GetTypeName(rfid.PICC_GetType(rfid.uid.sak));
  if (tip.indexOf("MIFARE 1KB") >= 0)       strcpy(sonTip, "MIFARE 1KB");
  else if (tip.indexOf("MIFARE") >= 0)      strcpy(sonTip, "MIFARE");
  else if (tip.indexOf("Ultralight") >= 0)  strcpy(sonTip, "Ultralight");
  else                                      snprintf(sonTip, sizeof(sonTip), "%.19s", tip.c_str());

  okumaSayisi++;
  Serial.printf("KART %d: %s  (%s)\n", okumaSayisi, sonUid, sonTip);

  ekranCiz();
  delay(400);

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// ======================================================= ANA (test sirasi AYNI)
byte OLED_ADDR = 0x3C;

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n\n=== KART OKU -> EKRAN ===");
  Serial.printf("SDA=%d  SCL=%d  RES=yok (3V3'e bagli)\n", PIN_SDA, PIN_SCL);

  byte a = scanBothOrders();
  if (a) {
    OLED_ADDR = a;
    Serial.printf("  OLED adresi kullanilacak: 0x%02X\n", OLED_ADDR);
  } else {
    Serial.println();
    Serial.println("  !! EKRAN BULUNAMADI — u8g2 yine de denenecek (0x3C)");
    Serial.println("     Kontrol: VCC=3V3, GND, SCL=22, SDA=21");
    Serial.println("              RES->3V3, A0(DC)->GND, CS->GND");
    Serial.println("              R4 + R9 takili mi, R5 sokuldu mu");
    Serial.println();
  }

  Wire.end();
  delay(20);
  Wire.begin(PIN_SDA, PIN_SCL);          // once NORMAL sira
  Wire.setClock(100000);
  delay(20);
  oled.setI2CAddress(OLED_ADDR << 1);    // 8-bit adres, begin'den ONCE
  oled.begin();
  oled.setBusClock(400000);
  oled.setContrast(200);
  oled.setPowerSave(0);

  // begin sonrasi hat canli mi?
  Wire.beginTransmission(OLED_ADDR);
  bool canli = (Wire.endTransmission() == 0);

  if (!canli) {
    Serial.println("  normal sirada NAK -> sira cevrilip tekrar denenecek");
    Wire.end();
    delay(20);
    Wire.begin(PIN_SCL, PIN_SDA);
    Wire.setClock(100000);
    delay(20);
    oled.setI2CAddress(OLED_ADDR << 1);
    oled.begin();
    oled.setBusClock(400000);
    oled.setContrast(200);
    oled.setPowerSave(0);
    Wire.beginTransmission(OLED_ADDR);
    canli = (Wire.endTransmission() == 0);
    if (canli) {
      i2cSwapped = true;
      Serial.println("  >>> TAKAS sirada calisti — kablolar ters bagli");
    }
  } else {
    Serial.println("  normal sirada ACK — kablolar dogru");
  }

  Serial.printf("u8g2 begin() tamam (hat: %s)\n", canli ? "ACK" : "NAK");
  Serial.println();

  ekranCiz();

  // ---- RC522 ----
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_RFID_CS);
  rfid.PCD_Init();
  rfid.PCD_SetRegisterBitMask(MFRC522::TxControlReg, 0x03);

  byte ver = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  byte tx  = rfid.PCD_ReadRegister(MFRC522::TxControlReg);
  Serial.printf("RC522 version=0x%02X  anten=%s\n", ver,
                (tx & 0x03) == 0x03 ? "ACIK" : "KAPALI");
  Serial.println("kart bekleniyor...");
}

void loop() {
  kartOku();
  delay(80);
}
