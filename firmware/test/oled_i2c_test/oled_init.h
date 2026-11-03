/*
 * ===========================================================================
 *  oled_init.h — DAYANIKLI OLED BASLATMA (I2C bus recovery dahil)
 * ===========================================================================
 *
 *  Neden gerekli:
 *    Bazi SSD1309 modulleri ilk Wire.begin() sonrasi ilk isleme NAK veriyor.
 *    Hat bir kez durtulunce (Wire.end + begin) calisiyor. Sebep: slave yarim
 *    kalmis byte'tan sonra SDA'yi birakmiyor -> 9 clock darbesiyle kurtarilir.
 *
 *  Bu baslik dosyasi:
 *    1) Hattin bosta olup olmadigini olcer (SDA/SCL HIGH mi)
 *    2) 9 clock darbesi + STOP kosulu ile bus recovery yapar
 *    3) Wire.begin + probe'u 8 kez, 4 tur dener
 *    4) Basarinca u8g2.begin() ve test yazimi ile dogrular
 *    5) Basarisizsa tekrar kurtarip bastan dener
 *
 *  Kullanim:
 *      #include "oled_init.h"
 *      U8G2_SSD1309_128X64_NONAME0_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
 *      ...
 *      byte addr = oledBringUp(oled, 21, 22);   // 0 = basarisiz
 */

#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <U8G2lib.h>

// --- 9 clock darbesi + STOP: kilitli slave'i serbest birakir -------------
static void i2cBusRecover(uint8_t sda, uint8_t scl) {
  Wire.end();
  delay(2);

  pinMode(scl, OUTPUT);
  pinMode(sda, INPUT_PULLUP);
  digitalWrite(scl, HIGH);
  delayMicroseconds(10);

  // 9 darbe: slave yarim byte'ta kaldiysa bu onu tamamlar
  for (int i = 0; i < 9; i++) {
    digitalWrite(scl, LOW);  delayMicroseconds(8);
    digitalWrite(scl, HIGH); delayMicroseconds(8);
  }

  // STOP kosulu: SDA alcakken SCL yuksek -> sonra SDA yuksek
  pinMode(sda, OUTPUT);
  digitalWrite(sda, LOW);   delayMicroseconds(8);
  digitalWrite(scl, HIGH);  delayMicroseconds(8);
  digitalWrite(sda, HIGH);  delayMicroseconds(8);
  delay(2);
}

// --- tek ACK denemesi ---------------------------------------------------
static bool i2cProbe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

// --- hat bosta mi -------------------------------------------------------
static bool i2cIdleOk(uint8_t sda, uint8_t scl, bool verbose) {
  Wire.end();
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, INPUT_PULLUP);
  delay(5);
  int a = digitalRead(sda), b = digitalRead(scl);
  if (verbose) {
    Serial.printf("  hat bosta: SDA=%s SCL=%s", a ? "H" : "L", b ? "H" : "L");
    if (!a || !b) Serial.print("   <<< HAT LOW'DA: kisa devre / ters baglanti / cekilmis hat");
    Serial.println();
  }
  return a && b;
}

// --- ana fonksiyon ------------------------------------------------------
// Donus: OLED adresi (0x3C/0x3D) veya 0 = bulunamadi
static byte oledBringUp(U8G2& g, uint8_t sda, uint8_t scl, bool verbose = true) {
  const byte addrs[2] = {0x3C, 0x3D};

  if (verbose) Serial.println("  [oled] baslatiliyor (bus recovery + retry)");

  for (int round = 1; round <= 4; round++) {
    // 1) hat bosta mi
    if (!i2cIdleOk(sda, scl, verbose)) {
      if (verbose) Serial.println("  [oled] hat LOW'da, kurtarma denenecek");
    }

    // 2) kurtarma + baslat
    i2cBusRecover(sda, scl);
    Wire.begin(sda, scl);
    Wire.setClock(100000);
    delay(30);

    // 3) TAM tarama (1..127): her basarisiz islem clock darbesi uretir,
    //    kilitli slave'i kendisi acar. Sadece 0x3C/0x3D denemek yetmez.
    byte found = 0;
    for (int attempt = 0; attempt < 2 && !found; attempt++) {
      for (byte a = 1; a < 127; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
          if ((a == 0x3C || a == 0x3D)) { found = a; break; }
        }
      }
      if (!found) delay(30);
    }

    if (found) {
      if (verbose) Serial.printf("  [oled] ACK 0x%02X (tur %d)\n", found, round);

      // 4) u8g2 baslat ve test yazimi ile dogrula
      g.setI2CAddress(found << 1);
      g.begin();
      g.setBusClock(100000);
      g.setContrast(200);
      g.setPowerSave(0);

      g.clearBuffer();
      g.drawFrame(0, 0, 128, 64);
      g.sendBuffer();

      // 5) yazimdan sonra hat hala canli mi
      if (i2cProbe(found)) {
        if (verbose) Serial.println("  [oled] yazim sonrasi ACK — hazir");
        return found;
      }
      if (verbose) Serial.println("  [oled] yazim sonrasi NAK — tekrar denenecek");
    } else if (verbose) {
      Serial.printf("  [oled] tur %d basarisiz (8 probe)\n", round);
    }
    delay(120);
  }

  if (verbose) {
    Serial.println("  [oled] BULUNAMADI. Kontrol:");
    Serial.println("    CS->GND, A0->GND, RES->3V3, VCC=3.3V");
    Serial.println("    SDA/SCL'ye 4.7k pull-up, kablo <15 cm");
  }
  return 0;
}
