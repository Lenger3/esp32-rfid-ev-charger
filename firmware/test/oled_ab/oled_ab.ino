/*
 * ===========================================================================
 *  A/B TEST — ayni binary, ayni acilis, iki farkli I2C baslatma yolu
 * ===========================================================================
 *
 *  Amac: "i2c_test calisiyor, integration_test calismiyor" farkinin
 *        KOD mu YOKSA DONANIM/ZAMANLAMA mi oldugunu KESIN ayirmak.
 *
 *  A yolu = i2c_test'in birebir kopyasi  (calistigi soylenen)
 *  B yolu = integration_test'in yolu     (calismadigi soylenen)
 *
 *  Ikisi de ayni boot icinde, arka arkaya, 3 tur denenir.
 *
 *  YORUM:
 *    A hep OK, B hep NAK        -> KOD farki var. Asagidaki "FARK" bolumune bak.
 *    Ikisi de ayni sonuc        -> KOD farki YOK, sorun temas/zamanlama.
 *    Ikisi de karisik (bazen OK) -> temas marjinal (kablo/jumper).
 */

#include <Arduino.h>
#include <Wire.h>

#define PIN_SDA 21
#define PIN_SCL 22

// ============================================================ A YOLU
// i2c_test.ino'daki mantigin birebir kopyasi
byte i2cScanA(const char* tag) {
  Serial.printf("  [A] tarama (%s)\n", tag);
  byte found = 0, oled = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("      cihaz: 0x%02X\n", addr);
      if ((addr == 0x3C || addr == 0x3D) && !oled) oled = addr;
      found++;
    }
  }
  if (!found) Serial.println("      hicbir cihaz yok");
  return oled;
}

byte pathA() {
  byte oled = 0;
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
  delay(30);
  oled = i2cScanA("SDA=21 SCL=22");

  if (!oled) {
    Wire.end();
    delay(20);
    Wire.begin(PIN_SCL, PIN_SDA);      // TAKAS
    Wire.setClock(100000);
    delay(30);
    oled = i2cScanA("takas SDA=22 SCL=21");
    if (!oled) {
      Wire.end();
      delay(20);
      Wire.begin(PIN_SDA, PIN_SCL);
    }
  }
  return oled;
}

// ============================================================ B YOLU
// integration_test.ino'daki mantigin birebir kopyasi
byte pathB() {
  byte addr = 0;
  const byte addrs[2] = {0x3C, 0x3D};

  for (int round = 1; round <= 2 && !addr; round++) {
    for (int order = 0; order < 2 && !addr; order++) {
      byte sdaPin = order == 0 ? PIN_SDA : PIN_SCL;
      byte sclPin = order == 0 ? PIN_SCL : PIN_SDA;
      Wire.end();
      delay(10);
      Wire.begin(sdaPin, sclPin);
      Wire.setClock(100000);
      delay(40);
      for (int ai = 0; ai < 2; ai++) {
        Wire.beginTransmission(addrs[ai]);
        if (Wire.endTransmission() == 0) { addr = addrs[ai]; break; }
      }
    }
    if (!addr && round < 2) {
      // bus recovery
      Wire.end();
      pinMode(PIN_SCL, OUTPUT);
      pinMode(PIN_SDA, INPUT_PULLUP);
      for (int i = 0; i < 9; i++) {
        digitalWrite(PIN_SCL, LOW);  delayMicroseconds(6);
        digitalWrite(PIN_SCL, HIGH); delayMicroseconds(6);
      }
      pinMode(PIN_SDA, OUTPUT);
      digitalWrite(PIN_SDA, LOW); delayMicroseconds(6);
      digitalWrite(PIN_SCL, HIGH); delayMicroseconds(6);
      digitalWrite(PIN_SDA, HIGH); delayMicroseconds(6);
    }
  }
  if (addr) {
    Wire.end(); delay(10);
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(100000);
    delay(40);
  }
  return addr;
}

// ============================================================ durum
void idleLevels(const char* tag) {
  pinMode(PIN_SDA, INPUT_PULLUP);
  pinMode(PIN_SCL, INPUT_PULLUP);
  delay(15);
  Serial.printf("  %s hat bosta: SDA=%s SCL=%s\n", tag,
                digitalRead(PIN_SDA) ? "H" : "L",
                digitalRead(PIN_SCL) ? "H" : "L");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("##################################################");
  Serial.println("#  A/B TEST — kod mu, donanim mi?                 #");
  Serial.println("##################################################");

  int aOk = 0, bOk = 0;

  for (int round = 1; round <= 3; round++) {
    Serial.println();
    Serial.printf("================ TUR %d/3 ================\n", round);

    // A once
    idleLevels("(A oncesi)");
    byte a = pathA();
    Serial.printf("  [A] SONUC: %s\n", a ? "ACK / CALISTI" : "NAK / BASARISIZ");
    if (a) aOk++;

    delay(1500);

    // B sonra — A'nin biraktigi durumdan baslar (gercek hayattaki gibi)
    idleLevels("(B oncesi)");
    byte b = pathB();
    Serial.printf("  [B] SONUC: %s\n", b ? "ACK / CALISTI" : "NAK / BASARISIZ");
    if (b) bOk++;

    delay(1500);
  }

  // ---------------------------------------------------------- karar
  Serial.println();
  Serial.println("##################################################");
  Serial.println("#  SONUC");
  Serial.println("##################################################");
  Serial.printf("  A yolu (i2c_test mantigi)      : %d/3\n", aOk);
  Serial.printf("  B yolu (integration mantigi)   : %d/3\n", bOk);
  Serial.println();

  if (aOk == 3 && bOk == 0) {
    Serial.println("  >>> KOD FARKI VAR. B yolunun A'dan farki:");
    Serial.println("      - B, Wire.end() + Wire.begin() ile hatti yeniden baslatiyor");
    Serial.println("      - B, 40 ms bekliyor (A: 30 ms)");
    Serial.println("      - B, 0x3C ve 0x3D'yi tek tek deniyor (A: 1..127 tarama)");
    Serial.println("      COZUM: integration_test'te testI2C() basina Wire.end()");
    Serial.println("             cagrisini kaldir, A yolunu kullan.");
  } else if (aOk == 3 && bOk == 3) {
    Serial.println("  >>> IKISI DE CALISIYOR. Onceki basarisizlik geciciydi");
    Serial.println("      (temas/zamanlama). integration_test'i tekrar dene.");
  } else if (aOk == 0 && bOk == 0) {
    Serial.println("  >>> IKISI DE CALISMIYOR. Sorun kesinlikle DONANIM:");
    Serial.println("      CS->GND, A0->GND, RES->3V3, VCC=3.3V, kablo temasi.");
  } else {
    Serial.println("  >>> KARISIK sonuc = TEMAS MARJINAL.");
    Serial.println("      Kablolari cikar-tak, kisalt, 4.7k pull-up ekle.");
  }
  Serial.println();
  Serial.println("  Test bitti. Tekrar icin EN tusuna bas.");

  idleLevels("(son)");
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
}

void loop() {
  static unsigned long last = 0;
  if (millis() - last > 3000) {
    last = millis();
    Wire.beginTransmission(0x3C);
    bool ok = (Wire.endTransmission() == 0);
    Serial.printf("[%lus] 0x3C -> %s\n", millis() / 1000, ok ? "ACK" : "NAK");
  }
}
