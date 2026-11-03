/*
 * ===========================================================================
 *  ENTEGRASYON TESTI — OLED + RC522 + 4 BUTON
 *  ESP32-WROOM-32U  ·  EVCHARGE-01 kartli sarj kontrolu
 * ===========================================================================
 *
 *  Bu test 8 asamada her seyi sirayla dogrular ve ekranda rapor eder.
 *  Tuya TCP bu testte YOK (ayri test: tuya_local_client).
 *
 *  KABLOLAMA (plaket semasindan)
 *  -------------------------------------------------------------------------
 *   OLED (I2C)    SCL=G22  SDA=G21  RES=3V3  A0(DC)=GND  CS=GND  VCC=3V3
 *   RC522 (SPI)   CS=G5    SCK=G18  MOSI=G23  MISO=G19  RST=G27  3V3
 *   BUTON         UP=G32   DOWN=G33  BACK=G26  OK=G25   (diger bacak GND)
 *
 *  BEKLENEN SERI CIKTI (ozet)
 *  -------------------------------------------------------------------------
 *   [OK]  I2C taramasi        0x3C
 *   [OK]  OLED cizim          5 sayfa
 *   [OK]  SPI / RC522         0x82 (klon) veya 0x92
 *   [OK]  Anten               acik
 *   [OK]  GPIO buton          G32 G33 G26 G25 hepsi LOW/OPEN
 *   [OK]  Buton testi         4/4 basildi
 *   [OK]  Kart okuma          UID ...
 *   [OK]  Kararli calisma     60 sn, 0 reset
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <MFRC522.h>
#include "oled_init.h"

// ---------------------------------------------------------------- pinler
#define PIN_SDA    21
#define PIN_SCL    22

#define PIN_RFID_CS   5
#define PIN_RFID_RST  27
#define PIN_SCK       18
#define PIN_MOSI      23
#define PIN_MISO      19

#define PIN_BTN_UP    32
#define PIN_BTN_DOWN  33
#define PIN_BTN_BACK  26
#define PIN_BTN_OK    25

// ---------------------------------------------------------------- moduller
U8G2_SSD1309_128X64_NONAME0_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
MFRC522 rfid(PIN_RFID_CS, PIN_RFID_RST);
byte OLED_ADDR = 0x3C;
bool oledReady = false;

// ---------------------------------------------------------------- durum
struct Result { bool ok; char msg[24]; };
Result rI2C   = {false, "-"};
Result rOLED  = {false, "-"};
Result rSPI   = {false, "-"};
Result rAnt   = {false, "-"};
Result rGPIO  = {false, "-"};
Result rBtn   = {false, "0/4"};
Result rCard  = {false, "-"};
Result rSoak  = {false, "0 sn"};

const char* btnName(uint8_t pin) {
  switch (pin) {
    case PIN_BTN_UP:   return "UP";
    case PIN_BTN_DOWN: return "DOWN";
    case PIN_BTN_BACK: return "BACK";
    case PIN_BTN_OK:   return "OK";
  }
  return "?";
}

bool btnPressed[4] = {false, false, false, false};
const uint8_t btnPins[4] = {PIN_BTN_UP, PIN_BTN_DOWN, PIN_BTN_BACK, PIN_BTN_OK};

// ================================================================ YARDIMCI
void serHeader(const char* t) {
  Serial.println();
  Serial.println("==================================================");
  Serial.printf("  %s\n", t);
  Serial.println("==================================================");
}

void serResult(const Result& r, const char* what, const char* extra = "") {
  Serial.printf("  [%s] %-22s %-10s %s\n", r.ok ? "OK  " : "FAIL", what, r.msg, extra);
}

// ekran: ust bar + 2 sutun x 4 satir durum (128x64'e tam oturur)
void drawStatus(const char* headline) {
  if (!oledReady) return;
  oled.clearBuffer();

  oled.setFont(u8g2_font_6x10_tf);
  oled.drawBox(0, 0, 128, 12);
  oled.setDrawColor(0);
  oled.drawStr(2, 9, headline);
  oled.setDrawColor(1);

  const Result* cells[8] = {&rI2C, &rOLED, &rSPI, &rAnt,
                            &rGPIO, &rBtn, &rCard, &rSoak};
  const char* names[8] = {"I2C", "OLED", "SPI", "ANT",
                          "GPIO", "BTN", "KART", "SOAK"};

  oled.setFont(u8g2_font_5x7_tf);
  for (int i = 0; i < 8; i++) {
    int col = i / 4;          // 0 = sol, 1 = sag
    int row = i % 4;
    int x = 2 + col * 64;
    int y = 22 + row * 12;    // 22, 34, 46, 58  -> hepsi 64'un icinde
    oled.drawStr(x, y, cells[i]->ok ? "+" : "x");
    oled.drawStr(x + 6, y, names[i]);
    oled.drawStr(x + 30, y, cells[i]->msg);
  }
  oled.sendBuffer();
}

// ================================================================ TESTLER
// I2C hattini kurtar: SCL'ye 9 darbe + STOP kosulu
void busRecover() {
  Wire.end();
  delay(5);
  pinMode(PIN_SCL, OUTPUT);
  pinMode(PIN_SDA, INPUT_PULLUP);
  for (int i = 0; i < 9; i++) {
    digitalWrite(PIN_SCL, LOW);  delayMicroseconds(6);
    digitalWrite(PIN_SCL, HIGH); delayMicroseconds(6);
  }
  pinMode(PIN_SDA, OUTPUT);
  digitalWrite(PIN_SDA, LOW);  delayMicroseconds(6);
  digitalWrite(PIN_SCL, HIGH); delayMicroseconds(6);
  digitalWrite(PIN_SDA, HIGH); delayMicroseconds(6);
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
  delay(40);
}

// Tek seferlik ACK denemesi
byte probe(byte addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission();
}

// Hat bosta mi (kisa devre / ters baglanti teshisi)
void printIdleLevels() {
  pinMode(PIN_SDA, INPUT_PULLUP);
  pinMode(PIN_SCL, INPUT_PULLUP);
  delay(15);
  int a = digitalRead(PIN_SDA), b = digitalRead(PIN_SCL);
  Serial.printf("  hat bosta: SDA=%s SCL=%s", a ? "HIGH" : "LOW", b ? "HIGH" : "LOW");
  if (!a || !b) Serial.print("   <<< SORUN: hat LOW'da kalmis (kisa devre/ters baglanti)");
  Serial.println();
}

byte testI2C() {
  serHeader("1) I2C TARAMASI (OLED)");

  byte addr = oledBringUp(oled, PIN_SDA, PIN_SCL, true);

  if (!addr) {
    Serial.println();
    Serial.println("  HIC CIHAZ YOK — fiziksel kontrol listesi:");
    Serial.println("    1) CS  -> GND    (I2C modunda CS LOW olmali, EN SIK SEBEP)");
    Serial.println("    2) A0(DC) -> GND (adres 0x3C icin)");
    Serial.println("    3) RES -> 3V3");
    Serial.println("    4) VCC -> 3V3 (3.2-3.4 V olcu)");
    Serial.println("    5) SDA=21 SCL=22, kablo <15 cm");
    Serial.println("    6) SDA/SCL'ye 4.7k pull-up (en etkili cozum)");
    rI2C.ok = false;
    snprintf(rI2C.msg, sizeof(rI2C.msg), "YOK");
    return 0;
  }

  OLED_ADDR = addr;
  rI2C.ok = true;
  snprintf(rI2C.msg, sizeof(rI2C.msg), "0x%02X", addr);
  return addr;
}

void testOLED() {
  serHeader("2) OLED CIZIM");
  // oledBringUp() begin + kontrast + test yazimini zaten yapti
  oledReady = true;

  oled.clearBuffer();
  oled.setFont(u8g2_font_helvB10_tf);
  oled.drawStr(0, 14, "MERHABA");
  oled.setFont(u8g2_font_6x10_tf);
  oled.drawStr(0, 30, "test basliyor...");
  oled.sendBuffer();
  delay(900);

  // 4 kose testi: cizim alaninin tamamini kullanabiliyor mu
  oled.clearBuffer();
  oled.drawFrame(0, 0, 128, 64);
  oled.drawStr(4, 12, "TL");
  oled.drawStr(112, 12, "TR");
  oled.drawStr(4, 60, "BL");
  oled.drawStr(112, 60, "BR");
  oled.sendBuffer();
  delay(900);

  // yazdiktan sonra hat hala canli mi?
  Wire.beginTransmission(OLED_ADDR);
  bool alive = (Wire.endTransmission() == 0);
  Serial.printf("  yazim sonrasi hat: %s\n", alive ? "ACK (saglam)" : "NAK (kilitlendi)");

  rOLED.ok = alive;
  oledReady = true;
  snprintf(rOLED.msg, sizeof(rOLED.msg), alive ? "OK" : "BUS NK");
  Serial.println("  4 kose + baslik cizildi -> ekranda kontrol et");
}

void testSPI() {
  serHeader("3) SPI / RC522");
  pinMode(PIN_RFID_RST, OUTPUT);
  digitalWrite(PIN_RFID_RST, LOW);
  delay(20);
  digitalWrite(PIN_RFID_RST, HIGH);
  delay(60);

  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_RFID_CS);
  rfid.PCD_Init();
  rfid.PCD_SetRegisterBitMask(MFRC522::TxControlReg, 0x03);

  byte ver = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.printf("  VersionRegister = 0x%02X\n", ver);

  if (ver == 0x92 || ver == 0x91) {
    Serial.println("  orijinal NXP MFRC522");
    rSPI.ok = true;
  } else if (ver == 0x82 || ver == 0x88 || ver == 0x8C || ver == 0xB2) {
    Serial.println("  KLON cip (Fudan FM17522 / CS8522) — UID okuma icin sorun degil");
    rSPI.ok = true;
  } else if (ver == 0x00 || ver == 0xFF) {
    Serial.println("  cevap yok — MISO/CS/besleme kontrol");
    rSPI.ok = false;
  } else {
    Serial.println("  beklenmeyen deger — SCK/MOSI veya SPI hizi");
    rSPI.ok = false;
  }
  snprintf(rSPI.msg, sizeof(rSPI.msg), "0x%02X", ver);

  byte tx = rfid.PCD_ReadRegister(MFRC522::TxControlReg);
  rAnt.ok = ((tx & 0x03) == 0x03);
  snprintf(rAnt.msg, sizeof(rAnt.msg), rAnt.ok ? "ON" : "OFF");
  Serial.printf("  TxControlReg = 0x%02X -> anten %s\n", tx, rAnt.ok ? "ACIK" : "KAPALI");
}

void testGPIO() {
  serHeader("4) BUTON GPIO");
  for (int i = 0; i < 4; i++) {
    pinMode(btnPins[i], INPUT_PULLUP);
  }
  delay(10);

  int low = 0;
  for (int i = 0; i < 4; i++) {
    int v = digitalRead(btnPins[i]);
    Serial.printf("  %-5s GPIO %-3d = %s\n", btnName(btnPins[i]), btnPins[i],
                  v == HIGH ? "HIGH (bosta, dogru)" : "LOW (basili veya kisa devre!)");
    if (v == LOW) low++;
  }
  rGPIO.ok = (low == 0);
  snprintf(rGPIO.msg, sizeof(rGPIO.msg), low == 0 ? "4/4" : "%d KISA", low);
}

void testButtons() {
  serHeader("5) BUTON BASMA TESTI");
  Serial.println("  Her butona sirayla bas. 4/4 tamamlaninca devam eder.");
  Serial.println("  (20 sn icinde bitmezse atlanir)");
  Serial.println();

  if (oledReady) {
    oled.clearBuffer();
    oled.setFont(u8g2_font_6x10_tf);
    oled.drawStr(2, 12, "BUTON TESTI");
    oled.drawStr(2, 26, "UP DOWN BACK OK");
    oled.drawStr(2, 42, "sirayla bas");
    oled.sendBuffer();
  }

  bool seen[4] = {false, false, false, false};
  int done = 0;
  unsigned long t0 = millis();

  while (done < 4 && millis() - t0 < 20000) {
    for (int i = 0; i < 4; i++) {
      if (seen[i]) continue;
      if (digitalRead(btnPins[i]) == LOW) {
        delay(35);                                  // debounce
        if (digitalRead(btnPins[i]) == LOW) {
          seen[i] = true;
          done++;
          Serial.printf("  [%d/4] %s (GPIO %d) OK\n", done, btnName(btnPins[i]), btnPins[i]);
          if (oledReady) {
            oled.setFont(u8g2_font_6x10_tf);
            oled.drawStr(2 + i * 30, 56, "+");
            oled.sendBuffer();
          }
          while (digitalRead(btnPins[i]) == LOW) delay(10);   // birakmasini bekle
          delay(30);
        }
      }
    }
  }

  rBtn.ok = (done == 4);
  snprintf(rBtn.msg, sizeof(rBtn.msg), "%d/4", done);
  if (done < 4) {
    Serial.printf("  EKSIK: %d buton basilamadi\n", 4 - done);
    for (int i = 0; i < 4; i++) {
      if (!seen[i]) Serial.printf("    - %s (GPIO %d)\n", btnName(btnPins[i]), btnPins[i]);
    }
  }
}

void testCard() {
  serHeader("6) KART OKUMA");
  Serial.println("  Karti okuyucuya yaklastir (1-2 cm). 15 sn bekler.");

  if (oledReady) {
    oled.clearBuffer();
    oled.setFont(u8g2_font_6x10_tf);
    oled.drawStr(2, 12, "KART OKUTUN");
    oled.sendBuffer();
  }

  bool got = false;
  unsigned long t0 = millis();
  while (!got && millis() - t0 < 15000) {
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      got = true;
      Serial.print("  UID = ");
      for (byte i = 0; i < rfid.uid.size; i++) {
        if (rfid.uid.uidByte[i] < 0x10) Serial.print("0");
        Serial.print(rfid.uid.uidByte[i], HEX);
      }
      Serial.printf("  (%s, %d bayt)\n",
                    rfid.PICC_GetTypeName(rfid.PICC_GetType(rfid.uid.sak)),
                    rfid.uid.size);

      snprintf(rCard.msg, sizeof(rCard.msg), "%02X%02X",
               rfid.uid.size > 0 ? rfid.uid.uidByte[0] : 0,
               rfid.uid.size > 1 ? rfid.uid.uidByte[1] : 0);
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();

      if (oledReady) {
        oled.clearBuffer();
        oled.setFont(u8g2_font_6x10_tf);
        oled.drawStr(2, 12, "KART OK");
        char u[20];
        snprintf(u, sizeof(u), "%02X%02X%02X%02X",
                 rfid.uid.size > 0 ? rfid.uid.uidByte[0] : 0,
                 rfid.uid.size > 1 ? rfid.uid.uidByte[1] : 0,
                 rfid.uid.size > 2 ? rfid.uid.uidByte[2] : 0,
                 rfid.uid.size > 3 ? rfid.uid.uidByte[3] : 0);
        oled.drawStr(2, 28, u);
        oled.sendBuffer();
      }
      delay(1200);
    }
  }

  rCard.ok = got;
  if (!got) {
    snprintf(rCard.msg, sizeof(rCard.msg), "yok");
    Serial.println("  kart okunamadi (15 sn)");
  }
}

void testSoak() {
  serHeader("7) KARARLILIK (60 sn)");
  Serial.println("  OLED her 2 sn yenileniyor, kart surekli taranıyor.");
  Serial.printf("  baslangic heap = %u bayt\n", ESP.getFreeHeap());

  unsigned long t0 = millis();
  unsigned long lastOled = 0, lastHeap = 0;
  while (millis() - t0 < 60000) {
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      rfid.PICC_HaltA();
    }
    if (millis() - lastOled > 2000) {
      lastOled = millis();
      snprintf(rSoak.msg, sizeof(rSoak.msg), "%lusn", (millis() - t0) / 1000);
      drawStatus("ENTEGRASYON TESTI");
    }
    if (millis() - lastHeap > 15000) {
      lastHeap = millis();
      Serial.printf("  [%2lu sn] heap=%u\n", (millis() - t0) / 1000, ESP.getFreeHeap());
    }
    delay(50);
  }

  uint32_t heapEnd = ESP.getFreeHeap();
  Serial.printf("  bitis heap = %u bayt\n", heapEnd);
  rSoak.ok = true;
  snprintf(rSoak.msg, sizeof(rSoak.msg), "60sn");
}

void printSummary() {
  serHeader("SONUC");
  int pass = 0;
  Result* all[8] = {&rI2C, &rOLED, &rSPI, &rAnt, &rGPIO, &rBtn, &rCard, &rSoak};
  const char* names[8] = {"I2C tarama", "OLED cizim", "SPI / RC522", "Anten",
                          "Buton GPIO", "Buton basma", "Kart okuma", "60 sn soak"};
  for (int i = 0; i < 8; i++) {
    Serial.printf("  [%s] %-14s %s\n", all[i]->ok ? "OK  " : "FAIL", names[i], all[i]->msg);
    if (all[i]->ok) pass++;
  }
  Serial.println();
  Serial.printf("  %d / 8 GECTI %s\n", pass, pass == 8 ? ">>> TAMAM <<<" : "");
  Serial.println();

  // ozet ekran
  if (!oledReady) return;
  oled.clearBuffer();
  oled.setFont(u8g2_font_helvB10_tf);
  char buf[16];
  snprintf(buf, sizeof(buf), "%d/8 GECTI", pass);
  oled.drawStr(0, 14, buf);
  oled.setFont(u8g2_font_6x10_tf);
  oled.drawStr(0, 30, "kart okutarak");
  oled.drawStr(0, 42, "sonucu gorebilirsin");
  oled.sendBuffer();
}

// ================================================================ ANA
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("##################################################");
  Serial.println("#   ENTEGRASYON TESTI — OLED + RC522 + BUTON      #");
  Serial.println("##################################################");
  Serial.printf("  OLED  SCL=%d SDA=%d\n", PIN_SCL, PIN_SDA);
  Serial.printf("  RC522 CS=%d SCK=%d MOSI=%d MISO=%d RST=%d\n",
                PIN_RFID_CS, PIN_SCK, PIN_MOSI, PIN_MISO, PIN_RFID_RST);
  Serial.printf("  BUTON UP=%d DOWN=%d BACK=%d OK=%d\n",
                PIN_BTN_UP, PIN_BTN_DOWN, PIN_BTN_BACK, PIN_BTN_OK);

  byte a = testI2C();
  if (a) {
    testOLED();
  } else {
    Serial.println("\n  OLED bulunamadi, cizim testi atlandi");
    rOLED.ok = false;
    snprintf(rOLED.msg, sizeof(rOLED.msg), "ATLANDI");
  }

  testSPI();
  testGPIO();
  testButtons();
  testCard();
  testSoak();
  printSummary();

  Serial.println("  Test bitti. Kart okutarak sonucu ekranda gorebilirsin.");
  Serial.println("  Yeniden calistirmak icin EN (reset) tusuna bas.");
}

void loop() {
  // bitti — ekranda sonuc, kart okutunca UID goster
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    if (!oledReady) { rfid.PICC_HaltA(); delay(200); return; }
    oled.clearBuffer();
    oled.setFont(u8g2_font_6x10_tf);
    oled.drawStr(2, 12, "KART");
    char u[20];
    snprintf(u, sizeof(u), "%02X:%02X:%02X:%02X",
             rfid.uid.size > 0 ? rfid.uid.uidByte[0] : 0,
             rfid.uid.size > 1 ? rfid.uid.uidByte[1] : 0,
             rfid.uid.size > 2 ? rfid.uid.uidByte[2] : 0,
             rfid.uid.size > 3 ? rfid.uid.uidByte[3] : 0);
    oled.drawStr(2, 30, u);
    oled.drawStr(2, 48, "test tamam");
    oled.sendBuffer();
    rfid.PICC_HaltA();
    delay(1500);
    printSummary();
  }
  delay(80);
}
