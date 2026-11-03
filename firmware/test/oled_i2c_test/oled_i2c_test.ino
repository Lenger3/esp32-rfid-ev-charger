/*
 * M242-12864-7-V2.0  (SSD1309 2.42" 128x64, 7 pin) — I2C TEST
 * ESP32-WROOM-32U
 *
 * CALISAN SURUM — degistirme. kart_ekran.ino bunun birebir kopyasini kullanir.
 *
 * KABLOLAMA
 *   #1 GND  #2 VCC(3V3)  #3 SCL=GPIO22  #4 SDA=GPIO21
 *   #5 RES -> 3V3        #6 A0(DC) -> GND (adres 0x3C)
 *   #7 CS  -> GND
 */

#include <Wire.h>
#include <U8g2lib.h>

#define PIN_SDA   21
#define PIN_SCL   22
#define PIN_RES   U8X8_PIN_NONE

U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

byte OLED_ADDR = 0x3C;

// Tarama: OLED adresini dondurur (0x3C / 0x3D), yoksa 0
byte i2cScan(const char* tag) {
  Serial.printf("=== I2C TARAMASI (%s) ===\n", tag);
  byte found = 0, oled = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  bulundu: 0x%02X", addr);
      if (addr == 0x3C || addr == 0x3D) {
        Serial.print("   <-- SSD1309");
        if (!oled) oled = addr;
      }
      Serial.println();
      found++;
    }
  }
  if (found == 0) Serial.println("  hicbir cihaz cevap vermedi");
  else Serial.printf("  toplam %d cihaz\n", found);
  Serial.println("================================");
  return oled;
}

byte scanBothOrders() {
  byte oled = 0;

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
  delay(30);
  oled = i2cScan("SDA=21 SCL=22");

  if (!oled) {
    Serial.println();
    Serial.println("  >> Cevap yok. SDA/SCL yer degistirmis olabilir, takas deneniyor...");
    Wire.end();
    delay(20);
    Wire.begin(PIN_SCL, PIN_SDA);      // TAKAS
    Wire.setClock(100000);
    delay(30);
    oled = i2cScan("SDA=22 SCL=21 (takas)");
    if (oled) {
      Serial.println();
      Serial.println("  >>> KABLO TERS: SDA ve SCL yer degistirmis!");
      Serial.println();
    } else {
      Wire.end();
      delay(20);
      Wire.begin(PIN_SDA, PIN_SCL);
    }
  }
  return oled;
}

void page1_hello() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB10_tf);
  u8g2.drawStr(0, 13, "MERHABA");
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 27, "SSD1309 128x64");
  u8g2.drawStr(0, 39, "I2C test OK");
  char ab[16];
  snprintf(ab, sizeof(ab), "i2C 0x%02X  21/22", OLED_ADDR);
  u8g2.drawStr(0, 51, ab);
  u8g2.drawStr(0, 63, "EVCHARGE-01");
  u8g2.sendBuffer();
}

void page2_grid() {
  u8g2.clearBuffer();
  for (int x = 0; x < 128; x += 8) u8g2.drawVLine(x, 0, 64);
  for (int y = 0; y < 64; y += 8) u8g2.drawHLine(0, y, 128);
  u8g2.setDrawColor(0);
  u8g2.drawBox(1, 1, 52, 13);
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(3, 11, "izgara 8px");
  u8g2.sendBuffer();
}

void page3_frames() {
  u8g2.clearBuffer();
  u8g2.drawFrame(0, 0, 128, 64);
  u8g2.drawFrame(4, 4, 120, 56);
  u8g2.drawRFrame(8, 8, 112, 48, 6);
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(22, 32, "CERCEVE");
  u8g2.drawDisc(112, 52, 6);
  u8g2.drawCircle(96, 52, 6);
  u8g2.sendBuffer();
}

void page4_contrast() {
  char buf[20];
  for (int c = 10; c <= 250; c += 20) {
    u8g2.setContrast(c);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(2, 16, "kontrast");
    snprintf(buf, sizeof(buf), "%d / 255", c);
    u8g2.setFont(u8g2_font_helvB12_tf);
    u8g2.drawStr(2, 38, buf);
    u8g2.drawBox(2, 46, (c * 12) / 25, 9);
    u8g2.sendBuffer();
    delay(110);
  }
  u8g2.setContrast(200);
}

void page5_stress() {
  u8g2.clearBuffer();
  for (int i = 0; i < 40; i++) {
    int x = random(0, 118), y = random(0, 56);
    int w = random(2, 12), h = random(2, 8);
    if (random(0, 2)) u8g2.drawBox(x, y, w, h);
    else u8g2.drawFrame(x, y, w, h);
  }
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n\n=== M242 SSD1309 I2C TEST ===");
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

  Wire.begin(PIN_SDA, PIN_SCL);
  delay(20);
  u8g2.setI2CAddress(OLED_ADDR << 1);   // 8-bit adres, begin'den ONCE
  u8g2.begin();
  u8g2.setBusClock(400000);
  u8g2.setContrast(200);
  u8g2.setPowerSave(0);
  Serial.println("u8g2 begin() tamam — sayfalar basliyor");
  Serial.println();
}

void loop() {
  page1_hello();    Serial.println("sayfa 1/5  MERHABA");   delay(2000);
  page2_grid();     Serial.println("sayfa 2/5  izgara");    delay(2000);
  page3_frames();   Serial.println("sayfa 3/5  cerceve");   delay(2000);
  page4_contrast(); Serial.println("sayfa 4/5  kontrast");  delay(400);
  page5_stress();   Serial.println("sayfa 5/5  stres");     delay(4000);
}
